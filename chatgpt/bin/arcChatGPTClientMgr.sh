#!/bin/bash
#------------------------------------------------------------------------------
# File:     ArcChatGPTClientMgr.sh
# Purpose:  Set the virtual environment and start the arcChatGPTClientMgr.
# Author:   Aumtech, Inc.
#------------------------------------------------------------------------------


keepProcessing=1

#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
function handleSignal()
{
	keepProcessing=0
	myLog "Got signal.  1 is $1"
} # END: handleSignal

#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
function myLog()
{
	d=`date "+%F %T"`
	echo "${d}: ${*}"   >${outFile}

} # END: myLog()

#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
function getNumClientstToRun()
{
	REQUESTS_PER_CLIENT=12
	rdt="${ISPBASE}/Telecom/Tables/ResourceDefTab"

	count=`cat ${rdt} | wc -l`


	if [ $(( count % 12 )) -eq 0 ]
	then
		numClients=$(( count / REQUESTS_PER_CLIENT ))
	else
		numClients=$(( count / REQUESTS_PER_CLIENT + 1))
	fi
	if [ ${numClients} -eq 0 ]
	then
		numClients=1
	fi

	return ${numClients}
} # END: getNumClientstToRun()

#------------------------------------------------------------------------------
# main execution starts here
#------------------------------------------------------------------------------
version="3.9.1"

case $1 in
	-v)
		echo "ArcChatGPTClient $version"
		exit
		;;
	-V)
		echo "ArcChatGPTClient $version"
		exit
		;;
	*)
		;;
esac


if [ ${#ISPBASE} -eq 0 ]
then
	echo "Environment variable ISPBASE is not set.  Correct and retry"
	exit
fi

cd $ISPBASE/Telecom/Exec/chatgpt
outFile="out.txt"
. venv/bin/activate

trap handleSignal SIGINT
trap handleSignal SIGHUP
trap handleSignal SIGQUIT
trap handleSignal SIGABRT
trap handleSignal SIGTERM

getNumClientstToRun
numClientst=$?

declare -a pidArray

i=0
while [ ${i} -lt ${numClients} ]	# get the # of clients to be started
do
	python arcChatGPTClient.py &
	npid=$!
	if [ ${npid} -ne 0 ]
	then
		pidArray[${i}]=${npid}
		myLog "Started arcChatGPTClient.py [" ${i} "]"
		i=$(( i + 1 ))
	else
		myLog "Failed to start arcChatGPTClient.py [" ${i} "]  Exiting."
		exit
	fi
done

while [ ${keepProcessing} -eq 1 ] 		# monitor the clients; restart if necessary
do
#	myLog "in while pidArray[${#pidArray[@]}: ${pidArray[@]}"

	i=0
	while [ ${i} -lt ${#pidArray[@]} ]
	do
		pid=${pidArray[${i}]}
		kill -0 ${pid}
		rc=$?
		if [ ${rc} -ne 0 ]
		then
			myLog "pid ${pid} stopped. Restarting."

			python arcChatGPTClient.py &
			newPid=$!
			if [ ${newPid} -ne 0 ]
			then
				pidArray[${i}]=${newPid}
				myLog "Restarted arcChatGPTClient.py [${i},${newPid}]"
				break
			fi
			i=$(( i + 1 ))
		else
			i=$(( i + 1 ))
		fi
		sleep 2

	done
done

myLog "Shutting down."

for pid in ${pidArray[@]} 		# send a kill to the clients
do
	kill -15 ${pid}
	rc=$?
	if [ ${rc} -ne 0 ]
	then
		myLog "Failed to send kill to client PID ${pid}."
	else
		myLog "Successfully sent kill to client PID ${pid}."
	fi
done
