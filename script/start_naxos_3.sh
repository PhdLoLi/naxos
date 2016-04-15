client_nums=(1 10 30)
write_ratios=(100 90 10 0)

for client_num in ${client_nums[@]}
do
    for write_ratio in ${write_ratios[@]}
    do
        echo Client_Num: $client_num Write_Ratio: $write_ratio Read_From node0

        for k in $( seq 0 2 )
        do
        echo node${k}
           nohup ssh -t root@node${k} "cd /home/lijing/naxos &&  bin/naxos ${k} 3 50 1 5" & #> logs/naxos_node${k}.out 2>&1
        done
#        echo  $client_num 
        ssh -t root@node4 "cd /home/lijing/naxos &&  bin/clients $client_num $write_ratio 0" #> logs/clients_$client_num .out 2>&1 
        
        for k in $( seq 0 2 )
        do
           ssh -t root@node${k} "killall -v naxos" 
        done
    done
done
