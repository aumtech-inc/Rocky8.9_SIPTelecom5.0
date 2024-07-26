#----------------------------------------------------------------------------
#----------------------------------------------------------------------------

ECHO		= 	@echo
# CC		= 	@gcc	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CC		= 	gcc	
CFLAGS		= -ggdb -fPIC -D_LARGEFILE64_SOURCE

ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2

BASE		=	$(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
LSBASE  = $(BASE)/loggingServices/realTimeReports
COMINC  = ${BASE}/Common/include
WSCINC  = ${LSBASE}/client/include
LIBDIR  = ${LSBASE}/lib

LIBPATH		= 	-L ${LIBDIR} -L ${BASE}/Common/lib
INCPATH		=	-I ${WSCINC}  -I ${COMINC}

INCLUDES	=	$(COMINC)/loginc.h $(COMINC)/COMmsg.h		\
			$(COMINC)/LOGmsg.h $(COMINC)/CDRmsg.h		\
			$(COMINC)/CEVmsg.h $(COMINC)/NETmsg.h		\
			${COMINC}/arc_snmpdefs.h

EXECS		=	arcRTRClient

# CFLAGS		=	-s 
# CFLAGS		=	-g

.c.o: ${INCLUDES} $<
	${ECHO} "Compiling $<"
	${CC} $(CFLAGS) -O0 -c ${INCPATH} $<

#	${CC} -O2 -mcpu=pentium4 -march=pentium4 -c ${INCPATH} $<

all:		$(LIBS) $(EXECS)

install:
	scp arcRTRClient arc@10.0.10.236:/home/arc/.ISP/Global/Exec
	scp arcRTRClient arc@10.0.10.24:/home/arc/.ISP/Global/Exec

clean:
	rm -f *.o $(EXECS)


arcRTRClient: arcRTRClient.o 
	${CC} $(CFLAGS) -O0 -o arcRTRClient arcRTRClient.o  ${BASE}/Common/Log/log_routines.o $(LIBPATH) -lrtrcComm -lgaUtils -lpthread
