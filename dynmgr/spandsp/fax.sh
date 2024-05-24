#!/bin/bash 
#  
#  ./fax: invalid option -- h
#  
#   fax  FAX Test Driver, by Joe Santapau use this to add or debug FAX features 
#  
#   usage: fax <various options>
#  
#    -i infile 
#    -o outfile 
#    -e t38 ecm mode 
#    -E t30 ecm mode 
#    -t use t38 
#    -s  span width 
#    -a  audio device name, ex /dev/dsp
#

VALGRINDCMD="valgrind --tool=memcheck --leak-check=full "
INFILE="./i.tif"
OUTFILE="./o.tif"
AUDIODEV="/dev/dsp"
SLEEP=10 

check_status(){
  if [ $? -ne 0 ]; then echo -ne "[OK]" ; else echo -ne "[FAILED]"; fi
}

fax_input(){
 echo -en "$1 "
 if [ -e $1 ]; then echo -en "input file [OK]" ; else echo -en "input file [MISSING]"; fi
}

fax_success(){
 echo -en "$1 "
 if [ -e $1 ]; then echo -en " output fax [OK]" ; else echo -en "output fax[FAILED]"; fi
}

test_one() {

   echo -ne <<END

   Testing t38 fax
  	infile=$INFILE 
	outfile=$OUTFILE 
	T38 Enabled 
	ErrorCorrectionMode=2
	T30ErrorCorrection on
	T38SpanWidth=3
END

   if [ -e o.tif ] ; then rm o.tif ; fi
   echo " Testingfax t38 with udptl redundancy set ... "
   ./fax -i $INFILE -o $OUTFILE -e 2 -E -t -s 3 -a $AUDIODEV 
   check_status 
   if [ -e o.tif ] ; then echo " fax outfile exists ..." ; fi
}

test_two() {

   if [ -e o.tif ] ; then rm o.tif ; fi
   echo " Testingfax t38 with udptl redundancy set ... "
   ./fax -i $INFILE -o $OUTFILE -e 0 -E -t -s 0 -a $AUDIODEV
   check_status 
   if [ -e o.tif ] ; then echo " fax outfile exists ..." ; fi
}

test_three() {

   fax_input $INFILE 
   if [ -e $OUTFILE ] ; then rm $OUTFILE ; fi
   echo " Testingfax t38 with udptl redundancy set ..."
   ./fax -i $INFILE -o $OUTFILE -a $AUDIODEV
   check_status 
   fax_success $OUTFILE 
}

echo test 1 ./fax -i i.tif -o out.tif -t -e 0 -s 0 -E -S 0
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 0 -s 0 -E -S 0  -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 0 -s 1 -E -S 1  -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 0 -s 2 -E -S 2  -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 1 -s 0 -E -S 0  -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 1 -s 1 -E -S 1 -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 1 -s 2 -E -S 2 -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 2 -s 0 -E -S 0 -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 2 -s 1 -E -S 1 -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 2 -s 2 -E -S 2 -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 0 -s 0 -S 0  -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 0 -s 1 -S 1 -d
sleep $SLEEP  
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 0 -s 2 -S 2 -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 1 -s 0 -S 0 -d
sleep $SLEEP 
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 1 -s 1 -S 1 -d
sleep $SLEEP2
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 1 -s 2 -S 2 -d
sleep $SLEEP2
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 2 -s 0 -S 0 -d
sleep $SLEEP2
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 2 -s 1 -S 1 -d
sleep $SLEEP2
$VALGRINDCMD ./fax -i i.tif -o out.tif -t -e 2 -s 2 -S 2 -d
sleep $SLEEP2








 

