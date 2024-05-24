#ifndef TEL_COMMON_DOT_H
#define TEL_COMMON_DOT_H

/*-----------------------------------------------------------------------------
Program:        TEL_Common.h
Purpose:        Contains everything that is common to the H323 Telecom
		API set, except actual code. This file includes the following:
		1) All include files (system & application).
		2) All externs (defined in TEL_Common.c).
		3) All #defines internal to the APIs, 
			i.e. those not in Telecom.h
Author:         Wenzel-Joglekar
Date:		08/24/2000	
Date:		09/13/2000 gpw added GV_AppPhraseDir
Update:		09/27/2000 gpw added system phrase numbers
Update:		09/27/2000 gpw added include of time.h, sys/time.h
Update:		09/27/2000 gpw added include of time.h, sys/time.h
Update:		10/10/2000 gpw added HANDLE_RETURN
Update:		10/13/2000 gpw added setjmp.h for alarm
Update:		10/18/2000 gpw added TEL_SendToMonitor.h
Update:		10/19/2000 gpw poll.h
Update:		10/27/2000 gpw added stdarg.h
Update:		10/29/2000 apj added shm_def.h , BNS.h, ISPSYS.h.
Update:		12/04/2000 gpw added monitor.h
Update:		03/13/2001 apj Changed APP_TO_APP_INFO_LENGTH to 512.
Update:		03/28/2001 apj Added BEEP_FILE_NAME define.
Update:		03/29/2001 apj Added MAX_APPL_NAME define
Update:		07/16/2001 apj Added TEL_PromptAndCollect.h include.
Update:		07/18/2001 apj Added GV_IndexDelimit extern.
Update:		08/02/2006 djb Added ARC_MRCP_V2 #ifdef logic.
------------------------------------------------------------------------------*/
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/wait.h>

#include "Telecom.h"
#include "AppMessages.h"
#include "TEL_LogMessages.h"
#include "TEL_SendToMonitor.h"
#include "SR_Mnemonics.h"
#include "AI_Mnemonics.h"
#include "shm_def.h"
#include "BNS.h"
#include "ISPSYS.h"
#include "monitor.h"
#include "TEL_PromptAndCollect.h"
#include "UTL_accessories.h"
#include "arcAI.h"

#ifdef ARC_MRCP_V2
	#include "arcSR_mrcpV2.h"
#else
	#include "arcSR.h"
#endif
#include "arc_snmpdefs.h"

#define	VERSION			"1.0"
// djb  - #define MAX_LENGTH		1024	/* Generic length, eliminate */
#define MAX_LENGTH		1024	/* Generic length, eliminate */
#define MAX_LOG_MSG_LENGTH 	512+1	
#define MAX_FIFO_NAME_LENGTH 	128+1	
#define MAX_DIR_NAME_LENGTH	128+1	
#define MAX_DTMF_KEY_SEQUENCE_LENGTH	64	
#define MAX_DIAL_SEQUENCE_LENGTH	64	
#define MAX_USER_TO_USER_INFO_LENGTH	64	
#define MAX_APP_TO_APP_INFO_LENGTH	512	
#define FIFO_DIR		"/tmp"	
#define BEEP_FILE_NAME		"beep.wav"	
#define MAX_APPL_NAME 	50


#ifdef TEL_COMMON
int apiInit (char *mod, int apiId, char *apiAndParms,
         int initRequired, int party1Required, int party2Required);

int telVarLog (char *mod, int mode, int msgId, int msgType, char *msgFormat, ...);
int getCodecValue(char *zStrInput, char *zStrOutput, char *zStrKey, char zChStartDelim, char zChStopDelim);
int sendRequestToDynMgr (char *mod, struct MsgToDM *request);
int readResponseFromDynMgr (char *mod, int timeout, void *pMsg, int zMsgLength);
int examineDynMgrResponse (char *mod, struct MsgToDM *req, struct MsgToApp *res);
void setTime (char *time_str, long *tics);
int sendRequestToAIClient (char *mod, struct MsgToDM *request);
int disc (char *mod, int callNum);
int writeStartCDRRecord (char *mod);
int saveTypeAheadData (char *mod, int callNum, char *data);
int prepLongDestinationForFileIPC(char *zLabel, char *zFileContents, char *zNewDestination);
int writeStringToSharedMemory (char *mod, int slot, char *valueString);
int connectCalls(char *mod, void *m);
int goodDestination (char *mod, int informat, char *destination, char *editedDestination);
int writeCustomCDRRecord (char *mod, char *custom_key, char *custom_time, int msgId);
int tellResp1stPartyDisconnected (char *mod, int callNum, int pid);
int get_name_value (char *section, char *name, char *defaultVal, char *value,
                int bufSize, char *file, char *err_msg);
int setCDRKey (char *mod, char *port_no);
int get_first_phrase_extension (char *mod, char *directory, char *extension);
int openChannelToDynMgr (char *mod);
int get_characters (char *mod, char *toString, char *fromString, char termChar, int maxLength,
                char *allowableChars, int *termCharPressed);
int get_first_phrase_extension (char *mod, char *directory, char *extension);
int sendRequestToSRClient (char *mod, struct MsgToDM *request);
int openChannelToAIClient (char *mod, char *zAIClientFifo);
int closeChannelToAIClient (char *mod);
void handleReturn (int rc);

#else
extern int apiInit (char *mod, int apiId, char *apiAndParms,
         int initRequired, int party1Required, int party2Required);
extern int telVarLog (char *mod, int mode, int msgId, int msgType, char *msgFormat, ...);
extern int getCodecValue(char *zStrInput, char *zStrOutput, char *zStrKey, char zChStartDelim, char zChStopDelim);
extern int sendRequestToDynMgr (char *mod, struct MsgToDM *request);
extern int readResponseFromDynMgr (char *mod, int timeout, void *pMsg, int zMsgLength);
extern int examineDynMgrResponse (char *mod, struct MsgToDM *req, struct MsgToApp *res);
extern void setTime (char *time_str, long *tics);
extern int sendRequestToAIClient (char *mod, struct MsgToDM *request);
extern int disc (char *mod, int callNum);
extern int writeStartCDRRecord (char *mod);
extern int saveTypeAheadData (char *mod, int callNum, char *data);
extern int prepLongDestinationForFileIPC(char *zLabel, char *zFileContents, char *zNewDestination);
extern int writeStringToSharedMemory (char *mod, int slot, char *valueString);
extern int connectCalls(char *mod, void *m);
extern int goodDestination (char *mod, int informat, char *destination, char *editedDestination);
extern int writeCustomCDRRecord (char *mod, char *custom_key, char *custom_time, int msgId);
extern int tellResp1stPartyDisconnected (char *mod, int callNum, int pid);
extern int get_name_value (char *section, char *name, char *defaultVal, char *value,
                int bufSize, char *file, char *err_msg);
extern int setCDRKey (char *mod, char *port_no);
extern int get_first_phrase_extension (char *mod, char *directory, char *extension);
extern int openChannelToDynMgr (char *mod);
extern int get_characters (char *mod, char *toString, char *fromString, char termChar, int maxLength,
                char *allowableChars, int *termCharPressed);
extern int get_first_phrase_extension (char *mod, char *directory, char *extension);
extern int sendRequestToSRClient (char *mod, struct MsgToDM *request);
int openChannelToAIClient (char *mod, char *zAIClientFifo);
int closeChannelToAIClient (char *mod);
void handleReturn (int rc);


#endif // TEL_COMMON

#define HANDLE_RETURN(X) {handleReturn(X); return(X);}


const char *arc_val_tostr(int value);

#define util_sleep(secs, mill) (usleep(secs * mill))

extern char GV_IndexDelimit;

/*---------------------------------------------------------------------------
System phrase base numbers
---------------------------------------------------------------------------*/
#define MINUTE_BASE     90
#define NUMBER_BASE     100
#define HUNDRED_BASE    201
#define THOUSAND_BASE   202
#define MILLION_BASE    203
#define BILLION_BASE    203
#define MONTH_BASE      249
#define DAYOFMONTH_BASE 269
#define DOLLARS_BASE    321
#define DOLLARS_AND_BASE 323
#define DOLLAR_BASE     320
#define DOLLAR_AND_BASE 322
#define CENT_BASE       324
#define CENTS_BASE      325
#define CURRENT_CENTURY 1900
#define YESTERDAY_PHRASE 332
#define TODAY_PHRASE    333
#define TOMORROW_PHRASE 334
#define AM_PHRASE       330
#define PM_PHRASE       331
#define HOUR_PHRASE     351
#define MIN_PHRASE      353
#define WEEK_PHRASE     310
#define MIDNIGHT_PHRASE         344
#define NOON_PHRASE             345
#define MORNING_PHRASE          346
#define AFTERNOON_PHRASE        347
#define EVENING_PHRASE          348
#define ALPHANUM_BASE		700

#include "TEL_Globals.h"

#endif 


