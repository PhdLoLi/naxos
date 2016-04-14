for k in $( seq 0 2 )
do
   nohup ssh -t lijing@node${k} "cd naxos && bin/naxos ${k} 3 50 1 5" &
done

ssh -t lijing@node3 "cd naxos && bin/clients $1 100 0"

for k in $( seq 0 2 )
do
   ssh -t lijing@node${k} "killall -v naxos" 
done
