set -x
gcc -g -c -I /home/dev/isp2.2/SIPTelecom3.6/Common/include -I /home/dev/isp2.2/Common/Log customEvent.c
if [ $? -ne 0 ] 
then
	exit
fi

gcc -ggdb -fPIC -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -O2 -c        \
    -I /home/dev/isp2.2/SIPTelecom3.6/Common/include        \
    -I /home/dev/isp2.2/WSC/include         \
    -I /home/dev/isp2.2/Common/Log         \
    -I /home/dev/isp2.2/SNMP/include        \
    -I . logxxprt.c
if [ $? -ne 0 ] 
then
	exit
fi

gcc -ggdb -O2	\
	-o customEvent       \
	customEvent.o logxxprt.o	\
    -L /home/dev/isp2.2/SIPTelecom3.6/Common/lib        \
    -L /home/dev/isp2.2/SIPTelecom3.6/SNMP/lib        \
    -L /home/dev/isp2.2/SIPTelecom3.6/lib        \
	-lISPCDR -lISPUtil -lISPLog -lTELLog -lloglite -lgaUtils -larcSNMP

if [ $? -ne 0 ] 
then
	exit
fi

scp customEvent arc@skywalker:/home/arc/.ISP/Telecom/Exec
