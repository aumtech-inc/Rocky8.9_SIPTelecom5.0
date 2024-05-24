/*-----------------------------------------------------------------------------
Program:        TEL_SRRecognize.c 
Purpose:        Actually does the speech recognition.
Author:         Anand Joglekar
Date:			10/01/2002
Update: 05/05/06 djb MR # 633 - Removed references to SR_CONCEPT_AND_TAG.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

#define MAX_LEAD_SILENCE 60000
#define MAX_TRAIL_SILENCE 60000
#define MAX_TOTAL_TIME 60000

int gAltRetCode;

static int parametersAreGood(char *mod, int zParty, int *zpBargein,
	int *zpBeep, char *zpResource, int *zpTotalTime, int *zTokenTypeRequested,
	int *zpLeadSilence, int *zpTrailSilence, int *zpToneInterruptOption);

int TEL_SRRecognize(int zParty, int zBargein, int zToneInterruptOption,
	int zBeep, int zLeadSilence, int zTrailSilence, int zTotalTime, 
	int * zpAlternatives, char *zpResource, int zTokenTypeRequested, 
	int *zpVendorErrorCode)
{
	static char mod[]="TEL_SRRecognize";
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

	if ( GV_SRType == GOOGLE_SR )
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Google SR is enabled. Returning with no action.");
		return(0);
	}

	gAltRetCode = 0;
	*zpVendorErrorCode = 0;
	sprintf(apiAndParms,"%s(%s, %s, %s, %s, %d, %d, %d, %d, %s, %s, "
		"&VendorErrorCode", mod, arc_val_tostr(zParty), arc_val_tostr(zBargein),
		arc_val_tostr(zToneInterruptOption), arc_val_tostr(zBeep), zLeadSilence, zTrailSilence, 
		zTotalTime, zpAlternatives, zpResource, 
		convertTokenType(zTokenTypeRequested));
	rc = apiInit(mod, TEL_SRRECOGNIZE, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	if(!parametersAreGood(mod, zParty, &zBargein, &zBeep, zpResource,
		&zTotalTime, &zTokenTypeRequested, &zLeadSilence, &zTrailSilence, 
		&zToneInterruptOption))
	{
		HANDLE_RETURN(-1);
	}


	memset(lDtmfBuf, 0, sizeof(lDtmfBuf));
	lMsg.opcode=DMOP_GETDTMF;
	lMsg.appPid 	= GV_AppPid;
	lMsg.appPassword = 0;
	if (zParty == FIRST_PARTY)
	{
		typeAheadBuffer = GV_TypeAheadBuffer1;
		lMsg.appCallNum=GV_AppCallNum1;
	}
	else
	{
		typeAheadBuffer = GV_TypeAheadBuffer2;
		lMsg.appCallNum=GV_AppCallNum2;
	}

	if(zBeep == YES)
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
				"Beep phrase (%s) doesn't exist. [%d, %s] Setting "
				" beep to NO.", lBeepFileName, errno, strerror(errno));
			zBeep = NO;
			memset(lMsg.beepFile, 0, sizeof(lMsg.beepFile));
		}
	}

	while(1)
	{
		rc = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		if(rc == -2) break;

		rc = examineDynMgrResponse(mod, &lMsg, &response);	
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

	switch(zToneInterruptOption)
	{
		case FIRST_PARTY_INTERRUPT:
		case SECOND_PARTY_INTERRUPT:
		case YES:
			if(*typeAheadBuffer != '\0')
				HANDLE_RETURN(TEL_SR_RECEIVED_DTMF);
			break;

		case NONINTERRUPT:
		case NO:
			switch(zParty)
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

	lMsg.party = zParty;
	lMsg.bargein = zBargein;
	lMsg.total_time = zTotalTime;
	lMsg.lead_silence = zLeadSilence;
	lMsg.trail_silence = zTrailSilence;
	lMsg.tokenType = zTokenTypeRequested;
	lMsg.alternatives = *zpAlternatives;

	sprintf(lMsg.resource, "%s", lDtmfBuf);
	
	if((zToneInterruptOption != NONINTERRUPT) && (zToneInterruptOption != NO))
        lMsg.interrupt_option = 1;
	else
        lMsg.interrupt_option = 0;

	memset(lMsg.beepFile, 0, sizeof(lMsg.beepFile));
	if(zBeep == YES)
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
				"Failed to access beep file (%s). [%d, %s] "
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
                        "Beep phrase (%s) doesn't exist. [%d, %s] Setting "
			" beep to NO.", lBeepFileName, errno, strerror(errno));
			zBeep = NO;
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
#ifdef MRCP_SR
				rc = sendRequestToSRClient(mod, &msg);
#else
				rc = sendRequestToDynMgr(mod, &msg); 
#endif
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
				*zpVendorErrorCode = response.vendorErrorCode;
				gAltRetCode = response.alternateRetCode;
				*zpAlternatives = response.message[0];

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

				switch(zToneInterruptOption)
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
						printf("Unknown interruptOption: %d\n",
						zToneInterruptOption);
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
static int parametersAreGood(char *mod, int zParty, int *zpBargein,
	int *zpBeep, char *zpResource, int *zpTotalTime, int *zTokenTypeRequested,
	int *zpLeadSilence, int *zpTrailSilence, int *zpToneInterruptOption)
{
	char m[MAX_LENGTH];
	int  errors=0;
	int  lDefaultLeadSilence = 3000;
	int  lDefaultTrailSilence = 3000;

	switch(*zpBargein)
	{
		case NONINTERRUPT:
		case NO:
		case INTERRUPT:
		case YES:
			break;

		default:
			sprintf(m, "Warning: Invalid Bargein parameter: %s. "
				"Valid values: YES, NO. Using default NO.", arc_val_tostr(*zpBargein));
			telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
			*zpBargein = NO;
			break;
	}
			
	switch(zParty)
	{
		case FIRST_PARTY:
			break;

		default:
			errors ++;
			sprintf(m, "Invalid Party parameter: %s. "
				"Valid values: FIRST_PARTY.", arc_val_tostr(zParty));
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, m);
			break;
	}

	if((*zpTotalTime < 1) ||
	   (*zpTotalTime > MAX_TOTAL_TIME))
	{
		sprintf(m, "Record time is %d. Valid values are 1 - %d. Using %d.", 
			*zpTotalTime, MAX_TOTAL_TIME, MAX_TOTAL_TIME);
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
		*zpTotalTime = 60;
	}

	if((*zpLeadSilence < 0) || (*zpLeadSilence > MAX_LEAD_SILENCE))
	{
		sprintf(m, "Invalid lead_silence parameter: %d. Valid values are 0"
			"- %d. Using default %d.", *zpLeadSilence, MAX_LEAD_SILENCE, 
			lDefaultLeadSilence);
		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn, m);
		*zpLeadSilence = lDefaultLeadSilence;
	}

	if((*zpTrailSilence < 250) || (*zpTrailSilence > MAX_TRAIL_SILENCE))
	{
		sprintf(m, "Invalid trail_silence parameter: %d. Valid values are "
			"250 - %d. Using default %d.", *zpTrailSilence, MAX_TRAIL_SILENCE, 
			lDefaultTrailSilence);
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
		*zpTrailSilence = lDefaultTrailSilence;
	}

	switch(*zpBeep)
	{
		case YES:
		case NO:
			break;

		default:
			sprintf(m,
				"Invalid Beep parameter: %s. Valid values: YES, NO. "
				"Using default YES.", arc_val_tostr(*zpBeep));
			telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
			*zpBeep = YES;
			break;
	}

	switch(*zTokenTypeRequested)
	{
		case SR_PATH_WORD:
		case SR_PATH_WORD_CONF:
		case SR_CONCEPT_WORD:
		case SR_CONCEPT_WORD_CONF:
		case SR_TAG:
		case SR_TAG_CONFIDENCE:
		case SR_CONCEPT:
		case SR_CONCEPT_CONFIDENCE:
		case SR_CATEGORY:
		case SR_ATTRIBUTE_VALUE:
		case SR_ATTRIBUTE_NAME:
		case SR_ATTRIBUTE_CONCEPTS:
		case SR_ATTRIBUTE_TYPE:
		case SR_ATTRIBUTE_STATUS:
		case SR_ATTRIBUTE_CONFIDENCE:
		case SR_CONCEPT_AND_ATTRIBUTE_PAIR:
		case SR_CONCEPT_AND_WORD:
		case SR_REC:
		case SR_LEARN:
			break;

		default:
			sprintf(m, "Must request at least 1 token type. Using SR_ALL as "
				"default. TokenTypeRequested = (%d)", *zTokenTypeRequested);
			telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
			*zTokenTypeRequested = SR_ALL;
			break;
	} // switch

	switch(*zpToneInterruptOption)
	{
		case YES:
		case NO:
			break;

		default:
			sprintf(m, "Invalid ToneInterruptOption option parameter: %s. "
			   "Valid values: YES, NO. Using default NO.", 
			   arc_val_tostr(*zpToneInterruptOption));
			telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn, m);
			*zpToneInterruptOption = NO;
			break;
	}

	return(errors == 0);   
}
