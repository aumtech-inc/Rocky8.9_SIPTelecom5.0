#!/usr/bin/make -e 
#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:	Make file for the common utilities library libUTILS.a 
# Author:	ARC development staff
# Date:		05/31/95
# Update:	12/05/95 G. Wenzel updated for ISP 2.2 
# Update:	12/10/95 G. Wenzel added support.c and util_heartbeat.c
# Update:	02/17/96 added TELINC SNAINC TCPINC to INCLUDE list
# Update:	05/22/96 G. Wenzel removed messages.h from HEADERS (ssio stuff)
# Update:	06/10/97 sja	Added WSS_Mnemonics.h
# Update:	08/07/97 djb	Added SNMP includes and API source 
# Update:	08/07/97 djb	Removed SNMP includes and API source 
# Update:	06/17/99 gpw	Added util_get_name_value.c
# Update:	06/17/99 gpw	Added util_get_cfg_filename.c
# Update:	06/21/99 gpw	chnaged util_heartbeat to util_send_to_monitor
# Update:	08/01/99 gpw	change Telecom to DTelecom
# Update:	08/24/99 gpw	added ResourceMgrAPIs
#----------------------------------------------------------------------------
CC		=	gcc	-fPIC -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
ECHO		=	echo
MV		=	mv
AR		=	ar
LIB             =       -lnsl  -lsocket -lgen

ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2

BASE=$(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
B=$(ISP_BASE_DIR)/$(ISP_VERSION)
COMMON=$(BASE)/Common
COMINC=$(COMMON)/include
COMLIB=${COMMON}/lib
TCP=$(BASE)/TCP
SNA=$(BASE)/SNA
TEL=${BASE}
WSS="/home/dan/isp2.2/SIPTelecom3.6/loggingServices/logForwarding/server"
TCPINC=$(TCP)/include
SNAINC=$(SNA)/include
TELINC=$(TEL)/include
WSSINC=$(WSS)/include
RESINC=$(COMMON)/ResourceMgr

UTLLIB=${COMMON}/lib/libUTILS.a
HANDLIB=${COMMON}/lib/libhandshake.a

INCLUDE=-I$(COMINC) -I$(TELINC) -I$(SNAINC) -I$(TCPINC) -I$(WSSINC) -I$(RESINC)

HEADERS=$(COMINC)/COMmsg.h $(COMINC)/LOGmsg.h $(COMINC)/ispinc.h $(COMINC)/isp_rpc.h 

CFLAGS=-O -s
#CFLAGS=-g

HAND_OBJECTS	= 	obj/turnkey_handshake.o

UTIL_OBJECTS	=	 \
			obj/ISPglb.o \
			obj/util_options.o \
			obj/util_sleep.o \
			obj/support.o \
			obj/util_send_to_monitor.o \
			obj/util_get_name_value.o \
			obj/util_get_cfg_filename.o \
			obj/loadapis.o  \
			obj/ResourceMgrAPIs.o 

all:		util hand

util: $(UTIL_OBJECTS)
		@$(ECHO) "Building Library $(UTLLIB)"
		@$(AR) r $(UTLLIB) $(UTIL_OBJECTS)

hand: $(HAND_OBJECTS)
		@$(ECHO) "Building Library $(HANDLIB)"
		@$(AR) r $(HANDLIB) $(HAND_OBJECTS)

clean:
	rm -f $(UTIL_OBJECTS) $(UTLLIB) $(HAND_OBJECTS) $(HANDLIB)


obj/ISPglb.o: 		ISPglb.c 	$(HEADERS)
			@$(ECHO) "Building ISPglb.o"
			@$(CC) $(CFLAGS) $(INCLUDE) -c ISPglb.c
			@$(MV) *.o obj

obj/util_options.o: 	util_options.c 	$(HEADERS)
			@$(ECHO) "Building util_options.o"
			@$(CC) $(CFLAGS) $(INCLUDE) -c util_options.c
			@$(MV) *.o obj

obj/util_sleep.o: 	util_sleep.c $(HEADERS)
			@$(ECHO) "Building util_sleep.o"
			@$(CC) $(CFLAGS) $(INCLUDE) -c util_sleep.c
			@$(MV) *.o obj

obj/support.o: 		support.c $(HEADERS)
			@$(ECHO) "Building support.o - shared memory routines"
			@$(CC) $(CFLAGS) $(INCLUDE) -c support.c
			@$(MV) *.o obj

obj/util_send_to_monitor.o: 	util_send_to_monitor.c $(HEADERS)
			@$(ECHO) "Building util_send_to_monitor.o"
			@$(CC) $(CFLAGS) $(INCLUDE) -c util_send_to_monitor.c
			@$(MV) *.o obj

obj/util_get_name_value.o: util_get_name_value.c $(HEADERS)
			@$(ECHO) "Building util_get_name_value.o - get name=value routine"
			@$(CC) $(CFLAGS) $(INCLUDE) -c util_get_name_value.c
			@$(MV) *.o obj

obj/util_get_cfg_filename.o: util_get_cfg_filename.c $(HEADERS)
			@$(ECHO) "Building util_get_cfg_filename.o - get filename using a token"
			@$(CC) $(CFLAGS) $(INCLUDE) -c util_get_cfg_filename.c
			@$(MV) *.o obj

obj/turnkey_handshake.o: turnkey_handshake.c $(HEADERS)
			@$(ECHO) "Building turnkey_handshake.o"
			@$(CC) $(CFLAGS) $(INCLUDE) -c turnkey_handshake.c
			@$(MV) *.o obj

obj/loadapis.o: loadapis.c $(HEADERS)
			@$(ECHO) "Building loadapis.o"
			@$(CC) $(CFLAGS) $(INCLUDE) -c loadapis.c
			@$(MV) *.o obj

obj/ResourceMgrAPIs.o: ResourceMgrAPIs.c $(HEADERS)
			@$(ECHO) "Building ResourceMgrAPIs.o"
			@$(CC) $(CFLAGS) $(INCLUDE) -c ResourceMgrAPIs.c
			@$(MV) *.o obj
