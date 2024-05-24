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
BASE 	= ${ISP_BASE_DIR}/${ISP_VERSION}
LSBASE 	= ${BASE}/loggingServices
COMINC 	= ${BASE}/Common/include
WSSINC 	= .
LIBDIR  = .

# CFLAGS	= -s -O
CFLAGS	= -ggdb3

CC 	= @gcc ${CFLAGS}
ECHO 	= @echo
AR 	= ar
ARFLAGS = r

INCLUDES = -I $(WSSINC) -I $(COMINC)

# LIBPATHS = 	-L ${LIBDIR} -L ${BASE}/Common/lib
LIBPATHS = 	-L ${LIBDIR} 

LIBS	= 	$(LICLIBS) -lgaUtils -lISPLog -lloglite

EXECS 	= 	testC testS

SRCS	= com.c testS.c testC.c

OBJS	= ${SRCS:.c=.o}

all: $(EXECS)

install:
	scp log_listen arc@10.0.0.59:/home/arc/.ISP/Global/Exec/log_listen
	scp log_svc    arc@10.0.0.59:/home/arc/.ISP/Global/Exec/log_svc

com.o:	com.c
	  @$(CC) -c $(INCLUDES) com.c

testS: testS.c
	${ECHO} "Building testS ..."
	$(CC) -o testS ${INCLUDES} testS.c ${LIBPATHS} ${LIBS}

log_svc: log_svc.c
	${ECHO} "Building testC ..."
	$(CC) -o testC ${INCLUDES} testC.c ${LIBPATHS} -llsServer ${LIBS}

clean:
	rm -f *.o $(EXECS) core
