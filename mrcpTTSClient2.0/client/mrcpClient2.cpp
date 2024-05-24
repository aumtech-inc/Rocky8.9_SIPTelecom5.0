/*------------------------------------------------------------------------------
Program Name:   mrcpClient2.cpp
				//case EXOSIP_CALL_RELEASED:  // received BYE event
Purpose     :   MRCP Client v2.0
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.
Update: 09/25/07 djb Added multiple ttsClient logic.

Description:
- read config parameters from configuration file: 
    mrcpServer, mrcpSreverPort, uri, rtspVersion, 
    mrcpVersion, transport, sendDescribe
- initialize eXosip stack
- create and start readerThread: 
   - read from application FIFO  
   - send INVITE/BYE to server or other requests to requestQueue
- create and start sipThread
   - waiting for sip events from server, esp. 200 OK or BYE event
   - create a mrcpThread to handle requestQueue and communicate with server
------------------------------------------------------------------------------*/


#include <iostream>
#include <pthread.h>
#include "mrcpCommon2.hpp"
#include "readerThread.hpp"
#include "sipThread.hpp"
#include "ttsMRCPClient.hpp"

#include <netdb.h>

#ifdef EXOSIP_4
#include <eXosip2/eXosip.h>
#else
#include "eXosip2.h"
#endif // EXOSIP_4


extern "C"
{
	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>
	#include <signal.h>
	#include <errno.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/utsname.h>
	#include "UTL_accessories.h"
	#include "arcFifos.h"
#ifdef EXOSIP_4
	#include "eXosip2/eX_setup.h"
#endif // EXOSIP_4
}

using namespace std;
using namespace mrcp2client;


static int	initializeClient();
static void exitClient(int arg);
static void cleanUpGlobalData();

int     gClientId;  /*DDN: Added gClientId to support multiple clients*/
int gCanProcessReaderThread = 0;
int gCanProcessSipThread = 0;
int gCanProcessMainThread = 0;

extern int		gTtsMrcpPort[];

int bindTtsClient()
{
    int yRc = -1;
    char mod[] = "bindTtsClient";
    int zCall = -1;

    cpu_set_t mask;
    int yIntCpuUsed = -1;
    struct utsname systemInfo;
    int yIntTotalCpus = 0;

    /*Find version information for this OS*/
    yRc = uname(&systemInfo);
    if(yRc != 0)
    {
        return -1;
    }
    else
    {
    }
    /*END:Find version information for this OS*/

    /*Set CPU Affinity*/
    if(strstr(systemInfo.release, "2.4.21") != NULL)
    {
        if(yRc = sched_getaffinity(getpid(), sizeof(mask), &mask) < 0)
        {
            return -1;
        }
        else
        {
        }
        
        for(int s = 0; s < 30; s++)
        {   
            if(!CPU_ISSET(s, &mask))
            {   
                yIntCpuUsed = --s;
                yIntTotalCpus = yIntCpuUsed + 1;
                break;
            }
        }
        
        CPU_ZERO(&mask);
        
        CPU_SET((1) % yIntTotalCpus, &mask);
        
        if(yRc = sched_setaffinity(getpid(), sizeof(mask), &mask) < 0)
        {   
            mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL,
                MRCP_2_BASE, ERR,
                "Failed to set TTS Client(%d) affinity to CPU (%d)",
                0, (1) % yIntTotalCpus);
            return -1;
        }
            
        mrcpClient2Log(__FILE__, __LINE__, -1, mod,
            REPORT_VERBOSE, MRCP_2_BASE, INFO,
            "Set TTS Client(%d) affinity to CPU (%d)",
            0, (1) % yIntTotalCpus);

    }
    /*END:Set CPU Affinity*/

    return 0;

}/*END: int bindTtsClient()*/

////////////////////////////////
int main(int argc, char* argv[])
////////////////////////////////
{
	char		mod[] = "main";
	int			rc = -1;
	int			yPort = -1;

#ifdef EXOSIP_4
    struct eXosip_t *pexcontext;
#endif // EXOSIP_4

	gClientId = 0;

    if(argc >= 2)
    {
        gClientId = atoi(argv[1]);
    }

	if(argc == 2 && strcmp(argv[1], "-v") == 0)
	{
		cout << "Aumtech's MRCP 2.0 TTS Client for SIP Telecom.  ";
		cout << "Compiled on " << __DATE__ << " " << __TIME__ << "." << endl;
		exit(0);
	}

	mrcpClient2Log(__FILE__, __LINE__, yPort, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO, 
		"Starting Aumtech's MRCP 2.0 TTS Client for SIP Telecom.");

    signal(SIGINT, exitClient);
    signal(SIGTERM, exitClient);
    signal(SIGQUIT, exitClient);
	signal(SIGHUP, exitClient);

	bindTtsClient();
	if ( (rc = initializeClient()) != 0 )
	{
		exit(-1);  // error message is logged in routine
	}

	gCanProcessMainThread = 1;
	gCanProcessSipThread = 1;
	gCanProcessReaderThread = 1;

	//create sipThread and readerThreads
    SipThread* sipThread = SipThread::getInstance();
    sipThread->start();

	ReaderThread readerThread;
    readerThread.start();

	// check if threads still alive
	pthread_t sipThreadId = sipThread->getThreadId();
	pthread_t readerThreadId = readerThread.getThreadId();

	while(gCanProcessMainThread)
	{
		if (  ((rc = pthread_kill(sipThreadId, 0)) == ESRCH) 
			|| ( (rc = pthread_kill(readerThreadId, 0)) == ESRCH) )
		{
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, 
				"SipThread or ReaderThread died. ttsClient system shutdown.");
			cleanUpGlobalData();
			break;
		}

		sleep(5);

	} // END: for()

	// MR 5007 - had to also verify if readerThread goes away,
	//           this properly shuts down so it can be restarted
	//
	gCanProcessMainThread = 0;
	gCanProcessSipThread = 0;
	gCanProcessReaderThread = 0;

    pthread_kill(readerThreadId, SIGTERM);
    pthread_kill(sipThreadId, SIGTERM);

	sleep(5);
	pthread_exit(NULL);

	exit(0);

} // END: main()


//============================
static int initializeExosip2() 
//============================
{
	char mod[] = "initializeExosip2 ";
	int yPort  = -1;
	int rc = -1;
	int exosipInitCount = 0;
	char localHostName[256];
	string localHostIp;
	string transportLayer;

#ifdef EXOSIP_4
    static eXosip_t	*eXcontextL;
    eXosip_t		*leXcontext;
#endif // EXOSIP_4

	while (1)
	{
#ifdef EXOSIP_4
        eXcontextL = eXosip_malloc ();
        rc = eXosip_init (eXcontextL);
#else
		rc = eXosip_init ();
#endif // EXOSIP_4

		if (rc < 0)
		{
			if (exosipInitCount > 10)
			{
				mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_NORMAL,
					MRCP_2_BASE, ERR, "eXosip_init() failed; rc=%d.  Unable to "
					"initialize eXosip.  Cannot continue.", rc);
				return(-1);
			}
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_NORMAL,
					MRCP_2_BASE, ERR, "eXosip_init() failed; rc=%d.  "
					"Retrying...", rc);
			sleep (5);
			continue;
		}
		else
		{
#ifdef EXOSIP_4
			gMrcpInit.setEx4Context(eXcontextL);
			mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "eXosip_init(addr=%u) succeeded. rc=%d", eXcontextL, rc);
#endif // EXOSIP_4
			break;
		}

	} // END: while(1)

	FILE *eXosip_traceFile = NULL;
	if(access("/tmp/ttsClient.debug", F_OK) == 0)
	{
    	if ( (eXosip_traceFile = fopen("ttsClient.nohup", "a+")) == NULL)
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"fopen(mrcpClient2.nohup) failed. [%d, %s]", errno, strerror(errno));
		}
		else
		{
			TRACE_INITIALIZE(END_TRACE_LEVEL, eXosip_traceFile);

			mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Executed TRACE_INITIALIZE(END_TRACE_LEVEL, )");
		}
	}

    localHostIp = gMrcpInit.getLocalIP();

    if ( localHostIp.empty() )
    {
        mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_NORMAL,
                MRCP_2_BASE, ERR, "Local IP is empty; unable to continue.  "
                "Verify the (MRCP:localIP) in the arcMRCP2.cfg file and "
                "restart.");

        for (;;)
        {
            sleep(3);
        }
    }

	transportLayer = gMrcpInit.getTransportLayer(TTS);

	if ( strcmp(transportLayer.c_str(), "TCP") == 0 )
	{
#ifdef EXOSIP_4
        rc = eXosip_listen_addr(eXcontextL, IPPROTO_TCP, localHostIp.c_str(),
                    gMrcpInit.getSipPort(TTS), AF_INET, 0);
#else
		rc = eXosip_listen_addr(IPPROTO_TCP, localHostIp.c_str(),
					gMrcpInit.getSipPort(TTS), AF_INET, 0, NULL);
#endif // EXOSIP_4

		mrcpClient2Log(__FILE__, __LINE__, -1, mod,
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Listening on TCP socket %d.",
				gMrcpInit.getSipPort(TTS));
	}
	else
	{
#ifdef EXOSIP_4
        rc = eXosip_listen_addr(eXcontextL, IPPROTO_UDP, localHostIp.c_str(),
                    gMrcpInit.getSipPort(TTS), AF_INET, 0);
#else
		rc = eXosip_listen_addr(IPPROTO_UDP, localHostIp.c_str(),
					gMrcpInit.getSipPort(TTS), AF_INET, 0, NULL);
#endif // EXOSIP_4

		mrcpClient2Log(__FILE__, __LINE__, -1, mod,
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Listening on UDP socket %d.",
				gMrcpInit.getSipPort(TTS));
	}

	if (rc != 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR, "eXosip_listen_to() on (%s:%d) failed; rc=%d. "
			"Cannot continue. Please check MRCP configuration file.", 
			localHostIp.c_str(), gMrcpInit.getSipPort(TTS), rc);

#ifdef EXOSIP_4
		eXosip_quit(eXcontextL);
#else
		eXosip_quit();
#endif // EXOSIP_4

		return(-1);
	}
	return(0);

} // END: initializeExosip2()

/*------------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/
static int initializeClient()
{
	char    mod[] = "initializeClient";
	char    fifoDir[128];
	int     rc;
	string	localHostIp;
	char	tmpMsg1[256];
	string	sFifoDir;

//    rc = gethostname (localHostName, 255);
 //   localHostIp = getIpAddress (localHostName);

    gMrcpInit.setClientId(gClientId);/*DDN: Added to support multiple clients*/

	if (( rc == gMrcpInit.readCfg()) == -1 )
	{
		return(-1);
	}

    memset((char *)fifoDir, '\0', sizeof(fifoDir));
    if ((rc = UTL_GetArcFifoDir(DYNMGR_FIFO_INDEX, fifoDir,
                    sizeof(fifoDir), tmpMsg1, sizeof(tmpMsg1))) != 0 )
    {
        sprintf(fifoDir, "%s", "/tmp");
        mrcpClient2Log(__FILE__, __LINE__, -1, mod,
                REPORT_DETAIL, MRCP_2_BASE, WARN,
        		"Defaulting fifo directory to %s.  %s", fifoDir, tmpMsg1);
    }
    sFifoDir = string(fifoDir);
    gMrcpInit.setDynmgrFifoDir(sFifoDir);

    memset((char *)fifoDir, '\0', sizeof(fifoDir));
    if ((rc = UTL_GetArcFifoDir(MRCP_FIFO_INDEX, fifoDir,
                    sizeof(fifoDir), tmpMsg1, sizeof(tmpMsg1))) != 0 )
    {
        sprintf(fifoDir, "%s", "/tmp");
        mrcpClient2Log(__FILE__, __LINE__, -1, mod,
                REPORT_DETAIL, MRCP_2_BASE, WARN,
        		"Defaulting fifo directory to %s.  %s", fifoDir, tmpMsg1);
    }
    sFifoDir = string(fifoDir);
    gMrcpInit.setMrcpFifoDir(sFifoDir);

    memset((char *)fifoDir, '\0', sizeof(fifoDir));
    if ((rc = UTL_GetArcFifoDir(APPL_FIFO_INDEX, fifoDir,
                    sizeof(fifoDir), tmpMsg1, sizeof(tmpMsg1))) != 0 )
    {
        sprintf(fifoDir, "%s", "/tmp");
        mrcpClient2Log(__FILE__, __LINE__, -1, mod,
                REPORT_DETAIL, MRCP_2_BASE, WARN,
        		"Defaulting fifo directory to %s.  %s", fifoDir, tmpMsg1);
    }
    sFifoDir = string(fifoDir);
    gMrcpInit.setApplFifoDir(sFifoDir);

	gMrcpInit.printInitInfo();

	rc = initializeExosip2();	

	pthread_mutex_init(&callMapLock, NULL);

	reloadMrcpRtpPorts();

	return(rc);		// rc was set from initializeExosip2() above

} // END: initializeClient


/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static void exitClient(int arg)
{

	mrcpClient2Log(__FILE__, __LINE__, -1, "exitClient",
			REPORT_DETAIL, MRCP_2_BASE, WARN, "system cleanup shutting down.");

	gCanProcessReaderThread = 0;
	gCanProcessSipThread = 0;
	gCanProcessMainThread = 0;

	cleanUpGlobalData();

	exit(0);

} // END: exitClient


/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static void cleanUpGlobalData()
{
	int		noSessionsStillOpen;
	int		j;

	pthread_mutex_lock( &callMapLock);

	callMap.clear(); 
	pthread_mutex_unlock( &callMapLock);
	pthread_mutex_destroy( &callMapLock);

	noSessionsStillOpen = 1;
	j = 0;
	while ( noSessionsStillOpen )
	{
		noSessionsStillOpen = 0;
		for(int i = 0; i < MAX_PORTS; i++)
		{
			if(gSrPort[i].getPid() >= 0)
			{
				mrcpClient2Log(__FILE__, __LINE__, i, "cleanGlobalData", 
					REPORT_DETAIL, MRCP_2_BASE, INFO, 
					"Port %d is still active with pid %d.  Setting switch "
					"to shut it down.", i, gSrPort[i].getPid());
				noSessionsStillOpen = 1;
				gSrPort[i].setCloseSession(1);
			}
		} // END: for(int i=0; ...)

		if ( noSessionsStillOpen )
		{
			usleep(PIPE_READ_TIMEOUT * 1000000 + 10000);
		}

		if ( j >= 3 )
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, "cleanGlobalData", 
					REPORT_NORMAL, MRCP_2_BASE, INFO, 
					"Sessions are still open; may be in wait state. Shutting down.");
			break;
		}
		j++;
	}

} // END: cleanGlobalData()


