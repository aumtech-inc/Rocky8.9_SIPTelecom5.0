#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:	Master make file for cleaning up and building all Common
#		libraries
#
# Author:	Unkown
# Date:		09/17/94
# Update:	02/17/96 G. Wenzel used environment variables to define dir's
# Update:	02/26/96 G. Wenzel changed rdb_rules to sc_rule for dir change
# Update:	03/20/96 G. Wenzel removed gtlogcfg, lumped with Log
# Update:	11/06/97 D. Barto  added accessories
# Update:	06/30/99 G. Wenzel added ResourceMgr, ResourceMgr.clean
# Update: 2000/03/17 sja Added saUtils
# Update: 	11/06/01 apj Added flexlm.
#----------------------------------------------------------------------------

ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2

BASE = $(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6/loggingServices/realTimeReports
COMMON=$(BASE)/Common

all:	client_api \
		client_src \
		server_api \
		server_src

clean:	client_api.clean \
		client_src.clean \
		server_api.clean \
		server_src.clean

client_api:		Always
	( cd $(BASE)/client/apisrc; make -f Makefile all)

client_src:		Always
	( cd $(BASE)/client/src; make -f Makefile all)

server_api:		Always
	( cd $(BASE)/server/apisrc; make -f Makefile all)

server_src:		Always
	( cd $(BASE)/server/src; make -f Makefile all)

client_api.clean:		Always
	( cd $(BASE)/client/apisrc; make -f Makefile clean)

client_src.clean:		Always
	( cd $(BASE)/client/src; make -f Makefile clean)

server_api.clean:		Always
	( cd $(BASE)/server/apisrc; make -f Makefile clean)

server_src.clean:		Always
	( cd $(BASE)/server/src; make -f Makefile clean)

Always:
