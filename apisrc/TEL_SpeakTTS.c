#ident	"@(#)TEL_SpeakTTS 97/08/13 2.2.0 Copyright 1997 Aumtech, Inc."
/*------------------------------------------------------------------------------
File:		TEL_SpeakTTS.c
Purpose:	Convert text to speech using ScanSoft TTS.
Author:		Sandeep Agate
Update: 11/04/2002 apj Added PUT_QUEUE_ASYNC.
Update: 11/11/2002 apj Added counter to generated file name.
Update: 16/08/2004 mpb If tel_speak fails, return rc instead of -1.
Update: 12/02/2009 djb MR# 2793.  Added not DONT_SPEAK_AND_KEEP for -3 return.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "Telecom.h"

#include "ttsStruct.h"

typedef struct
{
    char      rId[4];
    unsigned long rLen;
    char      wId[4];
    char      fId[4];
    unsigned long   fLen;
    unsigned short  nFormatTag;
    unsigned short  nChannels;
    unsigned long   nSamplesPerSec;
    unsigned long   nAvgBytesPerSec;
    unsigned short  nBlockAlign;
    unsigned short  nBitsPerSample;
    char      dId[4];
    unsigned long   dLen;
} WavHeaderStruct; 


#define MAX_TTS_STRING	256
#define MAX_TRIES	3
#define RETRY_SLEEP	1
// #define FIFO_DIR "FIFO_DIR"

static	char	logBuf[256];
static	char	fifo_dir[128];
static	int	get_fifo_dir(char *);
static  char	fileName[30];
static 	int	counter = 0;

static char	ModuleName[] = "TEL_SpeakTTS";

extern int tel_Speak(char *mod, int interruptOption, void *m, int updateLastPhraseSpoken);

/* Static Function Prototypes */

static int getResult(int fd, char *resultFifo,
		int timeout, int cleanupOption, ARC_TTS_RESULT *ttsResult);
static int 	makeResultFifo(char *resultFifo, int *fd);
// static int 	sendRequest(const ARC_TTS_REQUEST_SINGLE_DM *ttsRequest);
static int speakResult(ARC_TTS_RESULT *ttsResult, int syncOption, 
			int interruptOption, int cleanupOption, struct Msg_Speak *zpMsg);
static int getFileAndPath(char *path, char *absolutePath);

extern char* get_time();

extern int	errno;

/*------------------------------------------------------------------------------
sendRequestToTTSClient(): Write the request to the FIFO.
------------------------------------------------------------------------------*/
int sendRequestToTTSClient (const ARC_TTS_REQUEST_SINGLE_DM * ttsRequest)
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
                 "Failed to open request fifo (%s) for writing. errno=%d. [%s], "
                 "ttsClient may not be running or may not be "
                 "connected to tts host.", fifo_dir, errno, strerror (errno));
    }
    else {
      telVarLog (mod, REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
                 "Failed to open request fifo (%s) for writing. errno=%d, [%s].",
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
                 "Sent %d bytes to (%s). "
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

int modifyFileNameForPutQueueAsync(char *zpSrc, char *zpDest, 
		int zCleanupOption)
{
	int	rc;

	*zpDest = '\0';

	if(zCleanupOption == SPEAK_AND_DELETE)
	{
		sprintf(zpDest, "speaktts@%d_%d.wav", getpid(), counter);
		counter++;
	}
	else
	{
		if(strstr(zpSrc, ".wav") != NULL)
		{
			sprintf(zpDest, "%s", zpSrc);
		}
		else
		{
			sprintf(zpDest, "%s.wav", zpSrc);
		}
	}

	return(0);
}

int TEL_SpeakTTS(int interruptOption,int cleanupOption, int syncOption,
				int timeout, char *speechFile, char *userString)
{
	char 	apiAndParms[MAX_LENGTH] = "";
	char	resultFifo[80];
    char	absolutePathAndFileName[1024];
	int		rc, resultFd;
	ARC_TTS_REQUEST_SINGLE_DM	ttsRequest;
	ARC_TTS_RESULT	ttsResult;
	char	invalid_parm_str[128];
	FILE	*fp;
	char	*tmpFileName;
	char	tmpUserString[512];
	struct Msg_Speak msg;
	int 	party = FIRST_PARTY;
	char 	*typeAheadBuffer;
	static char mod[]="TEL_SpeakTTS";

	memset(tmpUserString, 0, sizeof(tmpUserString));
	if(strlen(userString) > 432)
		strncpy(tmpUserString, userString, 432);
	else
		sprintf(tmpUserString, "%s", userString);

	sprintf(apiAndParms,"%s(%s,%s,%s,%d,speechFile,%s)", ModuleName,
				arc_val_tostr(interruptOption),
				arc_val_tostr(cleanupOption),
				arc_val_tostr(syncOption),
				timeout,tmpUserString);
	rc = apiInit(ModuleName, TEL_SPEAKTTS, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);

	memset(fileName, 0, sizeof(fileName));

	if((int)strlen(userString) > MAX_TTS_STRING)
	{
		/* Create a file and send it */
		tmpFileName = get_time();
		sprintf(fileName, "%s.%d.%d", tmpFileName, getpid(), GV_AppCallNum1);
		if((fp = fopen(fileName, "w+")) == NULL)
		{
			sprintf(logBuf,"Failed to open file (%s), [%d, %s]", 
								fileName, errno, strerror(errno));
			telVarLog(ModuleName,REPORT_NORMAL,TEL_CANT_OPEN_FILE, 
					GV_Err, logBuf); 
			HANDLE_RETURN(-1);
		}
		fprintf(fp, "%s", userString);
		fclose(fp);
	}

	msg.opcode = DMOP_SPEAK;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;
	msg.list = 0;
	msg.allParties = (party == BOTH_PARTIES);
	msg.synchronous = syncOption;
	memset(msg.file, 0, sizeof(msg.file));

	msg.interruptOption = interruptOption;
	if(interruptOption == NONINTERRUPT)
	{
		if((party == FIRST_PARTY) || (party == BOTH_PARTIES))
       		msg.appCallNum = GV_AppCallNum1;
		else
       		msg.appCallNum = GV_AppCallNum2;
	}
	else
	{
		if(interruptOption == SECOND_PARTY_INTERRUPT)
       		msg.appCallNum = GV_AppCallNum2;
		else
       		msg.appCallNum = GV_AppCallNum1;
	}

	if( ( syncOption == PLAY_QUEUE_SYNC || syncOption == PLAY_QUEUE_ASYNC) &&
		( ! GV_MrcpTTS_Enabled ) )
	{
		int retCode;

#if 0
{
		struct playMedia *lpMediaMessage = (struct playMedia *)malloc(sizeof(struct playMedia));
		createPlayMediaMsg(lpMediaMessage, msg);
		writePlayMediaParamsToFile(lpMediaMessage, &msg);	
		msg.opcode = DMOP_PLAYMEDIA;
		retCode = tel_PlayMedia(ModuleName, lpMediaMessage, &msg, 1);
		free(lpMediaMessage);
}
#endif

		retCode = tel_Speak(ModuleName, interruptOption, &msg, 1);
		if(retCode != TEL_SUCCESS && retCode != 1)
		{
			if(strlen(fileName) > 0)
				unlink(fileName);
			HANDLE_RETURN (retCode);
		}
   		HANDLE_RETURN (0);
	}

	memset(&ttsRequest, 0, sizeof(ARC_TTS_REQUEST_SINGLE_DM));

	if(userString[0] == '\0')
	{
		telVarLog(ModuleName, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"No data passed to be spoken.");
		if(strlen(fileName) > 0)
			unlink(fileName);
		HANDLE_RETURN(-1);
	}

	if(syncOption != SYNC && syncOption != ASYNC && syncOption != ASYNC_QUEUE &&
		syncOption != PUT_QUEUE && syncOption != PUT_QUEUE_ASYNC &&
		syncOption != PLAY_QUEUE_ASYNC && syncOption != PLAY_QUEUE_SYNC)
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        	"Invalid value for sync: %d. Valid values are SYNC, ASYNC, "
			"ASYNC_QUEUE, PUT_QUEUE,PUT_QUEUE_ASYNC, "
			"PLAY_QUEUE_SYNC, and PLAY_QUEUE_ASYNC.", syncOption);
		if(strlen(fileName) > 0)
			unlink(fileName);
		HANDLE_RETURN(-1);
	}

	if(interruptOption != INTERRUPT && interruptOption != NONINTERRUPT)
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        	"Invalid value for interrupt option: %d. Valid values are "
			"INTERRUPT, NONINTERRUPT.", interruptOption);
		if(strlen(fileName) > 0)
			unlink(fileName);
		HANDLE_RETURN(-1);
	}

	if(	cleanupOption != SPEAK_AND_KEEP		&& 
		cleanupOption != SPEAK_AND_DELETE 	&&
		cleanupOption != DONT_SPEAK_AND_KEEP
	  )
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        	"Invalid value for cleanupOption: %d. Valid values are "
			"SPEAK_AND_KEEP, SPEAK_AND_DELETE, DONT_SPEAK_AND_KEEP.", 
			cleanupOption);
		if(strlen(fileName) > 0)
			unlink(fileName);
		HANDLE_RETURN(-1);
	}

	if(timeout < 0)
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        	"Invalid value for timeout: %d. Valid values are 0 or greater.",
			timeout);
		if(strlen(fileName) > 0)
			unlink(fileName);
		HANDLE_RETURN(-1);
	}

	if((speechFile == (char *)0) || (speechFile == (char *)1))
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Filename (%s) is invalid or NULL.", speechFile);
		if(strlen(fileName) > 0)
			unlink(fileName);
		HANDLE_RETURN(-1);
	}

	if (party == FIRST_PARTY)
	{
		typeAheadBuffer = GV_TypeAheadBuffer1;
	}
	else
	{
		typeAheadBuffer = GV_TypeAheadBuffer2;
	}

	if((syncOption != PUT_QUEUE) && (syncOption != PUT_QUEUE_ASYNC))
	{
		switch(interruptOption)
		{
			case FIRST_PARTY_INTERRUPT:
			case SECOND_PARTY_INTERRUPT:
				if(*typeAheadBuffer != '\0')
					HANDLE_RETURN(TEL_SUCCESS);
				break;

			case NONINTERRUPT:
				switch(party)
				{
					case FIRST_PARTY:
						memset(GV_TypeAheadBuffer1, 0,
							sizeof(GV_TypeAheadBuffer1));
						break;
					
					case SECOND_PARTY:
						memset(GV_TypeAheadBuffer2, 0,
							sizeof(GV_TypeAheadBuffer2));
						break;
				
					case BOTH_PARTIES:
						memset(GV_TypeAheadBuffer1, 0,
							sizeof(GV_TypeAheadBuffer1));
						memset(GV_TypeAheadBuffer2, 0,
							sizeof(GV_TypeAheadBuffer2));
						break;
				}
				break;

			default:
				break;
		}
	}

	// DING DING DING 

	sprintf(ttsRequest.port, 	"%d", GV_AppCallNum1);
	sprintf(ttsRequest.pid, 	"%d", getpid());
	sprintf(ttsRequest.app, 	"%s", "avTest");
	ttsRequest.async = syncOption;
	sprintf(ttsRequest.asyncCompletionFifo, "%s", GV_RequestFifo);

	if(strlen(fileName) > 0)
	{
		sprintf(ttsRequest.string, 	"%s", fileName);
		sprintf(ttsRequest.type, "%s", "FILE");
	}
	else if(strncmp(userString, "^FILE^", 6) == 0)
	{
		sprintf(ttsRequest.string, "%s", userString + 6);
		sprintf(ttsRequest.type, "%s", "FILE");
	}
	else
	{
		sprintf(ttsRequest.string, 	"%s", userString);

		sprintf(ttsRequest.type, "%s", GV_TTS_DataType);
	}

	sprintf(ttsRequest.language, "%s", GV_TTS_Language);
	sprintf(ttsRequest.gender, "%s", GV_TTS_Gender);
	sprintf(ttsRequest.compression, "%s", GV_TTS_Compression);

	if(GV_TTS_VoiceName != NULL &&
		GV_TTS_VoiceName[0] != '\0')
	{
		sprintf(ttsRequest.voiceName, "%s", GV_TTS_VoiceName);
	}
	else
	{
		memset(ttsRequest.voiceName, 0, sizeof(ttsRequest.voiceName));
	}

	sprintf(ttsRequest.timeOut, "%d", timeout);

	memcpy(&ttsRequest.speakMsgToDM, &msg, sizeof(struct Msg_Speak)); 

	getFileAndPath(speechFile, absolutePathAndFileName);
	makeResultFifo(resultFifo, &resultFd);
	sprintf(ttsRequest.resultFifo, 	"%s", resultFifo);

	sprintf(ttsRequest.fileName,    "%s", absolutePathAndFileName);
	sprintf(ttsRequest.speakMsgToDM.file, "%s", ttsRequest.fileName);
//	sprintf(msg.file, "%s", ttsRequest.fileName);

	if (syncOption == PUT_QUEUE_ASYNC)
	{
		int retCode;

#if 0
{
		struct playMedia *lpMediaMessage = (struct playMedia *)malloc(sizeof(struct playMedia));
		createPlayMediaMsg(lpMediaMessage, msg);
		writePlayMediaParamsToFile(lpMediaMessage, &msg);	
		msg.opcode = DMOP_PLAYMEDIA;
		retCode = tel_PlayMedia(ModuleName, lpMediaMessage, &msg, 1);
		free(lpMediaMessage);
}
#endif
		if ( ! GV_MrcpTTS_Enabled )
		{
			retCode = tel_Speak(ModuleName, interruptOption, &msg, 1);
			if(retCode != TEL_SUCCESS && retCode != 1)
			{
				if(strlen(fileName) > 0)
					unlink(fileName);
				HANDLE_RETURN (retCode);
			}
		}
	}

#if 0
	if(syncOption != SYNC)
	{
		ttsRequest.async = 1;
	}
#endif

	if ( (syncOption == PUT_QUEUE_ASYNC) && 
	     ( ! GV_MrcpTTS_Enabled ) )
	{
		char tmpBuf[128];
	
		ttsRequest.async = 1;
		memset(tmpBuf, 0, sizeof(tmpBuf));
		modifyFileNameForPutQueueAsync(ttsRequest.fileName, tmpBuf,
			cleanupOption);
		sprintf(ttsRequest.fileName, "%s", tmpBuf);
	}

	if ( ! GV_MrcpTTS_Enabled )
	{
		memcpy(&ttsRequest.speakMsgToDM, &msg, sizeof(struct Msg_Speak));
		sprintf(ttsRequest.speakMsgToDM.file, "%s", ttsRequest.fileName);
	}
	if(sendRequestToTTSClient(&ttsRequest) < 0)
	{
		close(resultFd);
		unlink(resultFifo);
		if(strlen(fileName) > 0)
			unlink(fileName);
		HANDLE_RETURN(-1);		/* Message written in routine */
	}

	rc = getResult(resultFd, resultFifo, timeout, cleanupOption, &ttsResult);

	if ( rc == DMOP_DISCONNECT )
	{
		HANDLE_RETURN(disc(mod, GV_AppCallNum1));
	}

	if( (rc < 0) || ( atoi(ttsResult.resultCode) < 0 ) )
	{
		close(resultFd);
		unlink(resultFifo);
		if(strlen(fileName) > 0)
		{
			unlink(fileName);
		}
		if ( strlen(ttsResult.speechFile) > 0 )
		{
			if(access(ttsResult.speechFile, F_OK) == 0)
			{
				unlink(ttsResult.speechFile);
			}
		}
		if ( strlen(absolutePathAndFileName) > 0 )
		{
			if(access(absolutePathAndFileName, F_OK) == 0)
			{
				unlink(absolutePathAndFileName);
			}
		}
		HANDLE_RETURN(rc);		/* Message written in routine */
	}

	/* unlink the file if it was created because userstring is > 500 */
	if(strlen(fileName) > 0)
	{
		unlink(fileName);
	}
	else if(strncmp(userString, "^FILE^", 6) == 0)
	{
		unlink(ttsRequest.string);
	}

	/* Cleanup the result fifo that we created */
	close(resultFd);
	unlink(resultFifo);

	if(syncOption == PUT_QUEUE_ASYNC)
	{
		HANDLE_RETURN(TEL_SUCCESS);
	}

	if(cleanupOption == DONT_SPEAK_AND_KEEP)
	{
		HANDLE_RETURN(TEL_SUCCESS);
	}

#if 0 // MR 4545
	if ( ! GV_MrcpTTS_Enabled )
	{

		if((rc = speakResult(&ttsResult, syncOption, 
						interruptOption, cleanupOption, &msg)) < 0)
		{
			if(rc == -3)
			{
				if(cleanupOption == SPEAK_AND_DELETE)
					unlink(ttsResult.speechFile);
			}
			HANDLE_RETURN(rc);		/* Message written in routine */
		}
	}
#endif
	if((cleanupOption == SPEAK_AND_DELETE) && (syncOption == SYNC))
	{
		if ( strlen(ttsResult.speechFile) > 0 )
		{
			if(access(ttsResult.speechFile, F_OK) == 0)
			{
				unlink(ttsResult.speechFile);
			}
		}
		if ( strlen(absolutePathAndFileName) > 0 )
		{
			if(access(absolutePathAndFileName, F_OK) == 0)
			{
				unlink(absolutePathAndFileName);
			}
		}
	}

	sprintf(apiAndParms, "speechFile=%s", speechFile);

	rc = apiInit(ModuleName, TEL_SPEAKTTS, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);

	HANDLE_RETURN(TEL_SUCCESS);

} /* END: TEL_SpeakTTS() */


int createPlayMediaMsg(struct playMedia *lpMediaMessage, struct Msg_Speak msg)
{
	static char mod[] = "createPlayMediaMsg";
	//lpMediaMessage->opCode = DMOP_PLAYMEDIA;
	//lpMediaMessage->appCallNum = msg.appCallNum;
	//lpMediaMessage->appPid = msg.appPid;
	//lpMediaMessage->appRef = msg.appRef;
	//lpMediaMessage->appPassword = msg.appPassword;
	lpMediaMessage->party = msg.allParties;
	lpMediaMessage->interruptOption = msg.interruptOption;
		



	sprintf(lpMediaMessage->audioFileName, "%s", msg.file);
	sprintf(lpMediaMessage->videoFileName, "%s", "NULL");
	lpMediaMessage->sync = msg.synchronous;
	//lpMediaMessage->audioinformat = PHRASE_FILE;
	//lpMediaMessage->audiooutformat = PHRASE;
	//lpMediaMessage->videoinformat = VIDEO_FILE;
	//lpMediaMessage->videooutformat = VIDEO;

	lpMediaMessage->audioLooping = NO;
	lpMediaMessage->videoLooping = NO;
	lpMediaMessage->addOnCurrentPlay = NO;
	lpMediaMessage->syncAudioVideo = NO;
	
	//sprintf(lpMediaMessage->key,"%s" ,msg.key);
		
	telVarLog(mod,REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"playMedia Struct vals : party = %d ; interruptOption = %d ;audioFileName = %s ;videoFileName = %s ;sync = %d ;audioLooping = %d ;videoLooping = %d ;addOnCurrentPlay = %d ;syncAudioVideo = %d ",lpMediaMessage->party,lpMediaMessage->interruptOption, lpMediaMessage->audioFileName, lpMediaMessage->videoFileName, lpMediaMessage->sync, lpMediaMessage->audioLooping, lpMediaMessage->videoLooping, lpMediaMessage->addOnCurrentPlay, lpMediaMessage->syncAudioVideo);
		
}


/*------------------------------------------------------------------------------
makeResultFifo(): Make the FIFO on which the result is going to be written.
------------------------------------------------------------------------------*/
static int makeResultFifo(char *resultFifo, int *fd)
{
	sprintf(resultFifo, "/tmp/ttsResult.%d.fifo", GV_AppCallNum1);
	if(mkfifo(resultFifo, S_IFIFO|PERMS) < 0 && errno != EEXIST)
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_CREATE_FIFO, 
			GV_Err, TEL_CANT_CREATE_FIFO_MSG, resultFifo, 
			errno, strerror(errno));
		return(-1);
	}

	if((*fd = open(resultFifo, O_CREAT|O_RDWR)) < 0)
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_OPEN_FIFO, 
			GV_Err, TEL_CANT_OPEN_FIFO_MSG, resultFifo, 
			errno, strerror(errno));
		return(-1);
	}

	return(0);
} /* END: makeResultFifo() */
#if 0
/*------------------------------------------------------------------------------
sendRequest(): Write the request to the FIFO.
------------------------------------------------------------------------------*/
static int sendRequest(const ARC_TTS_REQUEST_SINGLE_DM *ttsRequest)
{
	int	i, rc, fd;
	static int got_fifodir = 0;

	if(got_fifodir == 0)
	{
		memset(fifo_dir, 0, sizeof(fifo_dir));
		get_fifo_dir(fifo_dir);
		strcat(fifo_dir, "/tts.fifo");
		got_fifodir = 1;
	}

	if((fd = open(fifo_dir, O_WRONLY|O_NONBLOCK)) == -1)
	{
		if((errno == 6) || (errno == 2))
		{
			telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err, 
				"Failed to open request fifo (%s) for writing. errno=%d. [%s], "
				"ttsClient may not be running or may not be "
				"connected to tts host.", fifo_dir, errno, strerror(errno));
		}		
		else
		{
			telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err, 
				"Failed to open request fifo (%s) for writing. errno=%d, [%s].",
				fifo_dir, errno, strerror(errno));
		}
		return(-1);
	}
	for(i=0; i<MAX_TRIES; i++)
	{
		rc = write(fd, ttsRequest, sizeof(ARC_TTS_REQUEST_SINGLE_DM));
		if(rc == -1 && errno == EAGAIN)
		{
			sleep(RETRY_SLEEP);	
		}
		else
		{
			break;
		}
	}
	if(rc == -1)
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_WRITE_FIFO, 
			GV_Err, TEL_CANT_WRITE_FIFO_MSG,fifo_dir, errno, strerror(errno));
		return(-1);
	}
	close(fd);
	return(0);
} /* END: sendRequest() */
#endif
#include <sys/poll.h>
/*------------------------------------------------------------------------------
getResult(): Get the result from the result fifo
------------------------------------------------------------------------------*/
static int getResult(int fd, char *resultFifo,
		int timeout, int cleanupOption, ARC_TTS_RESULT *ttsResult)
{
	static char		mod[]="getResult";
	int	rc;
	struct pollfd	pollSet[2];
	time_t t1, t2;
	struct MsgToApp response;
	struct MsgToDM  request;
	int			i;

	memset(ttsResult, 0, sizeof(ARC_TTS_RESULT));
	memset((struct MsgToApp *)&response, 0, sizeof(response));
	memset((struct MsgToDM *)&request, 0, sizeof(request));

	if ( GV_ResponseFifoFd <= 0 )
	{
		if ((mkfifo (GV_ResponseFifo, S_IFIFO | 0666) < 0)
			        && (errno != EEXIST))
		{
			telVarLog (mod, REPORT_NORMAL, TEL_CANT_CREATE_FIFO, GV_Err,
					TEL_CANT_CREATE_FIFO_MSG, GV_ResponseFifo,
					errno, strerror (errno));
			return (-1);
   		}

		if ((GV_ResponseFifoFd = open (GV_ResponseFifo, O_RDWR)) < 0)
		{
			telVarLog (mod, REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
				TEL_CANT_OPEN_FIFO_MSG, GV_ResponseFifo, errno, strerror (errno));
			return (-1);
		}

	}

	pollSet[0].fd = fd;
	pollSet[1].fd = GV_ResponseFifoFd;
	pollSet[0].events = POLLIN;
	pollSet[1].events = POLLIN;

	if(timeout == 0)
	{
		while(1)
		{
			rc = poll(pollSet, 2, -1);
			if((rc < 0) && (errno == 4))
				continue;
			break;
		}
	}
	else
	{
		time(&t1);
		while(1)
		{
			rc = poll(pollSet, 2, 100);
			if((rc < 0) && (errno == 4))
			{
				time(&t2);
				if((t2-t1) < timeout)
				{
					continue;
				}
			}
			if(rc == 0)
			{
                if ( (!GV_Connected1) &&
                     ( cleanupOption != DONT_SPEAK_AND_KEEP) )
				{
					return(TEL_DISCONNECTED);
				}
				else
				{
					time(&t2);
					if((t2-t1) < timeout)
						continue;
					else
						break;
				}
			}
			break;
		}
	}
	if(rc < 0)
	{
		sprintf(logBuf, "poll() failed, [%d, %s]", errno, strerror(errno));
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_POLL_FAILED, GV_Err, logBuf);
		return(-1);
	}

    for(i=0; i<2; i++)
    {
        if(pollSet[i].revents == 0)
        {
            continue;
        }
		if ( i == 0 )
		{
			if ( GV_MrcpTTS_Enabled ) 
			{
				rc = read(fd, (char *)&response, sizeof(struct MsgToApp));
				if(rc == -1)
				{
					telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_READ_FIFO, GV_Err,
						"Failed to read TTS result from FIFO (%s), errno=%d "
						"(%s).", resultFifo, errno, strerror(errno));
					return(-1);
				}
			}
			else
			{
				rc = read(fd, ttsResult, sizeof(ARC_TTS_RESULT));
				if(rc == -1)
				{
					telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_READ_FIFO, GV_Err,
						"Failed to read TTS result from FIFO (%s),  [%d, %s] ",
						ttsResult->resultFifo, errno, strerror(errno));
					return(-1);
				}
				telVarLog(ModuleName, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
					"Read %d bytes from fifo (%s):"
					"fileName=(%s) pid=(%s) "
			        "resultCode=(%s) speechFile=(%s) error=(%s).",
			        rc, ttsResult->resultFifo,
			        ttsResult->fileName, ttsResult->pid,
			        ttsResult->resultCode, ttsResult->speechFile,
			        ttsResult->error);
	
				return(0);
			}
		}
		else if ( i == 1 )
		{
			telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Reading from non-tts fifo....");
			rc = read (GV_ResponseFifoFd, (char *) &response, sizeof(response));
			if (timeout > 0)
			{
				/* Turn off the alarm & reset the handler. */
				alarm (0);
				signal (SIGALRM, NULL);
			}

			if (rc == -1)
			{
				telVarLog (mod, REPORT_NORMAL, TEL_CANT_READ_FIFO, GV_Err,
						TEL_CANT_READ_FIFO_MSG, GV_ResponseFifo,
						errno, strerror (errno));
				return (-1);
			}
			telVarLog (mod, REPORT_DETAIL, TEL_BASE, GV_Info,
					"Received %d bytes. "
					"msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,rc:%d,data:%s}",
					rc,
					response.opcode,
					response.appCallNum,
					response.appPid,
					response.appRef,
					response.appPassword, response.returnCode, response.message);
			if ( response.opcode == DMOP_DISCONNECT )
			{
				request.appPid = response.appPid;
				rc = examineDynMgrResponse(mod, &request, &response);
				return(DMOP_DISCONNECT);
			}

			if ( response.opcode == DMOP_GETDTMF)
			{
//				if (response.appCallNum != msg.appCallNum)
//				{
//					/* Save data for the other leg of the call */
					saveTypeAheadData(mod, response.appCallNum, response.message);
//				}
			}
			return (0);
		}
	}

//	telVarLog(ModuleName, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
//		"DJB: sizeof(ARC_TTS_RESULT)=%ld  sizeof(struct MsgToApp)=%d",
//			sizeof(ARC_TTS_RESULT), sizeof(struct MsgToApp));
//
//	telVarLog(ModuleName, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
//		"Received TTS Result port:%d,op:%d,pid=%d,appRef=%d,returnCode=%d,"
//		"message=(%s).",
//		response.appCallNum, response.opcode, response.appPid,
//		response.returnCode, response.appRef,response.message);

	telVarLog(ModuleName, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
		"Received TTS Result port:%d,op:%d,pid=%d,appRef=%d,returnCode=%d,"
		"message=(%s).",
		response.appCallNum, response.opcode, response.appPid,
		response.appRef,response.returnCode, response.message);

	if(response.returnCode < 0)
	{
		return (response.returnCode);
	}

	if ( response.opcode == DMOP_GETDTMF )
	{
		char		dtmf[64];
		char		*p;
		char		fileName[256];
		char		message[256];
	
		sprintf(message, "%s", response.message);
		p = response.message;
	
		memset((char *)fileName, '\0', sizeof(fileName));
		memset((char *)dtmf, '\0', sizeof(dtmf));
		while ( *p != '\0' )
		{
			if ( *p == '^' )
			{
				*p = '\0';
				sprintf(dtmf, "%s", message);
				sprintf(ttsResult->speechFile, "%s", ++p);
				break;
			}
			p++;
		}
		if ( fileName[0] == '\0' )
		{
   			telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,
				"Invalid message (%s) received for DMOP_GETDTMF(%d). Expecting "
				"format of \"dtmf^filename\".",
				response.message, DMOP_GETDTMF);
			return(-1);
		}
		else
		{
			telVarLog(ModuleName, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"Message parsed with dtmf=(%s) and fileName=(%s)",
				dtmf, fileName);
			saveTypeAheadData(mod, response.appCallNum, dtmf);
		}
	}
	else
	{
		sprintf(ttsResult->speechFile, "%d", response.message);
	}

	sprintf(ttsResult->port, "%d", response.appCallNum);
	sprintf(ttsResult->pid, "%d", response.appPid);
	sprintf(ttsResult->resultCode, "%d", response.returnCode);

	return(0);
} /* END: getResult() */

int addWavHeaderToFile(char *zpFileName)
{
	static char mod[] = {"addWavHeaderToFile"};
	char tmpFileName[20] = {"tmpNoWavFile"};
	int rc;
	WavHeaderStruct lWavHeaderStruct;
	int fdReadFrom, fdWriteTo;
	char buf[512];

	unlink(tmpFileName);

	if((fdReadFrom = open(zpFileName, O_RDWR)) < 0)
	{
		telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,  
			"Can't open file (%s) for reading.  [%d, %s]", 
			zpFileName, errno, strerror(errno)); 
		return(-1);
	}	

	if((fdWriteTo = creat(tmpFileName, 0666)) < 0)
	{
		telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,  
			"Failed to create file (%s) for writing.  [%d, %s]", 
			tmpFileName, errno, strerror(errno)); 
		close(fdReadFrom);
		return(-1);
	}	

	sprintf(lWavHeaderStruct.rId, "RIFF");
	lWavHeaderStruct.rLen = 36L;
	sprintf(lWavHeaderStruct.wId, "%s", "WAVE");
	sprintf(lWavHeaderStruct.fId, "%s", "fmt ");
	lWavHeaderStruct.fLen = 16L;
	lWavHeaderStruct.nFormatTag = 7;
    lWavHeaderStruct.nChannels = 1;
    lWavHeaderStruct.nSamplesPerSec = 8000L;
    lWavHeaderStruct.nAvgBytesPerSec = 8000L;
    lWavHeaderStruct.nBlockAlign = 1;
    lWavHeaderStruct.nBitsPerSample = 8;
	sprintf(lWavHeaderStruct.dId, "%s", "data");
    lWavHeaderStruct.dLen = 100L;

	if(write(fdWriteTo, &lWavHeaderStruct, sizeof(WavHeaderStruct)) == -1)
	{
		telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,  
			"Failed to write a wav header to file (%s).  [%d, %s]", 
			tmpFileName, errno, strerror(errno));
		close(fdReadFrom);
		close(fdWriteTo);
        return(-1);
	}

	buf[sizeof(buf)-1] = '\0';
	while((rc = read(fdReadFrom, buf, sizeof(buf)-1)) > 0)
	{
		if(write(fdWriteTo, buf, rc) == -1)
		{
			telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,  
				"Failed to write to a file (%s).  [%d, %s]", 
				tmpFileName, errno, strerror(errno));
			close(fdReadFrom);
			close(fdWriteTo);
        	return(-1);
		}
	}

	if(rc < 0)
	{
		telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,  
			"Failed to read from a tts file (%s). [%d, %s]", 
			zpFileName, errno, strerror(errno));
		close(fdReadFrom);
		close(fdWriteTo);
		return(-1);
	}

	close(fdReadFrom);
	close(fdWriteTo);

	rename(tmpFileName, zpFileName);

	return(0);
}

/*------------------------------------------------------------------------------
speakResult(): Speak the TTS file
------------------------------------------------------------------------------*/
static int speakResult(ARC_TTS_RESULT *ttsResult, int syncOption, 
			int interruptOption, int cleanupOption, struct Msg_Speak *zpMsg)
{
	int	rc;
	char	tmpSpeechFile[256];
	int	option;

	if(access(ttsResult->speechFile, F_OK|R_OK) == -1)
	{
		telVarLog(ModuleName, REPORT_NORMAL, TEL_CANT_ACCESS_FILE, 
				GV_Err, "Can't access file (%s), [%d, %s]",
				ttsResult->speechFile, errno, strerror(errno));
		return(-1);
	}
	
	memset(tmpSpeechFile, 0, sizeof(tmpSpeechFile));
	/*
	 * We need the .wav extension. So rename the file with that ext.
	 */
	if((cleanupOption == SPEAK_AND_DELETE) &&
	   (syncOption != SYNC))
	{
		sprintf(tmpSpeechFile, "speaktts@%d_%d.wav", getpid(), counter);
		counter++;
	}
	else
	{
		char *p;
		p = strrchr(ttsResult->speechFile, '.');
		if((p == NULL) || (strcmp(p, ".wav") != 0))
		{
			sprintf(tmpSpeechFile, "%s%s", ttsResult->speechFile, ".wav");
		}
		else
		{
			sprintf(tmpSpeechFile, "%s", ttsResult->speechFile);
		}
	}

	if(rename(ttsResult->speechFile, tmpSpeechFile) < 0)
	{
   		telVarLog(ModuleName,REPORT_NORMAL,TEL_BASE, GV_Err,
				"Failed to rename (%s) to (%s).  [%d, %s]",
				ttsResult->speechFile,tmpSpeechFile,
				errno,strerror(errno));

		return(-1);
	}

	sprintf(ttsResult->speechFile, "%s", tmpSpeechFile);

	addWavHeaderToFile(ttsResult->speechFile);

	if(interruptOption == NONINTRPT)
		option = NONINTRPT;
	else
		option = FIRST_PARTY_INTERRUPT;

	/*  Speak the file that contains the ext.  */
	sprintf(zpMsg->file, "%s", tmpSpeechFile);
	zpMsg->synchronous = syncOption;

#if 0
	zpMsg->synchronous = 1;
	switch(syncOption)
	{
		case SYNC:
			zpMsg->synchronous = 1;
			break;

		case ASYNC:
			zpMsg->synchronous = 0;
			break;

		case ASYNC_QUEUE:
			zpMsg->synchronous = 2;
			break;

		default:
			break;
	}
#endif
#if 0
{
		struct Msg_Speak msg;
		struct playMedia *lpMediaMessage = (struct playMedia *)malloc(sizeof(struct playMedia));
		memcpy(&msg, zpMsg, sizeof(struct Msg_Speak));
		createPlayMediaMsg(lpMediaMessage, msg);
		zpMsg->opcode = DMOP_PLAYMEDIA;
		writePlayMediaParamsToFile(lpMediaMessage, zpMsg);	
		rc = tel_PlayMedia(ModuleName, lpMediaMessage, zpMsg, 1);
		free(lpMediaMessage);
}
#endif
	rc = tel_Speak(ModuleName, option, zpMsg, 1);
	if(rc != TEL_SUCCESS && rc != 1)
	{
		return(rc);
	}

	if((cleanupOption != SPEAK_AND_DELETE) ||
	   (syncOption == SYNC))
	{
		/*  Remove the ext. that we added */
		if(rename(tmpSpeechFile, ttsResult->speechFile) < 0)
		{
   			telVarLog(ModuleName,REPORT_NORMAL,TEL_BASE, GV_Err,
					"Failed to rename (%s) to (%s). [%d, %s]",
					tmpSpeechFile, ttsResult->speechFile,
					errno,strerror(errno));
			return(-1);
		}
	}

	return(rc);
} /* END: speakResult() */
/*------------------------------------------------------------------------------
getFileAndPath(): returns Absolute path of file name in following cases
		  filename is null
		  filename containts relative path
		  filename containts absolute path
------------------------------------------------------------------------------*/
static int getFileAndPath(char *path, char *absolutePath)
{
	int 	retcode;

	char	currPath[512];
	char	*tmpPtr;
	char 	presentDir[512];
	char 	*fileName;
	char	gPath[100];
	char	*ptr;
	char	mydir[100];
	char	mPath[100];
	char	relativePath[100];
	static int count = 0;

	absolutePath[0] = '\0';

	sprintf(gPath, "%s", path);
	getcwd(presentDir, 64);

	if( strlen(gPath) == 0)
	{
		sprintf(gPath,"%s.%d.%d", "sound", (int)getpid(), count);
		sprintf(absolutePath,"%s/%s", presentDir, gPath);
		count++;
		return(0);
	}
	
	if((tmpPtr = (char *)strstr(gPath,"/")) == NULL )
	{
		sprintf(absolutePath, "%s/%s", presentDir, gPath);
		return(0);
	}
	else
	{
		fileName = strrchr(gPath, '/' );
		fileName++;

		if((ptr = (char*)strrchr(gPath, (int)'/')) != NULL)
		{
			strncpy(mPath, gPath, ptr - gPath);
			mPath[ptr - gPath] = '\0';
		}
		retcode = chdir(mPath);
		if(retcode < 0)
		{
			sprintf(logBuf, "%s (%s). %s (%s/%s).",
				"Failed to find directory ", mPath, 
				"Setting to current directory: ", presentDir, fileName);
			telVarLog(ModuleName, REPORT_NORMAL, TEL_CANT_FIND_FILE,
							GV_Err, logBuf);
		}
		getcwd(mydir, 64);
		if(strcmp(mydir,"/") == 0)
		{
			sprintf(absolutePath, "%s%s", mydir, fileName);
		}
		else
		{
			sprintf(absolutePath, "%s/%s", mydir, fileName);
		}
		chdir(presentDir);
		return(0);
	}
} /* getFileAndPath */

/*-----------------------------------------------------------------------------
This routine will read the fifo directory from .Global.cfg file. if no 
directory is found it will assume /tmp
-----------------------------------------------------------------------------*/
static int get_fifo_dir (char *fifo_dir)
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
               "fifo_dir (%s) doesn't exist, assuming /tmp", fifo_dir);
    sprintf (fifo_dir, "%s", "/tmp");
  }

// #ifdef DEBUG
//  fprintf (stderr, "fifo directory set to >%s<\n", fifo_dir);
// #endif
  return (0);
}

