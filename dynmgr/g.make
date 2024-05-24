echo
echo "Linking ArcSipMediaMgr"
echo
g++ -Wno-write-strings   -Wno-narrowing  -Wno-int-to-pointer-cast -o 		\
	ArcSipMediaMgr 			\
	-L . 			\
	-L /home/dan/isp2.2/SIPTelecom3.6/lib 			\
	-L /home/dan/isp2.2/SIPTelecom3.6/Common/lib 		\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libeXosip2-4.1.0/src/.libs 		\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/src/osip2/.libs 		\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/libosip2-4.1.0/src/osipparser2/.libs 		\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/c-ares-1.9.1/.libs 		\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/spandsp-0.0.6/src/.libs 		\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/openssl-1.0.2k  		\
	-L /home/dan/isp2.2/SIPTelecom3.6/thirdParty/ortp-0.6.3.core.ipv6/src/.libs 		\
	-L ./spandsp 		\
	-L ./soundtouch/soundtouch/lib/ 		\
	-L ./soundtouch/  			\
	ArcSipMediaMgr.o ArcSipCommon.o dl_open.o dynVarLog.o \
	-lISPUtil -lISPCDR -lUTILS -lISPLog 		\
	-lcrypto -lortp -lcares -ltiff -lpthread -losip2 -losipparser2 -lspandsp 	\
	-lssl -lm -larctones -larcpromptcontrol -lSoundTouch -ldl -DARCSIP_VERSION=`cat .version` -DARCSIP_BUILD=`cat .build`
