/*----------------------------------------------------------------------------- 
rogram:	TTSSpeak.cpp;
Purpose:	Perform SPEAK logic for MRCPv2 client.
Author:		Aumtech, Inc.
Update:		07/20/06 djb	Created the file.
--------------------------------------------------------------------------------
Notes - typical scenario, but does not always occur in this order:

1)	DMOP_SPEAKMRCPTTS --> dynmgr
	*	sendSRRecognizeToDM(zPort, <mrcpServer IP>, <rtp port[0]>, 
						<rtp port[1]>, <fileName>, <payloadType>, <codecType>);
	*	tells the dynmgr to start collecting / sending speech
2)	Send SPEAK to mrcp server.
3)	Now dynmgr should be receiving/speaking/saving data from the mrcpServer,
	and we are waiting for events.
	* SPEAK-COMPLETE	<-- mrcpServer
		- recognition is over, send the DMOP_STOPTTS_RECEIVING to dynmgr
		  [ call to stopCollectingSpeech() ]
	* DMOP_DISCONNECT		<-- dynmgr
	* DMOP_STOPTTS			<-- dynmgr
		- These tell the client that the application is gone.  The client is to 
		  stop processing at this point by sending a STOP to the mrcpServer.
------------------------------------------------------------------------------*/
// #include "parseResult.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#include <fstream>
#include <string>
#include "AppMessages_mrcpTTS.h"

#include "mrcpCommon2.hpp"
#include "mrcp2_HeaderList.hpp"
#include "mrcp2_Request.hpp"
#include "mrcp2_Response.hpp"
#include "mrcp2_Event.hpp"
#include "mrcpTTS.hpp"
#include "parseResult.hpp"

using namespace mrcp2client;
using namespace std;

extern "C"
{
	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>
	#include "Telecom.h"
	#include "arcSR.h"
}


typedef struct
{
    int     statusCode;
    int     completionCause;
    string  eventStr;
    string  serverState;
    string  content;
} EventInfo;

#define RC_NO_GRAMMARS  -98

static const string	SPEAKCOMPLETE_STR	= "SPEAK-COMPLETE";

XmlResultInfo           parsedResults[MAX_PORTS];
string					xmlResult[MAX_PORTS];

/*
** Function prototypes
*/
string sCreateSpeakRequest(int zPort,ARC_TTS_REQUEST_SINGLE_DM *zParams,
			string & zContentToBeSpoken);
int sGetTTSString(int zPort, char *zFile, string & zStr);
int sSendSTOPToServer(int zPort, int *zRecognizerStarted,
						EventInfo *zEventInfo);
int sCheckForMrcpEvent(int zPort, EventInfo *zEventInfo);
int sendSpeakMRCPToDM(int zPort, char *zpServerIP, int zRtpPort,
                int zRtcpPort, char *zFile, int zPayloadType,
                int zCodecType, Msg_Speak *zMsgSpeak, long zId);
int sendStopToDM(int zPort, int zOpcode, Msg_Speak *zMsgSpeak);
void sGetTtsId(int zPort, long *zId);

extern int checkMrcpRequest(int zPort, int zInputFlag);

extern int	sendRecognizeToDM(int zPort, const char *zpServerIP, int zRtpPort,
				int zRtcpPort, char *zUtteranceFile, int zPayloadType,
				int zCodecType);

/*------------------------------------------------------------------------------
TTSSpeak():
------------------------------------------------------------------------------*/
int TTSSpeak(int zPort, ARC_TTS_REQUEST_SINGLE_DM *zParams)
{
	static char		mod[]="TTSSpeak";
	time_t	 		lCurrentTime	= 0;
	time_t	 		lStartTime		= 0;
	int				rc;
	int				yRecognizerStarted = 0;
    EventInfo       yEvent;
	char			serverIP[64];
	long			yId = 0;
	ARC_TTS_REQUEST_SINGLE_DM	*pParams = NULL;
	int				savedOpcode;
	string			largeString = "";
	string			yEventName;
	string			ySpeakMsg;
	time_t			timeout;
	time_t			startTime;
	time_t			currentTime;
	char			yStrTtsData[4096];
	char			encoding[32];
	string			sIP;
	
	sIP = gSrPort[zPort].getServerIP();
	sprintf(serverIP, "%s", sIP.c_str());

	savedOpcode = zParams->speakMsgToDM.opcode;
	if ( zParams->speakMsgToDM.opcode == DMOP_START_TTS ) 
	{
		yId = atol(zParams->string);
		if ( yId <= 0 )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Invalid Queue ID (%ld).  Unable to "
				"process TTS put queue.");
			return(-1);
		}

		pParams = gSrPort[zPort].retrieveTTSRequest(yId, largeString);
		if ( ! pParams ) 
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Unable to retrieve TTS Put Queue for id (%ld).", yId);
			return(-1);
		}

		if ( largeString.empty() ) 
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, "Successfully retrieved queued TTS request "
				"for id=(%ld), string=(%.*s)",
				yId, 300, pParams->string);
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, "Successfully retrieved queued TTS request "
				"for id=(%ld), largeString=(%.*s)",
				yId, 300, largeString.c_str());
		}
//		memcpy(zParams, pParams, sizeof(ARC_TTS_REQUEST_SINGLE_DM));
//		bcopy(pParams, zParams, sizeof(ARC_TTS_REQUEST_SINGLE_DM));
		sprintf(zParams->fileName,	"%s", pParams->fileName);
		sprintf(zParams->type,		"%s", pParams->type);
		sprintf(zParams->port,		"%s", pParams->port);
		sprintf(zParams->pid,		"%s", pParams->pid);
		sprintf(zParams->app,		"%s", pParams->app);
//		sprintf(zParams->string,	"%s", pParams->string);
		sprintf(zParams->language,	"%s", pParams->language);
        sprintf(zParams->voiceName, "%s", pParams->voiceName);
        sprintf(zParams->gender,    "%s", pParams->gender);

		sprintf(zParams->compression, "%s", pParams->compression);
		sprintf(zParams->timeOut,	"%s", pParams->timeOut);
		sprintf(zParams->resultFifo,"%s", pParams->resultFifo);
		zParams->async = pParams->async;
		sprintf(zParams->asyncCompletionFifo, "%s", pParams->asyncCompletionFifo);

		zParams->speakMsgToDM.opcode		= pParams->speakMsgToDM.opcode;
		zParams->speakMsgToDM.appCallNum	= pParams->speakMsgToDM.appCallNum;
		zParams->speakMsgToDM.appPid		= pParams->speakMsgToDM.appPid;
		zParams->speakMsgToDM.appRef		= pParams->speakMsgToDM.appRef;
		zParams->speakMsgToDM.appPassword	= pParams->speakMsgToDM.appPassword;
		zParams->speakMsgToDM.list			= pParams->speakMsgToDM.list;
		zParams->speakMsgToDM.allParties	= pParams->speakMsgToDM.allParties;
		zParams->speakMsgToDM.interruptOption = pParams->speakMsgToDM.interruptOption;
		zParams->speakMsgToDM.synchronous	= pParams->speakMsgToDM.synchronous;
		zParams->speakMsgToDM.deleteFile	= pParams->speakMsgToDM.deleteFile;
		sprintf(zParams->speakMsgToDM.key,	"%s", pParams->speakMsgToDM.key);
		sprintf(zParams->speakMsgToDM.file,	"%s", pParams->speakMsgToDM.file);

		sprintf(yStrTtsData, "%s", pParams->string);

//		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
//				MRCP_2_BASE, INFO, "pParams->string=(%s)  zParams->string=(%s)",
//				pParams->string, zParams->string);				
//
		(void) gSrPort[zPort].removeTTSRequest(yId);
	
	}
	else
	{
		switch ( zParams->speakMsgToDM.synchronous )
		{
			case PUT_QUEUE:
			case PUT_QUEUE_ASYNC:
			case PLAY_QUEUE_SYNC:
			case PLAY_QUEUE_ASYNC:
				sGetTtsId(zPort, &yId);
				
				rc = gSrPort[zPort].addTTSRequest(yId, zParams);
				if ( rc == 0 )
				{
					mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
						REPORT_VERBOSE, MRCP_2_BASE, INFO,
						"Added TTS Request to queue; id=(%ld)", yId);
				}
				else
				{
					mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
						REPORT_NORMAL, MRCP_2_BASE, ERR, 
						"Failed to add TTS to queue.");
					return(TEL_FAILURE);
				}

//				mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
//					REPORT_VERBOSE, MRCP_2_BASE, INFO,
//					"DJB: Calling printTTSRequests() for zport %d; appPort=%d",
//					zPort, gSrPort[zPort].SRPort::getApplicationPort());

				gSrPort[zPort].printTTSRequests();

				rc = sendSpeakMRCPToDM(zPort,
						serverIP,
						gSrPort[zPort].getRtpPort(),
						gSrPort[zPort].getRtpPort() + 1,
						zParams->fileName,
						atoi(gSrPort[zPort].getPayload().c_str()), 
						atoi(gSrPort[zPort].getCodec().c_str()),
						&(zParams->speakMsgToDM), yId);
				if ( rc < 0 )
				{
					return(TEL_FAILURE);
				}
				return(0);
				break;
			default:
				if ( strcmp(zParams->type, "FILE") == 0 )
				{
					if ((rc = sGetTTSString(zPort, zParams->string, 
											largeString)) < 0 )
					{
						largeString = "";
					}
				}
				else
				{
					sprintf(yStrTtsData, "%s", zParams->string);
				}
				break;
		}
	}
	timeout = atol(zParams->timeOut);

	if ( largeString.empty() ) 
	{
		//string tmpString(zParams->string);
		//string tmpString(pParams->string);

		string tmpString(yStrTtsData);
		
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, "Calling sCreateSpeakRequest with"
				"string=(%.*s)", 300, tmpString.c_str());
		ySpeakMsg = sCreateSpeakRequest(zPort, zParams, tmpString);
	}
	else
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, "Calling sCreateSpeakRequest with"
				"string=(%.*s)", 300, largeString.c_str());
		ySpeakMsg = sCreateSpeakRequest(zPort, zParams, largeString);
	}
		
	if ( ySpeakMsg.empty() ) 
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
			"Unable to create SPEAK request.  Empty request generated.  "
			"Unable to send SpeakTTS request.");

		return(TEL_FAILURE);
	}

	string		yRecvMsg;
	string		yServerState;
	int			yCompletionCode;
	int			yStatusCode;

	if ( savedOpcode != DMOP_START_TTS ) 
	{

		//sprintf(serverIP, "%s", gMrcpInit.getServerIP(TTS).c_str());
		rc = sendSpeakMRCPToDM(zPort,
			serverIP,
			gSrPort[zPort].getRtpPort(),
			gSrPort[zPort].getRtpPort() + 1,
			zParams->fileName,
			atoi(gSrPort[zPort].getPayload().c_str()), 
			atoi(gSrPort[zPort].getCodec().c_str()),
			&(zParams->speakMsgToDM), (long) 0);
		if ( rc < 0 )
		{
			return(TEL_FAILURE);
		}
	}

	gSrPort[zPort].setStopProcessingTTS(false);
	gSrPort[zPort].setInProgressReceived(false);
	time(&startTime);

	/*DDN: 07172012 Try to flush the port before sending new SPEAK */
	sCheckForMrcpEvent(zPort, &yEvent);

	rc = processMRCPRequest (zPort, ySpeakMsg, yRecvMsg,
				yServerState, &yStatusCode, &yCompletionCode, yEventName);

	gSrPort[zPort].setInProgressReceived(true);

	if ( rc == YES_EVENT_RECEIVED )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, 
			"Event (%s) received for SPEAK request, sending STOP to Media Mgr.",
			 yEventName.c_str());
		if ( yEventName == SPEAKCOMPLETE_STR )
		{
			sendStopToDM(zPort, DMOP_TTS_COMPLETE_SUCCESS,
						&(zParams->speakMsgToDM));
		}
		else
		{
			sendStopToDM(zPort, DMOP_TTS_COMPLETE_FAILURE,
					&(zParams->speakMsgToDM));
		}
	
		return(0);
	}

	if ( ( yStatusCode != 200 ) ||
		 ( yServerState != "IN-PROGRESS" ) )
	{
		sendStopToDM(zPort, DMOP_TTS_COMPLETE_TIMEOUT, &(zParams->speakMsgToDM));
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"No response received for request. Backing out.");
		return(-2);    // return from here, but don't send response to the app
	}
	yRecognizerStarted = 1;

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO, 
		"SpeakTTS successfully initiated with the MRCP Server.");

	lStartTime				= -1;
	for (;;)
	{
//		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
//			MRCP_2_BASE, INFO, "%d = gSrPort[%d].IsStopProcessingTTS()",
//			 gSrPort[zPort].IsStopProcessingTTS(), zPort);
		if ( gSrPort[zPort].IsStopProcessingTTS() )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, 
				"Sending STOP to  MRCP Server.");
			rc = sSendSTOPToServer(zPort, &yRecognizerStarted, &yEvent);
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, 
				"After sending STOP to  MRCP Server rc=%d.", rc);
			if ( rc == YES_EVENT_RECEIVED )
			{
				if ( yEvent.eventStr == SPEAKCOMPLETE_STR )
				{
					sendStopToDM(zPort, DMOP_TTS_COMPLETE_SUCCESS,
						&(zParams->speakMsgToDM));
					return(0);
				}
			}
			if ( rc == 0 )
			{
				return(0);
			}
			break;
		}

		rc = sCheckForMrcpEvent(zPort, &yEvent);
		if ( rc == -1 )
		{
			yRecognizerStarted = 0;
			sendStopToDM(zPort, DMOP_TTS_COMPLETE_FAILURE,
					&(zParams->speakMsgToDM));
			break;
		}
		if ( rc == YES_EVENT_RECEIVED )
		{
			if ( yEvent.eventStr == SPEAKCOMPLETE_STR )
			{
				sendStopToDM(zPort, DMOP_TTS_COMPLETE_SUCCESS,
					&(zParams->speakMsgToDM));
				return(0);
			}
		}

		time(&currentTime);
		if ( currentTime - startTime > timeout )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, 
				"Sending STOP to  MRCP Server.");
			rc = sSendSTOPToServer(zPort, &yRecognizerStarted, &yEvent);
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, 
				"After sending STOP to  MRCP Server.");
			if ( rc == YES_EVENT_RECEIVED )
			{
				if ( yEvent.eventStr == SPEAKCOMPLETE_STR )
				{
					return(0);
				}
			}
			sendStopToDM(zPort, DMOP_TTS_COMPLETE_TIMEOUT,
						&(zParams->speakMsgToDM));
			break;
		}
		usleep(100);
	}

	if ( gSrPort[zPort].IsDisconnectReceived() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"No response received for request. Backing out, but not notifying media manager.");
	}
	else
	{
		sendStopToDM(zPort, DMOP_TTS_COMPLETE_TIMEOUT, &(zParams->speakMsgToDM));
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"No response received for request. Backing out.");
	}
	return(-2);    // return from here, but don't send response to the app

} // END: TTSSpeak()

/*------------------------------------------------------------------------------
sCreateSpeakRequest():
------------------------------------------------------------------------------*/
string sCreateSpeakRequest(int zPort,ARC_TTS_REQUEST_SINGLE_DM *zParams,
			string & zContentToBeSpoken)
{
	static char	mod[] = "sCreateSpeakRequest";
	int		rc = 0;
	MrcpHeader					 	header;
	MrcpHeaderList				 	headerList;
	int                             yCounter;
	string							yContent = "";
	char							yContentLength[16];
	string							ySpeakMsg;
	char							yContentType[32];
	char							*p;
	char							yEncoding[32] = "";
	char							yTempData[4096];

#if 0
	if ( strstr(zParams->type, "FILE") != NULL )
	{
		if ((rc = sGetTTSString(zPort, zParams->string, yContent)) < 0 )
		{
			ySpeakMsg = "";
			return(ySpeakMsg);
		}
	}
	else
	{
		yContent = zContentToBeSpoken;
	}
#endif

	sprintf(yTempData, "%s", zContentToBeSpoken.c_str());
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, 
				"Temporary data=(%.*s)", 300, yTempData);
	
	if(strncmp(yTempData, "encoding=", strlen("encoding=")) == 0)
	{
		char *dataStart;
		dataStart = strstr(yTempData, "data=");
		if(dataStart != NULL && strlen(dataStart) > 5)
		{
//			BT-191
//			snprintf(yEncoding, strlen(yTempData) - strlen("encoding=") - strlen(dataStart), 
//					"%s", yTempData + strlen("encoding="));
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, "Removed encoding from speak request.");
			dataStart = dataStart + 5;
			yContent = dataStart;
		}
	
	}
	else
	{
		yContent = zContentToBeSpoken;
	}		

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, "calling sCreateSpeakRequest with"
				"string=(%.*s)", 300, yContent.c_str());

	if ( (p = (char *)strstr(yContent.c_str(), "?xml")) == NULL )
	{
		sprintf(yContentType, "%s", "text/plain");
	}
	else
	{
		sprintf(yContentType, "%s", "application/ssml+xml");
	}

	sprintf(yContentLength, "%d", yContent.length());
	
	header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
	headerList.push_back(header);
    header.setNameValue("Voice-Gender", zParams->gender);
	headerList.push_back(header);
	if(yEncoding && yEncoding[0] != '\0')
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO,
			"yEncoding=%s", yEncoding);
		header.setNameValue("com.aumtech.ttsparam.encoding", yEncoding);
		headerList.push_back(header);
	}

	if(zParams->voiceName != NULL &&
		zParams->voiceName[0] != '\0')
	{
		header.setNameValue("Voice-Name", zParams->voiceName);
		headerList.push_back(header);
	}
	header.setNameValue("Speech-Language", zParams->language);
	headerList.push_back(header);
	header.setNameValue("Content-Type", yContentType);
	headerList.push_back(header);
    header.setNameValue("Content-Length", yContentLength);
	headerList.push_back(header);

	gSrPort[zPort].incrementRequestId();

	ClientRequest speakRequest(
		gMrcpInit.getMrcpVersion(),
		"SPEAK",
		gSrPort[zPort].getRequestId(),
		headerList,
		atoi(yContentLength));

	string speakMsg = speakRequest.buildMrcpMsg();
	ySpeakMsg = speakMsg + yContent;
	
	return(ySpeakMsg);
} // END: sCreateSpeakRequest()

/*------------------------------------------------------------------------------
sSendSTOPToServer():
------------------------------------------------------------------------------*/
int sSendSTOPToServer(int zPort, int *zRecognizerStarted,
						EventInfo *zEventInfo)
{
    static char mod[] = "sSendSTOPToServer";
    string          yEventString    = "";
    int             yCompletionCause= -1;
	string			yContent = "";
	string			yStopMsg;
	string			yRecvMsg;
	string			yServerState;
	string			yEventName;
	int				yCompletionCode;
	int				yStatusCode;
	int				rc;
	MrcpHeader	 	header;
	MrcpHeaderList 	headerList;
	char			yContentLength[16];

	if ( ! *zRecognizerStarted )
	{
		return(0);;
	}

	sprintf(yContentLength, "%d", yContent.length());

	header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
	headerList.push_back(header);

	gSrPort[zPort].incrementRequestId();
	ClientRequest stopRequest(
		gMrcpInit.getMrcpVersion(),
		"STOP",
		gSrPort[zPort].getRequestId(),
		headerList,
		atoi(yContentLength));

	string stopMsg = stopRequest.buildMrcpMsg();
	yStopMsg = stopMsg + yContent;
	
	rc = processMRCPRequest (zPort, yStopMsg, yRecvMsg,
				yServerState, &yStatusCode, &yCompletionCode, yEventName);

	*zRecognizerStarted  = 0;

	if ( rc == YES_EVENT_RECEIVED )
	{
		zEventInfo->statusCode		= yStatusCode;
		zEventInfo->completionCause	= yCompletionCode;
		zEventInfo->eventStr		= yEventName;
		zEventInfo->serverState		= yServerState;
		zEventInfo->content			= yRecvMsg;
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
            "MRCP event response: rc=%d; eventName=(%s), "
            "completionCause=%d; statusCode=%d; serverState=(%s).",
            rc, yEventString.c_str(), yCompletionCause, yStatusCode,
            yServerState.c_str());

		string          yRecvMsg        = "";
		string          yServerState    = "";
		string          yEventString    = "";
		int             yCompletionCause= -1;
		int             yStatusCode     = -1;
		
		rc = readMrcpSocket(zPort, READ_RESPONSE, yRecvMsg,
					yEventString, yServerState,  &yStatusCode, &yCompletionCause);
		if ( rc == 0 )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO, 
				"MRCP event response: rc=%d; eventName=(%s), "
				"completionCause=%d; statusCode=%d; serverState=(%s).",
				rc, yEventString.c_str(), yCompletionCause, yStatusCode,
				yServerState.c_str());
		}
		return(YES_EVENT_RECEIVED);
	}

	return(rc);

} /* END: sSendSTOPToServer() */

/*------------------------------------------------------------------------------
sCheckForMrcpEvent():
------------------------------------------------------------------------------*/
int sCheckForMrcpEvent(int zPort, EventInfo *zEventInfo)
{
	static char		mod[]= "sCheckForMrcpEvent";
	int 			yEventCounter = 0 ;
	int				rc;

    string          yRecvMsg        = "";
    string          yServerState    = "";
    string          yEventString    = "";
    int             yCompletionCause= -1;
    int             yStatusCode     = -1;

	rc = readMrcpSocket(zPort, READ_EVENT, yRecvMsg,
				yEventString, yServerState,  &yStatusCode, &yCompletionCause);
	if ( rc == YES_EVENT_RECEIVED )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, 
			"MRCP event response: rc=%d; eventName=(%s), "
			"completionCause=%d; statusCode=%d; serverState=(%s).",
			rc, yEventString.c_str(), yCompletionCause, yStatusCode,
			yServerState.c_str());

		zEventInfo->statusCode		= yStatusCode;
		zEventInfo->completionCause	= yCompletionCause;
		zEventInfo->eventStr		= yEventString;
		zEventInfo->serverState		= yServerState;
		zEventInfo->content			= yRecvMsg;
	}
//	else
//	{
//		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
//			MRCP_2_BASE, INFO, "No events on queue.");
//	}
	return(rc);
} /* END: sCheckForMrcpEvent() */

/*------------------------------------------------------------------------------
sendSpeakMRCPToDM():
------------------------------------------------------------------------------*/
int sendSpeakMRCPToDM(int zPort, char *zpServerIP, int zRtpPort,
                int zRtcpPort, char *zFile, int zPayloadType,
                int zCodecType, Msg_Speak *zMsgSpeak, long zId)
{
    char                    mod[] = "sendSpeakMRCPToDM";
	struct Msg_SpeakMrcpTts	lMsgSpeak;
    struct MsgToDM          lMsgToDM;
    int                     rc;

    memset(&lMsgToDM, '\0', sizeof(lMsgToDM));
    memset(&lMsgSpeak, '\0', sizeof(lMsgSpeak));

    memcpy(&lMsgSpeak, zMsgSpeak, sizeof(Msg_Speak));
	lMsgSpeak.opcode = DMOP_SPEAKMRCPTTS;	// change the opcode for dynmgr

	if ( zId > 0 )
	{
    	sprintf(lMsgSpeak.resource, "%s|%d|%d|%d|%d|%ld",
		            zpServerIP, zRtpPort, zRtcpPort,
		            zPayloadType, zCodecType, zId);
	}
	else
	{
    	sprintf(lMsgSpeak.resource, "%s|%d|%d|%d|%d|%d",
		            zpServerIP, zRtpPort, zRtcpPort,
		            zPayloadType, zCodecType, 0);
	}

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO, "lMsgSpeak.appPid=%d  lMsgSpeak.resource=(%s) "
		"lMsgSpeak.file=(%s)  lMsgSpeak.deleteFile=(%d)",
		lMsgSpeak.appPid, lMsgSpeak.resource, lMsgSpeak.file,
		lMsgSpeak.deleteFile);

    memcpy(&lMsgToDM, &lMsgSpeak, sizeof(Msg_SpeakMrcpTts));

    rc = sendRequestToDynMgr(zPort, mod, &lMsgToDM);

    return(rc);
} // END: sendSpeakMRCPToDM()

/*------------------------------------------------------------------------------
sendStopToDM():
------------------------------------------------------------------------------*/
int sendStopToDM(int zPort, int zOpcode, Msg_Speak *zMsgSpeak)
{
    char                    mod[] = "sendStopToDM";
	struct Msg_SpeakMrcpTts	lMsgSpeak;
    struct MsgToDM          lMsgToDM;
    int                     rc;

    memset(&lMsgToDM, '\0', sizeof(lMsgToDM));
    memset(&lMsgSpeak, '\0', sizeof(lMsgSpeak));

    memcpy(&lMsgToDM, zMsgSpeak, sizeof(struct MsgToDM));
	lMsgToDM.opcode = zOpcode;	

    rc = sendRequestToDynMgr(zPort, mod, &lMsgToDM);

    return(rc);
} // END: sendStopToDM()

/*------------------------------------------------------------------------------
sGetTtsId():
------------------------------------------------------------------------------*/
void sGetTtsId(int zPort, long *zId)
{
	char		tBuf[64];
    struct timeval  tp;
    struct timezone tzp;

	gettimeofday(&tp, &tzp);
	sprintf(tBuf, "%d%ld", zPort, tp.tv_usec);

	*zId = atol(tBuf);

} // END: sGetTtsId()

/*------------------------------------------------------------------------------
sGetTTSString():
------------------------------------------------------------------------------*/
int sGetTTSString(int zPort, char *zFile, string & zStr)
{
	static char		mod[]="sGetTTSString";
	struct stat		yStat;
	string			yString;
	char			*pData;
	int				fd;
	int				rc;
	
	zStr = "";
    if(stat(zFile, &yStat) < 0)
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "Failed to stat file(%s). [%d: %s] Unable to read TTS string.",
            zFile, errno,strerror(errno));
        return(-1);
    }

	pData = (char *)malloc(yStat.st_size + 10);

	if ( pData == NULL )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "Failed to allocate memory. [%d:%s] Unable to process TTS string.",
            errno, strerror(errno));
        return(-1);
	}

	memset(pData, '\0', yStat.st_size+1);
	
	if ( (fd = open(zFile, O_RDONLY)) == -1)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "Failed to open TTS string file(%s). [%d: %s] "
			"Unable to read TTS string.",
            zFile, errno, strerror(errno));
		free(pData);
        return(-1);
	}

	if ( (rc = read(fd, pData, yStat.st_size+1)) < yStat.st_size )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "Failed to read TTS string from file(%s). [%d: %s] ",
            zFile, errno, strerror(errno));
		free(pData);
		close(fd);
        return(-1);
	}
	close(fd);
	zStr = pData;
	free(pData);

	return(0);
} // END: sGetTTSString()
