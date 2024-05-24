/*-----------------------------------------------------------------------------
Program:        TEL_Speak.c
Purpose:        Speak phrases and system files.
Author:         Wenzel-Joglekar
Date:		08/24/2000
Upate:		10/19/2000 
Upate:	03/28/2001 APJ Added updateLastPhraseSpoken to tel_Speak.
Upate:	04/25/2001 APJ Changed some of the valid in-out format combination.
Upate:	04/25/2001 APJ Took out PLAY_BACK_CONTROL option.
Update: 04/27/2001 apj In parameter validation, put the missing value
		       passed in the log message.
Update: 05/02/2001 apj For INTERRUPT, making default for first party as 
		       FIRST_PARTY_INTERRUPT, for second party as 
		       SECOND_PARTY_INTERRUPT & NONINTERRUPT for both parties.
Update: 05/02/2001 apj For first party, Make SECOND_PARTY_INTERRUPT to
		       FIRST_PARTY_INTERRUPT and viceaversa.
Update: 06/28/2001 apj Look for typed ahead keys.
Update: 07/17/2001 apj Added tel_PlayTag code.
Update: 10/31/2001 apj Decide which parties voice handle to use when party is 
					   BOTH_PARTIES depending upon interrupt option.
Update: 11/01/2001 apj In append_system_phrase, removed hardcoding of wav.
Update: 11/01/2001 apj Added ASYNC_QUEUE.
Update: 01/14/2002 apj Changed -7 return code to TEL_SOURCE_NONEXISTENT define.
Update: 03/07/2002 apj After sending DTMF interrupt to DM, sleep 50 ms.
Update: 04/26/2005 ddn Added code for SIPIVR Streaming
Update: 09/13/2007 djb Added mrcpTTS changes.
Update: 04/27/2011 djb MR 3469.  Undid arcML change.
Update: 07/18/2011 djb MR 3201.  Commented out return from SL_append_number().
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "ttsStruct.h"


/* Function prototypes */
static int appendFiles(int fdw, int fdr, char *writeF, char *readF, char *msg);
static int make_silence_file(int millisec, int port, char *out, char *msg);
static int make_list(char *mod, int in, int out, char *s, char *list);
static int make_number_list(char *mod, int in, int out, char * s, char *list);
static int make_digit_list(char *mod, int in, int out, char * s, char *list);
static int make_date_list(char *mod, int in, int out, char *s, char *list);
static int make_dollar_list(char *mod, int in, int out, char *s, char *list);
static int make_phrase_file(char *mod, int in, int out, char *s, char *file);
static int make_alphanum_list(char *mod, int in, int out, char *s, char *list);
static int make_day_list(char *mod, int in, int out, char *s, char *list);
static int make_time_list(char *mod, int in, int out, char *s, char *list);
static int append_month(char *mod, int month);
static int append_year(char *mod, int year, int num_of_digits);
static int append_day_of_month(char *mod, int day);
static int convert_time_to_seconds(char *mod, char *time_str);
static int SL_append_time(char *mod, int format, long tics);
static int SL_append_number(char *mod, int in, int out, long number);
static int adjust_file_for_APP_PHRASE_DIR( char *file, char *adjusted_file);
static int tel_PlayTag(char *zMod, int zParty, char *zPhraseTag, int zOption,
                                int zMode, char *zDataOverrides);

void removeNonDigits(char *string);
void removeNonTelChars(char *input);

static int parametersAreGood(char *mod, int party, int *interruptOption, 
			int informat, int outformat, void *data, 
			int synchronous);

static FILE *fp_list;


int getValue(
		char	*zStrInput,   
		char	*zStrOutput,
		char	*zStrKey,
		char	zChStartDelim,
		char	zChStopDelim)
{
	int			IsSingleLineCommentOn = 0;
	int			IsMultiLineCommentOn  = 0;
	int			yIntLength = 0;
	int			yIntIndex  = 0;
	char		*yStrTempStart;
	char		*yStrTempStop;
	char		yNext;
	char		yPrev;

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


		if( (	yIntIndex == 0 ||
				yPrev == ' '	||   
				yPrev == '\t'	||   
				yPrev == '\n' )&&
				strstr(zStrInput + yIntIndex, zStrKey) ==  
				zStrInput + yIntIndex)
		{
			//yIntIndex += strlen(zStrKey);

			yStrTempStart = strchr(zStrInput + yIntIndex,  
					zChStartDelim) + 1;

			yStrTempStop =  strchr(zStrInput + yIntIndex,  
					zChStopDelim);

			if(!yStrTempStop 	||
				!yStrTempStart 	||
				yStrTempStop <	yStrTempStart) 
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

} /* END: getValue() */

int getStreamingParams(char *zQuery, char *zSubscriber, 
						int *zMsgId, 
						int *zNodeId, 
						long *zTime)
{
	int rc = 0;
	int index;

	char tmpVal[100];

	strcat(zQuery, "&");

	for(index = 0; index < strlen(zQuery); index++)
	{
		sprintf(tmpVal, "%s", "0");
		if(strstr(zQuery + index, ASC_NODEID) == zQuery + index)
		{
			getValue(
					zQuery + index,   
					tmpVal,
					ASC_NODEID,
					'=',
					'&');

			*zNodeId = atoi(tmpVal);

		}
		else
		if(strstr(zQuery + index, ASC_SUBSCRIBERID) == zQuery + index)
		{
			getValue(
					zQuery + index,   
					tmpVal,
					ASC_SUBSCRIBERID,
					'=',
					'&');

			sprintf(zSubscriber, "%s", tmpVal);
		}
		else
		if(strstr(zQuery + index, ASC_MESSAGEID) == zQuery + index)
		{
			getValue(
					zQuery + index,   
					tmpVal,
					ASC_MESSAGEID,
					'=',
					'&');

			*zMsgId = atoi(tmpVal);
		}
		else
		if(strstr(zQuery + index, ASC_DICTATIONTIME) == zQuery + index)
		{
			getValue(
					zQuery + index,   
					tmpVal,
					ASC_DICTATIONTIME,
					'=',
					'&');

			*zTime = atol(tmpVal);
		}

	}

    return rc;
}


int notifyStreamingClient(struct Msg_Speak zMsgSpeak, char *zUrl)
{

	int rc, fd;
	char mod[] = "notifyStreamingClient";
	struct Msg_AppToAsc yAppToAsc;
	char fifoName[128];
	char zSubscriber[40];
	int	zMsgId;
	int zNodeId;
	long zTime;

	char yUrl[255];
	int zDefaultPort;
	char zProtocol[20]; 
	char zWebServer[255]; 
	char zPort[10]; 
	char yQuery[255]; 
	char *zQuery = &(yQuery[0]); 
	char zErrorMsg[255];

	static int totalStreamRequests = 0;

	memset(&yAppToAsc, 0, sizeof(struct Msg_AppToAsc));
	if(zMsgSpeak.synchronous == PUT_QUEUE_ASYNC)
		yAppToAsc.async = 1;
	else
		yAppToAsc.async = 0;


	sprintf(yAppToAsc.file, "%s", "\0");

	zQuery = strchr(zUrl, '?');
	if(!zQuery || strlen(zQuery) == 0)
	{
		return (-1);
	}

	rc = getStreamingParams(zQuery, zSubscriber, &zMsgId, &zNodeId, &zTime);
	if(rc < 0) return (rc);

	sprintf(fifoName, "%s/%s", ASC_DIR, ASC_MainFifo);

	if(totalStreamRequests == 0)
	{
		yAppToAsc.opcode 		= SCOP_DOWNLOAD_FIRST_TIME;
	}
	else
	{
		yAppToAsc.opcode 		= SCOP_DOWNLOAD;
	}

	yAppToAsc.appCallNum 	= GV_AppCallNum1;
	yAppToAsc.appPid 		= GV_AppPid;
	yAppToAsc.appRef 		= GV_AppRef-1;
	yAppToAsc.appPassword 	= GV_AppPassword;

	yAppToAsc.allParties 		= zMsgSpeak.allParties;
	yAppToAsc.interruptOption 	= zMsgSpeak.interruptOption;
	yAppToAsc.synchronous 		= zMsgSpeak.synchronous;
	yAppToAsc.dmId 				= GV_DynMgrId;



	if(strlen(zUrl) > 0)
	{
		int yIntUrlLen = strlen(zUrl);

		if (zUrl[yIntUrlLen -1 ] != '&')
		{
			sprintf(yAppToAsc.file, "%s&%s%s%s",
							zUrl,
							"destacodec=",
							GV_Audio_Codec,
							"&");
		}
		else
		{
			sprintf(yAppToAsc.file, "%s%s%s%s",
							zUrl,
							"destacodec=",
							GV_Audio_Codec,
							"&");
		}
	}


	sprintf(yAppToAsc.key, "%s", zMsgSpeak.key);

	fd = open(fifoName, O_WRONLY|O_NONBLOCK);
	if (fd < 0)
	{
		telVarLog(mod ,REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err, 
				"Failed to open request fifo (%s) for writing. [%d, %s], "
				"arcStreamingClient may not be running.",
				fifoName, errno, strerror(errno));

		return(-1);
	}

	rc = write(fd, &(yAppToAsc), sizeof(struct Msg_AppToAsc));
	if(rc == -1)
	{
		telVarLog(mod ,REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err, 
				"Failed to write to request fifo (%s). [%d, %s]",
				fifoName, errno, strerror(errno));

		close(fd);
		return(-1);
	}
	
	close(fd);

	totalStreamRequests++;

	return(0);

}/*END: static int notifyStreamingClient*/

int parseUrl(char *zUrl, int zDefaultPort, char *zProtocol, 
						char *zWebServer, char *zPort, char *zQuery, 
						char *zErrorMsg)
{
	char	*yPtr;
	char	yTmpProtocol[80];
	char	yTmpWebServer[80];
	char	yTmpPort[20];
	char	yToken[512];
	char	yLastToken[512];
	int		yIsDelimiter;
	int		yState;
	int		ySlashCount;
	int		yDone;
	char	yDelimiters[20];

	#define STATE_PROTOCOL		1
	#define STATE_SERVER		2
	#define STATE_PORT			3
	#define STATE_QUERY			4

	/*
	 * Clear buffers
	 */
	yTmpProtocol[0] = '\0';
	yTmpWebServer[0]= '\0';
	yTmpPort[0] 	= '\0';
	zProtocol[0] 	= '\0';
	zWebServer[0] 	= '\0';
	zPort[0] 		= '\0';
	zQuery[0] 		= '\0';
	zErrorMsg[0] 	= '\0';

	/*
	 * Check input parameters
	 */
	if(! zUrl || ! *zUrl)
	{
		sprintf(zErrorMsg, "Failed to parse URL, no URL string specified.");

		return(-1);
	}


	yDone = 0;
	yLastToken[0] = 0;
	yPtr = zUrl;

	sprintf(yDelimiters, "%s", ":/");

	if(strstr(zUrl, "://"))
	{
		yState = STATE_PROTOCOL;
	}
	else
	{
		yState = STATE_SERVER;
	}

	while(yDone != 1 && sGetNextToken(yPtr, yDelimiters, yToken, &yIsDelimiter))
	{
		if(yToken[0] == '\0')
		{
			if(yLastToken[0] != '\0')
			{
				switch(yState)
				{
				case STATE_PROTOCOL:
					sprintf(yTmpProtocol, "%s", yLastToken);
					break;

				case STATE_SERVER:
					sprintf(yTmpWebServer, "%s", yLastToken);
					break;

				case STATE_PORT:
					sprintf(yTmpPort, "%s", yLastToken);
					break;

				case STATE_QUERY:
					if(yLastToken[0] == '/')
					{
						sprintf(zQuery, "%s", yLastToken);
					}
					else
					{
						sprintf(zQuery, "/%s", yLastToken);
					}
					break;
				}
			}
			break;
		}

		if(yIsDelimiter)
		{
			if(yToken[0] != ':' && yToken[0] != '/')
			{
				sprintf(zErrorMsg, "Illegal delimiter (%c) in url (%s).",
						yToken[0], zUrl);

				return(-1);
			}

			switch(yState)
			{
			case STATE_PROTOCOL:
				if(*yToken == ':')
				{
					sprintf(yTmpProtocol, "%s", yLastToken);
					
					ySlashCount = 0;
				}
				else if(*yToken == '/')
				{
					ySlashCount++;

					if(ySlashCount >= 2)
					{
						yState = STATE_SERVER;
					}
				}
				break;

			case STATE_SERVER:
				if(*yToken == ':')
				{
					sprintf(yTmpWebServer, "%s", yLastToken);
					
					yState = STATE_PORT;
				}
				else if(*yToken == '/')
				{
					sprintf(yTmpWebServer, "%s", yLastToken);
					
					sprintf(yDelimiters, "%s", "\n");
					yState = STATE_QUERY;
				}
				break;

			case STATE_PORT:
				sprintf(yTmpPort, "%s", yLastToken);
				
				sprintf(yDelimiters, "%s", "\n");
				yState = STATE_QUERY;
				break;

			case STATE_QUERY:
				yDone = 1;
				break;
			}

		}
		else
		{
			sprintf(yLastToken, "%s", yToken);
		}

		yPtr += strlen(yToken);

	}


	sprintf(zProtocol,	"%s", yTmpProtocol);
	sprintf(zWebServer,	"%s", yTmpWebServer);

	if(yTmpPort[0] == '\0')
	{
		sprintf(zPort,	"%d", zDefaultPort);
	}
	else
	{
		sprintf(zPort,	"%s", yTmpPort);
	}


	return(0);

}

static void setStreamError(char *zErrMsg)
{
	GV_LastStreamError = 0;

	if(!zErrMsg)
	{
		return;
	}

	/*Mim. length is 5*/
	if(strlen(zErrMsg) < 5)
	{
		return;
	}

	if(strstr(zErrMsg, "ERR:") != NULL)
	{
		sscanf(zErrMsg, "ERR:%d", &GV_LastStreamError);
	}

}/*END: static void setStreamError*/



int TEL_Speak(int party, int interruptOption, int informat,
                        int outformat, void *data, int synchronous)
{
	static const char mod[]="TEL_Speak";
	char apiAndParms[4096] = "";
	char apiAndParmsFormat[]="%s(%s,%s,%s,%s,%s,%s)";
	int  rc;
	char digit_string[256] = "";
	double  *double_float;
	char    string_to_speak[256]= "";
	int     *num;
	struct MsgToApp response;
	char *typeAheadBuffer;
	int lStreaming = 0;
	char lUrl[1024]="";
 
	struct Msg_Speak msg;

	memset(&msg, 0, sizeof(struct Msg_Speak));
	msg.deleteFile = 0;	/*This param is used by TEL_SpeakTTS only.*/
	
	sprintf(apiAndParms,apiAndParmsFormat, mod, arc_val_tostr(party), 
		arc_val_tostr(interruptOption), arc_val_tostr(informat), arc_val_tostr(outformat), 
		data, arc_val_tostr(synchronous));

  	rc = apiInit(mod, TEL_SPEAK, apiAndParms, 1, 1, 0);
	if (rc != 0) HANDLE_RETURN(rc);

	if (informat == PHRASE_TAG)
	{
   		rc = tel_PlayTag(mod, party, data, interruptOption, synchronous, "");
 		HANDLE_RETURN(rc);
	}

//printf("RGDEBUG::%d::%s::data=%s\n", __LINE__, __FUNCTION__, data);fflush(stdout);
 
	if(strstr((char *)data, "stream://") != NULL)
	{
//printf("RGDEBUG::%d::%s::found streaming request\n", __LINE__, __FUNCTION__);fflush(stdout);
		lStreaming = 1;
		*lUrl = '\0';
		strncpy(lUrl, data, sizeof(lUrl)-1);
		*((char *)data + 10) = '\0';
//printf("RGDEBUG::%d::%s::found streaming request\n", __LINE__, __FUNCTION__);fflush(stdout);
	}

 	if(synchronous == PUT_QUEUE_ASYNC  && (lStreaming == 0))
	{
		synchronous = PUT_QUEUE;
	}

//printf("RGDEBUG::%d::%s::found streaming request\n", __LINE__, __FUNCTION__);fflush(stdout);
	if (!parametersAreGood(mod, party, &interruptOption, 
		informat, outformat, data, synchronous))
			HANDLE_RETURN(TEL_FAILURE);

//printf("RGDEBUG::%d::%s::found streaming request\n", __LINE__, __FUNCTION__);fflush(stdout);
	/* Check for typed ahead keys */

	/* Note: msg is only set up to allow logging of unexpected msgs */
	msg.opcode=DMOP_GETDTMF;
	msg.appPid 	= GV_AppPid;
	msg.appPassword = 0;
	if (party == FIRST_PARTY)
	{
		typeAheadBuffer = GV_TypeAheadBuffer1;
		msg.appCallNum=GV_AppCallNum1;
	}
	else
	{
		typeAheadBuffer = GV_TypeAheadBuffer2;
		msg.appCallNum=GV_AppCallNum2;
	}
//printf("RGDEBUG::%d::%s::found streaming request\n", __LINE__, __FUNCTION__);fflush(stdout);

	while(1)
	{
	rc = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
	if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
	if(rc == -2) break;
		
	if(response.opcode == DMOP_PLAYMEDIA)
	{
		response.opcode = DMOP_SPEAK;
	}

	rc = examineDynMgrResponse(mod, &msg, &response);	
	switch(rc)
	{
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum, 
							response.message);
			break;
		case DMOP_DISCONNECT:
			HANDLE_RETURN(disc(mod, response.appCallNum,
				GV_AppCallNum1, GV_AppCallNum2));
			break;
		default:
			/* Unexpected msg. Logged in examineDynMgr.. */
			break;
	} /* switch rc */
	}


//printf("RGDEBUG::%d::%s::found streaming request\n", __LINE__, __FUNCTION__);fflush(stdout);

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

		case FIRST_PARTY_PLAYBACK_CONTROL:
		case SECOND_PARTY_PLAYBACK_CONTROL:
			break;

		default:
			break;
	}


        msg.opcode = DMOP_SPEAK;

		if(interruptOption == NONINTERRUPT)
		{
			msg.interruptOption = 0;
			if((party == FIRST_PARTY) || (party == BOTH_PARTIES))
        		msg.appCallNum = GV_AppCallNum1;
			else
        		msg.appCallNum = GV_AppCallNum2;
		}
		else if( 	interruptOption == FIRST_PARTY_PLAYBACK_CONTROL ||  
					interruptOption == SECOND_PARTY_PLAYBACK_CONTROL)
		{
        	msg.appCallNum = GV_AppCallNum1;
			msg.interruptOption = interruptOption;
		}
		else
		{
			msg.interruptOption = 1;
			if(interruptOption == SECOND_PARTY_INTERRUPT)
        		msg.appCallNum = GV_AppCallNum2;
			else
        		msg.appCallNum = GV_AppCallNum1;
		}
        msg.appPid 	= GV_AppPid;
        msg.appRef 	= GV_AppRef++;
        msg.appPassword = GV_AppPassword;
        msg.list 	= 0;
        msg.allParties 	= (party == BOTH_PARTIES);
#if 0
		msg.synchronous = 1;
		switch(synchronous)
		{
			case SYNC:
				msg.synchronous = 1;
				break;

			case ASYNC:
				msg.synchronous = 0;
				break;

			case ASYNC_QUEUE:
				msg.synchronous = 2;
				break;

			default:
				break;
		}
#endif
		msg.synchronous = synchronous;
//printf("RGDEBUG::%d::%s::found streaming request\n", __LINE__, __FUNCTION__);flush(stdout);

 	switch(informat)
        {
        case    INTEGER :
        case    TICS    :
        case    PHRASENO:
                num = data;
                sprintf(string_to_speak, "%d", *num);
                break;
        case    PHRASE_FILE:

                data ? sprintf(string_to_speak, "%s", (char *)data) : 0;

                break;
        case    DOUBLE :
                double_float = data;
                /* Convert double to string with format "dddd.cc" */
                sprintf(string_to_speak, "%.2lf", *double_float);
                break;
        default :       /* All other informats are some type of string */
        		/* Check that string is less than 256 */
                data ? sprintf(string_to_speak, "%s", (char *)data) : 0;
		break;
        }                                                     

	if (informat == PHRASE_FILE)
	{
		msg.list=0;
		if((synchronous == PLAY_QUEUE_SYNC) ||
		   (synchronous == PLAY_QUEUE_ASYNC) ||
		   (synchronous == CLEAR_QUEUE_ALL))
		{
			msg.file[0] = '\0';
		}
		else
		{


			if(lStreaming == 1)
			{
				sprintf(msg.file, "%s", string_to_speak);

				if(synchronous != PUT_QUEUE_ASYNC)
				{
					rc = notifyStreamingClient(msg, lUrl);
					if(rc < 0)
						HANDLE_RETURN(rc);
				}
				else
					msg.file[0] = '\0';
			}
			else
			{
				rc = make_phrase_file(mod, informat, outformat, 
						string_to_speak, msg.file);
				if(rc != 0)
					HANDLE_RETURN(rc);
			}


		}
	}
	else if(informat == MILLISECONDS)
	{
		char yErrMsg[256];
		int milliseconds = atoi(string_to_speak);

		msg.list = 0;
		sprintf(msg.key, "%s", "\0");

		rc = make_silence_file(milliseconds, msg.appCallNum, msg.file, yErrMsg);
		if(rc < 0)
		{
			telVarLog(mod,REPORT_NORMAL,TEL_BASE,GV_Err,
			"%s failed (%s)", mod, yErrMsg);
			HANDLE_RETURN(rc);
		}
	}
	else
	{
		msg.list=1;
		rc=make_list(mod, informat, outformat, 
						string_to_speak, msg.file);
		if(rc != 0)
			HANDLE_RETURN(rc);
	}

    if ( ( GV_MrcpTTS_Enabled ) &&
         ( synchronous == CLEAR_QUEUE_ALL ) )
    {
            ARC_TTS_REQUEST_SINGLE_DM   ttsRequest;
            memset(&ttsRequest, 0, sizeof(ARC_TTS_REQUEST_SINGLE_DM));
            sprintf(ttsRequest.port,        "%d", msg.appCallNum);
            sprintf(ttsRequest.pid,         "%d", getpid());
            sprintf(ttsRequest.app,         "%s", "bogus");
            sprintf(ttsRequest.string,      "%s", "bogus");
            sprintf(ttsRequest.resultFifo,  "%s", "bogus");
            sprintf(ttsRequest.timeOut, "%d", 5);
            ttsRequest.async = synchronous;
            ttsRequest.speakMsgToDM.opcode = CLEAR_QUEUE_ALL;
            rc = sendRequestToTTSClient(&ttsRequest);
    }
//printf("RGDEBUG::%d::%s::found streaming request\n", __LINE__, __FUNCTION__);fflush(stdout);

	rc = tel_Speak(mod, interruptOption, &msg, 1);

//printf("RGDEBUG::%d::%s::found streaming request\n", __LINE__, __FUNCTION__);fflush(stdout);
	if((lStreaming == 1) && (rc == 0) && (synchronous == PUT_QUEUE_ASYNC))
	{
		sprintf(msg.file, "%s", string_to_speak);
		rc = notifyStreamingClient(msg, lUrl);
		if(rc < 0)
		{
			msg.synchronous = CLEAR_QUEUE_ALL;
			*msg.file = '\0';
			rc = tel_Speak(mod, interruptOption, &msg, 1);

			HANDLE_RETURN(-1);
		}
	}
	HANDLE_RETURN(rc);
}


int tel_Speak(char *mod, int interruptOption, void *m, 
				int updateLastPhraseSpoken)
{
	int rc;
	int requestSent;
	struct Msg_Speak msg;
	struct MsgToApp response;
	int lStreaming = 0;

	msg=*(struct Msg_Speak *)m;

	requestSent=0;

	if(strstr(msg.file, "stream://") != NULL)
		lStreaming = 1;

	if((lStreaming == 1) && (msg.synchronous != PUT_QUEUE_ASYNC))
	{
		requestSent=1;
	}

	while ( 1 )
	{
		if (!requestSent)
		{
			rc=readResponseFromDynMgr(mod,-1,&response,
							sizeof(response));
			if(rc == -1) return(TEL_FAILURE);
			if(rc == -2)
			{

				if((lStreaming == 0)  || (msg.synchronous == PUT_QUEUE_ASYNC))
				{
		     		rc = sendRequestToDynMgr(mod, &msg);
        			if (rc == -1) return(-1);
				}

				requestSent=1;
				if(updateLastPhraseSpoken == 1)
				{
					if (msg.appCallNum == GV_AppCallNum1)
						GV_LastSpeakMsg1 = msg;
					else
						GV_LastSpeakMsg2 = msg;
				}
				rc = readResponseFromDynMgr(mod,0,
						&response,sizeof(response));
				if(rc == -1) return(TEL_FAILURE);
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,0,
					&response,sizeof(response));
			if(rc == -1) return(TEL_FAILURE);
		}

		if(response.opcode == DMOP_PLAYMEDIA)
		{
			response.opcode = DMOP_SPEAK;
		}
	
		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_SPEAK:
		case DMOP_PLAYMEDIA:
			if(msg.synchronous == PUT_QUEUE_ASYNC)
			{
				/* response.message contains speakQueueId */
				memcpy(((struct Msg_Speak *)m)->key, response.message, 29);
				((struct Msg_Speak *)m)->key[29] = '\0';
			}

			if(response.returnCode == -1)
			{
				setStreamError(response.message);
			}

			return(response.returnCode);
			break;
		case DMOP_GETDTMF:
			if (response.appCallNum != msg.appCallNum)
                        {
                                /* Save data for the other leg of the call */
                                saveTypeAheadData(mod, response.appCallNum,
                                                        response.message);
                                break;
                        }
			switch(interruptOption)
			{
			case NONINTERRUPT: 
					break;
			case INTERRUPT:
			case FIRST_PARTY_INTERRUPT:
			case SECOND_PARTY_INTERRUPT:
				/* We've already weeded out if wrong callNum */
                                saveTypeAheadData(mod, response.appCallNum,
                                                        response.message);
				if (strcmp(response.message,"CLEAR")==0) 
				{
					break;
				}

				if (requestSent)
				{
                    msg.opcode=DMOP_STOPACTIVITY;
                    sendRequestToDynMgr(mod, &msg);
					usleep(50);
				}
                return(TEL_SUCCESS);
                break;

		case FIRST_PARTY_PLAYBACK_CONTROL:
		case SECOND_PARTY_PLAYBACK_CONTROL:
			saveTypeAheadData(mod, response.appCallNum, response.message);
			break;

			default:
				printf("Unknown interruptOption: %d\n", 
					interruptOption);
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

/*--------------------------------------------------------------------------
This routine will decide which type of data to speak based on informat
and outformat and construct the list of files to be passed back. 
That list is the "file" to be spoken that will be passed in the message
to the dynamic manager
-------------------------------------------------------------------------*/
static int make_list(char *mod, int informat, int outformat, char *string,  
			char *list)
{
        int ret;           
	static int count=1;       
	char reason[MAX_LENGTH];

	if( strlen(string) <= 0) return (0); /* Nothing to speak */

	/* Determine the name of the list file. */
	sprintf(list, "/tmp/list.%d.%d", GV_AppPid, count++);

	fp_list = fopen(list, "w");
	if (fp_list == NULL)
	{
		sprintf(reason, "Failed to open (%s) for write. errno=%d", 
			list, errno);
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, 
			arc_val_tostr(informat), arc_val_tostr(outformat), reason);
		return(-1);
	}
	
	fprintf(fp_list, "# %s in=%d, out=%d, s=(%s)\n", 
			list, informat, outformat, string);

	switch(outformat)
	{
         case DAY:
                ret = make_day_list(mod, informat, outformat, string, list);
                break;
        case  DATE_YTT:
        case  DATE_MMDD:
        case  DATE_MMDDYY:
        case  DATE_MMDDYYYY:
        case  DATE_DDMM:
        case  DATE_DDMMYY:
        case  DATE_DDMMYYYY:
                ret = make_date_list(mod, informat,outformat,string,list);
                break;
        case  TIME_STD:
        case  TIME_STD_MN:
        case  TIME_MIL:
        case  TIME_MIL_MN:
                ret = make_time_list(mod, informat, outformat,string, list);
                break;
        case  DOLLAR:
                ret = make_dollar_list(mod, informat, outformat, string, list);
                break;
        case  NUMBER:
                ret = make_number_list(mod, informat, outformat, string, list);
                break;
        case  DIGIT:
                ret = make_digit_list(mod, informat, outformat, string, list);
                break;
        case  ALPHANUM:
                ret = make_alphanum_list(mod, informat, outformat, string,list);
                break;
        default:
       		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
       			"Invalid value for outformat: %d.", outformat);
       		fclose(fp_list);
		// unlink(list);
                return (-1);
                break;
        }
       	fclose(fp_list);
        return (ret);
} 

/*---------------------------------------------------------------------------
This function verifies that all parameters are valid and sets any global
variables necessary for execution. It returns 1 (yes) if all parmeters are
good, 0 (no) if not.
---------------------------------------------------------------------------*/
static int parametersAreGood(char *mod, int party, int *interruptOption, 
			int informat, int outformat, void *data, 
			int synchronous)
{
	int errors=0;
 	char partyValues[]="FIRST_PARTY, SECOND_PARTY, BOTH_PARTIES";     

	char interruptOptionValues[]="INTERRUPT, NONINTERRUPT, FIRST_PARTY_INTERRUPT, SECOND_PARTY_INTERRUPT,FIRST_PARTY_PLAYBACK_CONTROL, SECOND_PARTY_PLAYBACK_CONTROL ";

 	char informatValues[]="STRING, MILITARY, STANDARD, TICS, MM_DD_YY, DD_MM_YY, YY_MM_DD, DOUBLE, INTEGER, PHONE, PHRASE_FILE, MILLISECONDS";     
	char inoutFormat[]="Invalid input & output format combination: %d,%d."; 
 	char synchronousValues[]="SYNC, ASYNC, ASYNC_QUEUE, PUT_QUEUE, PLAY_QUEUE_SYNC, PLAY_QUEUE_ASYNC";
	int inoutFormatError=0;
	switch(party)
        {
       	case FIRST_PARTY:
		break;
	case SECOND_PARTY:
		if (!GV_Connected2)
		{
			errors++;
       			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"party=SECOND_PARTY, but second party is not connected.");
		}
		break;
	case BOTH_PARTIES:
		if (!GV_Connected2)
		{
			errors++;
       			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"party=BOTH_PARTIES, but second party is not connected.");
		}
		break;
	case SECOND_PARTY_NO_CALLER:
		/* Do we want to support this option?? */
		break;

	default:
		errors++;
       		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
       			"Invalid value for party: %d. Valid values are %s.",
			party, partyValues);
 		break;
	}
	
	switch(*interruptOption)
        {
       	case INTERRUPT:
		switch(party)
		{
		case FIRST_PARTY:
               		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
				"Invalid interrupt_option: INTERRUPT. "
                        	"Using FIRST_PARTY_INTERRUPT.");
                        *interruptOption = FIRST_PARTY_INTERRUPT;
			break;

		case SECOND_PARTY:
		case SECOND_PARTY_NO_CALLER: 
               		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
				"Invalid interrupt_option: INTERRUPT. "
                        	"Using SECOND_PARTY_INTERRUPT.");
                        *interruptOption = SECOND_PARTY_INTERRUPT;
			break;
		
		case BOTH_PARTIES:	
               		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
				"Invalid interrupt_option: INTERRUPT. "
                        	"Using NONINTERRUPT.");
                        *interruptOption = NONINTERRUPT;
			break;

		default:
			break;
		}
		break;

        case NONINTERRUPT:
		break;

	case FIRST_PARTY_INTERRUPT:
                if((party == SECOND_PARTY) ||
                   (party == SECOND_PARTY_NO_CALLER))
                {
               		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
                        "Invalid interrupt_option: FIRST_PARTY_INTERRUPT "
                     	"when party is SECOND_PARTY or SECOND_PARTY_NO_CALLER. "
                        "Using SECOND_PARTY_INTERRUPT.");
                        *interruptOption = SECOND_PARTY_INTERRUPT;
                }
		break;

	case SECOND_PARTY_INTERRUPT:
                if(party == FIRST_PARTY)
                {
               		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
                        "Invalid interrupt_option: SECOND_PARTY_INTERRUPT "
                     	"when party is FIRST_PARTY. "
                        "Using FIRST_PARTY_INTERRUPT.");
                        *interruptOption = FIRST_PARTY_INTERRUPT;
                }
		break;

		case FIRST_PARTY_PLAYBACK_CONTROL:
		case SECOND_PARTY_PLAYBACK_CONTROL:
			break;

	default:
		errors++;
               	telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid value for interruptOption: %d. "
			"Valid values are %s", 
			*interruptOption, interruptOptionValues);
		break;
	} 

	switch(informat)
	{
	case MILLISECONDS:
		switch(outformat)
		{
			case SILENCE:
				break;
			default:
				inoutFormatError = 1;
				break;
		}
		break;
 	case STRING:
		switch(outformat)
		{
		case TIME_STD:
		case TIME_STD_MN:
		case TIME_MIL:
		case DAY:
		case DATE_YTT:
		case DATE_MMDD:
		case DATE_MMDDYY:
		case DATE_MMDDYYYY:
		case DATE_DDMM:
		case DATE_DDMMYY:
		case DATE_DDMMYYYY:
		case DOLLAR:	
		case NUMBER:
		case DIGIT:
		case ALPHANUM:
		case PHRASE: /* Not sure if this will play properly */	
			break;
		default:	
			inoutFormatError=1;
			break;
		}
		break;
	case MILITARY:
	case STANDARD: 
		switch(outformat)
		{
		case TIME_STD:
		case TIME_STD_MN:
		case TIME_MIL:
			break;
		default:	
			inoutFormatError=1; 
			break;
		}
		break;
	case TICS:
		switch(outformat)
		{
		case TIME_STD:
		case TIME_STD_MN:
		case TIME_MIL:
		case DAY:
		case DATE_YTT:
		case DATE_MMDD:
		case DATE_MMDDYY:
		case DATE_MMDDYYYY:
		case DATE_DDMM:
		case DATE_DDMMYY:
		case DATE_DDMMYYYY:
			break;
		default:	
			inoutFormatError=1; 
			break;
		}
		break;
	case MM_DD_YY: 
	case DD_MM_YY: 
	case YY_MM_DD:
		switch(outformat)
		{
		case DAY:
		case DATE_YTT:
		case DATE_MMDD:
		case DATE_MMDDYY:
		case DATE_MMDDYYYY:
		case DATE_DDMM:
		case DATE_DDMMYY:
		case DATE_DDMMYYYY:
			break;
		default:	
			inoutFormatError=1; 
			break;
		}
		break;
	case DOUBLE:
		switch(outformat)
		{
		case DOLLAR: 
			break;
		default:
			inoutFormatError=1; 
			break;
		}
		break;
	case INTEGER:
		switch(outformat)
		{
		case NUMBER: 
		case DIGIT: 
			break;
		default:	
			inoutFormatError=1; 
			break;
		}
		break;
	case PHONE: 
		switch(outformat)
		{
		case DIGIT: 
			break;
		default: 	
			inoutFormatError=1; 
			break;
		}
		break;
	case PHRASE_FILE:     
		switch(outformat)
		{
		case PHRASE: 
			break;
		default:	
			inoutFormatError=1; 
			break;
		}
		break;
	default:
		errors++;
       		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
       			"Invalid value for informat: %d. "
			"Valid values are %s.", informat, informatValues);
		break;
	}	

	if (inoutFormatError)
	{
		errors++;
		sprintf(Msg, inoutFormat, informat, outformat);
		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, Msg);
	}

	switch(synchronous)
        {
       	case SYNC:
       	case ASYNC:
       	case ASYNC_QUEUE:
       	case PUT_QUEUE:
       	case PUT_QUEUE_ASYNC:
       	case PLAY_QUEUE_SYNC:
       	case PLAY_QUEUE_ASYNC:
		break;
	default:
		errors++;
       		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
       			"Invalid value for synchronous: %d. "
			"Valid values are %s.", synchronous, synchronousValues);
		break;
	} 

	return(errors == 0);
} 

/*----------------------------------------------------------------------------
This routine will speak a number based on the string passed to it.
Note:   an integer input could be 10 digits and still be <  2^32.
        There is not a system phrase for billion.
----------------------------------------------------------------------------*/
static int make_number_list(char *mod,int in,int out,char *string,char *list)
{
int     i, j;
int     number_to_speak;
int     ret_code;
char    tmp_string[256];
char    reason[128];
        
	switch(in)
        {
        case  STRING:
                j = 0;
                for(i = 0; i < (int)strlen(string); i++)
                {
                        if(!isdigit(string[i]))
                        {
                                if(string[i] == ',')
                                {
                                        continue; /* Skip the comma. */
                                }
                                else
                                {
				/* ?? Incorrect */
                                        telVarLog(mod,REPORT_NORMAL,
                                       		TEL_ENONNUMERICSTR, GV_Err,
						"speak_number", string);
                                        return (-1);
                                }
                        }
                        /* Copy it. */
                        tmp_string[j++] = string[i];
                }
                tmp_string[j] = '\0';
                strcpy(string, tmp_string);
                if((int)strlen(string) > 9 )
                {
					sprintf(reason, "number is greater than 9 digits");
					telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
		       			TEL_CANT_SPEAK_MSG, string, 
						arc_val_tostr(in), arc_val_tostr(out), reason);
					return (-1);
                }
                if((int)strlen(string) == 0 )
                {
					sprintf(reason, "unable to speak empty string");
					telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
		       			TEL_CANT_SPEAK_MSG, string, 
						arc_val_tostr(in), arc_val_tostr(out), reason);
					return (-1);
                }
                ret_code = SL_append_number(mod, in, out, atol(string));
                if(ret_code == -1) return (-1);
                break;
        case INTEGER:
                number_to_speak=atoi(string);
                if(number_to_speak > 999999999)
                {
			sprintf(reason, "number is greater than 999,999,999");
       			telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, number_to_speak, 
			arc_val_tostr(in), arc_val_tostr(out), reason);
                        return (-1);
                }
                if((int)strlen(string) == 0 )
                {
                    sprintf(reason, "unable to speak empty string");
                    telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
                        TEL_CANT_SPEAK_MSG, string,
                        arc_val_tostr(in), arc_val_tostr(out), reason);
                    return (-1);
                }
                ret_code = SL_append_number(mod, in, out, number_to_speak);
                if(ret_code == -1) return (-1);
                break;
        } /* end switch */

        return(0);
}

/*-------------------------------------------------------------------------
This routine constructs the list file for a speaking digits.
--------------------------------------------------------------------------*/
static int make_digit_list(char *mod, int in, int out, char *string, char *list)
{
        int     i;
        int     ret;
        char    stringEdited[MAX_LENGTH];
		
       	memset(stringEdited,'\0', sizeof(stringEdited));

        switch(in)
        {
        case INTEGER:
        case SOCSEC:
        case PHONE:
        case STRING:
		strcpy(stringEdited, string);
		removeNonDigits(stringEdited);
		break;
	default:
		break;
        } /* switch */
	
	for(i=0; stringEdited[i] != '\0'; i++) 
	{
		ret=append_system_phrase(mod,NUMBER_BASE+(stringEdited[i]-'0'));
		if (ret != 0) return(ret);
        } /* for */

        return (0);
}

/*------------------------------------------------------------------------
This routine produces an output string from an input string by only copying
over digits. All other characters are discarded.
-------------------------------------------------------------------------*/
void removeNonDigits(char *input)
{
	char output[MAX_LENGTH];

        register int i, j;
        for(i=0, j=0; input[i] != '\0'; i++)
        {
                if(isdigit(input[i]))
                {
                        output[j] = input[i];
                        j++;
                }
        }
        output[j] = '\0';
	strcpy(input, output);
        return;
}

/*----------------------------------------------------------------------------
Appends a line containing the appropriate system phrase pathname to
a list file, using the global file pointer.  
Note: This routine should check for the existence of the file.
----------------------------------------------------------------------------*/
int append_system_phrase(char *mod, int phrase_no)
{
	char file[512];

   	sprintf(file, "%s/%04d.%s", GV_SystemPhraseDir, phrase_no, 
		GV_FirstSystemPhraseExt);
   	
	fprintf(fp_list, "%s\n", file);
	return(0);
}

/*----------------------------------------------------------------------------
Create the pathname for the phrase to be spoken, adjusting it for the 
value of the default directory for application phrases, if necessary.
----------------------------------------------------------------------------*/
static int make_phrase_file(char *mod,int in, int out,
				char *infile, char *outfile)
{
	char reason[MAX_LENGTH];

        if(out != PHRASE)
        {
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, infile, 
			arc_val_tostr(in), arc_val_tostr(out), "outformat must be PHRASE");
                return (-1);
        }
	
	adjust_file_for_APP_PHRASE_DIR(infile, outfile);

	if(access(outfile, F_OK) != 0)
	{
		sprintf(reason,"file (%s) does not exist.",outfile);
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, outfile, 
			arc_val_tostr(in), arc_val_tostr(out), reason);
		return(TEL_SOURCE_NONEXISTENT);
  	}
        
	return(0);
}

/*----------------------------------------------------------------------------
This routine will adjust the file name according to GlobalString variable
APP_PHRASE_DIR.
-----------------------------------------------------------------------------*/
static int adjust_file_for_APP_PHRASE_DIR( char *file, char *adjusted_file)
{
    char yCurrentDir[256];
    char *cwd;


    if((cwd = getcwd(NULL, 255)) == NULL)
    {
        return (-1);
    }

    sprintf(yCurrentDir, "%s", cwd);
    free(cwd);

    if(file[0] != '/')   /*Does not start with slash '/' */
    {
        if(strlen(GV_AppPhraseDir) != 0)
        {
            sprintf(adjusted_file, "%s/%s", GV_AppPhraseDir, file);

#if 0
            if(access(adjusted_file, F_OK) != 0)
            {
                sprintf(adjusted_file, "%s/%s", yCurrentDir, file);
            }
#endif
        }
        else
        {
            sprintf(adjusted_file, "%s/%s", yCurrentDir, file);
        }
    }
    else
    {
        sprintf(adjusted_file, "%s", file);
    }


    return(0);
}


/*-------------------------------------------------------------------------
This routine makes the list to speak the day (Sunday - Saturday).
--------------------------------------------------------------------------*/
static int make_day_list(char *mod, int in, int out, char *string, char *list)
{
        int     ret;
        int     month;
        int     day;
        int     year;
        int     year_for_msg;
        char    ccyymmdd[20];
        char    filename[256];
        char    msg[64];
        int     d_of_w;
        int     phr_base;

        ret = parse_date(string, in, ccyymmdd, &year, &month, &day);
        switch(ret)
        {
        case -1:
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, 
			arc_val_tostr(in), arc_val_tostr(out), "date is invalid");
                return(-1);
                break;                                                         
       case -2:
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, 
			arc_val_tostr(in), arc_val_tostr(out), "input format is invalid");
                return(-1);
                break;
        }

        year_for_msg = year;

        /*
        Note: For purposes of speaking the day, you can adjust the years
                from 1900 to 2099 so that they will fall within the
                range 1970 to 2038, while maintaining the same day
                of the week for the given month and day. You can use
                the unix "cal" function to prove this.  gpw 8/14/98
         */

        if (year == 1900)
                year = 1973;
        else
        if ( (year >= 1901) && (year <= 1925) )
                year = year + 112;
        else
        if ( (year >= 1926) && (year <= 1970) )
                year = year + 56;
        else
        if ( (year >= 2038) && (year <= 2091) )
                year = year - 56;
        else
        if ( (year >= 2092) && (year <= 2099) )
               year = year - 112;
        else
        if ( (year >= 1970) && (year <= 2038) )
                ; /* Do nothing; this range is always good */
        else
        {
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, arc_val_tostr(in), arc_val_tostr(out), 
			"year must be between 1900 and 2099");
                return(-1);
        }

        d_of_w=day_of_week(year, month, day);
        if (d_of_w >= 7 || d_of_w < 0)
        {
		sprintf(msg,
		"day of week can't be determined for year/month/day=%d/%d/%d",
			year_for_msg, month, day);
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, arc_val_tostr(in), arc_val_tostr(out), msg);
                return (-1);
        }

        ret = append_system_phrase(mod, WEEK_PHRASE + d_of_w);
        if (ret != 0) return(-1);

        return (0);
}

	
/*---------------------------------------------------------------------------
This routine will speak the time according to requested format.
---------------------------------------------------------------------------*/
static int make_time_list(char *mod, int informat, int outformat, 
						char *string, char *list)
{
        long    seconds = 0L;
        int     ret_code = 0;

        switch(informat)
        {
        case MILITARY:
        case STANDARD:
        case STRING  :
                seconds = convert_time_to_seconds(mod, string);
                switch(seconds)
                {
                case -1:
       			telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, arc_val_tostr(informat), 
			arc_val_tostr(outformat), "am/pm hour combination is invalid");
                        return(-1);
                        break;
                case -2:
       			telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, arc_val_tostr(informat), 
			arc_val_tostr(outformat), "hour is invalid");
                        return(-1);
                        break;
                case -3:
       			telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, arc_val_tostr(informat), 
			arc_val_tostr(outformat), "minute is invalid");
                        return(-1);
                        break;
                case -4:
       			telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, arc_val_tostr(informat), 
			arc_val_tostr(outformat), "conversion of date to seconds failed");
                        return(-1);
                        break;
                default:
                        break;
                }
                break;
        case TICS  :
                seconds = atoi(string);
                break;
        }

        ret_code = SL_append_time(mod, outformat, seconds);
        return(ret_code);
}


/*---------------------------------------------------------------------------
This routine will a string containing a time to seconds since 1/1/1970.
Sting should be in MILITARY (e.g. "1200"), STANDARD ( e.g., "12:01), or
STRING (e.g., "12:01 am")
Returns:  0 - success
	 -1 - problem with am/pm indicator
	 -2 - invalid hour
	 -3 - invalid minute
	 -4 - Couldn't convert to seconds.
---------------------------------------------------------------------------*/
static int convert_time_to_seconds(mod, time_str)
char 	*mod;
char	*time_str;
{

	char hour_str[]="hh";
	char minute_str[]="mm";
	char temp[50];
	int am_found=0, pm_found=0;
	int hour, minute;
	int i, j;
	struct tm t;	
	time_t seconds;
	int	ctr =0;

	  /* Remove blanks and tabs from time string; convert to upper */
	j = 0;
	for (i=0; i < (int)(strlen(time_str)); i++)
	{
		if ( (time_str[i] != ' ') && (time_str[i] != '\t') )
		{
			temp[j++] = toupper(time_str[i]);	
		}
	}
	temp[j]='\0';
       
	if ( strstr(temp, "AM") ) am_found = 1;
	if ( strstr(temp, "PM") ) pm_found = 1;

	if (strchr(temp, ':') != 0)   /* format is hh:mm */
	{
		if (temp[2] == ':')        /* hh:mm */
		{
			hour_str[0] = temp[0];
			hour_str[1] = temp[1];
			minute_str[0] = temp[3];
			minute_str[1] = temp[4];
		}
		if(temp[1] == ':')        /* h:mm */
		{
			hour_str[0] = '0';
			hour_str[1] = temp[0];
			minute_str[0] = temp[2];
			minute_str[1] = temp[3];
		}
	}
	else                            /* format is hhmm */
	{
		ctr=0;
		for(i=0; i <= 4; i++)
			{
			if(isdigit(temp[i]))
				ctr++;
			}
		if(ctr == 3)
			{
			hour_str[0] = '0';
			hour_str[1] = temp[0];
			minute_str[0] = temp[1];
			minute_str[1] = temp[2];
			}
		else
			{
			hour_str[0] = temp[0];
			hour_str[1] = temp[1];
			minute_str[0] = temp[2];
			minute_str[1] = temp[3];
			}
	}
	hour=atoi(hour_str);
	minute=atoi(minute_str);

	if ( (am_found || pm_found) && (hour > 12) ) 	return(-1);
	if ( hour < 0 || hour > 23 )               	return(-2);
	if ( minute < 0 || minute > 59 )        	return(-3);

	/* Adjust hour for am/pm indicator */
	if ( am_found && (hour == 12) ) hour = 0;
	if ( pm_found && (hour <  12) ) hour = hour + 12;

        t.tm_year    = 96;		/* 1996 */
        t.tm_mon     = 3;		/* April (4-1) */
        t.tm_mday    = 12;		/* 12 th */
        t.tm_hour    = hour;
        t.tm_min     = minute;
        t.tm_sec          = 0;
        t.tm_isdst   = -1;
        if ( (seconds = mktime(&t)) == -1) return (-4);
	return(seconds);
}
/*-----------------------------------------------------------------------------
This routine takes a string in any of the valid date input formats and
gives back a string in yyyymmdd format and integers for year, month, day.
Return codes:
         0 - success
        -1 - invalid input - string too short, month, day no good, etc.
        -2 - invalid input format
-----------------------------------------------------------------------------*/
int parse_date(string, informat, ccyymmdd_ptr, year_ptr, month_ptr, day_ptr)
char    *string;
int     informat;
char    *ccyymmdd_ptr;
int     *year_ptr;
int     *month_ptr;
int     *day_ptr;
{
        char    cent_str[]="cc";
        char    year_str[]="yy";
        char    month_str[]="mm";
        char    day_str[]="dd";
        char    ccyymmdd[]="ccyymmdd";
        long    tics;
       	struct  tm st;
        int     month, day, year,century;
        int     i, j;
        char    string1[100];
        if ( informat != TICS)
        {
                /* Determine century for when input format is YY, not YYYY */
                time(&tics);
		st = *(struct tm *)localtime(&tics);
		strftime(ccyymmdd, 20, "%Y%m%d", &st);
                cent_str[0]     = ccyymmdd[0];
                cent_str[1]     = ccyymmdd[1];
                century         = atoi(cent_str)*100; /* either 1900 or 2000 */

                /* Remove all non-numeric characters, including spaces */
                j = 0;
                for(i = 0; i < (int)strlen(string); i++)
                {
                        if(isdigit(string[i]))
                        {
                                string1[j] = string[i];
                                j++;
                        }
                }
                string1[j] = '\0';
        }

        switch (informat)
        {
        case TICS:
                tics=atol(string);
		st = *(struct tm *)localtime(&tics);
		strftime(ccyymmdd, 20, "%Y%m%d", &st);
                cent_str[0]     = ccyymmdd[0];
                cent_str[1]     = ccyymmdd[1];
                year_str[0]     = ccyymmdd[2];
                year_str[1]     = ccyymmdd[3];
                month_str[0]    = ccyymmdd[4];
                month_str[1]    = ccyymmdd[5];
                day_str[0]      = ccyymmdd[6];
                day_str[1]      = ccyymmdd[7];
                year            = atoi(year_str) + atoi(cent_str) * 100;
                month           = atoi(month_str);
                day             = atoi(day_str);
                break;
        case STRING:
        case MM_DD_YY:
                switch(strlen(string1))
                {
                case 6:         /* mmddyy */
                        month_str[0]    = string1[0];
                        month_str[1]    = string1[1];
                        day_str[0]      = string1[2];
                        day_str[1]      = string1[3];
                        year_str[0]     = string1[4];
                        year_str[1]     = string1[5];
                        year            = atoi(year_str) + century;
                        month           = atoi(month_str);
                        day             = atoi(day_str);
                        break;
                case 8: /* mmddyyyy */
                        month_str[0]    = string1[0];
                        month_str[1]    = string1[1];
                        day_str[0]      = string1[2];
                        day_str[1]      = string1[3];
                        cent_str[0]     = string1[4];
                        cent_str[1]     = string1[5];
                        year_str[0]     = string1[6];
                        year_str[1]     = string1[7];
                        year            = atoi(year_str) + atoi(cent_str)*100;
                        month           = atoi(month_str);
                        day             = atoi(day_str);
                        break;
                default:
                        return(-1);
                        break;
                }
                break;
        case DD_MM_YY:
                switch(strlen(string1))
                {
                case 6:         /* ddmmyy */
                        day_str[0]      = string1[0];
                        day_str[1]      = string1[1];
                        month_str[0]    = string1[2];
                        month_str[1]    = string1[3];
                        year_str[0]     = string1[4];
                        year_str[1]     = string1[5];
                        year            = atoi(year_str) + century;
                        month           = atoi(month_str);
                        day             = atoi(day_str);
                        break;
                case 8:         /* ddmmyyyy */
                        day_str[0]      = string1[0];
                        day_str[1]      = string1[1];
                        month_str[0]    = string1[2];
                        month_str[1]    = string1[3];
                        cent_str[0]     = string1[4];
                        cent_str[1]     = string1[5];
                        year_str[0]     = string1[6];
                        year_str[1]     = string1[7];
                        year            = atoi(year_str)+atoi(cent_str) * 100;
                        month           = atoi(month_str);
                        day             = atoi(day_str);
                        break;
                default:
                        return(-1);
                        break;
                }
                break;
        case YY_MM_DD:
		switch(strlen(string1))
                {
                case 6:         /* yymmdd */
                        year_str[0]     = string1[0];
                        year_str[1]     = string1[1];
                        month_str[0]    = string1[2];
                        month_str[1]    = string1[3];
                        day_str[0]      = string1[4];
                        day_str[1]      = string1[5];
                        year            = atoi(year_str) + century;
                        month           = atoi(month_str);
                        day             = atoi(day_str);
                        break;
                case 8: /* yyyymmdd */
                        cent_str[0]     = string1[0];
                        cent_str[1]     = string1[1];
                        year_str[0]     = string1[2];
                        year_str[1]     = string1[3];
                        month_str[0]    = string1[4];
                        month_str[1]    = string1[5];
                        day_str[0]      = string1[6];
                        day_str[1]      = string1[7];
                        year            = atoi(year_str)+ atoi(cent_str) * 100;
                        month           = atoi(month_str);
                        day             = atoi(day_str);
                        break;
                default:
                        return(-1);
                       break;
                }
                break;
        default:
                /* Invalid input format */
                return(-2);
                break;

        } /* end of switch */

        /* Set return values */
        *month_ptr = month;
        strcpy(ccyymmdd_ptr,ccyymmdd);
        *day_ptr = day;
        *year_ptr = year;

        if ((month < 1) || (month > 12) || (day < 1) || (day > 31)) return(-1);

        /* Check for number of days in each month. */
        switch(month)
        {
        case 1:     /* 31 days. */
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
                if (day > 31) return(-1);
                break;
        case 4:     /* 30 days. */
        case 6:
        case 9:
        case 11:
                if (day > 30) return(-1);
                break;
        case 2:     /* February. */
                if( (year % 4) == 0)
                {
                        if(day > 29) return(-1);
                }
                else
                {
                        if(day > 28) return(-1);
                }
                break;
        default:
                return(-1);
                break;
        } /* switch on month */

      return 0;
}

/*----------------------------------------------------------------------------
Determine the day of the week and return it.
----------------------------------------------------------------------------*/
int day_of_week(year, month, day)
int year, month, day;
{
        struct tm st;

        st.tm_year = year - 1900;
        st.tm_mon  = month - 1;
        st.tm_mday = day;
        st.tm_hour = 12;
        st.tm_min  = 0;
        st.tm_sec  = 0;
        st.tm_isdst= -1;

        /* Return the day of the week or -1 on failure */
        if (mktime(&st) == -1)
                return(-1);
        else
                return(st.tm_wday);
}

/*---------------------------------------------------------------------------
Makes the list file for speaking all of the dates.
--------------------------------------------------------------------------*/
static make_date_list(mod, informat,outformat,string,list)
char	*mod;		/* the calling module */
int	informat;
int	outformat;
char	*string;
char 	*list;
{
	int	month;
	int	day;
	int	year;
	int	ret, ret_code;
	char	ccyymmdd[20];
	char	reason[80];
	time_t	today, tomorrow, yesterday;
	struct	tm *p;
	int m, d, y;
	int seconds_in_a_day = 86400; /* 24 * 60 * 60 */ 
	
	ret = parse_date(string, informat, ccyymmdd, &year, &month, &day);
	switch(ret)
	{
	case -1:
		sprintf(reason,"it is an invalid date");
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, 
			arc_val_tostr(informat), arc_val_tostr(outformat), reason);
		return(-1);
		break;
	case -2:
		sprintf(reason,"informat is invalid");
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, 
			arc_val_tostr(informat), arc_val_tostr(outformat), reason);
		return(-1);
		break;
	}
	
	switch (outformat)
	{
  	case DATE_YTT:  /* Yesterday, Today, Tommorrow. */
		time(&today);

		p = localtime(&today);
		m = p->tm_mon + 1;
		d = p->tm_mday;
		y = p->tm_year + 1900;
		if ( month == m && day == d && year == y)
		{
			ret_code = append_system_phrase(mod, TODAY_PHRASE);
			break;	
		}

		/* See if we should speak "Tomorrow" */	 
		tomorrow = today + seconds_in_a_day; 
		p = localtime(&tomorrow);
		m = p->tm_mon + 1;
		d = p->tm_mday;
		y = p->tm_year + 1900;
		if ( month == m && day == d && year == y)
		{
			ret_code = append_system_phrase(mod, TOMORROW_PHRASE);
			break;
		}
	
		/* See if we should speak yesterday */	
		yesterday = today - seconds_in_a_day;
		p = localtime(&yesterday);
		m = p->tm_mon + 1;
		d = p->tm_mday;
		y = p->tm_year + 1900;
		if ( month == m && day == d && year == y)
		{
			ret_code = append_system_phrase(mod, YESTERDAY_PHRASE);
			break;
		}

		/* Neither today, tomorrow, nor yesterday; so just speak date */
 		ret_code = append_month(mod, month);
		if (ret_code != 0 ) break;

               	ret_code = append_day_of_month(mod, day);
		if (ret_code != 0) break;

    		break;
  	case DATE_MMDD	:
 		ret_code = append_month(mod, month);
		if (ret_code != 0 ) break;

                ret_code = append_day_of_month(mod,day);
		if (ret_code != 0 ) break;
    		
		break;
  	case DATE_MMDDYY	:
                ret_code = append_month(mod, month);
		if (ret_code != 0 ) break;
	
                ret_code =  append_day_of_month(mod,day);
		if (ret_code != 0 ) break;
                
		ret_code = append_year(mod, year, 2);
		if (ret_code != 0 ) break;
               
    		break;
  	case DATE_MMDDYYYY	:
                ret_code = append_month(mod, month);
		if (ret_code != 0 ) break;
                
		ret_code = append_day_of_month(mod, day);
		if (ret_code != 0) break;

                ret_code = append_year(mod, year, 4);
		if (ret_code != 0 ) break;

    		break;
  	case DATE_DDMM	:
                ret_code = append_day_of_month(mod, day);
		if (ret_code != 0) break;

                ret_code = append_month(mod, month);
		if (ret_code != 0 ) break;

    		break;
  	case DATE_DDMMYY	:
                ret_code = append_day_of_month(mod, day);
		if (ret_code != 0) break;

                ret_code = append_month(mod, month);
		if (ret_code != 0 ) break;

		ret_code = append_year(mod, year, 2);
 		if (ret_code != 0 ) break;

    		break;
  	case DATE_DDMMYYYY:
		ret_code = append_day_of_month(mod, day);
		if (ret_code != 0) break;
               
		ret_code = append_month(mod, month);
		if (ret_code != 0 ) break;
                
		ret_code = append_year(mod, year, 4);
		if (ret_code != 0 ) break;

    		break;
	} /* switch */

	if (ret_code == -1)
	{
		sprintf(reason,"of unspecified error parsing the date");
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, 
			arc_val_tostr(informat), arc_val_tostr(outformat), reason);
	}

        return(ret_code);
}

/*---------------------------------------------------------------------------
append_day_of_month(): speak day of month 
---------------------------------------------------------------------------*/
static	int append_day_of_month(mod, day)
char 	*mod;
int	day;
{
	int ret;

	ret = append_system_phrase(mod, DAYOFMONTH_BASE+day);
	return (ret);
} 

/*-----------------------------------------------------------------------------
Append the month to the list file. 
Note:	This function assumes that the month passed has already been validated 
	and does no checking or logging.
-----------------------------------------------------------------------------*/
static	int append_month(mod, month)
char 	*mod;
int	month;
{
	int rc;

	rc = append_system_phrase(mod, MONTH_BASE+month);
	return(rc);
} 

/*---------------------------------------------------------------------------
Append the year to the list, up to 2099.
For 2 digit year:
	if 2000 speak "two thousand"
	if last two digits are 01-09, "oh one" to "oh nine"
	otherwise just speak the year, e.g., "98" is "ninety-eight"
----------------------------------------------------------------------------*/
static	int append_year(mod, year, num_of_digits)
char 	*mod;
int	year;
int	num_of_digits;
{
	int	century, millenium, two_digit_year;
	int	phr_num;
	int	ret;

	switch(num_of_digits)
	{
	case 2:
		two_digit_year = year % 100;

		if ( year == 2000) goto year_2000;
	
		if (two_digit_year < 10)
			phr_num=MINUTE_BASE + two_digit_year;
		else
			phr_num=NUMBER_BASE + two_digit_year;
		
		/* Speak 2 digit year as either oh-1, oh-2, etc. or the # */
		ret = append_system_phrase(mod, phr_num);
		return (ret);
		break;
	case 4:
/* Note: year_2000 is a label needed for a goto! Can you BE-lieve it? - gpw */
year_2000:
		if( (year >= 2000) && (year < 2100) )
		{
			millenium = year/1000;
			two_digit_year = year - (millenium*1000); 	
			phr_num=NUMBER_BASE + millenium;
			/* Speak the millenium, e.g. 2 */
			ret = append_system_phrase(mod, phr_num);
			if (ret != 0) return(ret);
			/* Speak "thousand" */
			ret = append_system_phrase(mod, THOUSAND_BASE);
			if (ret != 0) return(ret);
			if(two_digit_year == 0) return(0);
		}
		else
		{
			century = year / 100;	
			two_digit_year = year - (century*100); 
			
			/* speak century */
			ret = append_system_phrase(mod, NUMBER_BASE + century);
			if (ret != 0) return(ret);
		} 
		/* speak 2 digit year, e.g. "98" */
		ret = append_system_phrase(mod, NUMBER_BASE + two_digit_year);
		return(ret);
		break;
	default:
		sprintf(Msg, "Invalid length (%d) passed for year: %d.", 
			num_of_digits, year);
       		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, Msg);
		return (-1);
		break;

	} /* switch */
	
} /* speak year */


/*-------------------------------------------------------------------------
This routine will speak string of alphanumeric data w/ rising inflection
on the first char and falling on the last.
There must be a set of phrases in the speech file for the numbers and letters
with flat, rising, & falling inflections. They must start at ALPHANUM_BASE.
The phrases must be in the following order:
	A-Z,0-9  flat inflection
	A-Z,0-9  rising inflection
	A-Z,0-9  falling inflection
--------------------------------------------------------------------------*/
static int make_alphanum_list(char *mod, int informat, int outformat, 
				char *string, char *list)
{
	int base=ALPHANUM_BASE;	/* 1st phrase for alphnum A-Z, 0-9 */
	int flat_base=base;	/* # of A flat phrase */
	int rise_base=base+36; 	/* # of A rise phrase */
	int fall_base=base+72;  /* # of A fall phrase */
	int num_letters=26;	/* Number of letters A-Z */
	
	char t[256];		/* temporary string */
	int ret, ret_code;
	int  pos;
	int  phrase;		/* number of phrase to speak */
	int last_position;	/* last string position */
	int  i,j;
	int zero;		/* Ascii value of 'A' */
	int a;			/* Ascii value of '0' */
	int ch; 
	int first_bad_char = 1;	/* Did we find a bad character before ? */	
	zero	= (int)'0';
	a 	= (int)'A';

	j=0;
	for (i=0; i < (int)strlen(string); i++)
	{
		ch=toupper(string[i]);
		if ( ( ch >= 'A' && ch <= 'Z') ||
		       (ch >= '0' && ch <= '9') )
		{
			t[j] = ch;
			j++;
		}
		else
		{
			/* Log msg only for 1st Non alphanum character found */
			if (first_bad_char)
			{
				first_bad_char = 0;
       				telVarLog(mod,REPORT_NORMAL, 
					TEL_SPEAK_WARN, GV_Warn,
       					TEL_SPEAK_WARN_MSG, string, 
					arc_val_tostr(informat), arc_val_tostr(outformat));
			}
		}
	}
	t[j]='\0';
	last_position = j - 1;

	/* Speak each character or digit with the proper intonation */
	for (pos=0; pos < last_position + 1; pos++)
	{
		if (pos == 0)
		{
			if ( isdigit(t[pos]) )
				phrase=rise_base+num_letters+(int)t[pos]-zero;
			else
				phrase = rise_base + (int)t[0] - a;
		}
		else if (pos == last_position)
		{
			if ( isdigit(t[pos]) )
				phrase=fall_base+num_letters+(int)t[pos]-zero;
			else
				phrase = fall_base + (int)t[pos] - a;
		}
		else
		{
			if ( isdigit(t[pos]) )
				phrase=flat_base+num_letters+(int)t[pos]-zero;
			else
				phrase = flat_base + (int)t[pos] - a;
		}	

		ret = append_system_phrase(mod, phrase);
		if (ret != 0) return(ret);

	} /* for */

  	return(0);
} 

/*----------------------------------------------------------------------------
This routine appends the dollar amount to the list file.
Note: regardless of the informat, the dollar amount is coming in as a string.
-----------------------------------------------------------------------------*/
static int make_dollar_list(char *mod, int in, int out, 
					char *input_string, char *list)
{
	int	i, j, ret;
	int	decimal_pos;		/* position of the decimal point */
	char	string[128];
	char	dollar_str[128];	/* holds dollar portion of string */
	char	reason[128];		/* reason used in error messages. */
	long	dollars;
	int	cents;			
	int	num_decimal_points=0;	/* # of decimal points in the string */
	int	num_bad_chars_found=0;	/* # of char found not "$0-9," */
	int	num_digits_after_decimal; /* # digits after the decimal point */

	/* Clear out the buffer we will fill after removing bad characters */
	memset(string, 0, sizeof(string));

	for(i=0,j=0;i<(int)strlen(input_string);i++)
	{
		switch(input_string[i])
		{
		case '.':
			num_decimal_points++;
			if (num_decimal_points > 1)
			{
				sprintf(reason, 
					"there are multiple decimal points");
       				telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, 
				GV_Err, TEL_CANT_SPEAK_MSG, input_string, 
				arc_val_tostr(in), arc_val_tostr(out), reason);
				return(-1);
			}
			decimal_pos = j;
			string[j++] = input_string[i];
			break;
		case '$':
		case ',':
			/* ignore these characters */
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			string[j] = input_string[i];
			j++;
			break;	
		default:
			num_bad_chars_found++;
			if (num_bad_chars_found == 1)
			{
       				telVarLog(mod,REPORT_NORMAL, 
					TEL_SPEAK_WARN, GV_Warn,
       					TEL_SPEAK_WARN_MSG, input_string, 
					arc_val_tostr(in), arc_val_tostr(out));
			}
			break;
		} /* switch */	
	
	} /* for */

  	if(num_decimal_points == 0)
	{
    		dollars = atol(string);
    		cents = 0;
	}
  	else
  	{
		/* Calculate dollars from the string left of decimal */
		strcpy(dollar_str, string);
		dollar_str[decimal_pos]='\0';
		dollars=atol(dollar_str);
		num_digits_after_decimal=(int)strlen(&string[decimal_pos]) - 1;

		switch(num_digits_after_decimal)
		{
		case 0:
			cents=0;
			break;
		case 1:
			cents=(string[decimal_pos+1]-'0')*10;
			break;
		default:	
			cents=(string[decimal_pos+1]-'0')*10 
					+ string[decimal_pos+2]-'0';
			break;
		} /* switch */
  	}
	
       	ret = SL_append_dollar(mod, in, out, dollars, cents);
       	if(ret == -1) return (ret);

	return (0);
}

/*----------------------------------------------------------------------------
Append the number to the list using recursive algoritham.
----------------------------------------------------------------------------*/
static int SL_append_number(char *mod, int in, int out, long number)
{
	long a_billion=1000000000;
	long a_million=1000000;
	long a_thousand=1000;
	long a_hundred=100;
	long max_number=1900000000;

	char string[MAX_LENGTH];
	char reason[MAX_LENGTH];
	
	int ret;
	int phr_base;

//		djb - This was originally commented out to fix an arcML issue.  
//		Caused another issue (MR 3469).  After setting it back, we
//		(me and Harshal) were not able to reproduce the arcML issue, so
//	if (number == (long)0) return(0);
	if (number < (long)0)
	{
		sprintf(string,"%ld", number);
		sprintf(reason, "number is negative");
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, 
			arc_val_tostr(in), arc_val_tostr(out), reason);
		return (-1);
	}

	if (number > max_number)
	{
		sprintf(string,"%ld", number);
		sprintf(reason,"number (%ld) exceeds maximum of %ld", 
				number, max_number);
       		telVarLog(mod,REPORT_NORMAL, TEL_CANT_SPEAK, GV_Err,
       			TEL_CANT_SPEAK_MSG, string, arc_val_tostr(in), arc_val_tostr(out), reason);
		return (-1);
	}

	if (number >=  a_billion)
	{
		SL_append_number(mod, in, out, (long)number/a_billion);
		ret=append_system_phrase(mod, BILLION_BASE);	
		if (ret !=0) return(ret);
		SL_append_number(mod, in, out, 
			number-(((long)number/a_billion)*a_billion));
	}
	else if (number >= a_million)
	{
		SL_append_number(mod, in, out, (long)number/a_million);
		ret=append_system_phrase(mod,MILLION_BASE);	
		if (ret !=0) return(ret);
		SL_append_number(mod, in, out, 
			number-(((long)number/a_million)*a_million));
	}
	else if (number >= a_thousand)
	{
		SL_append_number(mod, in, out, (long)number/1000);
		ret=append_system_phrase(mod, THOUSAND_BASE);	
		if (ret !=0) return(ret);
		SL_append_number(mod, in, out,  
			number-(((long)number/a_thousand)*a_thousand));
	}
	else if (number >= a_hundred)
	{	
		SL_append_number(mod, in, out, (long)number/100);
		ret=append_system_phrase(mod, HUNDRED_BASE);	
		if (ret !=0) return(ret);

		if ( number-(((long)number/a_hundred)*a_hundred) == 0 )
		{
			return(0);
		}
		SL_append_number(mod, in, out, 
			number-(((long)number/a_hundred)*a_hundred));
	}
else 
	{	
		ret=append_system_phrase(mod, NUMBER_BASE + number);
		return(ret);
	}
	return(0);
} /* SL speak number */

/*------------------------------------------------------------
SL_append_dollar(): Speak the amount.
-------------------------------------------------------------*/
int SL_append_dollar(char *mod, int in, int out, long dollars, int cents)
{
	int ret;
	int phr_base;

	if (dollars == 0L && cents == 0)	/* 0 dollars and 0 cents */
	{
		ret=append_system_phrase(mod, NUMBER_BASE);
		if (ret != 0) return(ret);

		ret=append_system_phrase(mod, DOLLARS_AND_BASE);
		if (ret != 0) return(ret);

		ret=append_system_phrase(mod, NUMBER_BASE);
		if (ret != 0) return(ret);
		
		ret=append_system_phrase(mod, CENTS_BASE);
		return(ret);
	}

	phr_base = NUMBER_BASE;
	if (dollars == 1)
	{
		if (cents == 0)
			phr_base = DOLLAR_BASE;
		else	
			phr_base = DOLLAR_AND_BASE;
		ret=append_system_phrase(mod, phr_base);
		if (ret != 0) return(ret);
		if (cents == 0) return (0);
	}
	else if (dollars > 1)
	{
		if (SL_append_number(mod, in, out, dollars) < 0) return (-1);

		if (cents == 0)
			phr_base = DOLLARS_BASE;	/* dollars */
		else	
			phr_base = DOLLARS_AND_BASE;	/* dollars and */

		ret=append_system_phrase(mod, phr_base);

		if (ret != 0) return(ret);
		if (cents == 0) return (0);
	}

	phr_base = NUMBER_BASE;
	if (dollars == 0L && cents > 0)	/* 0 dollars and some cents */
	{
		ret=append_system_phrase(mod, phr_base);
		if (ret != 0) return(ret);
		
		phr_base = DOLLARS_AND_BASE;	/* dollars */
		ret=append_system_phrase(mod, phr_base);
		if (ret != 0) return(ret);
	}

	if (cents > 1)
	{
		if (SL_append_number(mod, in, out, (long)(cents)) < 0) 
								return (-1);
		phr_base = CENTS_BASE;		/* cents */
	}
	else
		phr_base = CENT_BASE;		/* cent */

	ret=append_system_phrase(mod, phr_base);
	if (ret != 0) return(ret);
	
	return (0);
} /* SL speak dollar */


/*---------------------------------------------------------------------------
speak time	: Speak all time format
---------------------------------------------------------------------------*/
static	int SL_append_time(mod, format, tics)
char 	*mod;
int	format;
long	tics;
{
	char	hour_str[5];
	char	min_str[5];
	long	t = tics;
	struct  tm st;
	char	am_pm[5];	/* AM - PM */
	int	hour, min;
	int	ret;

//cftime(min_str, "%M", &t);
//time(&t);
st = *(struct tm *)localtime(&t);
strftime(min_str, 20, "%M", &st);

switch(format)
	{
	case TIME_STD:
		strftime(hour_str, 20, "%I", &st);
		strftime(am_pm, 20, "%p", &st);
		//cftime(hour_str, "%I", &t);	/* hour 1 to 12 */
		//cftime(am_pm, "%p", &t);
		break;
	case TIME_STD_MN:
		strftime(hour_str, 20, "%I", &st);
		strftime(am_pm, 20, "%p", &st);
		//cftime(hour_str, "%I", &t);	/* hour 1 to 12 */
		//cftime(am_pm, "%p", &t);
		if((atoi(hour_str) == 12) && (atoi(min_str) == 0))
		{
			if(strcmp(am_pm, "AM") == 0)
			{
				ret = append_system_phrase(mod, MIDNIGHT_PHRASE);
				if (ret != 0) return(ret);
			}
			else if (strcmp(am_pm, "PM") == 0)
			{
				ret = append_system_phrase(mod, NOON_PHRASE);
				if (ret != 0) return(ret);
			}
			return(0);
		}
		break;
	case TIME_MIL:
		strftime(hour_str, 20, "%H", &st);
		//cftime(hour_str, "%H", &t);	/* hour 0 to 23 */
		break;
	case TIME_MIL_MN:
		strftime(hour_str, 20, "%H", &st);
		//cftime(hour_str, "%H", &t);	/* hour 0 to 23 */
		if((atoi(hour_str) == 0) && (atoi(min_str) == 0))
		{
			ret = append_system_phrase(mod, MIDNIGHT_PHRASE);
			return(ret);
		}
		if((atoi(hour_str) == 12) && (atoi(min_str) == 0))
		{
			ret = append_system_phrase(mod, NOON_PHRASE);
			return(ret);
		}
		break;
	default:
		break;
	} /* switch */

	hour = atoi(hour_str);
	min  = atoi(min_str);

	/* speak the (numerical) hour */
	ret = append_system_phrase(mod, NUMBER_BASE+hour);
	if (ret != 0) return(ret);
	
	if (format == TIME_MIL && min == 0)		
	{
		ret = append_system_phrase(mod, HUNDRED_BASE);
		if (ret != 0) return(ret);
	}

	/* speak minutes */	
	if (min != 0)
	{
		if(min <= 9)
		{
			ret = append_system_phrase(mod, MINUTE_BASE+min);
			if (ret != 0) return(ret);
		}
		else
		{
			ret = append_system_phrase(mod, NUMBER_BASE+min);
			if (ret != 0) return(ret);
		}
	}

	if (strcmp(am_pm, "AM") == 0)
	{
		ret = append_system_phrase(mod, AM_PHRASE);
		if (ret != 0) return(ret);
	}
	else if (strcmp(am_pm, "PM") == 0)
	{
		ret = append_system_phrase(mod, PM_PHRASE);
		if (ret != 0) return(ret);
	}

	return (0);
} 

/*------------------------------------------------------------------------------
Play the loaded tags to zFilename.
------------------------------------------------------------------------------*/
static int tel_PlayTag(char *zMod, int zParty, char *zPhraseTag, int zOption,
                                int zMode, char *zDataOverrides)
{
        int     yRc;
        PhraseTagList   *pTmpTag;
        int     yCounter;
        char    yDiagMsg[128];
        int     yTagType;
        char    yInputFormatStr[64];
        char    yOutputFormatStr[64];
        int     yInputFormat;
        int     yOutputFormat;
        char    yTagData[128];

        if ( zOption == INTERRUPT )
        {
                zOption = FIRST_PARTY_INTERRUPT;
        }

        if ( ! GV_TagsLoaded )
        {
                sprintf(Msg, TEL_TAGS_NOT_LOADED_MSG);
       		telVarLog(zMod, REPORT_NORMAL, TEL_TAGS_NOT_LOADED, 
			GV_Err, Msg);
                return (-1);
        }

        pTmpTag = &GV_AppTags;
        while (pTmpTag->nextTag != (struct phraseTagStruct *)NULL)
        {
                if ( strcmp(pTmpTag->tagName, zPhraseTag) == 0)
                {
                        /* Speak all phrases associated w/ the tag */
                        for(yCounter = 0; yCounter < pTmpTag->nTags; yCounter++)
                        {
#ifdef DEBUG
                fprintf(stdout,"tel_PlayTag count=%d, <tag,subtag>=<%s:%s>.\n",
                yCounter, pTmpTag->tagName,pTmpTag->tag[yCounter].tagName);
                fflush(stdout);
#endif
                        /* Determine the data, taking account of overrides */
                                strcpy(yTagData, pTmpTag->tag[yCounter].data);
                                GV_DontSendToTelMonitor=1; /* No monitor */
#ifdef DEBUG
                                GV_DontSendToTelMonitor = 0; /*  4 Debugging */
#endif
                                yRc = TEL_Speak(zParty, zOption,
                                        pTmpTag->tag[yCounter].inputFormat,
                                        pTmpTag->tag[yCounter].outputFormat,
                                (void *)yTagData, zMode);

                                if ( (yRc == TEL_DISCONNECTED) ||
                                        (yRc == TEL_LOST_AGENT) )
                                {
                                        return (yRc);
                                }

                                if (yRc != TEL_SUCCESS)
                                {
                                        sprintf(Msg,
                                        TEL_FAILED_TO_SPEAK_TAG_MSG, zPhraseTag,
                                             pTmpTag->tag[yCounter].data,yRc);
       					telVarLog(zMod, REPORT_NORMAL, 
						TEL_FAILED_TO_SPEAK_TAG, 
						GV_Warn, Msg);
                                } 
                                else
                                {
                                        sprintf(Msg, TEL_SPOKE_TAG_MSG,
                                                zPhraseTag,
                                                pTmpTag->tag[yCounter].data);
       					telVarLog(zMod, REPORT_VERBOSE, 
						TEL_SPOKE_TAG, GV_Info, Msg);
                                }
                        }
                        return (TEL_SUCCESS);
                }
                pTmpTag = pTmpTag->nextTag;
        }

        sprintf(Msg, TEL_UNABLE_TO_FIND_TAG_MSG, "UH OH!");
       	telVarLog(zMod, REPORT_NORMAL, TEL_UNABLE_TO_FIND_TAG, GV_Err, Msg);
        return (-1);
} /* END: tel_PlayTag() */

int sGetNextToken(char *zStr, char *zDelimiters, char *zToken, 
						int *zIsDelimiter)
{
	char 	*yPtr;
	int		yState;
	int		yIndex;

	zToken[0] = 0;

	yIndex = 0;
	yState = 0;
	yPtr = zStr;

	while(yPtr)
	{
		if(strchr(zDelimiters, *yPtr))
		{
			if(yState == 0)
			{
				sprintf(zToken, "%c", *yPtr);
				*zIsDelimiter = 1;
			}
			else
			{
				zToken[yIndex] = '\0';
				*zIsDelimiter = 0;
			}

			return(1);
		}
		
		yState = 1;

		zToken[yIndex] = *yPtr;

		yIndex++;

		yPtr++;

	} /* while */

	return(0);

} /* END: sGetNextToken() */ 

int make_silence_file(int zMilliSecs, int zPort, char *zOut, char *zMessage)
{
	int duration 		= zMilliSecs;
	static 	int yCounter 		= 0;
	char ySilenceFile[265];
	char yPhraseFile[265];
	int	fd, phraseFd;
	int	phrase;
	int	pid;

	pid = getpid();

	if(!zMessage)
	{
		return (-1);
	}

	if(zMilliSecs <= 0)
	{
		sprintf(zMessage, "Invalid duration(%d)", zMilliSecs);
		return (-1);
	}

	if(!zOut)
	{
		sprintf(zMessage, "%s", "NULL output buffer");
		return (-1);
	}

	sprintf(ySilenceFile, "silence@%d_%d.64p", pid, yCounter++);

	if((fd = open(ySilenceFile, O_CREAT|O_RDWR, S_IRWXU | S_IRWXG)) < 0)
	{
		
		sprintf(zMessage, "Failed to create file(%s) errno=%d",
				ySilenceFile, errno);
		return (-1);
	}
	
	while( duration > 0 )
	{
		/*
		** DIVIDE DURATION BY EACH OF OUR SILENCE PHRASES
		** STARTING WITH THE LARGEST FIRST.  IF IT WILL GO AT
		** LEAST ONCE, PLAY THAT PHRASE.
		**
		** REDUCE DURATION BY THE LENGTH OF THE PHRASE
		** AND LOOP AGAIN.
		*/

		if( duration /8192 )
		{
			duration -= 8192;	// Reduce duration
			phrase	 = 8192;	// Set Phrase File Name
		}
		else if( duration /4096 )
		{
			duration -= 4096;
			phrase	 = 4096;
		}
		else if( duration /2048 )
		{
			duration -= 2048;
			phrase	 = 2048;
		}
		else if( duration /1024 )
		{
			duration -= 1024;
			phrase	 = 1024;
		}
		else if( duration /512 )
		{
			duration -= 512;
			phrase	 = 512;
		}
		else if( duration /256 )
		{
			duration -= 256;
			phrase	 = 256;
		}
		else if( duration /128 )
		{
			duration -= 128;
			phrase	 = 128;
		}
		else if( duration /64 )
		{
			duration -= 64;
			phrase	 = 64;
		}
		else if( duration /32 )
		{
			duration -= 32;
			phrase	 = 32;
		}
		else if( duration / 16 )
		{
			duration -= 16;
			phrase	 = 16;
		}
		else if( duration / 8 )
		{
			duration -= 8;
			phrase	 = 8;
		}
		else if( duration / 4 )
		{
			duration -= 4;
			phrase	 = 4;
		}
		else if( duration / 2 )
		{
			duration -= 2;
			phrase	 = 2;
		}
		else if( duration / 1 )
		{
			duration -= 1;
			phrase	 = 1;
		}
		else
		{
			duration = 0;
			continue;
		}

		sprintf(yPhraseFile,"silence_%d.64p", phrase);

		if((phraseFd = open(yPhraseFile, O_RDONLY)) < 0)
		{
			
			sprintf(zMessage, "Failed to open file(%s) errno=%d",
					yPhraseFile, errno);

			close(fd);
			unlink(ySilenceFile);
			return (-1);
		}

		if(appendFiles(fd, phraseFd, ySilenceFile, yPhraseFile, zMessage) == -1)
		{
			close(fd);
			close(phraseFd);
			unlink(ySilenceFile);
			return (-1);
		}

		close(phraseFd);
	}

	close(fd);
	sprintf(zOut, "%s", ySilenceFile);
	return(0);
}

int appendFiles(int fd_w, int fd_r, char * zWriteFile, char * zReadFile, 
					char *zMessage)
{

	int nread, nwrite;	
	char    buf[512];

	while((nread = read(fd_r, buf, sizeof(buf))) != 0)
	{
		if(nread == -1)
		{
			sprintf(zMessage, "Failed to copy (%s) to (%s). [%d, %s]",
                zReadFile, zWriteFile, errno, strerror(errno));
			return (-1);
		}

		nwrite = write(fd_w, buf, nread);

		if(nwrite != nread)
		{
            sprintf(zMessage, "Failed to copy (%s) to (%s). [%d, %s]",
                zReadFile, zWriteFile, errno, strerror(errno));
			return (-1);
		}

	}

	return (0);
}
