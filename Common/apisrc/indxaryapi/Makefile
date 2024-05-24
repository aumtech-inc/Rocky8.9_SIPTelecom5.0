#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:	Make file to build "index array" library: libISPIndary.a.
#		APIs are ARY_IndxaryInit() and ARY_IndxaryRetrieve(). 
#
#
# Written by:	ISP 2.1 Development staff.
# Orig date:	Aug 9, 1994
#
# Last Chg by:	George Wenzel
# Last Update:	Feb 2, 1996
#----------------------------------------------------------------------------

ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2


LIB = libISPIndxary.a
BASE = $(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
COMMON = $(BASE)/Common
COMINC = $(BASE)/Common/include

LIBDIR = ${COMMON}/lib
CC = gcc -O -s	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
AR = ar
ARFLAGS = r
ECHO = /bin/echo

OBJECTS = ARY_IndxaryInit.o ARY_IndxaryRetrieve.o

INCLUDES = -I $(COMINC)

all:	$(LIBDIR)/$(LIB)

clean:
	rm -f $(OBJECTS) $(LIBDIR)/$(LIB)

$(LIBDIR)/$(LIB): $(OBJECTS)
	@$(ECHO) "Building Library $(LIBDIR)/$(LIB)"
	@$(AR) $(ARFLAGS) $(LIBDIR)/$(LIB) $(OBJECTS)

ARY_IndxaryInit.o: ARY_IndxaryInit.c
	@$(ECHO) "Building ARY_IndxaryInit API";
	@$(CC) -c $(INCLUDES) ARY_IndxaryInit.c

ARY_IndxaryRetrieve.o: ARY_IndxaryRetrieve.c
	@$(ECHO) "Building ARY_IndxaryRetrieve API";
	@$(CC) -c $(INCLUDES) ARY_IndxaryRetrieve.c

$(OBJECTS): $(COMINC)/IndexArray.h

$(OBJECTS): $(COMINC)/COM_Mnemonics.h

$(OBJECTS): $(COMINC)/COMmsg.h

$(OBJECTS): $(COMINC)/ispinc.h
