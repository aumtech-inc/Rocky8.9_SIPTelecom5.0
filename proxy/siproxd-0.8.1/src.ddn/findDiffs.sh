
out1="/tmp/fdiffs.txt"
dF="/tmp/f"
if [ ! -d ${dF} ]
then
	echo "Directory ${dF} does not exist. Nope."
	exit
fi
echo "Finding differences. Hold your horses...."
find -P . \( -name \*.[ch] -o -name \*.[ch]pp \) -print  |egrep -v "not_sure|ddn_test|arc\/bkps" | while read x
do
	danFile=${dF}`echo "$x" | cut -c2-`
	# echo "${danFile}"
	diff -q ${x} ${danFile}
	if [ $? -ne 0 ]
	then
		ls -l ${x} ${danFile}
		echo 

		echo "++ diff   ${x}       ${danFile}"
		diff  ${x} ${danFile}
		echo
		echo "--------------------------------------------------------------"
		
	fi
done >${out1} 2>&1

vi ${out1}

