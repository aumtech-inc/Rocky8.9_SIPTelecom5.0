ls -1 *.hpp | while read x
do
	echo -n "*** $x...   "
	if [ `diff $x /tmp/m/include/$x |wc -l` -eq 0 ]
	then
		echo
		continue
	fi
	echo "copying - diff."
	cp -p /tmp/m/include/$x .

	
done
