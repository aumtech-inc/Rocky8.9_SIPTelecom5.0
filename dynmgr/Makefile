

CC		= @g++
CC +=  -Wno-write-strings   -Wno-narrowing  -Wno-int-to-pointer-cast

ARCSIP_VERSION    =`cat .version`
ARCSIP_BUILD      =`cat .build`
BUILD_INFO      =-DARCSIP_VERSION=${ARCSIP_VERSION} -DARCSIP_BUILD=${ARCSIP_BUILD}

CFLAGS	= -ggdb -DOS_ROCKY -DSR_MRCP -D__DEBUG__ -DTTS_CHANGES -DENABLE_TRACE -DMAX_PORTS=960 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DOSIP_MT
CFLAGS  += -DEXOSIP_VERSION_401

BASE			= ${ISP_BASE_DIR}/${ISP_VERSION}/SIPTelecom3.6
AVB_BASE        = ${BASE}/arcavb

RM		= @rm
RMFLAGS		= -f

ECHO		= @echo


INCPATHS 	= 								\
		-I ${BASE}/Common/include			\
		-I . 								\
		-I ./spandsp 						\
		-I ./soundtouch 					\
	    -I ${BASE}/thirdParty/libeXosip2-4.1.0/src            \
	    -I ${BASE}/thirdParty/libeXosip2-4.1.0/include        \
	    -I ${BASE}/thirdParty/libosip2-4.1.0/src              \
	    -I ${BASE}/thirdParty/libosip2-4.1.0/include          \
	    -I ${BASE}/thirdParty/ortp-0.6.3.core.ipv6              \
	    -I ${BASE}/thirdParty/ortp-0.6.3.core.ipv6/src			\
	    -I ${BASE}/thirdParty/ortp-0.6.3.core.ipv6/include              \
	    -I ${BASE}/thirdParty/c-ares-1.5.2                      \
	    -I ${BASE}/thirdParty/spandsp-0.0.6/src                 \
	    -I ${BASE}/dynmgr/soundtouch/soundtouch/include		\
	    -I ${BASE}/include                      \
	    -I ./video_stack				



#-I /usr/lib/glib/include \
#-I ${HOME}/include/glib-2.0	\

LIBPATHS	=	\
		-L .	\
		-L ${BASE}/lib	\
		-L ${BASE}/Common/lib 	\
		-L ${BASE}/thirdParty/libeXosip2-4.1.0/src/.libs           \
		-L ${BASE}/thirdParty/libosip2-4.1.0/src/osip2/.libs       \
		-L ${BASE}/thirdParty/libosip2-4.1.0/src/osipparser2/.libs \
		-L ${BASE}/thirdParty/c-ares-1.5.2/.libs         \
		-L ${BASE}/thirdParty/spandsp-0.0.6/src/.libs		\
		-L ${BASE}/thirdParty/tiff-3.9.4/libtiff/.libs			\
		-L ${BASE}/thirdParty/openssl-1.0.2k		\

LIBPATHS_MEDIA	=	\
		-L ${BASE}/thirdParty/ortp-0.6.3.core.ipv6/src/.libs     \
		-L ./spandsp		\
		-L ./soundtouch/soundtouch/lib/ \
		-L ./soundtouch/ 			\

LIBRARIES_MEDIA	=\
	-lISPUtil \
	-lISPCDR \
	-lUTILS	\
	-lISPLog \
	-lcrypto \
	-lortp \
	-lcares \
	-ltiff \
	-lpthread	\
	-losip2 \
	-losipparser2 \
	-lspandsp	\
	-lssl		\
	-lm				\
	-larctones		\
	-larcpromptcontrol		\
	-lSoundTouch			\
	-ldl


LIBRARIES	=\
	-lISPUtil \
	-lISPCDR \
	-lUTILS	\
	-lISPLog \
	-lcrypto \
	-leXosip2 \
	-losip2 \
	-losipparser2 \
	-lssl \
	-lcares \
	-lpthread \
	-ldl \
	-lm

#-lnetsnmp \
# -lefence	\

HEADERS		=

SOURCES		= ArcSipCallMgr.c RegistrationInfo.c  RegistrationHandlers.c  

SOURCES2		= 	\
		ArcSipMediaMgr.c

SOURCES3= ArcSipRedirector.c CallRedirection.c regex_redirection.c


OBJECTS		= ArcSipCallMgr.o \
			RegistrationInfo.o  \
			RegistrationHandlers.o  \
			SubscriptionHandlers.o  \
			SubscriptionInfo.o  \
			IncomingCallHandlers.o  \
			OutboundCallHandlers.o  \
			callTerminateList.o  \
			SipInit.o  \
			Options.o \
			CallOptions.o \
			enum.o  \
			ArcSipCommon.o \
			dl_open.o \
			recycle.o \
			arc_sdp.o \
			osip_utils.o \
			dtmf_utils.o \
			dynVarLog.o
            #CallOutboundHandlers.o

OBJECTS2	= ArcSipMediaMgr.o\
				ArcSipCommon.o\
				dl_open.o \
				dynVarLog.o

OBJECTS3 = ArcSipRedirector.o \
			write_pid_file.o \
			ArcSipCommon.o\
			SipInit.o \
			RegistrationInfo.o \
			SubscriptionInfo.o \
			InboundRegistrationInfo.o \
			InboundRegistrationHandlers.o \
			CallRedirection.o \
			RegistrationHandlers.o \
			SubscriptionHandlers.o \
			dl_open.o \
			regex_redirection.o	\
			dynVarLog.o

#OBJECTS1	= dummy_sr.o

EXECUTABLE	= ArcSipCallMgr
EXECUTABLE2	= ArcSipMediaMgr
EXECUTABLE3 = ArcSipRedirector
EXECUTABLE4 = ArcSipRedirectorStandAlone

#-------------------------------------------------------------------------------
# DO NOT CHANGE ANYTHING BELOW THIS LINE
#
#
# Suffix Rules
#
.c.o:
	${ECHO} "Compiling $<"
	${CC} -DSR_MRCP ${CFLAGS} -c $< ${INCPATHS} ${BUILD_INFO}

.cpp.o:
	${ECHO} "Compiling $<"
	${CC} -DSR_MRCP ${CFLAGS} -c $< ${INCPATHS} ${BUILD_INFO}

#${CC} -DDUMMY ${CFLAGS} -c $< ${INCPATHS}
#
# Targets
#
all: ${EXECUTABLE} ${EXECUTABLE2} ${EXECUTABLE3}

${EXECUTABLE}: ${OBJECTS}
	${ECHO}
	${ECHO} "Linking ${EXECUTABLE}"
	${ECHO}
	${CC} -o ${EXECUTABLE} ${LIBPATHS} ${OBJECTS} 			\
			${LIBRARIES} ${BUILD_INFO}    

${EXECUTABLE2}: ${OBJECTS2}
	${ECHO}
	${ECHO} "Linking ${EXECUTABLE2}"
	${ECHO}
	${CC} -o ${EXECUTABLE2} ${LIBPATHS} ${LIBPATHS_MEDIA} ${OBJECTS2} \
			${LIBRARIES_MEDIA} ${BUILD_INFO}

${EXECUTABLE3}: ${OBJECTS3}
	${ECHO}
	${ECHO} "Linking ${EXECUTABLE3}"
	${ECHO}
	${CC} -o ${EXECUTABLE3} ${LIBPATHS} ${OBJECTS3} ${LIBRARIES} ${BUILD_INFO}   

clean: Always
	rm ${RMFLAGS} ${OBJECTS} ${EXECUTABLE} ${OBJECTS2} ${EXECUTABLE2} ${OBJECTS3} ${EXECUTABLE3} *.o

install: all

Always:
