/*------------------------------------------------------------------------------
Program Name:	ArcSipCallMgr/ArcIPDynMgr
File Name:		ArcSipCallMgr.h
Purpose:  		ArcSipCallMgr specific #includes for SIP/2.0
Author:			Aumtech, inc.
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
Update: 07/07/2004 DDN Created the file
------------------------------------------------------------------------------*/

#include "ArcSipCommon.h"

#include <stdint.h>

/*Arc log routine*/
extern "C" int LogARCMsg (char *parmModule, int parmMode,
			  char *parmResourceName, char *parmPT_Object,
			  char *parmApplName, int parmMsgId, char *parmMsg);

int writeWavHeaderToFile (FILE * infile);
int getPassword (int zCall);
int writeGenericResponseToApp (int zLine, int zCall, struct MsgToApp *zpResponse,
			       char *mod);
void *answerCallThread (void *z);
void *dropCallThread (void *z);
void *timerThread (void *z);
void *supervisedTransferCallThread (void *z);
void *transferCallThread (void *z);
int stopAllCallThreads (char *mod, int zCallNum);
int nano_sleep (int Seconds, int nanoSeconds);
int writeToResp (char *mod, void *zpMsg);
int canContinue (char *mod, int zCallNum);
int sendMsgToSRClient (int zPort, struct MsgToDM *zpMsgToDM);
int nano_sleep (int Seconds, int nanoSeconds);
int util_sleep (int Seconds, int Milliseconds);
int speakFile (int sleep_time, int iterrupt_option, char *zFileName,
	       int synchronous, int zCall, RtpSession * zSession,
	       struct MsgToApp *zpMsgToApp);

int notifyMediaMgrThroughTimer (int zLine, int zCall, struct MsgToDM *zpMsgToDM,
				char *mod);
int notifyMediaMgr (int zLine, int zCall, struct MsgToDM *zpMsgToDM, char *mod);
int createMrcpRtpDetails (int zCall);





 /*EOF*/
