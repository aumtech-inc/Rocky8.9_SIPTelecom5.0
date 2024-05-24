#------------------------------------------------------------------------------
# File:		Makefile
#
# Purpose:	Compile and build the multLang test applications
#
# Update: 	2002/01/16 djb	Created the file.
#------------------------------------------------------------------------------

include		../Make.def

HEADERS 	=

SRCS 		= french.c		\
				mTest.c

LIB			= ../lib/libarcMultLang.a

OBJS 		= ${SRCS:.c=.o}

EXEC 		= ${TELECOM}/Exec/french

TEST_EXEC	= ${TELECOM}/Exec/mTest

RMFILES 	= ${OBJS} ${EXEC} core

.c.o: ${HEADERS} $<
	${ECHO} "Building ${<F:.c=.o}"
	${CC} ${CFLAGS} -c $< ${INCPATHS}

all:	${EXEC}  ${TEST_EXEC}

${EXEC}: french.o ${LIB}
	${ECHO}
	${ECHO} "Linking $@"
	${ECHO}
	@${CC} ${CFLAGS} ${LIBPATHS} -o $@ french.o ${LIB} ${LIBS_ARC_TEL} ${LIBS_ARC_COM} ${LIBS_SYSTEM}

${TEST_EXEC}: mTest.o ${LIB}
	${ECHO}
	${ECHO} "Linking $@"
	${ECHO}
	@${CC} ${CFLAGS} ${LIBPATHS} -o $@ mTest.o ${LIB} ${LIBS_ARC_TEL} ${LIBS_ARC_COM} ${LIBS_SYSTEM}

${LIB_TELIM_COM}:

Always:

clean:
	@for f in ${RMFILES} ; do if test -f $$f; \
		then \
			echo "Removing $$f" ; \
			/bin/rm -f $$f ; \
		fi ; \
	done
