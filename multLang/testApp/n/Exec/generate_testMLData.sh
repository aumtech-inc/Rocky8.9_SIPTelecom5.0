#!/bin/ksh
#--------------------------------------------------------------------------------------------------------
# File:		generate_testMLData.sh
# Purpose:	Create input data files for testing the
#			arcML package.
#--------------------------------------------------------------------------------------------------------

num=0
cents=0
numFile="testMLNumbers.dat"
currencyFile="testMLCurrency.dat"
timeFile="testMLTime.dat"
dateFile="testMLDate.dat"

rm $numFile  $currencyFile  $timeFile $dateFile 2>/dev/null
while [ $num -le 100 ]
do
	echo "$num" >>$numFile
	if [ $cents -gt 99 ]
	then
		cents=0
	fi
	if [ $cents -lt 10 ]
	then
		xCents="0$cents"
	else
		xCents="$cents"
	fi
	echo "$num.$xCents" >>$currencyFile
	num=`expr $num + 1`
	cents=`expr $cents + 1`
done 

typeset -iZR2 hour
typeset -iZR2 minute

hour=0
minute=0
while [ $hour -lt 24 ]
do
	while [ $minute -lt 60 ]
	do
		echo "$hour$minute" >>$timeFile
		minute=`expr $minute + 3`
	done
	minute=0
	hour=`expr $hour + 1`
done

typeset -iZR2 month
typeset -iZR2 day
typeset -iZR4 year
month=1
day=1
year=2002
while [ $month -lt 13 ]
do
	echo "$month-$day-$year" >>$dateFile
	month=`expr $month + 1`
done

month=1
day=$month
year=2002
while [ $month -lt 13 ]
do
	if [ $month -eq 2 ];then
		maxDay=28
	elif [ $month -eq 4 ];then
		maxDay=30
	elif [ $month -eq 6 ];then
		maxDay=30
	elif [ $month -eq 9 ];then
		maxDay=30
	elif [ $month -eq 11 ];then
		maxDay=30
	else
		maxDay=31
	fi

	while [ $day -le $maxDay ]
	do
		echo "$month-$day-$year" >>$dateFile
		day=`expr $day + 3`
	done
	month=`expr $month + 1`
	day=$month
done

