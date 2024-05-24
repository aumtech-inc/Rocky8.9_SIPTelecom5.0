#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:	Make file for building Workstation Client API  library.
#
#
#
# Update:	03/15/97 sja	Ported to the "Lite" version
#----------------------------------------------------------------------------

DBPORT		= 6790
LOGPORT		= 6791
ARCPORT		= 6792

LIBLOGCOMM	= liblogComm.a

ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2

#-----------------------------------------------------------------
# You should not have to edit anything bellow this line.
#-----------------------------------------------------------------

BASE = $(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
LSBASE = ${BASE}/loggingServices/logForwarding
WSCINC = ${LSBASE}/client/include
LIBDIR = ${LSBASE}/lib
#CFLAGS = -O -s
#CFLAGS = -g -D_LARGEFILE64_SOURCE 
CFLAGS = -g 
# CC = gcc -m32 $(CFLAGS)
CC = gcc $(CFLAGS)
ECHO = echo
AR = ar
ARFLAGS = r

APISRC =  	\
		sc_Common.c 	\
		sc_Exit.c 	\
		sc_GetGlobal.c \
		sc_RecvData.c 	\
		sc_RecvFile.c	\
		sc_SendData.c 	\
		sc_SendFile.c 	\
		sc_SetGlobal.c

OBJECTS = 	${APISRC:.c=.o}

INCLUDES = -I ${WSCINC}  -I . -I../include 

all:	$(LIBDIR)/$(LIBLOGCOMM) 

lint:
	lint $(CFLAGS) $(INCLUDES) $(APISRC)

clean:
	-rm -f $(OBJECTS) $(LIBDIR)/$(LIBLOGCOMM)

$(LIBDIR)/$(LIBLOGCOMM) : $(OBJECTS) 
	@-rm -f sc_Init.o 2>/dev/null
	@$(ECHO) "Building sc_Init.c API"	
	@$(CC) -DSERVER_PORT=${LOGPORT} -c $(INCLUDES) sc_Init.c
	@$(ECHO) "Building $(LIBDIR)/$(LIBLOGCOMM)"	
	@$(AR) $(ARFLAGS) $(LIBDIR)/$(LIBLOGCOMM)  $(OBJECTS) sc_Init.o

sc_Common.o: sc_Common.c
	@$(ECHO) "Building sc_Common.c API"	
	@$(CC) -c $(INCLUDES) sc_Common.c

sc_Init.o: sc_Init.c
	@$(ECHO) "Building sc_Init.c API"	
	@$(CC) -DSERVER_PORT=${SERVER_PORT} -c $(INCLUDES) sc_Init.c

sc_Exit.o: sc_Exit.c
	@$(ECHO) "Building sc_Exit.c API"	
	@$(CC) -c $(INCLUDES) sc_Exit.c

sc_SendData.o: sc_SendData.c
	@$(ECHO) "Building sc_SendData.c API"	
	@$(CC) -c $(INCLUDES) sc_SendData.c

sc_RecvData.o: sc_RecvData.c
	@$(ECHO) "Building sc_RecvData.c API"	
	@$(CC) -c $(INCLUDES) sc_RecvData.c

sc_RecvFile.o: sc_RecvFile.c
	@$(ECHO) "Building sc_RecvFile.c API"	
	@$(CC) -c $(INCLUDES) sc_RecvFile.c

sc_SendFile.o: sc_SendFile.c
	@$(ECHO) "Building sc_SendFile.c API"	
	@$(CC) -c $(INCLUDES) sc_SendFile.c

sc_SetGlobal.o: sc_SetGlobal.c
	@$(ECHO) "Building sc_SetGlobal.c API"	
	@$(CC) -c $(INCLUDES) sc_SetGlobal.c

sc_GetGlobal.o: sc_GetGlobal.c
	@$(ECHO) "Building sc_GetGlobal.c API"	
	@$(CC) -c $(INCLUDES) sc_GetGlobal.c
