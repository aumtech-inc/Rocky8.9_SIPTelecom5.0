
function comp_callmgr()
{
	unset FFLAGS
	echo "Compiling ArcSipCallMgr.c"
	g++ -Wno-write-strings    -DSR_MRCP -ggdb -O2 -DSR_MRCP -D__DEBUG__ -DTTS_CHANGES 	\
		-DENABLE_TRACE -DMAX_PORTS=960 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64		\
		-DOSIP_MT -DEXOSIP_VERSION_401 -c ArcSipCallMgr.c  \
		-I /home/dan/isp2.2/SIPTelecom3.6/Common/include 			\
		-I . 			\
		-I ./spandsp 			\
		-I ./soundtouch 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libeXosip2-4.1.0/src 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libeXosip2-4.1.0/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/src 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/c-ares-1.5.2 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/spandsp-0.0.6/src 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/dynmgr/soundtouch/soundtouch/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6				\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6/include				\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6/src				\
		-I /home/dan/isp2.2/SIPTelecom3.6/include 			\
		-I /usr/lib/glib-2.0/include 			\
		-I ./video_stack 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/arcavb/Current_VoiceID_Release 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/arcavb/Current_VoiceID_Release/customers/aumtech 		\
			-DARCSIP_VERSION=`cat .version` -DARCSIP_BUILD=`cat .build`
	}
	
function comp_mediamgr
{
	g++ -Wno-write-strings -Wno-narrowing   -DSR_MRCP -ggdb -O2 -DSR_MRCP -D__DEBUG__ -DTTS_CHANGES 		\
		-DENABLE_TRACE -DMAX_PORTS=960 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 			\
		-DOSIP_MT -DEXOSIP_VERSION_401					\
		-c ArcSipMediaMgr.c 					\
		-I /home/dan/isp2.2/SIPTelecom3.6/Common/include 			\
		-I . 			\
		-I ./spandsp 			\
		-I ./soundtouch 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libeXosip2-4.1.0/src 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libeXosip2-4.1.0/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/src 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/c-ares-1.5.2 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/spandsp-0.0.6/src 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/dynmgr/soundtouch/soundtouch/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/glib-2.14.4			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/glib-2.14.4/glib		\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6				\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6/include				\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6/src				\
		-I /home/dan/isp2.2/SIPTelecom3.6/include 			\
		-I /usr/lib/glib-2.0/include 			\
		-I ./video_stack 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/arcavb/Current_VoiceID_Release 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/arcavb/Current_VoiceID_Release/customers/aumtech 		\
			-DARCSIP_VERSION=`cat .version` -DARCSIP_BUILD=`cat .build`
}

function comp_other()
{
	src="$1.c"
	if [ ! -f ${src} ];
	then
		src="${1}.cpp"
	fi
	echo "Compiling ${src}"
	g++ -Wno-write-strings    -DSR_MRCP -ggdb -O2 -DSR_MRCP -D__DEBUG__ -DTTS_CHANGES 		\
		-DENABLE_TRACE -DMAX_PORTS=960 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 			\
		-DOSIP_MT -DEXOSIP_VERSION_401					\
		-c ${src} 					\
		-I /home/dan/isp2.2/SIPTelecom3.6/Common/include 			\
		-I . 			\
		-I ./spandsp 			\
		-I ./soundtouch 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libeXosip2-4.1.0/src 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libeXosip2-4.1.0/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/src 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/c-ares-1.5.2 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/spandsp-0.0.6/src 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/dynmgr/soundtouch/soundtouch/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/glib-2.14.4			\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/glib-2.14.4/glib		\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6				\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6/include				\
		-I /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6/src				\
		-I /home/dan/isp2.2/SIPTelecom3.6/include 			\
		-I /usr/lib/glib-2.0/include 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/arcavb/Current_VoiceID_Release 			\
		-I /home/dan/isp2.2/SIPTelecom3.6/arcavb/Current_VoiceID_Release/customers/aumtech 		\
			-DARCSIP_VERSION=`cat .version` -DARCSIP_BUILD=`cat .build`

#		-I ./video_stack 			\

}

function link_callmgr
{

	echo
	echo "Linking ArcSipCallMgr"
	echo
	g++ -Wno-write-strings     		\
	-o ArcSipCallMgr 			\
	-L . 			\
	-L /home/dan/isp2.2/SIPTelecom3.6/lib 			\
	-L /home/dan/isp2.2/SIPTelecom3.6/Common/lib 			\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libeXosip2-4.1.0/src/.libs 			\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/src/osip2/.libs 			\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/src/osipparser2/.libs 			\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/c-ares-1.9.1/.libs 			\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/spandsp-0.0.6/src/.libs			\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/openssl-1.0.2k			\
	ArcSipCallMgr.o RegistrationInfo.o RegistrationHandlers.o SubscriptionHandlers.o		\
	SubscriptionInfo.o IncomingCallHandlers.o OutboundCallHandlers.o callTerminateList.o		\
	SipInit.o Options.o CallOptions.o enum.o ArcSipCommon.o dl_open.o recycle.o			\
	arc_sdp.o osip_utils.o dtmf_utils.o dynVarLog.o             \
	-lISPUtil -lISPCDR -lUTILS -lISPLog -lcrypto       \
	-leXosip2 -losip2 -losipparser2 -lssl -lcares -lpthread -ldl -lm			\
	-DARCSIP_VERSION=`cat .version` -DARCSIP_BUILD=`cat .build`


#	-leXosip2 -losip2 -losipparser2 -lssl -lcares -lpthread -ldl -lspandsp -lm		\
}

function link_mediamgr
{
	echo
	echo "Linking ArcSipMediaMgr"
	echo
	g++ -Wno-write-strings     	\
		-o ArcSipMediaMgr 			\
		-L . 			\
		-L /home/dan/isp2.2/SIPTelecom3.6/lib 			\
		-L /home/dan/isp2.2/SIPTelecom3.6/Common/lib 			\
		-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libeXosip2-4.1.0/src/.libs 			\
		-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/src/osip2/.libs 			\
		-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/src/osipparser2/.libs 			\
		-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/c-ares-1.9.1/.libs 			\
		-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/spandsp-0.0.6/src/.libs 			\
		-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6/src/.libs 			\
		-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/tiff-3.9.4/libtiff/.libs				\
		-L ./spandsp 			\
		-L ./soundtouch/soundtouch/lib/ 			\
		-L ./soundtouch/ 				\
		-L /home/dan/isp2.2/SIPTelecom3.6/arcavb/Current_VoiceID_Release/lib		\
		ArcSipMediaMgr.o ArcSipCommon.o dl_open.o dynVarLog.o \
		-lISPUtil -lISPCDR -lUTILS -lISPLog -lcrypto -lortp -lcares -ltiff		\
		-lpthread -losip2 -losipparser2 -lspandsp -lm -larctones -larcpromptcontrol		\
		-lSoundTouch -ldl -DARCSIP_VERSION=`cat .version` -DARCSIP_BUILD=`cat .build`
#		-lSoundTouch -lVoiceID_APIlogic -lVoiceIDutils -ldl -DARCSIP_VERSION=`cat .version` -DARCSIP_BUILD=`cat .build`
}


#-----------------------------------------------
# main
#-----------------------------------------------

#comp_callmgr
#comp_mediamgr

#link_mediamgr
link_callmgr
exit
for x in RegistrationInfo RegistrationHandlers SubscriptionInfo IncomingCallHandlers OutboundCallHandlers callTerminateList SipInit Options CallOptions enum ArcSipCommon dl_open recycle arc_sdp osip_utils dtmf_utils dynVarLog SubscriptionHandlers
do
	comp_other $x
done
link_callmgr
link_mediamgr

exit
