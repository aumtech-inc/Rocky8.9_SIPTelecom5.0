/*------------------------------------------------------------------------------
Program Name:   readerThread.cpp

								action to have them processed.
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.
Update: 09/25/07 djb Added multiple ttsClient logic.
------------------------------------------------------------------------------*/
#include <pthread.h>
#include <signal.h>
#include <iostream>
#include <sys/poll.h>

#include "readerThread.hpp"
#include "mrcpThread.hpp"
#include "mrcpCommon2.hpp"
#include "mrcpTTS.hpp"
#include "ttsMRCPClient.hpp"

#include <algorithm>

extern "C"
{
	#include <string.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/time.h>
	#include <fcntl.h>
	#include <stdio.h>
	#include <errno.h>
	#include "UTL_accessories.h"
	#include "arcFifos.h"
	#include "ttsStruct.h"
}

#ifdef SDM
extern int	gTecStreamSw[MAX_PORTS];
#endif

using namespace std;
using namespace mrcp2client;

static char     gRequestFifo[256];
static int      gRequestFifoFd;

static char     gTtsRequestFifo[256] = "";
static int      gTtsRequestFifoFd = -1;
extern int      gClientId;

#define RECYCLE_BASE    5     // djb - testing only
#define RECYCLE_DELTA   1

extern int	gCanProcessReaderThread;
extern int	gCanProcessSipThread;
extern int	gCanProcessMainThread;

extern int		gTtsMrcpPort[];
extern char    gMrcpTtsDetailsFile[];

void		*readerThreadFunc(void*);
int			readAndProcessTtsRequest(int zInputFlag);

static int	openTtsRequestFifo();
//static void processInit(ARC_TTS_REQUEST_SINGLE_DM *zTtsRequestSingleDM);
static void	initGlobalData(int xPort);

static void checkUpdateMrcpRtpPorts();

static void	checkClearApp();

static void readerThreadSpeak(int zPort, 
			ARC_TTS_REQUEST_SINGLE_DM *zTtsRequestSingleDM);
/*------------------------------------------------------------------------------ 
  class ReaderThread implementation
 *-----------------------------------------------------------------------------*/ 
void ReaderThread::start()
{
	static char		mod[] = "ReaderThread::start";
	int	   	rc = -1;
	//int	    *msg = new int();
	int *msg;
	int		yPort = -1;

#if 0
	struct sigaction act;

	signal(SIGPIPE, sigpipe);
	memset (&act, '\0', sizeof(act));
	act.sa_sigaction = &readerThreadSigterm;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTERM, &act, NULL) < 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR,
			"sigaction(SIGTERM ): system call failed. [%d, %s]",
			errno, strerror(errno));
		pthread_detach(pthread_self());
		pthread_exit(NULL);
	}
#endif


    pthread_attr_t threadAttr;
    pthread_attr_init (&threadAttr);
    pthread_attr_setdetachstate (&threadAttr, PTHREAD_CREATE_DETACHED);

	//rc = pthread_create(&readerThreadId, &threadAttr, readerThreadFunc, (void*)msg);
	rc = pthread_create(&readerThreadId, &threadAttr, readerThreadFunc, (void*)NULL);

	if (rc != 0) {
		mrcpClient2Log(__FILE__, __LINE__, -1,  mod, 
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"Error creating readerThread.  rc=%d.  [%d, %s]",
			rc, errno, strerror(errno));
		
		pthread_detach(pthread_self());
		pthread_exit(NULL);
	}

} // END: ReaderThread::start()


pthread_t ReaderThread::getThreadId() const
{
	return readerThreadId;
} 

/*------------------------------------------------------------------------------ 
   void* readerThreadFunc (void* msg)
 *-----------------------------------------------------------------------------*/ 
// - read request from one or more application FIFO
// - send SIP INVITE/BYE to server 
// - load requestList 
void* readerThreadFunc (void* msg)
{
	static char	mod[]="readerThreadFunc";
	int		zPort = -1;
	int		rc;
	time_t	last_ts;
	time_t	current_ts;

	if ((rc = openTtsRequestFifo()) != 0)
	{
		return (NULL);
	}

	gCanProcessReaderThread = 1;
	time(&last_ts);
	while (gCanProcessReaderThread)
	{
		rc = readAndProcessTtsRequest(0);
		if (rc != 0 )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			   MRCP_2_BASE, ERR, "Error in readAndProcessTtsRequest.");
			continue;
		}

		usleep(10);

		// clean up global data left by past applications
		checkClearApp();

		time(&current_ts);
		if ( current_ts - last_ts > 2 )
		{
			checkUpdateMrcpRtpPorts();
			last_ts = current_ts;
		}

		if ( access(gTtsRequestFifo, F_OK) != 0 ) 		// MR 5007
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL, 
		   		MRCP_2_BASE, WARN, "Fifo (%s) does not exist; re-creating it.", gTtsRequestFifo);
			if ( gTtsRequestFifoFd > 0 )
			{
				close(gTtsRequestFifoFd);
				gTtsRequestFifoFd = -1;
			}
				
			if ((rc = openTtsRequestFifo()) != 0) // There was a problem opening the fifo.
			{                                     // Exit so this process will exit and get re-created
				gCanProcessReaderThread = 0;
			}
		}
		
	} // END: while (1)

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL, 
		   MRCP_2_BASE, WARN, "ReaderThread exiting.");
	if ( gTtsRequestFifoFd > 0 )
	{
		close(gTtsRequestFifoFd);
	}
	if ( access(gTtsRequestFifo, F_OK) == 0 )
	{
		unlink(gTtsRequestFifo);
	}
				
	pthread_detach(pthread_self());
	pthread_exit (NULL);
}


/*******************************************************************************
readAndProcessTtsRequest()
*******************************************************************************/
int readAndProcessTtsRequest(int zInputFlag)
{
	char						mod[]="readAndProcessTtsRequest";
	int							fd = gTtsRequestFifoFd;
	ARC_TTS_REQUEST_SINGLE_DM	ttsRequestSingleDM;
	ARC_TTS_REQUEST_SINGLE_DM	tmpRequest;
	short						incompleteInfo;
	int							rc, nbytes, done, err_no;
	int							yPort;
	struct pollfd				pollSet[1];

	bool            			yIntCanRecycle = true;
	static int					totalTTSSpeakReceived = 0;

	memset((ARC_TTS_REQUEST_SINGLE_DM *)&ttsRequestSingleDM, 0,
			sizeof(ARC_TTS_REQUEST_SINGLE_DM));

	done = 0;
	while(!done)
	{
		memset(&tmpRequest, 0, sizeof(ARC_TTS_REQUEST_SINGLE_DM));
		incompleteInfo = 0;

		errno = 0;
		pollSet[0].fd = fd;		/* fd of the fifo descriptor already opened */
		pollSet[0].events = POLLIN;
		rc = poll(pollSet, 1, PIPE_READ_TIMEOUT*1000);
		if(rc < 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL,
					MRCP_2_BASE, ERR, "poll of %s failed.  [%d, %s]",
					gTtsRequestFifo, errno, strerror(errno));
			return(-1);
		}

		if(pollSet[0].revents == 0)
		{
			return(0);
		}

		nbytes = read(fd, &tmpRequest, sizeof(ARC_TTS_REQUEST_SINGLE_DM));
		yPort = atoi(tmpRequest.port);
		mrcpClient2Log(__FILE__, __LINE__, yPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, 
			"Received %d bytes. msg={op:%d,c#:%s,pid:%s,ref:%d,sync=%d,%.*s}",
			nbytes,  tmpRequest.speakMsgToDM.opcode,
			tmpRequest.port,  tmpRequest.pid,
			tmpRequest.speakMsgToDM.appRef, 
			tmpRequest.async, 200, tmpRequest.string);

		if(nbytes == -1)
		{
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, 
					REPORT_NORMAL, MRCP_2_BASE, ERR, 
					"Failed to read next TTS request from (%s). [%d, %s]",
					gTtsRequestFifo, errno, strerror(errno));
			return(-1);
		}

		if(nbytes < sizeof(ARC_TTS_REQUEST_SINGLE_DM))
		{
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, 
					REPORT_NORMAL, MRCP_2_BASE, ERR, 
					"Read %d bytes when expecting %d",
					nbytes, sizeof(ARC_TTS_REQUEST_SINGLE_DM));
			return(-1);
		}

		if(tmpRequest.port[0] == '\0')
		{
			incompleteInfo = 1;
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, 
					REPORT_NORMAL, MRCP_2_BASE, ERR, 
					"NULL %s in TTS request","port");
		}

		if(tmpRequest.pid[0] == '\0')
		{
			incompleteInfo = 1;
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, 
					REPORT_NORMAL, MRCP_2_BASE, ERR, 
					"NULL %s in TTS request","PID");
		}

		if ( tmpRequest.speakMsgToDM.opcode == DMOP_HALT_TTS_BACKEND ) 
		{
			int waitCount = 0;
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Received DMOP_HALT_TTS_BACKEND opcode %d. "
				"Setting Stop flag and "
				"clearing queued TTS requests.",
                tmpRequest.speakMsgToDM.opcode);
	
#if 0
			while(gSrPort[yPort].isInProgressReceived() == false)
			{
				waitCount++;
				if(waitCount > 100)
				{
					break;
				}
				usleep(20 * 1000);
			}	
		
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "After isInProgressReceived. ");
#endif
			gSrPort[yPort].setStopProcessingTTS(true);
			gSrPort[yPort].clearTTSRequests();
			return(0);
		}

#if 0
		if(tmpRequest.string[0] == '\0')
		{
			incompleteInfo = 1;
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, 
					REPORT_NORMAL, MRCP_2_BASE, ERR, 
					"NULL %s in TTS request", "string");
		}
		if(tmpRequest.resultFifo[0] == '\0')
		{
			incompleteInfo = 1;
			mrcpClient2Log(__FILE__, __LINE__, yPort, mod, 
					REPORT_NORMAL, MRCP_2_BASE, ERR, 
					"NULL %s in TTS request", "resultFifo");
		}
#endif
		if(incompleteInfo)
		{
			continue;	/* Incomplete information */
		}
		/*
		 * Check if the process that sent this request is still alive.
		 */

		done = 1;
	} /* while */

	if ( ( tmpRequest.speakMsgToDM.opcode == DMOP_START_TTS ) &&
	     ( gSrPort[yPort].isTTSRequestOnQueue(atoi(tmpRequest.string)) == false ) )
	{
		mrcpClient2Log(__FILE__, __LINE__, yPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Received DMOP_START_TTS(%d) for id (%s) which does not exist in the "
				"TTS queue.  Ignoring...", tmpRequest.speakMsgToDM.opcode,
				tmpRequest.string);
		struct MsgToApp                 response;
		memset((struct MsgToApp *)&response, '\0', sizeof(response));
		response.appCallNum  = atoi(tmpRequest.port);
		response.appPid      = atoi(tmpRequest.pid);
		response.appRef      = tmpRequest.speakMsgToDM.appRef;
		response.returnCode  = -1;
		sprintf(response.message,
                "Received DMOP_START_TTS(%d) for id (%s) which does not exist in the "
				"TTS queue.", tmpRequest.speakMsgToDM.opcode,
				tmpRequest.string);

		rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
								&response, sizeof(response)); 
		return(0);
	}

	if(tmpRequest.speakMsgToDM.opcode == DMOP_START_TTS)
	{
		totalTTSSpeakReceived++;
	}

	readerThreadSpeak(yPort, &tmpRequest);

#ifdef RECYCLE

	/*Client should exit if totalCalls processed > (RECYCLE_BASE + gClientId*RECYCLE_DELTA) 
		and all current calls are gone*/

    if( (yIntCanRecycle == true) &&
		(totalTTSSpeakReceived >= (RECYCLE_BASE + gClientId*RECYCLE_DELTA)))
	{
		int yIntActiveCallCount = 0;
		int i;

		for (i = 0; i < MAX_PORTS; i++)
		{
			if(gSrPort[i].IsStopProcessingTTS())
			{
				yIntActiveCallCount+=1;

				break;
			}
		}

		if(yIntActiveCallCount == 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, 0, mod,
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Finished processing (%d) calls, recycling mrcpClient2", totalTTSSpeakReceived);

			gCanProcessReaderThread = 0;
		}
	}
#endif

	return(0);
} // END: readAndProcessTtsRequest()

/*------------------------------------------------------------------------------
  static void  readerThreadSpeak()
   - initialize internal variables
   - initialize SRPort, requestQueue, grammarList for the application port
   - send response back to application
------------------------------------------------------------------------------*/
static void readerThreadSpeak(int zPort, 
			ARC_TTS_REQUEST_SINGLE_DM *zTtsRequestSingleDM)
{
	static char	mod[] = "readerThreadSpeak";
	ARC_TTS_RESULT		response;
	int					ttsTimeOut = 0;
	int					callNum;
	int					oldPid;
	int					rc;
	MrcpThread			mrcpThread; // create a mrcpThread instance
    struct timeval      t;
    long                milliseconds;
    long                lastDisconnectTime;


	callNum = atoi(zTtsRequestSingleDM->port);

	oldPid = gSrPort[callNum].getPid();

	if ( oldPid != -1 )
	{
		mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
            REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"oldPid=%d; tts request pid = %d.",
			oldPid, atoi(zTtsRequestSingleDM->pid));

		if ( ( oldPid != atoi(zTtsRequestSingleDM->pid) ) &&
			 ( zTtsRequestSingleDM->speakMsgToDM.opcode == DMOP_DISCONNECT ) )
		{

		    if ( atoi(zTtsRequestSingleDM->pid) == -1 )
			{
				sprintf(zTtsRequestSingleDM->pid, "%d",  oldPid);

				mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
					REPORT_DETAIL, MRCP_2_BASE, WARN,
	                "DMOP_DISCONNECT received for a port/pid -1.  Assuming disconnect for pid %d and will process it..",
					atoi(zTtsRequestSingleDM->pid));
			}
			else
			{
				mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
					REPORT_DETAIL, MRCP_2_BASE, WARN,
	                "DMOP_DISCONNECT received for a port/pid that is not active. "
					"oldPid=%d; tts request pid = %d. Ignoring.",
					oldPid, atoi(zTtsRequestSingleDM->pid));
				return;
			}
		}
	}
	
	if ( oldPid != atoi(zTtsRequestSingleDM->pid) )
	{
        lastDisconnectTime = gSrPort[callNum].getLastDisconnect();

        mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO, "lastDisconnectTime=%ld.", lastDisconnectTime);
        if ( lastDisconnectTime != -1 )
        {
            gettimeofday(&t, NULL); // get current time
            milliseconds = t.tv_sec*1000LL + t.tv_usec/1000; // caculate milliseconds

            if ( milliseconds - lastDisconnectTime < 300 )      // .3 seconds between disconnect and speak from new call
            {
				struct MsgToApp                 response;
                mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                    REPORT_DETAIL, MRCP_2_BASE, WARN,
                    "Speak request came in too soon after disconnect (0.3 seconds); associated with pid %d.  Ignoring.",
                    oldPid);

				memset((struct MsgToApp *)&response, '\0', sizeof(response));
				response.appCallNum  = atoi(zTtsRequestSingleDM->port);
				response.appPid      = atoi(zTtsRequestSingleDM->pid);
				response.appRef      = zTtsRequestSingleDM->speakMsgToDM.appRef;
				response.returnCode  = 0;
				sprintf(response.message, "%s", "Speak request had a timing issue.");

				rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
								&response, sizeof(response)); 
                return;
            }
            else
            {
                mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                        REPORT_VERBOSE, MRCP_2_BASE, INFO, "%ld - %ld = %ld since last disconnect.  OK.",
                        milliseconds, lastDisconnectTime, milliseconds - lastDisconnectTime);
            }
        }

		// -----------------------------------------------------
		initGlobalData(callNum);
	
		// Initialize SRPort  (partly)
		// ---------------------------
		gSrPort[callNum].setPid(atoi(zTtsRequestSingleDM->pid));
		gSrPort[callNum].setApplicationPort(callNum);
		gSrPort[callNum].setAppResponseFifo(zTtsRequestSingleDM->resultFifo);
		gSrPort[callNum].eraseCallMapByPort(callNum);
	
		// start mrcpThread
		// -----------------
		// yyq note: mrcpThread should not start before global data
		//            initialization 
		if ( mrcpThread.start(callNum) != 0 )
		{
			if ( zTtsRequestSingleDM->speakMsgToDM.opcode == DMOP_SPEAK )
			{
				struct MsgToApp                 response;

				mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
	                REPORT_DETAIL, MRCP_2_BASE, WARN,
	                "Unable to start mrcp Thread.");

				memset((struct MsgToApp *)&response, '\0', sizeof(response));
				response.appCallNum  = atoi(zTtsRequestSingleDM->port);
				response.appPid      = atoi(zTtsRequestSingleDM->pid);
				response.appRef      = zTtsRequestSingleDM->speakMsgToDM.appRef;
				response.returnCode  = -1;
				sprintf(response.message,
					"Unable to start MRCP thread. Cannot process TTS request.");
				rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
								&response, sizeof(response)); 
			}
			return;
		}
	
		usleep(10);
	}

	if ( zTtsRequestSingleDM->speakMsgToDM.opcode == DMOP_DISCONNECT )
	{
		gSrPort[callNum].setDisconnectReceived(true);
		if ( ! gSrPort[callNum].IsLicenseReserved() )
		{
			mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
				REPORT_DETAIL, MRCP_2_BASE, WARN,
                "DMOP_DISCONNECT received for a port/pid that does not have a "
				"reserved port.");
		}
		else
		{
			gSrPort[callNum].setStopProcessingTTS(true);
		}
	}
	mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Sending opcode %d to request queue for port %d.",
                zTtsRequestSingleDM->speakMsgToDM.opcode, callNum);

	rc = pthread_mutex_lock( &requestQueueLock[callNum]);
	if( 0 == rc)
	{
		ARC_TTS_REQUEST_SINGLE_DM		requestTtsDM;
		memcpy(&requestTtsDM, zTtsRequestSingleDM, sizeof(requestTtsDM));
		requestQueue[callNum].push(requestTtsDM);
		pthread_mutex_unlock( &requestQueueLock[callNum]);
	
	}
	else
	{
		mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"readerThread unable to acquire lock to requestQueue.  "
			"Cannot process request.");
	}

} // END: readerThreadSpeak()

/*******************************************************************************
openTtsRequestFifo():
*******************************************************************************/
static int openTtsRequestFifo()
{
	char	mod[]={"openTtsRequestFifo"};
	char    msg[512];
	int     rc;
	int     zPort = -1;

	sprintf(gTtsRequestFifo, "%s/%s.%d",
		gMrcpInit.getMrcpFifoDir().c_str(), TTS_FIFO, gClientId);

	if ( (mknod(gTtsRequestFifo, S_IFIFO | 0777, 0) < 0) && (errno != EEXIST))
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR, "Can't create fifo (%s). [%d, %s].",
			gTtsRequestFifo, errno, strerror(errno));
		return(-1);
	}

	if ( (gTtsRequestFifoFd = open(gTtsRequestFifo, O_RDWR|O_NONBLOCK)) < 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR,
			"Can't open request fifo (%s). [%d, %s].",
			gTtsRequestFifo, errno, strerror(errno));
		return(-1);
	}

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "Successfully open TTS request fifo (%s). fd=%d",
		gTtsRequestFifo, gTtsRequestFifoFd);
	
	return(0);

} // END: openTtsRequestFifo()


/*------------------------------------------------------------------------------
checkClearApp():
------------------------------------------------------------------------------*/
static void checkClearApp()
{
	static char					mod[]="checkClearApp";
	ARC_TTS_REQUEST_SINGLE_DM	requestTtsDM;
//	struct MsgToDM	msgToDM;

	int		i;
	char	procFile[256];
	int		pid;
	int		sendDisconnect;


	// TODO: a readThread is responsible for a range of ports
	//       instead of all ports.   It's going to mess up if all
	//       readerThreads run this cleanUp.  Modify this when we add
	//       multiple readerThreads.
	for (i = 0; i < MAX_PORTS; i++)
	{
		pid = gSrPort[i].getPid();
	
		if ( ( pid != -1 ) ||
		     ( gSrPort[i].getCloseSession() == 1 ) )
		{
			sendDisconnect = 0;
			sprintf(procFile, "/proc/%d", pid);
			if ( access(procFile, F_OK) != 0 )
			{
				sendDisconnect = 1;
				mrcpClient2Log(__FILE__, __LINE__, i, mod, REPORT_DETAIL,
					MRCP_2_BASE, WARN, 
					"WARNING: Application pid (%d) no longer exists.  "
					"Sending opcode %d to request queue for port %d.",
					pid, DMOP_DISCONNECT, i);
			}
			else if ( gSrPort[i].getCloseSession() == 1 )
			{
				mrcpClient2Log(__FILE__, __LINE__, i, mod, REPORT_DETAIL,
					MRCP_2_BASE, WARN, 
					"WARNING: Signal recieved to close session for pid %d.  "
					"Sending opcode %d to request queue for port %d.",
					pid, DMOP_DISCONNECT, i);
				sendDisconnect = 1;
			}
			if ( sendDisconnect )
			{
				requestTtsDM.speakMsgToDM.opcode = DMOP_DISCONNECT;
				sprintf(requestTtsDM.port, "%d", i);
				sprintf(requestTtsDM.pid, "%d", pid);

				pthread_mutex_lock( &requestQueueLock[i]);
				requestQueue[i].push(requestTtsDM);
				pthread_mutex_unlock( &requestQueueLock[i]);

				usleep(PIPE_READ_TIMEOUT * 10000);
				// Just in case the mrcpThread is gone, let's be
				// sure the elements are initialized, or this will
				// repeat indefinitely.
//				gSrPort[i].initializeElements();
//				gSrPort[i].destroyMutex();

				return;
			}
		}
	} // END: for(i=0 ...)
} // END: checkClearApp()


// initialize global data for an application port 
static void  initGlobalData(int port)
{

	int rc = -1;

	if ( ! gSrPort[port].getRequestQueueLockInitialized() )
	{
		pthread_mutexattr_t mutexAttr;
		pthread_mutexattr_init(&mutexAttr);
		pthread_mutexattr_settype (&mutexAttr, PTHREAD_MUTEX_ERRORCHECK);
	
		pthread_mutex_init( &requestQueueLock[port], &mutexAttr);
		gSrPort[port].setRequestQueueLockInitialized(true);
		mrcpClient2Log(__FILE__, __LINE__, port, "initGlobalData", REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Initialized requestQueueLock");
	}

	gSrPort[port].initializeElements();

//	grammarList[port].removeAll();

	while( !requestQueue[port].empty())
	{
		requestQueue[port].pop();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void checkUpdateMrcpRtpPorts()
{
    static char		mod[] = "checkUpdateMrcpRtpPorts";
    static time_t	last_time_modified = 0;
    struct stat		file_stat;

/* check if reload file exists if not create one */
    if (access (gMrcpTtsDetailsFile, R_OK) != 0)
    {
//		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_DETAIL, MRCP_2_BASE, WARN, 
//				"Unable to reload (%s). [%d, %s] No reload of mrcp rtp ports will be attempted.",
//				gMrcpTtsDetailsFile, errno, strerror(errno));
		return;
    }

    if (stat (gMrcpTtsDetailsFile, &file_stat) == 0)      /* check if last modified time is changed */
    {
        if (file_stat.st_mtime != last_time_modified)
        {
			reloadMrcpRtpPorts();
            last_time_modified = file_stat.st_mtime;
        }
    }                                             /* stat */

    return;
} // END: checkUpdateMrcpRtpPorts()
