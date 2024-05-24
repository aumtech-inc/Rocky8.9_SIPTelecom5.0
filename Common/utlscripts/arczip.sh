#!/bin/bash

#============================================================================================
#Script:arczip.sh
#Purpose: This is used to zip folder to save disk space and after zip delete original folder.
#         Also, it checks the size of ${SWISBSDK}/out.txt.  If > 100 MB, it zeros it out.
#Usage: Run a script using ./<Srcipt Name> or 'bash <Script name>'. Bash must required.
#Author: Mayank Jain
#============================================================================================


sysname=`uname -n`
PREVIOUSMONTHDATE1=`date +%Y%m%d -d "1 day ago"`
PREVIOUSMONTHDATE2=`date +%Y%m%d -d "2 day ago"`
curdate=`date +%Y%m%d`
MODE=debug

myEcho(){

	THE_DATE="`date +%Y/%m/%d`"
	THE_TIME="`date +%H:%M:%S`"
	timeStamp="$THE_DATE $THE_TIME"

	if [ $MODE = "debug" ]
	then
		echo "$timeStamp | " $*
	fi
	

} # END: myEcho()

checkVXILogDotTxt()
{
	dir="/home/arc/.ISP/Telecom/Exec/vxi"

	fle="${dir}/out.txt"

	if [ ! -f ${fle} ]
	then
		echo "`date +"%Y-%m-%d %H:%M:%S"`|arczip.sh: ${fle} does not exist. Unable to process."
		return
	fi
	
	fleSize=`ls -l ${fle} | cut -d " "  -f5`

	if [ ${fleSize} -ge 102400000 ]  # 100 MB
	then
		>${fle}
		echo "`date +"%Y-%m-%d %H:%M:%S"`|arczip.sh: Size of ${fle} >= 100 MB (${fleSize}). Truncating. "
	else
		echo "`date +"%Y-%m-%d %H:%M:%S"`|arczip.sh: Size of ${fle} < 100 MB (${fleSize}). No action taken. "
	fi

} # END: checkVXILogDotTxt()

#------------------------------------------------------------------------------
# main
#------------------------------------------------------------------------------


loaddir=/home/arc/.ISP/LOG/load/
if [ ! -d ${loaddir} ]
then
	mkdir -p ${loaddir} ]
fi

cd $loaddir

listOfFiles=`ls -l | egrep -v "$PREVIOUSMONTHDATE1|$PREVIOUSMONTHDATE2|$curdate|*.tgz" | awk '{print $9}' | grep "^loaded."`

for i in $listOfFiles
do
	tar=`tar -czvf $i.tgz $i`
	if [ $? == 0 ] 
	then
		myEcho "Tar file is: $i.tgz"
		rm -rf $i

		if [ -d $i ]
		then
			myEcho "Folder $i is not deleted. Failed."
		else
			myEcho "Folder $i is deleted successfully"
		fi
	else
		myEcho "There is an issue while making tar file."
	fi	
done

checkVXILogDotTxt
