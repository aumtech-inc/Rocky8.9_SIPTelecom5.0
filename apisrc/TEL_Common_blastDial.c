/*-------------------------------------------------------------------------
Program:        TEL_Common.c
Purpose:        Contains all common (global) variables and functions
		used by any of the IPIVR Telecom Services APIs. 
Author:         Wenzel-Joglekar
Date:		12/04/2000
Update:		02/27/2001 removed extern ArcLogId[4]; added REPORT_CDR
Update:		03/27/2001 mpb Commented REPORT_CDR define.
Update:		03/29/2001 apj Added writeStringToSharedMemory.
Update:		04/25/2001 apj Added COMP_WAV.
Update:		05/08/2001 apj Added proc_sigterm.
Update:		05/16/2001 apj set MsgTpApp to 0 first in readResponseFromDynMgr
Update:		05/29/2001 apj Call LogOff only once in proc_sigterm.
Update:		06/26/2001 apj Moved some of the printfs to Log.
Update: 	06/26/2001 apj Added mod to get_characters.
Update: 	06/28/2001 apj In get_characters, do not clear toString evenif
				fromString in NULL or character to get are 0.
Update: 	07/17/2001 apj Added GV_DontSendToTelMonitor logic.
Update: 	07/17/2001 apj Added PHRASE_TAG.
Update: 	08/06/2001 apj In examineDynMgrResponse,handle DMOP_INITIATECALL
Update:		11/01/2001 apj Added get_first_phrase_extension.
Update:		11/01/2001 apj Added modifiedPopen and modifiedPclose.
Update:		06/18/2004 apj Added setCDRKey.
Update:     08/25/04 djb Changed sys_errlist strerror(errno).
Update:     11/14/04 apj Added a log message in examineDynMgrResponse.
Update: 02/03/05 djb	Changed examineDynMgrResponse() to not log error msg
                    	req opcode is recog and res opcode is speak.  mr #55
Update: 02/09/05 djb	Changed examineDynMgrResponse() to not log error msg
						if result.returnCode == -3.
Update: 	05/09/2005 rg Added PORT in GV_CDRKey before the portnum 
Update: 09/01/05 djb MR # 633 - Removed references to SR_CONCEPT_AND_TAG. 
Update: 09/13/07 djb Added mrcpTTS logic.
------------------------------------------------------------------------------*/
#define GLOBAL

#include "TEL_Common.h"
#include "arcFifos.h"
#include "ttsStruct.h"

jmp_buf env_alrm;
void handle_timeout (int dummy);
static int trim_whitespace (char *string);
void setTime (char *time_str, long *tics);
static char fifo_dir[128];
static int get_fifo_dir (char *);


char temStr[] = "DDNDEBUG";


/*extern char ArcLogId[4]; */
//#define REPORT_CDR REPORT_NORMAL

/* Dummy version of send to monitor until we can link in the real one */
int tellResp1stPartyDisconnected (char *mod, int callNum, int pid);
int closeChannelToSRClient (char *mod);

/*----------------------------------------------------------------------------
Common logging routine for all Telecom APIs. 
This should eventually call LogARCMsg.
----------------------------------------------------------------------------*/
int
telVarLog (char *mod, int mode, int msgId, int msgType, char *msgFormat, ...)
{
  va_list ap;
  char m[1024];
  char m1[512];
  char type[32];
  char portString[32];
  int rc;

  /*strcpy(ArcLogId,"ARC");        */
  switch (msgType) {
  case 0:
    strcpy (type, "ERR: ");
    break;
  case 1:
    strcpy (type, "WRN: ");
    break;
  case 2:
    strcpy (type, "INF: ");
    break;
  default:
    strcpy (type, "");
    break;
  }

  memset (m1, '\0', sizeof (m1));
  va_start (ap, msgFormat);
  vsprintf (m1, msgFormat, ap);
  va_end (ap);

  sprintf (m, "%s%s", type, m1);
  /* Set global variable so that 1st field in log msg defaults to "ISP" */
  /*strcpy(ArcLogId,"");   */
  sprintf (portString, "%d", GV_AppCallNum1);
  rc = LogARCMsg (mod, mode, portString, "TEL", GV_AppName, msgId, m);
  return (rc);
}

/*----------------------------------------------------------------------------
Common logging routine for all Telecom APIs. 
This should eventually call LogARCMsg.
----------------------------------------------------------------------------*/
int
telCDRLog (char *mod, int mode, int msgId, int msgType, char *msgFormat, ...)
{
  va_list ap;
  char m[1024];
  char m1[512];
  char type[32];
  char portString[32];
  int rc;

  memset (m, '\0', sizeof (m));
  va_start (ap, msgFormat);
  vsprintf (m, msgFormat, ap);
  va_end (ap);

  /* Set global variable so that 1st field in log msg is "CDR" */
  /* strcpy(ArcLogId,"CDR");       */
  sprintf (portString, "%d", GV_AppCallNum1);
  rc = LogARCMsg (mod, mode, portString, "TEL", GV_AppName, msgId, m);
  return (rc);
}

/*----------------------------------------------------------------------------
Function to get the results of a request back from the dynamic manager.
If the timeout parameter is >0 then the alarming mechanism is turned on.
----------------------------------------------------------------------------*/
int
readResponseFromDynMgr (char *mod, int timeout, void *pMsg, int zMsgLength)
{
  int rc;
  static int fifo_is_open = 0;
  struct MsgToApp *response;
  struct pollfd pollset[1];

  memset (pMsg, 0, sizeof (struct MsgToApp));
  if (!fifo_is_open) {
    if ((mkfifo (GV_ResponseFifo, S_IFIFO | 0666) < 0)
        && (errno != EEXIST)) {
      telVarLog (mod, REPORT_NORMAL, TEL_CANT_CREATE_FIFO, GV_Err,
                 TEL_CANT_CREATE_FIFO_MSG, GV_ResponseFifo,
                 errno, strerror (errno));
      return (-1);
    }

    if ((GV_ResponseFifoFd = open (GV_ResponseFifo, O_RDWR)) < 0) {
      telVarLog (mod, REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
                 TEL_CANT_OPEN_FIFO_MSG, GV_ResponseFifo,
                 errno, strerror (errno));

      /* SHOULD WE PUT SOMETHING INTO THE STRUCTURE ?? */
      return (-1);
    }
    fifo_is_open = 1;
  }

  pollset[0].fd = GV_ResponseFifoFd;
  pollset[0].events = POLLIN;

  if (timeout > 0) {
    signal (SIGALRM, handle_timeout);

    /* 2nd parameter != 0 in sigsetjmp means save off signal mask */
    rc = sigsetjmp (env_alrm, 1);
    if (rc == 1) {
      /* Timeout has occurred . */
      return (TEL_TIMEOUT);
    }
    alarm (timeout);
  }
  else if (timeout == -1) {
    rc = poll (pollset, 1, 0);
    if (rc < 0) {
      telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
                 "error in poll, rc=%d, errno=%d. [%s]",
                 rc, errno, strerror (errno));
      return (-1);
    }
    if (pollset[0].revents == 0) {
      /* Timed out waiting for data to read */
      return (-2);
    }
    else if (pollset[0].revents == 1) {
      /* There is data to read. */
    }
    else {
      /* Unexpected return code from poll */
      telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
                 "Unexpected return code on poll: %d. errno=%d. [%s]",
                 rc, errno, strerror (errno));
      return (-1);
    }
  }

  if (access (GV_ResponseFifo, F_OK) != 0) {
    telVarLog (mod, REPORT_NORMAL, TEL_CANT_READ_FIFO, GV_Err,
               TEL_CANT_READ_FIFO_MSG, GV_ResponseFifo,
               errno, strerror (errno));
    /* SHOULD WE PUT SOMETHING INTO THE STRUCTURE ?? */
    return (-1);
  }

  rc = read (GV_ResponseFifoFd, (char *) pMsg, zMsgLength);
  if (timeout > 0) {
    /* Turn off the alarm & reset the handler. */
    alarm (0);
    signal (SIGALRM, NULL);
  }

  response = (struct MsgToApp *) pMsg;
  if (rc == -1) {
    telVarLog (mod, REPORT_NORMAL, TEL_CANT_READ_FIFO, GV_Err,
               TEL_CANT_READ_FIFO_MSG, GV_ResponseFifo,
               errno, strerror (errno));
    /* SHOULD WE PUT SOMETHING INTO THE STRUCTURE ?? */
    return (-1);
  }
  telVarLog (mod, REPORT_DETAIL, TEL_BASE, GV_Info,
             "Received %d bytes. "
             "msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,rc:%d,data:%s}",
             rc,
             response->opcode,
             response->appCallNum,
             response->appPid,
             response->appRef,
             response->appPassword, response->returnCode, response->message);
  return (0);
}

/*-----------------------------------------------------------------------------
This function opens the fifo upon which requests are to the dynamic manager
 are written. 
-----------------------------------------------------------------------------*/
int
openChannelToDynMgr (char *mod)
{
  if ((GV_RequestFifoFd = open (GV_RequestFifo, O_WRONLY)) < 0) {
    telVarLog (mod, REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
               TEL_CANT_OPEN_FIFO_MSG, GV_RequestFifo, errno,
               strerror (errno));
    return (-1);
  }
  return (0);
}

/*-----------------------------------------------------------------
This routine is the SERV_M3 & SERV_ALL handler. It is used in conjunction
with the HANDLE_RETURN macro defined in TEL_Globals.h. Every Telecom
Services API should call HANDLE_RETURN to return from its main routine,
and ONLY its main routine.
--------------------------------------------------------------------*/


#if 1

void
handleReturn (int rc)
{
  switch (rc) {
  case -3:
    if (SERV_M3 != NULL) {
      SERV_M3 ();
      return;
    }
    break;
  default:
    break;
  }

  if (SERV_ALL != NULL)
    SERV_ALL (rc);
  return;
}

#endif

#if 0
void handleReturn(int rc){
  return;
}
#endif




/*--------------------------------------------------------------------------
This function checks the sent message against the received message.
If it is unexpected, it logs the error. 
Return codes are: 
	-1=unexpected msg , 0=opcode requested, DMOP_GETDTMF, DMOP_DISCONNECT
--------------------------------------------------------------------------*/
int
examineDynMgrResponse (char *mod, struct MsgToDM *req, struct MsgToApp *res)
{

  switch (res->opcode) {
  case DMOP_GETDTMF:
  case DMOP_DISCONNECT:
    /* Must handle dtmf & disconnect. Make sure it was for me. */
    if (res->appPid != req->appPid) {
      telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
                 TEL_UNEXPECTED_RESPONSE_MSG,
                 res->opcode, res->appPid, res->appRef,
                 req->opcode, req->appPid, req->appRef);
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"%s|%d|returning opcode=%d", __FILE__, __LINE__, res->opcode);
      return (-1);
    }
    if (res->opcode == DMOP_DISCONNECT) {
      /* Note: assumption is we can only get callNum1 or 2 */
      if (res->appCallNum == GV_AppCallNum1) {


        if (GV_BLegCDR == 1 && GV_Connected2 == 1) {
          char portString[32];
          char lMsg[512];
			long bridgeEndTimeSec = 0L;
			char bridgeEndTime[32];

			setTime(bridgeEndTime,&bridgeEndTimeSec);


          sprintf (lMsg, "%s:BE:%s:%s:%s:%s:%s:%s",
                   GV_CDRKey, GV_AppName, GV_ANI, GV_DNIS,
                   bridgeEndTime, GV_CustData1, GV_CustData2);

          sprintf (portString, "%d", GV_AppCallNum2);

          LogARCMsg (mod, REPORT_CDR, portString, "TEL",
                     GV_AppName, 20013, lMsg);
        }

        GV_Connected1 = 0;
        setTime (GV_DisconnectTime, &GV_DisconnectTimeSec);
        tellResp1stPartyDisconnected (mod, res->appCallNum, res->appPid);
        writeEndCDRRecord (mod);
      }
      else{
        if (GV_BLegCDR == 1) {
          char portString[32];
          char lMsg[512];
			long bridgeEndTimeSec = 0L;
			char bridgeEndTime[32];

			setTime(bridgeEndTime,&bridgeEndTimeSec);

          sprintf (lMsg, "%s:BE:%s:%s:%s:%s:%s:%s",
                   GV_CDRKey, GV_AppName, GV_ANI, GV_DNIS,
                   bridgeEndTime, GV_CustData1, GV_CustData2);

          sprintf (portString, "%d", GV_AppCallNum2);

          LogARCMsg (mod, REPORT_CDR, portString, "TEL",
                     GV_AppName, 20013, lMsg);
        }

		if(GV_AppCallNum2 != NULL_APP_CALL_NUM && 
			GV_AppCallNum3 != NULL_APP_CALL_NUM)
		{
		}
		else
		{
        	GV_Connected2 = 0;
		}
      }
    }

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"%s|%d|returning opcode=%d", __FILE__, __LINE__, res->opcode);
    return (res->opcode);
    break;
  default:
    /* Neither dtmf or disconnect. Was it the expected one? */

    switch (res->opcode - req->opcode) {
    case 0:
      /* Response opcode was expected opcode */
      if (res->appPid == req->appPid && res->appRef == req->appRef)
		{
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"%s|%d|returning opcode=%d", __FILE__, __LINE__, req->opcode);
        return (req->opcode);
		}
      /* Note: do not put a break here */
    default:
		if ( req->opcode == DMOP_BLASTDIAL )
		{
			return(req->opcode);
		}
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"%s|%d|res->appPid=%d req->appPid=%d  res->opcode=%d", __FILE__, __LINE__,
			res->appPid, req->appPid, res->opcode);

      if (res->appPid == req->appPid && res->opcode == DMOP_INITIATECALL) {
        memcpy (&GV_BridgeResponse, res, sizeof (struct MsgToApp));

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"%s|%d|res->appPid=%d req->appPid=%d  res->opcode=%d", __FILE__, __LINE__,
			res->appPid, req->appPid, res->opcode);
        return (-1);
      }

      if (((req->opcode == DMOP_SRRECOGNIZE) &&
           (res->opcode == DMOP_SPEAK)) ||
          (res->returnCode == TEL_DISCONNECTED)) {
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"%s|%d|returning -1", __FILE__, __LINE__);
        return (-1);
        break;
      }

      sprintf (Msg, TEL_UNEXPECTED_RESPONSE_MSG,
               res->opcode, res->appPid, res->appRef,
               req->opcode, req->appPid, req->appRef);
      telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err, Msg);
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"%s|%d|returning -1", __FILE__, __LINE__);
      return (-1);
      break;
    }
    break;
  }
}

/*--------------------------------------------------------------------------
This function logs a message for an unexpected response from the dynamic
manager. It may be called from any Telecom Services API.
--------------------------------------------------------------------------*/
int
logUnexpectedResponse (char *mod, struct MsgToApp *response,
                       struct MsgToDM *request)
{
  telVarLog (mod, REPORT_NORMAL, TEL_UNEXPECTED_RESPONSE, GV_Err,
             TEL_UNEXPECTED_RESPONSE_MSG,
             response->opcode, response->appPid, response->appRef,
             request->opcode, request->appPid, request->appRef);
}

/*-------------------------------------------------------------------------
This function is the only function used by all the APIs to send a message
to the dynamic manager.
-------------------------------------------------------------------------*/
int
sendRequestToDynMgr (char *mod, struct MsgToDM *request)
{
  int rc;
  struct Msg_AnswerCall *answerCallRequest;
  struct Msg_DropCall *dropCallRequest;
  struct Msg_InitiateCall *initiateRequest;
  struct Msg_InitTelecom *initTelecomRequest;
  struct Msg_OutputDTMF *outputDTMFRequest;
  struct Msg_PortOperation *portOperationRequest;
  struct Msg_Record *recordRequest;
  //struct Msg_SetKillAppGracePeriod *setKillAppGracePeriodRequest;
  struct Msg_Speak *speakRequest;
  struct Msg_SetGlobal *setGlobal;
  char str[MAX_LENGTH];

  rc = write (GV_RequestFifoFd, (char *) request, sizeof (struct MsgToDM));
  if (rc == -1) {
    telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
               "Can't write message to <%s>. errno=%d.",
               GV_RequestFifo, errno);
    return (-1);
  }
  sprintf (Msg, "Sent %d bytes to <%s>. "
           "msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d",
           rc, GV_RequestFifo,
           request->opcode,
           request->appCallNum,
           request->appPid, request->appRef, request->appPassword);
/* ?? Not all APIs included; Record not complete*/
  switch (request->opcode) {
  case DMOP_ANSWERCALL:
    answerCallRequest = (struct Msg_AnswerCall *) request;
    sprintf (str, ",rings:%d}", answerCallRequest->numRings);
    strcat (Msg, str);
    break;
  case DMOP_DROPCALL:
    dropCallRequest = (struct Msg_DropCall *) request;
    sprintf (str, ",all:%d", dropCallRequest->allParties);
    strcat (Msg, str);
    break;
  case DMOP_SETGLOBAL:
    setGlobal = (struct Msg_SetGlobal *) request;
    sprintf (str, ",name:<%s>,value:%d}", setGlobal->name, setGlobal->value);
    //sprintf(str, ",secs:%d}",setKillAppGracePeriodRequest->seconds);
    strcat (Msg, str);
    break;
#if 0
  case DMOP_SETKILLAPPGRACEPERIOD:
    setKillAppGracePeriodRequest =
      (struct Msg_SetKillAppGracePeriod *) request;
    sprintf (str, ",secs:%d}", setKillAppGracePeriodRequest->seconds);
    strcat (Msg, str);
    break;
#endif
  case DMOP_INITIATECALL:
    initiateRequest = (struct Msg_InitiateCall *) request;
    sprintf (str, ",rings:%d", initiateRequest->numRings);
    strcat (Msg, str);
    sprintf (str, ",ip:%s", initiateRequest->ipAddress);
    strcat (Msg, str);
    sprintf (str, ",ph:%s", initiateRequest->phoneNumber);
    strcat (Msg, str);
    sprintf (str, ",party:%d", initiateRequest->whichOne);
    strcat (Msg, str);
    sprintf (str, ",c#1:%d", initiateRequest->appCallNum1);
    strcat (Msg, str);
    sprintf (str, ",c#2:%d}", initiateRequest->appCallNum2);
    strcat (Msg, str);
    break;
  case DMOP_INITTELECOM:
    initTelecomRequest = (struct Msg_InitTelecom *) request;
    sprintf (str, ",fifo:%s}", initTelecomRequest->responseFifo);
    strcat (Msg, str);
    break;
  case DMOP_OUTPUTDTMF:
    outputDTMFRequest = (struct Msg_OutputDTMF *) request;
    sprintf (str, ",dtmf:%s}", outputDTMFRequest->dtmf_string);
    strcat (Msg, str);
    break;
  case DMOP_PORTOPERATION:
    portOperationRequest = (struct Msg_PortOperation *) request;
    sprintf (str, ",opr:%d", portOperationRequest->operation);
    strcat (Msg, str);
    sprintf (str, ",port:%d}", portOperationRequest->callNum);
    strcat (Msg, str);
    break;
  case DMOP_RECORD:
    recordRequest = (struct Msg_Record *) request;
    sprintf (str, ",party:%d", recordRequest->party);
    strcat (Msg, str);
    sprintf (str, ",file:%s", recordRequest->filename);
    strcat (Msg, str);
    sprintf (str, ",secs:%d", recordRequest->record_time);
    strcat (Msg, str);
    sprintf (str, ",comp:%d", recordRequest->compression);
    strcat (Msg, str);
    sprintf (str, ",ovrw:%d", recordRequest->overwrite);
    strcat (Msg, str);
    break;
  case DMOP_SPEAK:
    speakRequest = (struct Msg_Speak *) request;
    sprintf (str, ",list:%d", speakRequest->list);
    strcat (Msg, str);
    sprintf (str, ",all:%d", speakRequest->allParties);
    strcat (Msg, str);
    sprintf (str, ",sync:%d", speakRequest->synchronous);
    strcat (Msg, str);
    sprintf (str, ",file:%s}", speakRequest->file);
    strcat (Msg, str);
    break;
  default:
    strcat (Msg, "}");
    break;
  }
  telVarLog (mod, REPORT_DETAIL, TEL_BASE, GV_Info, "%s", Msg);
  return (rc);
}

/*------------------------------------------------------------------------------
sendRequestToTTSClient(): Write the request to the FIFO.
------------------------------------------------------------------------------*/
int
sendRequestToTTSClient (const ARC_TTS_REQUEST_SINGLE_DM * ttsRequest)
{
  int i, rc, fd;
  static int got_fifodir = 0;
  static char mod[] = "sendRequestToTTSClient";

  if (got_fifodir == 0) {
    char tts_fifo[128] = "";
    memset (fifo_dir, 0, sizeof (fifo_dir));
    get_fifo_dir (fifo_dir);

    got_fifodir = 1;

    if (GV_MrcpTTS_Enabled) {
      int ttsClientId = GV_AppCallNum1 / 12;
      sprintf (tts_fifo, "/tts.fifo.%d", ttsClientId);
    }
    else {
      sprintf (tts_fifo, "%s", "/tts.fifo");
    }
    strcat (fifo_dir, tts_fifo);
  }

  if ((fd = open (fifo_dir, O_WRONLY | O_NONBLOCK)) == -1) {
    if ((errno == 6) || (errno == 2)) {
      telVarLog (mod, REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
                 "Failed to open request fifo <%s> for writing. errno=%d. [%s], "
                 "ttsClient may not be running or may not be "
                 "connected to tts host.", fifo_dir, errno, strerror (errno));
    }
    else {
      telVarLog (mod, REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
                 "Failed to open request fifo <%s> for writing. errno=%d, [%s].",
                 fifo_dir, errno, strerror (errno));
    }
    return (-1);
  }
  telVarLog (mod, REPORT_VERBOSE, TEL_CANT_OPEN_FIFO, GV_Info,
             "Successfully opened fifo (%s).  fd=%d", fifo_dir, fd);
  for (i = 0; i < 3; i++) {
    rc = write (fd, ttsRequest, sizeof (ARC_TTS_REQUEST_SINGLE_DM));
    if (rc == -1 && errno == EAGAIN) {
      sleep (1);
    }
    else {
      telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
                 "Sent %d bytes to <%s>. "
                 "msg={op:%d,c#:%s,pid:%s,ref:%d,sync=%d,%.*s", rc, fifo_dir,
                 ttsRequest->speakMsgToDM.opcode,
                 ttsRequest->port,
                 ttsRequest->pid,
                 ttsRequest->speakMsgToDM.appRef,
                 ttsRequest->async, 200, ttsRequest->string);
      break;
    }
  }
  if (rc == -1) {
    telVarLog (mod, REPORT_NORMAL, TEL_CANT_WRITE_FIFO, GV_Err,
               "Failed to write to (%s).  [%d, %s]",
               fifo_dir, errno, strerror (errno));
    return (-1);
  }
  close (fd);
  return (0);
}                               /* END: sendRequestToTTSClient() */

/*-----------------------------------------------------------------------------
This routine will read the fifo directory from .Global.cfg file. if no 
directory is found it will assume /tmp
-----------------------------------------------------------------------------*/
static
get_fifo_dir (char *fifo_dir)
{
  char *ispbase, *ptr;
  char cfgFile[128], line[128];
  char lhs[50];
  FILE *fp;
  char logBuf[128];
  static char mod[] = "get_fifo_dir";

  fifo_dir[0] = '\0';

  if ((ispbase = getenv ("ISPBASE")) == NULL) {
    telVarLog (mod, REPORT_NORMAL, TEL_CANT_GET_ISPBASE, GV_Err,
               "Env var ISPBASE not set,setting fifo dir to /tmp");
    sprintf (fifo_dir, "%s", "/tmp");
    return (-1);
  }

  memset (cfgFile, 0, sizeof (cfgFile));
  sprintf (cfgFile, "%s/Global/.Global.cfg", ispbase);

  if ((fp = fopen (cfgFile, "r")) == NULL) {
    telVarLog (mod, REPORT_NORMAL, TEL_CANT_OPEN_FILE, GV_Err,
               "Failed to open global config file (%s) for reading,"
               "errno=%d, setting fifo dir to /tmp", cfgFile, errno);
    sprintf (fifo_dir, "%s", "/tmp");
    return (-1);
  }

  while (fgets (line, sizeof (line) - 1, fp)) {
    if (line[0] == '#') {
      continue;
    }

    if ((char *) strchr (line, '=') == NULL) {
      continue;                 /* line should be : lhs=rhs     */
    }

    ptr = line;

    memset (lhs, 0, sizeof (lhs));

    sprintf (lhs, "%s", (char *) strtok (ptr, "=\n"));
    if (lhs[0] == '\0') {
      continue;                 /* No left hand side!           */
    }

    if (strcmp (lhs, "FIFO_DIR") != 0) {
      continue;                 /* Not our label                */
    }

    ptr = (char *) strtok (NULL, "\n");
    if (ptr == NULL) {
      sprintf (logBuf, "No value specified for %s in %s assuming /tmp",
               FIFO_DIR, cfgFile);
      telVarLog (mod, REPORT_NORMAL, TEL_CANT_FIND_IN_FILE, GV_Err, logBuf);
      sprintf (fifo_dir, "%s", "/tmp");
      fclose (fp);
      return (-1);
    }

    sprintf (fifo_dir, "%s", ptr);

    break;
  }                             /* while */
  fclose (fp);

  if (access (fifo_dir, F_OK) != 0) {
    telVarLog (mod, REPORT_NORMAL, TEL_CANT_ACCESS_FILE, GV_Err,
               "fifo_dir <%s> doesn't exist, assuming /tmp", fifo_dir);
    sprintf (fifo_dir, "%s", "/tmp");
  }

//#ifdef DEBUG
 // fprintf (stderr, "fifo directory set to >%s<\n", fifo_dir);
//#endif
  return (0);
}

/*---------------------------------------------------------------------------
This function does intial checking for an API and writes appropriate error
messages. It should be called by ALL APIs immediately upon starting. 
This function can be used to log all incoming API calls with their parameters
and will be used to send information to the monitor. This is the only function
in that should call the "send_to_monitor" API.
Note: Sending to monitor is implemented so that it can be turned off. That is,
	if for one instance of the API call you do not want it sent to the
	monitor, just set GV_SendToMonitor to 0. This variable is then
	turned back on. This allows you to call an API from within an API
	and not affect the monitor.

	Note: We may not need to use GV_SendToMonitor if we implement
		apis that are called from other api's by breaking the
		callable apis into a header section and then a section
		that actually accomplishes the work. In this case, 
		the latter function will be called directly from the other
		API.
---------------------------------------------------------------------------*/
int
apiInit (char *mod, int apiId, char *apiAndParms,
         int initRequired, int party1Required, int party2Required)
{
  if (initRequired && !GV_InitCalled) {
    telVarLog (mod, REPORT_NORMAL, TEL_TELECOM_NOT_INIT, GV_Err,
               TEL_TELECOM_NOT_INIT_MSG);
    return (TEL_FAILURE);
  }
  if (party1Required && !GV_Connected1) {
    telVarLog (mod, REPORT_DETAIL, TEL_BASE, GV_Err,
               "Party 1 is not connected.");
    return (TEL_DISCONNECTED);
  }
  if (party2Required && !GV_Connected2) {
    telVarLog (mod, REPORT_DETAIL, TEL_BASE, GV_Err,
               "Party 2 is not connected.");
    return (-10);
  }
  if (GV_DontSendToTelMonitor)
    GV_DontSendToTelMonitor = 0;        /* Reset */
  else
    send_to_monitor (MON_API, apiId, apiAndParms);

  return (0);
}


/*-----------------------------------------------------------------------------
Given a parameter value defined in Telecom.h, this routine will return its
"mnemonic" equivalent. It is used for showing the mnemonics of passed
parameters when calling send_to_monitor.
-----------------------------------------------------------------------------*/
const char *
arc_val_tostr (int parameter_value)
{

  static const char *ISP_Parm[] = {
    "Unknown",                  /* 0 */
    "S_AMENG",
    "S_FRENCH",
    "S_GERMAN",
    "S_DUTCH",
    "S_SPANISH",
    "S_BRITISH",
    "S_FLEMISH",
    "Unknown",
    "Unknown",
    "NUMERIC",                  /* 10 */
    "ALPHA",
    "NUMSTAR",
    "ALPHANUM",
    "Unknown",
    "Unknown",
    "Unknown",
    "AUTO", "AUTOSKIP",
    "MANDATORY",
    "ONHOOK",                   /* 20 */
    "OFFHOOK",
    "DIAL_OUT",
    "CONNECT",
    "CONNECT",
    "ALL",
    "INTERRUPT",
    "NONINTERRUPT",
    "FIRST_PARTY_INTERRUPT",
    "SECOND_PARTY_INTERRUPT",
    "PHRASE",                   /* 30 */
    "DOLLAR",
    "NUMBER",
    "DIGIT",
    "DAY",
    "DATE_YTT",
    "DATE_MMDD",
    "DATE_MMDDYY",
    "DATE_MMDDYYYY",
    "DATE_DDMM",
    "DATE_DDMMYY",              /* 40 */
    "DATE_DDMMYYYY",
    "TIME_STD",
    "TIME_STD_MN",
    "Unknown",
    "TIME_MIL",
    "TIME_MIL_MN", "Unknown",
    "Unknown",
    "Unknown",
    "MILITARY",                 /* 50 */
    "STANDARD",
    "TICS",
    "MM_DD_YY",
    "DD_MM_YY",
    "YY_MM_DD",
    "Unknown",
    "Unknown",
    "Unknown",
    "ENABLE",
    "DISABLE",                  /* 60 */
    "Unknown",
    "Unknown",
    "Unknown",
    "PHRASE_TAG",
    "PHRASENO",
    "INTEGER",
    "DOUBLE",
    "SOCSEC",
    "PHONE",
    "STRING",                   /* 70 */
    "Unknown",
    "FIRST_PARTY",
    "SECOND_PARTY",
    "BOTH_PARTIES",
    "YESBEEP", "NOBEEP",
    "NAMERICAN",
    "NONE",
    "Unknown",
    "PVEVOCAB",                 /* 80 */
    "STDVOCAB",
    "Unknown",
    "Unknown",
    "Unknown",
    "L_AMENG",
    "L_FRENCH",
    "L_GERMAN",
    "L_BRITISH",
    "L_SPANISH",
    "L_DUTCH",                  /* 90 */
    "L_FLEMISH",
    "Unknown",
    "Unknown",
    "TEXT_FILE",
    "PHRASE_FILE",
    "SYSTEM_PHRASE",
    "Unknown",
    "Unknown",
    "Unknown",
    "SYNC",                     /* 100 */
    "ASYNC",
    "SENTENCE",
    "YES",
    "NO",
    "Unknown",
    "Unknown",
    "COMP_24ADPCM",
    "COMP_32ADPCM",
    "COMP_48PCM",
    "COMP_64PCM",               /* 110 */
    "COMP_WAV",
    "SCHEDULED_DNIS_AND_APP",
    "STANDALONE_APP",
    "SCHEDULED_DNIS_ONLY",
    "CHECK_DISCONNECT",
    "CLEAR_CHANNEL",
    "SPEAK_AND_KEEP",
    "SPEAK_AND_DELETE",
    "DONT_SPEAK_AND_KEEP",
    "LISTEN_ALL",               /* 120 */
    "PLAYBACK_CONTROL",         /* 121 */
    "SECOND_PARTY_NO_CALLER",   /* 122 */
    "FIRST_PARTY_PLAYBACK_CONTROL",     /* 123 */
    "SECOND_PARTY_PLAYBACK_CONTROL",    /* 124 */
  };

  const int Max_isp_parm = (sizeof (ISP_Parm) / sizeof (char *)) - 1;

  if ((parameter_value < 0) || (parameter_value > Max_isp_parm))
    return ("INVALID");

  return (ISP_Parm[parameter_value]);
}

/*----------------------------------------------------------------------
Alarm timeout routine. This routine requires an integer to be passed.
It is not used. It resets the alarm and causes control to go to the 
point where setjmp() is called and puts its second parameter as a
return value of setjmp().
----------------------------------------------------------------------*/
void
handle_timeout (int dummy)
{
  alarm (0);
  signal (SIGALRM, NULL);
  siglongjmp (env_alrm, 1);
}

/*----------------------------------------------------------------------
This function returns a disconnect return code, depending upon which
party disconnected.
----------------------------------------------------------------------*/
int
disc (char *mod, int callNum)
{
  if (callNum == GV_AppCallNum1)
    return (TEL_DISCONNECTED);
  if (callNum == GV_AppCallNum2 || callNum == GV_AppCallNum3) {
    /*      GV_AppCallNum2= NULL_APP_CALL_NUM; */
    return (-10);
  }
  telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
             "Unknown appCallNum <%d> in response. Expected %d or %d or %d",
             callNum, GV_AppCallNum1, GV_AppCallNum2, GV_AppCallNum3);
  return (TEL_FAILURE);
}

/*----------------------------------------------------------------------
This function saves data into the appropriate type ahead buffer, based
on the call number passed to it.
----------------------------------------------------------------------*/
int
saveTypeAheadData (char *mod, int callNum, char *data)
{
  static char clearSignal[] = "CLEAR";
  telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
             "In saveTypeAhead. data=<%s>", data);
  if (callNum == GV_AppCallNum1) {
    if (strcmp (data, clearSignal) == 0) {
      memset (GV_TypeAheadBuffer1, '\0', sizeof (GV_TypeAheadBuffer1));
      telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
                 "Clearing DTMF buffer 1");
    }
    else
      strcat (GV_TypeAheadBuffer1, data);
  }
  else if (callNum == GV_AppCallNum2 || callNum == GV_AppCallNum3) {
    if (strcmp (data, clearSignal) == 0) {
      memset (GV_TypeAheadBuffer2, '\0', sizeof (GV_TypeAheadBuffer2));
      telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
                 "Clearing DTMF buffer 2");
    }
    else
      strcat (GV_TypeAheadBuffer2, data);
  }
  else {
    sprintf (Msg,
             "Unknown appCallNum <%d> in response. "
             "Expected %d or %d. DTMF input <%s> discarded.",
             callNum, GV_AppCallNum1, GV_AppCallNum2, data);
  }
  return (0);
}

/*----------------------------------------------------------------------
This function determines if the phone number or IP address passed is 
appropriate to the value of the informat parameter. It also parses out 
any extraneous characters and returns a "cleaned" version of the phone number.
Return codes: 0=invalid phone number, 1=good phone number
?? NOTE THIS ROUTINE MUST BE COMPLETED
----------------------------------------------------------------------*/
int
goodDestination (char *mod, int informat,
                 char *destination, char *editedDestination)
{
  /* ?? Just done for now, we need to edit properly */
  strcpy (editedDestination, destination);
  switch (informat) {
  case NAMERICAN:
    break;
  case NONE:
    break;
  case IP:
    break;
  case E_164:
    break;
  default:
    telVarLog (mod, REPORT_NORMAL, TEL_INVALID_PARM,
               GV_Err,
               "Invalid destination <%s> for informat: %d.",
               destination, informat);
    return (0);
    break;
  }
  return (1);
}

/*------------------------------------------------------------------------------
Program:	get_name_value
Purpose:	This routine gets a name value pair from a file. If no header
		is specified, the first instance of the name found in the file
		is processed.
		This routine was adapted from gaGetProfileString, but has no
		dependencies on gaUtils.
		Parameters: 
		section: name of section in file, e.g. [Header]. If this is
			NULL (""), the whole file is searched regardless of
			headers.
		name: The "name" of a "name=value" pair.
		defaultVal: The value to return if name is not found in file.
		value: The "value" of a "name=value" pair. This what you are
			looking for in the file.
		bufSize: Maximum size of the value that can be returned. 
			This is so the routine will not cause you to core dump.
		file: the name of the file to search in
		err_msg: The errror message if any.
		Values returned are: 0 (success), -1 (failure)
Author:		George Wenzel
Date:		06/17/99
------------------------------------------------------------------------------*/
int
get_name_value (char *section, char *name,
                char *defaultVal, char *value,
                int bufSize, char *file, char *err_msg)
{
  static char mod[] = "util_get_name_value";
  FILE *fp;
  char line[512];
  char currentSection[80], lhs[256], rhs[256];
  int found, inDesiredSection = 0, sectionSpecified = 1;
  sprintf (value, "%s", defaultVal);
  err_msg[0] = '\0';
  if (section[0] == '\0') {
    /* If no section specified, we're always in desired section */
    sectionSpecified = 0;
    inDesiredSection = 1;
  }
  if (name[0] == '\0') {
    sprintf (err_msg, "No name passed to %s.", mod);
    return (-1);
  }
  if (file[0] == '\0') {
    sprintf (err_msg, "No file name passed to %s.", mod);
    return (-1);
  }
  if (bufSize <= 0) {
    sprintf (err_msg,
             "Return bufSize (%d) passed to %s must be > 0.", bufSize, mod);
    return (-1);
  }

  if ((fp = fopen (file, "r")) == NULL) {
    sprintf (err_msg,
             "Failed to open file <%s>. errno=%d. [%s]",
             file, errno, strerror (errno));
    return (-1);
  }

  memset (line, 0, sizeof (line));  found = 0;
  while (fgets (line, sizeof (line) - 1, fp)) {
    /*  Strip \n from the line if it exists */
    if (line[(int) strlen (line) - 1] == '\n') {
      line[(int) strlen (line) - 1] = '\0';
    }

    trim_whitespace (line);
    if (!inDesiredSection) {
      if (line[0] != '[') {
        memset (line, 0, sizeof (line));
        continue;
      }

      memset (currentSection, 0, sizeof (currentSection));
      sscanf (&line[1], "%[^]]", currentSection);
      if (strcmp (section, currentSection) != 0) {
        memset (line, 0, sizeof (line));
        continue;
      }
      inDesiredSection = 1;
      memset (line, 0, sizeof (line));
      continue;
    }
    else {
      /* If we are already in a section and have encountered
         another section AND a section was specified then
         get out of the loop.  */
      if (line[0] == '[' && sectionSpecified) {
        memset (line, 0, sizeof (line));
        break;
      }
    }

    memset (lhs, 0, sizeof (lhs));
    memset (rhs, 0, sizeof (rhs));
    if (sscanf (line, "%[^=]=%[^=]", lhs, rhs) < 2) {
      memset (line, 0, sizeof (line));
      continue;
    }
    trim_whitespace (lhs);
    if (strcmp (lhs, name) != 0) {
      memset (line, 0, sizeof (line));
      continue;
    }
    found = 1;
    sprintf (value, "%.*s", bufSize, rhs);
    break;
  }                             /* while fgets */
  fclose (fp);
  if (!found) {
    if (sectionSpecified)
      sprintf (err_msg,
               "Failed to find <%s> under section <%s> in file <%s>.",
               name, section, file);
    else
      sprintf (err_msg,
               "Failed to find <%s> entry in file <%s>.", name, file);
    return (-1);
  }
  return (0);
}

/*----------------------------------------------------------------------------
Trim white space from the front and back of a string.
----------------------------------------------------------------------------*/
static int
trim_whitespace (char *string)
{
  int sLength;
  char *tmpString;
  char *ptr;
  if ((sLength = strlen (string)) == 0)
    return (0);
  tmpString = (char *) calloc ((size_t) (sLength + 1), sizeof (char));
  ptr = string;
  if (isspace (*ptr)) {
    while (isspace (*ptr))
      ptr++;
  }

  sprintf (tmpString, "%s", ptr);
  ptr = &tmpString[(int) strlen (tmpString) - 1];
  while (isspace (*ptr)) {
    *ptr = '\0';
    ptr--;
  }

  sprintf (string, "%s", tmpString);
  free (tmpString);
  return (0);
}

/*----------------------------------------------------------------------------
This function informs Responsibility that the application's first party 
disconnected. ?? Need the shared memory routine from Madhav.
----------------------------------------------------------------------------*/
int
tellResp1stPartyDisconnected (char *mod, int callNum, int pid)
{

  int rc;
  static int appRef = 1;
  struct MsgToRes msg;
  telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
             "Telling responsibility that pid=%d, callNum=%d disconnected in <%s>.",
             pid, callNum, mod);
  tran_shmid = shmget (SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);
  if ((tran_tabl_ptr = shmat (tran_shmid, 0, 0)) == (char *) -1) {
    printf ("shmat failed, errno=%d\n", errno);
    return (-1);
  }

  write_fld (tran_tabl_ptr, GV_shm_index, APPL_SIGNAL, 2);
  shmdt (tran_tabl_ptr);        /* detach the shared memory segment */
  return (0);
}

/*----------------------------------------------------------------------------
This function writes a start CDR record.
Format of msg: key:SC:termcode:mmddyyyyhhmmsshh:cust1:cust2
key=host-port-startTime
----------------------------------------------------------------------------*/
int
writeStartCDRRecord (char *mod)
{
  int static startCDRWritten = 0;
  char hostName[32];
  char currentTimeString[32];
  time_t currentTimeTics;
  if (startCDRWritten) {
    sprintf (Msg, "Attempt to write duplicate start CDR record.");
    telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Warn, Msg);
    return (0);
  }

  memset (hostName, '\0', sizeof (hostName));
  gethostname (hostName, sizeof (hostName) - 1);
  setTime (currentTimeString, &currentTimeTics);
#if 0
  sprintf (GV_CDRKey, "%s-PORT%d-%s",
           hostName, GV_AppCallNum1, currentTimeString);
#endif
  sprintf (GV_CDRKey, "%s-PORT%d-%s",
           hostName, GV_AppCallNum1, GV_RingEventTime);
  sprintf (GV_CDR_Key, "%s", GV_CDRKey);
  sprintf (Msg, "%s:SC:%s:%s:%s:%s:%s:%s",
           GV_CDRKey, GV_AppName, GV_ANI, GV_DNIS,
           GV_ConnectTime, GV_CustData1, GV_CustData2);
  /*telCDRLog(mod,REPORT_NORMAL, 20010, 99, Msg); */
  telCDRLog (mod, REPORT_CDR, 20010, 99, Msg);
  startCDRWritten = 1;
  return (0);
}

/*--------------------------------------------------------------------------
Construct Call Detail Recording Key, a unique key used for the life of the call.
Its format is: host_name + PORT# + time of call to 100th of second.
--------------------------------------------------------------------------*/
int
setCDRKey (char *mod, char *port_no)
{
  char host[32];
  int ret;
  static int first_time = 1;
  static char prev_key[50];
  char host_name[32];
  char *ptr1, *ptr2;
  if (first_time) {
    first_time = 0;
    strcpy (prev_key, "");
  }

  memset (host, 0, sizeof (host));
  memset (host_name, 0, sizeof (host_name));
  if ((ret = gethostname (host_name, sizeof (host_name) - 1)) == -1) {
    sprintf (Msg, "gethostname failed. errno=%d.", errno);
    telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err, Msg);
    return (-1);
  }

#if 0
  ptr2 = host_name;
  ptr1 = strchr (host_name, '.');
  if (ptr1 != NULL) {
    strncpy (host, host_name, strlen (ptr2) - strlen (ptr1));
  }
  else {
    strncpy (host, host_name, 8);
    host[31] = '\0';
  }
#endif


  /*DDN TO BE USED INSTEAD OF THE CODE ABOVE */

  host_name[31] = '\0';
  if ((ptr2 = strchr (host_name, '.')) != NULL) {
    *ptr2 = '\0';
  }

  sprintf (host, "%s", host_name);
  sprintf (GV_CDRKey, "%s-PORT%s-%s", host, port_no, GV_RingEventTime);
  sprintf (GV_CDR_Key, "%s", GV_CDRKey);
  /* If previous key is not null, we're creating a second key */
  /* This is allowed, but unusual, so we log it in diagnose mode */
  if (strcmp (prev_key, "")) {
    sprintf (Msg, "CDR key set twice. <%s> and <%s>.", prev_key, GV_CDRKey);
    telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Warn, Msg);
  }

  strcpy (prev_key, GV_CDRKey);
  return (0);
}

/*----------------------------------------------------------------------------
This function writes an end CDR record.
Format of msg: key:EC:termcode:mmddyyyyhhmmsshh:cust1:cust2
key=host-port-startTime
----------------------------------------------------------------------------*/
int
writeEndCDRRecord (char *mod)
{
  int static endCDRWritten = 0;
  char finalStatus[3];
  if (endCDRWritten) {
    sprintf (Msg, "Attempt to write duplicate end CDR record.");
    telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Warn, Msg);
    return (0);
  }

  if (GV_TerminatedViaExitTelecom)
    strcpy (finalStatus, "01");
  else
    strcpy (finalStatus, "02");
  sprintf (Msg, "%s:EC:%s:%s:%s:%s",
           GV_CDRKey, finalStatus,
           GV_DisconnectTime, GV_CustData1, GV_CustData2);
  /*telCDRLog(mod,REPORT_NORMAL, 20011, 99, Msg); */
  telCDRLog (mod, REPORT_CDR, 20011, 99, Msg);
  endCDRWritten = 1;
  return (0);
}

/*----------------------------------------------------------------------------
This function writes a custom CDR record.
Format of msg: key:<Custom Key>:termcode:mmddyyyyhhmmsshh:cust1:cust2
key=host-port-startTime
----------------------------------------------------------------------------*/
int
writeCustomCDRRecord (char *mod,
                      char *custom_key, char *custom_time, int msgId)
{
  char hostName[32];
  char currentTimeString[32];
  time_t currentTimeTics;
  char yKey[3];
  char *pChar;
  if (custom_key == NULL || custom_key[0] == '\0' || strlen (custom_key) > 2) {
    sprintf (yKey, "%s", "CC");
  }
  else {
    sprintf (yKey, "%s", custom_key);
  }

  memset (hostName, '\0', sizeof (hostName));
  gethostname (hostName, sizeof (hostName) - 1);
  if ((pChar = strchr (hostName, '.')) != NULL) {
    *pChar = '\0';
  }

  if (custom_time == 0 || *custom_time == 0) {
    setTime (currentTimeString, &currentTimeTics);
  }
  else {
    sprintf (currentTimeString, "%s", custom_time);
  }

  sprintf (Msg, "%s:%s:%s:%s:%s:%s:%s:%s",
           GV_CDRKey, yKey, GV_AppName, GV_ANI,
           GV_DNIS, GV_ConnectTime, GV_CustData1, GV_CustData2);
  /*telCDRLog(mod,REPORT_NORMAL, 20010, 99, Msg); */
  telCDRLog (mod, REPORT_CDR, msgId, 99, Msg);
  return (0);
}

/*--------------------------------------------------------------------------
Set a string to the time in format: MMDDYYHHMMSSXX where XX is 100ths of sec.
--------------------------------------------------------------------------*/
void
setTime (char *time_str, long *tics)
{
  int rc;
  struct timeval tp;
  struct timezone tzp;
  struct tm *tm;
  if ((rc = gettimeofday (&tp, &tzp)) == -1) {
    strcpy (time_str, "0000000000000000");
    time (tics);
    return;
  }

  tm = localtime (&tp.tv_sec);
  sprintf (time_str, "%02d%02d%04d%02d%02d%02d%02d",
           tm->tm_mon + 1, tm->tm_mday,
           tm->tm_year + 1900, tm->tm_hour,
           tm->tm_min, tm->tm_sec, tp.tv_usec / 10000);
  *tics = tp.tv_sec;
  return;
}

int
writeFieldToSharedMemory (char *mod, int slot, int type, int value)
{
  tran_shmid = shmget (SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);
  if ((tran_tabl_ptr = shmat (tran_shmid, 0, 0)) == (char *) -1) {
    printf ("%s:shmat failed, errno=%d\n", mod, errno);
    return (-1);
  }

  write_fld (tran_tabl_ptr, slot, type, value);
  shmdt (tran_tabl_ptr);        /* detach the shared memory segment */
  return (0);
}

int
writeStringToSharedMemory (char *mod, int slot, char *valueString)
{
  char *ptr, *ptr1;
  int lengthOfValuestring = strlen (valueString);
  tran_shmid = shmget (SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);
  if ((tran_tabl_ptr = shmat (tran_shmid, 0, 0)) == (char *) -1) {
    printf ("%s:shmat failed, errno=%d\n", mod, errno);
    return (-1);
  }

  ptr = tran_tabl_ptr;
  ptr += (slot * SHMEM_REC_LENGTH);
  /* position the pointer to the field */
  ptr += vec[APPL_NAME - 1];    /* application start index */
  ptr1 = ptr;
  (void) memset (ptr1, ' ', MAX_APPL_NAME);
  ptr += (MAX_APPL_NAME - lengthOfValuestring);
  (void) memcpy (ptr, valueString, lengthOfValuestring);
  return (0);
}

/*----------------------------------------------------------------------------	
This function extracts characters from a buffer, treating it like a fifo. 
That is, up to maxLength characters are removed from the beginning of the
string and returned. 
The function returns the number of characters extracted into toString.
Example: With fromString="12345", maxLength=2; toString should be "12", 
	 fromString should be left as "345" and the function should return 2,
	 the number of characters extracted from fromString into toString.
----------------------------------------------------------------------------*/
int
get_characters (char *mod, char *toString,
                char *fromString,
                char termChar, int maxLength,
                char *allowableChars, int *termCharPressed)
{
  int numCharsExtracted = 0;    /* # of valid characters returned */
  int pos;                      /* position being examinined */
  int gotTermChar = 0;
  char holdString[MAX_LENGTH];
  char holder[2];
  *termCharPressed = 0;
  if ((strlen (fromString) == 0)
      || (maxLength == 0)) {
    return (0);
  }

  /* Terminate the string which will hold one byte only */
  holder[1] = '\0';
  memset (holdString, 0, sizeof (holdString));
  strcpy (holdString, fromString);
  pos = 0;
  while (!gotTermChar && numCharsExtracted < maxLength) {
    if (holdString[pos] != '\0') {
      holder[0] = holdString[pos];
      if (holder[0] == termChar) {
        pos++;
        gotTermChar = 1;
        *termCharPressed = 1;
      }
      else {
        if (strchr (allowableChars, holder[0]) != NULL) {
          pos++;
          strcat (toString, holder);
          numCharsExtracted++;
        }
        else {
          pos++;
          telVarLog (mod, REPORT_NORMAL, TEL_BASE,
                     GV_Err, "Invalid character <%c>.", holder[0]);
        }
      }
    }
    else {
      /* holdString is empty so can't extract any more */
      break;
    }
  }

  /* Adjust fromString to account for all characters examined */
  strcpy (fromString, &holdString[pos]);
  return (numCharsExtracted);
}

void
proc_sigterm ()
{
#ifndef CLIENT_GLOBAL_CROSSING
  if(LogOff) 
#endif
	{
      LogOff ();
	}

  TEL_ExitTelecom ();
  exit (0);
  return;
}

FILE *
modifiedPopen (const char *zpCommand, const char *zpType, pid_t * zpChildPid)
{
  int lPipeFd[2];
  pid_t lChildPid;
  FILE *lpFilePtr;
  if (((zpType[0] != 'r') && (zpType[0] != 'w'))
      || (zpType[1] != 0)) {
    errno = EINVAL;
    return (NULL);
  }

  if (pipe (lPipeFd) < 0)
    return (NULL);
  if ((lChildPid = fork ()) < 0)
    return (NULL);
  else if (lChildPid == 0) {
    /* Child Process */
    switch (zpType[0]) {
    case 'r':
      close (lPipeFd[0]);
      if (lPipeFd[1] != STDOUT_FILENO) {
        dup2 (lPipeFd[1], STDOUT_FILENO);
        close (lPipeFd[1]);
      }
      break;
    default:
      close (lPipeFd[1]);
      if (lPipeFd[0] != STDIN_FILENO) {
        dup2 (lPipeFd[0], STDIN_FILENO);
        close (lPipeFd[0]);
      }
      break;
    }

    execl ("/bin/sh", "sh", "-c", zpCommand, (char *) 0);
    _exit (127);
  }

  /* Parent process */
  switch (zpType[0]) {
  case 'r':
    close (lPipeFd[1]);
    if ((lpFilePtr = fdopen (lPipeFd[0], zpType)) == NULL)
      return (NULL);
    break;
  default:
    close (lPipeFd[0]);
    if ((lpFilePtr = fdopen (lPipeFd[1], zpType)) == NULL)
      return (NULL);
    break;
  }

  *zpChildPid = lChildPid;
  return (lpFilePtr);
  return (0);
}

int
modifiedPclose (FILE * zpFilePtr, pid_t zChildPid)
{
  int lStatus;
  if (zChildPid == 0)
    return (-1);
  if (fclose (zpFilePtr) == EOF)
    return (-1);
  errno = 0;
  while (waitpid (zChildPid, &lStatus, 0) < 0) {
    if (errno != EINTR)
      return (-1);
  }

  return (0);
}

int
get_first_phrase_extension (char *mod, char *directory, char *extension)
{
  char command[256];
  char error_msg[512];
  int last_position;
  FILE *fp;
  pid_t lChildProcessPid;
  char *pLastDot;
  char phrase[512];
  if (access (directory, F_OK) != 0) {
    telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
               "Directory <%s> does not exist. Cannot access the first system phrase.",
               directory);
    return (-1);
  }

  sprintf (command, "ls -1 %s", directory);
  fp = modifiedPopen (command, "r", &lChildProcessPid);
  if (fp == NULL) {
    telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
               "Failed to get the first system phrase file in %s. errno=%d.",
               directory, errno);
    return (-1);
  }
  memset (phrase, '\0', sizeof (phrase));
  fgets (phrase, sizeof (phrase) - 1, fp);
  modifiedPclose (fp, lChildProcessPid);
  if (strcmp (phrase, "") == 0) {
    telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
               "Directory <%s> appears to be empty. Cannot access the first system "
               "phrase.", directory);
    return (-1);
  }

  /* Chop off trailing CR if one is found */
  last_position = strlen (phrase) - 1;
  if (phrase[last_position] == '\n')
    phrase[last_position] = '\0';
  if ((pLastDot = strrchr (phrase, '.')) == NULL) {
    telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
               "There is no extension to the first file <%s> in the directory <%s>.",
               phrase, directory);
    return (-1);
  }

  sprintf (extension, "%s", ++pLastDot);
  return (0);
}

char *
convertTokenType (int zType)
{
  switch (zType) {
  case SR_PATH_WORD:
    return ("SR_PATH_WORD");
    break;
  case SR_PATH_WORD_CONF:
    return ("SR_PATH_WORD_CONF");
    break;
  case SR_CONCEPT_WORD:
    return ("SR_CONCEPT_WORD");
    break;
  case SR_CONCEPT_WORD_CONF:
    return ("SR_CONCEPT_WORD_CONF");
    break;
  case SR_TAG:
    return ("SR_TAG");
    break;
  case SR_TAG_CONFIDENCE:
    return ("SR_TAG_CONFIDENCE");
    break;
  case SR_CONCEPT:
    return ("SR_CONCEPT");
    break;
  case SR_CONCEPT_CONFIDENCE:
    return ("SR_CONCEPT_CONFIDENCE");
    break;
  case SR_CATEGORY:
    return ("SR_CATEGORY");
    break;
  case SR_ATTRIBUTE_VALUE:
    return ("SR_ATTRIBUTE_VALUE");
    break;
  case SR_ATTRIBUTE_NAME:
    return ("SR_ATTRIBUTE_NAME");
    break;
  case SR_ATTRIBUTE_CONCEPTS:
    return ("SR_ATTRIBUTE_CONCEPTS");
    break;
  case SR_ATTRIBUTE_TYPE:
    return ("SR_ATTRIBUTE_TYPE");
    break;
  case SR_ATTRIBUTE_STATUS:
    return ("SR_ATTRIBUTE_STATUS");
    break;
  case SR_ATTRIBUTE_CONFIDENCE:
    return ("SR_ATTRIBUTE_CONFIDENCE");
    break;
  case SR_LEARN:
    return ("SR_LEARN");
    break;
  case SR_REC:
    return ("SR_REC");
    break;
  case SR_CONCEPT_AND_ATTRIBUTE_PAIR:
    return ("SR_CONCEPT_AND_ATTRIBUTE_PAIR");
    break;
  case SR_CONCEPT_AND_WORD:
    return ("SR_CONCEPT_AND_WORD");
    break;
  default:
    return ("UNKNOWN");
    break;
  }

  return ("UNKNOWN");
}

/*--------------------------------------------------------------------------
	Set Global variable GV_CDR_Time in format:
	MMDDYYHHMMSSXX where XX is 100ths of seconds, and return it.
--------------------------------------------------------------------------*/
char *
get_time ()
{
  int ret_code;
  struct timeval tp;
  struct timezone tzp;
  struct tm *tm;
  static char current_time[20];
  if ((ret_code = gettimeofday (&tp, &tzp)) == -1) {
    return ((char *) -1);
  }

  tm = localtime (&tp.tv_sec);
  sprintf (current_time,
           "%02d%02d%04d%02d%02d%02d%02d",
           tm->tm_mon + 1, tm->tm_mday,
           tm->tm_year + 1900, tm->tm_hour,
           tm->tm_min, tm->tm_sec, tp.tv_usec / 10000);
  return (current_time);
}

//#ifdef MRCP_SR
#if 1

int GV_SRFifoFd;
static char GV_SRFifo[100] = "";
int
openChannelToSRClient (char *mod)
{
  int rc;
  char mrcpVersion[8] = "";
  char Msg[256];
  char value[16];
  char tmpSRFifo[128];
  char fifoDir[128];
  char tmpMsg2[256];
  if (GV_SRFifo[0] == '\0') {
    memset ((char *) fifoDir, '\0', sizeof (fifoDir));
    if ((rc =
         UTL_GetArcFifoDir (MRCP_FIFO_INDEX, fifoDir,
                            sizeof (fifoDir), Msg, sizeof (Msg))) != 0) {
      sprintf (fifoDir, "%s", "/tmp");
      sprintf (tmpMsg2, "Defaulting fifo directory to %s.  %s", fifoDir, Msg);
      telVarLog (mod, REPORT_VERBOSE, TEL_CANT_OPEN_FIFO, GV_Info, tmpMsg2);
    }

    sprintf (value, "%s", "MrcpVersion");
    rc = get_name_value ("", value, "1.0",
                         mrcpVersion,
                         sizeof (mrcpVersion), GV_TelecomConfigFile, Msg);
    if (rc != 0) {
      sprintf (GV_SRFifo, "%s/%s.%d", fifoDir,
               MRCP_TO_SR_CLIENT, GV_AppCallNum1);
      telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Warn,
                 "Failed to find '%s' in <%s>. Using <%s>.",
                 value, GV_TelecomConfigFile, mrcpVersion);
    }

    if (mrcpVersion[0] == '2') {
      if (GV_TotalSRRequests > 0) {
        sprintf (GV_SRFifo, "%s/%s.%d", fifoDir,
                 MRCP_TO_SR_CLIENT2, GV_AttchaedSRClient);
      }
      else {
        sprintf (GV_SRFifo, "%s/%s", fifoDir, MRCP_TO_SR_CLIENT2);
      }
    }
    else {
      sprintf (GV_SRFifo, "%s/%s.%d", fifoDir,
               MRCP_TO_SR_CLIENT, GV_AppCallNum1);
    }
  }

  if ((mkfifo (GV_SRFifo, S_IFIFO | 0666) < 0)
      && (errno != EEXIST)) {
    telVarLog (mod, REPORT_NORMAL,
               TEL_CANT_CREATE_FIFO, GV_Err,
               TEL_CANT_CREATE_FIFO_MSG, GV_SRFifo, errno, strerror (errno));
    return (-1);
  }

  if ((GV_SRFifoFd = open (GV_SRFifo, O_RDWR)) < 0) {
    telVarLog (mod, REPORT_NORMAL, TEL_CANT_OPEN_FIFO,
               GV_Err, TEL_CANT_OPEN_FIFO_MSG,
               GV_SRFifo, errno, strerror (errno));
    return (-1);
  }
  telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
             "Successfully opened mrcpClient fifo (%s).  fd=%d",
             GV_SRFifo, GV_SRFifoFd);
  return (0);
}

int
closeChannelToSRClient (char *mod)
{

  if (GV_SRFifoFd > 0) {
    close (GV_SRFifoFd);
    GV_SRFifoFd = -1;
    GV_SRFifo[0] = '\0';
  }

  return (0);
}


int
sendRequestToSRClient (char *mod, struct MsgToDM *request)
{
  int rc;
  request->appPassword = request->appCallNum;
  rc = write (GV_SRFifoFd, (char *) request, sizeof (struct MsgToDM));
  if (rc == -1) {
    telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
               "Can't write message to <%s>. errno=%d.", GV_SRFifo, errno);
    return (-1);
  }
  telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
             "Sent %d bytes to <%s, fd=%d>. "
             "msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d",
             rc, GV_RequestFifo, GV_SRFifoFd,
             request->opcode, request->appCallNum,
             request->appPid, request->appRef, request->appPassword);
  return (rc);
}

#endif
