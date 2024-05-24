# To install the patch,

1. Login as  user where Telecom Services are installed.
2. cd to unzipped patch directory
3. Stop all Telecom Service related processes.
4. Run  "./installPatch.sh"


# Things to do manually (Mainly the config settings that were not present in last certified package)
None

NOTE: 1.	Any patch can be reverted by running removePatch.sh located in the backup directory as specified at the end of installation."
	  2.	cat $HOME/.arcSIPTEL.patchInfo to get the version and status of
			the patch.

