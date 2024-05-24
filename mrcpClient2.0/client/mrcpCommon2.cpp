/*------------------------------------------------------------------------------
////Program Name:   mrcpCommon2.cpp
Purpose     :   MRCP Client v2.0 Common Routines.
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.
Update: 09/04/14 djb MR 4368 - writeResponseToApp() modified to to always send
                               back response
------------------------------------------------------------------------------*/
#include <stdarg.h>
#define MRCP2_COMMON_GLOBALS
#include "mrcpCommon2.hpp"
#include "mrcp2_HeaderList.hpp"

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <time.h>
#include "arcFifos.h"
}

using namespace std;
using namespace mrcp2client;
bool isItVXI = false;

// char			gAppResponseFifo[128];
// static int		gAppResponseFifoFd = 0;


/* 
 * RequestId is a single class to gurantee montonically increasing request-id's.  
 */
#if 0
//=================
// class Singleton
//=================
Singleton& Singleton::getInstance()
{
//	static Singleton s;
	counter++;
	return s;
}

int Singleton::getCounter() 
{
	return counter;
}

int Singleton::counter = 0;
#endif

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
            "Input integer i=%d MUST BE POSITIVE.", i );
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
	int				goToSaved = 0;
	int				rc = -1;
	char			lMsg[256];
	int				fd = -1;
	struct MsgToApp	*m;
	char 			appRespFifo[256];
	string			stringAppRespFifo;

	pthread_mutex_lock( &gSrPort[zCallNumber].fifoLock);

	m=(struct MsgToApp *)pMsg;

	gSrPort[zCallNumber].getAppResponseFifo(stringAppRespFifo); // BT-259

	if(stringAppRespFifo.empty())
	{
		goToSaved = 1;
		stringAppRespFifo = gSrPort[zCallNumber].getAppResponseFifo_saved();
		sprintf(appRespFifo, "%s", stringAppRespFifo.c_str());

		mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_DETAIL,
					MRCP_2_BASE, WARN, "App Response fifo was empty. Attempting to resend to (%s) "
					"for opcode %d.", appRespFifo, m->opcode);
//		pthread_mutex_unlock( &gSrPort[zCallNumber].fifoLock);
	}
	else
	{
		sprintf(appRespFifo, "%s", stringAppRespFifo.c_str());
	}

//	fd = gSrPort[zCallNumber].getAppResponseFd();
//	if ( fd <= 0 )

	if ( gSrPort[zCallNumber].getAppResponseFd() <= 0 )
	{
		if(isItVXI)
		{
			mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, 
				"[%d] Calling openAppResponseFifo with port number %d.", __LINE__, zCallNumber);

			if ( goToSaved )
			{
				fd = gSrPort[zCallNumber].openAppResponseFifo_saved(lMsg, zCallNumber);
			}
			else
			{
				fd = gSrPort[zCallNumber].openAppResponseFifo(lMsg, zCallNumber);
			}
			if ( fd <= 0 )
			{
				mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_NORMAL,
								MRCP_2_BASE, ERR,
								"[%d] Unable to open application response fifo (%s).  [%s]",
								__LINE__, appRespFifo,
								lMsg);
				pthread_mutex_unlock( &gSrPort[zCallNumber].fifoLock);
				return(-1);
			}
		}
		else
		{
			mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, 
				"[%d] Calling openAppResponseFifo without port number.", __LINE__);
			if (( fd = gSrPort[zCallNumber].openAppResponseFifo(lMsg)) <= 0 )
			{
				mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_NORMAL,
						MRCP_2_BASE, ERR,
						"[%d] Unable to open application response fifo (%s).  [%s]",
						__LINE__, appRespFifo,
						lMsg);

				pthread_mutex_unlock( &gSrPort[zCallNumber].fifoLock);
				return(-1);
			}
		}
		mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, 
				"[%d] Successfully opened application response fifo (%s).  fd=%d",
				__LINE__, appRespFifo, fd);
	}
	
	rc = write(gSrPort[zCallNumber].getAppResponseFd(),
					(char *)pMsg, zMsgLength);
	if ( goToSaved )
	{
		gSrPort[zCallNumber].closeAppResponseFifo(__FILE__, __LINE__, zCallNumber);
	}

	if(rc == -1)
	{
        if ( ( m->opcode != DMOP_DISCONNECT ) &&
             ( m->opcode != DMOP_SREXIT ) )
        {
			mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, "[%d] Failed to write %d bytes to (%s).  [errno=%d] "
				"msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,rc:%d}.",
				__LINE__,
				zMsgLength,
			    appRespFifo,	
				errno,
				m->opcode,
				m->appCallNum,
				m->appPid,
				m->appRef,
				m->appPassword,
				m->returnCode);
		}
		pthread_mutex_unlock( &gSrPort[zCallNumber].fifoLock);
		return(-1);
	}

	mrcpClient2Log(zFile, zLine, zCallNumber, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "[%d] Wrote %d bytes to (%s). "
		"msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,rc:%d}.",
		__LINE__, zMsgLength,
		appRespFifo,
		m->opcode,
		m->appCallNum,
		m->appPid,
		m->appRef,
		m->appPassword,
		m->returnCode);
	
	pthread_mutex_unlock( &gSrPort[zCallNumber].fifoLock);
	return(0);

} // END: writeResponseToApp():


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
    char        charThreadId[64]="";
	pid_t		threadID = -1;
    pthread_t   id = pthread_self();

//    threadId2String(id, charThreadId);
	threadID = syscall(SYS_gettid);

	switch(msgType)
	{
		case 0:     sprintf(type, "%s", "ERR: "); break;
		case 1:     sprintf(type, "%s", "WRN: "); break;
		case 2:     sprintf(type, "%s", "INF: "); break;
		default:    sprintf(type, "%s", ""); break;
	}

	memset((char *)m1, '\0', sizeof(m1));
	va_start(ap, msgFormat);
	vsnprintf(m1, 500, msgFormat, ap);
	va_end(ap);

	m1[511] = 0;
 
//	sprintf(m,"%s[%s:%d:pid-%d] %s", type, zFile, zLine, getpid(), m1);
//	sprintf(m,"%s[%s:%d:thd-%s] %.*s", type, zFile, zLine, charThreadId, 500, m1);
	sprintf(m,"%s[%s:%d:thd-%d] %.*s", type, zFile, zLine, threadID, 500, m1);

	sprintf(lPortStr, "%d", zCallNum);

	m[511] = 0;

	LogARCMsg(zpMod, mode, lPortStr, "SRC", "mrcpClient2", msgId, m);

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


/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
string getIpAddress (const string& zHostName)
{
    static char		mod[] = "getHostIpAddress";
    string			zIp ;
    struct hostent	*myHost;
    struct in_addr	*addPtr;
    int				err = 0;

    if (zHostName == "")
    {
		mrcpClient2Log(__FILE__, __LINE__, -1, mod,
				REPORT_NORMAL, MRCP_2_BASE, ERR,
        		"Empty hostname received (%s).  Unable to retrieve local "
				"system IP.", zHostName.c_str());
		return (string) NULL;
    }

    if ( (myHost = gethostbyname (zHostName.c_str())) == NULL )
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, mod,
				REPORT_NORMAL, MRCP_2_BASE, ERR,
        		"Unable to retrieve local system IP; gethostbyname() failed.");
		return (string) NULL;
	}

    addPtr = (struct in_addr *) *myHost->h_addr_list;

    zIp = string((char *) inet_ntoa (*addPtr));

    return zIp;

} //END: getHostIpAddress

int setVXMLVersion()
{
//   VXI_CHANGES
	char mod[] = "setVXMLVersion";
	FILE    *fin;           /* file pointer for the ps pipe */
	int i;
	char    buf[256];
	char ps[] = "arcVXML2 -v";
	
	if((fin = popen(ps, "r")) != NULL)
	{
		(void) fgets(buf, sizeof buf, fin);     /* strip off the header */
		/* get the responsibility s proc_id */
        
		if(strstr(buf, "VXI") != NULL )
		{
			isItVXI = true;
		}
	}
	else
	{
	}
	(void) pclose(fin);
	mrcpClient2Log(__FILE__, __LINE__, -1, mod,
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Set isItVXI=%d.", isItVXI);

	return 0;
}
