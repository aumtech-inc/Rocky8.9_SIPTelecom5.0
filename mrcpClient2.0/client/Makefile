#-------------------------------------------------------------------------------
# File:     Makefile
# Author:   Aumtech, Inc.
# Update: rg 01/05/2008	Created the file
#-------------------------------------------------------------------------------
include ../Make.def.eXosip4.1

SRCS = mrcpClient2.cpp mrcpCommon2.cpp SRPort.cpp GrammarList.cpp
SRCS += sipThread.cpp readerThread.cpp mrcpThread.cpp
SRCS +=  mrcp2_HeaderList.cpp  mrcp2_Request.cpp mrcp2_Event.cpp \
         mrcp2_Response.cpp mrcp2_parser_utils.cpp MrcpInitialize.cpp	\

MGR_SRC = mrcpClient2Mg.c

INCPATHS = -I .  		\
	-I ${MRCP_HOME}/include \
	-I ${MRCP_HOME}/include/parser \
	-I ${SVID_HOME}/include		\
	-I ${SVID_HOME}/Common/include		\
    -I ${SVID_HOME}/thirdParty/libosip2-4.1.0/include		\
    -I ${SVID_HOME}/thirdParty/libeXosip2-4.1.0/include

LIBPATHS =   \
	-L ${MRCP_LIB}	\
	-L ${SVID_HOME}/lib  \
	-L ${SVID_HOME}/Common/lib  \
    -L ${SVID_HOME}/thirdParty/libosip2-4.1.0/src/osipparser2/.libs     \
    -L ${SVID_HOME}/thirdParty/libosip2-4.1.0/src/osip2/.libs           \
    -L ${SVID_HOME}/thirdParty/libeXosip2-4.1.0/src/.libs  				\
    -L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/openssl-1.0.2k     \
    -L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/c-ares-1.9.1/.libs         \
	-L ${SVID_HOME}/thirdParty/xerces-c-src_2_8_0/lib 


LIBS =    \
	-lSR_mrcpV2.eXosip4.1	\
	-lISPUtil	\
	-lISPLog 	\
	-lcrypto	\
	-leXosip2	\
	-losipparser2	\
	-losip2		\
	-lssl			\
	-lcares			\
	-lxerces-c	\
	-lpthread


MRCPCLIENT2	= ${MRCP_HOME}/bin/mrcpClient2.sip.eXosip4.1

MRCPCLIENT2MGR	= ${MRCP_HOME}/bin/mrcpClient2Mgr

CPPFLAGS += ${INCPATHS} -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DOSIP_MT

OBJ = $(SRCS:.cpp=.o)

all: ${MRCPCLIENT2} ${MRCPCLIENT2MGR}

.cpp.o: ${SRCS}
	g++ -ggdb -Wno-write-strings -fpermissive -g3 -DEXOSIP_4 -DOSIP_MT -DENABLE_TRACE -c $< ${INCPATHS}

.c.o: ${MGR_SRC}
	gcc -Wno-implicit-function-declaration -c $< -I ${SVID_HOME}/include

${MRCPCLIENT2}: ${OBJ} ${SR_LIB}
	g++ -ggdb -g3 -DEXOSIP_4 -DOSIP_MT -o $@ ${OBJ} ${LIBPATHS} ${LIBS} -Wall

${MRCPCLIENT2MGR}: mrcpClient2Mgr.o
	gcc -o $@ mrcpClient2Mgr.o -L ${SVID_HOME}/Common/lib -lISPUtil -lISPLog

MGR_SRC = mrcpClient2Mg.c
${SR_LIB}:
	( cd ${MRCP_HOME}/apisrc; make )

clean:
	rm -f *.o ${MRCPCLIENT2} ${MRCPCLIENT2MGR}

