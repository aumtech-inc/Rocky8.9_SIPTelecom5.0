#----------------------------------------------------------------------------
# Program:	Makefile
#----------------------------------------------------------------------------

ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2

BASE = $(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6/loggingServices/logForwarding
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
