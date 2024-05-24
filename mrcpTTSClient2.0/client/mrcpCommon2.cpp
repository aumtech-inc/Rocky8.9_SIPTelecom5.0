/*------------------------------------------------------------------------------
//Program Name:   mrcpCommon2.cpp
Purpose     :   MRCP Client v2.0 Common Routines.
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.
------------------------------------------------------------------------------*/
#define MRCP2_COMMON_GLOBALS
#include "mrcpCommon2.hpp"
#include "mrcp2_HeaderList.hpp"

extern "C"
{
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <arpa/inet.h>
}

using namespace std;
using namespace mrcp2client;

// char			gAppResponseFifo[128];
// static int		gAppResponseFifoFd = 0;

#define	MAX_PORTS		960
int		gTtsMrcpPort[MAX_PORTS];
char	gMrcpTtsDetailsFile[256]=".mrcpTTSRtpDetails";

/* 
 * RequestId is a single class to gurantee montonically increasing request-id's.  
 */

//=====================
// class ClientRequest
//=====================
ClientRequest::ClientRequest(const string& xVersion,
                    const string& xMethod,
                    const string& xRequestId,
					list<MrcpHeader>& xHeaders,
                    const int contentLength)
{
	string colon = ":";
	string space = " ";
	string crlf = "\r\n";

	version = xVersion + space;

	method = space + xMethod + space;
	requestId = xRequestId + crlf;

	// headers
	list<MrcpHeader>::iterator iter = xHeaders.begin();

	while(iter != xHeaders.end() )
	{
		MrcpHeader cnode = *iter;
		headers +=  cnode.getName() + colon + cnode.getValue() + crlf;
		++iter;
	}

	headers += crlf;

	// total msgLength = summation [startline + headers + content]_length
	msgLength = contentLength; 
}

string ClientRequest::buildMrcpMsg()
{
	char mod[] ="ClientRequest::buildMrcpMsg";
	string msg = "";
	char strMsgLength[10];
	int tmpSize, nDigits1, nDigits2;

	tmpSize = version.size() + method.size() 
			+ requestId.size() + headers.size()
			+ msgLength;  //here msgLength is the content length
/*
	tmpSize = version.size() + method.size() 
			+ requestId.size() + headers.size(); 
*/
//	mrcpClient2Log(__FILE__, __LINE__, -1,
//         mod, REPORT_NORMAL, MRCP_2_BASE, INFO,
//         "tmpSize=%d, contentLength=%d,", tmpSize, msgLength);

	nDigits1 = _numOfDigits(tmpSize);

	tmpSize = tmpSize + nDigits1;

	nDigits2 = _numOfDigits(tmpSize);

	msgLength = tmpSize + nDigits2 - nDigits1;
/*
	msgLength += tmpSize + nDigits2 - nDigits1;
*/

//	mrcpClient2Log(__FILE__, __LINE__, -1,
//          mod, REPORT_NORMAL, MRCP_2_BASE, INFO,
//          "nDigits2=%d, nDigits1=%d, msgLength=%d.", 
//			nDigits2, nDigits1, msgLength);


	sprintf(strMsgLength, "%d", msgLength);

	// msg does not include content message
	msg = version + string(strMsgLength) + method + requestId + headers; 

//	mrcpClient2Log(__FILE__, __LINE__, -1,
//            mod, REPORT_NORMAL, MRCP_2_BASE, INFO,
//            "final msgLength=%d, msg =(%s).", msg.length(), msg.data() );

	return msg;
}


int ClientRequest::_numOfDigits(int i)
{

	char mod[] ="ClientRequest::_numOfDigits";

	if( i <= 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, -1,
            mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
            "input integer i=%d MUST BE POSITIVE", i );
		exit(-1);
	}

	int n = 0;
	while( i >= 1 ) 
	{ 
		n = n + 1; 
		int tmp = i/10; 
		i = tmp; 

//		mrcpClient2Log(__FILE__, __LINE__, -1,
//           mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
//          "n=%d.", n );
	}

	return n;
}

//=================================================================
int writeResponseToApp(int zCallNumber, char *zFile, int zLine, 
		void *pMsg, int zMsgLength)
//=================================================================
{
	char			mod[]="writeResponseToApp";
	int				rc;
	char			lMsg[256];
	int				fd = -1;
	struct MsgToApp	*m;
	string			appRespFifo;

	m=(struct MsgToApp *)pMsg;

	fd = gSrPort[zCallNumber].getAppResponseFd();
	if ( fd <= 0 )
	{
		if (( fd = gSrPort[zCallNumber].openAppResponseFifo(lMsg)) <= 0 )
		{
			mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_NORMAL,
					MRCP_2_BASE, ERR,
					"Unable to open application response fifo (%s).  [%s]",
					gSrPort[zCallNumber].getAppResponseFifo().c_str(),
					lMsg);
			return(-1);
		}
		mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, 
				"Successfully opened application response fifo (%s).  fd=%d",
				gSrPort[zCallNumber].getAppResponseFifo().c_str(), fd);
	}
	
	rc = write(fd, (char *)pMsg, zMsgLength);
	if(rc == -1)
	{
		mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR,
			"Failed to write %d bytes to (%s:fd=%d).  [%d, %s]",
			zMsgLength, gSrPort[zCallNumber].getAppResponseFifo().c_str(), fd,
			errno, strerror(errno));
		return(-1);
	}
	mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "Wrote %d bytes to (%s, fd=%d). "
		"msg={op:%d,c#:%d,pid:%d,ref:%d,rc:%d,msg:%s}.",
		rc,
		gSrPort[zCallNumber].getAppResponseFifo().c_str(),
		fd,
		m->opcode,
		m->appCallNum,
		m->appPid,
		m->appRef,
		m->returnCode,
		m->message);

	return(0);

} // END: writeResponseToApp():

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void reloadMrcpRtpPorts()
{
	static char mod[]="reloadMrcpRtpPorts";
	FILE	*fp;
	int		i;
	char	buf[128];
	int		telecomPort;
	int		mrcpPort;

	for (i=0; i<MAX_PORTS; i++)		// initialize all to -1
	{
		gTtsMrcpPort[i] = -1;
	}

	if ((fp = fopen(gMrcpTtsDetailsFile, "r")) == NULL )
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO, 
        	"Unable to open (%s). [%d, %s] Cannot read in mrcp rtp ports. Will attemp dynamic rtp ports.\n",
			gMrcpTtsDetailsFile, errno, strerror(errno));
		return;
	}

	while (fgets(buf, 32, fp))
	{
		if ( buf[strlen(buf)-1] == '\n' )
		{
			buf[strlen(buf)-1] = '\0';
		}
		sscanf(buf, "%d|%d", &telecomPort, &mrcpPort);
		gTtsMrcpPort[telecomPort] = mrcpPort;
	}

	for (i=0; i<MAX_PORTS; i++)
	{
		if ( gTtsMrcpPort[i] != -1 )
		{
        	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
						"gTtsMrcpPort[%d]=%d", i, gTtsMrcpPort[i]);
		}
	}

	return;
} // END: reloadMrcpRtpPorts()

//====================================================================
void mrcpClient2Log(char *zFile, int zLine, int zCallNum, char *zpMod,
int mode, int msgId, int msgType, char *msgFormat, ...)
//====================================================================
{
	char		mod[]="mrcpClient2Log";
	va_list		ap;
	char		m[1024];
	char		m1[1024];
	char		type[32];
	int			rc;
	char		lPortStr[10];
//	char		charThreadId[64]="";
	pthread_t   id = syscall(SYS_gettid);
	pthread_t   id_self = pthread_self();

	//threadId2String(id, charThreadId);

	switch(msgType)
	{
		case 0:     sprintf(type, "%s", "ERR: "); break;
		case 1:     sprintf(type, "%s", "WRN: "); break;
		case 2:     sprintf(type, "%s", "INF: "); break;
		default:    sprintf(type, "%s", ""); break;
	}

	memset((char *)m1, '\0', sizeof(m1));
	va_start(ap, msgFormat);
	vsprintf(m1, msgFormat, ap);
	va_end(ap);

	sprintf(m,"%s[%s:%d:thd-%lu:%lu] %s", type, zFile, zLine, id, id_self, m1);

	sprintf(lPortStr, "%d", zCallNum);

	LogARCMsg(zpMod, mode, lPortStr, "SRC", "ttsClient", msgId, m);

} // END: mrcpClient2Log()

/*------------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/
void threadId2String(pthread_t pt, char *charPt)
{   
    unsigned char *ptc = (unsigned char*)(void*)(&pt);
    int i;
    char        tmpThreadId[128]="";
        
    *charPt = '\0';
        
    for (i=0; i<sizeof(pt); i++)
    {   
        if ( i == 0 )
        {   
            sprintf(charPt, "%02x", (unsigned)(ptc[i]));
        }
        else
        {
            sprintf(tmpThreadId, "%s%02x", charPt, (unsigned)(ptc[i]));
            sprintf(charPt, "%s", tmpThreadId);
        }
    }

} // END: threadId2String()


/*------------------------------------------------------------------------------
doesAppExist():
------------------------------------------------------------------------------*/
int doesAppExist(int zAppPid)
{
	char		procFile[128];

	sprintf(procFile, "/proc/%d", zAppPid);
	if ( access(procFile, F_OK) != 0)
	{
		return(0);
	}

	return(1);
} // END: doesAppExist()
