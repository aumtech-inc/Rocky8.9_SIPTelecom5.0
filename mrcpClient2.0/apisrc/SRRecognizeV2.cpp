/*----------------------------------------------------------------------------- 
Program:	SRRecognizeV2.cpp
Purpose:	Perform RECOGNIZE logic for MRCPv2 client.
Author:		Aumtech, Inc.
Update:		07/20/06 djb	Created the file.
Update:		01/16/07 rbm	Added Speech-Complete-Timeout and mapped it to 
							trail silence.
Update: 09/25/06 djb MR 2344.  Checked for callactive after IN-PROGRESS received
--------------------------------------------------------------------------------
Notes - typical scenario, but does not always occur in this order:
1)	Send RECOGNIZE to mrcp server.
2)	DMOP_SRRECOGNIZEFROMCLIENT --> dynmgr
	*	sendSRRecognizeToDM(zPort, <mrcpServer IP>, <rtp port[0]>, 
							<rtp port[1]>, <payloadType>, <codecType>);
	*	tells the dynmgr to start collecting / sending speech
3)	DMOP_PROMPTEND <-- dynmgr
	*	tells us to send the START-INPUT-TIMERS to MRCP server,
		if we haven't already.
4)	Now dynmgr should be sending data to the mrcpServer, and we are waiting
		for events.
	* START-OF-SPEECH		<-- mrcpServer
		- send DMOP_SPEECHDETECTED to dynmgr (IP only, not PSTN) 
		  [ call to sendSpeechDetected() ]
		- send DMOP_STOPCH_SENDING to dynmgr (pstn)
		       DMOP_PORTOPERATION/STOP_ACTIVITY to dynmgr (ip) 
		  [ call to stopBarginPhrase() ]
	* RECOGNITION-COMPLETE	<-- mrcpServer
		- recognition is over, send the DMOP_STOPACTIVITY to dynmgr
		  [ call to stopCollectingSpeech() ]
		- if not a dtmf recognition and not SIP Telecom, wait for 
		  the TEC_STREAM event from dynmgr.
	* TEC_STREAM			<-- dynmgr
		- Tells the client that the dynmgr is no longer recording/sending audio.
	* DMOP_DISCONNECT		<-- dynmgr
	* DMOP_EXIT				<-- dynmgr
		- These tell the client that the application is gone.  The client is to 
		  stop processing at this point by sending a STOP to the mrcpServer.
------------------------------------------------------------------------------*/
// #include "parseResult.h"

#include <fstream>
#include <string>
#include "mrcpCommon2.hpp"
//#include "GrammarList.hpp"
#include "mrcp2_HeaderList.hpp"
#include "mrcp2_Request.hpp"
#include "mrcp2_Response.hpp"
#include "mrcp2_Event.hpp"
#include "mrcpSR.hpp"
#include "parseResult.hpp"

using namespace mrcp2client;
using namespace std;

extern int gCanProcessReaderThread;
extern int gCanProcessSipThread;
extern int gCanProcessMainThread;


extern "C"
{
	#include <stdio.h>
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

#ifdef SDM
	extern int gTecStreamSw[MAX_PORTS];
// struct   MsgToApp        gRecogResponse;
#endif

static const string	RECOGNITIONCOMPLETE_STR	= "RECOGNITION-COMPLETE";
static const string	INTERPRETATIONCOMPLETE_STR	= "INTERPRETATION-COMPLETE";
static const string	RECORDCOMPLETE_STR	= "RECORD-COMPLETE";
static const string	STARTOFINPUT_STR		= "START-OF-INPUT";

XmlResultInfo           parsedResults[MAX_PORTS];
string					xmlResult[MAX_PORTS];

// int						eventFirstTime;
// int						outofsyncEventReceived;
// EventInfoChar			outofsyncEventInfo;
// Mrcp2_Event				outofsyncMrcpv2Event;
// 

/*
** Function prototypes
*/
int sReserveAndLoadGrammars(int zPort, int *zReleaseResourceHere,
									int *zUnloadGrammarsHere);
int sRecognitionCleanup(int zPort, int zUnloadGrammarsHere,
									int zReleaseResourceHere);
long getTimeDiffInMiliSeconds(struct timeval *zpTimeVal1, 
								struct timeval *zpTimeVal2);
string sCreateRecognizeRequest(int zPort,
				int zLeadSilence, int zTrailSilence, int zTotalTime);
string sCreateInterpretRequest(int zPort);
string sCreateRecordRequest(int zPort,
				int zLeadSilence, int zTrailSilence, int zTotalTime);
string sCreateInterpretRequest(int zPort);

string sCreateSetParamRequestDefault(int zPort,
				int zLeadSilence, int zTrailSilence, int zTotalTime);

int sSendSTOPToServer(int zPort, int *zRecognizerStarted);
int sLoadAllResults(int zPort, EventInfo *zEventInfo,
												int *zNumAlternatives);
int sCheckForMrcpEvent(int zPort, EventInfo *zEventInfo, Mrcp2_Event     &mrcpEvent);

extern int checkMrcpRequest(int zPort, int zInputFlag);
int sStartTheMrcpServerTimers(int zPort);

extern int	arePhrasesPlayingOrScheduled(int zPort);
extern int	isDTMFDuringSpeechRec(int zPort);
extern int	stopCollectingSpeech(int zPort);
extern int	stopBargeInPhrase(int zPort);
extern void	sendSpeechDetected(int zPort);
extern int	isPhraseBargedIn(int zPort);
extern int	sendRecognizeToDM(int zPort, const char *zpServerIP, int zRtpPort,
				int zRtcpPort, char *zUtteranceFile, int zPayloadType,
				int zCodecType);
extern int readAndProcessAppRequest(int zInputFlag);

/*------------------------------------------------------------------------------
SRRecognizeV2():
------------------------------------------------------------------------------*/
int SRRecognizeV2(int zPort, int zAppPid, SRRecognizeParams *zParams, char *zWaveformUri)
{
	static char		mod[] = "SRRecognizeV2";
	time_t	 		lCurrentTime	= 0;
	time_t	 		lStartTime		= 0;
	time_t 			tempInt;
	int 			rc2 = 0;
	int 			rc = 0;
	int				yStartOfSpeechDetected;
	int				yRecognitionCompleteReceived;
	int				yRecognizerStarted;
	int				yTimedOut;
	int				yReleaseResourceHere;
	int				yUnloadGrammarsHere;
	int				yNumResults;

	int				yInterruptOption;
	int				yLeadSilence;
	int				yTrailSilence;
	int				yTotalTime;
	int				yBargein;

	int				yRecordingDone;

	int				lGotDTMF = 0;
	struct timeval	lTimeVal;
	long			lTimeDiffInMiliSec;
	EventInfo		yEvent;
	int 			lStartTimerDone = 0;
	Mrcp2_Event     mrcpEvent;

	OutOfSyncEvent	*pOutOfSyncEvent; 
		
    if ( ! gSrPort[zPort].IsTelSRInitCalled() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
	   		"SRSetParameter failed: Must call SRInit first.");
#ifdef SDM
		gTecStreamSw[zPort] = 1;
#endif
		return(TEL_FAILURE);
    }
	zParams->numResults = 0;

	yStartOfSpeechDetected				= 0;
	yRecognitionCompleteReceived		= 0;
	yRecognizerStarted					= 0;
	yTimedOut							= 0;
	yUnloadGrammarsHere					= 0;
	yReleaseResourceHere				= 0;
	yRecognizerStarted					= 0;
	memset((XmlResultInfo *)&(parsedResults[zPort]), '\0',
				sizeof(XmlResultInfo));


    if ( ! gSrPort[zPort].IsCallActive() )
    {
#ifdef SDM
		gTecStreamSw[zPort] = 1;
#endif
        return(TEL_DISCONNECTED);
    }
	
	yInterruptOption	= zParams->interruptOption;
	yLeadSilence		= zParams->leadSilence;
	yTrailSilence		= zParams->trailSilence;
	yTotalTime			= zParams->totalTime;
	yBargein			= zParams->bargein;

	if (yTrailSilence >= 1000000)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, WARN, 
			"Warning: Trailing silence (%d) is too large.  "
			"It must be less than 100000 ms.  Setting it to 999999.",
			yTrailSilence);

		yTrailSilence = 990000;
	}

	// This gives backend server extra 3 seconds to generate no-input.
	// yLeadSilence = yLeadSilence + 3000;

	yInterruptOption	= zParams->interruptOption;
	
	if ( yLeadSilence < 250 )
	{
		yLeadSilence = 250;
	}
	
	if ( yTotalTime < 250 )
	{
		yTotalTime = 250;
	}

	if(yTotalTime < (yLeadSilence / 1000) )
	{
		yTotalTime = yLeadSilence / 1000 ;
	}

//	grammarList[zPort].printGrammarList();

	if ( ( ! gSrPort[zPort].IsLicenseReserved() ) &&
	     ( ! gSrPort[zPort].IsGrammarAfterVAD() ) )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
				"Error: No resource is reserved and $GRAMMAR_AFTER_VAD is not "
				"set.  Unable to perform recognition.");
#ifdef SDM
		gTecStreamSw[zPort] = 1;
#endif
    	if ( ! gSrPort[zPort].IsCallActive() )
	    {
	        return(TEL_DISCONNECTED);
	    }
		return(TEL_FAILURE);
	}
	gSrPort[zPort].setRecognitionARecord(false);		// MR 4949

	/*DDN: Corrected the sequence of zReleaseResourceHere and zUnloadGrammarsHere*/
	rc = sReserveAndLoadGrammars(zPort, &yReleaseResourceHere, &yUnloadGrammarsHere);
	if(rc < 0)
	{
#ifdef SDM
		gTecStreamSw[zPort] = 1;
#endif
		sRecognitionCleanup(zPort, yUnloadGrammarsHere, yReleaseResourceHere);
    	if ( ! gSrPort[zPort].IsCallActive() )
	    {
	        return(TEL_DISCONNECTED);
	    }
		return(TEL_FAILURE);
	}

#if 0
	string ySetParamRequest = sCreateSetParamRequestDefault(zPort, yLeadSilence,
								yTrailSilence, yTotalTime);
#endif

	string yRecogMsg = "";
	char ySimulateInterpretFileName[64] = "";
	char ySimulateRecordFileName[64] = "";
	sprintf(ySimulateInterpretFileName, "%s", "/tmp/.interpret.trigger");
	sprintf(ySimulateRecordFileName, "%s", "/tmp/.record.trigger");

	if(access(ySimulateInterpretFileName, F_OK) == 0)
	{
		yRecogMsg = sCreateInterpretRequest(zPort);
	}
	else if(access(ySimulateRecordFileName, F_OK) == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			SR_BASE, INFO, 
			"Found %s calling sCreateRecordRequest.", ySimulateRecordFileName);
		yRecogMsg = sCreateRecordRequest(zPort, yLeadSilence,
						yTrailSilence, yTotalTime);
		gSrPort[zPort].setRecognitionARecord(true);		// MR 4949
	}
	else
	{
		yRecogMsg = sCreateRecognizeRequest(zPort, yLeadSilence,
						yTrailSilence, yTotalTime);
	}

	if ( yRecogMsg.empty()) 
	{
#ifdef SDM
		gTecStreamSw[zPort] = 1;
#endif
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
			"Empty request generated.  Unable to send Recognition request.");
    	if ( ! gSrPort[zPort].IsCallActive() )
	    {
	        return(TEL_DISCONNECTED);
	    }
		return(TEL_FAILURE);
	}


	gSrPort[zPort].setOutOfSyncEventReceived(0);
	gSrPort[zPort].clearOutOfSyncEvent();

	/*
	** Now we have a reserved resource and built the RECOGNIZE. Send/process it.
	*/
	string		yRecvMsg;
	string		yServerState;
	int			yCompletionCode;
	int			yStatusCode;

#if 0
	rc = processMRCPRequest (zPort, ySetParamRequest, yRecvMsg,
				yServerState, &yStatusCode, &yCompletionCode);
#endif

	rc = processMRCPRequest (zPort, yRecogMsg, yRecvMsg,
				yServerState, &yStatusCode, &yCompletionCode);
    mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_VERBOSE, MRCP_2_BASE, INFO, "%d = processMRCPRequest()", rc);

	if ( rc == TEL_NEW_MRCP_CONNECTION )
	{
		return(rc);
	}

	if ( yServerState != "IN-PROGRESS" )
	{
#ifdef SDM
		gTecStreamSw[zPort] = 1;
#endif
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
			"Recognition request failed; no IN-PROGRESS response received from MRCP Server. [yServerState is (%s)]",
			yServerState.c_str());
    	if ( ! gSrPort[zPort].IsCallActive() )
	    {
	        return(TEL_DISCONNECTED);
	    }
		return(-1);
	}
	if ( yStatusCode != 200 ) 
	{
#ifdef SDM
		gTecStreamSw[zPort] = 1;
#endif
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
			"Recognition request failed with status code %d.", yStatusCode);
    	if ( ! gSrPort[zPort].IsCallActive() )
	    {
	        return(TEL_DISCONNECTED);
	    }
		return(-1);
	}
	yRecognizerStarted = 1;
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO, 
		"Recogition successfully initiated with the MRCP Server. "
		" callAcitve()=%d  IsDisconnectReceived()=%d IsStopProcessingRecognition()=%d",
			gSrPort[zPort].IsCallActive(), gSrPort[zPort].IsDisconnectReceived(),
			gSrPort[zPort].IsStopProcessingRecognition());

    if ( ( ! gSrPort[zPort].IsCallActive() ) ||
         ( gSrPort[zPort].IsDisconnectReceived() ) ||
         ( gSrPort[zPort].IsStopProcessingRecognition() ) )
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
                MRCP_2_BASE, INFO, 
                "Call in no longer active [%d, %d, %d].  Halting recognition.",
				gSrPort[zPort].IsCallActive(),
				gSrPort[zPort].IsDisconnectReceived(),
				gSrPort[zPort].IsStopProcessingRecognition());

        rc = sSendSTOPToServer(zPort, &yRecognizerStarted);
		if ( rc == TEL_NEW_MRCP_CONNECTION )
		{
			return(TEL_NEW_MRCP_CONNECTION);
		}
        return(-1);
    }

	gSrPort[zPort].setArePhrasesPlayingOrScheduled(true);
	gSrPort[zPort].setStopProcessingRecognition(false);
	rc = sendRecognizeToDM(zPort, gSrPort[zPort].getMrcpConnectionIP().c_str(),
			gSrPort[zPort].getServerRtpPort(),
			gSrPort[zPort].getServerRtpPort() + 1,
			"", atoi(gSrPort[zPort].getPayload().c_str()), 
			atoi(gSrPort[zPort].getCodec().c_str()));
	if ( rc < 0 )
	{
		rc = sSendSTOPToServer(zPort, &yRecognizerStarted);
		if ( rc == TEL_NEW_MRCP_CONNECTION )
		{
			return(TEL_NEW_MRCP_CONNECTION);
		}
    	if ( ! gSrPort[zPort].IsCallActive() )
	    {
	        return(TEL_DISCONNECTED);
	    }
		return(TEL_FAILURE);
	}

	lStartTime				= -1;
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO, "Entering Loop.");
	for (;;)
	{
		//Look & process any Requests from DM or API.
		if ( gSrPort[zPort].IsStopProcessingRecognition() )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
				MRCP_2_BASE, INFO, 
				"StopProcessingRecognition is set.  Halting recognition.");
			rc = sSendSTOPToServer(zPort, &yRecognizerStarted);
			if ( rc == TEL_NEW_MRCP_CONNECTION )
			{
				return(TEL_NEW_MRCP_CONNECTION);
			}
			stopCollectingSpeech(zPort);
			break;
		}

		if (  gSrPort[zPort].IsOutOfSyncEventReceived() )   
		{
			rc = YES_EVENT_RECEIVED;
			pOutOfSyncEvent = gSrPort[zPort].getOutOfSyncEvent();

			yEvent.statusCode		= pOutOfSyncEvent->statusCode;
			yEvent.completionCause	= pOutOfSyncEvent->completionCause;
			yEvent.eventStr.assign(pOutOfSyncEvent->eventStr);
			yEvent.serverState.assign(pOutOfSyncEvent->serverState);
			yEvent.content.assign(pOutOfSyncEvent->content);

			mrcpEvent = pOutOfSyncEvent->mrcpV2Event; 

			gSrPort[zPort].setOutOfSyncEventReceived(0);
		}
		else
		{
			rc = sCheckForMrcpEvent(zPort, &yEvent, mrcpEvent);
		}

		if ( ( rc == -1 ) || ( rc == TEL_NEW_MRCP_CONNECTION ) )
		{
			yRecognizerStarted = 0;
			stopCollectingSpeech(zPort);
			break;
		}
		if ( rc == YES_EVENT_RECEIVED )
		{
			if ( yEvent.eventStr == STARTOFINPUT_STR)
			{
				yStartOfSpeechDetected = 1;
				(void) sendSpeechDetected(zPort);

		
				if ( ( yBargein == YES ) &&
				     ( gSrPort[zPort].IsPhrasesPlayingOrScheduled() ) )
				{
					(void) stopBargeInPhrase(zPort);
				}
				gSrPort[zPort].setArePhrasesPlayingOrScheduled(false);
			}
			else if ( yEvent.eventStr == RECOGNITIONCOMPLETE_STR ||
						 yEvent.eventStr == INTERPRETATIONCOMPLETE_STR ||
						yEvent.eventStr == RECORDCOMPLETE_STR)
			{
				stopBargeInPhrase(zPort); 
				stopCollectingSpeech(zPort);
				yRecognitionCompleteReceived = 1;
				yRecognizerStarted = 0;
				break;
			}
		}

		if ( ( ! gSrPort[zPort].IsPhrasesPlayingOrScheduled() ) &&
		     ( ! yStartOfSpeechDetected ) )
		{
			if ( lStartTime == -1 )
			{
				rc = sStartTheMrcpServerTimers(zPort);
				if ( rc == TEL_NEW_MRCP_CONNECTION )
				{
					return(TEL_NEW_MRCP_CONNECTION);
				}
				if ( rc == 0 )
				{
					time(&lStartTime);
				}
				else
				{
					break;
				}
				mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
						MRCP_2_BASE, INFO, "Continuing after starting timers.");
				continue;
			}
		}

		if(lStartTime != -1)		// check zTotalTime; may have to add
		{							// lead time.
			time(&lCurrentTime);
			tempInt = lCurrentTime - lStartTime;
			if(tempInt >= yTotalTime)
			{
				stopCollectingSpeech(zPort);
				rc = sSendSTOPToServer(zPort, &yRecognizerStarted);
				if ( rc == TEL_NEW_MRCP_CONNECTION )
				{
					return(TEL_NEW_MRCP_CONNECTION);
				}
				yRecognizerStarted = 0;
				if (  gSrPort[zPort].IsOutOfSyncEventReceived() )   
				{
					pOutOfSyncEvent = gSrPort[zPort].getOutOfSyncEvent();
					yEvent.eventStr.assign(pOutOfSyncEvent->eventStr);
					if ( yEvent.eventStr == RECOGNITIONCOMPLETE_STR ||
							yEvent.eventStr == INTERPRETATIONCOMPLETE_STR ||
								yEvent.eventStr == RECORDCOMPLETE_STR)
					{
						yEvent.statusCode		= pOutOfSyncEvent->statusCode;
						yEvent.completionCause	= pOutOfSyncEvent->completionCause;
						yEvent.serverState.assign(pOutOfSyncEvent->serverState);
						yEvent.content.assign(pOutOfSyncEvent->content);
		
						mrcpEvent = pOutOfSyncEvent->mrcpV2Event; 
						gSrPort[zPort].setOutOfSyncEventReceived(0);
						yRecognitionCompleteReceived = 1;
					}
					else
					{
						yTimedOut = 1;
					}
				}
				else
				{
					yTimedOut = 1;
				}
				break;
			}
		}
//		usleep(100);
	}
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO, "Out of Loop.");

	zParams->numResults = 0;
	if( ( yRecognizerStarted ) &&
	    ( ! yRecognitionCompleteReceived ) &&
	    ( yStartOfSpeechDetected ) )
	{
		stopCollectingSpeech(zPort);
	}

	if ( rc == TEL_NEW_MRCP_CONNECTION )
	{
		return(TEL_NEW_MRCP_CONNECTION);
	}

	if ( yRecognizerStarted )
	{
		rc = sSendSTOPToServer(zPort, &yRecognizerStarted);
		if ( rc == TEL_NEW_MRCP_CONNECTION )
		{
			return(TEL_NEW_MRCP_CONNECTION);
		}
		yRecognizerStarted = 0;
		if (  gSrPort[zPort].IsOutOfSyncEventReceived() )   
		{
			pOutOfSyncEvent = gSrPort[zPort].getOutOfSyncEvent();
			yEvent.eventStr.assign(pOutOfSyncEvent->eventStr);
			if ( yEvent.eventStr == RECOGNITIONCOMPLETE_STR ||
					yEvent.eventStr == INTERPRETATIONCOMPLETE_STR ||
						yEvent.eventStr == RECORDCOMPLETE_STR)
			{
				yEvent.statusCode		= pOutOfSyncEvent->statusCode;
				yEvent.completionCause	= pOutOfSyncEvent->completionCause;
				yEvent.serverState.assign(pOutOfSyncEvent->serverState);
				yEvent.content.assign(pOutOfSyncEvent->content);

				mrcpEvent = pOutOfSyncEvent->mrcpV2Event; 
				gSrPort[zPort].setOutOfSyncEventReceived(0);
				yRecognitionCompleteReceived = 1;
			}
			else
			{
				yTimedOut = 1;
			}
		}
	}
	sRecognitionCleanup(zPort, yUnloadGrammarsHere, yReleaseResourceHere);

    if ( ! gSrPort[zPort].IsCallActive() )
    {
        return(TEL_DISCONNECTED);
    }

	// at this point, we know we have a recognition-complete; success
	yNumResults = 0;
	if ( ! yRecognitionCompleteReceived )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, WARN, 
			"(%s) was not received.  No results are available.",
			"RECOGNITION-COMPLETE");
		if ( yTimedOut ) 
		{
			rc = TEL_TIMEOUT;
		}
		else
		{
			rc = TEL_FAILURE;
		}
	}
	else
	{
		rc = sLoadAllResults(zPort, &yEvent, &yNumResults);
	}

	zParams->numResults = yNumResults;
	
	string zParameterName = "Waveform-URI";	

	MrcpHeaderList headerList = mrcpEvent.getHeaderList(); // headerList

	while(!headerList.empty())
	{
		MrcpHeader      header;
		header = headerList.front();
			
		string name = header.getName();
		string value = header.getValue();
		if (name == zParameterName)
		{
			sprintf(zWaveformUri, "%s", value.c_str());
		}

		headerList.pop_front();

	} // END: headerList.empty()
	
	return(rc);
} /* END: SRRecognizeV2() */

/*------------------------------------------------------------------------------
sReserveAndLoadGrammars():
------------------------------------------------------------------------------*/

int sReserveAndLoadGrammars(int zPort, int *zReleaseResourceHere,
											int *zUnloadGrammarsHere)
{
	static char						mod[]="sReserveAndLoadGrammars";
	int								rc;
//	list <GrammarData>::iterator	yIter;
//	GrammarData						yGrammarData;
	int								yAtleastOneGrammarLoaded = 0;
	int								grammarType;
	string							grammarName;
	string							grammarData;
	float							zWeight = 1.0;
	
	if ( ( ! gSrPort[zPort].IsLicenseReserved() ) &&
	     ( gSrPort[zPort].IsGrammarAfterVAD() ) )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, 
			"No license is reserved and GrammarAfterVAD is set.  Attempting to "
			"reserve a resource and load grammars.");
		if ((rc = srGetResource(zPort)) == 0)
		{
			*zReleaseResourceHere				= 1;
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, 
				"Failed to get a speech resource. rc=%d.", rc);
			return(-1);
		}

//		yIter = grammarList[zPort].begin();
		yAtleastOneGrammarLoaded = 0;
		if ( ! grammarList[zPort].getNext(GRAMMARLIST_START_FROM_FIRST,
					&grammarType, grammarName, grammarData) )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, 
				"No grammars found in grammar list.  Unable to perform "
				"recognition");
			return(-1);
		}

		for (;;)
		{
			rc = srLoadGrammar(zPort, SR_GRAM_ID,
				grammarName.c_str(),
				grammarData.c_str(), LOAD_GRAMMAR_NOW, zWeight);
			if (rc != 0)
			{
				if ( gSrPort[zPort].IsFailOnGrammarLoad() )
				{
					mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
						REPORT_NORMAL, MRCP_2_BASE, ERR, 
						"$SR_FAIL_ON_GRAMMAR_LOAD is enabled.  Returning "
						"failure.");
					grammarList[zPort].removeAll();
					return(RC_NO_GRAMMARS);
				}
				mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
					REPORT_NORMAL, MRCP_2_BASE, INFO, 
					"WARNING: Grammar (%s:%s) will not be activated in the "
					"RECOGNIZE.", grammarName.c_str(), grammarData.c_str());
//				yGrammarData.type	= -1;
			}

			yAtleastOneGrammarLoaded = 1;
			if ( ! grammarList[zPort].getNext(GRAMMARLIST_NEXT,
						&grammarType, grammarName, grammarData) )
			{
				break;
			}
		}

		if ( yAtleastOneGrammarLoaded == 0 )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
					MRCP_2_BASE, ERR, "No grammars were successfully loaded.  "
					"No recognition will be performed.");
			return(-1);
		}
		*zUnloadGrammarsHere					= 1;
	}
	return(0);

} /* END: sReserveAndLoadGrammars() */


/*------------------------------------------------------------------------------
sSendSTOPToServer():
------------------------------------------------------------------------------*/
int sSendSTOPToServer(int zPort, int *zRecognizerStarted)
{
	static char mod[]="sSendSTOPToServer";
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
		return(0);
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
				yServerState, &yStatusCode, &yCompletionCode);
    mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_VERBOSE, MRCP_2_BASE, INFO, "Returning TEL_NEW_MRCP_CONNECTION(%d)",
            TEL_NEW_MRCP_CONNECTION);

	*zRecognizerStarted  = 0;

	return(rc);

} /* END: sSendSTOPToServer() */

/*------------------------------------------------------------------------------
sRecognitionCleanup():
	Perform the cleaning of grammars and sending the BYE, if necessary.
------------------------------------------------------------------------------*/
int sRecognitionCleanup(int zPort, int zUnloadGrammarsHere,
									int zReleaseResourceHere)
{
	int						rc;

	if ( zUnloadGrammarsHere )
	{
		grammarList[zPort].removeAll();
	}

	if ( zReleaseResourceHere ) 
	{
		rc = srReleaseResource(zPort);
	}

	return(0);
} /* END: sRecognitionCleanup() */

/*-----------------------------------------------------------------------------
sLoadAllResults():
------------------------------------------------------------------------------*/
int sLoadAllResults(int zPort, EventInfo *zEventInfo,
												int *zNumAlternatives)
{
	char		mod[] = "sLoadAllResults";
	int			yRc;
	int			i;
	int			j;
	char		yTmpGrammarName[128];
	char		yErrorMsg[256];
	static char	ySessionMsg[]="session:";

	if ( zEventInfo->completionCause != 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received (%s); recognition failed (%d).",
			RECOGNITIONCOMPLETE_STR.c_str(),
			zEventInfo->completionCause);

		*zNumAlternatives = parsedResults[zPort].numResults = 0;
		switch( zEventInfo->completionCause )
		{
			case 1:
				return(TEL_SR_NOTHING_RECOGNIZED);
				break;
			case 3:
				return(TEL_SR_SPOKE_TOO_LONG);
				break;
			case 2:
			case 15:
				return(TEL_TIMEOUT);
				break;
			default:
				return(TEL_FAILURE);
				break;
		}
	}

    if ( ! gSrPort[zPort].getHideDTMF() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Recognition successfully received results. (%s)", 
			zEventInfo->content.c_str());
	}

	parsedResults[zPort].numResults = 1;
	*zNumAlternatives = parsedResults[zPort].numResults;
	
	if ( zEventInfo->eventStr == RECORDCOMPLETE_STR )
	{
		gSrPort[zPort].setRecognitionARecord(true);		// MR 4949
		return(0);
	}

//	parsedResults[zPort].xmlResult = zEventInfo->content;  // core dump???
	xmlResult[zPort] = zEventInfo->content;

	memset((char *)yErrorMsg, '\0', sizeof(yErrorMsg));

	if ((yRc = parseXMLResultString(zPort, xmlResult[zPort].c_str(),
				xmlResult[zPort].length(),
				&(parsedResults[zPort]), yErrorMsg)) == -1)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, "Failed to parse result (%s).  [%d, %s]",
			xmlResult[zPort].c_str(), yRc, yErrorMsg);
		return(-1);
	}
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Completed loading results.");

	if (strncmp(parsedResults[zPort].resultGrammar,
					"session:", strlen("session:")) == 0)
	{
		sprintf(yTmpGrammarName, "%s",
			&(parsedResults[zPort].resultGrammar[strlen("session:")]));

		sprintf(parsedResults[zPort].resultGrammar, "%s", yTmpGrammarName);
	}

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO,
		"result:confidence:(%d)", parsedResults[zPort].confidence);
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO,
		"result:numInstanceChildren:(%d)",
		parsedResults[zPort].numInstanceChildren);
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO,
		"result:resultGrammar:(%s)", parsedResults[zPort].resultGrammar);
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO,
		"result:inputMode:(%s)", parsedResults[zPort].inputMode);
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, INFO,
		"result:literalTimings:(%s)", parsedResults[zPort].literalTimings);

    if ( ! gSrPort[zPort].getHideDTMF() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO,
			"result:inputValue:(%s)", parsedResults[zPort].inputValue);
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO,
			"result:instance:(%s)", parsedResults[zPort].instance);
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO,
			"result:numResults:(%d)", parsedResults[zPort].numResults);
	}

	for (i = 0; i < parsedResults[zPort].numInstanceChildren; i++)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "result.instance[%d].name:(%s)", i,
			parsedResults[zPort].instanceChild[i].instanceName);
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "result.instance[%d].value:(%s)", i,
			parsedResults[zPort].instanceChild[i].instanceValue);
	}

    if ( ! gSrPort[zPort].getHideDTMF() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO,
			"result:xmlResult:(%.*s)", 500, xmlResult[zPort].c_str());
	}

	return(0);

} /* END: sLoadAllResults() */

/*------------------------------------------------------------------------------
sCreateSetParamRequestDefault():
------------------------------------------------------------------------------*/
string sCreateSetParamRequestDefault(int zPort, int zLeadSilence,
								int zTrailSilence, int zTotalTime)
{
	static char	mod[] = "sCreateSetParamRequestDefault";
	int		rc = 0;
//	list <GrammarData>::iterator    iter;
//	GrammarData                     yGrammarData;
	MrcpHeader					 	header;
	MrcpHeaderList				 	headerList;
	int                             yCounter;
	string							yContent = "";
	char							yLeadSilence[16];
	char							yTotalTime[16];
	char							yTrailSilence[16];
	char							yContentLength[16];
	string							yLanguage;
	string							ySensitivity;
	string							yRecognitionMode;
	string							ySpeedAccuracy;
	string							yMaxNBest;
	string							yInterdigitTimeout;
	string							yTermTimeout;
	string							yTermChar;
	string							yIncompleteTimeout;
	string							yConfidenceThreshold;
	string							yRecogMsg;
	int								grammarType;
	string							grammarName;
	string							grammarData;

	if ( ! grammarList[zPort].getNext(GRAMMARLIST_START_FROM_FIRST,
					&grammarType, grammarName, grammarData) )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, 
				"No grammars found in grammar list.  Unable to perform "
				"recognition");
		return(NULL);
	}

	sprintf(yContentLength, "%d", yContent.length());
	sprintf(yLeadSilence, "%d", zLeadSilence);
	sprintf(yTotalTime, "%d", zTotalTime);
	sprintf(yTrailSilence, "%d", zTrailSilence);
	yLanguage = gSrPort[zPort].getLanguage().c_str();
	ySensitivity = gSrPort[zPort].getSensitivity().c_str();
	yRecognitionMode = gSrPort[zPort].getRecognitionMode().c_str();
	yInterdigitTimeout = gSrPort[zPort].getInterdigitTimeout().c_str();
	yTermTimeout = gSrPort[zPort].getTermTimeout().c_str();
	yTermChar = gSrPort[zPort].getTermChar().c_str();
	yIncompleteTimeout = gSrPort[zPort].getIncompleteTimeout().c_str();
	yConfidenceThreshold = gSrPort[zPort].getConfidenceThreshold().c_str();
	
	header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
	headerList.push_back(header);

    header.setNameValue("Content-Type", "text/uri-list");
	headerList.push_back(header);

	header.setNameValue("Start-Input-Timers", "false");
	headerList.push_back(header);

    header.setNameValue("No-Input-Timeout", yLeadSilence);
	headerList.push_back(header);

    header.setNameValue("Recognition-Timeout", yTotalTime);
	headerList.push_back(header);

    header.setNameValue("Speech-Complete-Timeout", yTrailSilence);
	headerList.push_back(header);
	
	if ( yIncompleteTimeout.length() > 0 )
	{
    	header.setNameValue("Speech-Incomplete-Timeout", yIncompleteTimeout.c_str());
		headerList.push_back(header);
	}

	if ( yTermTimeout.length() > 0 )
	{
    	header.setNameValue("DTMF-Term-Timeout", yTermTimeout.c_str());
		headerList.push_back(header);
	}
	else
	{
    	header.setNameValue("DTMF-Term-Timeout", yTrailSilence);
		headerList.push_back(header);
	}
	
	if ( yTermChar.length() > 0 )
	{
    	header.setNameValue("DTMF-Term-Char", yTermChar.c_str());
		headerList.push_back(header);
	}

	if ( yInterdigitTimeout.length() > 0 )
	{
    	header.setNameValue("DTMF-Interdigit-Timeout", yInterdigitTimeout.c_str());
		headerList.push_back(header);
	}
	else
	{
    	header.setNameValue("DTMF-Interdigit-Timeout", yTrailSilence);
		headerList.push_back(header);
	}
#if 0
	if ( yLanguage.length() > 0 )
	{
    	header.setNameValue("speech-language", yLanguage.c_str());
		headerList.push_back(header);
	}
	else
	{
    	header.setNameValue("speech-language", "en-US");
		headerList.push_back(header);
	}
#endif

	if ( ySensitivity.length() > 0 )
	{
		header.setNameValue("Sensitivity-Level", ySensitivity.c_str());
		headerList.push_back(header);
	}


	if ( yRecognitionMode.length() > 0 )
	{
		header.setNameValue("Recognition-Mode", yRecognitionMode.c_str());
		headerList.push_back(header);
	}

#if 0
	else
	{
		header.setNameValue("Sensitivity-Level", "0.5");
		headerList.push_back(header);
	}
#endif


	if ( yConfidenceThreshold.length() > 0 )
	{
		header.setNameValue("Confidence-Threshold", yConfidenceThreshold.c_str());
		headerList.push_back(header);
	}
	else
	{
    	header.setNameValue("Confidence-Threshold", "0.5");
		headerList.push_back(header);
	}
	
	if ( ySpeedAccuracy.length() > 0 )
	{
		header.setNameValue("Speed-Vs-Accuracy", ySpeedAccuracy.c_str());
		headerList.push_back(header);
	}

	if ( yMaxNBest.length() > 0 )
	{
		header.setNameValue("n-best-list-length", yMaxNBest.c_str());
		headerList.push_back(header);
	}
	else
	{
    	header.setNameValue("n-best-list-length", "1");
		headerList.push_back(header);
	}


    header.setNameValue("Content-Length", yContentLength);
	headerList.push_back(header);

	gSrPort[zPort].incrementRequestId();
	ClientRequest recognizeRequest(
		gMrcpInit.getMrcpVersion(),
		"SET-PARAM",
		gSrPort[zPort].getRequestId(),
		headerList,
		atoi(yContentLength));

	string recogMsg = recognizeRequest.buildMrcpMsg();
	yRecogMsg = recogMsg + yContent;
	
	return(yRecogMsg);
} // END: sCreateSetParamRequestDefault()


/*------------------------------------------------------------------------------
sCreateRecognizeRequest():
------------------------------------------------------------------------------*/
string sCreateRecognizeRequest(int zPort, int zLeadSilence,
								int zTrailSilence, int zTotalTime)
{
	static char	mod[] = "sCreateRecognizeRequest";
	int		rc = 0;
//	list <GrammarData>::iterator    iter;
//	GrammarData                     yGrammarData;
	MrcpHeader					 	header;
	MrcpHeaderList				 	headerList;
	int                             yCounter;
	string							yContent = "";
	char							yLeadSilence[16];
	char							yTrailSilence[16];
	char							yTotalTime[16];
	char							yContentLength[16];
	string							yLanguage;
	string							ySensitivity;
	string							yRecognitionMode;
	string							ySpeedAccuracy;
	string							yMaxNBest;
	string							yInterdigitTimeout;
	string							yTermTimeout;
	string							yTermChar;
	string							yIncompleteTimeout;
	string							yConfidenceThreshold;
	string							yRecogMsg;
	int								grammarType;
	string							grammarName;
	string							grammarData;
    int                             waveformURI;

#if 0
	int								ySimulateInlines = 0;
	string							ySimulatedGrammarData;
	char							ySimulatedGrammarContentType[64];
	char							ySimulatedGrammarContentId[64];
	char							yLine[256];
	char							ySimulateSRGrammarsFileName[128];

	sprintf(ySimulateSRGrammarsFileName, "%s", "/tmp/simulateSRGrammars.txt");

	if(access(ySimulateSRGrammarsFileName, F_OK) == 0)
	{
		ySimulateInlines = 1;

		FILE* ySimulateDataFp = fopen(ySimulateSRGrammarsFileName, "r");

		/*Reset the flag ySimulateInlines if file is there, but not readable.*/
		if(!ySimulateDataFp)
		{
			ySimulateInlines = 0;
		}
		else
		{
			yLine[0] = 0;

			int yLineCounter = 0;	

			while ( fgets(yLine, sizeof(yLine)-1, ySimulateDataFp))
			{
				if(yLine[0])
				{
					/*Read first line for "Content-Type" */
					if(yLineCounter == 0)
					{
						yLine[strlen(yLine) - 1] = 0;
						sprintf(ySimulatedGrammarContentType, "%s", yLine);
						yLineCounter++;

					}
					else if(yLineCounter == 1)
					{
						yLine[strlen(yLine) - 1] = 0;
						sprintf(ySimulatedGrammarContentId, "%s", yLine);
						yLineCounter++;

					}
					else
					{
						/*Read subsequent lines for grammar data */
						ySimulatedGrammarData = ySimulatedGrammarData + yLine;
					}
				}
			}

			yContent = ySimulatedGrammarData;

			fclose(ySimulateDataFp);

			/*Reset the flag ySimulateInlines if file is there, but is empty.*/
			if(yContent.length() == 0)
			{
				ySimulateInlines = 0;
			}
		}
	}

	if(!ySimulateInlines)
	{

#endif
		if ( ! grammarList[zPort].getNextActiveGrammar(GRAMMARLIST_START_FROM_FIRST,
					&grammarType, grammarName, grammarData) )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
					MRCP_2_BASE, ERR, 
					"No grammars found in grammar list.  Unable to perform "
					"recognition");
			return("");
		}

		for (;;)
		{
			yContent = yContent + "session:" + grammarName + "\r\n";
	
			if ( ! grammarList[zPort].getNextActiveGrammar(GRAMMARLIST_NEXT, &grammarType, grammarName, grammarData) )
			{
				break;
			}
		}
#if 0
	}
	else	/*Else simulate the contents as if it were grammars attached to RECOGNIZE*/
	{
	}
#endif

    waveformURI = gMrcpInit.getwaveformURI();
#if 0

	// build content
	iter = grammarList[zPort].begin();
	while ( iter != grammarList[zPort].end() )
	{
		yGrammarData = *iter;
		yContent = yContent + "session:" + yGrammarData.grammarName + "\r\n";
		iter++;
	}
#endif
	sprintf(yContentLength, "%d", yContent.length());
	sprintf(yLeadSilence, "%d", zLeadSilence);
	sprintf(yTrailSilence, "%d", zTrailSilence);
	sprintf(yTotalTime, "%d", zTotalTime);
	yLanguage = gSrPort[zPort].getLanguage().c_str();
	ySensitivity = gSrPort[zPort].getSensitivity().c_str();
	yRecognitionMode = gSrPort[zPort].getRecognitionMode().c_str();
	ySpeedAccuracy = gSrPort[zPort].getSpeedAccuracy().c_str();
	yMaxNBest = gSrPort[zPort].getMaxNBest().c_str();
	yInterdigitTimeout = gSrPort[zPort].getInterdigitTimeout().c_str();
	yTermTimeout = gSrPort[zPort].getTermTimeout().c_str();
	yTermChar = gSrPort[zPort].getTermChar().c_str();
	yIncompleteTimeout = gSrPort[zPort].getIncompleteTimeout().c_str();
	yConfidenceThreshold = gSrPort[zPort].getConfidenceThreshold().c_str();
	
	header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
	headerList.push_back(header);

#if 0
	if(!ySimulateInlines)
	{
#endif
    	header.setNameValue("Content-Type", "text/uri-list");
		headerList.push_back(header);
#if 0
	}
	else
	{
		header.setNameValue("Content-Type", ySimulatedGrammarContentType);
		headerList.push_back(header);
		header.setNameValue("Content-Id", ySimulatedGrammarContentId);
		headerList.push_back(header);
	}
#endif

	header.setNameValue("Start-Input-Timers", "false");
	headerList.push_back(header);

    header.setNameValue("No-Input-Timeout", yLeadSilence);
	headerList.push_back(header);

    header.setNameValue("Recognition-Timeout", yTotalTime);
	headerList.push_back(header);

    header.setNameValue("Speech-Complete-Timeout", yTrailSilence);
	headerList.push_back(header);
	
	if ( yIncompleteTimeout.length() > 0 )
	{
    	header.setNameValue("Speech-Incomplete-Timeout", yIncompleteTimeout.c_str());
		headerList.push_back(header);
	}

	if ( yTermTimeout.length() > 0 )
	{
    	header.setNameValue("DTMF-Term-Timeout", yTermTimeout.c_str());
		headerList.push_back(header);
	}
	else
	{
    	header.setNameValue("DTMF-Term-Timeout", yTrailSilence);
		headerList.push_back(header);
	}
	
	if ( yTermChar.length() > 0 )
	{
    	header.setNameValue("DTMF-Term-Char", yTermChar.c_str());
		headerList.push_back(header);
	}

	if ( yInterdigitTimeout.length() > 0 )
	{
    	header.setNameValue("DTMF-Interdigit-Timeout", yInterdigitTimeout.c_str());
		headerList.push_back(header);
	}
	else
	{
    	header.setNameValue("DTMF-Interdigit-Timeout", yTrailSilence);
		headerList.push_back(header);
	}

#if 1
    header.setNameValue("Cancel-If-Queue", "false");
    headerList.push_back(header);
#endif

    if ( waveformURI )
    {
        header.setNameValue("Waveform-URI", "TRUE");
        headerList.push_back(header);

        header.setNameValue("Save-Waveform", "TRUE");
        headerList.push_back(header);
    }

	if ( yLanguage.length() > 0 )
	{
    	header.setNameValue("speech-language", yLanguage.c_str());
		headerList.push_back(header);
	}
	else
	{
    	header.setNameValue("speech-language", "en-US");
		headerList.push_back(header);
	}


	if ( ySensitivity.length() > 0 )
	{
		header.setNameValue("Sensitivity-Level", ySensitivity.c_str());
		headerList.push_back(header);
	}
	
	if ( yRecognitionMode.length() > 0 )
	{
		header.setNameValue("Recognition-Mode", yRecognitionMode.c_str());
		headerList.push_back(header);
	}
#if 0
	else
	{
		header.setNameValue("Sensitivity-Level", "0.5");
		headerList.push_back(header);
	}
#endif

#if 0
    header.setNameValue("n-best-list-length", "1");
	headerList.push_back(header);
#endif

	if ( yConfidenceThreshold.length() > 0 )
	{
		header.setNameValue("Confidence-Threshold", yConfidenceThreshold.c_str());
		headerList.push_back(header);
	}
	else
	{
    	header.setNameValue("Confidence-Threshold", "0.5");
		headerList.push_back(header);
	}

	if ( ySpeedAccuracy.length() > 0 )
	{
		header.setNameValue("Speed-Vs-Accuracy", ySpeedAccuracy.c_str());
		headerList.push_back(header);
	}

	if ( yMaxNBest.length() > 0 )
	{
		header.setNameValue("n-best-list-length", yMaxNBest.c_str());
		headerList.push_back(header);
	}


    header.setNameValue("Content-Length", yContentLength);
	headerList.push_back(header);

	gSrPort[zPort].incrementRequestId();
	ClientRequest recognizeRequest(
		gMrcpInit.getMrcpVersion(),
		"RECOGNIZE",
		gSrPort[zPort].getRequestId(),
		headerList,
		atoi(yContentLength));

	string recogMsg = recognizeRequest.buildMrcpMsg();
	yRecogMsg = recogMsg + yContent;
	
	return(yRecogMsg);
} // END: sCreateRecognizeRequest()

int UTL_ReadLogicalLine(char *buffer, int bufSize, FILE *fp)
{
    int     rc;
    char        str[BUFSIZ] = "";
    char        tmpStr[BUFSIZ] = "";
    char        *ptr;
    static char mod[]="UTL_ReadLogicalLine";

    memset(str, 0, sizeof(str));
    //memset(buffer, 0, bufSize);
    buffer[0] = '\0';

    while(fgets(str, sizeof(str)-1, fp))
    {
/*
 * skip empty lines
 */
        if (str[0] == '\0' )
        {
            continue;
        }

/*
 * skip lines with only whitespace
 */
        ptr = str;
        while(isspace(*ptr))
        {
            ptr++;
        }
        if(ptr[0] == '\0' )
        {
            continue;
        }
        if(ptr[0] == '#')
        {
            continue;
        }

/*
 * Check if the last character in the string is a '\'
 */
        if(str[strlen(str) - 2] == '\\')
        {
            sprintf(tmpStr, "%s", buffer);
            sprintf(buffer, "%s%.*s", tmpStr, strlen(str) - 2, str);
            continue;
        }

        if ( strlen(buffer) == 0 )
        {
            sprintf(buffer, "%s", str);
        }
        else
        {
            sprintf(tmpStr, "%s", buffer);
            sprintf(buffer, "%s%s", tmpStr, str);
        }

        break;
    }

    rc = strlen(buffer);
    if(rc == 0)
    {
        return(0);
    }
    else
    {
        return(rc);

    }
}

int getRecordHeaders(char *recordURI, char* captureOnSpeech, 
	char *yTrailSilence, char *yTotalTime, char *yLeadSilence)
{
	char recordTriggerFile[] = "/tmp/.record.trigger";
	char    yLine[1024] = "";
	FILE    *yFp = NULL;
    char    *yPtr;
    char    yName[50] = "";
    char    yValue[1024] = "";
    char    yStrTokBuf[1024];


	if((yFp = fopen(recordTriggerFile, "r")) == NULL)
	{
		return 0;
	}

	while(UTL_ReadLogicalLine(yLine, sizeof(yLine)-1, yFp))
	{	
/*
 ** Save the name of the parameter
 */
        yPtr = strtok_r(yLine, "=", (char**)&yStrTokBuf);

        if(! yPtr)
        {
            continue;
        }

        sprintf(yName, "%s", yPtr);

/*
 ** Save the value of the parameter
 */
        yPtr = (char *)strtok_r(NULL, "\n", (char**)&yStrTokBuf);

        if(! yPtr)
        {
            *yValue = 0;
        }
        else
        {
            sprintf(yValue, "%s", yPtr);
        }

        if(strcmp(yName, "Record-URI") == 0)
        {
            if(! *yValue)
            {
				*recordURI = 0;
            }
            else
            {
				sprintf(recordURI, "%s", yValue);
            }
        }
        
		if(strcmp(yName, "Capture-On-Speech") == 0)
        {
            if(! *yValue)
            {
				*captureOnSpeech = 0;
            }
            else
            {
				sprintf(captureOnSpeech, "%s", yValue);
            }
        }
		if(strcmp(yName, "No-Input-Timeout") == 0)
        {
            if(! *yValue)
            {
				*yLeadSilence = 0;
            }
            else
            {
				sprintf(yLeadSilence, "%s", yValue);
            }
        }
		if(strcmp(yName, "Max-Time") == 0)
        {
            if(! *yValue)
            {
				*yTotalTime = 0;
            }
            else
            {
				sprintf(yTotalTime, "%s", yValue);
            }
        }
		if(strcmp(yName, "Final-Silence") == 0)
        {
            if(! *yValue)
            {
				*yTrailSilence = 0;
            }
            else
            {
				sprintf(yTrailSilence, "%s", yValue);
            }
        }


	}
    if(yFp != NULL)
    {
        fclose(yFp);
    }

    yFp = NULL;


	return 0;
}


/*------------------------------------------------------------------------------
sCreateRecordRequest():
------------------------------------------------------------------------------*/
string sCreateRecordRequest(int zPort, int zLeadSilence,
								int zTrailSilence, int zTotalTime)
{
	static char	mod[] = "sCreateRecordRequest";
	int		rc = 0;
//	list <GrammarData>::iterator    iter;
//	GrammarData                     yGrammarData;
	MrcpHeader					 	header;
	MrcpHeaderList				 	headerList;
	int                             yCounter;
	string							yContent = "";
	char							yLeadSilence[16];
	char							yTrailSilence[16];
	char							yTotalTime[16];
	char							yContentLength[16];
	string							yLanguage;
	string							ySensitivity;
	string							yRecognitionMode;
	string							ySpeedAccuracy;
	string							yMaxNBest;
	string							yInterdigitTimeout;
	string							yTermTimeout;
	string							yTermChar;
	string							yIncompleteTimeout;
	string							yConfidenceThreshold;
	string							yRecogMsg;
	int								grammarType;
	string							grammarName;
	string							grammarData;
	char							yCaptureOnSpeech[16];
	char 							yRecordURI[256];
    int                             waveformURI;

    waveformURI = gMrcpInit.getwaveformURI();
	sprintf(yContentLength, "%d", yContent.length());
	yLanguage = gSrPort[zPort].getLanguage().c_str();
	ySensitivity = gSrPort[zPort].getSensitivity().c_str();
	yRecognitionMode = gSrPort[zPort].getRecognitionMode().c_str();
	ySpeedAccuracy = gSrPort[zPort].getSpeedAccuracy().c_str();
	yMaxNBest = gSrPort[zPort].getMaxNBest().c_str();
	yInterdigitTimeout = gSrPort[zPort].getInterdigitTimeout().c_str();
	yTermTimeout = gSrPort[zPort].getTermTimeout().c_str();
	yTermChar = gSrPort[zPort].getTermChar().c_str();
	yIncompleteTimeout = gSrPort[zPort].getIncompleteTimeout().c_str();
	yConfidenceThreshold = gSrPort[zPort].getConfidenceThreshold().c_str();
	
	header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
	headerList.push_back(header);


	header.setNameValue("Start-Input-Timers", "false");
	headerList.push_back(header);
	
	getRecordHeaders(yRecordURI, yCaptureOnSpeech, yTrailSilence, yTotalTime, yLeadSilence);

	if(yLeadSilence[0] != '\0')
	{
		header.setNameValue("No-Input-Timeout", yLeadSilence);
		headerList.push_back(header);
	}
	else
	{
		sprintf(yLeadSilence, "%d", zLeadSilence);
		header.setNameValue("No-Input-Timeout", yLeadSilence);
		headerList.push_back(header);
	}

	header.setNameValue("Media-Type", "audio/wav");
	headerList.push_back(header);
	
	if(yTotalTime[0] != '\0')
	{
		header.setNameValue("Max-Time", yTotalTime);
		headerList.push_back(header);
	}	
	else
	{
		sprintf(yTotalTime, "%d", zTotalTime);
	}

	if(yTrailSilence[0] != '\0')
	{
		header.setNameValue("Final-Silence", yTrailSilence);
		headerList.push_back(header);
	}
	else
	{
		sprintf(yTrailSilence, "%d", zTrailSilence);
		header.setNameValue("Final-Silence", yTrailSilence);
		headerList.push_back(header);
	}
	

	if(yRecordURI[0] != '\0')
	{
		header.setNameValue("record-uri", yRecordURI);
		headerList.push_back(header);
	}
	
	if(yCaptureOnSpeech[0] != '\0')
	{
		header.setNameValue("capture-on-speech", yCaptureOnSpeech);
		headerList.push_back(header);
	}
	
	if ( ySensitivity.length() > 0 )
	{
		header.setNameValue("Sensitivity-Level", ySensitivity.c_str());
		headerList.push_back(header);
	}
	
	if ( yRecognitionMode.length() > 0 )
	{
		header.setNameValue("Recognition-Mode", yRecognitionMode.c_str());
		headerList.push_back(header);
	}

	gSrPort[zPort].incrementRequestId();
	ClientRequest recognizeRequest(
		gMrcpInit.getMrcpVersion(),
		"RECORD",
		gSrPort[zPort].getRequestId(),
		headerList,
		atoi(yContentLength));

	string recogMsg = recognizeRequest.buildMrcpMsg();
	yRecogMsg = recogMsg + yContent;
	
	return(yRecogMsg);
} // END: sCreateRecordRequest()

/*------------------------------------------------------------------------------
sCreateInterpretRequest():
------------------------------------------------------------------------------*/
string sCreateInterpretRequest(int zPort)
{
	static char	mod[] = "sCreateRecognizeRequest";
	int		rc = 0;
	MrcpHeader					 	header;
	MrcpHeaderList				 	headerList;
	int                             yCounter;
	string							yContent = "";
	string							yInterpretText = "";
	char							yContentLength[16];
	string							yRecogMsg;
	int								grammarType;
	string							grammarName;
	string							grammarData;
	string							yTermChar;

	int								ySimulateInterpret = 0;
	string							ySimulatedInterpretData;
	char							ySimulatedInterpretContentType[64];
	char							ySimulatedInterpretContentId[64];
	char							yLine[256];
	char							ySimulateInterpretFileName[128];

	sprintf(ySimulateInterpretFileName, "%s", "/tmp/.interpret.trigger");
	yTermChar = gSrPort[zPort].getTermChar().c_str();

	if(access(ySimulateInterpretFileName, F_OK) == 0)
	{
		ySimulateInterpret = 1;

		FILE* ySimulateDataFp = fopen(ySimulateInterpretFileName, "r");

		/*Reset the flag ySimulateInlines if file is there, but not readable.*/
		if(!ySimulateDataFp)
		{
			ySimulateInterpret = 0;
		}
		else
		{
			yLine[0] = 0;

			fgets(yLine, sizeof(yLine)-1, ySimulateDataFp);
			{
				if(yLine[0])
				{
					yInterpretText = yLine;
				}
			}


			fclose(ySimulateDataFp);

			/*Reset the flag ySimulateInlines if file is there, but is empty.*/
			if(yInterpretText.length() == 0)
			{
				ySimulateInterpret = 0;
			}
		}
	}
#if 1
	int								ySimulateInlines = 0;
	string							ySimulatedGrammarData;
	char							ySimulatedGrammarContentType[64];
	char							ySimulatedGrammarContentId[64];
	char							yLine1[256];
	char							ySimulateSRGrammarsFileName[128];

	sprintf(ySimulateSRGrammarsFileName, "%s", "/tmp/simulateSRGrammars.txt");

	if(access(ySimulateSRGrammarsFileName, F_OK) == 0)
	{
		ySimulateInlines = 1;

		FILE* ySimulateDataFp = fopen(ySimulateSRGrammarsFileName, "r");

		/*Reset the flag ySimulateInlines if file is there, but not readable.*/
		if(!ySimulateDataFp)
		{
			ySimulateInlines = 0;
		}
		else
		{
			yLine1[0] = 0;

			int yLineCounter = 0;	

			while ( fgets(yLine1, sizeof(yLine1)-1, ySimulateDataFp))
			{
				if(yLine1[0])
				{
					/*Read first line for "Content-Type" */
					if(yLineCounter == 0)
					{
						yLine1[strlen(yLine1) - 1] = 0;
						sprintf(ySimulatedGrammarContentType, "%s", yLine1);
						yLineCounter++;

					}
					else if(yLineCounter == 1)
					{
						yLine1[strlen(yLine1) - 1] = 0;
						sprintf(ySimulatedGrammarContentId, "%s", yLine1);
						yLineCounter++;

					}
					else
					{
						/*Read subsequent lines for grammar data */
						ySimulatedGrammarData = ySimulatedGrammarData + yLine1;
					}
				}
			}

			yContent = ySimulatedGrammarData;

			fclose(ySimulateDataFp);

			/*Reset the flag ySimulateInlines if file is there, but is empty.*/
			if(yContent.length() == 0)
			{
				ySimulateInlines = 0;
			}
		}
	}

	if(!ySimulateInlines)
	{

#endif

		if ( ! grammarList[zPort].getNextActiveGrammar(GRAMMARLIST_START_FROM_FIRST,
			&grammarType, grammarName, grammarData) )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
						SR_NO_ACTIVE_GRAMMARS, ERR, 
						"No grammars found in grammar list.  Unable to perform "
						"recognition.");
			return("");
		}

		for (;;)
		{
			yContent = yContent + "session:" + grammarName + "\r\n";
	
			if ( ! grammarList[zPort].getNextActiveGrammar(GRAMMARLIST_NEXT, &grammarType, grammarName, grammarData) )
			{
				break;
			}
		}
#if 1
	}
	else	/*Else simulate the contents as if it were grammars attached to RECOGNIZE*/
	{
	}
#endif

	sprintf(yContentLength, "%d", yContent.length()+1);
	
	header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
	headerList.push_back(header);
	header.setNameValue("Interpret-Text", yInterpretText.c_str());
	headerList.push_back(header);

#if 1
	if(!ySimulateInlines)
	{
#endif
    	header.setNameValue("Content-Type", "text/uri-list");
		headerList.push_back(header);
#if 1
	}
	else
	{
		header.setNameValue("Content-Type", ySimulatedGrammarContentType);
		headerList.push_back(header);
		header.setNameValue("Content-Id", ySimulatedGrammarContentId);
		headerList.push_back(header);
	}
#endif

	if ( yTermChar.length() > 0 )
	{
    	header.setNameValue("DTMF-Term-Char", yTermChar.c_str());
		headerList.push_back(header);
	}
    header.setNameValue("Content-Length", yContentLength);
	headerList.push_back(header);

	gSrPort[zPort].incrementRequestId();
	ClientRequest recognizeRequest(
		gMrcpInit.getMrcpVersion(),
		"INTERPRET",
		gSrPort[zPort].getRequestId(),
		headerList,
		atoi(yContentLength));

	string recogMsg = recognizeRequest.buildMrcpMsg();
	yRecogMsg = recogMsg + yContent;
	
	return(yRecogMsg);
} // END: sCreateInterpretRequest()

/*------------------------------------------------------------------------------
sStartTheMrcpServerTimers():
------------------------------------------------------------------------------*/
int sStartTheMrcpServerTimers(int zPort)
{
	static char						mod[] = "sStartTheMrcpServerTimers";
	int								rc = 0;
	MrcpHeaderList				 	headerList;
	MrcpHeader						header;
	char							yContentLength[16];
	string							yStartTimerMsg;
	string							yContent = "";

	sprintf(yContentLength, "%d", yContent.length());

	gSrPort[zPort].incrementRequestId();

    header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
    headerList.push_back(header);

	ClientRequest recognizeRequest(
		gMrcpInit.getMrcpVersion(),
		"START-INPUT-TIMERS",
		gSrPort[zPort].getRequestId(),
		headerList, atoi(yContentLength));

	string sTimerMsg = recognizeRequest.buildMrcpMsg();
	yStartTimerMsg = sTimerMsg + yContent;
	
	string		yRecvMsg		= "";
	string		yServerState	= "";
	int			yCompletionCode	= -1;
	int			yStatusCode		= -1;

	rc = processMRCPRequest (zPort, sTimerMsg, yRecvMsg,
				yServerState, &yStatusCode, &yCompletionCode);

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
		MRCP_2_BASE, REPORT_VERBOSE, 
		"MRCP response: rc=%d; completionCode=%d; statusCode=%d; "
		"serverState=%s.",
		rc, yCompletionCode, yStatusCode, yServerState.c_str());
	
	return(rc);
} // END: sStartTheMrcpServerTimers()

/*------------------------------------------------------------------------------
sCheckForMrcpEvent():
------------------------------------------------------------------------------*/
int sCheckForMrcpEvent(int zPort, EventInfo *zEventInfo, Mrcp2_Event     & mrcpEvent)
{
	static char		mod[]= "sCheckForMrcpEvent";
//static int 		yEventCounter = 0 ;
	int				rc;

#if 0
//	memset((EventInfo *)zEventInfo, '\0', sizeof(EventInfo));
	if (yEventCounter++ % 10 != 0 )
	{
		yEventCounter = 0;
		return(0);
	}
#endif
		
    string          yRecvMsg        = "";
    string          yServerState    = "";
    string          yEventString    = "";
    int             yCompletionCause= -1;
    int             yStatusCode     = -1;

	rc = readMrcpSocket(zPort, READ_EVENT, yRecvMsg,
				yEventString, yServerState,  mrcpEvent, &yStatusCode, &yCompletionCause);
	if ( rc == YES_EVENT_RECEIVED )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, REPORT_VERBOSE, 
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
	return(rc);

#if 0
	if ( arcRTSP.mrcpResult.event == EVENT_START_OF_SPEECH )
	{
		yStartOfSpeechDetected = 1;
		*zEvent = EVENT_START_OF_SPEECH;
		gStopBargeinPhrase = 1;
		gStartRecordTimeLead = -2;
		(void) sendSpeechDetected(zPort);
		return(YES_EVENT_RECEIVED);
	}
	if ( arcRTSP.mrcpResult.event == EVENT_RECOGNITION_COMPLETE )
	{
	#if 0
		printf("APJDEBUG DM_SRRecognize Got EVENT_RECOGNITION_COMPLETE, first "
			"do settings for UTTERENCE and then proceed.\n"); fflush(stdout);
		yStartOfSpeechDetected = 1;
		gStopBargeinPhrase = 1;
		gStartRecordTimeLead = -2;
	#endif

		*zEvent = EVENT_RECOGNITION_COMPLETE;
		yRecognitionCompleteReceived = 1;
		yRecognizerStarted = 0;
		gServerDetectedEOS = 1;

		if ( ( arcRTSP.mrcpResult.completionCause == 0 ) ||
		     ( arcRTSP.mrcpResult.completionCause == 1 ) )
		{
			yStartOfSpeechDetected = 1;
		}
		else
		if ( arcRTSP.mrcpResult.completionCause == 2 )
		{
			gLeadTimeout = 1;
		}

		return(YES_EVENT_RECEIVED);
	}
	return(NO_EVENT_RECEIVED);
#endif

} /* END: sCheckForMrcpEvent() */

/*------------------------------------------------------------------------------
getTimeDiffInMiliSeconds():
------------------------------------------------------------------------------*/
long getTimeDiffInMiliSeconds(struct timeval *zpTimeVal1, 
		struct timeval *zpTimeVal2)
{
	if(zpTimeVal1->tv_usec >= zpTimeVal2->tv_usec)
	{
		return(((zpTimeVal1->tv_sec - zpTimeVal2->tv_sec) * 1000) +
			   ((zpTimeVal1->tv_usec - zpTimeVal2->tv_usec) / 1000));
	}
	else
	{
		return((((zpTimeVal1->tv_sec - 1) - zpTimeVal2->tv_sec) * 1000) +
			  (((zpTimeVal1->tv_usec + 1000000) - zpTimeVal2->tv_usec) / 1000));
	}
	
	return(0);
} /* END: getTimeDiffInMiliSeconds() */
