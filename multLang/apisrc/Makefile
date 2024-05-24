#------------------------------------------------------------------------------
# File:		Makefile
# Purpose:	This file compiles all the code for multiple languages. 
#
# Update: 2002/01/14 djb	Created the file.
#------------------------------------------------------------------------------

include ${ISP_BASE_DIR}/${ISP_VERSION}/SIPTelecom3.6/multLang/Make.def

LIB	= ${ML_HOME}/lib/libarcML.a

APISRC =	\
 			TEL_MLSpeakNumber.c		\
 			TEL_MLSpeakCurrency.c	\
 			TEL_MLSpeakTime.c	\
 			TEL_MLSpeakDate.c	\
			ML_Common.c	

OBJECTS = ${APISRC:.c=.o}

RMFILES	=	${LIB} ${OBJECTS}

all:	${LIB}

${LIB} : ${OBJECTS}
	${ECHO} "Building $@"
	${ECHO} "${AR} ${ARFLAGS} $@ ${OBJECTS}"
	${AR} ${ARFLAGS} $@ ${OBJECTS}

ML_Common.o: ML_Common.c
	@${ECHO} "Building ML_Common.o"
	@${CC} -c ${INCPATHS} ML_Common.c

TEL_MLSpeakCurrency.o: TEL_MLSpeakCurrency.c
	@${ECHO} "Building TEL_MLSpeakCurrency.o"
	@${CC} -c ${INCPATHS} TEL_MLSpeakCurrency.c
 
TEL_MLSpeakNumber.o: TEL_MLSpeakNumber.c
	@${ECHO} "Building TEL_MLSpeakNumber.o"
	@${CC} -c ${INCPATHS} TEL_MLSpeakNumber.c

TEL_MLSpeakTime.o: TEL_MLSpeakTime.c
	@${ECHO} "Building TEL_MLSpeakTime.o"
	@${CC} -c ${INCPATHS} TEL_MLSpeakTime.c

TEL_MLSpeakDate.o: TEL_MLSpeakDate.c
	@${ECHO} "Building TEL_MLSpeakDate.o"
	@${CC} -c ${INCPATHS} TEL_MLSpeakDate.c

tags: Always
	${CTAGS} ${SRCS}

Always:

clean:
	@for f in ${RMFILES} ; do if test -f $$f; \
		then \
			echo "Removing $$f" ; \
			/bin/rm -f $$f ; \
		fi ; \
	done

install:
	scp ${ML_HOME}/lib/libarcML.a arc@10.0.10.83:/home/arc/.ISP/Telecom/Applications/lib
