#!/bin/ksh
#
#
echo "Running   $0"
echo -n "Enter the parent build number:"
read parentBuild


#if [ !$parentBuild ]
#then
	#echo "Incomplete info.  aborting"
	#exit
#fi
#
echo -n "Enter the patch number:"
read patchNumber
#if [ !$patchNumber ]
#then
	#echo "Incomplete info.  aborting"
	#exit
#fi

echo "Parent 	Build: $parentBuild" 	>.patchInfo
echo "Patch 	Number: $patchNumber" 	>>.patchInfo
echo "Patch 	Date: `date`" 			>>.patchInfo

if [ -f patch.sip.b_$parentBuild.p_$patchNumber.`date +%m%d%Y`.tar.Z ]
then
	rm patch.sip.b_$parentBuild.p_$patchNumber.`date +%m%d%Y`.tar.Z
fi

mkdir patch.sip.b_$parentBuild.p_$patchNumber.`date +%m%d%Y`
cp -rp .patchInfo README.txt release_notes.txt files installPatch.sh patch.sip.b_$parentBuild.p_$patchNumber.`date +%m%d%Y`
tar -zcvf patch.sip.b_$parentBuild.p_$patchNumber.`date +%m%d%Y`.tar.Z patch.sip.b_$parentBuild.p_$patchNumber.`date +%m%d%Y`
rm -rf patch.sip.b_$parentBuild.p_$patchNumber.`date +%m%d%Y`
