/*-----------------------------------------------------------------------------
Program:        TEL_SRRecognizeV2.c 
Purpose:        Recognize for mrcpClient V2.
Author:         Aumtech, Inc.
Update: 08/05/2006 djb Created this from TEL_SRRecognize.c.
Update: 08/26/2006 djb Removed debug statements.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

#define MAX_LEAD_SILENCE 60000
#define MAX_TRAIL_SILENCE 60000
#define MAX_TOTAL_TIME 60000

int gAltRetCode;

extern int mkdirp(char *zDir, int perms);
static int parametersAreGood(char *zMod, SRRecognizeParams *zParams);

int TEL_SRRecognizeV2(SRRecognizeParams *zParams)
{
	static char mod[]="TEL_SRRecognizeV2";
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

	rc = apiInit(mod, TEL_SRRECOGNIZE, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	if( !parametersAreGood(mod, zParams) )
	{
		HANDLE_RETURN(-1);
	}

	if ( ( GV_SRType == GOOGLE_SR ) && 
	     ( GV_GoogleRecord == YES ) )
	{
		int			recInterrupt;
		char		tmpWav[128];
		char		wavTime[32];
		int			flags;
		time_t 		tmpTM;
		struct tm	*pTM;
		static char	tmpDir[64] = "/tmp/.googleAudio";

		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Google SR w/record file is enabled. Calling TEL_Record().");

		if(access(tmpDir, F_OK) == -1)
		{
			flags =     S_IWRITE|S_IREAD|S_IEXEC;
			flags |=    S_IRGRP|S_IROTH;
			flags |=    S_IXGRP|S_IXOTH;

			if(mkdirp(tmpDir, flags) == -1)
			{
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Failed to create directory (%s). Using /tmp.", tmpDir);
				sprintf(tmpDir, "%s", "/tmp");
			}
		}

		(void *)time(&tmpTM);
		pTM = localtime(&tmpTM);
		(void)strftime(wavTime, 32, "%d%H%M%S", pTM);

		sprintf(tmpWav, "%s/tmpRec.%d.%s.wav", tmpDir, GV_AppCallNum1, wavTime);

		if ( zParams->interruptOption == YES )
		{
			recInterrupt = INTERRUPT;
		}
		else
		{
			recInterrupt = NONINTERRUPT;
		}


		return(TEL_Record(zParams->party, tmpWav, zParams->totalTime / 1000, 
					COMP_WAV, YES, 
					zParams->leadSilence / 1000, zParams->trailSilence / 1000, 
					zParams->beep, recInterrupt,
					zParams->termination_char, SYNC));
	}

	gAltRetCode = 0;
	sprintf(apiAndParms,"%s(%s, %s, %s, %s, %d, %d, %d, %s)",
		mod, arc_val_tostr(zParams->party), arc_val_tostr(zParams->bargein),
		arc_val_tostr(zParams->interruptOption), arc_val_tostr(zParams->beep), 
		zParams->leadSilence, zParams->trailSilence, 
		zParams->totalTime, zParams->resourceNames);

	memset(lDtmfBuf, 0, sizeof(lDtmfBuf));
	lMsg.opcode=DMOP_GETDTMF;
	lMsg.appPid 	= GV_AppPid;
	lMsg.appPassword = 0;
	if (zParams->party == FIRST_PARTY)
	{
		typeAheadBuffer = GV_TypeAheadBuffer1;

		lMsg.appCallNum=GV_AppCallNum1;
	}
	else
	{
		typeAheadBuffer = GV_TypeAheadBuffer2;
		lMsg.appCallNum=GV_AppCallNum2;
	}

	if(zParams->beep == YES)
	{	
		sprintf(lBeepFileName, "%s/%s", GV_SystemPhraseDir, 
			BEEP_FILE_NAME);

		if(strlen(GV_BeepFile) != 0)
		{
			rc = access(GV_BeepFile, F_OK);
			if(rc == 0)
				sprintf(lBeepFileName, "%s", GV_BeepFile);
			else
			{
				telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
				"Failed to access beep file (%s). errno=%d. "
				"Using default beep file.", GV_BeepFile, errno);
			}
		}
		
		rc = access(lBeepFileName, F_OK);
		if(rc == 0)
		{
			sprintf(lMsg.beepFile, "%s", lBeepFileName); 
		}
  		else
		{
			telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
				"Beep phrase (%s) doesn't exist. errno=%d. Setting "
				" beep to NO.", lBeepFileName, errno);
			zParams->beep = NO;
			memset(lMsg.beepFile, 0, sizeof(lMsg.beepFile));
		}
	}

	while(1)
	{
		rc = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		if(rc == -2) break;

		rc = examineDynMgrResponse(mod, (struct MsgToDM *)&lMsg, &response);	
		switch(rc)
		{
			case DMOP_GETDTMF:
				saveTypeAheadData(mod, response.appCallNum, 
					response.message);
				break;

			case DMOP_DISCONNECT:
				//HANDLE_RETURN(disc(mod, response.appCallNum, GV_AppCallNum1, GV_AppCallNum2));
				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;

			default:
				/* Unexpected msg. Logged in examineDynMgr.. */
				break;
		} /* switch rc */
	}

	switch(zParams->interruptOption)
	{
		case FIRST_PARTY_INTERRUPT:
		case SECOND_PARTY_INTERRUPT:
		case YES:
			if(*typeAheadBuffer != '\0')
				HANDLE_RETURN(TEL_SR_RECEIVED_DTMF);
			break;

		case NONINTERRUPT:
		case NO:
			switch(zParams->party)
			{
				case FIRST_PARTY:
					sprintf(lDtmfBuf, "%s", GV_TypeAheadBuffer1);
					memset(GV_TypeAheadBuffer1, 0,
						sizeof(GV_TypeAheadBuffer1));
					break;
				
				case SECOND_PARTY:
					sprintf(lDtmfBuf, "%s", GV_TypeAheadBuffer2);
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

	lMsg.opcode = DMOP_SRRECOGNIZE;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;

	lMsg.party			= zParams->party;
	lMsg.bargein		= zParams->bargein;
	lMsg.total_time		= zParams->totalTime;
	lMsg.lead_silence	= zParams->leadSilence;
	lMsg.trail_silence	= zParams->trailSilence;

	sprintf(lMsg.resource, "%s", lDtmfBuf);
	
	if((zParams->interruptOption != NONINTERRUPT) && 
	   (zParams->interruptOption != NO))
        lMsg.interrupt_option = 1;
	else
        lMsg.interrupt_option = 0;

	memset(lMsg.beepFile, 0, sizeof(lMsg.beepFile));
	if(zParams->beep == YES)
	{	
		sprintf(lBeepFileName, "%s/%s", GV_SystemPhraseDir, BEEP_FILE_NAME);

		if(strlen(GV_BeepFile) != 0)
		{
			rc = access(GV_BeepFile, F_OK);
			if(rc == 0)
				sprintf(lBeepFileName, "%s", GV_BeepFile);
			else
			{
				telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
				"Failed to access beep file (%s).  [%d, %s] "
				"Using default beep file.", GV_BeepFile, errno, strerror(errno));
			}
		}
		
		rc = access(lBeepFileName, F_OK);
		if(rc == 0)
		{
			sprintf(lMsg.beepFile, "%s", lBeepFileName); 
		}
		else
		{
			telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
				"Beep phrase (%s) doesn't exist. [%d,%s] Setting "
				" beep to NO.", lBeepFileName, errno, strerror(errno));
			zParams->beep = NO;
			memset(lMsg.beepFile, 0, sizeof(lMsg.beepFile));
		}
	}

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRRecognize));

	requestSent = 0;
	while(1)
	{
		if(!requestSent)
		{
			rc = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			if(rc == -2)
			{
				if ( GV_SRType == GOOGLE_SR )
				{
					telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"Google SR is enabled. Sending DMOP_RECOGNIZE request.");
					rc = sendRequestToDynMgr(mod, &msg); 
				}
				else
				{
					rc = sendRequestToSRClient(mod, &msg);
				}
        		if (rc == -1) HANDLE_RETURN(-1);
				requestSent=1;
				rc = readResponseFromDynMgr(mod,600,
						&response,sizeof(response));
				if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,600,&response,sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		}

		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
					"Timeout waiting for response from Dynamic Manager.");
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);

		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
			case DMOP_SRRECOGNIZE:
//				*zpVendorErrorCode = response.vendorErrorCode;
				gAltRetCode = response.alternateRetCode;
//				zParams->numResults = response.message[0];
				zParams->numResults = atoi(response.message);

				if(lGotDTMF == 1)
				{
					HANDLE_RETURN(TEL_SR_RECEIVED_DTMF);
				}
				HANDLE_RETURN(response.returnCode);
				break;

			case DMOP_GETDTMF:
				if(response.appCallNum != msg.appCallNum)
				{
					/* Save data for the other leg of the call */
					saveTypeAheadData(mod, response.appCallNum,
						response.message);
					break;
				}

				switch(zParams->interruptOption)
				{
					case NONINTERRUPT: 
					case NO:
						break;
					case YES:
					case FIRST_PARTY_INTERRUPT:
					case SECOND_PARTY_INTERRUPT:
						/* We've already weeded out if wrong callNum */
						saveTypeAheadData(mod, response.appCallNum,
							response.message);
						if(strcmp(response.message,"CLEAR")==0) 
						{
							break;
						}

						lGotDTMF = 1;
						break;

					default:
						telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
									"Unknown interruptOption: %d.",
									zParams->interruptOption );
				}
				break;

			case DMOP_DISCONNECT:
   				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;

			default:
				/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		} /* switch rc */
	} /* while */
}
	
/*-------------------------------------------------------------------------
This function verifies that all parameters are valid.
It returns 1 (yes) if all parmeters are good, 0 (no) if not.
-------------------------------------------------------------------------*/
static int parametersAreGood(char *mod, SRRecognizeParams *zParams)
//static int parametersAreGood(char *mod, int zParty, int *zpBargein,
//	int *zpBeep, char *zpResource, int *zpTotalTime, int *zTokenTypeRequested,
//	int *zpLeadSilence, int *zpTrailSilence, int *zpToneInterruptOption)
{
	char m[MAX_LENGTH];
	int  errors=0;
	int  lDefaultLeadSilence = 3000;
	int  lDefaultTrailSilence = 3000;

	switch(zParams->party)
	{
		case FIRST_PARTY:
			break;

		default:
			errors ++;
			sprintf(m, "Invalid Party parameter: %s. "
				"Valid values: FIRST_PARTY.", arc_val_tostr(zParams->party));
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, m);
			break;
	}
    switch(zParams->bargein)
    {
        case NONINTERRUPT:
        case NO:
        case INTERRUPT:
        case YES:
            break;

        default:
            sprintf(m, "Warning: Invalid Bargein parameter: %s. "
                "Valid values: YES, NO. Using default NO.",
				arc_val_tostr(zParams->bargein));
            telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
            zParams->party = NO;
            break;
    }

	if((zParams->totalTime < 1) ||
	   (zParams->totalTime > MAX_TOTAL_TIME))
	{
		sprintf(m, "Record time is %d. Valid values are 1 - %d. Using %d.", 
			zParams->totalTime, MAX_TOTAL_TIME, MAX_TOTAL_TIME);
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
		zParams->totalTime = 60000;
	}

	if((zParams->leadSilence < 0) || (zParams->leadSilence > MAX_LEAD_SILENCE))
	{
		sprintf(m, "Invalid lead_silence parameter: %d. Valid values are 0"
			"- %d. Using default %d.", zParams->leadSilence, MAX_LEAD_SILENCE, 
			lDefaultLeadSilence);
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
		zParams->leadSilence = lDefaultLeadSilence;
	}

	if( (zParams->trailSilence < 250) || 
	    (zParams->trailSilence > MAX_TRAIL_SILENCE))
	{
		sprintf(m, "Invalid trail_silence parameter: %d. Valid values are "
			"250 - %d. Using default %d.", zParams->trailSilence, MAX_TRAIL_SILENCE, 
			lDefaultTrailSilence);
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
		zParams->trailSilence = lDefaultTrailSilence;
	}

	switch(zParams->beep)
	{
		case YES:
		case NO:
			break;

		default:
			sprintf(m,
				"Invalid Beep parameter: %s. Valid values: YES, NO. "
				"Using default YES.", arc_val_tostr(zParams->beep));
			telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
			zParams->beep = YES;
			break;
	}

	switch(zParams->interruptOption)
	{
		case YES:
		case NO:
			break;

		default:
			sprintf(m, "Invalid ToneInterruptOption option parameter: %s. "
			   "Valid values: YES, NO. Using default NO.", 
			   arc_val_tostr(zParams->interruptOption));
			telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
			zParams->interruptOption = NO;
			break;
	}

	return(errors == 0);   
}
