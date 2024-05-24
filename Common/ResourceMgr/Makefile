#------------------------------------------------------------------------------
# File:		Makefile
# Purpose:	This file compiles all the code for the Resource Manager.
# Author:	Dan Barto
# Date:		04/29/98
# Update:	06/17/99 gpw corrected all the silly inaccuracies in this header
# Update:	06/17/99 gpw added libUTILS
# Update:	06/24/99 gpw added licensing libraries, etc.
# Update:	08/24/99 gpw removed ResourceMgrAPIs; now in libUTILS.a 
# Update:	03/14/01 mpb added changes for FLEXLM. 
#------------------------------------------------------------------------------



ISP_BASE_DIR=/home/dev
ISP_VERSION=isp2.2 

BASE=$(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
FLEXLM_INC=$(BASE)/Common/flexNET/v11.5/machind
FLEXLM_OBJ=$(BASE)/Common/flexNET/v11.5/i86_re3
FLEXLM_LIB=$(BASE)/Common/flexNET/v11.5/i86_re3
FLEXLM_LIB2=${BASE}/Common/flexNET/v11.5/i86_re3/activation/lib

EC =            @echo
CC =            gcc	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS =        -ggdb3
AR =            ar
ARFLAGS =       ru
CP =            cp

#
# Libraries
#
LIBPATHS  =  -L ${BASE}/Common/lib -L ${FLEXLM_LIB} -L ${FLEXLM_LIB2} -L ${BASE}/SNMP/lib
INCPATHS  = -I ${BASE}/Common/include -I ${FLEXLM_INC}
ISPLIBS  =	-lrespcom -lISPUtil -lUTILS -lRULES -lgaUtils  -lISPLog -lloglite
LICLIBS	= -larcLicenseV11.5_with_handleLic -llmgr -lnoact -lcrvs -lsb -lpthread
SNMPLIBS = -larcSNMP -lsnmp  -lcrypto

#
# System Libraries
#
#SYSLIBS  =	-lnsl -lsocket -lgen -lelf 

ALLLIBS  = ${LICLIBS} ${ISPLIBS} ${SNMPLIBS} 

SRCS = ResourceMgr.c  
OBJS =	${SRCS:.c=.o}

INCLUDE = ResourceMgr.h

EXECS = ResourceMgr

RMFILES = 	${OBJS} ${EXECS} core

.c.o: ${INCLUDE} $<
	${EC} "Building ${<F:.c=.o}"
	${CC} ${CFLAGS} -c $< ${INCPATHS}

all: ${EXECS} 

#
ResourceMgr: ResourceMgr.o
	${EC} "\nLinking $@\n"
	#${CC} ${CFLAGS} ${LIBPATHS} -o $@ ResourceMgr.o ${ALLLIBS}
	${CC} ${CFLAGS} ${LIBPATHS} -o $@ ResourceMgr.o ${FLEXLM_OBJ}/lm_new.o ${ALLLIBS}

Always:

install:
	cp ResourceMgr ${BASE}/bin
clean:
	@for f in ${RMFILES} ; do if test -f $$f; \
		then \
			echo "Removing $$f" ; \
			/bin/rm -f $$f ; \
		fi ; \
	done
