#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:	Make file to build the utility APIs and archive into 
#		libISPUtil.a. APIs are:
#		UTL_Common(), UTL_CreateFile(), UTL_AddToFile(), 
#		UTL_DeleteFile(), UTL_BusinessHours(), UTL_CopyFile(),
#		UTL_GetField(), UTL_Substring(), UTL_SendToMonitor, 
#		UTL_LogMessage, conv.o
#
# Author:	George Wenzel
# Date:		02/02/96
# Update:	02/26/96 G. Wenzel added UTL_SendToMonitor
# Update:	03/15/96 G. Wenzel added UTL_LogMessage
# Update:	2000/05/09 sja	Added UTL_accessories.c
#----------------------------------------------------------------------------


ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2


LIB = libISPUtil.a
BASE = $(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
COMMON = $(BASE)/Common
COMINC = $(COMMON)/include

LIBDIR = ${COMMON}/lib
CC = gcc -fPIC -ggdb -O -s	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
AR = ar
ARFLAGS = r
ECHO = /bin/echo

# OBJECTS =  UTL_Common.o UTL_CreateFile.o UTL_AddToFile.o \
# 	UTL_DeleteFile.o UTL_BusinessHours.o UTL_CopyFile.o \
# 	UTL_GetField.o UTL_Substring.o UTL_SendToMonitor.o \
# 	UTL_LogMessage.o conv.o UTL_accessories.o
OBJECTS =  UTL_accessories.o UTL_Common.o UTL_CreateFile.o UTL_AddToFile.o \
	UTL_DeleteFile.o UTL_BusinessHours.o UTL_CopyFile.o \
	UTL_GetField.o UTL_Substring.o UTL_SendToMonitor.o \
	UTL_LogMessage.o conv.o

SOURCES =  UTL_accessories.c UTL_Common.c UTL_CreateFile.c UTL_AddToFile.c \
	UTL_DeleteFile.c UTL_BusinessHours.c UTL_CopyFile.c\
	UTL_GetField.c UTL_Substring.o UTL_SendToMonitor.c \
	UTL_LogMessage.c conv.c


INCLUDES = -I $(COMINC)

all:	$(LIBDIR)/$(LIB)

sources: $(SOURCES)
	$(CC) -c $(INCLUDES) $@

clean:
	rm -f $(OBJECTS) $(LIBDIR)/$(LIB)

$(LIBDIR)/$(LIB): $(OBJECTS)
	@$(ECHO) "Building Library $(LIBDIR)/$(LIB)"
	@$(AR) $(ARFLAGS) $(LIBDIR)/$(LIB) $(OBJECTS)

UTL_Common.o: UTL_Common.c
	@$(ECHO) "Building UTL_Common API"
	@$(CC) -c $(INCLUDES) UTL_Common.c

UTL_CreateFile.o: UTL_CreateFile.c
	@$(ECHO) "Building UTL_CreateFile API"
	@$(CC) -c $(INCLUDES) UTL_CreateFile.c

UTL_AddToFile.o: UTL_AddToFile.c
	@$(ECHO) "Building UTL_AddToFile API"
	@$(CC) -c $(INCLUDES) UTL_AddToFile.c

UTL_BusinessHours.o: UTL_BusinessHours.c
	@$(ECHO) "Building UTL_BusinessHours API"
	@$(CC) -c $(INCLUDES) UTL_BusinessHours.c

UTL_DeleteFile.o: UTL_DeleteFile.c
	@$(ECHO) "Building UTL_DeleteFile API"
	@$(CC) -c $(INCLUDES) UTL_DeleteFile.c

UTL_CopyFile.o: UTL_CopyFile.c
	@$(ECHO) "Building UTL_CopyFile API"
	@$(CC) -c $(INCLUDES) UTL_CopyFile.c

UTL_GetField.o: UTL_GetField.c
	@$(ECHO) "Building UTL_GetField API"
	@$(CC) -c $(INCLUDES) UTL_GetField.c

UTL_Substring.o: UTL_Substring.c
	@$(ECHO) "Building UTL_Substring API"
	@$(CC) -c $(INCLUDES) UTL_Substring.c

UTL_SendToMonitor.o: UTL_SendToMonitor.c
	@$(ECHO) "Building UTL_SendToMonitor API"
	@$(CC) -c $(INCLUDES) UTL_SendToMonitor.c

UTL_LogMessage.o: UTL_LogMessage.c
	@$(ECHO) "Building UTL_LogMessage API"
	@$(CC) -c $(INCLUDES) UTL_LogMessage.c

conv.o: conv.c
	@$(ECHO) "Building conv.c - conversion routines."
	@$(CC) -c $(INCLUDES) conv.c

UTL_accessories.o: UTL_accessories.c
	@$(ECHO) "Building UTL_accessories.c - UTL accessory routines."
	@$(CC) -c $(INCLUDES) UTL_accessories.c

$(OBJECTS): $(COMINC)/ISPCommon.h

$(OBJECTS): $(COMINC)/ispinc.h

$(OBJECTS): $(COMINC)/COMmsg.h

$(OBJECTS): $(COMINC)/COM_Mnemonics.h
