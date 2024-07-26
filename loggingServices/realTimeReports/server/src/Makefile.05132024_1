#----------------------------------------------------------------------------
# Program:      Makefile
# Purpose:      Make file for building the listener for WSS apps.
# Author:   	sja
# Date:    	01/22/97 sja	Created the Makefile
# Date:    	01/24/97 sja	Made CC silent
# Date:    	06/02/97 sja	Renamed db_listen to db_listener
# Date:    	07/22/97 sja	Added arc_listen
# Date:    	08/18/99 gpw	Changed arcLic to arcLicense (Globetrotter)
# Date:    	1999/11/09 sja	arc_listen is now built using arc_listen.c
# Update:   09/11/12 djb    Changed to loggingServices
#----------------------------------------------------------------------------


ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2

BASE 	= ${ISP_BASE_DIR}/${ISP_VERSION}/SIPTelecom3.6
LSBASE 	= ${BASE}/loggingServices/realTimeReports
COMINC 	= ${BASE}/Common/include
WSSINC 	= ${LSBASE}/server/include
LIBDIR  = ${LSBASE}/lib

# CFLAGS	= -s -O
CFLAGS	= -ggdb3

CC 	= @gcc ${CFLAGS}
ECHO 	= @echo
AR 	= ar
ARFLAGS = r

INCLUDES = -I $(WSSINC) -I $(COMINC)

LIBPATHS = 	-L ${LIBDIR}  -L ${BASE}/Common/lib

LIBS	= 	-lgaUtils -lISPLog

EXECS 	= 	arcRTRListener arcRTRServer

all: $(EXECS)

install:
	scp arcRTRServer arc@10.0.10.236:/home/arc/.ISP/Global/Exec
	scp arcRTRListener arc@10.0.10.236:/home/arc/.ISP/Global/Exec

arcRTRListener: com_listen.c com_listen.h
	${ECHO} "Building arcRTRListener ..."
	$(CC) -DSERVER_PORT=6800 -o arcRTRListener ${INCLUDES} com_listen.c ${LIBPATHS} ${LIBS}

arcRTRServer: arcRTRServer.c
	${ECHO} "Building arcRTRServer ..."
	$(CC) -o arcRTRServer /home/dan/isp2.2/SIPTelecom3.6/Common/Log/log_routines.o ${INCLUDES} arcRTRServer.c ${LIBPATHS} -lrtrsServer ${LIBS}

clean:
	rm -f *.o $(EXECS) core
