#----------------------------------------------------------------------------
# Program:	Makefile 
# Purpose:	Make file for compiling the scheduling rules and archiving
#		them into the dynamic common library: libRULES.so
#
# Author:	George Wenzel
# Date:		02/02/96
# Update:	02/26/96 G. Wenzel changed names: sc_rule?_api.c to sc_rule?.c
# Update:       05/22/96 G. Wenzel removed messages.h from HEADERS (ssio stuff)
# Update:       08/13/98 G. Wenzel now builds libRULES.a too.
#----------------------------------------------------------------------------
CC		=	gcc	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
AR		= 	ar
ECHO		=	echo
MV		=	mv
LIB             =       -lnsl  -lsocket -lgen 
AR = ar
ARFLAGS = r


ISP_BASE_DIR=/home/dan
ISP_VERSION=/isp2.2
BASE=$(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6
COMMON=$(BASE)/Common
COMINC=$(COMMON)/include
COMLIB=${COMMON}/lib

SYSLIBS=-lnsl -lsocket -lgen -L/usr/ccs/lib -lelf 

UTLLIBS=-L$(COMLIB) -lUTILS -lISPLog -lRULES

INCLUDE=-I$(COMINC)

HEADERS=$(COMINC)/COMmsg.h $(COMINC)/LOGmsg.h $(COMINC)/ispinc.h  

CFLAGS=-O -s

LIB_OBJECTS	=	obj/sc_rule0.o \
			obj/sc_rule1.o \
			obj/sc_rule2.o \
			obj/sc_rule3.o \
			obj/sc_rule4.o \
			obj/sc_rule5.o \
			obj/sc_rule6.o \
			obj/sc_rule7.o \
			obj/sc_rule8.o \
			obj/sc_rule9.o 

all:	$(LIB_OBJECTS)
	mkdir -p obj
	#( echo "Building dynamic library $(COMMON)/lib/libRULES.so" )
	#( gcc -G -o $(COMMON)/lib/libRULES.so $(LIB_OBJECTS) ) 
	( echo "Building static library $(COMMON)/lib/libRULES.a" ) 
	( ar $(ARFLAGS) ${COMMON}/lib/libRULES.a $(LIB_OBJECTS) ) 

clean:	
	rm -f $(LIB_OBJECTS) ${COMMON}/lib/libRULES.so


obj/sc_rule0.o: 	sc_rule0.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule0.o - Scheduling rule 0"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule0.c
			mkdir -p obj
			@$(MV) *.o obj

obj/sc_rule1.o: 	sc_rule1.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule1.o - Scheduling rule 1"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule1.c
			mkdir -p obj
			@$(MV) *.o obj

obj/sc_rule2.o: 	sc_rule2.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule2.o - Scheduling rule 2"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule2.c
			mkdir -p obj
			@$(MV) *.o obj

obj/sc_rule3.o: 	sc_rule3.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule3.o - Scheduling rule 3"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule3.c
			mkdir -p obj
			@$(MV) *.o obj

obj/sc_rule4.o: 	sc_rule4.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule4.o - Scheduling rule 4"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule4.c
			mkdir -p obj
			@$(MV) *.o obj

obj/sc_rule5.o: 	sc_rule5.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule5.o - Scheduling rule 5"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule5.c
			mkdir -p obj
			@$(MV) *.o obj

obj/sc_rule6.o: 	sc_rule6.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule6.o - Scheduling rule 6"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule6.c
			mkdir -p obj
			@$(MV) *.o obj

obj/sc_rule7.o: 	sc_rule7.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule7.o - Scheduling rule 7"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule7.c
			mkdir -p obj
			@$(MV) *.o obj

obj/sc_rule8.o: 	sc_rule8.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule8.o - Scheduling rule 8"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule8.c
			mkdir -p obj
			@$(MV) *.o obj

obj/sc_rule9.o: 	sc_rule9.c 	$(HEADERS)
			@$(ECHO) "Building sc_rule9.o - Scheduling rule 9"
			@$(CC) $(CFLAGS) $(INCLUDE) -c sc_rule9.c
			mkdir -p obj
			@$(MV) *.o obj

