read_nodes=(3)
client_nums=(1 10 30)
write_ratios=(100 90 10 0)
for read_node in ${read_nodes[@]}
do
    for client_num in `seq 1 1`
    #for client_num in ${client_nums[@]}
    do
        for write_ratio in ${write_ratios[@]}
        do
            echo Client_Num: $client_num Write_Ratio: $write_ratio Read_From node_$read_node
    
            for k in $( seq 0 2 )
            do
              echo node${k}
              cd ~/naxos && nohup bin/naxos ${k} 3 500 0 5 &
            done
    #        echo  $client_num 
    #        ssh -t root@node4 "cd /home/lijing/naxos &&  bin/clients $client_num $write_ratio 2"
            cd ~/naxos &&  bin/clients $client_num $write_ratio $read_node        
            killall -v naxos
        done
    done
    cur_time=`date +"%m%d%H"`
    folder_name="${cur_time}""_3r_node""${read_node}"
    cd ~/naxos/results/naxos && mkdir $folder_name  && mv *.txt $folder_name
done
