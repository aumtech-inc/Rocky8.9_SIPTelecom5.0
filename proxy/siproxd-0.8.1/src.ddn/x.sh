
ls -1tr *.c | while read x
do
	echo "*** $x ***"
	diff $x ../src.orig/$x
done
