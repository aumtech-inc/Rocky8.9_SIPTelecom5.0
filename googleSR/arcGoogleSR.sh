#!/bin/bash
#	File
#
#	Description:
#		This scripts starts the Aumtech's Google Speech Rec jar.
#		It is started by ArcIPResp on starttel. 
#		It then starts GoogleSpeechRec.jar.
#
#	Configuration:
#		program_name: 	arcGoogleSR.sh 
#
#------------------------------------------------------------------------------
# cleanUp()
#------------------------------------------------------------------------------
cleanUp()
{
	echo "$0: `date` - Exiting.  Killing ${program_name}; pid ${googleJarPid}." 
	kill -0 ${googleJarPid}
	if [ $? -eq 0 ]
	then
		kill ${googleJarPid}
	fi

	if [ -p "${gsrFifo}" ]
	then
		echo "$0: `date` - Removing ${gsrFifo}." 
		rm  ${gsrFifo} 
	fi

	exit 0
} # END: cleanUp

#------------------------------------------------------------------------------
# main()
#------------------------------------------------------------------------------
ulimit -s unlimited >/dev/null 2>&1
program_name="GoogleSpeechRec.jar"
vxi_version="3.6"

case $1 in
	-v)
	echo "$0 $vxi_version"
	exit
	;;
	-V)
	echo "$0 $vxi_version"
	exit
	;;
	*)
	;;
esac

if [ $# -ne 2 ]; then
	echo "Usage: $0 <resp ID> <my ID>"
	exit
fi

gsrFifo="/tmp/ArcGSRRequestFifo"
if [ -p "${gsrFifo}" ]; then
	rm ${gsrFifo}
fi

mkfifo ${gsrFifo}

# trap cleanUp SIGHUP SIGINT SIGTERM SIGKILL

cd ${ISPBASE}/Telecom/Exec

respId="${1}"
myId="${2}"
myPid="$$"

declare -i googleJarPid=0

while [ 1 ]
do
		if [ ${googleJarPid} -eq 0 ]
		then
				java -jar ${program_name} >>nohup.out 2>&1 &
				googleJarPid=$!
				echo "$0: `date` - Started GoogleSpeechRec.jar, pid ${googleJarPid}." 
		fi

		#
		# Now we check if the pid is not there 
		#
		if [ ${googleJarPid} -gt 0 ]
		then
			kill -0 ${googleJarPid}
			if [ $? -ne 0 ]
			then
				echo "$0: `date` - Restarting GoogleSpeechRec.jar pid ${googleJarPid} stopped." 
				java -jar ${program_name} >>nohup.out 2>&1 &
				googleJarPid=$!

				echo "$0: `date` - Started GoogleSpeechRec.jar pid ${googleJarPid}." 
				sleep 0.1
			fi
		fi

		sleep 0.1
done

