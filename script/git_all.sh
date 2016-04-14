for k in $( seq 0 4 )
do
   nohup ssh lijing@node${k} 'bash -s' < git.sh &
done
   ssh lijing@node5 'bash -s' < git.sh 
