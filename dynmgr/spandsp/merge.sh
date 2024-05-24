
for i in `ls *.[ch]`
  do 
	echo $i 
	vim -d $i ../../dynmgr.current/spandsp/ 
done 

