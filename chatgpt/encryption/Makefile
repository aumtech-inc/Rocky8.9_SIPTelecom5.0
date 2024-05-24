#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:
#----------------------------------------------------------------------------

ECHO		= 	@echo
CC		= 	gcc  	 
CFLAGS		= -ggdb -fPIC 

SRCS		=	arcChatGPTEncrypt.c		\
				ArcDecrypt.c			\
				testDecrypt.c

OBJS	= 	${SRCS:.c=.o}

LIB		= libArcDecrypt.so

BINS	= ../bin/arcChatGPTEncrypt  testDecrypt

.c.o: ${INCLUDES} $< 
	${ECHO} "Compiling $<"
	${CC} $(CFLAGS) -O2 -c  $<

all:		${LIB} ${BINS}

clean:
	rm -f ${LIB} ${OBJS} ${BINS}

libArcDecrypt.so:	ArcDecrypt.o
	gcc -ggdb -shared -o libArcDecrypt.so  $< -L. 

../bin/arcChatGPTEncrypt: arcChatGPTEncrypt.o
	gcc -ggdb  -o ../bin/arcChatGPTEncrypt $<

testDecrypt: testDecrypt.o
	gcc  -ggdb -o testDecrypt $< -L . -lArcDecrypt
