
EXTRA_DIST=master system LOAD export.c export.h

DEFS= $(PTHREAD_CFLAGS) -DG_LOG_DOMAIN=\"oRTP\"
AM_CFLAGS=  -I$(top_srcdir) $(GLIB_CFLAGS) $(IPV6_CFLAGS) $(TRUESPEECH_CFLAGS)
AM_LDFLAGS= $(PTHREAD_LDFLAGS)


if HPUXKERNEL

KERNEL_BITS= $$(getconf KERNEL_BITS)
SYSTEM= $$(/usr/bin/uname -s)
RELEASE= $$(/usr/bin/uname -r | cut -c 3-)

ORTP_OBJS=ortp.o rtpsession.o rtpparse.o rtpmod.o ortpdlkm.o rtpsignaltable.o avprofile.o telephonyevents.o

.c.o:
	make -f ../../Framework/mod_${SYSTEM}_${RELEASE}_${KERNEL_BITS}_dlkm.mk CFILE=$< \
		OFILE=$@ RELEASE=${RELEASE} KERNEL_BITS=${KERNEL_BITS} DEFS="-I.. -DHAVE_CONFIG_H" 

all-local: $(ORTP_OBJS) 
	ld -o mod.o -r $(ORTP_OBJS)

else

ortp_includedir=$(includedir)/ortp

ortp_include_HEADERS=str_utils.h rtpmod.h rtpsession.h rtp.h rtpport.h rtptimer.h \
					ortp.h telephonyevents.h sessionset.h payloadtype.h scheduler.h \
					 rtpsignaltable.h ../ortp-config.h port_fct.h rtcp.h

lib_LTLIBRARIES = libortp.la

libortp_la_SOURCES=str_utils.c str_utils.h	\
			port_fct.c port_fct.h \
			rtpmod.c rtpmod.h	\
			rtpparse.c  \
			rtpsession.c rtpsession.h rtp.h rtpport.h \
			ortpdlkm.c \
			rtpsignaltable.c rtpsignaltable.h \
			rtptimer.c	rtptimer.h \
			posixtimer.c \
			ortp.c ortp.h \
			scheduler.c scheduler.h \
			avprofile.c  \
			sessionset.c sessionset.h \
			telephonyevents.c telephonyevents.h \
			ortp-config-win32.h errno-win32.h \
			payloadtype.c payloadtype.h \
			rtcp.h


libortp_la_LIBADD=$(GLIB_LIBS) $(PTHREAD_LIBS) -lm



noinst_PROGRAMS= rtpsend rtprecv mrtpsend mrtprecv test_timer rtpmemtest tevrtpsend tevrtprecv tevmrtprecv

rtpsend_SOURCES= rtpsend.c
rtpsend_LDADD= libortp.la

rtprecv_SOURCES= rtprecv.c
rtprecv_LDADD=  libortp.la

mrtpsend_SOURCES= mrtpsend.c
mrtpsend_LDADD= libortp.la

mrtprecv_SOURCES= mrtprecv.c
mrtprecv_LDADD= libortp.la

rtpmemtest_SOURCES= rtpmemtest.c
rtpmemtest_LDADD= libortp.la

test_timer_SOURCES= test_timer.c
test_timer_LDADD=	libortp.la

tevrtpsend_SOURCES= tevrtpsend.c
tevrtpsend_LDADD=	libortp.la

tevrtprecv_SOURCES= tevrtprecv.c
tevrtprecv_LDADD=	libortp.la

tevmrtprecv_SOURCES= tevmrtprecv.c
tevmrtprecv_LDADD=	libortp.la

all-local:


endif
