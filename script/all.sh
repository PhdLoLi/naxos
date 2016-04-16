# run @ node4
/home/lijing/NDNPaxos/script/start_naxos_3.sh
/home/lijing/naxos/script/start_naxos_3.sh
# run @ node6
ssh -t root@node6 "/home/lijing/NDNPaxos/script/start_naxos_5.sh"
ssh -t root@node6 "/home/lijing/naxos/script/start_naxos_5.sh"
ssh -t root@node6 "/home/lijing/naxos/script/start_naxos_5_2.sh"
