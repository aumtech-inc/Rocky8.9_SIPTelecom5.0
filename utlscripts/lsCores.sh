#------------------------------------------------------------------------------
# File:		lsCores.sh
# Author:	Aumtech, Inc.
# Purpose:	List the contents of the default 'core' directory.
#
#         	May be run by any user.
#------------------------------------------------------------------------------

set -x
ls -latr /var/lib/systemd/coredump/
