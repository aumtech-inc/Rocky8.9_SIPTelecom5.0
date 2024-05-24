
for i in `ls *.[ch]`
  do 
	echo $i 
	echo "QUIT ? y/n ? (y) "
    read QUIT 
	if [ "$QUIT" == "y" ] ; then 
		exit 0
    fi  
	vim -d $i ../include.current/ 
done 

