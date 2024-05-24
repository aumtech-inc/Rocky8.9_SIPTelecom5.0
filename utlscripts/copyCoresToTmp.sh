#------------------------------------------------------------------------------
# File:		copyCoresToTmp.sh
# Author:	Aumtech, Inc.
# Purpose:	For each core.zst or core.lz4 file in the default 'core' directory,
#         	This will:
#         	1) extract/uncompress the core file from the file,
#         	2) copy the newly-created core file to /tmp, and
#         	3) remove the newly-crated core file from the 'core' directory.         	
#
#			The ownership of the /tmp/core* files will be changed to the arc
#			user.
#
#         	Must be run by the 'root' user.
#------------------------------------------------------------------------------

if [ `id -nu` != "root" ]
then
	echo "Unable to execute $0.  Must have root privilege."
	exit
fi

cd /var/lib/systemd/coredump
rm -f /tmp/core*

if [ `ls -1tr *zst 2>/dev/null | wc -l` -ge 1 ]
then
	ls -1tr *zst | while read x
	do
		zstd -d $x
		echo "cp -p ${x%.zst} /tmp"
		cp -p ${x%.zst} /tmp
		chmod 777 /tmp/${x%.zst}
		echo 
		file /tmp/${x%.zst}
	
		rm ${x%.zst}

	done
fi

if [ `ls -1tr *lz4 2>/dev/null | wc -l` -ge 1 ]
then
	ls -1tr *lz4 | while read x
	do
		lz4 $x
		echo "cp -p ${x%.lz4} /tmp"
		cp -p ${x%.lz4} /tmp
		chmod 777 /tmp/${x%.lz4}
		echo 
	
		rm ${x%.lz4}

	done
fi

chown arc /tmp/core.* 2>/dev/null
