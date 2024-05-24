#------------------------------------------------------------------------------
# File:		Makefile
#
# Purpose:	This file compiles all the source code containing
#		global accessory routines.
#		These routines are archived into libgaUtils.a.
#
# Usage:	make clean all
# 
# Author:	Sandeep Agate		
#
# Update:	10/01/96 sja	Created the file
# Update:	11/06/97 djb	changed gaGetField to gaStringRoutines
# Update:	09/25/98 sja	Added gaNT.c to SRCS
#------------------------------------------------------------------------------
CC = 		@gcc -ggdb	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
EC = 		@/bin/echo
CFLAGS = 	-O -s
AR = 		@ar
ARFLAGS = 	ru
CP = 		cp

ISP_BASE_DIR=/home/dan
ISP_VERSION=isp2.2

BASE    =   $(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
COMMON	=	${BASE}/Common
COMINC	=	${COMMON}/include
COMLIB	=	${COMMON}/lib

# LIBPATHS	= -L . \

INCPATHS	= -I ${COMINC}

SYSLIBS	= 	-lmalloc

HEADERS = 	${COMINC}/gaUtils.h		\
		${COMINC}/gaIncludes.h

SRCS	=	\
		gaOther.c		\
		gaNT.c			\
		gaSCUtil.c		\
		gaLog.c			

OBJS = 		${SRCS:.c=.o}

GA_LIB = 	${COMLIB}/libgaUtils.a

RMFILES = 	${GA_LIB} ${OBJS} core *.o

.c.o: ${HEADERS}
	${EC} "Building ${<F:.c=.o}"
	${CC} ${CFLAGS} -c $< ${INCPATHS}

all: ${GA_LIB} ${EXECS}

${GA_LIB}: ${OBJS}
	${EC} "Building $@"
	${AR} ${ARFLAGS} $@ ${OBJS}

#install: all
#	@for f in ${INSTALLFILES} ; do \
#		echo "Copying $$f to ${INSTALLPATH}"; \
#		${CP} $$f ${INSTALLPATH}; \
#	done
#
clean:
	@for f in ${RMFILES} ; do if test -f $$f; \
		then \
			echo "Removing $$f" ; \
			/bin/rm -f $$f ; \
		fi ; \
	done

clena: clean
