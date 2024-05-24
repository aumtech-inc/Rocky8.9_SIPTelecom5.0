#! /bin/sh -e

trap 'echo ; exit 13' TERM INT

# Make sure run by sudo

if [ "$UID" -ne 0 ]
then
	echo "Please run this script using 'sudo' or as root (su)."
	exit 1
fi

if [ ! -d /usr ]
then
	mkdir /usr
fi

if [ ! -d /usr/local ]
then
	mkdir /usr/local
fi

if [ ! -d /usr/local/share ]
then
	mkdir /usr/local/share
fi

if [ ! -d /usr/local/share/macrovision ]
then
	mkdir /usr/local/share/macrovision
fi

install bin/*.yaa /usr/local/share/macrovision/

#
# Script to configure trusted storage directory for London beta 1
# release.
#
# Figure out platform and where the directory needs to be, and report
# platform info to user.
#

echo 
echo "Checking system..."

if [ -d '/Library/Preferences' ]
then
   platform="Mac OS X"
   rootpath="/Library/Preferences/FLEXnet Publisher"
else
   platform=`uname`
   rootpath="/usr/local/share/macrovision/storage"
fi

echo "Configuring for $platform, Trusted Storage path $rootpath..."

set +e

#
# Check for existance of directory, if not present try to create
#

if [ -d "$rootpath" ]
then
   echo "$rootpath already exists..."
else
   echo "Creating $rootpath..."
   mkdir -p "$rootpath"
fi

#
# Set correct permissions on directory
#

echo "Setting permissions on $rootpath..."
chmod 777 "$rootpath"
rc=$?
if [ $rc -ne 0 ]
then
   echo "Unable to set permissions on $rootpath, chmod returned error $rc."
   exit $rc
fi

echo "Permissions set..."

# All done.

echo "Configuration completed successfully."
echo 

exit 0
