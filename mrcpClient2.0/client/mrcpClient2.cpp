/*------------------------------------------------------------------------------
Program Name:   mrcpClient2.cpp
				//case EXOSIP_CALL_RELEASED:  // received BYE event
Purpose     :   MRCP Client v2.0
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.
Update: 11/06/09 djb MR 2696 - Added log msg (1104 TEL_ETVSIRGO) on startup
                     failure.
Update: 07-22-14 djb	MR 4327 - Commented out the pthread_kill() on exit.

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
#include <sys/utsname.h>
#include "mrcpCommon2.hpp"
#include "readerThread.hpp"
#include "sipThread.hpp"

#include <netdb.h>
#ifdef EXOSIP_4
#include "eXosip2/eXosip.h"
#else
#include "eXosip2.h"
#endif // EXOSIP_4

extern "C"
{
	#include <string.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <signal.h>
	#include <errno.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/time.h>
	#include <time.h>
    #include <mcheck.h>

	#include "UTL_accessories.h"
	#include "arcFifos.h"

}

using namespace std;
using namespace mrcp2client;

static pthread_t sipThreadId;
static pthread_t readerThreadId;

static int	initializeClient();
static void exitClient(int arg);
static void exitClient_sigterm(int sig, siginfo_t *info, void *ctx);
static void cleanUpGlobalData();
int     gClientId;  /*DDN: Added gClientId to support multiple clients*/

int	gCanProcessReaderThread = 0;
int	gCanProcessSipThread = 0;
int	gCanProcessMainThread = 0;


int bindMrcpClient()
{
	int yRc = -1;
	char mod[] = "bindMrcpClient";
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

#if 0
	/*Set CPU Affinity*/
	if(strstr(systemInfo.release, "2.4.21") != NULL)
	{
		//if(yRc = sched_getaffinity(getpid(), &mask) < 0)
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
		
		//if(yRc = sched_setaffinity(getpid(), &mask) < 0)
		if(yRc = sched_getaffinity(getpid(), sizeof(mask), &mask) < 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, 
				"Failed to set MRCP Client(%d) affinity to CPU (%d)",
				0, (1) % yIntTotalCpus);
			return -1;
		}
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Set MRCP Client(%d) affinity to CPU (%d)",
			0, (1) % yIntTotalCpus);
}
	/*END:Set CPU Affinity*/
#endif
	
	return 0;

}/*END: int bindMrcpClient()*/


////////////////////////////////
int main(int argc, char* argv[])
////////////////////////////////
{
	char				mod[] = "main";
	int					rc = -1;
	int					yPort = -1;
	struct timeval		tp;
    struct timezone		tzp;
    struct tm			*tm;
	char				timeStr[256];
	struct  sigaction	sig_act, sig_oact;
	static time_t		last_time_modified;
	struct stat			file_stat;
	string				cfgFile;
	struct sigaction	act;

#ifdef EXOSIP_4
    struct eXosip_t *pexcontext;
#endif // EXOSIP_4

#if 0
    mtrace();
#endif

	pthread_mutex_init(&callMapLock, NULL);

	/*DDN: Added to support multiple clients*/
	gClientId = 0;
	if(argc >= 2)
	{
		gClientId = atoi(argv[1]);
	}

	if(argc == 2 && strcmp(argv[1], "-v") == 0)
	{
		#ifdef SDM
			cout << "Aumtech's MRCP 2.0 Client for SDM Telecom (pstn).";
		#else
			cout << "Aumtech's MRCP 2.0 Client for SIP Telecom.";
		#endif
		cout << "Compiled on " << __DATE__ << " " << __TIME__ << "." << endl;
		exit(0);
	}

#ifdef SDM
	mrcpClient2Log(__FILE__, __LINE__, yPort, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO, 
		"Starting Aumtech's MRCP 2.0 Client for SDM Telecom (pstn).");
#else
	mrcpClient2Log(__FILE__, __LINE__, yPort, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO, 
		"Starting Aumtech's MRCP 2.0 Client for SIP Telecom.");
#endif

    signal(SIGINT, exitClient);
    signal(SIGQUIT, exitClient);
	signal(SIGHUP, exitClient);

	memset (&act, '\0', sizeof(act));
    act.sa_sigaction = &exitClient_sigterm;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGTERM, &act, NULL) < 0)
    {
        mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL,
            MRCP_2_BASE, ERR,
            "sigaction(SIGTERM): system call failed. [%d, %s]",
            errno, strerror(errno));
        pthread_detach(pthread_self());
        pthread_exit(NULL);
    }


    /* set death of child function */
    sig_act.sa_handler = NULL;
    sig_act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
    if(sigaction(SIGCHLD, &sig_act, &sig_oact) != 0)
    {
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "sigaction(SIGCHLD, SA_NOCLDWAIT): system call failed. [%d, %s]",
            errno, strerror(errno));
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			1104, ERR,  "Unable to start speech recognition");
        exit(-1);
    }

    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "signal(SIGCHLD, SIG_IGN): system call failed. [%d, %s]",
            errno, strerror(errno));
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			1104, ERR,  "Unable to start speech recognition");
        exit(-1);
    }
    /*END: set death of child function */

	bindMrcpClient();/*Bind the client to last CPU available*/

	if ( (rc = initializeClient()) != 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR,  "Unable to start speech recognition");
		exit(-1);  // error message is logged in routine
	}

	last_time_modified = time((time_t *) NULL);
	cfgFile = gMrcpInit.getCfgFile();

	gCanProcessMainThread = 1;
	gCanProcessSipThread = 1;
	gCanProcessReaderThread = 1;

	//create sipThread and readerThreads
    SipThread* sipThread = SipThread::getInstance();
    sipThread->start();

	ReaderThread readerThread;
    readerThread.start();

	// check if threads still alive
//	pthread_t sipThreadId = sipThread->getThreadId();
//	pthread_t readerThreadId = readerThread.getThreadId();
	sipThreadId = sipThread->getThreadId();
	readerThreadId = readerThread.getThreadId();
	mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, 
			"Retrieved sipThreadId=%d; readerThreadId=%d",
			sipThreadId, readerThreadId);

	while (gCanProcessMainThread)
	{
		if (  ((rc = pthread_kill(sipThreadId, 0)) == ESRCH) 
			|| ( (rc = pthread_kill(readerThreadId, 0)) == ESRCH) )
		{
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, 
				"SipThread or ReaderThread died. MrcpClient2 system shutdown.");
			//cleanUpGlobalData();
			break;
		}

		sleep(5);

		if((rc = access(cfgFile.c_str(), R_OK)) != 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_DETAIL,
					MRCP_2_BASE, WARN, "The configuration file (%s) in not "
					"accessible. rc=%d [%d, %s] ",
					cfgFile.c_str(), rc, errno, strerror(errno));
			continue;
		}

		if(stat(cfgFile.c_str(), &file_stat) == 0)
		{
			if(file_stat.st_mtime != last_time_modified)
			{
				last_time_modified = file_stat.st_mtime;
				mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_VERBOSE,
					MRCP_2_BASE, INFO,  "Config file (%s) has changed.  "
					"Reloading...", cfgFile.c_str());
				rc = gMrcpInit.readCfg();
				gMrcpInit.printInitInfo();
			} /* stat */
		}

	} // END: for()

	gCanProcessMainThread = 0;
	gCanProcessSipThread = 0;
	gCanProcessReaderThread = 0;

    pthread_kill(readerThreadId, SIGTERM);
    pthread_kill(sipThreadId, SIGTERM);

#ifdef EXOSIP_4
    pexcontext=gMrcpInit.getExcontext();
    eXosip_quit(pexcontext);
#endif // EXOSIP_4

//	mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_VERBOSE,
//			MRCP_2_BASE, INFO, 
//			"Attempting to pthread_kill readerThreadId=%d", readerThreadId);
//	pthread_kill(readerThreadId, SIGTERM);
//
//	mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_VERBOSE,
//			MRCP_2_BASE, INFO, 
//			"Attempting to pthread_kill sipThreadId=%d", sipThreadId);
//	pthread_kill(sipThreadId, SIGTERM);
//
	mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO,  "Exiting.");
	sleep(5);
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
    static eXosip_t     *eXcontextL;
#endif // EXOSIP_4

    FILE *eXosip_traceFile = NULL;

	if(access("/tmp/mrcpClient2.debug", F_OK) == 0)
	{
		eXosip_traceFile = fopen("mrcpClient2.nohup", "a+");

		TRACE_INITIALIZE(END_TRACE_LEVEL, eXosip_traceFile);
	}

	while (1)
	{
		// TRACE_INITIALIZE(END_TRACE_LEVEL, NULL);

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
#endif // EXOSIP_4
			break;
		}

	} // END: while(1)

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

	transportLayer = gMrcpInit.getTransportLayer(SR);

    if ( strcmp(transportLayer.c_str(), "TCP") == 0 )
    {
#ifdef EXOSIP_4
        rc = eXosip_listen_addr(eXcontextL, IPPROTO_TCP, localHostIp.c_str(),
                    gMrcpInit.getSipPort(SR), AF_INET, 0);
#else
		rc = eXosip_listen_addr(IPPROTO_TCP, localHostIp.c_str(),
					gMrcpInit.getSipPort(SR), AF_INET, 0, NULL);
#endif // EXOSIP_4

        mrcpClient2Log(__FILE__, __LINE__, -1, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Listening on TCP socket %d.", gMrcpInit.getSipPort(SR));
	}
	else
	{
#ifdef EXOSIP_4
        rc = eXosip_listen_addr(eXcontextL, IPPROTO_UDP, INADDR_ANY,
                gMrcpInit.getSipPort(SR), AF_INET, 0);
#else
		rc = eXosip_listen_addr(IPPROTO_UDP, INADDR_ANY,
				gMrcpInit.getSipPort(SR), AF_INET, 0, NULL);
#endif // EXOSIP_4

		mrcpClient2Log(__FILE__, __LINE__, -1, mod,
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Listening on UDP socket %d.", gMrcpInit.getSipPort(SR));
	}


	if (rc != 0)
	{
					//MRCP_2_BASE, ERR, "eXosip_listen_to() on (%s:%d) failed; rc=%d. "
		mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_NORMAL,
					MRCP_2_BASE, ERR, "eXosip_listen_addr() on (%s:%d) failed; rc=%d. "
					"Cannot continue. Please check MRCP configuration file.", 
					localHostIp.c_str(), gMrcpInit.getSipPort(SR), rc);


#ifdef EXOSIP_4
        eXosip_quit(eXcontextL);
#else
		eXosip_quit();
#endif // EXOSIP_4

		sleep(5);

		return(-1);
	}

	return(0);

} // END: initializeExosip2()

/*------------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/

static void initializeLocks()
{
	//pthread_mutex_init(&callMapLock, NULL);
}

static void destroyLocks()
{
}

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
	char	failureReason[1024];

//    localHostIp = getIpAddress (localHostName);

	setVXMLVersion();
        
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
                REPORT_VERBOSE, MRCP_2_BASE, ERR,
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
                REPORT_VERBOSE, MRCP_2_BASE, ERR, 
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

	if ( (rc = initializeExosip2()) != 0 )
	{
		return(rc);
	}

	return(0);
} // END: initializeClient


/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static void exitClient(int arg)
{
	struct timeval  tp;
    struct timezone tzp;
    struct tm   *tm;
	char	timeStr[256];

	mrcpClient2Log(__FILE__, __LINE__, -1, "exitClient", 
		REPORT_VERBOSE, MRCP_2_BASE, INFO, "System cleanup shutting down.");
	
	gCanProcessReaderThread = 0;
	gCanProcessSipThread = 0;
	gCanProcessMainThread = 0;

	mrcpClient2Log(__FILE__, __LINE__, -1, "exitClient", 
		REPORT_DETAIL, MRCP_2_BASE, WARN, 
			"Sending SIGTERM to readerThreadId(%ld) and sipThreadId(%ld).",
			readerThreadId, sipThreadId);

	/*No pthread routines in alarm handler*/
#if 0
	pthread_kill(readerThreadId, SIGTERM);
	pthread_kill(sipThreadId, SIGTERM);
	cleanUpGlobalData();
#endif

	sleep(2);

	exit(0);

} // END: exitClient

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static void exitClient_sigterm(int sig, siginfo_t *info, void *ctx)

{
	static char		mod[]="exitClient_sigterm";
	struct timeval  tp;
    struct timezone tzp;
    struct tm   *tm;
	char	timeStr[256];

    mrcpClient2Log(__FILE__, __LINE__, -1, mod,
            REPORT_VERBOSE, MRCP_2_BASE, INFO,
            "SIGTERM (%d) caught from pid (%ld), user (%ld).  Exiting.",
            sig, (long)info->si_pid, (long)info->si_uid);

	mrcpClient2Log(__FILE__, __LINE__, -1, mod,
		REPORT_DETAIL, MRCP_2_BASE, WARN, "System cleanup shutting down.");
	
	gCanProcessReaderThread = 0;
	gCanProcessSipThread = 0;
	gCanProcessMainThread = 0;

	mrcpClient2Log(__FILE__, __LINE__, -1, mod,
		REPORT_DETAIL, MRCP_2_BASE, WARN, 
			"Sending SIGTERM to readerThreadId(%ld) and sipThreadId(%ld).",
			readerThreadId, sipThreadId);

	/*No pthread routines in alarm handler*/
#if 0
	pthread_kill(readerThreadId, SIGTERM);
	pthread_kill(sipThreadId, SIGTERM);
	cleanUpGlobalData();
#endif

	sleep(2);

	exit(0);

} // END: exitClient

void sigChildFunction(int arg)
{
	mrcpClient2Log(__FILE__, __LINE__, -1, "sigChildFunction", 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Received SIGCHLD signal.  Ignoring.");

	return;
} // END: sigChildFunction()


/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static void cleanUpGlobalData()
{

	pthread_mutex_lock( &callMapLock);

	callMap.clear(); 
	pthread_mutex_unlock( &callMapLock);
	pthread_mutex_destroy( &callMapLock);

#if 0
	for(int i = 0; i < MAX_PORTS; i++)
	{
		if(gSrPort[i].getApplicationPort() >= 0)
		{
			// close open sockets
        	int sockfd = gSrPort[i].getSocketfd() ;
        	if ( sockfd > 0) 
			{
				close(sockfd);
			}

			// clear other application port specific elements
			gSrPort[i].initializeElements();

			grammarList[i].removeAll();

			while( !requestQueue[i].empty()) 
			{
				requestQueue[i].pop();
			}

			// destroy locks
			pthread_mutex_destroy( &requestQueueLock[i]);
		}
	} // END: for(int i=0; ...)
#endif

} // END: cleanGlobalData
