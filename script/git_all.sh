for k in $( seq 0 6 )
do
   nohup ssh lijing@node${k} 'bash -s' < git.sh &
done

#   ssh lijing@node5 'bash -s' < git.sh 
cd /home/lijing/naxos && git stash && git pull && ./waf configure -l info -m S && ./waf
