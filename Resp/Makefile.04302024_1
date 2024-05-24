#----------------------------------------------------------------------------
# Program:      Makefile
# Purpose:      Make file for building IP Responsibility.
# Author:   	George Wenzel
# Date:		12/04/00
#----------------------------------------------------------------------------
CC = gcc 
COPTS= -Wall -ggdb -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-variable
ECHO = echo
CFLAGS =  -c -ggdb -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS_E = -ggdb -o
AR = ar
ARFLAGS = -r

BASE = ${HOME}/isp2.2/SIPTelecom3.6
COMMON=${BASE}/Common
FLEXLM_BASE=${COMMON}/flexNET/v11.5/amd64_re3
FLEXLM_INC=${FLEXLM_BASE}/activation -I ${COMMON}/flexNET/v11.5/machind 
FLEXLM_INC_2=${BASE}/Common/flexNET/v11.5/src


#/home/dev/isp2.2/SIPTelecom3.4/Common/flexNET/v11.5/amd64_re3
#/home/dev/isp2.2/SIPTelecom3.4/Common/flexNET/v11.5/amd64_re3/activation/lib/libnoact_pic.a
#/home/dev/isp2.2/SIPTelecom3.4/Common/flexNET/v11.5/lib

COMINC = ${COMMON}/include
RESINC = ${BASE}/Resp
COMLIB = ${BASE}/Common/lib
TELINC = ${BASE}/include
TELAPI = ${BASE}/apisrc

LIBPATHS  = -L ${COMMON}/lib -L ${FLEXLM_BASE}/lib -L ${FLEXLM_BASE} -L ${FLEXLM_BASE}/activation/lib -L ${COMMON}/flexNET/v11.5/lib -L ${COMMON}/lib 

UTLLIBS  = -lUTILS -lRULES -lgaUtils  -lISPLog -lISPUtil
LICLIBS  = -larcLicenseV11.5 -llmgr_pic -lcrvs_pic -lsb_pic -lnoact_pic -lpthread
#SNMP_LIBS = -larcSNMP -lISPUtil


#SYSLIBS		= -lposix

LOGLIBS  = -lISPLog 
LOGLIB	= ${BASE}/Common/lib/libISPLog.a

INCLUDES = -I . -I ${TELINC} -I ${COMINC} -I ${FLEXLM_INC} -I ${FLEXLM_INC_2}
EXECUTABLES = ArcIPResp ArcIPStopResp

all : ${EXECUTABLES}

clean : 
	rm -f  ${EXECUTABLES} ArcIPResp.o ArcIPStopResp.o

ArcIPResp : ArcIPResp.c
	@${ECHO} "Building ArcIPResp"
	@${CC} ${COPTS} ${CFLAGS_E} ArcIPResp ArcIPResp.c  ${TELAPI}/ocmUtil.o 	\
		${FLEXLM_BASE}/lm_new.o							\
		${INCLUDES} ${LIBPATHS} ${LICLIBS} ${UTLLIBS} 
	cp ArcIPResp ../bin

ArcIPStopResp : ArcIPStopResp.c
	@${ECHO} "Building ArcIPStopResp"
	@${CC} ${COPTS} ${CFLAGS_E} ArcIPStopResp ArcIPStopResp.c 		\
		${INCLUDES} ${LIBPATHS} ${UTLLIBS}  
	cp ArcIPStopResp ../bin

${EXECUTABLES}: ${COMINC}/BNS.h

${EXECUTABLES}: ${COMINC}/ISPSYS.h

${EXECUTABLES}: ${RESINC}/ArcIPResp.h

${EXECUTABLES}: ${RESINC}/ArcIPRespMsg.h

${EXECUTABLES}: ${COMINC}/shm_def.h

${EXECUTABLES}: ${COMINC}/err_msg.h

${EXECUTABLES}: ${LOGLIB} 

${LOGLIB}:	
${EXECUTABLES}: ${COMINC}/BNS.h

${EXECUTABLES}: ${COMINC}/ISPSYS.h

${EXECUTABLES}: ${RESINC}/ArcIPResp.h

${EXECUTABLES}: ${RESINC}/ArcIPRespMsg.h

${EXECUTABLES}: ${COMINC}/shm_def.h

${EXECUTABLES}: ${COMINC}/err_msg.h

${EXECUTABLES}: ${LOGLIB} 

${LOGLIB}:	
