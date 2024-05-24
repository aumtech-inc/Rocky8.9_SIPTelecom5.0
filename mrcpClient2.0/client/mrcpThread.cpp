/*------------------------------------------------------------------------------
Program Name:   mrcpThread.cpp

Purpose     :   mrcpThread for the mrcp 2.0 client
Author      :   Aumtech, Inc.
Update      :   06/12/06 yyq Created the file.
Update : 08/06/10 djb MR 3052. Added sigterm logic.

Description:
MrcpThread is created by sipThread when it receives a 200 OK for INVITE request. 
It fetches requests from a global requestQueue for a specific application port, 
converts to Mrcp2.0 request message, builds TCP connection and sends to server, 
waits for response, parses responses and writes responses back to application.  

------------------------------------------------------------------------------*/

#include <signal.h>
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
	#include <errno.h>
	#include "UTL_accessories.h"
	#include "arcFifos.h"
	#include <sys/time.h>
	#include <time.h>
}

#ifdef SDM
int						gTecStreamSw[MAX_PORTS];
extern	XmlResultInfo	parsedResults[];
// struct	MsgToApp		gRecogResponse;
#endif

extern int     gCanProcessReaderThread;
extern int     gCanProcessSipThread;
extern int     gCanProcessMainThread;
int			   gTotalLicenseFailure = 0;

static pthread_mutex_t gMutexPortLock=PTHREAD_MUTEX_INITIALIZER;   // MR 4831

static void sigterm(int sig, siginfo_t *info, void *ctx);
void sigpipe(int x);
void	sStopThisThread(int zPort);
int openChannelToDynMgr(int zPort, int zDmId);
int getNewRequest(int callNum, bool *zGotNewRequest, MsgToDM *zMsgToDM);

static struct MsgToApp	sigTermResponse;

static void sigterm(int sig, siginfo_t *info, void *ctx)
{
    char mod[]="sigterm";

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
	        REPORT_DETAIL, MRCP_2_BASE, WARN,
			"SIGTERM (%d) caught from pid (%ld), user (%ld), sigTermResponse.opcode=%d.  Exiting.",
            sig, (long)info->si_pid, (long)info->si_uid, sigTermResponse.opcode);

	/*No pthread calls from sigterm*/
#if 0
	pthread_detach(pthread_self());
	pthread_exit(NULL);
#endif

	gCanProcessReaderThread = 0;
	gCanProcessSipThread = 0;
	gCanProcessMainThread = 0;

	if ( ( sigTermResponse.opcode != -1 ) &&
	     ( sigTermResponse.opcode != 0 ) )
	{
		mrcpClient2Log(__FILE__, __LINE__, sigTermResponse.appCallNum, mod, 
	        REPORT_DETAIL, MRCP_2_BASE, WARN,
			"Sending failure for opcode (%d) to app.", sigTermResponse.opcode);

		(int)writeResponseToApp(sigTermResponse.appCallNum, __FILE__, __LINE__,
				&sigTermResponse, sizeof(sigTermResponse));
	}
    return;
}/*END: void sigterm*/

void sigpipe(int x)
{
    char mod[]="sigpipe";

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
	        REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"SIGPIPE caught - %d.  Ignoring.", x);

    return;
}/*END: int sigpipe*/
/*----------------------------------------------------------------------------
 *---------------------------------------------------------------------------*/
void* mrcpThreadFunc(void *zAppPort)
{
	char mod[] = "mrcpThreadFunc";
	int			rc;
	int			pid;
//	struct MrcpPortInfo mPortInfo;
	int			callNum = -1;
	bool		gotNewRequest;
	struct MsgToDM msgToDM;
    struct sigaction act;
	void *void1;

    signal(SIGPIPE, sigpipe);

    memset (&act, '\0', sizeof(act));
    act.sa_sigaction = &sigterm;
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

	callNum = *(int *)zAppPort;

	sigTermResponse.opcode = -1;
	// process request queue
	//--------------------------
	while (gCanProcessReaderThread)
	{
		// fetch a request from requestQueue 
		// ---------------------------------
		rc = getNewRequest(callNum, &gotNewRequest, &msgToDM);
	
		if ( ( rc == EBUSY ) || (gotNewRequest == false) )
		{
			usleep(10000); 
			continue;
		}
		else if ( rc == -1 )
		{
			usleep(10000); 
            pthread_exit(NULL);
			continue;
		}

#if 0
		// create requstId 
		// ---------------
		// requestId must be monotonically increasing 
//		int		intRequestId = Singleton::getInstance().getCounter();
		char	tmpRequestId[16]; // 32 bit unsigned int => 10 bits
		sprintf(tmpRequestId, "%d", intRequestId);

		gSrPort[callNum].setRequestId(string(tmpRequestId));
#endif

		mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
	        REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"mrcpThread received request %d.", msgToDM.opcode);

		// parse appliation request
		// ------------------------
		// (only TEL_SRLoadGrammar and TEL_SRRecognize load requestQueue) 
		switch(msgToDM.opcode)
		{
			case DMOP_SRINIT:
				processSrInit(&msgToDM);
				break;
			case DMOP_SRLOADGRAMMAR:
				processSrLoadGrammar(&msgToDM);
				break;
			case DMOP_SRSETPARAMETER:
				processSrSetParameter(&msgToDM);
				break;
			case DMOP_SRGETPARAMETER:
				processSrGetParameter(&msgToDM);
				break;
			case DMOP_SRRESERVERESOURCE:
				processSrReserveResource(&msgToDM);
				break;
			case DMOP_SRRELEASERESOURCE:
				processSrReleaseResource(&msgToDM);
				break;
			case DMOP_SRRECOGNIZE:
				processSrRecognizeV2(&msgToDM);
				break;
			case DMOP_SRGETRESULT:
				processSrGetResult(&msgToDM);
				break;
			case DMOP_SRGETXMLRESULT:
				processSrGetXMLResult(&msgToDM);
				break;
            case DMOP_DISCONNECT:
				processDmopDisconnect(callNum, &msgToDM);
				sStopThisThread(callNum);
				break;
            case DMOP_SREXIT:
				processSrExit(&msgToDM);
				sStopThisThread(callNum);
				break;
            case DMOP_SRUNLOADGRAMMARS:
                processSrUnloadGrammars(&msgToDM);
                break;
			case DMOP_SRUNLOADGRAMMAR:
                processSrUnloadGrammar(&msgToDM);
                break;


#ifdef SDM
            case DMOP_TEC_STREAM:
                if ( gTecStreamSw[callNum] )
                {
					struct MsgToApp		*pResponse;
					mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
						REPORT_VERBOSE, MRCP_2_BASE, INFO, 
                		"Received DMOP_TEC_STREAM (%d:%s).  Sending response "
						"to application.", msgToDM.opcode, msgToDM.data);

//					rc = writeResponseToApp(gRecogResponse.appCallNum,
//							__FILE__, __LINE__, &gRecogResponse,
//							sizeof(gRecogResponse));

					pResponse = gSrPort[callNum].getRecognizeResponse();

					rc = writeResponseToApp(pResponse->appCallNum,
							__FILE__, __LINE__, pResponse,
							sizeof(struct MsgToApp));
                    gTecStreamSw[callNum] = 0;
                }
                else
                {
					mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
						REPORT_VERBOSE, MRCP_2_BASE, INFO, 
                		"Received DMOP_TEC_STREAM (%d:%s).  "
						"Response already sent to application; ignoring.",
						msgToDM.opcode, msgToDM.data);
                    gTecStreamSw[callNum] = 1;
                }
                break;
#endif
			default:
	 			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			        REPORT_VERBOSE, MRCP_2_BASE, INFO,
					"Invalid request received (%d), unable to process.",
                    msgToDM.opcode);
				continue;
	
		} // END: switch()

	} // END: for(;;)


	// thread return
	mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
        REPORT_DETAIL, MRCP_2_BASE, WARN, "Exiting...");

	sStopThisThread(callNum);

	return void1;
} // END: mrcpThreadFunc

/*------------------------------------------------------------------------------
getNewRequest()
------------------------------------------------------------------------------*/
int getNewRequest(int callNum, bool *zGotNewRequest, MsgToDM *zMsgToDM)
{
	static char 	mod[] = "getNewRequest";
	int		rc;

	*zGotNewRequest = false;
	memset((MsgToDM *)zMsgToDM, '\0', sizeof(MsgToDM));
	rc = pthread_mutex_trylock( &requestQueueLock[callNum]);
	if ( 0 == rc)
	{
		if( ! requestQueue[callNum].empty() ) 
		{
			*zMsgToDM = requestQueue[callNum].front();
			requestQueue[callNum].pop();
			*zGotNewRequest = true;
		}
		pthread_mutex_unlock( &requestQueueLock[callNum]);
		return(0);
	} 
	else if ( EBUSY == rc) 
	{
		return(EBUSY);
	}
	else
	{
		mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
       		REPORT_NORMAL, MRCP_2_BASE, ERR, 
			"Unable to acquire the lock to access request queue.");
//		pthread_exit(NULL);
		return(-1);
	}

	return(0);
} // END: getNewRequest()

/*------------------------------------------------------------------------------
processSrInit(appRequest)
------------------------------------------------------------------------------*/
void	processSrInit(MsgToDM *appRequest)
{
	static char					mod[]="processSrInit";
	struct Msg_SRInit			msg;
	int							callNum;
	struct MsgToApp				response;
	int							rc = -1;
	int							dmId;
	int                         clientId = -1;

	msg = *(struct Msg_SRInit *)appRequest;
	callNum = msg.appCallNum;

  	mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Processing {op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
		msg.opcode, msg.appCallNum, msg.appPid, msg.appRef, msg.appPassword);

	response.returnCode = rc;
	response.appCallNum = msg.appCallNum;
	response.opcode = msg.opcode;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.appPassword = msg.appPassword;
	
  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sigTermResponse.opcode = -1;

#ifdef SDM
    dmId = msg.appPassword;
	/*DDN: Added for multiple client support*/
	clientId = gMrcpInit.getClientId();
	sprintf(response.message, "%d", clientId);
#else
    dmId = callNum / 48;
	clientId = gMrcpInit.getClientId();
	sprintf(response.message, "%d", clientId);
#endif

	gSrPort[callNum].setDisconnectReceived(false);
	gSrPort[callNum].setCallActive(true);
	gSrPort[callNum].setCleanUpCalled(false);
	gSrPort[callNum].setHideDTMF(false);
	rc = SRInit(callNum);
	response.returnCode = rc;
#ifdef SDM
	// memset((struct MsgToApp *)&gRecogResponse, '\0', sizeof(struct MsgToApp));
	gSrPort[callNum].clearRecognizeResponse();
#endif
	memset((struct Msg_SRRecognize *)&(gLast_SRRecognize[callNum]),
				'\0', sizeof(struct Msg_SRRecognize));
	memset((struct Msg_SRReleaseResource *)&(gLast_SRReleaseResource[callNum]),
				'\0', sizeof(struct Msg_SRReleaseResource));

	if(rc != 0)
	{
		sprintf(response.message, "Failed to Initialize MRCP SR.  rc=%d.", rc);
		rc = writeResponseToApp(callNum, __FILE__, __LINE__,
				&response, sizeof(response));
		sigTermResponse.opcode = -1;

		sStopThisThread(callNum);
	}

    openChannelToDynMgr(callNum, dmId);
	rc = writeResponseToApp(callNum, __FILE__, __LINE__,
				&response, sizeof(response));
	sigTermResponse.opcode = -1;

	return;
} // END: processSrInit()

/*------------------------------------------------------------------------------
processSrReserveResource():
		send Sip INVITE message with SDP and write response to application
------------------------------------------------------------------------------*/
void processSrReserveResource(struct MsgToDM *pMsgToDM)
{
	static char						mod[]="processSrReserveResource";
	struct Msg_SRReserveResource	msg; 
	struct MsgToApp					response;
	int								rc=-1;
	int								callNum;
	int								fd;
	static int						alreadyCalledMax = 0;

	msg = *(struct Msg_SRReserveResource *)pMsgToDM;
	callNum = msg.appCallNum;
	gSrPort[msg.appCallNum].setOpCode(msg.opcode);

	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "Processing {op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
		msg.opcode,   msg.appCallNum,
		msg.appPid,   msg.appRef,
		msg.appPassword);

	rc = SRReserveResource(callNum);

	if(rc != 0)
	{
		gTotalLicenseFailure++;
		if(gTotalLicenseFailure > MAX_LICENSE_FAILURES)
		{
			if ( alreadyCalledMax == 0 )
			{
				bool	zonk=true;
				gMrcpInit.setTooManyFailures_shutErDown(zonk);
				mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, REPORT_DETAIL,
						MRCP_2_BASE, INFO, "Maximum license failures (%d) reached.  Setting flag to shut down.",
						MAX_LICENSE_FAILURES);
				alreadyCalledMax == 1;
			}
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, "Total license failures = %d.", gTotalLicenseFailure);
		}
	}
	else
	{
		//Successfully Reserved a licence Reset the counter 
		gTotalLicenseFailure = 0;
	}

	response.returnCode = rc;
	response.appCallNum = msg.appCallNum;
	response.opcode = msg.opcode;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.appPassword = msg.appPassword;
	memset(response.message, 0, sizeof(response.message));
	
	rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
		&response, sizeof(response));

	return;
} // END: processSrReserveResource()

/*------------------------------------------------------------------------------
processSrLoadGrammar(appRequest)
------------------------------------------------------------------------------*/
void	processSrLoadGrammar( MsgToDM *appRequest)
{
	static char mod[]="processSrLoadGrammar";

	struct Msg_SRLoadGrammar	msg;
	struct MsgToApp				response;
	int							rc;
	int							callNum;
	char 						message[256];
	string 						lMsg;
	//int							lVendorErrorCode;

	msg = *(struct Msg_SRLoadGrammar *)appRequest;
	callNum = msg.appCallNum;

	sprintf(message, "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
			msg.opcode, msg.appCallNum, msg.appPid, 
			msg.appRef, msg.appPassword);

	string grammarFile = msg.grammarFile;
//  	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
//		REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"DJB:grammar:(%d:%s) sizeof(struct Msg_SRLoadGrammar)=%d",
//		msg.grammarType, msg.grammarFile, sizeof(struct Msg_SRLoadGrammar));

	response.appCallNum = msg.appCallNum;
	response.opcode = msg.opcode;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.appPassword = msg.appPassword;

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "client terminated during operation");

	// build DEFINE grammar request and send to server
	rc = SRLoadGrammar(
			callNum, 
			msg.grammarType,  // int
			grammarFile, msg.weight);

	memset(response.message, 0, sizeof(response.message));
	if(rc == 10)
	{
		sprintf(response.message, "%d", rc);
		rc = -1;
	}

	response.returnCode = rc;
	
	rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
		&response, sizeof(response));
	sigTermResponse.opcode = -1;

	return;

} // END: processSrLoadGrammar()

//=================================================
void processSrRecognizeV2(MsgToDM *pAppRequest)
//=================================================
{
	static char mod[]="processSrRecognizeV2";

	struct Msg_SRRecognize	msg;
	struct MsgToApp			response;
	int						rc;
	int						lCallNum;
	int						lNumOfAlternatives = 0;
	SRRecognizeParams		yParams;
	char					yWaveformUri[516];

	msg = *(struct Msg_SRRecognize *)pAppRequest;

  	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
    	"{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,party:%d,bargein:%d,"
		"total_time:%d,lead:%d,trail:%d,beep:(%s),intr:%d,token:%d,"
		"numAlt:%d,res(%s)}",
	    msg.opcode,   msg.appCallNum,
	    msg.appPid,   msg.appRef,
	    msg.appPassword, msg.party,
	    msg.bargein,  msg.total_time,
	    msg.lead_silence, msg.trail_silence,
	    msg.beepFile, msg.interrupt_option,
	    msg.tokenType,  msg.alternatives,
	    msg.resource);

	lCallNum = msg.appCallNum;
	response.appCallNum = msg.appCallNum;
	response.opcode=msg.opcode;
	response.appPid=msg.appPid;
	response.appRef=msg.appRef;
	response.appPassword=msg.appPassword;

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "Client terminated during operation.");

	//  gDTMFDuringSpeechRec = 0;

  	memcpy(&(gLast_SRRecognize[lCallNum]), &msg, sizeof(msg));
	yParams.interruptOption	= msg.interrupt_option;
	yParams.leadSilence		= msg.lead_silence;
	yParams.trailSilence	= msg.trail_silence;
	yParams.totalTime		= msg.total_time;
	yParams.bargein			= msg.bargein;
	yParams.numResults		= 0;
	
#ifdef SDM
	gTecStreamSw[response.appCallNum] = 0;
	// memset((struct MsgToApp *)&gRecogResponse, '\0', sizeof(struct MsgToApp));
	gSrPort[lCallNum].clearRecognizeResponse();
#endif
        
	mrcpClient2Log(__FILE__, __LINE__, response.appCallNum, mod,
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Calling SRRecognizeV2...");
	rc = SRRecognizeV2(lCallNum, msg.appPid, &yParams, yWaveformUri);

    response.returnCode = rc;
    response.alternateRetCode = 0;
    response.vendorErrorCode = 0;
#ifndef SDM
	sprintf(response.message, "%d^%s", yParams.numResults, yWaveformUri);
#else
    //response.message[0] = yParams.numResults;
    sprintf(response.message, "%s", yParams.numResults);
#endif

	if ( gSrPort[response.appCallNum].IsDisconnectReceived() )
    {
        mrcpClient2Log(__FILE__, __LINE__, response.appCallNum, mod,
				REPORT_DETAIL, MRCP_2_BASE, WARN,
                "Disconnect was received.  Not sending response "
                "back to the application as the dynmgr should have "
                "performed this.");
    }

#ifdef SDM
	// memcpy(&gRecogResponse, &response, sizeof(response));
	gSrPort[lCallNum].setRecognizeResponse(&response);
	switch ( response.returnCode )
	{
		case TEL_TIMEOUT:
            if ( gTecStreamSw[response.appCallNum] )
            {
				gTecStreamSw[response.appCallNum] = 0;
				mrcpClient2Log(__FILE__, __LINE__, response.appCallNum, mod, 
					REPORT_VERBOSE, MRCP_2_BASE, INFO, 
					"Recognition timed out. Already received DMOP_TEC_STREAM, "
					"so send results back to the application now.");

				if ( ! gSrPort[response.appCallNum].IsDisconnectReceived() )
			    {
					rc=writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
							&response, sizeof(response));
				}
			}
			else
			{
				gTecStreamSw[response.appCallNum] = 1;
				mrcpClient2Log(__FILE__, __LINE__, response.appCallNum,
						mod, REPORT_DETAIL, MRCP_2_BASE, WARN, 
						"Recognition timed out.  Not sending results back to "
						"application until DMOP_TEC_STREAM received "
						"from dynmgr.");
			}
			break;
		case TEL_SUCCESS:
			if ( gTecStreamSw[response.appCallNum] )
			{
				gTecStreamSw[response.appCallNum] = 0;
				mrcpClient2Log(__FILE__, __LINE__, response.appCallNum, mod, 
					REPORT_VERBOSE, MRCP_2_BASE, INFO, 
					"Recognition complete.  Already received DMOP_TEC_STREAM, "
					"so send results back to the application now.");
				if ( ! gSrPort[response.appCallNum].IsDisconnectReceived() )
		    	{
					rc=writeResponseToApp(response.appCallNum, __FILE__,
						__LINE__, &response, sizeof(response));
				}
			}
			else
			{
#if 0
				if ((strcmp(parsedResults[lCallNum].inputMode, "dtmf") == 0 ) ||
				    (strcmp(parsedResults[lCallNum].inputMode, "DTMF") == 0 ) )
				{
					gTecStreamSw[response.appCallNum] = 0;
					mrcpClient2Log(__FILE__, __LINE__, response.appCallNum,
						mod, REPORT_VERBOSE, MRCP_2_BASE, INFO, 
						"Recognition complete.  DTMF received. ");
					if ( ! gSrPort[response.appCallNum].IsDisconnectReceived() )
		    		{
						if ( ! gSrPort[response.appCallNum].IsDisconnectReceived() )
		    			{
							rc=writeResponseToApp(response.appCallNum, __FILE__,
								__LINE__, &response, sizeof(response));
						}
					}
				}
				else
				{
#endif
					gTecStreamSw[response.appCallNum] = 1;
					mrcpClient2Log(__FILE__, __LINE__, response.appCallNum,
						mod, REPORT_VERBOSE, MRCP_2_BASE, INFO, 
						"Recognition complete.  Not sending results back to "
						"application until DMOP_TEC_STREAM received "
						"from dynmgr.");
//				}
			}
			break;
		default:
			mrcpClient2Log(__FILE__, __LINE__, response.appCallNum, mod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO, 
				"Checking  gTecStreamSw[%d] = %d. addr = %u",
				response.appCallNum, gTecStreamSw[response.appCallNum],
				&gTecStreamSw[response.appCallNum]);

            if ( gTecStreamSw[response.appCallNum] )
            {
				gTecStreamSw[response.appCallNum] = 0;
				mrcpClient2Log(__FILE__, __LINE__, response.appCallNum, mod, 
					REPORT_VERBOSE, MRCP_2_BASE, INFO, 
					"Recognition complete with failure (%d). Already received DMOP_TEC_STREAM, "
					"so send results back to the application now.", response.returnCode);

				if ( ! gSrPort[response.appCallNum].IsDisconnectReceived() )
			    {
					rc=writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
							&response, sizeof(response));
				}
			}
			else
			{
				gTecStreamSw[response.appCallNum] = 1;
				mrcpClient2Log(__FILE__, __LINE__, response.appCallNum,
						mod, REPORT_VERBOSE, MRCP_2_BASE, INFO, 
						"Recognition complete with failure (%d).  Not sending results back to "
						"application until DMOP_TEC_STREAM received "
						"from dynmgr.", response.returnCode);
			}
			break;
	}
#else		// IP - just return it to the application
	if ( ! gSrPort[response.appCallNum].IsDisconnectReceived() )
	{
		rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
				&response, sizeof(response));
	}
	sigTermResponse.opcode = -1;
	return;
#endif
	sigTermResponse.opcode = -1;
} // END: processSrRecognizeV2()

//=================================================
void processSrGetResult(MsgToDM *pAppRequest)
//=================================================
{
	static char mod[]="processSrGetResult";

	struct Msg_SRGetResult	msg;
	struct MsgToApp			response;
	int						rc;
	int						lCallNum;
	char					yResult[4096];
	int						yConfidence;

	msg = *(struct Msg_SRGetResult *)pAppRequest;

  	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,numAlt:%d,token:%d,delimiter:%c}",
		msg.opcode,     msg.appCallNum,
        msg.appPid,     msg.appRef,
        msg.appPassword, msg.alternativeNumber,
        msg.tokenType,  msg.delimiter);


	lCallNum = msg.appCallNum;
	response.appCallNum = msg.appCallNum;
	response.opcode=msg.opcode;
	response.appPid=msg.appPid;
	response.appRef=msg.appRef;
	response.appPassword=msg.appPassword;
    response.vendorErrorCode = 0;

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "client terminated during operation");

	memset(yResult, 0 , sizeof(yResult));
	rc = SRGetResult(lCallNum, msg.alternativeNumber, msg.tokenType,
            msg.delimiter, yResult, &yConfidence);

    response.returnCode = rc;
    if(rc == 0)
    {
        if(yConfidence == 1)
        {
            yConfidence = 1000;
        }
        else
        {
            yConfidence = yConfidence * 10;
        }
        sprintf(response.message, "%d^%s", yConfidence, yResult);
    }

	rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
		&response, sizeof(response));
	sigTermResponse.opcode = -1;

} // END: processSrGetResult()

//=================================================
void processSrUnloadGrammars(MsgToDM *pAppRequest)
//=================================================
{
	static char mod[]="processSrUnloadGrammars";

	struct Msg_SRUnloadGrammar	msg;
	struct MsgToApp				response;
	int							rc;
	int							lCallNum;
	char						yResult[256];
	int							yConfidence;

	msg = *(struct Msg_SRUnloadGrammar *)pAppRequest;

  	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
        "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
        msg.opcode,     msg.appCallNum,
        msg.appPid,     msg.appRef,
        msg.appPassword);

	lCallNum = msg.appCallNum;
	response.appCallNum = msg.appCallNum;
	response.opcode=msg.opcode;
	response.appPid=msg.appPid;
	response.appRef=msg.appRef;
	response.appPassword=msg.appPassword;
    response.vendorErrorCode = 0;

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "client terminated during operation");

	memset(yResult, 0 , sizeof(yResult));
	rc = SRUnloadGrammars(lCallNum);

    response.returnCode = rc;

	rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
		&response, sizeof(response));
	sigTermResponse.opcode = -1;

} // END: processSrUnloadGrammars()

//=================================================
void processSrUnloadGrammar(MsgToDM *pAppRequest)
//=================================================
{
    static char mod[]="processSrUnloadGrammar";

    struct Msg_SRUnloadGrammar  msg;
    struct MsgToApp             response;
    int                         rc;
    int                         lCallNum;
    char                        yResult[256];
    int                         yConfidence;

    msg = *(struct Msg_SRUnloadGrammar *)pAppRequest;

    mrcpClient2Log(__FILE__, __LINE__, -1, mod,
        REPORT_VERBOSE, MRCP_2_BASE, INFO,
        "grammarNmae(%s) {op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
        msg.grammarName, msg.opcode,     msg.appCallNum,
        msg.appPid,     msg.appRef,
        msg.appPassword);

    lCallNum = msg.appCallNum;
    response.appCallNum = msg.appCallNum;
    response.opcode=msg.opcode;
    response.appPid=msg.appPid;
    response.appRef=msg.appRef;
    response.appPassword=msg.appPassword;
    response.vendorErrorCode = 0;

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "Client terminated during operation.");

    memset(yResult, 0 , sizeof(yResult));
    rc = SRUnloadGrammar(lCallNum, msg.grammarName);

    response.returnCode = rc;

    rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
        &response, sizeof(response));
	sigTermResponse.opcode = -1;

} // END: processSrUnloadGrammars()




//======================================================
void processSrGetXMLResult(MsgToDM *pAppRequest)
{

	static char mod[]="processSrGetXMLResult";

	struct Msg_SRGetXMLResult	msg;
	struct MsgToApp				response;
	int							rc;
	int							lCallNum;

	msg = *(struct Msg_SRGetXMLResult *)pAppRequest;

  	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Processing {op:%d,c#:%d,pid:%d,ref:%d,pw:%d,filename:(%s)}",
        msg.opcode,     msg.appCallNum,
        msg.appPid,     msg.appRef,
        msg.appPassword, msg.fileName);

	lCallNum = msg.appCallNum;
	response.appCallNum = msg.appCallNum;
	response.opcode=msg.opcode;
	response.appPid=msg.appPid;
	response.appRef=msg.appRef;
	response.appPassword=msg.appPassword;
    response.vendorErrorCode = 0;

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "Client terminated during operation.");

	rc = SRGetXMLResult(lCallNum, msg.fileName);

    response.returnCode = rc;

	rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
		&response, sizeof(response));

	sigTermResponse.opcode = -1;
}

void processDmopDisconnect(int zPort, MsgToDM *pAppRequest)
{
	bool			gotNewRequest;
	struct MsgToDM	msgToDM;
	static char		mod[]="processDmopDisconnect";

	struct Msg_SRExit			msg;
	struct MsgToApp				response;
	int							rc;
//	int							lCallNum;


	memset(&response, 0, sizeof(struct MsgToApp));

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "Client terminated during operation.");

	rc = SRExit(zPort);

	// fetch a request to respond to the application
	// ---------------------------------------------
	rc = getNewRequest(zPort, &gotNewRequest, &msgToDM);
	
	if ( ( rc != 0 ) || ( gotNewRequest == false) )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"No request found on queue; sending nothing to the application.");
		return;
	}
	else
	{
		response.appCallNum		= msgToDM.appCallNum;
		response.opcode			= msgToDM.opcode;
		response.appPid			= msgToDM.appPid;
		response.appRef			= msgToDM.appRef;
		response.appPassword	= msgToDM.appPassword;

		mrcpClient2Log(__FILE__, __LINE__, response.appCallNum, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Sending disconnect back to application for "
			"op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
       		 response.opcode,     response.appCallNum,
       		 response.appPid,     response.appRef,
       		 response.appPassword);
	}

	rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
		&response, sizeof(response));
	sigTermResponse.opcode = -1;

//	gSrPort[lCallNum].closeAppResponseFifo(__FILE__, __LINE__, lCallNum);
	gSrPort[zPort].closeAppResponseFifo(__FILE__, __LINE__, zPort);

} // END: processDmopDisconnect()

void processSrExit(MsgToDM *pAppRequest)
{

	static char mod[]="processSrExit";

	struct Msg_SRExit			msg;
	struct MsgToApp				response;
	int							rc;
	int							lCallNum;
	
	memset(&response, 0, sizeof(struct MsgToApp));

	msg = *(struct Msg_SRExit *)pAppRequest;

  	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Processing {op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
        msg.opcode,     msg.appCallNum,
        msg.appPid,     msg.appRef,
        msg.appPassword);

	lCallNum = msg.appCallNum;
	response.appCallNum = msg.appCallNum;
	response.opcode=msg.opcode;
	response.appPid=msg.appPid;
	response.appRef=msg.appRef;
	response.appPassword=msg.appPassword;
    response.vendorErrorCode = 0;

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "Client terminated during operation.");

	rc = SRExit(lCallNum);

    response.returnCode = rc;

	rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
		&response, sizeof(response));
	sigTermResponse.opcode = -1;

	gSrPort[lCallNum].closeAppResponseFifo(__FILE__, __LINE__, lCallNum);

} // END: processSrExit()

//======================================================
void processSrReleaseResource(MsgToDM *pAppRequest)
//======================================================
{
    char mod[] = "processSrReleaseResource";
    struct Msg_SRReleaseResource	msg;
    struct MsgToApp					response;
	
	memset(&response, 0, sizeof(struct MsgToApp));

    int		callNum	= -1;
    int		rc		= -1;


    msg = *(struct Msg_SRReleaseResource *)pAppRequest;

    callNum = msg.appCallNum;

  	memcpy(&(gLast_SRReleaseResource[callNum]), &msg, sizeof(msg));

  	mrcpClient2Log(__FILE__, __LINE__, callNum, mod, REPORT_VERBOSE, MRCP_2_BASE,
			INFO, "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
			msg.opcode, msg.appCallNum, msg.appPid, 
			msg.appRef, msg.appPassword);


	response.appCallNum = msg.appCallNum;
	response.opcode = msg.opcode;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.appPassword = msg.appPassword;
	memset(response.message, 0, sizeof(response.message));

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "Client terminated during operation.");

    rc = SRReleaseResource(callNum);

	response.returnCode = rc;
	
	rc = writeResponseToApp(response.appCallNum, __FILE__, __LINE__,
		&response, sizeof(response));

	sigTermResponse.opcode = -1;
} // END: processSrReleaseResource()

/*------------------------------------------------------------------------------
processSrGetParameter():
------------------------------------------------------------------------------*/
void processSrGetParameter(struct MsgToDM *zpMsgToDM)
{
    static char					mod[]="processSrGetParameter";
    struct Msg_SRGetParameter	msg;
    struct MsgToApp				response;
    int		rc;
    int		callNum;
    char	lName[100];
    char	lValue[200];
    char	lMsg[256];
    char	message[256];
	
	memset(&response, 0, sizeof(struct MsgToApp));

    msg = *(struct Msg_SRGetParameter *)zpMsgToDM;

	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO,
		"processing {op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
        msg.opcode, msg.appCallNum, msg.appPid, msg.appRef, msg.appPassword);

    callNum = msg.appCallNum;
    response.appCallNum = msg.appCallNum;
    response.opcode = msg.opcode;
    response.appPid = msg.appPid;
    response.appRef = msg.appRef;
    response.appPassword = msg.appPassword;
    memset(response.message, 0, sizeof(response.message));

    memset(lName, 0, sizeof(lName));
    memset(lValue, 0, sizeof(lValue));

    //sscanf(msg.nameValue, "%[^=]=%s", lName, lValue);
	sprintf(lName, "%s", msg.name);

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "Client terminated during operation.");

    rc = SRGetParameter(callNum, lName, lValue);
    response.returnCode = rc;

    rc = writeResponseToApp(callNum, __FILE__, __LINE__, 
		&response, sizeof(response));

	sigTermResponse.opcode = -1;
    return;
} // END: processSrGetParameter() 

/*------------------------------------------------------------------------------
processSrSetParameter():
------------------------------------------------------------------------------*/
void processSrSetParameter(struct MsgToDM *zpMsgToDM)
{
    static char					mod[]="processSrSetParameter";
    struct Msg_SRSetParameter	msg;
    struct MsgToApp				response;
    int		rc;
    int		callNum;
    char	lName[100];
    char	lValue[256];
    char	lMsg[256];
    char	message[256];

    msg = *(struct Msg_SRSetParameter *)zpMsgToDM;

	mrcpClient2Log(__FILE__, __LINE__, msg.appCallNum, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO,
		"processing {op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
        msg.opcode, msg.appCallNum, msg.appPid, msg.appRef, msg.appPassword);

    callNum = msg.appCallNum;
    response.appCallNum = msg.appCallNum;
    response.opcode = msg.opcode;
    response.appPid = msg.appPid;
    response.appRef = msg.appRef;
    response.appPassword = msg.appPassword;
    memset(response.message, 0, sizeof(response.message));

    if(msg.nameValue[0] == '\0')
    {
        response.returnCode = -1;
        sprintf(response.message,"Failed to get Msg_SRSetParameter.");
        rc = writeResponseToApp(callNum, __FILE__, __LINE__, 
				&response, sizeof(response));
        return;
    }

    memset(lName, 0, sizeof(lName));
    memset(lValue, 0, sizeof(lValue));

    sscanf(msg.nameValue, "%[^=]=%s", lName, lValue);

  	memcpy(&sigTermResponse, &response, sizeof(response));
	sigTermResponse.returnCode = -1;
	sprintf(sigTermResponse.message, "%s", "Client terminated during operation.");

    rc = SRSetParameter(callNum, lName, lValue);
    response.returnCode = rc;

    rc = writeResponseToApp(callNum, __FILE__, __LINE__, 
		&response, sizeof(response));
	sigTermResponse.opcode = -1;

    return;
} // END: processSrSetParameter() 

/*------------------------------------------------------------------------------
processUnloadGrammar():	
	This is for VXI only.
------------------------------------------------------------------------------*/
void  processUnloadGrammar(struct MsgToDM *pMsgToDM)
{
    static char                 mod[]="processLoadGrammars";
    struct Msg_SRLoadGrammar    msg;
    int                         callNum;

	return;
} // END: processUnloadGrammar()

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
            "Unable to create fifo (%s). [%d, %s]",
			dynMgrFifo, errno, strerror(errno));
        return(-1);
    }

    if ( (fd = open(dynMgrFifo, O_WRONLY)) < 0)
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, ERR, 
            "Unable to open fifo (%s). [%d, %s]",
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

    gSrPort[zPort].setArePhrasesPlayingOrScheduled(false);

    return(0);
} // END: stopCollectingSpeech

/*------------------------------------------------------------------------------
sendRecognizeToDM():
------------------------------------------------------------------------------*/
int sendRecognizeToDM(int zPort, const char *zpServerIP, int zRtpPort,
                int zRtcpPort, char *zUtteranceFile, int zPayloadType,
                int zCodecType)
{
    char 					mod[] = "sendSRRecognizeToDM";
    struct Msg_SRRecognize	msg;
    struct MsgToDM			lMsgToDM;
	int						rc;

//printf("DDNDEBUG: %s %d  zPort(%d) zpServerIp(%s) zRtpPort(%d)\n", __FILE__, __LINE__, zPort, zpServerIP, zRtpPort);fflush(stdout);

    memcpy(&msg, &(gLast_SRRecognize[zPort]), sizeof(msg));
    sprintf(msg.resource, "%s|%d|%d|%d|%d|%s|%s",
            zpServerIP, zRtpPort, zRtcpPort,
            zPayloadType, zCodecType, zUtteranceFile,
            gLast_SRRecognize[zPort].resource);

    msg.opcode = DMOP_SRRECOGNIZEFROMCLIENT;

	if(gSrPort[zPort].IsGrammarGoogle())
	{
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_VERBOSE, MRCP_2_BASE, INFO,
            "ARCGS: This is Google Grammar. Notifying Media Mgr.");

		msg.tokenType = SR_GOOGLE;
	}
	else
	{
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_VERBOSE, MRCP_2_BASE, INFO,
            "ARCGS: This is NOT a Google Grammar.");
	}

//#ifdef SDM
	msg.appPassword = gMrcpInit.getClientId();
//#endif

    memset(&lMsgToDM, 0, sizeof(lMsgToDM));
    memcpy(&lMsgToDM, &msg, sizeof(msg));

    rc = sendRequestToDynMgr(zPort, mod, &lMsgToDM);

    return(rc);
} // END: sendRecognizeToDM()

/*------------------------------------------------------------------------------
sendReleaseResourceToDM():
------------------------------------------------------------------------------*/
int sendReleaseResourceToDM(int zPort)
{
    char							mod[] = "sendReleaseResourceToDM";
    struct Msg_SRReleaseResource	msg;
    struct MsgToDM			lMsgToDM;
	int						rc;

    memcpy(&msg, &(gLast_SRReleaseResource[zPort]), sizeof(msg));
    msg.opcode = DMOP_SRRELEASERESOURCE;

    memset(&lMsgToDM, 0, sizeof(lMsgToDM));
    memcpy(&lMsgToDM, &msg, sizeof(msg));

    rc = sendRequestToDynMgr(zPort, mod, &lMsgToDM);

    return(rc);
} // END: sendReleaseResourceToDM()

//===============================
//  class:  MrcpThread
//===============================

MrcpThread::MrcpThread()
{
//	pthread_t mrcpThreadId;
}


MrcpThread::~MrcpThread() {}

int callNums[MAX_PORTS];


//----------------------
int MrcpThread::start(int zPort)
//----------------------
{
	char			mod[] = "MrcpThread::start"; 
	int				rc;
	char            thd[64];

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

	callNums[zPort] = zPort;

    if(gSrPort[zPort].getMrcpThreadId())
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
				(void *) &callNums[zPort]);

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

//    threadId2String(mrcpThreadId, thd);

    mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO, "Successfully created thread id [%lu,%lu] ",
                mrcpThreadId, gSrPort[zPort].getMrcpThreadId());
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

	if( gCanProcessReaderThread)
	{
		if ( ! gSrPort[zPort].IsCleanUpCalled() )
		{
			cleanUp(zPort);
		}

		gSrPort[zPort].closeAppResponseFifo(__FILE__, __LINE__, zPort);
		gSrPort[zPort].initializeElements();
	}

    gSrPort[zPort].setMrcpThreadId((pthread_t)NULL);

	mrcpClient2Log(__FILE__, __LINE__, zPort, "sStopThisThread", 
        REPORT_VERBOSE, MRCP_2_BASE, INFO, "Exiting.");
	pthread_detach(pthread_self());
	pthread_exit(NULL);
} // END: sStopThisThread()

