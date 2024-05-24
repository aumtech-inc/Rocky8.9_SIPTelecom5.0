#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:
#----------------------------------------------------------------------------

ISP_BASE_DIR=${HOME}
ISP_VERSION=isp2.2

BASE        = ${ISP_BASE_DIR}/${ISP_VERSION}/SIPTelecom3.6

CHATGPT_HOME       = ${BASE}/chatgpt
CHATGPT_LIB        = $(CHATGPT_HOME)/lib

ECHO		= 	@echo
CC		= 	gcc  	
CFLAGS		= -ggdb -fPIC 

SRCS		=	arcAICommon.c

OBJS	= 	${SRCS:.c=.o}

LIB		= ${CHATGPT_LIB}/libarcAICommon.so

BIN		= 

all:  ${LIB}

arcAICommon.o:	arcAICommon.c
	${ECHO} "Compiling $<"
	${CC} $(CFLAGS)  -c  $<

${LIB}:	arcAICommon.o
	gcc -shared -o $@  ${OBJS} -L../lib -laiISPLog -lailoglite

clean:
	rm -f ${LIB} ${OBJS} 
