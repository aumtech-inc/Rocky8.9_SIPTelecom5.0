#----------------------------------------------------------------------------
# Program:      Makefile
# Purpose:      Make file Responsibility Subsystem and Responsibility
#		communications library.
# Author:   	George Wenzel
# Date:		08/13/99
#----------------------------------------------------------------------------
CC = gcc -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 
ECHO = echo
CFLAGS =  -c  -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
AR = ar
ARFLAGS = -r

BASE = $(ISP_BASE_DIR)/$(ISP_VERSION)/SIPTelecom3.6

COMINC = $(BASE)/Common/include
TELINC = $(BASE)/include

LIBPATHS  = -L $(BASE)/Common/lib  -L /home/dan/isp2.2/SIPTelecom3.6/lib

INCLUDES = -I . -I $(TELINC) -I $(COMINC)

EXECUTABLES = isp_baseresp isp_stopresp
RESPCOM_SRC =  respcom.c
RESPCOM_OBJ =  respcom.o

all : $(EXECUTABLES)

clean : 
	rm -f  $(EXECUTABLES) *.o

isp_baseresp : isp_baseresp.c portActive.c 
	@$(ECHO) "Building isp_baseresp"	
	@$(CC) -o isp_baseresp isp_baseresp.c portActive.c      \
        -I .        \
        -I /home/dan/isp2.2/SIPTelecom3.6/include       \
        -I /home/dan/isp2.2/SIPTelecom3.6/Common/include        \
        -L /home/dan/isp2.2/SIPTelecom3.6/Common/lib            \
        -L /home/dan/isp2.2/SIPTelecom3.6/lib       \
        -L /home/dan/isp2.2/SNMP/lib        \
    -L /home/dan/isp2.2/SIPTelecom3.6/Common/flexNET/v11.5/amd64_re3/lib            \
    -L /home/dan/isp2.2/SIPTelecom3.6/Common/flexNET/v11.5/amd64_re3            \
    -L /home/dan/isp2.2/SIPTelecom3.6/Common/flexNET/v11.5/amd64_re3/activation/lib             \
    -L /home/dan/isp2.2/SIPTelecom3.6/Common/flexNET/v11.5/lib   					\
			-larcLicenseV11.5 -llmgr_pic -lcrvs_pic -lsb_pic -lnoact_pic			\
			-lISPCDR  -lRULES -lUTILS -lISPUtil -lISPLog -lTELLog -lloglite -lgaUtils
			

isp_stopresp : isp_stopresp.c 
	@$(ECHO) "Building isp_stopresp"	
	@$(CC) -o isp_stopresp isp_stopresp.c \
        -I .        \
        -I /home/dan/isp2.2/SIPTelecom3.6/include       \
        -I /home/dan/isp2.2/SIPTelecom3.6/Common/include        \
        -L /home/dan/isp2.2/SIPTelecom3.6/Common/lib            \
        -L /home/dan/isp2.2/SIPTelecom3.6/lib       \
        -L /home/dan/isp2.2/SNMP/lib        \
            -lISPCDR -lRULES -lUTILS -lISPUtil -lISPLog -lTELLog -lloglite -lgaUtils

$(RESPCOM_OBJ): $(RESPCOM_SRC)
	@$(ECHO) "Building module $(RESPCOM_OBJ)"
	@$(CC) -c $(INCLUDES) $(RESPCOM_SRC)	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

portActive.o : portActive.c
	@$(ECHO) "Building module portActive.o"
	@$(CC) -c portActive.c	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

