clear

make clean

make isp_monitor
if [ $? -eq 0 ]
then
	set -x
	cp isp_monitor /tmp/tel_monitor
	set +x
else
	echo
	echo "ZONK!!!!"
	echo
	exit
fi

make textMonitor
if [ $? -eq 0 ]
then
	set -x
	cp textMonitor /tmp
	set +x
else
	echo
	echo "ZONK!!!!"
	echo
fi
