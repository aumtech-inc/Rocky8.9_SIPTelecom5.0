#----------------------------------------------------------------------------
# Program:      Makefile
# Purpose:      Make file for building the ARC monitor
#
# Author:   	George Wenzel
# Date:		12/10/95
# Update:  	02/02/96 G. Wenzel changed tel_monitor to isp_monitor
# Update:  	02/17/96 added include directories TCPINC and SNAINC
#		required to include appropiate Mnemonic files.
# Update:  	11/20/98 D. Barto  added -DNO_LOGX_MSG to the link
# Update:  	06/17/99 gpw changed RESMGR to point to Common, not Telecom
# Update:  	08/24/99 gpw removed make of ResourceMgrAPIs; now in libUTILS.a
# Update: 08/16/01 mpb removed LiS.
# Update: 10/30/02 djb changed -lcurses to /usr/lib/libncurses.a
#----------------------------------------------------------------------------

CC = gcc
#CC = gcc -D ISP_LOG		# to enable logging for debug purposes
ECHO = echo
CFLAGS = -c -ggdb
CFLAGS_E = -ggdb  -o
AR = ar
ARFLAGS = -r

ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2

BASE = $(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
COMINC = $(BASE)/Common/include

LIBPATHS  =  -L ${BASE}/Common/lib

SYSLIBS  = -lncurses
UTLLIBS  = -lUTILS  -lISPLog 
#UTLLIBS  = -lUTILS 

INCLUDES = -I .  -I $(COMINC) 

EXECUTABLES = isp_monitor textMonitor debugShm


all :  $(EXECUTABLES)

clean : 
	rm -f $(EXECUTABLES) *.o

isp_monitor : isp_monitor.c  
	@$(ECHO) "Building ARC monitor"	
	$(CC) $(CFLAGS_E) isp_monitor isp_monitor.c $(INCLUDES) $(LIBPATHS) $(UTLLIBS) $(SYSLIBS)

debugShm : debugShm.c  
	@$(ECHO) "Building debugShm"	
	$(CC) $(CFLAGS_E) debugShm debugShm.c $(INCLUDES) $(LIBPATHS) -lUTILS

textMonitor : textMonitor.c
	@$(ECHO) "Building Text monitor"
	$(CC)  -o textMonitor -ggdb textMonitor.c -I .              \
			-I ${HOME}/isp2.2/SIPTelecom3.6/Common/include          \
			-L ${HOME}/isp2.2/SIPTelecom3.6/Common/lib       \
			-lUTILS  -lISPLog -lcurses
			

$(EXECUTABLES): $(COMINC)/BNS.h

$(EXECUTABLES): $(COMINC)/ISPSYS.h

$(EXECUTABLES): $(COMINC)/resp.h

$(EXECUTABLES): $(COMINC)/shm_def.h
