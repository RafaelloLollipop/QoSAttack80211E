# QoSAttack80211E

Sample script to show attack based on the switching cwMax, cwMin and TXOP param between two wifi networks on the same channel.
Script takes cwMin, cwMax, TXOP from params and configure network B. Network A is based on the default params.

To change distance (default AP(0), STA1(3m), STA2(7m), AP2(10m)), data rates or tos - modify script by hand. (obvious or commented)

2nd case:
In network A we have two different flows. (eg. AC_VI and AC_BE). To run 2nd flow in A network set secondCase to `1`.


Example: ./waf --run "scratch/qosattack802.11e --cwMin=10 --cwMax=20 --TXOP=1280"