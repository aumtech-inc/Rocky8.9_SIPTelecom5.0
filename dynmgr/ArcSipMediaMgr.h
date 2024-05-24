/*------------------------------------------------------------------------------
Program Name:	ArcSipMediaMgr
File Name:		ArcSipMediaMgr.h
Purpose:  		ArcSipMediaMgr specific #includes for SIP/2.0
Author:			Aumtech, inc.
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
Update: 07/07/2004 DDN Created the file
------------------------------------------------------------------------------*/

#include "ArcSipCommon.h"
#include "arcToneDetector.h"
#include "arcSR.h"

/*Arc log routine*/
extern "C" int LogARCMsg (char *parmModule, int parmMode, char *parmResourceName, char *parmPT_Object,
			  char *parmApplName, int parmMsgId, char *parmMsg);

int writeWavHeaderToFile (int zCall, FILE * infile);
int getPassword (int zCall);
int writeGenericResponseToApp (int zCall, struct MsgToApp *zpResponse,
			       char *mod, int zLine);
int nano_sleep (int Seconds, int nanoSeconds);
int writeToResp (char *mod, void *zpMsg);
int canContinue (char *mod, int zCallNum, int line);
int nano_sleep (int Seconds, int nanoSeconds);
int util_sleep (int Seconds, int Milliseconds);
int speakFile (int sleep_time, int iterrupt_option, char *zFileName, int synchronous, int zCall, int zAppRef, RtpSession * zSession,
	       int *zpTermReason, struct MsgToApp *zpMsgToApp);
int sendFile(int sleep_time, char *zFileName,int zCall, int zAppRef, RtpSession *zSession, int *zpTermReason, struct MsgToApp * zResponse);

int sendRequestToGoogleSRFifo(int zCall, int zLine, int gsopcode, char *recordedFile);
int sendByeToGoogleSRFifo(int zCall, int zLine);
int sendByeNoWaitToGoogleSRFifo(int zCall, int zLine);

/*Streaming function declarations*/
int stream_read (int zFd, char *zpAudioBuf, unsigned zCnt);
int cleanUpStreamBuffersAndFifo (int zPort, int zAppRef);
int stopDownloadStreamData (int zPort, int zOpcode);
int getStreamData (int zPort, char *zpAudioBuf, int zCnt);
int getStreamDataForPlayBack(int zCall, char *zpAudioBuf, int zCnt);

#if 0
int getStreamDataForPlayBack (int zPort, char *zpAudioBuf, int zCnt);
#endif

int addToStreamingList (int zPort, char *zFileName, int zLastBuffer);
int closeStreamingClientSession (int zPort);
int deleteStreamingList (int zPort);
int createStreamingFifo (int zPort, int zAppRef);


int getStreamingParams (char *zQuery, char *zSubscriber, char *zSession,
			int *zMsgId, int *zNodeId, long *zTime);


int getValue (char *zStrInput, char *zStrOutput, char *zStrKey,
	      char zChStartDelim, char zChStopDelim);

#ifdef ACU_LINUX
int openChannelToTonesClient();
int openChannelToTonesFaxClient(int zCall);
int openChannelToConfMgr();
#endif

/*END: Streaming function declarations*/
