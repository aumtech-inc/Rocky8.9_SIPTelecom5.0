#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:	Make file for building Workstation Server API  library.
#
#
# Update:	03/04/97 sja	Added WSS_RecvFile.c
# Update:	03/28/97 sja	Added WSS_SetGlobal.c & WSS_GetGlobal.c
# Update:	03/28/97 sja	Added WSS_ClearQue.c
# Update:	04/15/97 sja	Ported to the "Lite" version
# Update:	06/02/97 sja	Added ss_GetGlobalString
# Update:	05/04/98 gpw	Removed references to Common include files
# Update:	09/11/12 djb	Changed to loggingServices
#----------------------------------------------------------------------------

LIB = liblsServer.a

ISP_BASE_DIR=/home/dev
ISP_VERSION=isp2.2

BASE = $(ISP_BASE_DIR)/$(ISP_VERSION)/loggingServices/testApp
WSSINC = $(BASE)/include
LIBDIR = $(BASE)/lib
#CFLAGS = -g
#CFLAGS = -ggdb3 -D_LARGEFILE64_SOURCE
CFLAGS = -ggdb3 
CC = gcc $(CFLAGS)
ECHO = echo
AR = ar
ARFLAGS = r

APISRC =  	\
		ss_Common.c 		\
		ss_Exit.c 		\
		ss_GetGlobal.c 		\
		ss_GetGlobalString.c	\
		ss_Init.c 		\
		ss_RecvData.c 		\
		ss_RecvFile.c 		\
		ss_SendData.c 		\
		ss_SendFile.c 		\
		ss_SetGlobal.c

OBJECTS = ${APISRC:.c=.o}

INCLUDES = -I $(WSSINC) 

all:	${LIBDIR}/${LIB}

lint:
	lint $(CFLAGS) $(INCLUDES) $(APISRC)

clean:
	rm -f $(OBJECTS) $(LIBDIR)/$(LIB)

$(LIBDIR)/$(LIB) : $(OBJECTS) 
	@$(ECHO) "Building $(LIBDIR)/$(LIB)"	
	@$(AR) $(ARFLAGS) $(LIBDIR)/$(LIB)  $(OBJECTS)

ss_Common.o: ss_Common.c
	@$(ECHO) "Building ss_Common.c API"	
	@$(CC) -c $(INCLUDES) ss_Common.c

ss_Init.o: ss_Init.c
	@$(ECHO) "Building ss_Init.c API"	
	@$(CC) -c $(INCLUDES) ss_Init.c

ss_Exit.o: ss_Exit.c
	@$(ECHO) "Building ss_Exit.c API"	
	@$(CC) -c $(INCLUDES) ss_Exit.c

ss_SendData.o: ss_SendData.c
	@$(ECHO) "Building ss_SendData.c API"	
	@$(CC) -c $(INCLUDES) ss_SendData.c

ss_RecvData.o: ss_RecvData.c
	@$(ECHO) "Building ss_RecvData.c API"	
	@$(CC) -c $(INCLUDES) ss_RecvData.c

ss_SendFile.o: ss_SendFile.c
	@$(ECHO) "Building ss_SendFile.c API"	
	@$(CC) -c $(INCLUDES) ss_SendFile.c

ss_RecvFile.o: ss_RecvFile.c
	@$(ECHO) "Building ss_RecvFile.c API"	
	@$(CC) -c $(INCLUDES) ss_RecvFile.c

ss_SetGlobal.o: ss_SetGlobal.c
	@$(ECHO) "Building ss_SetGlobal.c API"	
	@$(CC) -c $(INCLUDES) ss_SetGlobal.c

ss_GetGlobalString.o: ss_GetGlobalString.c
	@$(ECHO) "Building ss_GetGlobalString.c API"	
	@$(CC) -c $(INCLUDES) ss_GetGlobalString.c

ss_GetGlobal.o: ss_GetGlobal.c
	@$(ECHO) "Building ss_GetGlobal.c API"	
	@$(CC) -c $(INCLUDES) ss_GetGlobal.c

