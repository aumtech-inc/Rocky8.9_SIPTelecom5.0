#------------------------------------------------------------------------------
# File:		rmCores.sh
# Author:	Aumtech, Inc.
# Purpose:	Remove all the core file from the default core directory.
#         	Must be run by the 'root' user.
#------------------------------------------------------------------------------

if [ `id -nu` != "root" ]
then
	echo "Unable to execute $0.  Must have root privilege."
	exit
fi

cd /var/lib/systemd/coredump
rm -fv core.*
