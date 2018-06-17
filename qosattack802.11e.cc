#include "ns3/command-line.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-net-device.h"
#include "ns3/qos-txop.h"
#include "ns3/wifi-mac.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

//  BSS A (36)        BSS B (36)
//   *      *          *      *
//   |      |          |      |
//  AP A   STA A      AP B   STA B
//
// The configuration is the following on the 2 networks:
// - STA A sends AC_BE traffic to AP A with default AC_BE TXOP value of 0 (1 MSDU);
// - STA B sends AC_BE traffic to AP B with non-default AC_BE TXOP of 3.008 ms;
//
// The user can select the distance between the stations and the APs, can enable/disable the RTS/CTS mechanism
// and can choose the payload size and the simulation duration.
// Example: ./waf --run "scratch/qosattack802.11e --cwMin=10 --cwMax=20 --TXOP=1280"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("80211eTxop");

int main (int argc, char *argv[])
{
  uint32_t payloadSize = 1472; //bytes
  double simulationTime = 10; //seconds
  bool enablePcap = 0;
  bool secondCase = 0;
  bool verifyResults = 0; //used for regression
  uint32_t cwMin = 0;
  uint32_t cwMax = 0;
  uint32_t TXOP = 0;

  CommandLine cmd;
  cmd.AddValue ("cwMin", "Set cwMin for the simulation network B", cwMin);
  cmd.AddValue ("cwMax", "Set cwMax for the simulation network B", cwMax);
  cmd.AddValue ("TXOP", "Set TXOP for the simulation network B", TXOP);
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2);
  NodeContainer wifiApNodes;
  wifiApNodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.SetChannel (channel.Create ());

  WifiHelper wifi; //the default standard of 802.11a will be selected by this helper since the program doesn't specify another one
  wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
  WifiMacHelper mac;

  NetDeviceContainer staDeviceA, staDeviceB, apDeviceA, apDeviceB;
  Ssid ssid;

  //Network A
  ssid = Ssid ("network-A");
  phy.Set ("ChannelNumber", UintegerValue (36));
  mac.SetType ("ns3::StaWifiMac",
               "QosSupported", BooleanValue (true),
               "Ssid", SsidValue (ssid));
  staDeviceA = wifi.Install (phy, mac, wifiStaNodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "QosSupported", BooleanValue (true),
               "Ssid", SsidValue (ssid),
               "EnableBeaconJitter", BooleanValue (false));
  apDeviceA = wifi.Install (phy, mac, wifiApNodes.Get (0));

  //Network B
  ssid = Ssid ("network-B");
  phy.Set ("ChannelNumber", UintegerValue (36));
  mac.SetType ("ns3::StaWifiMac",
               "QosSupported", BooleanValue (true),
               "Ssid", SsidValue (ssid));

  staDeviceB = wifi.Install (phy, mac, wifiStaNodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "QosSupported", BooleanValue (true),
               "Ssid", SsidValue (ssid),
               "EnableBeaconJitter", BooleanValue (false));
  apDeviceB = wifi.Install (phy, mac, wifiApNodes.Get (1));

  //Modify EDCA configuration (TXOP limit) for AC_BE
  Ptr<NetDevice> dev = wifiApNodes.Get (1)->GetDevice (0);
  Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
  Ptr<WifiMac> wifi_mac = wifi_dev->GetMac ();
  PointerValue ptr;
  Ptr<QosTxop> edca;
  wifi_mac->GetAttribute ("BE_Txop", ptr);
  edca = ptr.Get<QosTxop> ();
  edca->SetTxopLimit (MicroSeconds (TXOP));
  edca->SetMinCw (cwMin);
  edca->SetMaxCw (cwMax);

  /* Setting mobility model */
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  //Set position for APs
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));
  //Set position for STAs
  positionAlloc->Add (Vector (3, 0.0, 0.0));
  positionAlloc->Add (Vector (7, 0.0, 0.0));
  //Remark: while we set these positions 10 meters apart, the networks do not interact
  //and the only variable that affects transmission performance is the distance.

  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (wifiApNodes);
  mobility.Install (wifiStaNodes);

  /* Internet stack */
  InternetStackHelper stack;
  stack.Install (wifiApNodes);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer StaInterfaceA;
  StaInterfaceA = address.Assign (staDeviceA);
  Ipv4InterfaceContainer ApInterfaceA;
  ApInterfaceA = address.Assign (apDeviceA);

  address.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer StaInterfaceB;
  StaInterfaceB = address.Assign (staDeviceB);
  Ipv4InterfaceContainer ApInterfaceB;
  ApInterfaceB = address.Assign (apDeviceB);

  /* Setting applications */
  uint16_t port = 5001;
  UdpServerHelper serverA (port);
  ApplicationContainer serverAppA = serverA.Install (wifiApNodes.Get (0));
  serverAppA.Start (Seconds (0.0));
  serverAppA.Stop (Seconds (simulationTime + 1));

  InetSocketAddress destA (ApInterfaceA.GetAddress (0), port);
// Zmiana BE na VI dla network A
//  destA.SetTos (0x70); //AC_BE
  destA.SetTos (0xb8); //AC_VI


  OnOffHelper clientA ("ns3::UdpSocketFactory", destA);
  clientA.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientA.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  clientA.SetAttribute ("DataRate", StringValue ("37000kb/s"));
  clientA.SetAttribute ("PacketSize", UintegerValue (payloadSize));


  uint16_t port2 = 5002;
  UdpServerHelper serverAB (port2);
  ApplicationContainer serverAppAB = serverAB.Install (wifiApNodes.Get (0));
  if (secondCase) {
    serverAppAB.Start (Seconds (0.0));
    serverAppAB.Stop (Seconds (simulationTime + 1));
    InetSocketAddress destAB (ApInterfaceA.GetAddress (0), port2);
  // Zmiana BE na VI dla network A
    destAB.SetTos (0x70); //AC_BE
//    destAB.SetTos (0xb8); //AC_VI

    OnOffHelper clientAB ("ns3::UdpSocketFactory", destAB);
    clientAB.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientAB.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientAB.SetAttribute ("DataRate", StringValue ("1kb/s"));
    clientAB.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    ApplicationContainer clientAppAB = clientAB.Install (wifiStaNodes.Get (0));
    clientAppAB.Start (Seconds (1.0));
    clientAppAB.Stop (Seconds (simulationTime + 1));
  }


  ApplicationContainer clientAppA = clientA.Install (wifiStaNodes.Get (0));
  clientAppA.Start (Seconds (1.0));
  clientAppA.Stop (Seconds (simulationTime + 1));

  UdpServerHelper serverB (port);
  ApplicationContainer serverAppB = serverB.Install (wifiApNodes.Get (1));
  serverAppB.Start (Seconds (0.0));
  serverAppB.Stop (Seconds (simulationTime + 1));

  InetSocketAddress destB (ApInterfaceB.GetAddress (0), port);
// Zmiana BE na VI dla network B
  destB.SetTos (0x70); //AC_BE
    //    destB.SetTos (0xb8); //AC_VI

  OnOffHelper clientB ("ns3::UdpSocketFactory", destB);
  clientB.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientB.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
//  clientB.SetAttribute ("DataRate", StringValue ("100000kb/s"));
  clientB.SetAttribute ("DataRate", StringValue ("37000kb/s"));
  clientB.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer clientAppB = clientB.Install (wifiStaNodes.Get (1));
  clientAppB.Start (Seconds (1.0));
  clientAppB.Stop (Seconds (simulationTime + 1));


  AsciiTraceHelper ascii;
  phy.EnableAsciiAll (ascii.CreateFileStream ("out.tr"));
  phy.EnableAscii (ascii.CreateFileStream ("staA.tr"), staDeviceA.Get (0));
  phy.EnableAscii (ascii.CreateFileStream ("staB.tr"), staDeviceB.Get (0));

   FlowMonitorHelper flowmon_helper;
   Ptr<FlowMonitor> monitor = flowmon_helper.InstallAll ();
   monitor->SetAttribute ("StartTime", TimeValue (Seconds (0.0)) ); //Time
//  from which flowmonitor statistics are gathered.
   monitor->SetAttribute ("DelayBinWidth", DoubleValue (0.001));
   monitor->SetAttribute ("JitterBinWidth", DoubleValue (0.001));
   monitor->SetAttribute ("PacketSizeBinWidth", DoubleValue (20));





  if (enablePcap)
    {
      phy.EnablePcap ("AP_A", apDeviceA.Get (0));
      phy.EnablePcap ("STA_A", staDeviceA.Get (0));
      phy.EnablePcap ("AP_B", apDeviceB.Get (0));
      phy.EnablePcap ("STA_B", staDeviceB.Get (0));



    }

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  monitor->SerializeToXmlFile ("out.xml", true, true);

  /* Show results */
  uint64_t totalPacketsThrough = DynamicCast<UdpServer> (serverAppA.Get (0))->GetReceived ();
  double throughput = totalPacketsThrough * payloadSize * 8 / (simulationTime * 1000000.0);
  std::cout << "Network A - Victim AC_VI: Throughput with defaults: " << throughput << " Mbit/s" << '\n';
  if (verifyResults && (throughput < 28 || throughput > 29))
    {
      NS_LOG_ERROR ("Obtained throughput " << throughput << " is not in the expected boundaries!");
      exit (1);
    }

  if (secondCase) {
  totalPacketsThrough = DynamicCast<UdpServer> (serverAppAB.Get (0))->GetReceived ();
  throughput = totalPacketsThrough * payloadSize  * 8 / (simulationTime * 1000000.0);
  std::cout << "Network A - Victim AC_BE: Throughput with defaults: " << throughput << " Mbit/s" << '\n';
  }


  totalPacketsThrough = DynamicCast<UdpServer> (serverAppB.Get (0))->GetReceived ();
  throughput = totalPacketsThrough * payloadSize * 8 / (simulationTime * 1000000.0);
  std::cout << "Network B - Attacker AC_BE: Throughput with cwMin " << cwMin << ", cwMax "<< cwMax << " and TXOP limit (" << TXOP << "ms): " << throughput << " Mbit/s" << '\n';
      if (verifyResults && (throughput < 35.5 || throughput > 36.5))
    {
      NS_LOG_ERROR ("Obtained throughput " << throughput << " is not in the expected boundaries!");
      exit (1);
    }

Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>
(flowmon_helper.GetClassifier ());
 //iterate over traffic flows
 std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
 for (std::map<FlowId, FlowMonitor::FlowStats>::iterator flow = stats.begin ();
flow != stats.end (); flow++)
 {
 Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow->first);
 std::cout << t.destinationAddress << ":" << t.destinationPort << "\n";
 std::cout<<"meanDelayInMS = " << flow->second.delaySum / (flow->second.rxPackets * 1000000.0) <<std::endl;
 std::cout<<"meanJitter1000 = " << (flow->second.jitterSum / (flow->second.rxPackets)) / 1000 <<std::endl;
 std::cout<<"meanJitter1000000 = " << (flow->second.jitterSum / (flow->second.rxPackets)) / 1000000 <<std::endl;
 std::cout<<"JitterSum = " << flow->second.jitterSum <<std::endl;
 std::cout<<"JitterSumInMS = " << flow->second.jitterSum / 1000000.0 <<std::endl;
 }
  return 0;
}