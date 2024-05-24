#----------------------------------------------------------------------------
# Program:      Makefile
# Purpose:      Make file for building the VFX Fax based FaxServer.
# Author:	Dan Barto
#----------------------------------------------------------------------------
CC = gcc  -ggdb 
ECHO = echo
AR = ar
RM = rm -f
ARFLAGS = r
CFLAGS = -c
CFLAGS_E = -DT1C -o

-include ../../Make.defs



ISP_BASE_DIR=/home/dev
ISP_VERSION=isp2.2

BASE = ${ISP_BASE_DIR}/${ISP_VERSION}
TEL_BASE= ${BASE}/SIPTelecom3.3
COM_BASE= ${BASE}/Common
COMINC = ${COM_BASE}/include
TELINC = ${TEL_BASE}/include
LOGINC = ${TEL_BASE}/Log

LIBLIST= -lISPTel -lISPCDR -lrespcom -larcSNMP  -lUTILS -lISPUtil -lISPLog -lTELLog -lloglite -lgaUtils -larcSNMP -lsnmp -lcrypto


# old hopefully won't have to rebuild 
#SNMP_LIBS = -larcSNMP /usr/lib/libsnmp.a /usr/lib/libcrypto.a
SNMP_LIBS =

LIBPATHS= -L ${HOME}/lib \
		  -L /home/dev/isp2.2/SNMP/lib 

# INCLUDES = -I ${HOME}/include -I ${TELINC} -I ${LOGINC} -I ${COMINC}  \
# 	   -I ${COM_BASE}/ResourceMgr -I ${TEL_BASE}/Fax/include
INCLUDES = -I ${HOME}/include -I ${COMINC}  \
	   -I ${COM_BASE}/ResourceMgr 

FAX_SERVER = ${TEL_BASE}/Fax/bin/FaxServer

EXECUTABLES = ${FAX_SERVER}

all: executable

executable: ${EXECUTABLES}

clean : 
	rm -f ${EXECUTABLES}

${FAX_SERVER} : FaxServer.c 
	@${ECHO} "Building FaxServer executable"
	@${CC} ${CFLAGS_E} ${FAX_SERVER} FaxServer.c  ${HOME}/isp2.2/SIPTelecom3.3/apisrc/FAX_Common.c	\
		${INCLUDES} ${LIBPATHS}						\
		${LIBLIST} ${SNMP_LIBS}
