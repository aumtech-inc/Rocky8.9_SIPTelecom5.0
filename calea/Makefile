CC		= @g++ 
#CFLAGS	= -g -DSR_MRCP

CFLAGS	= -ggdb -w -O2 -D__DEBUG__  -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS  += -DSTANDALONE  -Wno-write-strings


RM		= @rm
RMFLAGS		= -f

ECHO		= @echo

INCPATHS 	= 	\
		-I /home/dev/isp2.2/SIPTelecom3.2/Common/include  \
		-I . 	\
		-I../include	\
		-I /home/dev/isp2.2/SIPTelecom3.2/include 	\
		-I /home/dev/isp2.2/SIPTelecom3.2/dynmgr 	\
		-I /usr/lib/glib/include \
		-I /home/dev/isp2.2/SIPTelecom3.2/Common/flexNET/v11.5/include

#		-I /home/devmaul/isp2.2/SIPTelecom3.2/glib-2.14.4 \

LIBPATHS	=	\
		-L .	\
		-L /home/dev/isp2.2/SIPTelecom3.2/lib 	\
		-L /home/dev/isp2.2/SIPTelecom3.3/Common/lib \
		-L /home/dev/isp2.2/SNMP/lib    \
		-L /home/dev/isp2.2/SIPTelecom3.2/Common/flexNET/v11.5/lib	\



LIBRARIES	=\
	-lISPUtil \
	-lUTILS	\
	-lISPLog \
 	-lloglite \
	-lpthread


HEADERS		=

SOURCES		= ArcCalea.c 

OBJECTS		= ArcCalea.o \

EXECUTABLE	= ArcCalea

#-------------------------------------------------------------------------------
# DO NOT CHANGE ANYTHING BELOW THIS LINE
#
#
# Suffix Rules
#
.c.o:
	${ECHO} "Compiling $<"
	${CC} ${CFLAGS} -c $< ${INCPATHS} ${BUILD_INFO}

#${CC} -DDUMMY ${CFLAGS} -c $< ${INCPATHS}
#
# Targets
#
all: ${EXECUTABLE}

${EXECUTABLE}: ${OBJECTS}
	${ECHO}
	${ECHO} "Linking ${EXECUTABLE}"
	${ECHO}
	${CC} -o ${EXECUTABLE} ${LIBPATHS} ${OBJECTS} 			\
			${LIBRARIES} ${BUILD_INFO}

clean: Always
	${RM} ${RMFLAGS} ${EXECUTABLE} ${OBJECTS}

install: all

Always:
