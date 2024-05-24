#!/bin/ksh
#
# Author: 		Aumtech, Inc.
# Created:		02/15/2010
#
# Step 1:	Check if SIP Telecom is installed
#
isAculab=0
today=`date +%m%d%Y`
myCount=1
myBkpDir="asdf"
myScript=`basename $0`
remove=0
mypwd=$PWD

if [ ! -d $HOME/.ISP/Telecom ]
then
	echo	"The user `id -gn` doesn't have Telecom Services installed. Login as correct user and try again."
	exit;
fi

echo "Note: Please stop all telecom related processes before installing the patch. (e.g. ttsClient mrcp client, tones client) etc."

if [ $myScript = "removePatch.sh" ]
then
	remove=1
else
	remove=0
fi

if [ -f /home/.arcSIPTEL.pkginfo ]
then
	echo	"Found /home/.arcSIPTEL.pkginfo"
else
	echo	"ERROR: SIP Telecom was not found. File /home/.arcSIPTEL.pkginfo doesn't exist"
	if [ remove -eq 0 ]
	then
		echo	"Aborting patch installation."
	fi
	exit;
fi

if tail -1 /home/.arcSIPTEL.pkginfo 2>/dev/null | grep "REMOVE" | grep -v grep >/dev/null 2>/dev/null
then
	echo "ERROR: SIP Telecom was removed. Can't apply patch."
	if [ remove -eq 0 ]
	then
		echo	"Aborting patch installation."
	else
		echo	"Failed to remove patch."
	fi
	exit;
fi

#
# Step 2:	Confirmation
#

#
# Step 3:	Checking if Telecom is running
#

if  ps -ef | grep "ArcIPDynMgr" | grep -v grep >/dev/null
then
	echo "ERROR: SIP Telecom is running.  Please stop the Telecom Services and try again."
	if [ remove -eq 0 ]
	then
		echo	"Aborting patch installation."
	else
		echo	"Failed to remove patch."
	fi
	exit;
fi

#
# Step 4:	Checking if tts/mrcp/tone clients are running
#
if  ps -ef | grep "ttsClient" | grep -v grep >/dev/null
then
	echo "ERROR: SIP Telecom's ttsClient is running.  Please stop it and try again."
	if [ remove -eq 0 ]
	then
		echo	"Aborting patch installation."
	else
		echo	"Failed to remove patch."
	fi
	exit;
fi

if  ps -ef | grep "mrcpClient2" | grep -v grep >/dev/null
then
	echo "ERROR: SIP Telecom's mrcp client is running.  Please stop it and try again."
	if [ remove -eq 0 ]
	then
		echo	"Aborting patch installation."
	else
		echo	"Failed to remove patch."
	fi
	exit;
fi

if  ps -ef | grep "ArcConferenceMgr" | grep -v grep >/dev/null
then
	echo "ERROR: SIP Telecom's ArcConferenceMgr is running.  Please stop it and try again."
	if [ remove -eq 0 ]
	then
		echo	"Aborting patch installation."
	else
		echo	"Failed to remove patch."
	fi
	exit;
fi


if  ps -ef | grep "ArcSipTonesClientMgr" | grep -v grep >/dev/null
then
	echo "ERROR: SIP Telecom's ArcSipTonesClientMgr is running.  Please stop it and try again."
	if [ remove -eq 0 ]
	then
		echo	"Aborting patch installation."
	else
		echo	"Failed to remove patch."
	fi
	exit;
fi


if  ps -ef | grep "ArcSipTonesClient" | grep -v grep >/dev/null
then
	echo "ERROR: SIP Telecom's ArcSipTonesClient is running.  Please stop it and try again."
	if [ remove -eq 0 ]
	then
		echo	"Aborting patch installation."
	else
		echo	"Failed to remove patch."
	fi
	exit;
fi

#
# Step 5:	Checking if it is Aculab compatible installation
#
cd ~/.ISP/Telecom/Exec
#if ./ArcIPDynMgr -v | grep -i "Aculab compatible" | grep -v grep >/dev/null
if ldd ./ArcIPDynMgr | grep -i "libacu_ip" | grep -v grep >/dev/null
then
	echo "Found Aculab Compatible Call Manager."
	isAculab=1
else
	echo "Found spanDSP Compatible Call Manager."
	isAculab=0
fi

cd -

#
# Step 6:	Taking backup of existing files
#
echo	"Taking backup of the existing files.."
while [ 1 ]
do
x=`ls -alrt "patch.backup"."$today"_"$myCount" |wc -l` 2>/dev/null
	if [ "$x" -gt 0 ]
	then
		echo "Found "patch.backup"."$today"_"$myCount""
		myCount=`expr $myCount + 1`
	else
		myBkpDir="patch.backup"."$today"_"$myCount"
		echo "Backing up as $myBkpDir "
		mkdir	$myBkpDir
		mkdir -p $myBkpDir/files/.ISP/Telecom/Exec/vxi/
		mkdir -p $myBkpDir/files/.ISP/Telecom/Applications/lib/
		mkdir -p $myBkpDir/files/.ISP/Telecom/Applications/include/
		mkdir -p $myBkpDir/files/.ISP/Telecom/Tables/
		mkdir -p $myBkpDir/files/.ISP/Telecom/Operations/
		mkdir -p $myBkpDir/files/ISP/Tables/
		mkdir -p $myBkpDir/files/ISP/Exec/
		mkdir -p $myBkpDir/files/ISP/include/
		mkdir -p $myBkpDir/files/ISP/lib/

		cp modifyInittab.sh $myBkpDir/

		if [ remove -eq 0 ]
		then
			cp	installPatch.sh $myBkpDir/removePatch.sh
		else
			cp	removePatch.sh $myBkpDir/removePatch.sh
		fi

		cp .patchInfo $myBkpDir/

		break

	fi
done

#
# Step 7:	Copying files to the destination
#

cd files

for f in `find . -name \* -type f -print`
do
	if [[ $f = "./.ISP/Telecom/Exec/ArcSipCallMgr" && $isAculab -eq 0 ]]
	then

		cp ~/.ISP/Telecom/Exec/ArcIPDynMgr ../$myBkpDir/files/.ISP/Telecom/Exec/ArcSipCallMgr
		cp $f ~/.ISP/Telecom/Exec/ArcIPDynMgr
		echo "Copying $f to ~/.ISP/Telecom/Exec/ArcIPDynMgr"

	elif [[ $f = "./.ISP/Telecom/Exec/ArcSipCallMgr.yesAculab" && $isAculab -eq 1 ]]
	then

		cp ~/.ISP/Telecom/Exec/ArcIPDynMgr ../$myBkpDir/files/.ISP/Telecom/Exec/ArcSipCallMgr.yesAculab	
		cp $f ~/.ISP/Telecom/Exec/ArcIPDynMgr
		echo "Copying $f to ~/.ISP/Telecom/Exec/ArcIPDynMgr"

	elif [[ $f = "./.ISP/Telecom/Exec/ArcSipMediaMgr" && $isAculab -eq 0 ]]
	then

		cp ~/.ISP/Telecom/Exec/ArcSipMediaMgr ../$myBkpDir/files/.ISP/Telecom/Exec/ArcSipMediaMgr
		cp $f ~/.ISP/Telecom/Exec/ArcSipMediaMgr
		echo "Copying $f to ~/.ISP/Telecom/Exec/ArcSipMediaMgr"

	elif [[ $f = "./.ISP/Telecom/Exec/ArcSipMediaMgr.yesAculab" && $isAculab -eq 1 ]]
	then

		cp ~/.ISP/Telecom/Exec/ArcSipMediaMgr ../$myBkpDir/files/.ISP/Telecom/Exec/ArcSipMediaMgr.yesAculab	
		cp $f ~/.ISP/Telecom/Exec/ArcSipMediaMgr
		echo "Copying $f to ~/.ISP/Telecom/Exec/ArcSipMediaMgr"

	elif [ $f = "./.ISP/Telecom/Exec/ArcSipRedirector.mrcp" ]
	then
		cp ~/.ISP/Telecom/Exec/ArcSipRedirector ../$myBkpDir/files/.ISP/Telecom/Exec/ArcSipRedirector.mrcp	
		cp $f ~/.ISP/Telecom/Exec/ArcSipRedirector
		echo "Copying $f to ~/.ISP/Telecom/Exec/ArcSipRedirector"
	elif [ $f = "./.ISP/Telecom/Tables/FaxServer.cfg" ]
	then
		if [ -f  ~/.ISP/Telecom/Tables/FaxServer.cfg ]
		then
			echo "~/.ISP/Telecom/Tables/FaxServer.cfg currently exists. "
			echo "Copying $f to ~/.ISP/Telecom/Tables/FaxServer.cfg.`date +%y%m%d`"
			cp $f ~/$f.`date +%y%m%d`
		else
			echo "Copying $f to ~/$f"
			cp $f ~/$f
		fi
	elif [ $f = "./.ISP/Telecom/Tables/OCServer.cfg" ]
	then
		if [ -f  ~/.ISP/Telecom/Tables/OCServer.cfg ]
		then
			echo "~/.ISP/Telecom/Tables/OCServer.cfg currently exists. "
			echo "Copying $f to ~/.ISP/Telecom/Tables/OCServer.cfg.`date +%y%m%d`"
			cp $f ~/$f.`date +%y%m%d`
		else
			echo "Copying $f to ~/$f"
			cp $f ~/$f
		fi
	elif [ $f = "./.ISP/Telecom/Tables/FaxServer.cfg" ]
	then
		if [ -f  ~/.ISP/Telecom/Tables/FaxServer.cfg ]
		then
			echo "~/.ISP/Telecom/Tables/FaxServer.cfg currently exists. "
			echo "Copying $f to ~/.ISP/Telecom/Tables/FaxServer.cfg.`date +%y%m%d`"
			cp $f ~/$f.`date +%y%m%d`
		else
			echo "Copying $f to ~/$f"
			cp $f ~/$f
		fi
	else
		echo "Backing up ~/$f to ../$myBkpDir/files/$f"
		cp ~/$f ../$myBkpDir/files/$f
		echo "Copying $f to ~/$f"
		cp $f ~/$f
	fi

done

#
# Step 8:	Patch Successful Message
#

cd -

#DDN Commented 03/25/2010
#echo "Inittab will be modified to check and add entires. Please login as root."
#su - root -c "$mypwd/modifyInittab.sh"
#END: DDN Commented 03/25/2010

echo ""
echo "SUCCESS: Patch applied succesfully... It can be reverted by runing removePatch.sh located in $myBkpDir."
echo ""

if [ $myScript = "removePatch.sh" ]
then
	cat .patchInfo >>$HOME/.arcSIPTEL.patchInfo
	echo "`date` REMOVED" >>$HOME/.arcSIPTEL.patchInfo
else
	cat .patchInfo >>$HOME/.arcSIPTEL.patchInfo
	echo "`date` INSTALLED" >>$HOME/.arcSIPTEL.patchInfo
fi
#
#
