/*------------------------------------------------------------------------------
Program Name:   readerThread.cpp
requests from the application fifo and take the approriate 
						action to have them processed.
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.
Update: 09/25/06 djb MR 2344. Added flag when DMOP_DISCONNECT is received.
Update: 09/04/14 djb MR 4368. Added sending back the SRExit
------------------------------------------------------------------------------*/
#include <pthread.h>
#include <signal.h>
#include <iostream>

#include "readerThread.hpp"
#include "mrcpThread.hpp"
#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"

#include <algorithm>

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include "unistd.h"
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <stdio.h>
	#include <errno.h>
	#include "UTL_accessories.h"
	#include "arcFifos.h"
}

extern bool isItVXI;
#ifdef SDM
extern int	gTecStreamSw[MAX_PORTS];
#endif

#define RECYCLE_BASE 	10000
//#define RECYCLE_BASE 	100000		// djb - testing only
#define RECYCLE_DELTA 	1000

int	gDefinedPortsByHalf	=	48;
int	gInitFailures		=	0;

extern int	gCanProcessReaderThread;
extern int	gCanProcessSipThread;
extern int	gCanProcessMainThread;

using namespace std;
using namespace mrcp2client;

static char     gRequestFifo[256];
static int      gRequestFifoFd;

/*DDN: Added Client Specific Request FIFO, which will be used in case of multiple clients*/
static char	gSpecialRequestFifo[256];
static int	gSpecialRequestFifoFd = -1;
extern int      gClientId;


void		*readerThreadFunc(void*);
int			readAndProcessAppRequest(int zInputFlag);

static int	openRequestFifo();
static int  openSpecialRequestFifo();
static int	processInit(struct MsgToDM *pMsgToDM);
static void	checkClearApp();
static void	initGlobalData(int xPort);
static void  getNumResources();
static void	killAllMrcpThreads();
// static int getAppResponseFifo(int zPort, char *resourceName);
static int sIsMrcpPortActive(int zPort, int zOpcode,
				struct MsgToDM *pMsgToDM);
static string sCreateAppResponseFifoName(int zPid);

static void readerThreadSigterm(int sig, siginfo_t *info, void *ctx)
{
    char mod[]="readerThreadSigterm";

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
	        REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"SIGTERM (%d) caught from pid (%ld), user (%ld).  Exiting.",
            sig, (long)info->si_pid, (long)info->si_uid);

#if 0
	pthread_detach(pthread_self());
	pthread_exit(NULL);
#endif

	gCanProcessReaderThread = 0;
	gCanProcessSipThread = 0;
	gCanProcessMainThread = 0;

    return;
}/*END: void readerThreadSigterm*/

static void sigpipe(int x)
{
    char mod[]="sigpipe";

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
	        REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"SIGPIPE caught - %d.  Ignoring.", x);

    return;
}/*END: int sigpipe*/

/*------------------------------------------------------------------------------ 
  class ReaderThread implementation
 *-----------------------------------------------------------------------------*/ 
void ReaderThread::start()
{
	static char		mod[] = "ReaderThread::start";
	int	   	rc = -1;
//	int	    *msg = new int();
	int	    msg;
	int		yPort = -1;
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

    pthread_attr_t threadAttr;
    pthread_attr_init (&threadAttr);
    pthread_attr_setdetachstate (&threadAttr, PTHREAD_CREATE_DETACHED);

	// rc = pthread_create(&readerThreadId, &threadAttr, readerThreadFunc, (void*)&msg);
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
void* readerThreadFunc (void* msg)
{
	static char	mod[]="readerThreadFunc";
	int		zPort = -1;
	int		rc;
	static int yCounter = 0;

	struct  sigaction   sig_act, sig_oact;

    /* set death of child function */
    sig_act.sa_handler = NULL;
    sig_act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
    if(sigaction(SIGCHLD, &sig_act, &sig_oact) != 0)
    {
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "sigaction(SIGCHLD, SA_NOCLDWAIT): system call failed. [%d, %s]",
            errno, strerror(errno));
		pthread_detach(pthread_self());
		pthread_exit(NULL);
    }

    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "signal(SIGCHLD, SIG_IGN): system call failed. [%d, %s]",
            errno, strerror(errno));
		pthread_detach(pthread_self());
		pthread_exit(NULL);
    }
    /*END: set death of child function */

	getNumResources();

	if ((rc = openRequestFifo()) != 0)
	{
		return (NULL);
	}

//#ifdef SDM
	/*DDN: Added to support multiple clients*/
	if ((rc = openSpecialRequestFifo()) != 0)
	{
		return (NULL);
	}
//#endif

	gCanProcessReaderThread = 1;

	while (gCanProcessReaderThread)
	{
		rc = readAndProcessAppRequest(MRCP_SR_REQ_FROM_CLIENT);
		if (rc != 0 )
		{
			usleep(400000);		// 400 MS / 0.4 seconds
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Rececived failure to read request. Sleeping 400 milliseconds.");
			continue;
//  				pthread_exit (NULL);
		}

		if ( access(gRequestFifo, F_OK) != 0 )       // MR 5116
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
						MRCP_2_BASE, WARN, "Fifo (%s) does not exist; re-creating it.", gRequestFifo);

			if ( gRequestFifoFd > 0 )
			{
				close(gRequestFifoFd);
				gRequestFifoFd = -1;
			}
			
			if ((rc = openRequestFifo()) != 0) // There was a problem opening the fifo.
			{                                     // Exit so this process will exit and get re-created
				gCanProcessReaderThread = 0;
			}
		}

		usleep(10);

		// clean up global data left by past applications
//		if ( yCounter % 10 == 0 )
//		{
			checkClearApp();
//			yCounter = 0;
//		}
//		yCounter++;
//		if (yCounter > 1000000) yCounter = 0; // reset to avoid int overflow

		// DJB - testing for debug - remove this for production
//		if (access("/tmp/stopIt", R_OK) == 0)
//		{
//			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
//		   		MRCP_2_BASE, ERR, "DJB: DING DING DING - found /tmp/stopIt");
//			break;
//		}
//
	} // END: while (1)

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL, 
		   MRCP_2_BASE, WARN, "ReaderThread exiting.");

	pthread_detach(pthread_self());
	pthread_exit (NULL);

}/*END: void* readerThreadFunc*/


//================================================
int readAndProcessAppRequest(int zInputFlag)
//================================================
{
	char			mod[]="readAndProcessAppRequest";
	char			procFile[128];
	int				rc;
	int             fdRegular = gRequestFifoFd;
	int             fdSpecial = gSpecialRequestFifoFd;
	int             fd = fdRegular;

	int				nEvents;
	int				callNum = -1;
	int				i,pid = -1;
	fd_set			readFdSet;
	struct MsgToDM	msgToDM;
	struct timeval	tval;
	static int 		totalSRInitsReceived = 0;
	static int 		totalSelectsAfterLimit = 0;
	static int		alreadyCalledMax = 0;

	struct   MsgToApp	response;


	bool			yIntCanRecycle = true;
	
	if ( zInputFlag != MRCP_SR_REQ_FROM_SRRECOGNIZE )
	{

		FD_ZERO (&readFdSet);

		/*Listen to main FIFO for first 10000 calls only.*/
		if(totalSRInitsReceived < (RECYCLE_BASE + gClientId*RECYCLE_DELTA))
		{
        	FD_SET(fdRegular,   &readFdSet);
		}

        FD_SET(fdSpecial,   &readFdSet);

		tval.tv_sec     = 0;
		// tval.tv_usec    = 100000;		// 0.1 seconds
		tval.tv_usec    = 400000;		// 0.4 seconds

		if(totalSRInitsReceived < (RECYCLE_BASE + gClientId*RECYCLE_DELTA))
		{
			if(fdRegular > fdSpecial)
			{
				fd = fdRegular;
			}
			else
			{
				fd = fdSpecial;
			}
		}
		else
		{
				fd = fdSpecial;
		}

		nEvents =  select (fd+1, &readFdSet, NULL, NULL, &tval);
		if ( nEvents < 0 )
		{
			if ( gCanProcessReaderThread )
			{
				mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
					REPORT_NORMAL, MRCP_2_BASE, ERR,
					"Select failed.  [%d, %s]", errno, strerror(errno));
			}
			else
			{
				mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
					REPORT_VERBOSE, MRCP_2_BASE, INFO,
					"Select failed.  [%d, %s]", errno, strerror(errno));
			}
			usleep(10000);
			return(-1);
		}	

		if ( nEvents == 0 )
		{
			if(totalSRInitsReceived >= (RECYCLE_BASE + gClientId*RECYCLE_DELTA))
			{
				totalSelectsAfterLimit++;

				if(totalSelectsAfterLimit < 10)
				{
					return(0);
				}

				int yIntActiveCallCount = 0;

				for (i = 0; i < MAX_PORTS; i++)
				{
					if(gSrPort[i].IsTelSRInitCalled())
					{
						yIntActiveCallCount+=1;

						break;
					}
				}

				if(yIntActiveCallCount == 0)
				{
					mrcpClient2Log(__FILE__, __LINE__, 0, mod,
						REPORT_NORMAL, MRCP_2_BASE, INFO,
						"Finished processing (%d) calls, recycling mrcpClient2", totalSRInitsReceived);

					gCanProcessReaderThread = 0;
					gCanProcessSipThread = 0;
					gCanProcessMainThread = 0;
				}
			}

			return(0);
		}
	}

  // Read the next API statement from the fifo 
  // ------------------------------------------

	if(totalSRInitsReceived < (RECYCLE_BASE + gClientId*RECYCLE_DELTA))
	{
		if(FD_ISSET(fdRegular, &readFdSet))
		{
			fd = fdRegular;
		}
		else
		{
			fd = fdSpecial;
		}
	}
	else
	{
			fd = fdSpecial;
	}

	memset((struct MsgToDM *)&msgToDM, '\0', sizeof(msgToDM));
	rc = read(fd, &msgToDM, sizeof(struct MsgToDM));
	if ( rc == -1)
	{
		if(errno == EAGAIN)
		{
//			usleep(10000);
			return(0);
		}

		mrcpClient2Log(__FILE__, __LINE__, callNum,
				mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Failed to read from (%s).  [%d, %s]",
				gRequestFifo, errno, strerror(errno));
		return(-1);
   	}
		
	callNum	= msgToDM.appCallNum;
	pid		= msgToDM.appPid;

	mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Received %d bytes. initCount(%d), capacity(%d), msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d}", rc,
			totalSRInitsReceived,
			(RECYCLE_BASE + gClientId*RECYCLE_DELTA),	
			msgToDM.opcode,
			msgToDM.appCallNum,
			msgToDM.appPid,
			msgToDM.appRef,
			msgToDM.appPassword);

	if ( ( msgToDM.appCallNum < 0 ) || ( msgToDM.appCallNum > MAX_PORTS ) )
	{
		mrcpClient2Log(__FILE__, __LINE__, callNum,
			mod, REPORT_NORMAL, MRCP_2_BASE, ERR, 
			"Invalid port received (%d). Must be between 0 and %d.  "
			"Unable to process.", msgToDM.appCallNum, MAX_PORTS);
		return(0);
	} 

	if ( zInputFlag == MRCP_SR_REQ_FROM_SRRECOGNIZE )
	{
		switch(msgToDM.opcode)
		{

			default:
				mrcpClient2Log(__FILE__, __LINE__, callNum, mod, REPORT_NORMAL,
						MRCP_2_BASE, ERR, "Invalid request (%d) from API.  "
						"Returning STOP_PROCESSING(%d).",
						msgToDM.opcode, MRCP_SR_REQ_STOP_PROCESSING);
				return(MRCP_SR_REQ_STOP_PROCESSING);
				break;
		}
		return(0);
	}

	switch(msgToDM.opcode)
	{
		case DMOP_SRPROMPTEND:
			gSrPort[callNum].setArePhrasesPlayingOrScheduled(false);
			mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
					REPORT_VERBOSE, MRCP_2_BASE, INFO, 
					"Received opCode DMOP_SRPROMPTEND(%d) request.  "
					"No longer playing phrases.", msgToDM.opcode);
			break;
		case DMOP_SRDTMF:
			mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
						REPORT_VERBOSE, MRCP_2_BASE, INFO, 
						"Received opCode DMOP_SRDTMF(%d) request.  Ignoring.",
						msgToDM.opcode);
			break;
		case DMOP_SRINIT: // initialize internal and global variables

			/*DDN: Added for multiple mrcpClients*/
			totalSRInitsReceived++;
			yIntCanRecycle = false;//avoid timing issue.

			if(totalSRInitsReceived >= (RECYCLE_BASE + gClientId*RECYCLE_DELTA))
			{

				mrcpClient2Log(__FILE__, __LINE__, 0, mod, 
					REPORT_DETAIL, MRCP_2_BASE, WARN,
					"SRInit count (%d) reached its limit, no new DMOP_SRINIT will be processed.", 
					totalSRInitsReceived);
			}

			if ((rc = processInit(&msgToDM)) == -1)
			{
				break;
			}
			usleep(20000);
		case DMOP_SRRECOGNIZE:
		case DMOP_SRLOADGRAMMAR:
		case DMOP_SRSETPARAMETER: 
		case DMOP_SRRESERVERESOURCE:
		case DMOP_SRRELEASERESOURCE:
		case DMOP_SRUNLOADGRAMMARS:
		case DMOP_SRGETPARAMETER:
		case DMOP_SRUNLOADGRAMMAR:
		case DMOP_SRGETRESULT:
		case DMOP_SRGETXMLRESULT:
		#ifdef SDM
		case DMOP_TEC_STREAM:
		#endif
			if (msgToDM.opcode == DMOP_SRRELEASERESOURCE)
			{
#ifndef SDM
				
				if ( gSrPort[callNum].IsDisconnectReceived() || 
					(! sIsMrcpPortActive(callNum, msgToDM.opcode, &msgToDM)) )
				{
					memset(( struct   MsgToApp *)& response, '\0', sizeof(response));
					memcpy(&response, &msgToDM, sizeof(response));	
					response.returnCode = 0;
					rc = writeResponseToApp(callNum, __FILE__, __LINE__,
						&response, sizeof(response));

					mrcpClient2Log(__FILE__, __LINE__, msgToDM.appCallNum, mod, 
						REPORT_VERBOSE, MRCP_2_BASE, INFO, 
						"Not sending opcode %d to request queue for port %d.",
						msgToDM.opcode, callNum);

					break;
				}
#endif

				gSrPort[callNum].setStopProcessingRecognition(true);
			}
			if ( ! sIsMrcpPortActive(callNum, msgToDM.opcode, &msgToDM) )
			{
//				mrcpClient2Log(__FILE__, __LINE__, msgToDM.appCallNum, mod, 
//					REPORT_VERBOSE, MRCP_2_BASE, INFO, 
//					"NOT Sending opcode %d to request queue for port %d.  "
//					"Port is not active.",
//					msgToDM.opcode, callNum);
				break;
			}

			mrcpClient2Log(__FILE__, __LINE__, msgToDM.appCallNum, mod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO, 
				"Sending opcode %d to request queue for port %d.",
				msgToDM.opcode, callNum);

			rc = pthread_mutex_lock( &requestQueueLock[callNum]);
			if( 0 == rc)
			{
				requestQueue[callNum].push(msgToDM);
				pthread_mutex_unlock( &requestQueueLock[callNum]);
			}
			else
			{
				mrcpClient2Log(__FILE__, __LINE__, msgToDM.appCallNum, mod, 
					REPORT_NORMAL, MRCP_2_BASE, ERR, 
					"readerThread unable to acquire lock to requestQueue. Mrcp not actvie."
					"Cannot process request.");
				pthread_mutex_unlock( &requestQueueLock[callNum]);
			}
			break;
		case DMOP_DISCONNECT:
		case DMOP_SREXIT:

			if(gSrPort[callNum].IsDisconnectReceived())
			{
				memset(( struct   MsgToApp *)& response, '\0', sizeof(response));
				memcpy(&response, &msgToDM, sizeof(response));	
				response.returnCode = -3;
				rc = writeResponseToApp(callNum, __FILE__, __LINE__,
						&response, sizeof(response));
	
				mrcpClient2Log(__FILE__, __LINE__, msgToDM.appCallNum, mod, 
					REPORT_VERBOSE, MRCP_2_BASE, INFO, 
					"Not sending opcode %d to request queue for port %d. Mrcp not active.",
					msgToDM.opcode, callNum);
				break;
			}
			else
			{
				if ( ! sIsMrcpPortActive(callNum, msgToDM.opcode, &msgToDM)) 
				{
					memset(( struct   MsgToApp *)& response, '\0', sizeof(response));
					memcpy(&response, &msgToDM, sizeof(response));	
					response.returnCode = 0;
					rc = writeResponseToApp(callNum, __FILE__, __LINE__,
						&response, sizeof(response));
	
					mrcpClient2Log(__FILE__, __LINE__, msgToDM.appCallNum, mod, 
						REPORT_VERBOSE, SR_BASE, INFO, 
						"Not sending opcode %d to request queue for port %d. Mrcp not active.",
						msgToDM.opcode, callNum);

					break;
				}
			}

			if (msgToDM.opcode == DMOP_DISCONNECT)
			{
				gSrPort[callNum].setDisconnectReceived(true);
                gSrPort[callNum].setCallActive(false);
			}
			gSrPort[callNum].setStopProcessingRecognition(true);
			gSrPort[callNum].setExitSentToMrcpThread(callNum, true);
			mrcpClient2Log(__FILE__, __LINE__, msgToDM.appCallNum, mod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO, 
				"Sending opcode %d to request queue for port %d.",
				msgToDM.opcode, callNum);

			rc = pthread_mutex_lock( &requestQueueLock[callNum]);
			if( 0 == rc)
			{
				requestQueue[callNum].push(msgToDM);
				pthread_mutex_unlock( &requestQueueLock[callNum]);
			}
			else
			{
				mrcpClient2Log(__FILE__, __LINE__, msgToDM.appCallNum, mod, 
					REPORT_NORMAL, MRCP_2_BASE, ERR, 
					"readerThread unable to acquire lock to requestQueue.  "
					"Cannot process request.");
			}
			break;
		default:
			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN, 
				"Invalid opcode (%d) received, ignoring.", msgToDM.opcode);
			break;
	}

	if ( gMrcpInit.IsTooManyFailures_shutErDown() )
	{
		if ( alreadyCalledMax == 0 )
		{
			totalSRInitsReceived = RECYCLE_BASE + gClientId*RECYCLE_DELTA + 10;
			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN, 
				"Too many reserve resource failures...");
			alreadyCalledMax = 1;
		}
	}
				
	/*DDN: Added to support multiple clients*/
	/*Client should exit if totalCalls processed > (RECYCLE_BASE + gClientId*RECYCLE_DELTA) and all current calls are gone*/

	if(	(yIntCanRecycle == true) && 
		(totalSRInitsReceived >= (RECYCLE_BASE + gClientId*RECYCLE_DELTA)))
	{
		int yIntActiveCallCount = 0;

		for (i = 0; i < MAX_PORTS; i++)
		{
			if(gSrPort[i].IsTelSRInitCalled())
			{
				yIntActiveCallCount+=1;

				break;
			}
		}

		if(yIntActiveCallCount == 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, 0, mod,
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Finished processing (%d) calls, recycling mrcpClient2", totalSRInitsReceived);

			gCanProcessReaderThread = 0;
			gCanProcessSipThread = 0;
			gCanProcessMainThread = 0;
		}
	}

	return(0);
} // END: readAndProcessAppRequest()


//==========================
static int openRequestFifo()
//==========================
{
	char	mod[]={"openRequestFifo"};
	char    msg[512];
	int     rc;
	int     zPort = -1;

	sprintf(gRequestFifo, "%s/%s",
		gMrcpInit.getMrcpFifoDir().c_str(), MRCP_TO_SR_CLIENT2);

	if ( (mknod(gRequestFifo, S_IFIFO | 0777, 0) < 0) && (errno != EEXIST))
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR, "Can't create fifo (%s). [%d, %s].", gRequestFifo,
			errno, strerror(errno));
		return(-1);
	}

	if ( (gRequestFifoFd = open(gRequestFifo, O_RDWR|O_NONBLOCK)) < 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR,
			"Can't open request fifo (%s). [%d, %s]",
			gRequestFifo, errno, strerror(errno));
		return(-1);
	}

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "Successfully open request fifo (%s). fd=%d",
		gRequestFifo, gRequestFifoFd);
	
	return(0);

} // END: openRequestFifo()

/*DDN: Added to support multiple clients*/
//==========================
static int openSpecialRequestFifo() 
//==========================
{
	char    mod[]={"openSpecialRequestFifo"};
	char    msg[512];
	int     rc;
	int     zPort = -1;

	sprintf(gSpecialRequestFifo, "%s/%s.%d",
		gMrcpInit.getMrcpFifoDir().c_str(), MRCP_TO_SR_CLIENT2, gClientId);

	if ( (mknod(gSpecialRequestFifo, S_IFIFO | 0777, 0) < 0) && (errno != EEXIST))
	{
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
            MRCP_2_BASE, ERR, "Can't create special fifo (%s). [%d, %s]", gSpecialRequestFifo,
            errno, strerror(errno));
        return(-1);
	}

	if ( (gSpecialRequestFifoFd = open(gSpecialRequestFifo, O_RDWR|O_NONBLOCK)) < 0)
	{
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
            MRCP_2_BASE, ERR,
            "Can't open special request fifo (%s). [%d, %s].",
            gSpecialRequestFifo, errno, strerror(errno));
        return(-1);
	}

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "Successfully open special request fifo (%s). fd=%d",
		gSpecialRequestFifo, gSpecialRequestFifoFd);

	return(0);

}// END: openSpecialRequestFifo()



/*------------------------------------------------------------------------------ 
  static void  processInit(struct MsgToDM *pMsgToDM)
 *-----------------------------------------------------------------------------*/ 
/* 
 *  - initialize internal variables
 *  - initialize SRPort, requestQueue, grammarList for the application port
 *  - send response back to application
 */
static int processInit(struct MsgToDM *pMsgToDM)
{
	char     				mod[]="processInit";
	struct   Msg_SRInit		msg;
	struct   MsgToApp		response;
	struct   MsgToDM		msgToDM;
	int						rc;
	string	 				sAppResponseFifo;
	int      				callNum = -1;
	MrcpThread				mrcpThread; // create a mrcpThread instance
	time_t currentTime;
	static char				tmpAppResponseFifo[128] = "";
	

	memset(( struct   MsgToApp *)& response, '\0', sizeof(response));
	msg = *(struct Msg_SRInit *)pMsgToDM;
	response.appCallNum = msg.appCallNum;
	response.opcode = msg.opcode;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.appPassword = msg.appPassword;
	response.returnCode = -1;

	callNum = msg.appCallNum;
#if 0
//	sprintf(lAppResponseFifo, "%s/%s.%d", gMrcpInit.getMrcpFifoDir().c_str(),
//				APPL_RESPONSE_FIFO, msg.appPid);
	sprintf(lAppResponseFifo, "%s/%s.%d", "/tmp",
				APPL_RESPONSE_FIFO, msg.appPid);
	sAppResponseFifo  = string(lAppResponseFifo);
#endif
//	sAppResponseFifo = sCreateAppResponseFifoName(msg.appPid);

	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO, "Processing "
		"{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,resourceFile:(%s),appName:(%s)}",
		msg.opcode, 	msg.appCallNum,
		msg.appPid, 	msg.appRef,
		msg.appPassword,msg.resourceNamesFile,
		msg.appName);

	gSrPort[callNum].getAppResponseFifo(sAppResponseFifo);		// BT-259

	sprintf(tmpAppResponseFifo, "%s", sAppResponseFifo.c_str()); 

	if ( (tmpAppResponseFifo[0] != '\0' ) &&						// BT-217
	     ( strcmp(msg.resourceNamesFile, tmpAppResponseFifo) != 0 ) &&
	     ( ! gSrPort[callNum].IsExitSentToMrcpThread(callNum) ) )
	{
		gSrPort[callNum].setExitSentToMrcpThread(callNum, true);
		gSrPort[callNum].setStopProcessingRecognition(true);

        memcpy(&msgToDM, pMsgToDM, sizeof(struct   MsgToApp));

		mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Applications don't match.  Sending DMOP_SREXIT to clean port.");
	
		msgToDM.opcode		= DMOP_SREXIT;
		msgToDM.appPid		= gSrPort[callNum].getPid();
	
		requestQueue[callNum].push(msgToDM);
		pthread_mutex_unlock( &requestQueueLock[callNum]);
		while ( gSrPort[callNum].getPid() != -1 ) 
		{
			usleep(5000); // sleep 0.2 seconds
		}
	}

	if ( gSrPort[callNum].IsTelSRInitCalled() )
	{

		time(&currentTime);

		if(currentTime - gSrPort[callNum].lastInitTime > 60)
		{
			mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO, 
				"Resetting SRInitCalled.");

			gSrPort[callNum].setTelSRInitCalled(false);	
		}
		else
		{
			
			sprintf(response.message, 
					"Error: port %d is already initialized for SR.  "
					"Returning failure.", callNum);

			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, REPORT_NORMAL,
					MRCP_2_BASE, ERR, "Port %d is already initialized for SR.  "
					"Returning failure.", callNum);

			rc = writeResponseToApp(callNum, __FILE__, __LINE__,
					&response, sizeof(response));
		
	#if 0	
			if ( gInitFailures > gDefinedPortsByHalf )
			{
				mrcpClient2Log(__FILE__, __LINE__, callNum, mod, REPORT_NORMAL, 
					MRCP_2_BASE, ERR, 
					"The number of ports to initialize failure (%d) has "
					"exceeded threshhold.  Must restart.  ReaderThread exiting.",
					gInitFailures);
				killAllMrcpThreads();

				sleep(2);

				pthread_detach(pthread_self());
				pthread_exit (NULL);
			}
			else
			{
				gInitFailures++;
			}
	#endif
			
			return(-1);
		}
	}

	gSrPort[callNum].lastInitTime = currentTime;

	// initialize global data structure and associate locks 
	// -----------------------------------------------------
	initGlobalData(callNum);

	// Initialize SRPort  (partly)
	// ---------------------------
	gSrPort[callNum].setPid(msg.appPid);
	gSrPort[callNum].setApplicationPort(callNum);

	gSrPort[callNum].setAppResponseFifo(msg.resourceNamesFile);
	gSrPort[callNum].getAppResponseFifo(sAppResponseFifo);
	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"App response fifo is (%s)", sAppResponseFifo.c_str());

// #ifdef SDM
// 	if( (isItVXI) && ( strcmp(msg.appName, "arcVXML2") == 0 ) )
// 	{
//     	rc = getAppResponseFifo(callNum, msg.resourceNamesFile);
// 		if ( rc == 0 )
// 		{
// 			mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
// 				REPORT_VERBOSE, MRCP_2_BASE, INFO, "App response fifo "
// 				"is set to (%s).",
// 				gSrPort[callNum].getAppResponseFifo().c_str());
// 		}
// 	}
// 	else
// 	{
// 		gSrPort[callNum].setAppResponseFifo(sAppResponseFifo);
// 		mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
// 			REPORT_VERBOSE, MRCP_2_BASE, INFO, "App response fifo is (%s)",
// 			sAppResponseFifo.c_str());
// 	}
// #else
//     rc = getAppResponseFifo(callNum, msg.resourceNamesFile);
// 	if ( rc == 0 )
// 	{
// 		mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
// 			REPORT_VERBOSE, MRCP_2_BASE, INFO, "App response fifo "
// 			"is set to (%s).",
// 			gSrPort[callNum].getAppResponseFifo().c_str());
// 	}
// 	else
// 	{
// 		gSrPort[callNum].setAppResponseFifo(sAppResponseFifo);
// 		mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
// 			REPORT_VERBOSE, MRCP_2_BASE, INFO, "App response fifo is (%s)",
// 			sAppResponseFifo.c_str());
// 	}
//#endif

	gSrPort[callNum].eraseCallMapByPort(callNum);

	if ( access(gSpecialRequestFifo, F_OK) != 0)		// BT-158
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_DETAIL,
			MRCP_2_BASE, WARN, 
			"SpecialRequestFifo (%s) does not exist. Attempting to re-create it.",
				gSpecialRequestFifo);
		if ((rc = openSpecialRequestFifo()) != 0)
		{
			return(-1);			// message is logged in routine
		}
	}

	// start mrcpThread
	// -----------------
	// yyq note: mrcpThread should not start before global data initialization 
	if ( mrcpThread.start(callNum) != 0 )
	{
		sprintf(response.message, 
				"Unable to start mrcpThread.  Returning failure");
		rc = writeResponseToApp(callNum, __FILE__, __LINE__,
				&response, sizeof(response));
		gSrPort[msg.appCallNum].initializeElements();
		return(-1);
	}


	return(0);

} // END: processInit()

static int sIsMrcpPortActive(int zPort, int zOpcode,
				struct MsgToDM *pMsgToDM)
{
	static char				mod[] = "sIsMrcpPortActive";
	struct   MsgToApp		response;
	string	 				sAppResponseFifo;
	int						rc;

	if ( zOpcode == DMOP_SRINIT )
	{
		return(1);
	}

	if ( gSrPort[zPort].IsTelSRInitCalled() )
	{
		return(1);
	}

	//  Now, handle the opcode where the mrcpThread is not yet been 
	//  initialized.
	switch (zOpcode)
	{
		case DMOP_DISCONNECT:
		case DMOP_SRPROMPTEND:		// ignore these; don't respond to app.
		case DMOP_SRDTMF:
		case DMOP_TEC_STREAM:
			return(0);
			break;
	}

//	sAppResponseFifo = sCreateAppResponseFifoName(pMsgToDM->appPid);

	gSrPort[zPort].getAppResponseFifo(sAppResponseFifo);
	if ( sAppResponseFifo.length() == 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_VERBOSE, MRCP_2_BASE, INFO, "App response fifo is empty. "
			"Assuming app is not active.");
		return(0);
	}
	
	if ( access(sAppResponseFifo.c_str(), F_OK) != 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_VERBOSE, MRCP_2_BASE, INFO, "App response fifo does not exist. "
			"Assuming app is not active.");
		return(0);
	}

	gSrPort[zPort].setPid(pMsgToDM->appPid);
	gSrPort[zPort].setApplicationPort(zPort);
//	gSrPort[zPort].setAppResponseFifo(sAppResponseFifo);

	memset(( struct   MsgToApp *)& response, '\0', sizeof(response));
	response.appCallNum = pMsgToDM->appCallNum;
	response.opcode = zOpcode;
	response.appPid = pMsgToDM->appPid;
	response.appRef = pMsgToDM->appRef;
	response.appPassword = pMsgToDM->appPassword;
	response.returnCode = -1;
	sprintf(response.message, 
				"Error: port %d is not initialized.  Must call SRInit first.",
				zPort);

	switch(zOpcode)
	{
		case DMOP_SRRELEASERESOURCE:
		case DMOP_SREXIT:
			if ( gSrPort[zPort].IsDisconnectReceived() )
			{
				memset((char *)response.message,'\0', sizeof(response.message));
				response.returnCode = 0;
			}
		break;
	}
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, "Port %d is NOT initialized for SR.  "
				"Returning %d for opcode %d.",
				zPort, response.returnCode, response.opcode);
	rc = writeResponseToApp(zPort, __FILE__, __LINE__,
				&response, sizeof(response));
	gSrPort[zPort].closeAppResponseFifo(__FILE__, __LINE__, zPort);
	gSrPort[zPort].initializeElements();
	return(0);
	
#if 0
	{
		case DMOP_SRRECOGNIZE:
		case DMOP_SRLOADGRAMMAR:
		case DMOP_SRSETPARAMETER: 
		case DMOP_SRRESERVERESOURCE:
		case DMOP_SRRELEASERESOURCE:
		case DMOP_SRUNLOADGRAMMARS:
		case DMOP_SRGETPARAMETER:
		case DMOP_SRUNLOADGRAMMAR:
		case DMOP_SRGETRESULT:
		case DMOP_SRGETXMLRESULT:
		case DMOP_SREXIT:
#endif
	return(0);
}

/*------------------------------------------------------------------------------
checkClearApp():
------------------------------------------------------------------------------*/
static void checkClearApp()
{
	static char		mod[]="checkClearApp";
	struct MsgToDM	msgToDM;

	int		i;
	char	procFile[256];
	int		pid;
	string	tmpAppRespFifo;
	char	charAppRespFifo[128];


	// TODO: a readThread is responsible for a range of ports
	//       instead of all ports.   It's going to mess up if all
	//       readerThreads run this cleanUp.  Modify this when we add
	//       multiple readerThreads.
	for (i = 0; i < MAX_PORTS; i++)						// BT-217
	{
		if ( ! gSrPort[i].IsTelSRInitCalled() ) 
		{
			continue;
		}
		gSrPort[i].getAppResponseFifo(tmpAppRespFifo);			 //  BT-259

		if ( tmpAppRespFifo.length() == 0 )
		{
			continue;
		}

		sprintf(charAppRespFifo, "%s", tmpAppRespFifo.c_str());
		if ( access(charAppRespFifo, F_OK) == 0)
		{
			continue;
		}
		if ( gSrPort[i].IsExitSentToMrcpThread(i) )
		{
			continue;
		}

		gSrPort[i].setExitSentToMrcpThread(i, true);
		gSrPort[i].setStopProcessingRecognition(true);

		msgToDM.opcode		= DMOP_SREXIT;
		msgToDM.appCallNum	= i;
		msgToDM.appPid		= pid;

		mrcpClient2Log(__FILE__, __LINE__, i, mod, REPORT_DETAIL,
			MRCP_2_BASE, WARN, 
			"Application response FIFO (%s) no longer exists. "
			"Sending DMOP_SREXIT to clear port.", charAppRespFifo);

		pthread_mutex_lock( &requestQueueLock[i]);
		requestQueue[i].push(msgToDM);
		pthread_mutex_unlock( &requestQueueLock[i]);

#if 0
		pid = gSrPort[i].getPid();
		if ( pid != -1 )
		{
			sprintf(procFile, "/proc/%d", pid);
			if ( access(procFile, F_OK) != 0)
			{
				if ( gSrPort[i].IsExitSentToMrcpThread(i) )
				{
					continue;
				}
				gSrPort[i].setExitSentToMrcpThread(i, true);
				gSrPort[i].setStopProcessingRecognition(true);

				msgToDM.opcode		= DMOP_SREXIT;
				msgToDM.appCallNum	= i;
				msgToDM.appPid		= pid;

				mrcpClient2Log(__FILE__, __LINE__, i, mod, REPORT_DETAIL,
					MRCP_2_BASE, WARN, 
					"Application pid (%d) no longer exists.  "
					"Sending opcode %d to request queue for port %d.",
					pid, msgToDM.opcode, i);

				pthread_mutex_lock( &requestQueueLock[i]);
				requestQueue[i].push(msgToDM);
				pthread_mutex_unlock( &requestQueueLock[i]);
				// Just in case the mrcpThread is gone, let's be
				// sure the elements are initialized, or this will
				// repeat indefinitely.
//				gSrPort[i].initializeElements();
				//gSrPort[i].destroyMutex();	DDN 06162011
			}
		}
#endif
	} // END: for(i=0 ...)
} // END: checkClearApp()

/*------------------------------------------------------------------------------
killAllMrcpThreads():
------------------------------------------------------------------------------*/
static void killAllMrcpThreads()
{
	static char		mod[]="killAllMrcpThreads";
	struct MsgToDM	msgToDM;
	pthread_t		pt;

	int		i;
	int		rc;
	char	procFile[256];
	int		pid;

	for (i = 0; i < MAX_PORTS; i++)
	{
	
		if ( (pt = gSrPort[i].getMrcpThreadId()) == 0)
		{
			continue;
		}

		mrcpClient2Log(__FILE__, __LINE__, i, mod, REPORT_VERBOSE,
					MRCP_2_BASE, INFO, 
					"Sending SIGTERM to port %d, threadId %ld.",
					i, pt);

		rc = pthread_kill(pt, SIGTERM);

		mrcpClient2Log(__FILE__, __LINE__, i, mod, REPORT_VERBOSE,
					MRCP_2_BASE, INFO, 
					"%d = pthread_kill(%d, SIGTERM)", rc, pt);

	} // END: for(i=0 ...)

} // END: killAllMrcpThreads()


// initialize global data for an application port 
static void  initGlobalData(int port)
{

	int rc = -1;

	pthread_mutexattr_t mutexAttr;
	pthread_mutexattr_init(&mutexAttr);
	pthread_mutexattr_settype (&mutexAttr, PTHREAD_MUTEX_ERRORCHECK);

	pthread_mutex_init( &requestQueueLock[port], &mutexAttr);
	pthread_mutex_init( &requestQueueLock[port], &mutexAttr);

	gSrPort[port].initializeElements();

	grammarList[port].removeAll();

	while( !requestQueue[port].empty()) {
		requestQueue[port].pop();
	}
}

/*-------------------------------------------------------------------
getNumResources():
--------------------------------------------------------------------*/
static void  getNumResources()
{
    static  char    mod[]="getNumResources";
    char    record[256];
    FILE    *fp;
    int     i;
    char    *p;
	char	*base_path;
	char	resourceDefTab[256];
	char	isp_base[256];

	/* set ISPBASE path */
	if((base_path=(char *)getenv("ISPBASE")) == NULL)
	{
		mrcpClient2Log(__FILE__, __LINE__, -1,  mod, 
			REPORT_DETAIL, MRCP_2_BASE, WARN,
	    	"Environment variable ISPBASE is not found/set.  Defaulting to "	
			"/home/arc/.ISP");
		sprintf(isp_base, "%s", "/home/arc/.ISP");
	}
	else
	{
		sprintf(isp_base, "%s", base_path);
	}
	sprintf(resourceDefTab, "%s/Telecom/Tables/ResourceDefTab", isp_base);
	
    i = 0;
    if((fp=fopen(resourceDefTab, "r")) == NULL)
    {
		mrcpClient2Log(__FILE__, __LINE__, -1,  mod, 
			REPORT_NORMAL, MRCP_2_BASE, WARN,
        	"Can't open file %s. [%d, %s]  Setting defined ports to %d.",
            resourceDefTab, errno, strerror(errno), gDefinedPortsByHalf);
		return;
    }

    while( fgets(record, sizeof(record), fp) != NULL)
    {
        if( (p = strstr(record, "DCHANNEL")) != (char *)NULL)
        {
            continue;
        }
        i++;
    }

    (void) fclose(fp);
	gDefinedPortsByHalf = i / 2;
	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Number of defined ports is set to %d; setting threshold to %d.",
			 i, gDefinedPortsByHalf);
    return;

} // END: getNumResources()

#if 0
static int getAppResponseFifo(int zPort, char *resourceName)
{
	char mod[] = "getAppResponseFifo";
    FILE *fp = NULL;
    char resourceContent[512];
    int rc;
    char lFifoName[32] = "";
    char *lfifoStartPt = NULL;
	struct stat yStat;
	int len = 0;

    memset(resourceContent, 0, 512);
    memset(lFifoName, 0, 32);

    if(resourceName == NULL && resourceName[0] == '\0' )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"resourceName is empty; unable to create app response "
			"fifo name.");
        return -1;
    }
	if( (rc = stat(resourceName, &yStat)) < 0)
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"stat() failed for resourceName name file (%s). "
			"[%d, %s] Unable to create app response  fifo name. ",
			resourceName, errno, strerror(errno));
        return(-1);
    }
	
	len = yStat.st_size;	
    
	fp = fopen(resourceName,"r");

    if(fp == NULL)
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_DETAIL, MRCP_2_BASE, WARN,
			"Unable to open resourceName file (%s).  [%d, %s] "
			"Unable to create app response fifo name.",
			resourceName, errno, strerror(errno));
        return -1;
    }

    rc = fread(resourceContent, len, 1, fp);

    lfifoStartPt = strstr(resourceContent, "FIFO");
	if(lfifoStartPt == NULL)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Failed to find FIFO in resourceName file (%s). "
			"Unable to create app response fifo name.",
			resourceName);

		fclose(fp);
		return -1;
	}
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"resourceFile=%s, app response fifo=%s len=%d, fifoLen=%d",
			 resourceName, lfifoStartPt, len, strlen(lfifoStartPt));
	

    if(lfifoStartPt != NULL && len > 6)
    {
        memcpy(lFifoName, lfifoStartPt + 5,strlen(lfifoStartPt)-5 );
        gSrPort[zPort].setAppResponseFifo(string(lFifoName));
		fclose(fp);
		return(0);
    }
	else
	{
		fclose(fp);
		return(-1);
	}
}
#endif

static string sCreateAppResponseFifoName(int zPid)
{
	char	 				lAppResponseFifo[256];

//	sprintf(lAppResponseFifo, "%s/%s.%d", gMrcpInit.getMrcpFifoDir().c_str(),
//				APPL_RESPONSE_FIFO, zPid);
	sprintf(lAppResponseFifo, "%s/%s.%d", "/tmp",
				APPL_RESPONSE_FIFO, zPid);

	return(string(lAppResponseFifo));

} // END: sCreateAppResponseFifoName()
