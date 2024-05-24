#----------------------------------------------------------------------------
# Program:      Makefile
# Purpose:      Make file for building the ISP 2.2 Responsibility Subsystem.
#		This consists of executables isp_baseresp, and isp_stopresp.
# Author:   	George Wenzel
# Date:		08/13/99
# Update: 	08/13/99 gpw no longer makes its own respcom lib; uses Common
# Update: 	10/27/00 gpw changed flexlm library locations
#----------------------------------------------------------------------------
CC = gcc 
ECHO = echo
CFLAGS =  -c
CFLAGS_E = -o 
AR = ar
ARFLAGS = -r

#BASE = $(ISP_BASE_DIR)/$(ISP_VERSION)
BASE = /home/arc/murali/isp2.2
# FLEXLM_INC=$(BASE)/Common/flexlm/v7.0/machind
# FLEXLM_OBJ=$(BASE)/Common/flexlm/v7.0/i86_r6
# FLEXLM_LIB=$(BASE)/Common/flexlm/v7.0/i86_r6

COMINC = $(BASE)/Common/include
RESINC = $(BASE)/Resp
COMLIB = $(BASE)/Common/lib
TELINC = $(BASE)/include

# LIBPATHS  = -L $(BASE)/lib -L $(BASE)/Common/lib -L $(BASE)/Common/Log -L $(FLEXLM_LIB)
LIBPATHS  = -L $(BASE)/lib -L $(BASE)/Common/lib -L ${BASE}/SNMP/lib -L $(BASE)/Common/Log

UTLLIBS  = -lUTILS  -lRULES

#LICLIBS  = -larcLicense  

#SYSLIBS	= -lposix

LOGLIBS  = -lISPLog 
LOGLIB	= $(BASE)/Common/lib/libISPLog.a

#LIBS = -lISPUtil -lUTILS -lRULES -lgaUtils  -lISPLog -lloglite -larcSNMP -lsnmp  -lcrypto
LIBS = -lISPUtil -lUTILS -lRULES -lgaUtils  -lISPLog -lloglite -larcSNMP -lsnmp 

INCLUDES = -I . -I $(TELINC) -I $(COMINC) 
EXECUTABLES = isp_baseresp isp_stopresp

all : $(EXECUTABLES)

clean : 
	rm -f  $(EXECUTABLES) isp_baseresp.o portActive.o

isp_baseresp : isp_baseresp.c portActive.c
	@$(ECHO) "Building isp_baseresp"
	@$(CC) -DDIALOGIC $(CFLAGS_E) isp_baseresp isp_baseresp.c portActive.c $(INCLUDES) $(LIBPATHS) ${LIBS}

isp_stopresp : isp_stopresp.c 
	@$(ECHO) "Building isp_stopresp"	
	@$(CC) $(CFLAGS_E) isp_stopresp isp_stopresp.c $(INCLUDES) $(LIBPATHS) $(UTLLIBS) $(LOGLIBS) $(IVIEWLIBS) $(SYSLIBS)

$(EXECUTABLES): $(COMINC)/BNS.h

$(EXECUTABLES): $(COMINC)/ISPSYS.h

$(EXECUTABLES): $(RESINC)/resp.h

$(EXECUTABLES): $(COMINC)/shm_def.h

$(EXECUTABLES): $(COMINC)/err_msg.h

$(EXECUTABLES): $(LOGLIB) 

${LOGLIB}:	
