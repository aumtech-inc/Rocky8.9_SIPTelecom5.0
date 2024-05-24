/*-----------------------------------------------------------------------------
Program:        TEL_SRRecognizeV2.c 
Purpose:        Recognize for mrcpClient V2.
Author:         Aumtech, Inc.
Update: 08/05/2006 djb Created this from TEL_SRRecognize.c.
Update: 08/26/2006 djb Removed debug statements.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

int gAltRetCode;

static int checkV2GoogleResult(char *zResultFile, char *zTranslation);
static int googleRecordOnNomatch(char *zUtteranceFile, char *zTranslation);

static char m[512];
int TEL_SRUtterance(char *zUtteranceFile, char *zTranslation)
{
	static char mod[]="TEL_SRUtterance";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;
	char lBeepFileName[256];
	char *typeAheadBuffer;
	int requestSent = 0;
	int lGotDTMF = 0;
	char lDtmfBuf[20];

	struct MsgToApp response;
	struct Msg_SRRecognize lMsg;
	struct MsgToDM msg; 

	rc = apiInit(mod, TEL_SRUTTERANCE, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	if ( strlen(zUtteranceFile) == 0) 
	{
		sprintf(m, "There is no utterance file to translate. Unable to perform translation. "
			"$RECORD_UTTERANCE must be set prior to the prompt.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
		HANDLE_RETURN(-1);
	}

	if (access(zUtteranceFile, F_OK) != 0 )
	{
		sprintf(m, "Failed to access utterance file (%s) for translation. Unable to perform translation. "
			"$RECORD_UTTERANCE must be set prior to the prompt.", zUtteranceFile);
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
		HANDLE_RETURN(-1);
	}

	sprintf(m, "%s", "Calling googleRecordOnNomatch()");
	telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info, m);

	rc = googleRecordOnNomatch(zUtteranceFile, zTranslation);

	if(GV_IsCallDisconnected1 == 1)
	{
		HANDLE_RETURN(TEL_DISCONNECTED);
	}

	if ( rc < 0 )
	{
		HANDLE_RETURN(rc);
	}

	HANDLE_RETURN(TEL_SUCCESS);
} // END: TEL_SRUtterance()

static int googleRecordOnNomatch(char *zUtteranceFile, char *zTranslation)
{
	static char mod[]="googleRecordOnNomatch";

	typedef struct
	{ 
		int		opcode;
		int		mmid;
		int		telport;
		int		rectime;
		int		trailtime;
		char	data[128];
		char	resultFileName[64];
		char	options[64];
	} GSR_request;

	typedef struct
	{ 
		int		opcode;
		int		mmid;
		int		telport;
		int		udpport;
		int		other;
		char	data[128];
		char	resultFileName[64];
		char	options[64];
	} GSR_response;

	GSR_request		gsrRequest;
	GSR_response	gsrResponse;
	char			resultStr[256];
	int			rc;
	char			resultFile[128];
	char			triggerFile[128];

	static char		googleResponseFifo[128] = "";
	static int		googleResponseFifoFd = -1;
	static char		googleRequestFifo[128] = "/tmp/ArcGSRRequestFifo";
	static int		googleRequestFifoFd = -1;

	time_t		tm;
	struct tm	tmStruct;
	char		tmChar[16];

	//
	// Send the the request struct
	//
	if(googleRequestFifoFd <= -1)
	{
		if ( (googleRequestFifoFd = open (googleRequestFifo, O_RDWR)) < 0 )
		{
			sprintf(m, "Failed to open request fifo (%s). [%d, %s] "
					"Unable to communicate with Google SR Client.",
					googleRequestFifo, errno, strerror (errno));
			telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Info, m);
			return(-1);
		}
	}

	time(&tm);
	localtime_r (&tm, &tmStruct);
	strftime(tmChar, 10, "%H%M%S", &tmStruct);

	memset((GSR_request *)&gsrRequest, '\0', sizeof(gsrRequest));
	gsrRequest.opcode 	= 6;
	gsrRequest.mmid 	= 99;
	gsrRequest.telport = GV_AppCallNum1;
	gsrRequest.rectime = 97;
	gsrRequest.trailtime = 96;
	sprintf(gsrRequest.data, "%s", zUtteranceFile);
	sprintf(gsrRequest.resultFileName, "/tmp/googleResult.%d.%s", GV_AppCallNum1, tmChar);
	sprintf(gsrRequest.options, "%s", "");

	rc = write (googleRequestFifoFd, &gsrRequest, sizeof (gsrRequest));
	
	sprintf(m, "Sent %d bytes to (%s) for opcode=%d, file=(%s) resultFile(%s)",
			rc, googleRequestFifo, gsrRequest.opcode, gsrRequest.data, gsrRequest.resultFileName);
	telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
	
	rc = checkV2GoogleResult(gsrRequest.resultFileName, zTranslation);
	close(googleResponseFifoFd);
	return(rc);

} // END: googleRecordOnNomatch()

static int checkV2GoogleResult(char *zResultFile, char *zTranslation)
{
	#define		TIME_TO_WAIT	60 // seconds
	int rc = 0;
	int removeFile = 1;
	int	status;
	time_t currentTime;
	time_t endTime;
	int gotResult;
	int			counter;
	char		tmpBuf[512];
	struct stat	stat_buf;

	char mod[] = "checkV2GoogleResult";

	*zTranslation= '\0';
	if((rc = access("/tmp/gsKeepAudio.txt", R_OK)) == 0)
	{
		removeFile = 0;
	}
	
	memset((char *)tmpBuf, '\0', sizeof(tmpBuf));
	char yStrGoogleResultFile[64];
	char yStrGoogleTriggerFile[64];

	char *triggerLine = NULL;
	size_t len = 0;
	ssize_t read;
	int triggerContent = 0;


	sprintf(yStrGoogleResultFile, "%s", zResultFile);
	sprintf(yStrGoogleTriggerFile, "%s.trigger", zResultFile);

    time(&currentTime);
	endTime = currentTime + TIME_TO_WAIT;

	while ( currentTime < endTime )
	{
		if(access(yStrGoogleTriggerFile, W_OK) > -1)
		{
			FILE *yGoogleTriggerFp = NULL;
			if((yGoogleTriggerFp = fopen(yStrGoogleTriggerFile, "r") )!= NULL)
			 {
				read = getline(&triggerLine, &len, yGoogleTriggerFp);	
	
				fclose(yGoogleTriggerFp);
	
				if(triggerLine)
				{
					triggerContent = atoi(triggerLine);
					free(triggerLine);
				}
			}
	
			sprintf(m, "Google trigger file contents (%d).", triggerContent);
			telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
	
			//  This means the final google result is expected. so wait for at least 10 sec
	
			counter = 1;
	
			while( (triggerContent == 0) &&
			       (currentTime < endTime) &&
				   (access(yStrGoogleResultFile, R_OK|W_OK)) < 0)
			{
				if ( counter++ % 15 == 0 )  // check for disconnect every 1.5  seconds
				{
					if ((rc = TEL_PortOperation(GET_FUNCTIONAL_STATUS, GV_AppCallNum1, &status)) == TEL_DISCONNECTED )
					{
						sprintf(m, "%s", "Got DISCONNECT waiting for google result file.");
						telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);

						if ( ( removeFile == 1 ) &&
						     ( access (yStrGoogleTriggerFile, F_OK) == 0 ) )
				        {
							if ( (rc = unlink(yStrGoogleTriggerFile)) == 0 )
							{
								sprintf(m, "Removed (%s)", yStrGoogleTriggerFile);
								telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
							}
							else
							{
								sprintf(m, "Failed to unlink (%s). [%d, %s]", yStrGoogleTriggerFile, errno, strerror(errno));
								telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
							}
						}
						return( TEL_DISCONNECTED);
					}
					return(-3);
				}
	
				usleep(100000); // 0.1 seconds
				time(&currentTime);
			}
			break;
		}
		else
		{
			usleep(100000); // 0.1 seconds
			time(&currentTime);
		}
	}
	
	if ( ( removeFile == 1 ) &&
	     ( access (yStrGoogleTriggerFile, F_OK) == 0 ) )
	{
			if ( (rc = unlink(yStrGoogleTriggerFile)) == 0 )
			{
				sprintf(m, "Removed (%s)", yStrGoogleTriggerFile);
				telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
			}
			else
			{
				sprintf(m, "Failed to unlink (%s). [%d, %s]", yStrGoogleTriggerFile, errno, strerror(errno));
				telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
			}
	}
	if ( (triggerContent == -2) ||
         (currentTime >= endTime) )
	{
		//return(TEL_TIMEOUT);
		return(-2);
	}

	if(access(yStrGoogleResultFile, R_OK|W_OK) > -1)
	{
		FILE *yGoogleFp = NULL;

		if((yGoogleFp = fopen(yStrGoogleResultFile, "r") )!= NULL)
		{
			int nBytes = 0;

			if(stat(yStrGoogleResultFile,  &stat_buf) < 0)
			{
				sprintf(m, "Failed to stat result file (%s). [%d, %s] Returning failure.",
								yStrGoogleResultFile, errno, strerror(errno));
				telVarLog(__LINE__, GV_AppCallNum1, mod,REPORT_NORMAL, TEL_BASE, GV_Info, m);
				fclose(yGoogleFp);
				if ( ( removeFile == 1 ) &&
	     		     ( access (yStrGoogleResultFile, F_OK) == 0 ) )
				{
					if ( (rc = unlink(yStrGoogleResultFile)) == 0 )
					{
						sprintf(m, "Removed (%s)", yStrGoogleResultFile);
						telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
					}
					else
					{
						sprintf(m, "Failed to unlink (%s). [%d, %s]", yStrGoogleResultFile, errno, strerror(errno));
						telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
					}
				}
			}
			if ( stat_buf.st_size <= 0 )
			{
				sprintf(m, "%s", "No results returned.  Returning failure.");
				telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Info, m);
			}
		
			nBytes = fread(tmpBuf, 1, stat_buf.st_size, yGoogleFp);
			sprintf(zTranslation, "%s", tmpBuf);
			fclose(yGoogleFp);
			
			sprintf(m, "Google Result (%s)", zTranslation);
			telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
		}
		else
		{
			sprintf(m, "Failed to open result file (%s). [%d, %s] Returning failure.",
							yStrGoogleResultFile, errno, strerror(errno));
			telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
			if ( ( removeFile == 1 ) &&
	     	     ( access (yStrGoogleResultFile, F_OK) == 0 ) )
			{
				if ( (rc = unlink(yStrGoogleResultFile)) == 0 )
				{
					sprintf(m, "Removed (%s)", yStrGoogleResultFile);
					telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
				}
				else
				{
					sprintf(m, "Failed to unlink (%s). [%d, %s]", yStrGoogleResultFile, errno, strerror(errno));
					telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
				}
			}
			return (-1);
		}

		if ( ( removeFile == 1 ) &&
     	     ( access (yStrGoogleResultFile, F_OK) == 0 ) )
		{
			if ( (rc = unlink(yStrGoogleResultFile)) == 0 )
			{
				sprintf(m, "Removed (%s)", yStrGoogleResultFile);
				telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
			}
			else
			{
				sprintf(m, "Failed to unlink (%s). [%d, %s]", yStrGoogleResultFile, errno, strerror(errno));
				telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
			}
		}
		return(0);
	}//end access yStrGoogleResult
	else
	{
		sprintf(m, "Failed to acces result file (%s). [%d, %s] Returning failure.",
							yStrGoogleResultFile, errno, strerror(errno));
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
		if ( ( removeFile == 1 ) &&
     	     ( access (yStrGoogleResultFile, F_OK) == 0 ) )
		{
			if ( (rc = unlink(yStrGoogleResultFile)) == 0 )
			{
				sprintf(m, "Removed (%s)", yStrGoogleResultFile);
				telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
			}
			else
			{
				sprintf(m, "Failed to unlink (%s). [%d, %s]", yStrGoogleResultFile, errno, strerror(errno));
				telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info, m);
			}
		}
		return (-1);
	}

} // END: checkV2GoogleResult(char *zTranslation)
