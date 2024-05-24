/*-----------------------------------------------------------------------------
Program	:	TEL_PlayMedia.c
Purpose	:	To play audio and/or video files.
Author	:	Anand Joglekar & Rahul Gupta
Date	:	12/8/2004
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

extern int getValue(
		char	*zStrInput,   
		char	*zStrOutput,
		char	*zStrKey,
		char	zChStartDelim,
		char	zChStopDelim);
int tel_PlayMedia(char *mod, PlayMedia *zPlayMedia, void *m,
                int updateLastPhraseSpoken);
static int parametersAreGood(char *mod, PlayMedia *zPlayMedia);
static int notifyPlayMediaStreamingClient(PlayMedia *zMsgSpeak, char *zUrl,
									char *zAudioUrl, char *zVideoUrl, 
									int *isVideoStream);
static int getKeys(char *zKeyMessage);
static int writePlayMediaParamsToFile(PlayMedia *zPlayMedia, struct MsgToDM  *msg);
static int createAudioUrl(char *zUrl, char *zAudioUrl,
                    char *zSubscriber, char *zType,
					char *zSessionId,
                    int zMsgId, int zNodeId,
                    long zTime, char *zCodec);
static int getPlayMediaValue(
        char    *zStrInput,
        char    *zStrOutput,
        char    *zStrKey,
        char    zChStartDelim,
        char    zChStopDelim);
static int getPlayMediaStreamingParams(char *zQuery, char *zSubscriber,
                        char * zType,
						char *zSessionId,
						int *zMsgId,
                        int *zNodeId,
                        long *zTime,
						char *zCodec,
                        int *zVideoMsgId, 
						int *zVideoNodeId, 
						long *zVideoTime, 
						char *zVideoCodec);

char key[30];
char vkey[30];

int TEL_PlayMedia(PlayMedia *zPlayMedia)
{
	static char 	mod[]="TEL_PlayMedia";
	char 			apiAndParmsFormat[]="%s(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)";	
	char 			apiAndParms[MAX_LENGTH] = "";
	int 			rc;
	int 			requestSent;
	struct MsgToApp response;
	struct MsgToDM 	msg; 
	char			*typeAheadBuffer;
	int				lStreaming = 0;
	int				isVideoStream = 0;
	char			lUrl[256];
	char			audioStreamUrl[256];
	char			videoStreamUrl[256];
	char			string_to_speak[256];

	memset(key , 0, sizeof(key));
	memset(vkey , 0, sizeof(vkey));
 
	// TODO Add play media structure element values tp apiAndParms.

	sprintf(apiAndParms,apiAndParmsFormat, mod, arc_val_tostr(zPlayMedia->party),
			arc_val_tostr(zPlayMedia->interruptOption), zPlayMedia->audioFileName, 
			zPlayMedia->videoFileName, arc_val_tostr(zPlayMedia->sync),
			arc_val_tostr(zPlayMedia->audioInformat), arc_val_tostr(zPlayMedia->audioOutformat),
			arc_val_tostr(zPlayMedia->videoInformat), arc_val_tostr(zPlayMedia->videoOutformat),
			arc_val_tostr(zPlayMedia->audioLooping), arc_val_tostr(zPlayMedia->videoLooping),
			arc_val_tostr(zPlayMedia->addOnCurrentPlay), arc_val_tostr(zPlayMedia->syncAudioVideo));


	rc = apiInit(mod, TEL_PLAYMEDIA, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
	
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"playMedia Struct vals : party = %d ; interruptOption = %d ;audioFileName = %s ;videoFileName = %s ;sync = %d ;audioInformat = %d ;audioOutformat = %d ;videoInformat = %d ;videoOutformat = %d ;audioLooping = %d ;videoLooping = %d ;addOnCurrentPlay = %d ;syncAudioVideo = %d ",zPlayMedia->party,zPlayMedia->interruptOption, zPlayMedia->audioFileName, zPlayMedia->videoFileName, zPlayMedia->sync, zPlayMedia->audioInformat, zPlayMedia->audioOutformat, zPlayMedia->videoInformat, zPlayMedia->videoOutformat, zPlayMedia->audioLooping, zPlayMedia->videoLooping, zPlayMedia->addOnCurrentPlay, zPlayMedia->syncAudioVideo);

//	telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
//					"TEL_PlayMedia Unsupported use TEL_Speak to play audio file");

//	return -1;


    if(strstr(zPlayMedia->audioFileName, "stream://") != NULL)
    {
        lStreaming = 1;
        *lUrl = '\0';
        strncpy(lUrl, zPlayMedia->audioFileName, sizeof(lUrl)-1);
		*((char *)zPlayMedia->audioFileName + 10) = '\0';
	}

 
 	if(zPlayMedia->sync == PUT_QUEUE_ASYNC && (lStreaming == 0))
	{
		zPlayMedia->sync = PUT_QUEUE;
	}

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"playMedia Struct vals : party = %d ; interruptOption = %d ;audioFileName = %s ;videoFileName = %s ;sync = %d ;audioInformat = %d ;audioOutformat = %d ;videoInformat = %d ;videoOutformat = %d ;audioLooping = %d ;videoLooping = %d ;addOnCurrentPlay = %d ;syncAudioVideo = %d ",zPlayMedia->party,zPlayMedia->interruptOption, zPlayMedia->audioFileName, zPlayMedia->videoFileName, zPlayMedia->sync, zPlayMedia->audioInformat, zPlayMedia->audioOutformat, zPlayMedia->videoInformat, zPlayMedia->videoOutformat, zPlayMedia->audioLooping, zPlayMedia->videoLooping, zPlayMedia->addOnCurrentPlay, zPlayMedia->syncAudioVideo);

	if(!parametersAreGood(mod, zPlayMedia))
		HANDLE_RETURN(TEL_FAILURE);

	if(zPlayMedia->audioFileName!= NULL && zPlayMedia->audioFileName[0] != '\0')
	{
		if(strcmp(zPlayMedia->audioFileName, "1"))
		{
			if(strstr(zPlayMedia->audioFileName, "stream://") == NULL)
			{
				if(access(zPlayMedia->audioFileName, F_OK) != 0)
				{
					//char reason[MAX_LENGTH];
					char reason[256];
					int rc;

					sprintf(reason, "Failed to read file (%s). [%s, %d]",
						zPlayMedia->audioFileName, strerror(errno), errno);

					telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
								TEL_CANT_SPEAK_MSG, zPlayMedia->audioFileName,
								arc_val_tostr(zPlayMedia->audioInformat),
								arc_val_tostr(zPlayMedia->audioOutformat),
								reason);
					rc = TEL_SOURCE_NONEXISTENT;
					if(rc != 0)
						HANDLE_RETURN(rc);
				}
			}
		}
	}


	if(zPlayMedia->videoFileName && zPlayMedia->videoFileName[0] != '\0')
	{
		if(strcmp(zPlayMedia->videoFileName, "1"))
		{
			if(strstr(zPlayMedia->videoFileName, "stream://") == NULL)
			{
				if(access(zPlayMedia->videoFileName, F_OK) != 0)
				{
					char reason[MAX_LENGTH];
					int rc;
					sprintf(reason,"Failed to read file (%s). [%s, %d]",
						zPlayMedia->videoFileName, errno, strerror(errno));
					telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
								TEL_CANT_SPEAK_MSG, zPlayMedia->videoFileName,
								arc_val_tostr(zPlayMedia->videoInformat),
								arc_val_tostr(zPlayMedia->videoOutformat),
								reason);
					rc = TEL_SOURCE_NONEXISTENT;
					if(rc != 0)
						return rc;
				}
			}
		}

	}



	if(zPlayMedia->audioFileName == NULL || zPlayMedia->audioFileName[0] == '\0')
	{
		sprintf(zPlayMedia->audioFileName, "%s","NULL");
	}
	if(zPlayMedia->videoFileName == NULL || zPlayMedia->videoFileName[0] == '\0')
	{
		sprintf(zPlayMedia->videoFileName, "%s", "NULL");
	}


	memset(&msg, 0, sizeof(&msg));


	/* Check for typed ahead keys */

	/* Note: msg is only set up to allow logging of unexpected msgs */
	msg.opcode=DMOP_GETDTMF;
	msg.appPid  = GV_AppPid;
	msg.appPassword = 0;
	if (zPlayMedia->party == FIRST_PARTY)
	{
		typeAheadBuffer = GV_TypeAheadBuffer1;
		msg.appCallNum=GV_AppCallNum1;
	}
	else
	{
		typeAheadBuffer = GV_TypeAheadBuffer2;
		msg.appCallNum=GV_AppCallNum2;
	}
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"typeAheadBuffer = %s", typeAheadBuffer);
	while(1)
	{
		rc = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		if(rc == -2) break;
		rc = examineDynMgrResponse(mod, &msg, &response);
		switch(rc)
		{
			case DMOP_GETDTMF:
				saveTypeAheadData(mod, response.appCallNum,
									response.message);
				break;
			case DMOP_DISCONNECT:
//				HANDLE_RETURN(disc(mod, response.appCallNum, GV_AppCallNum1, GV_AppCallNum2));
				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;
			default:
			/* Unexpected msg. Logged in examineDynMgr.. */
				break;
		} /* switch rc */
	}



    switch(zPlayMedia->interruptOption)
    {
        case FIRST_PARTY_INTERRUPT:
        case SECOND_PARTY_INTERRUPT:
            if(*typeAheadBuffer != '\0')
                HANDLE_RETURN(TEL_SUCCESS);
            break;

        case NONINTERRUPT:
            switch(zPlayMedia->party)
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




	msg.opcode = DMOP_PLAYMEDIA;


	if(zPlayMedia->interruptOption == NONINTERRUPT)
	{
		if((zPlayMedia->party == FIRST_PARTY) || (zPlayMedia->party == BOTH_PARTIES))
			msg.appCallNum = GV_AppCallNum1;
		else
			msg.appCallNum = GV_AppCallNum2;
	}
	else
	{
		if(zPlayMedia->interruptOption == SECOND_PARTY_INTERRUPT)
			msg.appCallNum = GV_AppCallNum2;
		else
			msg.appCallNum = GV_AppCallNum1;
	}




//	msg.appCallNum = GV_AppCallNum1;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;

	if(lStreaming == 1 && 
		(zPlayMedia->sync == PUT_QUEUE ||
		zPlayMedia->sync == ASYNC ||
		zPlayMedia->sync == SYNC))
	{
		if(zPlayMedia->sync != PUT_QUEUE_ASYNC)
		{
			rc = notifyPlayMediaStreamingClient(zPlayMedia, lUrl, audioStreamUrl, videoStreamUrl, &isVideoStream);
			if(rc < 0)
				HANDLE_RETURN(rc);
			sprintf(zPlayMedia->audioFileName, "%s", audioStreamUrl);
			sprintf(zPlayMedia->videoFileName, "%s", videoStreamUrl);
		}
		else
			sprintf(zPlayMedia->audioFileName, "%s", "NULL");
	
	}


	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"<audioFileName =%s>, lStreaming=%d, zPlayMedia->sync=%d", 
				zPlayMedia->audioFileName,
				lStreaming, zPlayMedia->sync);

	if(lStreaming == 1 && zPlayMedia->sync == PUT_QUEUE_ASYNC)
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"<lUrl=%s>", lUrl);
		sprintf(zPlayMedia->audioFileName, "%s", lUrl);
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"<audioFileName =%s>", zPlayMedia->audioFileName);
	}
	
#if 0
	if(isVideoStream == 1)
	{
		writePlayMediaParamsToFile(zPlayMedia, &msg);	
	}
	else 
#endif

	if(lStreaming != 1)
	{
		writePlayMediaParamsToFile(zPlayMedia, &msg);	
	}
	else
	if(lStreaming == 1 && zPlayMedia->sync == PUT_QUEUE_ASYNC)
	{
		writePlayMediaParamsToFile(zPlayMedia, &msg);	
	}
	if (zPlayMedia->party == FIRST_PARTY)
	{
        typeAheadBuffer = GV_TypeAheadBuffer1;
        msg.appCallNum=GV_AppCallNum1;
    }
    else
    {
        typeAheadBuffer = GV_TypeAheadBuffer2;
        msg.appCallNum=GV_AppCallNum2;
    }


	rc = tel_PlayMedia(mod, zPlayMedia, &msg, 1);

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"returning from tel_PlayMedia <rc = %d> <lStreaming = %d> <sync = %d >",
				rc, lStreaming, zPlayMedia->sync);
	

    if((lStreaming == 1) && (rc == 0) && (zPlayMedia->sync == PUT_QUEUE_ASYNC))
    {
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"Got Streaming with PUT_QUEUE_ASYNC notifying streaming client <lUrl = %s>",
						lUrl);
		rc = notifyPlayMediaStreamingClient(zPlayMedia, lUrl, audioStreamUrl, videoStreamUrl, &isVideoStream);
        if(rc < 0)
        {
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"notifyPlayMediaStreamingClient <rc = %d> calling CLEAR_QUEUE", rc);
            zPlayMedia->sync = CLEAR_QUEUE_ALL;
            *(zPlayMedia->audioFileName) = '\0';
			rc = writePlayMediaParamsToFile(zPlayMedia, &msg);
            rc = tel_PlayMedia(mod, zPlayMedia ,&msg, 1);

            HANDLE_RETURN(-1);
        }
    }
    HANDLE_RETURN(rc);
	
}

int tel_PlayMedia(char *mod, PlayMedia *zPlayMedia, void *m,
                int updateLastPhraseSpoken)
{

	int rc;
	int requestSent = 0;
	struct MsgToDM msg;
	struct MsgToApp response;
	struct MsgToApp pMediaRespBeforeStopActivity;
	int lStreaming = 0;
	int shouldSendDTMF = 0;
	msg=*(struct MsgToDM *)m;

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"<audioFileName =%s>", zPlayMedia->audioFileName);
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"zPlayMedia->interruptOption=%d", 
			zPlayMedia->interruptOption);

	if(strstr(zPlayMedia->audioFileName, "stream://") != NULL)
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"Setting <lStreaming = %d>", lStreaming);
		lStreaming = 1;
	}


	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"sync = %d", zPlayMedia->sync);
    if((lStreaming == 1) && (zPlayMedia->sync != PUT_QUEUE_ASYNC))
    {
        requestSent=1;
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Set requestSent to %d.", requestSent);
    }


	while ( 1 )
	{
		if (requestSent != 1)
		{
			rc=readResponseFromDynMgr(mod,-1,&response,
							sizeof(response));
			if(rc == -1) return(TEL_FAILURE);
			if(rc == -2)
			{
				rc = sendRequestToDynMgr(mod, &msg);
				if (rc == -1) return(-1);
				requestSent=1;

				rc = readResponseFromDynMgr(mod,0,
						&response,sizeof(response));
				if(rc == -1) return(TEL_FAILURE);
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,0,
					&response,sizeof(response));
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"Return code from readResponseFromDynMgr = %d", rc);

			if(rc == -1) return(TEL_FAILURE);
		}
	
		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
			case DMOP_PLAYMEDIA:
				if(zPlayMedia->sync == PUT_QUEUE_ASYNC)
				{
					/* response.message contains speakQueueId */
					getKeys(response.message);
					//memcpy(key, response.message, 29);
					//key[29] = '\0';
				}
				if(shouldSendDTMF == 1)
				{
					memcpy(&response, &pMediaRespBeforeStopActivity, sizeof(response));
					msg.opcode=DMOP_STOPACTIVITY;	
					telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
									"Setting msg.opcode to %d.", msg.opcode);
					return(TEL_SUCCESS);
				}
				return(response.returnCode);
				break;

			case DMOP_GETDTMF:
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
							"Got DMOP_GETDTMF in TEL_PlayMedia, zPlayMedia->interruptOption=%d.",
							zPlayMedia->interruptOption);
				if (response.appCallNum != msg.appCallNum)
				{
					telVarLog(mod,REPORT_DETAIL, TEL_INVALID_PARM, GV_Err,
								"Received appCallNum(%d), but it doesn't match (%d).",
								response.appCallNum, msg.appCallNum);
					/* Save data for the other leg of the call */
					saveTypeAheadData(mod, response.appCallNum, response.message);
					break;
				}

				switch(zPlayMedia->interruptOption)
				{
					case NONINTERRUPT: 
						telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
								"interruptOption = NONINTERRUPT");
						break;
					case INTERRUPT:
					case FIRST_PARTY_INTERRUPT:
					case SECOND_PARTY_INTERRUPT:
						telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
								"interruptOption = INTERRUPT || FIRST_PARTY_INTERRUPT || SECOND_PARTY_INTERRUPT");
						/* We've already weeded out if wrong callNum */
						saveTypeAheadData(mod, response.appCallNum,
											response.message);
						if (strcmp(response.message,"CLEAR")==0) 
						{
							break;
						}
						if (requestSent)
						{
                    		msg.opcode = DMOP_STOPACTIVITY;
							//DTMFchar = response.message;
							memcpy(&pMediaRespBeforeStopActivity, &response, sizeof(response));
                    		sendRequestToDynMgr(mod, &msg);
							shouldSendDTMF = 1;
							msg.opcode=DMOP_PLAYMEDIA;
						}
						return(TEL_SUCCESS);
                		break;
					case FIRST_PARTY_PLAYBACK_CONTROL:
					case SECOND_PARTY_PLAYBACK_CONTROL:
						saveTypeAheadData(mod, response.appCallNum, response.message);
					break;

					default:
						telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
								"Unknown interruptOption");
				}
				break;
			case DMOP_DISCONNECT:
				return(disc(mod, response.appCallNum)); 
				break;
			default:
				/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		} /* switch rc */
			
	} /* while */

}


static int writePlayMediaParamsToFile(PlayMedia *zPlayMedia, struct MsgToDM  *msg)
{
	int     		yFdDest;
	char        	buf[2048]; 
	int     		bytesWritten;
	char 			tempFileName[MAX_APP_MESSAGE_DATA];

	memset(tempFileName, 0, sizeof(tempFileName));

	sprintf(tempFileName, "/tmp/playmedia_%d_%d", msg->appPid, msg->appRef);
	
	if ((yFdDest = open(tempFileName, O_CREAT|O_WRONLY, 0777)) == -1)
	{
       	return(-1);
    }

    memset((char *)buf, 0, sizeof(buf));

	sprintf(buf, "%s%d%s"
				"%s%d%s"
				"%s%s%s"
				"%s%s%s"
				"%s%d%s"
				"%s%d%s"
				"%s%d%s"
				"%s%d%s"
				"%s%d%s"
				"%s%d%s"
				"%s%d%s"
				"%s%d%s"
				"%s%d",
				"party=",zPlayMedia->party, "\n",
				"interruptOption=",zPlayMedia->interruptOption, "\n",
				"audioFileName=",zPlayMedia->audioFileName, "\n",
				"videoFileName=",zPlayMedia->videoFileName, "\n",
				"sync=",zPlayMedia->sync, "\n",
				"audioInformat=",zPlayMedia->audioInformat, "\n",
				"audioOutformat=",zPlayMedia->audioOutformat, "\n",
				"videoInformat=",zPlayMedia->videoInformat, "\n",
				"videoOutformat=",zPlayMedia->videoOutformat, "\n",
				"audioLooping=",zPlayMedia->audioLooping, "\n",
				"videoLooping=",zPlayMedia->videoLooping, "\n",
				"addOnCurrentPlay=",zPlayMedia->addOnCurrentPlay, "\n",
				"syncAudioVideo=",zPlayMedia->syncAudioVideo);
		//printf(" The buf value = %s", buf);fflush(stdout);

	if ((bytesWritten = write(yFdDest, buf, strlen(buf))) != strlen(buf))
	{
		close(yFdDest);
		//(void) alnComUnlink(tempFileName);
		return(-1);
	}
	close(yFdDest);
	
	sprintf(msg->data, "%s", tempFileName);

	return 0;
}

static int parametersAreGood(char *mod, PlayMedia *zPlayMedia)
{

// TODO copy it from TEL_Speak
	int errors=0;
	char partyValues[]="FIRST_PARTY, SECOND_PARTY, BOTH_PARTIES";
	char interruptOptionValues[]="INTERRUPT, NONINTERRUPT, FIRST_PARTY_INTERRUPT, SECOND_PARTY_INTERRUPT";
//	char informatValues[]="STRING, MILITARY, STANDARD, TICS, MM_DD_YY, DD_MM_YY, YY_MM_DD, DOUBLE, INTEGER, PHONE, PHRASE_FILE, MILLISECONDS";
	char audioInformatValues[]="PHRASE_FILE";
	char videoInformatValues[]="VIDEO_FILE";
	char inoutFormat[]="Invalid input & output format combination: %d,%d.";
	char synchronousValues[]="SYNC, ASYNC, ASYNC_QUEUE, PUT_QUEUE, PLAY_QUEUE_SYNC, PLAY_QUEUE_ASYNC";
	char audioLoopingValues[] = "YES, NO, SPECIFIC";
	char videoLoopingValues[] = "YES, NO, SPECIFIC";
	char AddOnCurrentPlayValues[] = "YES, NO";
	char syncAudioVideoValues[] = "YES, NO, CLEAN_AUDIO, CLEAN_VIDEO";
	int inoutFormatError=0;
	int isItVideoStream;

	if(GV_Video_Codec[0] == '2' && strstr(zPlayMedia->audioFileName, "stream://") != NULL)
	{
		char 	yQuery[255];
		char    *zQuery = &(yQuery[0]);
		char    zType[16];		

		zQuery = strchr(zPlayMedia->audioFileName, '?');
		if(!zQuery || strlen(zQuery) == 0)
		{
			return (-1);
		}
//
//		djb - we don't do video
//
//		getPlayMediaStreamingParams(zQuery, zType);
//        
//		if(!strcmp(zType, "VIDEO") ||
//            !strcmp(zType, "VGREETING"))
//		{
//			isItVideoStream = 1;
//		}
	}



	if(zPlayMedia->sync == PLAY_QUEUE_SYNC || zPlayMedia->sync == PLAY_QUEUE_ASYNC)
	{
		if ( strcmp(zPlayMedia->videoFileName, "NULL") == 0 )
		{
			zPlayMedia->videoFileName[0] = '\0';
		}
		if((!zPlayMedia->audioFileName  || zPlayMedia->audioFileName[0]== '\0') &&
			(!zPlayMedia->videoFileName  || zPlayMedia->videoFileName[0]== '\0'))
		{
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"[%s, %d] Failed to interpret mediaSyncOption = %d, but no queue is specified to play",
						__FILE__, __LINE__, zPlayMedia->sync);

			errors++;		
			
		}
	}


	switch(zPlayMedia->party)
	{
		case FIRST_PARTY:
			break;
		case SECOND_PARTY:
			if (!GV_Connected2)
			{
				errors++;
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
							"Failed to access party=SECOND_PARTY, but second party is not connected.");
			}
			break;

		case BOTH_PARTIES:
			if (!GV_Connected2)
			{
				errors++;
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
							"Failed to access party=BOTH_PARTIES, but second party is not connected.");
			}
			break;
		case SECOND_PARTY_NO_CALLER:
		/* Do we want to support this option?? */
			break;
		default:
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"Failed to interpret value for party: %d. Valid values are %s.",
						zPlayMedia->party, partyValues);
			break;
	}
 
	switch(zPlayMedia->interruptOption)
	{
		case INTERRUPT:
			switch(zPlayMedia->party)
			{
				case FIRST_PARTY:
					telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
								"Invalid interrupt_option: INTERRUPT. "
								"Using FIRST_PARTY_INTERRUPT.");
					zPlayMedia->interruptOption = FIRST_PARTY_INTERRUPT;
					break;
				case SECOND_PARTY:
				case SECOND_PARTY_NO_CALLER:
					telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
								"Invalid interrupt_option: INTERRUPT. "
								"Using SECOND_PARTY_INTERRUPT.");
					zPlayMedia->interruptOption = SECOND_PARTY_INTERRUPT;
					break;
				case BOTH_PARTIES:
					telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
								"Invalid interrupt_option: INTERRUPT. "
								"Using NONINTERRUPT.");
					zPlayMedia->interruptOption = NONINTERRUPT;
					break;
        		default:
					break;
			}
			break;
        
		case NONINTERRUPT:
			break;

		case FIRST_PARTY_INTERRUPT:
			if((zPlayMedia->party == SECOND_PARTY) ||
				(zPlayMedia->party == SECOND_PARTY_NO_CALLER))
			{
				telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
							"Invalid interrupt_option: FIRST_PARTY_INTERRUPT "
							"when party is SECOND_PARTY or SECOND_PARTY_NO_CALLER. "
							"Using SECOND_PARTY_INTERRUPT.");
				zPlayMedia->interruptOption = SECOND_PARTY_INTERRUPT;
			}
			break;
		case SECOND_PARTY_INTERRUPT:
			if(zPlayMedia->party == FIRST_PARTY)
			{
				telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
							"Invalid interrupt_option: SECOND_PARTY_INTERRUPT "
							"when party is FIRST_PARTY. "
							"Using FIRST_PARTY_INTERRUPT.");
				zPlayMedia->interruptOption = FIRST_PARTY_INTERRUPT;
			}
			break;

#if 1
		case FIRST_PARTY_PLAYBACK_CONTROL:
			if(isItVideoStream == 1)
			{
				zPlayMedia->interruptOption = FIRST_PARTY_INTERRUPT;
			}
			else
			{
				zPlayMedia->interruptOption = FIRST_PARTY_PLAYBACK_CONTROL;
			}
			break;
		case SECOND_PARTY_PLAYBACK_CONTROL:
			if(isItVideoStream == 1)
			{
				zPlayMedia->interruptOption = FIRST_PARTY_INTERRUPT;
			}
			else
			{
				zPlayMedia->interruptOption = SECOND_PARTY_PLAYBACK_CONTROL;
			}
			break;
#endif

	default:
			errors++;
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"Failed to interpret the values for interruptOption: %d. "
						"Valid values are %s.",
						zPlayMedia->interruptOption, interruptOptionValues);
			break;
	}



	if(zPlayMedia->sync == PUT_QUEUE ||
		zPlayMedia->sync == PUT_QUEUE_ASYNC ||
		zPlayMedia->sync == SYNC ||
		zPlayMedia->sync == ASYNC)
	{
		if(zPlayMedia->audioFileName && zPlayMedia->audioFileName[0]!= '\0')
		{	
			switch(zPlayMedia->audioInformat)
			{
	
				case PHRASE_FILE:
					switch(zPlayMedia->audioOutformat)
					{
						case PHRASE:
//TODO To be deleted
						case PHRASE_FILE:
							break;
						default:
							inoutFormatError=1;
							break;
					}
				break;
		
//				case MILLISECONDS:
//					break;
			
				default:
					errors++;
					telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
								"Failed to interpret value for audioInformat: %d. "
								"Valid values are %s.", zPlayMedia->audioInformat, audioInformatValues);
					break;
			}
		}

		if (inoutFormatError)
    	{
			errors++;
			sprintf(Msg, inoutFormat, zPlayMedia->audioInformat, zPlayMedia->audioOutformat);
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, Msg);
    	}


		if(zPlayMedia->videoFileName && zPlayMedia->videoFileName[0]!= '\0')
		{
			switch(zPlayMedia->videoInformat)
	    	{
    		    case VIDEO_FILE:
        		    switch(zPlayMedia->videoOutformat)
            		{
                		case VIDEO:
                    		break;
	      	          default:
    	    	            inoutFormatError=1;
        	    	        break;
            		}
		        break;

    		    default:
        		    errors++;
            		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
                		        "Failed to interpret value for informat: %d. "
                    		    "Valid values are %s.", zPlayMedia->videoInformat, videoInformatValues);
		            break;
    		}
		}

	    if (inoutFormatError)
		{
			errors++;
			sprintf(Msg, inoutFormat, zPlayMedia->videoInformat, zPlayMedia->videoOutformat);
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, Msg);
		}	

	}



	switch(zPlayMedia->sync)
	{
		case SYNC:
		case ASYNC:
		case ASYNC_QUEUE:
		case PUT_QUEUE:
		case PUT_QUEUE_ASYNC:
		case PLAY_QUEUE_SYNC:
		case PLAY_QUEUE_ASYNC:
		case CLEAR_QUEUE_ALL:
		case CLEAR_QUEUE_AUDIO:
		case CLEAR_QUEUE_VIDEO:
			break;
		default:
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"Failed to interpret value for sync: %d. "
						"Valid values are %s.", zPlayMedia->sync, synchronousValues);
			break;
	}



	if(zPlayMedia->audioFileName && zPlayMedia->audioFileName[0]!= '\0')
	{
		switch(zPlayMedia->audioLooping)
		{
			case SPECIFIC 	:
				if(zPlayMedia->sync == SYNC || zPlayMedia->sync == ASYNC ||
					zPlayMedia->sync == PUT_QUEUE || zPlayMedia->sync == PUT_QUEUE_ASYNC)
				{
					telVarLog(mod,REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
								"audioLooping = %d is not valid for sync = %d. "
								"Valid values are %d/%d.", zPlayMedia->audioLooping, zPlayMedia->sync,YES,NO);
				}	
			case YES		:
			case NO			:
				break;

			default			:
//				errors++;
				telVarLog(mod,REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
							"Invalid value for audioLooping: %d. "
							"Setting value to Default = NO.", zPlayMedia->audioLooping);
				zPlayMedia->audioLooping = NO;
	            break;
		}
	}

	
	if(zPlayMedia->videoFileName && zPlayMedia->videoFileName[0]!= '\0')
	{
		switch(zPlayMedia->videoLooping)
		{
			case SPECIFIC	:
				if(zPlayMedia->sync == SYNC || zPlayMedia->sync == ASYNC ||
					zPlayMedia->sync == PUT_QUEUE || zPlayMedia->sync == PUT_QUEUE_ASYNC)
				{
					telVarLog(mod,REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
								"videoLooping = %d is not valid for sync = %d. "
								"Valid values are %d/%d.", zPlayMedia->videoLooping, zPlayMedia->sync,YES,NO);
				}
	
			case YES		:
			case NO			:
				break;

			default:
//				errors++;
				telVarLog(mod,REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
							"Invalid value for videoLooping: %d. "
							"Setting value to default = NO.", zPlayMedia->videoLooping);
				zPlayMedia->videoLooping = NO;
				break;
		}
	}



	if((zPlayMedia->audioFileName  && zPlayMedia->audioFileName[0]== '\0') ||
		(zPlayMedia->videoFileName  && zPlayMedia->videoFileName[0]== '\0'))
	{
		if(zPlayMedia->syncAudioVideo != NO)
		{
			telVarLog(mod,REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
						"Current syncAudioVideo = %d either AudioFile is not present or "
						" videoFileName is not present so syncAudioVideo to NO."
						, zPlayMedia->syncAudioVideo);
			zPlayMedia->syncAudioVideo = NO;
		}
	}

	switch(zPlayMedia->syncAudioVideo)
	{
		case YES        :
		case CLEAN_AUDIO:
		case CLEAN_VIDEO:
			if(zPlayMedia->videoLooping != zPlayMedia->audioLooping)
			{
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Info,
							"Current syncAudioVideo = %d But audioLooping(%d) and videoLooping(%d)"
							" param are not matched so setting audioLooping and videoLooping  to NO."
							, zPlayMedia->syncAudioVideo, zPlayMedia->audioLooping, zPlayMedia->videoLooping);
				zPlayMedia->audioLooping = NO;
				zPlayMedia->videoLooping = NO;

			}
			if(zPlayMedia->sync == PUT_QUEUE || zPlayMedia->sync == PUT_QUEUE_ASYNC)
			{
				if((zPlayMedia->audioFileName  && zPlayMedia->audioFileName[0]== '\0') ||
					(zPlayMedia->videoFileName  && zPlayMedia->videoFileName[0]== '\0'))
				{
					telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Info,
								"Current syncAudioVideo = %d either AudioFile is not present or "
								" videoFileName is not present so syncAudioVideo to NO."
								, zPlayMedia->syncAudioVideo);			
					zPlayMedia->syncAudioVideo = NO;	
				}
			}
				
		case NO         :
			break;

		default:
//			errors++;
			
			telVarLog(mod,REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
						"Invalid value for syncAudioVideo: %d. "
						"Setting to Default = NO.", zPlayMedia->syncAudioVideo);
			zPlayMedia->syncAudioVideo = NO;
			break;
	}



    return(errors == 0);
 
}





static int notifyPlayMediaStreamingClient(PlayMedia *zMsgSpeak, char *zUrl,
									char *zAudioUrl, char *zVideoUrl, 
									int *isVideoStream)
{

	int 	rc, fd;
	static char 	mod[] = "notifyPlayMediaStreamingClient";
	struct 	Msg_AppToAsc yAppToAsc;
	char 	fifoName[128];
	char 	videofifoName[128];
	char 	zSubscriber[40];
	char 	zType[16];
	char	zSession[40];
	int 	zMsgId;
	int 	zNodeId;
	long 	zTime;
	char 	zAudioCodec[40];
	int 	zVideoMsgId; 
	int 	zVideoNodeId;	
	long 	zVideoTime;
	char 	zVideoCodec[40];

	char 	yUrl[255];
	int 	zDefaultPort;
	char 	zProtocol[20];
	char 	zWebServer[255];
	char 	zPort[10];
	char 	yQuery[255];
	char 	*zQuery = &(yQuery[0]);
	char 	zErrorMsg[255];

	static int totalStreamRequests = 0;

	memset(&yAppToAsc, 0, sizeof(struct Msg_AppToAsc));
	if(zMsgSpeak->sync == PUT_QUEUE_ASYNC)
		yAppToAsc.async = 1;
	else
		yAppToAsc.async = 0;


	sprintf(yAppToAsc.file, "%s", "\0");

	zQuery = strchr(zUrl, '?');
	if(!zQuery || strlen(zQuery) == 0)
	{
		return (-1);
	}

//	rc = getPlayMediaStreamingParams(zQuery, zSubscriber, &zMsgId, &zNodeId, &zTime);
	rc = getPlayMediaStreamingParams(zQuery, zSubscriber, zType, zSession, &zMsgId, &zNodeId, &zTime, zAudioCodec,
										&zVideoMsgId, &zVideoNodeId, &zVideoTime, zVideoCodec);
	if(rc < 0) return (rc);


	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"Stream Params = <zSubscriber=%s; zType=%s; zSession=%s; zMsgId=%d; zNodeId=%d;"
				" zTime=%d; zAudioCodec=%s; zVideoMsgId=%d; zVideoNodeId=%d;"
				" zVideoTime=%d; zVideoCodec=%s",
				zSubscriber, zType, zSession,zMsgId, zNodeId, zTime, zAudioCodec,
				zVideoMsgId, zVideoNodeId, zVideoTime, zVideoCodec);

	int isItAudioOfVideo = 0;

	if(GV_Video_Codec[0] == '2')
	{
		if(!strcmp(zType, "VIDEO") ||
			!strcmp(zType, "VGREETING"))
		{
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"zType = %s so calling createAudioUrl and createVideoUrl", zType);
			*(isVideoStream) = 1;
			createAudioUrl(zUrl, zAudioUrl , zSubscriber, zType, zSession, 
							zMsgId, zNodeId, zTime, zAudioCodec);
			createVideoUrl(zUrl, zVideoUrl , zSubscriber, zType, zSession, 
							zVideoMsgId, zVideoNodeId, zVideoTime, zVideoCodec);
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"VideoUrl = (%s) , zAudioUrl = (%s)",
					zVideoUrl, zAudioUrl);
			sprintf(videofifoName, "%s/%s.VIDEO", ASC_DIR, ASC_MainFifo);
		}
		else
		{
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"zType = (%s); so its a normal audio streaming.", zType);
						*(isVideoStream) = 0;
		}
	}
	else
	{
		if(!strcmp(zType, "VIDEO") ||
            !strcmp(zType, "VGREETING"))
		{
			createAudioUrl(zUrl, zAudioUrl , zSubscriber, zType, zSession, 
							zMsgId, zNodeId, zTime, zAudioCodec);
			isItAudioOfVideo = 1;
		}
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"zType = (%s); its not a video call.", zType);
					*(isVideoStream) = 0;
	}
	
	sprintf(fifoName, "%s/%s", ASC_DIR, ASC_MainFifo);

	if(totalStreamRequests == 0)
	{
		yAppToAsc.opcode        = SCOP_DOWNLOAD_FIRST_TIME;
	}
	else
	{
		yAppToAsc.opcode        = SCOP_DOWNLOAD;
	}

	yAppToAsc.appCallNum		= GV_AppCallNum1;
	yAppToAsc.appPid			= GV_AppPid;
	yAppToAsc.appRef			= GV_AppRef-1;
	yAppToAsc.appPassword		= GV_AppPassword;

	yAppToAsc.allParties		= zMsgSpeak->party;
	yAppToAsc.interruptOption	= zMsgSpeak->interruptOption;
	yAppToAsc.synchronous		= zMsgSpeak->sync;
	yAppToAsc.dmId			= GV_DynMgrId;
	//yAppToAsc.dmId				= -1;

	if(*(isVideoStream) == 1 || isItAudioOfVideo == 1)
	{
		sprintf(yAppToAsc.file, "%s", zAudioUrl);
	}
	else 
	{
		createAudioUrl(zUrl, zAudioUrl , zSubscriber, zType, zSession, 
							zMsgId, zNodeId, zTime, zAudioCodec);
		sprintf(yAppToAsc.file, "%s", zAudioUrl);
		//sprintf(zAudioUrl, "%s", zUrl);
		sprintf(zVideoUrl, "%s", "NULL");
	}
//	sprintf(yAppToAsc.key, "%s", zMsgSpeak->key);
	sprintf(yAppToAsc.key, "%s", key);

	fd = open(fifoName, O_WRONLY|O_NONBLOCK);
	if (fd < 0)
	{
		telVarLog(mod ,REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
					"Failed to open request fifo (%s) for writting. [%d, %s], "
					"arcStreamingClient may not be running.",
					fifoName, errno, strerror(errno));

		return(-1);
	}

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"writing to fifo %s <fileName=%s>", fifoName, yAppToAsc.file);

	rc = write(fd, &(yAppToAsc), sizeof(struct Msg_AppToAsc));
	if(rc == -1)
	{
		telVarLog(mod ,REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
                    "Failed to write to request fifo (%s) for audio. [%d, %s]",
                    fifoName, errno, strerror(errno));

		close(fd);
		return(-1);
	}

	if(*isVideoStream == 1)
	{
		sprintf(yAppToAsc.file, "%s", zVideoUrl);
		sprintf(yAppToAsc.key, "%s", vkey);
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
                "Writing to fifo (%s) fileName=(%s).", fifoName, yAppToAsc.file);

		rc = write(fd, &(yAppToAsc), sizeof(struct Msg_AppToAsc));
		if(rc == -1)
		{
			telVarLog(mod ,REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
						"Failed to write to request fifo (%s) for video. [%d, %s]",
						fifoName, errno, strerror(errno));

			close(fd);
			return(-1);
		}
	}

	close(fd);
	totalStreamRequests++;

	return(0);

}/*END: static int notifyPlayStreamingClient*/

static int getPlayMediaValue(
        char    *zStrInput,
        char    *zStrOutput,
        char    *zStrKey,
        char    zChStartDelim,
        char    zChStopDelim)
{
    int         IsSingleLineCommentOn = 0;
    int         IsMultiLineCommentOn  = 0;
    int         yIntLength = 0;
    int         yIntIndex  = 0;
    char        *yStrTempStart;
    char        *yStrTempStop;
    char        yNext;
    char        yPrev;

    /*
     * Initialize vars
     */
    zStrOutput[0] = 0;

    if(!zStrInput || !(*zStrInput))
    {
        return(-1);
    }

    if(!zStrKey || !(*zStrKey))
    {

        return(-1);
    }

    yIntLength = strlen(zStrInput);

    for(yIntIndex = 0; yIntIndex < yIntLength; yIntIndex ++ )
    {
        if(yIntIndex == 0)
            yPrev = zStrInput[yIntIndex];
        else
            yPrev = zStrInput[yIntIndex - 1];

        if(yIntIndex == yIntLength - 1)
            yNext = zStrInput[yIntIndex];
        else
            yNext = zStrInput[yIntIndex + 1];


        if( (   yIntIndex == 0 ||
                yPrev == ' '    ||
                yPrev == '\t'   ||
                yPrev == '\n' )&&
                strstr(zStrInput + yIntIndex, zStrKey) ==
                zStrInput + yIntIndex)
        {
            //yIntIndex += strlen(zStrKey);

            yStrTempStart = strchr(zStrInput + yIntIndex,
                    zChStartDelim) + 1;

            yStrTempStop =  strchr(zStrInput + yIntIndex,
                    zChStopDelim);

            if(!yStrTempStop    ||
                !yStrTempStart  ||
                yStrTempStop <  yStrTempStart)
                {
                    zStrOutput[0] = 0;
                    return(0);
                }

            if( strchr(zStrInput + yIntIndex, '\n') &&
                strchr(zStrInput + yIntIndex, '\n') <
                yStrTempStop)
                {
                    zStrOutput[0] = 0;
                    return(0);
                }

            strncpy(zStrOutput,
                    yStrTempStart,
                    yStrTempStop - yStrTempStart);

            zStrOutput[yStrTempStop - yStrTempStart] = 0;


            return(0);
        }

    }

    return(0);

} /* END: getPlayMediaValue() */






static int getPlayMediaStreamingParams(char *zQuery, char *zSubscriber,
                        char * zType,
						char *zSessionId,
						int *zMsgId,
                        int *zNodeId,
                        long *zTime,
						char *zCodec,
                        int *zVideoMsgId, 
						int *zVideoNodeId, 
						long *zVideoTime, 
						char *zVideoCodec)
{
    int rc;
    int index;

    char tmpVal[100];

    strcat(zQuery, "&");

    for(index = 0; index < strlen(zQuery); index++)
    {
        sprintf(tmpVal, "%s", "0");
        if((strstr(zQuery + index, ASC_NODEID) == zQuery + index ) && 
			(strstr(zQuery + index, ASC_VIDEO_NODEID) != zQuery + index) )
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_NODEID,
                    '=',
                    '&');

            *zNodeId = atoi(tmpVal);

			index = index + strlen(ASC_NODEID) - 1;
        }
        else
        if(strstr(zQuery + index, ASC_SUBSCRIBERID) == zQuery + index)
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_SUBSCRIBERID,
                    '=',
                    '&');

            sprintf(zSubscriber, "%s", tmpVal);

        }
        else
        if(strstr(zQuery + index, ASC_SESSIONID) == zQuery + index)
        {
            getValue(
                    zQuery + index,
                    tmpVal,
                    ASC_SESSIONID,
                    '=',
                    '&');

            sprintf(zSessionId, "%s", tmpVal);
        }

        else
        if( (strstr(zQuery + index, ASC_MESSAGEID) == zQuery + index) &&
			(strstr(zQuery + index, ASC_VIDEO_MESSAGEID) != zQuery + index))
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_MESSAGEID,
                    '=',
                    '&');

            *zMsgId = atoi(tmpVal);

			index = index + strlen(ASC_MESSAGEID) - 1;
        }
        else
        if( (strstr(zQuery + index, ASC_DICTATIONTIME) == zQuery + index) &&
			(strstr(zQuery + index, ASC_VIDEO_DICTATIONTIME) != zQuery + index))
			
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_DICTATIONTIME,
                    '=',
                    '&');

            *zTime = atol(tmpVal);

			index = index + strlen(ASC_DICTATIONTIME) - 1;
        }
		else
        if( (strstr(zQuery + index, ASC_CODEC) == zQuery + index) &&
			(strstr(zQuery + index, ASC_VIDEO_CODEC) != zQuery + index))
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_CODEC,
                    '=',
                    '&');
            
			sprintf(zCodec, "%s", tmpVal);

			index = index + strlen(ASC_CODEC) - 1;
        }
        else
        if(strstr(zQuery + index, ASC_VIDEO_MESSAGEID) == zQuery + index)
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_VIDEO_MESSAGEID,
                    '=',
                    '&');

            *zVideoMsgId = atol(tmpVal);
			
			index = index + strlen(ASC_VIDEO_MESSAGEID) - 1;

        }
        else
        if(strstr(zQuery + index, ASC_VIDEO_DICTATIONTIME) == zQuery + index)
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_VIDEO_DICTATIONTIME,
                    '=',
                    '&');

            *zVideoTime = atol(tmpVal);
			
			index = index + strlen(ASC_VIDEO_DICTATIONTIME) - 1;

        }
        else
        if(strstr(zQuery + index, ASC_VIDEO_NODEID) == zQuery + index)
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_VIDEO_NODEID,
                    '=',
                    '&');

            *zVideoNodeId = atol(tmpVal);
			
			index = index + strlen(ASC_VIDEO_NODEID) - 1;

        }
        else
        if(strstr(zQuery + index, ASC_VIDEO_CODEC) == zQuery + index)
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_VIDEO_CODEC,
                    '=',
                    '&');

            sprintf(zVideoCodec, "%s", tmpVal);
			
			index = index + strlen(ASC_VIDEO_CODEC) - 1;

        }
        else
        if(strstr(zQuery + index, ASC_REQUESTTYPE) == zQuery + index)
        {
            getPlayMediaValue(
                    zQuery + index,
                    tmpVal,
                    ASC_REQUESTTYPE,
                    '=',
                    '&');

            sprintf(zType, "%s", tmpVal);

			index = index + strlen(ASC_REQUESTTYPE) - 1;
        }

//printf("ARCDEBUG: %s %s %d (query+index)=(%s) tmpVal=(%s)\n", __FILE__, __FUNCTION__, __LINE__, zQuery + index, tmpVal);fflush(stdout);


    }
}


int createVideoUrl(char *zUrl, char *zVideoUrl, 
					char *zSubscriber, char *zType,
					char *zSessionId, 
					int zVideoMsgId, int zVideoNodeId, 
					long zVideoTime, char *zVideoCodec)
{
	static char	mod[] = {"createVideoUrl"};
	char	*zQuery;
	char	initStream[255];
	int		urlLength = strlen(zUrl);
	int		queryLength;	

	if(urlLength <= 0)
		return -1;

	zQuery = strchr(zUrl, '?');
	if(!zQuery || strlen(zQuery) == 0)
	{
		return (-1);
	}
	queryLength = strlen(zQuery);

	snprintf(initStream, (urlLength - queryLength +2), "%s", zUrl);
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"initStream =  (%s)",
						initStream);

	sprintf(zVideoUrl, 
            "%s"
			"%s%s%s"
			"%s%s%s"
			"%s%s%s"
			"%s%d%s"
			"%s%d%s"
			"%s%d%s"
			"%s%s%s"
			"%s%s%s",
			initStream,
			"subscriberid=",zSubscriber, "&",
			"type=",zType, "&",
			"session=",zSessionId, "&",
			"vmsgid=",zVideoMsgId, "&",
			"vnodeid=",zVideoNodeId, "&",
			"vdictationtime=",zVideoTime , "&",
			"vcodec=",zVideoCodec, "&",
			"dest_vCodec=",GV_Video_Codec, "&");
	
	return 0;
}

static int createAudioUrl(char *zUrl, char *zAudioUrl,
                    char *zSubscriber, char *zType,
					char *zSessionId,
                    int zMsgId, int zNodeId,
                    long zTime, char *zCodec)
{

	static char	mod[] = {"createAudioUrl"};
	char	*zQuery;
	char	initStream[255];
	int		urlLength = strlen(zUrl);
	int		queryLength;	

	if(urlLength <= 0)
		return -1;

	zQuery = strchr(zUrl, '?');
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"zQuery =  (%s)",
						zQuery);
	if(!zQuery || strlen(zQuery) == 0)
	{
		return (-1);
	}

	queryLength = strlen(zQuery);
	snprintf(initStream, (urlLength - queryLength +2), "%s", zUrl);

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"initStream = (%s)", initStream);
	
	sprintf(zAudioUrl, "%s"
			"%s%s%s"
			"%s%s%s"
			"%s%s%s"
			"%s%d%s"
			"%s%d%s"
			"%s%d%s"
			"%s%s%s"
			"%s%s%s",
			initStream,
			"subscriberid=",zSubscriber, "&",
			"type=",zType, "&",
			"session=",zSessionId, "&",
			"msgid=",zMsgId, "&",
			"nodeid=",zNodeId, "&",
			"dictationtime=",zTime , "&",
			"codec=",zCodec, "&",
			"dest_aCodec=",GV_Audio_Codec, "&");
	

    return 0;
}

static int getKeys(char *zKeyMessage)
{
	static char mod[] = {"getKeys"};
	char *zQuery;
    zQuery = strchr(zKeyMessage, ';');

    if(!zQuery || strlen(zQuery) == 0)
    {
		sprintf(key, "%s", zKeyMessage);
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"key =(%s)", key);
    }
	else
	{
    	sscanf(zKeyMessage, "%[^;];%[^=]", key, vkey);
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"key = (%s), vkey = (%s)", key, vkey);
	}

	return 0;
}
