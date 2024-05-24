#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:	Make file for creating library libTELLog.a
#
# Written by:	ISP Developement staff
# Orig date:	Unknown
#
# Last Chg by:	George Wenzel
# Last Update:	Feb 2, 1996
#----------------------------------------------------------------------------


ISP_BASE_DIR=/home/dan
ISP_VERSION=/isp2.2
BASE=${ISP_BASE_DIR}/${ISP_VERSION}

ISPINC=${BASE}/SIPTelecom3.6/Log
ISPLIB=${BASE}/SIPTelecom3.6/lib

TELSRC= TELmsg.c
TELOBJ= TELmsg.o

INCLUDES=${ISPINC}/TELmsg.h

LIBS=${ISPLIB}/libTELLog.a

CFLAGS=-O -s -I${ISPINC}

all:	${LIBS} 

lint:
	lint ${CFLAGS} ${TELSRC}

clean:
	rm -f ${LIBS} ${TELOBJ}


${LIBS}: ${LIBS}{TELmsg.o} 
	ar ruv ${LIBS} TELmsg.o


${LIBS}{TELmsg.o}: ${ISPINC}/TELmsg.h
	gcc -c TELmsg.c
