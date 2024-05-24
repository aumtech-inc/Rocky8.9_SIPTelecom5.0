/*------------------------------------------------------------------------------
yPort Name:   mrcpThread.hpp

Purpose     :   mrcpThread for the mrcp 2.0 client
Author      :   Aumtech, Inc.
Update      :   06/12/06 yyq Created the file.
Update  : 01/29/15 djb MR 4512

Description:
MrcpThread is created by sipThread when it receives a 200 OK for INVITE request.
It fetches requests from a global requestQueue for a specific application port,
converts to Mrcp2.0 request message, builds TCP connection and sends to server,
waits for response, parses responses and writes responses back to application.
------------------------------------------------------------------------------*/

#include "mrcpThread.hpp"
#ifdef SDM
#include "parseResult.hpp"
#endif

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <sys/syscall.h>
	#include <errno.h>
	#include "UTL_accessories.h"
	#include "arcFifos.h"
	#include "ttsStruct.h"
}

#ifdef SDM
int						gTecStreamSw[MAX_PORTS];
extern	XmlResultInfo	parsedResults[];
struct	MsgToApp		gRecogResponse;
#endif

extern int	gCanProcessReaderThread;
extern int	gCanProcessSipThread;
extern int	gCanProcessMainThread;

static int savePorts[MAX_PORTS+1];

static pthread_mutex_t gMutexPortLock=PTHREAD_MUTEX_INITIALIZER;   // MR 4831

static ARC_TTS_REQUEST_SINGLE_DM savedReservedTtsDM;

void processGetParams(int zPort, char *zParamName, char *zParamValue);
void	sStopThisThread(int zPort);
int openChannelToDynMgr(int zPort, int zDmId);
int justReturnFailure(ARC_TTS_REQUEST_SINGLE_DM *pRequestTtsDM);

/*----------------------------------------------------------------------------
 *---------------------------------------------------------------------------*/
void* mrcpThreadFunc(void *zAppPort)
{
	char			mod[] = "mrcpThreadFunc";
	int				rc;
	int				pid;
	struct MsgToDM	msgToDM;
	ARC_TTS_REQUEST_SINGLE_DM	ttsRequestDM;
//	struct MrcpPortInfo mPortInfo;
	int				callNum = -1;
	ARC_TTS_RESULT	ttsResult;
	ARC_TTS_REQUEST_SINGLE_DM   *pParams;
	int				disconnectReceived = 0;
	time_t			mrcpThreadStartTime;
	time_t			mrcpMessagesStartTime;

	time_t			lastConnectorKeepALiveTime = 0;
	time_t			currentConnectorKeepALiveTime = 0;

	char			getParamName[128]="";
	char			getParamValue[128]="";

	void* void1;

    pthread_t       tmpGetThreadId;
    pthread_t       savedThreadId;

	time(&mrcpThreadStartTime);
	callNum = *(int *)zAppPort;

	memset((ARC_TTS_REQUEST_SINGLE_DM *) &savedReservedTtsDM, '\0', 
			sizeof(ARC_TTS_REQUEST_SINGLE_DM));

	// process request queue
	//--------------------------
	while (gCanProcessReaderThread)
	{
		// fetch a request from requestQueue 
		// ---------------------------------
		bool gotNewRequest = false;
				
		rc = pthread_mutex_trylock( &requestQueueLock[callNum]);
		if ( 0 == rc)
		{
			if( ! requestQueue[callNum].empty() ) 
			{
				ARC_TTS_REQUEST_SINGLE_DM tempTtsRequestDM;
				
				tempTtsRequestDM = requestQueue[callNum].front();
				memcpy(&ttsRequestDM, &tempTtsRequestDM, sizeof(tempTtsRequestDM));
				requestQueue[callNum].pop();
				gotNewRequest = true;
			
				time(&mrcpThreadStartTime);
			}

			pthread_mutex_unlock( &requestQueueLock[callNum]);
		}
		else if ( EBUSY == rc) 
		{
			usleep(10000);
			continue;
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
        		REPORT_NORMAL, MRCP_2_BASE, ERR, 
				"Unable to acquire the lock to access requestqueue. Shutting down thread.");
			sStopThisThread(callNum);
		}

		if(gotNewRequest == false) 
		{
			time(&mrcpMessagesStartTime);

            tmpGetThreadId = gSrPort[callNum].getMrcpThreadId(__FILE__, __LINE__);
            savedThreadId = gSrPort[callNum].getSavedMrcpThreadId(__FILE__, __LINE__);
            if ( ( savedThreadId != 0 ) &&
                 ( tmpGetThreadId != 0 ) &&
			     ( ! pthread_equal(savedThreadId, tmpGetThreadId ) ))
            {   
                mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                    REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "MRCP Thread anomaly; getMrcpThreadId()=%lu savedThreadId=%lu. Shutting down thread.",
                    tmpGetThreadId, savedThreadId);

//				rc = justReturnFailure(&ttsRequestDM); // there is no request to return failure to.

				sStopThisThread(callNum);
            }

			if(mrcpMessagesStartTime - mrcpThreadStartTime > 300)
			{

	 			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			        REPORT_DETAIL, MRCP_2_BASE, WARN,
					"No MRCP activity for last 300 seconds. Stopping TTS MRCP Session.");
				gSrPort[callNum].setStopProcessingTTS(true);
				gSrPort[callNum].clearTTSRequests();
				break;
			}
			usleep(10000); // minimize busy checking

			time(&currentConnectorKeepALiveTime);
			if ( lastConnectorKeepALiveTime != 0 )
			{
				if(currentConnectorKeepALiveTime - lastConnectorKeepALiveTime > 15)
				{
					sprintf(getParamName, "%s", "Voice-gender");
					mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
				        REPORT_VERBOSE, MRCP_2_BASE, INFO,
						"Sending keep-a-live to backend connector.");
					processGetParams(callNum, getParamName, getParamValue);
					time(&lastConnectorKeepALiveTime);
				}
			}

			continue;
		}

		time(&lastConnectorKeepALiveTime);

		mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
	        REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"mrcpThread received request %d", ttsRequestDM.speakMsgToDM.opcode);

		if ( ttsRequestDM.speakMsgToDM.opcode == DMOP_DISCONNECT )
		{
			if ( disconnectReceived )
			{
 				mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
					REPORT_VERBOSE, MRCP_2_BASE, INFO,
					"Received duplicate DMOP_DISCONNECT.");
			}

			disconnectReceived = 1;
			gSrPort[callNum].clearTTSRequests();
			usleep(1000);
	 		mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			       		 REPORT_VERBOSE, MRCP_2_BASE, INFO,
						"Calling processTtsReleaseResource");
			processTtsReleaseResource(callNum, &ttsRequestDM);
			sStopThisThread(callNum);
			lastConnectorKeepALiveTime = 0;
			continue;
		}

		switch(ttsRequestDM.speakMsgToDM.opcode)
		{
			case DMOP_HALT_TTS_BACKEND:
	 			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			        REPORT_VERBOSE, MRCP_2_BASE, INFO,
					"Received DMOP_HALT_TTS_BACKEND (%d).",
					DMOP_HALT_TTS_BACKEND);
				gSrPort[callNum].setStopProcessingTTS(true);
				gSrPort[callNum].clearTTSRequests();
				lastConnectorKeepALiveTime = 0;
				break;

			case DMOP_SPEAK:
				if ( ttsRequestDM.async == CLEAR_QUEUE_ALL ) 
				{
		 			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			       		 REPORT_VERBOSE, MRCP_2_BASE, INFO,
						"Clearing the entire queue.");
					gSrPort[callNum].clearTTSRequests();
					continue;
				}

				if ( (rc = processTtsReserveResource(&ttsRequestDM) ) != 0 )
				{
		 			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			       		 REPORT_VERBOSE, MRCP_2_BASE, INFO,
						"Calling sStopThisThread");
					sStopThisThread(callNum);
				}
				processSpeakTTS(&ttsRequestDM);
				time(&lastConnectorKeepALiveTime);
				break;

			case DMOP_START_TTS:
				processSpeakTTS(&ttsRequestDM);
				time(&lastConnectorKeepALiveTime);
				break;

			default:
	 			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			        REPORT_NORMAL, MRCP_2_BASE, ERR,
					"Invalid request received (%d), unable to process.",
                    ttsRequestDM.speakMsgToDM.opcode);
				continue;
	
		} // END: switch()

	} // END: for(;;)


	// thread return
	mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
        REPORT_VERBOSE, MRCP_2_BASE, INFO, "Exiting...");

	sStopThisThread(callNum);

	return void1;
} // END: mrcpThreadFunc

/*------------------------------------------------------------------------------
justReturnFailure():
		send Sip INVITE message with SDP and write response to application
------------------------------------------------------------------------------*/
int justReturnFailure(ARC_TTS_REQUEST_SINGLE_DM *pRequestTtsDM)
{
	static char						mod[]="justReturnFailure";
	struct Msg_Speak				msg; 
	struct MsgToApp					response;
	int								rc=-1;
	int								callNum;

	memset((struct MsgToApp *)&response, '\0', sizeof(response));

  	memcpy(&msg, &pRequestTtsDM->speakMsgToDM, sizeof(msg));
	callNum = msg.appCallNum;
	gSrPort[msg.appCallNum].setOpCode(msg.opcode);

	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "Processing {op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
		msg.opcode,   msg.appCallNum,
		msg.appPid,   msg.appRef,
		msg.appPassword);

	response.opcode				= pRequestTtsDM->speakMsgToDM.opcode;
	response.appCallNum			= atoi(pRequestTtsDM->port);
	response.appPid				= atoi(pRequestTtsDM->pid);
	response.appRef				= pRequestTtsDM->speakMsgToDM.appRef;
	response.returnCode 		= -1;
	sprintf(response.message,   "%s", "Internal Failure.");

	rc = writeResponseToApp(msg.appCallNum, __FILE__, __LINE__,
				&response, sizeof(response));
	return(0);
} // END: justReturnFailure()

/*------------------------------------------------------------------------------
processTtsReserveResource():
		send Sip INVITE message with SDP and write response to application
------------------------------------------------------------------------------*/
int processTtsReserveResource(ARC_TTS_REQUEST_SINGLE_DM *pRequestTtsDM)
{
	static char						mod[]="processTtsReserveResource";
	struct Msg_Speak				msg; 
	struct MsgToApp					response;
//	ARC_TTS_RESULT					ttsResult;
	int								rc=-1;
	int								callNum;

//	memset((ARC_TTS_RESULT *)&ttsResult, '\0', sizeof(ttsResult));
	memset((struct MsgToApp *)&response, '\0', sizeof(response));

  	memcpy(&msg, &pRequestTtsDM->speakMsgToDM, sizeof(msg));
	callNum = msg.appCallNum;
	gSrPort[msg.appCallNum].setOpCode(msg.opcode);

	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "Processing {op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
		msg.opcode,   msg.appCallNum,
		msg.appPid,   msg.appRef,
		msg.appPassword);

    if ( gSrPort[callNum].IsLicenseReserved() )
    {
//	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, REPORT_VERBOSE,
//		MRCP_2_BASE, INFO, "DJB: license already reserved");
        return(0);      // Already have a license.  Just return success.
    }

	rc = ttsGetResource(callNum);

	if(rc == 0)
	{
        memcpy(&savedReservedTtsDM, pRequestTtsDM,
						sizeof(ARC_TTS_REQUEST_SINGLE_DM));
		return(rc);
	}

	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR, "Failed to reserve resource.");

	response.opcode				= pRequestTtsDM->speakMsgToDM.opcode;
	response.appCallNum			= atoi(pRequestTtsDM->port);
	response.appPid				= atoi(pRequestTtsDM->pid);
	response.appRef				= pRequestTtsDM->speakMsgToDM.appRef;
	response.returnCode 		= rc;
	sprintf(response.message, "Unable to reserve TTS resource.");

	rc = writeResponseToApp(msg.appCallNum, __FILE__, __LINE__,
				&response, sizeof(response));
	return(-1);
} // END: processTtsReserveResource()

//=================================================
int processSpeakTTS(ARC_TTS_REQUEST_SINGLE_DM *pRequestTtsDM)
//=================================================
{
	static char 		mod[]="processSpeakTTS";
	int					rc;
	struct MsgToApp 	response;
	int					lPort;
	int					dmId;

	//dmId = pRequestTtsDM->speakMsgToDM.appPassword / 48;
	lPort = atoi(pRequestTtsDM->port);
	dmId = lPort / 48;

	response.appCallNum			= lPort;
	response.appPid				= atoi(pRequestTtsDM->pid);
	response.returnCode 		= -1;
	sprintf(response.message, "%s", pRequestTtsDM->fileName);
	lPort = atoi(pRequestTtsDM->port);

	if ( gSrPort[lPort].IsFdToDynmgrOpen() == false )
	{ 
		if ((rc = openChannelToDynMgr(lPort, dmId)) != 0 )
		{
			rc = writeResponseToApp(lPort, __FILE__, __LINE__,
				&response, sizeof(response));
			return(-1);
		}
	}

	mrcpClient2Log(__FILE__, __LINE__, atoi(pRequestTtsDM->port), mod,
        REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Processing SpeakTTS; op:%d,sync=%d,fn:%s,type=%s,port=%s,"
		"pid=%s,app=%s,"
		"string=%.*s,language=%s,gender=%s,voicename=%s,compression=%s,timeout=%s,"
		"resultFifo=%s.",
		pRequestTtsDM->speakMsgToDM.opcode,
		pRequestTtsDM->async,
		pRequestTtsDM->fileName,
		pRequestTtsDM->type,
		pRequestTtsDM->port,
		pRequestTtsDM->pid,
		pRequestTtsDM->app,
		30, pRequestTtsDM->string,
		pRequestTtsDM->language,
		pRequestTtsDM->gender,
		pRequestTtsDM->voiceName,
		pRequestTtsDM->compression,
		pRequestTtsDM->timeOut,
		pRequestTtsDM->resultFifo);

	lPort = atoi(pRequestTtsDM->port);
	rc = TTSSpeak(lPort, pRequestTtsDM);
	response.returnCode = rc;
	mrcpClient2Log(__FILE__, __LINE__, atoi(pRequestTtsDM->port), mod,
        REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"%d = TTSSpeak(%d)", rc, lPort);

    if ( rc == -1 )     // rc of -2 will bypass response to app, mediamgr will do it.
    {
        rc = writeResponseToApp(lPort, __FILE__, __LINE__,
                &response, sizeof(response));
    }

	return(0);

} // END: processSpeakTTS()

//=================================================
void processGetParams(int zPort, char *zParamName, char *zParamValue)
//=================================================
{
	static char 		mod[]="processGetParams";
	int					rc;
	struct MsgToApp 	response;
	int					lPort;
	int					dmId;

	// djb: 3-27-16  - this is currently just used as a keep-a-live with the connector
	*zParamValue = '\0';
	rc = TTSGetParams(zPort, zParamName, zParamValue);

//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"%d = TTSGetParams(%s, %s)", rc, zParamName, zParamValue);

	return;

} // END: processGetParams()

//======================================================
int processTtsReleaseResource(int zPort, 
		ARC_TTS_REQUEST_SINGLE_DM *pRequestTtsDM)
//======================================================
{
    char mod[] = "processTtsReleaseResource";
    struct Msg_SRReleaseResource	msg;
    struct MsgToApp					response;
	ARC_TTS_REQUEST_SINGLE_DM		*pParams;
    int		callNum	= -1;
    int		rc		= -1;
	long	bogus;

    //msg = *(struct Msg_SRReleaseResource *)pAppRequest;
    callNum = atoi(pRequestTtsDM->port);

//  	mrcpClient2Log(__FILE__, __LINE__, callNum, mod, REPORT_VERBOSE, MRCP_2_BASE,
//			INFO, "Calling ttsReleaseResource(%d)", callNum);

	rc = ttsReleaseResource(callNum);

	return(rc);
} // END: processTtsReleaseResource()

/*------------------------------------------------------------------------------
sendSpeechDetected():
------------------------------------------------------------------------------*/
void sendSpeechDetected(int zPort)
{
    char mod[] = "sendSpeechDetected";
    //struct timeb tb;
    struct timespec tb;

    struct  Msg_SpeechDetected msg;
    struct MsgToDM lMsgToDM;

#ifdef SDM
	return;
#endif
    //ftime(&tb);
	clock_gettime(CLOCK_REALTIME, &tb);

    memset(&lMsgToDM, 0, sizeof(struct MsgToDM));
    memset(&msg, 0, sizeof(struct  Msg_SpeechDetected));

    msg.opcode = DMOP_SPEECHDETECTED;
    msg.appCallNum = zPort;
    msg.appPid = gLast_SRRecognize[zPort].appPid;
    msg.appRef = gLast_SRRecognize[zPort].appRef;
    msg.appPassword = gLast_SRRecognize[zPort].appPassword;
    //msg.milliSecs = ( tb.time + ((double)tb.millitm)/1000 ) * 1000;
    msg.milliSecs = ( tb.tv_sec + ((double)tb.tv_nsec)/1000 ) * 1000;

    memcpy(&lMsgToDM, &msg, sizeof(struct Msg_SpeechDetected));
    sendRequestToDynMgr(zPort, mod, &lMsgToDM);

    return;

} // END: sendSpeechDetected()

/*------------------------------------------------------------------------------
stopBargeInPhrase():
------------------------------------------------------------------------------*/
int stopBargeInPhrase(int zPort)
{
    char mod[] = "stopBargeInPhrase";
    struct Msg_PortOperation msg;
    struct MsgToDM lMsgToDM;

    memset(&lMsgToDM, 0, sizeof(struct MsgToDM));
    memset(&msg, 0, sizeof(struct Msg_PortOperation));

    msg.opcode = DMOP_PORTOPERATION;
    msg.appCallNum = zPort;
    msg.appPid = gLast_SRRecognize[zPort].appPid;
    msg.appRef = gLast_SRRecognize[zPort].appRef;
    msg.appPassword = gLast_SRRecognize[zPort].appPassword;
    msg.operation = STOP_ACTIVITY;
    msg.callNum = zPort;

    memcpy(&lMsgToDM, &msg, sizeof(struct Msg_PortOperation));

    sendRequestToDynMgr(zPort, mod, &lMsgToDM);

    gSrPort[zPort].setArePhrasesPlayingOrScheduled(false);

    return(0);
}

/*------------------------------------------------------------------------------
openChannelToDynMgr():
------------------------------------------------------------------------------*/
int openChannelToDynMgr(int zPort, int zDmId)
{
    char	mod[] = "openChannelToDynMgr";
	int		fd; 
	int		rc;
	char	dynMgrFifo[256];

    if ( zDmId == -1 )
    {
        sprintf(dynMgrFifo, "%s/%s",
			gMrcpInit.getDynmgrFifoDir().c_str(), DYNMGR_REQUEST_FIFO);
    }
    else
    {
        sprintf(dynMgrFifo, "%s/%s.%d",
			gMrcpInit.getDynmgrFifoDir().c_str(), DYNMGR_REQUEST_FIFO, zDmId);
    }

    if ( (mknod(dynMgrFifo, S_IFIFO | 0777, 0) < 0)
            && (errno != EEXIST))
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, ERR, 
            "Unable to create fifo (%s). [%d, %s].",
			dynMgrFifo, errno, strerror(errno));
        return(-1);
    }

    if ( (fd = open(dynMgrFifo, O_WRONLY)) < 0)
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, ERR, 
            "Unable to open fifo (%s). [%d, %s].",
            dynMgrFifo, errno, strerror(errno));
        return(-1);
    }
    mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Successfully opened dynmgr fifo (%s).  fd=%d.",
			dynMgrFifo, fd);

	gSrPort[zPort].setFdToDynMgr(fd);

    return(0);
} // END: openChannelToDynMgr()

/*------------------------------------------------------------------------------
stopCollectingSpeech():
------------------------------------------------------------------------------*/
int stopCollectingSpeech(int zPort)
{
    char mod[] = "stopCollectingSpeech";
    struct Msg_PortOperation msg;
    struct MsgToDM lMsgToDM;

    memset(&lMsgToDM, 0, sizeof(struct MsgToDM));
    memset(&msg, 0, sizeof(struct Msg_PortOperation));

    msg.opcode = DMOP_STOPACTIVITY;
    msg.appCallNum = zPort;
    msg.appPid = gLast_SRRecognize[zPort].appPid;
    msg.appRef = gLast_SRRecognize[zPort].appRef;

    memcpy(&lMsgToDM, &msg, sizeof(struct Msg_StopActivity));

    sendRequestToDynMgr(zPort, mod, &lMsgToDM);

    return(0);
} // END: stopCollectingSpeech()

//===============================
//  class:  MrcpThread
//===============================

MrcpThread::MrcpThread()
{
//	pthread_t mrcpThreadId;
}


MrcpThread::~MrcpThread() {}


//----------------------
int MrcpThread::start(int zPort)
//----------------------
{
	char			mod[] = "MrcpThread::start"; 
	int				rc;
	char			thd[64];
	pthread_t		mrcpThreadId;

	if ( pthread_mutex_lock(&gMutexPortLock) != 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"pthread_mutex_lock() failed.  [%d, %s]",
			errno, strerror(errno));
	}
	else
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
		        REPORT_VERBOSE, MRCP_2_BASE, INFO, "Locked", thd);
	}
	savePorts[zPort] = zPort;

    if(gSrPort[zPort].getMrcpThreadId(__FILE__, __LINE__))
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"Will not appempt to create mrcpThread; thread alread exists.");

		if ( pthread_mutex_unlock(&gMutexPortLock) != 0 )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"pthread_mutex_unlock() failed.  [%d, %s]",
				errno, strerror(errno));
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			        REPORT_VERBOSE, MRCP_2_BASE, INFO, "Unlocked", thd);
		}
		return -1;
	}
	
	pthread_attr_t threadAttr;
	pthread_attr_init (&threadAttr);
	pthread_attr_setdetachstate (&threadAttr, PTHREAD_CREATE_DETACHED);

	rc = pthread_create(&mrcpThreadId, &threadAttr, mrcpThreadFunc,
				(void *) &savePorts[zPort]);

	if ( rc != 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"Failed to create mrcpThread; pthread_create() failed.  [%d, %s]",
			errno, strerror(errno));

		if ( pthread_mutex_unlock(&gMutexPortLock) != 0 )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"pthread_mutex_unlock() failed.  [%d, %s]",
				errno, strerror(errno));
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			        REPORT_VERBOSE, MRCP_2_BASE, INFO, "Unlocked", thd);
		}
		return(-1);
	}

    gSrPort[zPort].setMrcpThreadId(mrcpThreadId);
    gSrPort[zPort].setSavedMrcpThreadId(mrcpThreadId);

//	threadId2String(mrcpThreadId, thd);

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
		        REPORT_VERBOSE, MRCP_2_BASE, INFO, "Successfully created thread id [%lu,%lu] ",
				mrcpThreadId, gSrPort[zPort].getMrcpThreadId(__FILE__, __LINE__));

	if ( pthread_mutex_unlock(&gMutexPortLock) != 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"pthread_mutex_unlock() failed.  [%d, %s]",
			errno, strerror(errno));
	}
	else
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
		        REPORT_VERBOSE, MRCP_2_BASE, INFO, "Unlocked", thd);
	}

	return(0);
}

/*----------------------------------------------------------------------------
 *---------------------------------------------------------------------------*/
void sStopThisThread(int zPort)
{
	int		fd;
    static char mod[]="sStopThisThread";


	gSrPort[zPort].clearTTSRequests();
	cleanUp(zPort);
	gSrPort[zPort].closeAppResponseFifo(zPort);
    gSrPort[zPort].setPid(-1);
    gSrPort[zPort].setMrcpThreadId((pthread_t)NULL);

    if ( pthread_mutex_lock(&gMutexPortLock) != 0 )
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_NORMAL, MRCP_2_BASE, ERR,
            "pthread_mutex_lock() failed.  [%d, %s]",
            errno, strerror(errno));
    }
    else
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO, "Locked");
    }
    gSrPort[zPort].setMrcpThreadId((pthread_t)NULL);
    gSrPort[zPort].setSavedMrcpThreadId((pthread_t)NULL);

    if ( pthread_mutex_unlock(&gMutexPortLock) != 0 )
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_NORMAL, MRCP_2_BASE, ERR,
            "pthread_mutex_unlock() failed.  [%d, %s]",
            errno, strerror(errno));
    }
    else
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO, "Unlocked");
    }
    gSrPort[zPort].initializeElements();

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
		        REPORT_VERBOSE, MRCP_2_BASE, INFO, "Exiting mrcp thread.");
	pthread_detach(pthread_self());
	pthread_exit(NULL);
} // END: sStopThisThread()

