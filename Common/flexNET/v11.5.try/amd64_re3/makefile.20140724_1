###############################################################################
#
#	Copyright (c) 2005-2007 Macrovision Europe Ltd. and/or Macrovision Corporation. All Rights Reserved
#	This software has been provided pursuant to a License Agreement
#	containing restrictions on its use.  This software contains
#	valuable trade secrets and proprietary information of
#	Macrovision Corporation and is protected by law.  It may
#	not be copied or distributed in any form or medium, disclosed
#	to third parties, reverse engineered or used in any manner not
#	provided for in said License Agreement except with the prior
#	written authorization from Macrovision Corporation
#
###############################################################################


MAINSRCDIR=..
SRCDIR = $(MAINSRCDIR)/machind

INCFLAGS = -I$(SRCDIR) -I.

THREADLIB = -lpthread

XTRALIB = -ldl 

XTRACFLAG = -DRELEASE_VERSION -g -O2 -m64 -D__BITS64 -pipe -pthread -DLM_INTERNAL -DFLEXLM_KITBUILD -DFLEX_STATIC -DRELEASE_VERSION -DGPLATFORM=\\"amd64_re3\\" -DLINUX -D__FD_SETSIZE=65535 -DGLIBC -DLINUX64 -DAMD64 -DREDHAT -DREL -DRHLINUX64 -DPLATFORM_AMD64_RHLINUX 

LDSHAREFLAGS = -shared 

MAJORVER = 11

#
#	Use XTRAOBJS to define your object files for vendor daemon
#	initialization routines, checkout filter, checkin callback, etc.
#
XTRAOBJS = 

LDFLAGS = 

LINTFLAGS = -a -b -h

PIC = _pic

CFLAGS = -c -g $(INCFLAGS) $(XTRACFLAG) -DNO_ACTIVATION_SUPPORT

CFLAGS_PIC = -fPIC -DIS_PIC

SRCS	= $(SRCDIR)/lsvendor.c

STRIP = strip

OBJS =  lmcrypt.o \
	lmstrip.o \
	lsvendor.o 

#
# Activation Stub
#
ACTSTUB = ./activation/lib/libnoact.a

LD=$(CC)

DAEMON = arclicd

EXECS = lmcrypt lmflex


CLIENTLIB	= liblmgr.a libcrvs.a libsb.a $(BORROWOBJ) 
LIBS		= liblmgr_as.a liblmgr_s.a $(CLIENTLIB)

LINTLIB =	llib-llmgr.a.ln

UTILS = lmhostid lmdown lminstall lmremove \
	lmreread  lmswitchr lmstat lmdiag lmver lmpath lmborrow lmswitch

all: $(EXECS) $(DAEMON) utils 



lmnewgen:	$(MAINSRCDIR)/machind/lsvendor.c $(MAINSRCDIR)/machind/lm_code.h
	./lmrand1 -i $(SRCDIR)/lsvendor.c
	$(CC) -c $(CFLAGS) lmcode.c
	$(LD) $(LDFLAGS) lmnewgen.o lmcode.o $(CLIENTLIB) $(ACTSTUB) $(XTRALIB) $(THREADLIB) \
						-o lmnewgen

daemon:	$(DAEMON)

$(DAEMON):	$(XTRAOBJS) $(LIBS) $(SRCDIR)/lsserver.h $(SRCDIR)/lm_code.h lm_new.o 
	$(MAKE) lm_new.o
	$(CC) -c $(CFLAGS) lsrvend.c 
	mv lsrvend.o lsvendor.o
	rm -f lsrvend.c
	$(LD) $(LDFLAGS) -o $(DAEMON) lsvendor.o lm_new.o $(XTRAOBJS) \
				$(LIBS) $(ACTSTUB) $(XTRALIB) $(THREADLIB)
	$(STRIP) $(DAEMON)
#	rm -f lm_new.o

lm_new.o: $(SRCDIR)/lsvendor.c $(SRCDIR)/lm_code.h $(SRCDIR)/lmclient.h \
	$(CLIENTLIB)
	./lmrand1 -i $(SRCDIR)/lsvendor.c
	$(CC) -c $(CFLAGS)  lmcode.c
	$(LD) $(LDFLAGS) lmnewgen.o lmcode.o $(CLIENTLIB) $(XTRALIB) $(THREADLIB) $(ACTSTUB) \
						-o lmnewgen
	rm -f lm_new.c
	./lmnewgen $(DAEMON) -o lm_new.c
	$(CC) -c $(CFLAGS)  lm_new.c

lm_new$(PIC).o: $(SRCDIR)/lsvendor.c $(SRCDIR)/lm_code.h $(SRCDIR)/lmclient.h \
	$(CLIENTLIB)
	./lmrand1 -i $(SRCDIR)/lsvendor.c
	$(CC) -c $(CFLAGS)  lmcode.c
	$(LD) $(LDFLAGS) lmnewgen.o lmcode.o  $(CLIENTLIB) $(XTRALIB) $(THREADLIB) $(ACTSTUB) \
						-o lmnewgen
	rm -f lm_new.c
	./lmnewgen $(DAEMON) -o lm_new.c
	$(CC) -c $(CFLAGS) $(CFLAGS_PIC) -o lm_new$(PIC).o  lm_new.c

lmcrypt: $(SRCDIR)/lmcrypt.c \
		$(SRCDIR)/lmclient.h $(SRCDIR)/lm_code.h lmprikey.h $(CLIENTLIB)
	$(CC) $(CFLAGS) $(SRCDIR)/lmcrypt.c
	$(LD) $(LDFLAGS) -o lmcrypt lmcrypt.o \
	$(CLIENTLIB) $(ACTSTUB) $(XTRALIB) $(THREADLIB)

lmprikey.h: lm_new.o


lmflex:	$(SRCDIR)/lmflex.c  $(SRCDIR)/lm_code.h $(CLIENTLIB) lm_new.o
	$(MAKE) lm_new.o
	$(CC) $(CFLAGS) $(SRCDIR)/lmflex.c
	$(LD) $(LDFLAGS) -o lmflex lmflex.o lm_new.o $(CLIENTLIB) $(ACTSTUB) $(XTRALIB) \
        $(THREADLIB)
	rm lmflex.o
	$(STRIP) lmflex
	rm -f lm_new.o


lmstrip: $(SRCDIR)/lmstrip.c
	$(CC) $(CFLAGS) $(SRCDIR)/lmstrip.c
	$(LD) $(LDFLAGS) -o lmstrip lmstrip.o $(XTRALIB) $(THREADLIB)
	rm lmstrip.o


lmutil:	liblmutil.a liblmgr.a
	$(LD) $(LDFLAGS) -o lmutil liblmutil.a $(CLIENTLIB) $(XTRALIB) $(THREADLIB)
	$(STRIP) lmutil

lmgrd:	liblmgrd.a liblmgr_s.a liblmgr.a
	$(LD) $(LDFLAGS) -o lmgrd liblmgrd.a liblmgr_s.a liblmgr.a $(XTRALIB) $(THREADLIB)
	$(STRIP) lmgrd

shared_object:	liblmgr$(PIC).a libsb$(PIC).a libcrvs$(PIC).a lm_new$(PIC).o
	$(LD) $(LDSHAREFLAGS) -o liblmgr$(MAJORVER).so \
	lm_new$(PIC).o \
	liblmgr$(PIC).a \
	libcrvs$(PIC).a \
	libsb$(PIC).a \
	$(THREADLIB) \
	-lc

lint:;	lint $(INCFLAGS) $(LINTFLAGS) $(SRCS) $(LINTLIB)

clean:;	rm -f $(OBJS) $(EXECS) $(DAEMON) $(UTILS) lmcode.c lmcode.o \
	lmnewgen lsrvend.c lmflex lm_new* *.h




lmcrypt.o:	$(SRCDIR)/lmcrypt.c $(SRCDIR)/lm_code.h $(SRCDIR)/lmclient.h
	$(CC) $(CFLAGS) $(SRCDIR)/lmcrypt.c



utils:	
	-rm -f lmhostid lmver lmdown lmremove lmreread lmswitchr \
		lmstat lmdiag lminstall lmpath lmborrow lmswitch
	ln -s lmutil lmhostid
	ln -s lmutil lmver
	ln -s lmutil lmdown
	ln -s lmutil lmremove 
	ln -s lmutil lmreread 
	ln -s lmutil lmswitchr 
	ln -s lmutil lmstat 
	ln -s lmutil lmdiag
	ln -s lmutil lminstall
	ln -s lmutil lmpath
	ln -s lmutil lmborrow
	ln -s lmutil lmswitch

oldnames_comment:
	@echo Many filenames were shortened in v5.
	@echo If you want links to the old pre-v5 names, please type
	@echo 	\'make oldnames\'

oldnames:
	rm -f lmcrypter; ln lmcrypt lmcrypter	
	-(\
		cd ../machind; \
		rm -f ls_vendor.c; ln lsvendor.c ls_vendor.c; \
		rm -f lm_client.h; ln lmclient.h lm_client.h; \
		rm -f lm_errors.h; ln lmerrors.h lm_errors.h; \
		rm -f lm_hosttype.h; ln	lmhtype.h lm_hosttype.h; \
		rm -f lmcrypter.c; ln lmcrypt.c lmcrypter.c; \
		rm -f ls_feature.h; ln lsfeatur.h ls_feature.h; \
		rm -f ls_master.h; ln lsmaster.h ls_master.h; \
		rm -f ls_server.h;  ln lsserver.h ls_server.h; \
		rm -f ls_vendor.c; ln lsvendor.c ls_vendor.c; \
	)

eval:
	rm -f lmprikey.h $(UTILS) lmcrypt.o lmpubkey.h lsvendor.o lm_new.c \
			lmcode.c lmcode.o
