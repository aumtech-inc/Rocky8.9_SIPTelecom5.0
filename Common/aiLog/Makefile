#!/usr/bin/make -e 
#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:
#
# Author:	ISP Development staff
# Date:		Unknown
# Update:	02/02/96 G. Wenzel added header
# Update:	03/20/96 G. Wenzel removed LOGSHUT - not used
# Update:	04/12/96 G. Wenzel removed ITPINC - not used
# Update:	04/12/96 G. Wenzel replaced /usr/include/ism with $IVIEW_INC_DIR
# Update:	04/12/96 NOTE: Several .c files were changed ism/lm.h to lm.h
# Update:	04/16/96 G. Wenzel renamed log_mgr.c resp_log.c
# Update:	02/26/97 G. Wenzel replaced loglite.mk with logremote.mk
# Update:	04/21/97 D. Barto removed logremote.mk,reference to shell script
# Update:	10/30/97 D. Barto  added linking to libgaUtils.a for logging
# Update:	11/04/97 D. Barto  removed log_routines.o from libloglite.a
#----------------------------------------------------------------------------

ECHO		= 	@echo
# CC		= 	@gcc	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CC		= 	gcc  	
#CFLAGS		= 	-O2 -mcpu=pentium4 -march=pentium4 -s -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS		= -ggdb -fPIC -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
AR		= 	ar
ARFLAGS		= 	ru

ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2

B			=	$(ISP_BASE_DIR)/$(ISP_VERSION)
BASE		=	$(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
COMMON		=	$(BASE)/Common
COMINC		=	$(COMMON)/include
COMLIB		=	${COMMON}/lib
COMBIN		=	$(COMMON)/bin
WSCSRC		=	$(BASE)/loggingServices/logForwarding/client/apisrc
WSCLIB		=	$(BASE)/loggingServices/logForwarding/lib
#WSCINC		=	$(B)/WSC/include
WSCINC		=	$(BASE)/loggingServices/logForwarding/client/include
GASRC		=	$(COMMON)/accessories
CHATGPT		=	${BASE}/chatgpt/lib
#SYSLIB		=	-lsocket -lnsl
#SYSLIB		=	-lbsd

LOGSRC		=	COMmsg.c LOGmsg.c CDRmsg.c CEVmsg.c NETmsg.c \
			LOGXXCTL.c LOGXXPRT.c LOGglb.c LOGINIT.c LogARCMsg.c \
			resp_log.c

LOGOBJ 		= 	${LOGSRC:.c=.o}

LIBPATH		= 	-L ${COMLIB} -L ${WSCLIB}
INCPATH		=	-I ${COMINC} -I ${WSCINC}

INCLUDES	=	$(COMINC)/loginc.h $(COMINC)/COMmsg.h		\
			$(COMINC)/LOGmsg.h $(COMINC)/CDRmsg.h		\
			$(COMINC)/CEVmsg.h $(COMINC)/NETmsg.h		\
			${COMINC}/arc_snmpdefs.h

LIBS		=$(CHATGPT)/libaiISPLog.so	\
			$(CHATGPT)/libailoglite.so

#LIBS		=$(COMLIB)/libISPLog.a	\
#			$(WSCLIB)/liblogComm.a	\
#            $(COMLIB)/libgaUtils.a  \
#			$(COMLIB)/libloglite.a


EXECS		=	$(COMBIN)/log_cli  

# CFLAGS		=	-s 
# CFLAGS		=	-g

.c.o: ${INCLUDES} $<
	${ECHO} "Compiling $<"
	${CC} $(CFLAGS) -O2 -c ${INCPATH} $<

#	${CC} -O2 -mcpu=pentium4 -march=pentium4 -c ${INCPATH} $<

all:		$(LIBS) 

clean:
	rm -f $(LIBS) *.o $(EXECS)

${CHATGPT}/libailoglite.so: log_remote.o
#	${AR} ${ARFLAGS} $@ log_remote.o
	gcc -shared -o ${CHATGPT}/libailoglite.so log_remote.o

$(COMLIB)/libgaUtils.a:
	( cd $(GASRC); make all -f Makefile)

$(WSCLIB)/liblogComm.a:
	( cd $(WSCSRC); make all )

log_remote.o: log_remote.c
	${CC} $(CFLAGS) -c ${INCPATH} log_remote.c

log_routines.o: log_routines.c $(COMINC)/log_defs.h
	${CC} $(CFLAGS) -O2 -c ${INCPATH} log_routines.c
		
$(COMBIN)/log_cli: log_cli.o log_routines.o $(WSCLIB)/liblogComm.a	\
						$(COMLIB)/libgaUtils.a
	${CC} $(CFLAGS) -O2  -o $(COMBIN)/log_cli log_cli.o log_routines.o	\
				$(INCPATH) $(LIBPATH) ${LIBS} ${SYSLIB}
	
log_cli.o: log_cli.c $(COMINC)/log_defs.h
	${CC} $(CFLAGS) -O2  -c $(INCPATH) log_cli.c

${CHATGPT}/libaiISPLog.so:	${LOGOBJ} 
#	${AR} ${ARFLAGS} $@ ${LOGOBJ}
	echo "gcc -shared -o ${CHATGPT}/libaiISPLog.so ${LOGOBJ}"
	gcc -shared -o ${CHATGPT}/libaiISPLog.so ${LOGOBJ}

