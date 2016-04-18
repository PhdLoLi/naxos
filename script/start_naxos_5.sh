#argument 1: node_num
#argument 2: reading from node
read_nodes=(0 4)
client_nums=(1 10 30)
write_ratios=(100 90 10 0)

for read_node in ${read_nodes[@]}
    do
    for client_num in `seq 1 30`
    #for client_num in ${client_nums[@]}
    do
        for write_ratio in ${write_ratios[@]}
        do
            echo Client_Num: $client_num Write_Ratio: $write_ratio Read_From node_$read_node 
    
            for k in $( seq 0 4 )
            do
            echo ssh to node${k}
               nohup ssh -t root@node${k} "cd /home/lijing/naxos &&  bin/naxos ${k} 5 50 1 5" &
            done
    #        echo  $client_num 
    #        ssh -t root@node4 "cd /home/lijing/naxos &&  bin/clients $client_num $write_ratio 2"
    #exe from node* locally
             
    #        nohup ssh -t root@node7 "cd /home/lijing/naxos &&  bin/clients $client_num $write_ratio 3" &
    
            cd /home/lijing/naxos &&  bin/clients $client_num $write_ratio $read_node  #run @node6      
            for k in $( seq 0 4 )
            do
               ssh -t root@node${k} "killall -v naxos" 
            done
        done
    done
    cur_time=`date +"%m%d%H"`
    folder_name="${cur_time}""_5r_node""${read_node}"
    cd /home/lijing/naxos/results/naxos && mkdir $folder_name  && mv *.txt $folder_name
done
