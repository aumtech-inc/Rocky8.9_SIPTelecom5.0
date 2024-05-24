# Program:      Makefile
# Purpose:      Make file for building the MRCP Speech Rec API libraries
# Author:       Rahul Gupta
# Update:       01/05/08 rg Created the file 
#----------------------------------------------------------------------------

include			../Make.def.eXosip4.1

INCLUDE_DEPS	= ${MRCP_INC}/arcSR.hpp ${MRCP_INC}/mrcpCommon2.hpp

INC_PATHS	=	\
		-I ${MRCP_INC}		\
		-I ${SVID_INC}		\
		-I ${SVID_HOME}/Common/include  \
		-I ${SVID_HOME}/thirdParty/xerces-c-src_2_8_0/include/xercesc      \
		-I ${SVID_HOME}/thirdParty/xerces-c-src_2_8_0/include/xercesc/framework        \
		-I ${SVID_HOME}/thirdParty/xerces-c-src_2_8_0/include      \
		-I ${SVID_HOME}/thirdParty/xerces-c-src_2_8_0/include/xercesc/sax      \
		-I ${SVID_HOME}/thirdParty/xerces-c-src_2_8_0/include/xercesc/util         \
		-I ${SVID_HOME}/thirdParty/xerces-c-src_2_8_0/include/xercesc/dom/deprecated       \
        -I ${SVID_HOME}/thirdParty/libosip2-4.1.0/include \
        -I ${SVID_HOME}/thirdParty/libeXosip2-4.1.0/include

CPP_SRCS	=			\
	SRInit.cpp			\
	SRCommon.cpp			\
	SRReserveResource.cpp		\
	SRSetParameter.cpp		\
	SRGetParameter.cpp		\
	SRLoadGrammar.cpp		\
	SRReleaseResource.cpp		\
	SRRecognizeV2.cpp		\
	SRGetResult.cpp			\
	SRExit.cpp			\
	SRGetXMLResult.cpp		\
	SRUnloadGrammars.cpp		\
	SRUnloadGrammar.cpp		\
	DOMTreeErrorReporter.cpp	\
	parseResult.cpp

#	SRUnloadGrammar.cpp	 - VXI only


# SR_SRCS	=							\
#			SRTerminate.c			\
#			SRAddWords.c				\
#			SRDeleteWords.c			\
#			SRGetPhoneticSpelling.c	\
#			SRLearnWord.c
#										
#

# SR_OBJECTS	=	${SR_SRCS:.c=.o}

CPP_OBJECTS	=	${CPP_SRCS:.cpp=.o}

.cpp.o: ${INCLUDE_DEPS}
	@echo "Compiling $<"
	@g++  -Wno-write-strings -ggdb -g3 -DEXOSIP_4 -DOSIP_MT -c $< ${INC_PATHS}

all: ${SR_LIB} 

${SR_LIB} : ${SR_OBJECTS} ${CPP_OBJECTS}
	${AR} ${ARFLAGS} ${SR_LIB} ${SR_OBJECTS} ${CPP_OBJECTS}

clean : 
	rm -f ${SR_OBJECTS} ${CPP_OBJECTS} ${SR_LIB} 

tags: Always
	

Always:
