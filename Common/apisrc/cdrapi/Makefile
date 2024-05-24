#!/usr/bin/make -e 
#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:	Make file to build call detail record APIs and archive them
#		into common library: libISPCDR.a
#		CDR_Common(), CDR_ServiceTran(), CDR_CustomEvent()
# Author:	George Wenzel
# Date:		02/02/96
# Update:	04/15/99 gpw removed unsupported APIs
#----------------------------------------------------------------------------

ISP_BASE=/home/dan
ISP_VERSION=isp2.2

BASE = $(ISP_BASE)/$(ISP_VERSION)/SIPTelecom3.6
COMMON = $(BASE)/Common
COMINC = $(COMMON)/include
LIB = libISPCDR.a
ECHO   =  /bin/echo

LIBDIR = ${COMMON}/lib
CC = gcc -O -s	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
AR = ar
ARFLAGS = r

OBJECTS =  CDR_Common.o CDR_ServiceTran.o  CDR_CustomEvent.o 

INCLUDES = -I $(COMINC) -I $(COMMON)/Log

all:	$(LIBDIR)/$(LIB)

clean:
	rm -f $(OBJECTS) $(LIBDIR)/$(LIB)

$(LIBDIR)/$(LIB): $(OBJECTS)
	@$(ECHO) "Building Library $(LIB)"
	@$(AR) $(ARFLAGS) $(LIBDIR)/$(LIB) $(OBJECTS)

CDR_Common.o: CDR_Common.c
	@$(ECHO) "Building CDR_Common API"
	@$(CC) -c $(INCLUDES) CDR_Common.c

CDR_ServiceTran.o: CDR_ServiceTran.c
	@$(ECHO) "Building CDR_ServiceTran API"
	@$(CC) -c $(INCLUDES) CDR_ServiceTran.c

CDR_CustomEvent.o: CDR_CustomEvent.c
	@$(ECHO) "Building CDR_CustomerEvent API"
	@$(CC) -c $(INCLUDES) CDR_CustomEvent.c

$(OBJECTS): $(COMINC)/ispinc.h

$(OBJECTS): $(COMINC)/COMmsg.h

CDR_CustomEvent.o: $(COMINC)/CEVmsg.h

$(OBJECTS): $(COMINC)/CDRmsg.h

$(OBJECTS): $(COMINC)/COM_Mnemonics.h

$(OBJECTS): $(COMINC)/ISPCommon.h
