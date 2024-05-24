/*-----------------------------------------------------------------------------
Program:        TEL_Record.c
Purpose:        Record a phrase.
Author:         Wenzel-Joglekar
Date:		10/23/2000
?? NO MsgToDM
Update: 03/28/01 APJ Pass beep filename to DM instead of YES.
Update: 03/29/01 APJ Terminate recording on terminate char for INTERRUPT.
Update: 04/09/01 APJ Added compression validation.
Update: 04/25/01 APJ Checked Beep phrase existance if beep = YES.
Update: 04/27/2001 apj In parameter validation, put the missing value
		       passed in the log message.
Update: 05/02/2001 apj Do not save off the key that interrupted the recording.
Update: 05/04/2001 apj If filename is NULL, auto create it. 
Update: 05/04/2001 apj Make defaults for lead and trail silences as 3 seconds. 
Update: 05/04/2001 apj Make valid range for recordTime 1-1000 with default 60.
Update: 06/28/2001 apj Added GV_BeepFile Handling.
Update: 08/09/2001 apj Clear DTMFs in the beginning for interruptable recording.
Update: 08/09/2001 apj Throw away the key that interrupted the recording.
Update: 08/09/2001 apj in parametersAreGood,handle intrpt same as in TEL_Speak.
Update: 11/01/2001 apj Send interrupt option as a binary bit to DM.
Update: 01/14/2002 apj Using compression COMP_WAV for dialogic compressions.
Update: 10/07/2003 apj Put the message "file already there but overwrite is NO" 
Update: 10/10/2003 apj memset the msg structure first.
Update: 12/09/2003 apj Populate GV_RecordTermChar.
Update: 05/14/2004 apj Record term char, do not do atoi on message[0].
Update: 06/16/2004 apj Look for a key in RECORD_TERMINATE_KEY.
Update: 08/27/2004 ddn Sleep for 50 ms after sending STOPACTIVITY
Update: 11/21/2005 ddn added APPEND
------------------------------------------------------------------------------*/
#include "TEL_Common.h"


extern int get_uniq_filename(char  *filename);

static int parametersAreGood(char *mod, int party, char *filename, 
			int *recordTime, int *compression, int *overwrite, 
			int *lead_silence, int *trail_silence, int beep, 
			int *interruptOption, char terminate_char, 
			int synchronous);

int TEL_Record(int party, char *filename, int recordTime,
                        int compression, int overwrite, int lead_silence,
                        int trail_silence, int beep, int interrupt_option,
                        char terminate_char, int synchronous)
{
	static char mod[]="TEL_Record";
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s(%s,%s,%d,%s,%s,%d,%d,%s,%s,%c,%s)";
	int  rc = 0;
	int  requestSent = 0;
	
	struct MsgToApp response;
	struct Msg_Record msg, msg1;
	char lBeepFileName[256] = "";
	char *typeAheadBuffer;
	char partyVal[255] = "";
	char compressionVal[32] = "";
	char overwriteVal[32] = "";
	char beepVal[32] = "";
	char interrupt_optionVal[32] = "";
	char synchronousVal[32] = "";
	
#if 0
	snprintf(partyVal, sizeof(partyVal), "%s", "undefined");
	sprintf(compressionVal, "%s", arc_val_tostr(compression));
	sprintf(overwriteVal, "%s", arc_val_tostr(overwrite));
	sprintf(beepVal, "%s", arc_val_tostr(beep));
	sprintf(interrupt_optionVal, "%s", arc_val_tostr(interrupt_option));
	sprintf(synchronousVal, "%s", arc_val_tostr(synchronous));
#endif

	GV_RecordTermChar = (int)' ';

#if 1
   	sprintf(apiAndParms,apiAndParmsFormat, mod, "test",
		filename, recordTime, arc_val_tostr(compression), arc_val_tostr(overwrite), 
		lead_silence, trail_silence, arc_val_tostr(beep),
		arc_val_tostr(interrupt_option), terminate_char, arc_val_tostr(synchronous));
#endif

#if 0
   	sprintf(apiAndParms,apiAndParmsFormat, mod, partyVal,
		filename, recordTime, compressionVal, overwriteVal, 
		lead_silence, trail_silence, beepVal,
		interrupt_optionVal, terminate_char, synchronousVal);
#endif

	/* Do we need to account for party here ?? */
	rc = apiInit(mod, TEL_RECORD, apiAndParms, 1, 1, 0);
	if (rc != 0) HANDLE_RETURN(rc);

	if (!parametersAreGood(mod, party, filename, &recordTime,
			&compression, &overwrite, &lead_silence,
			&trail_silence, beep, &interrupt_option,
			terminate_char, synchronous))
	HANDLE_RETURN(TEL_FAILURE);
        
	memset(&msg, 0, sizeof(struct Msg_Record));
	msg.opcode = DMOP_RECORD;
	if (party == FIRST_PARTY)
	{	
        	msg.appCallNum = GV_AppCallNum1;
	}
	else
	{
        	msg.appCallNum = GV_AppCallNum2;
	}
	
	if(interrupt_option != NONINTERRUPT)
        msg.interrupt_option = 1;
	else
        msg.interrupt_option = 0;

  	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;

	msg.party = party;
	strcpy(msg.filename, filename);
	msg.record_time = recordTime;
	msg.compression = compression;
	msg.overwrite = overwrite;
	msg.lead_silence = lead_silence;
	msg.trail_silence = trail_silence;
	msg.terminate_char=terminate_char;
	msg.synchronous = synchronous;

	if(beep == YES)
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
			sprintf(msg.beepFile, "%s", lBeepFileName); 
                }
                else
                {
			telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
                        "Beep phrase (%s) doesn't exist.  [%d, %s] Setting "
			" beep to NO.", lBeepFileName, errno);
			beep = NO;
			memset(msg.beepFile, 0, sizeof(msg.beepFile));
                }
	}

	if(interrupt_option != NONINTERRUPT) 
	{	
	msg1.opcode=DMOP_GETDTMF;
        msg1.appPid 	= GV_AppPid;
        msg1.appPassword = 0;
	if (party == FIRST_PARTY)
	{
		typeAheadBuffer = GV_TypeAheadBuffer1;
		msg1.appCallNum=GV_AppCallNum1;
	}
	else
	{
		typeAheadBuffer = GV_TypeAheadBuffer2;
		msg1.appCallNum=GV_AppCallNum2;
	}

	while(1)
	{
	rc = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
	if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
	if(rc == -2) break;

	rc = examineDynMgrResponse(mod, (struct MsgToDM *)&msg1, &response);	
	switch(rc)
	{
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, msg1.appCallNum, "CLEAR");
			break;
		case DMOP_DISCONNECT:
			HANDLE_RETURN(disc(mod, response.appCallNum));
			break;
		default:
			/* Unexpected msg. Logged in examineDynMgr.. */
			break;
	} /* switch rc */
	}
	}

	requestSent=0;
	while ( 1 )
	{
		if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response,
				sizeof(response));
			if (rc == -1) HANDLE_RETURN(TEL_FAILURE);
			if (rc == -2)
			{
				rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&msg);
				if (rc == -1) HANDLE_RETURN(TEL_FAILURE);
				requestSent=1;
				rc = readResponseFromDynMgr(mod,0,
						&response,sizeof(response));
				if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,0,
				&response,sizeof(response));
                       	if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		}
		
		rc = examineDynMgrResponse(mod, (struct MsgToDM *)&msg, &response);	
		switch(rc)
		{
		case DMOP_RECORD:
			if ( interrupt_option != NONINTERRUPT ) 
			{
				if((terminate_char == ' ') || 
				   (terminate_char == '\0') ||
				   (terminate_char == response.message[0]) ||
				   (strchr(GV_RecordTerminateKeys, response.message[0])!=NULL))
				{
					GV_RecordTermChar = response.message[0];
				}
			}

			saveTypeAheadData(mod, msg.appCallNum, "CLEAR");
			HANDLE_RETURN(response.returnCode);
			break;
		case DMOP_GETDTMF:
			if (response.appCallNum != msg.appCallNum)
			{
				/* Save data for the other leg of the call */
                saveTypeAheadData(mod, response.appCallNum,
							response.message);
				break;
			}
			if ( interrupt_option != NONINTERRUPT ) 
			{
				if((terminate_char == ' ') || 
				   (terminate_char == '\0') ||
				   (terminate_char == response.message[0]) ||
				   (strchr(GV_RecordTerminateKeys, response.message[0])!=NULL))
				{
					GV_RecordTermChar = response.message[0];

					msg.opcode=DMOP_STOPACTIVITY;
					sendRequestToDynMgr(mod, (struct MsgToDM *)&msg); 

					usleep(50);

					saveTypeAheadData(mod, msg.appCallNum, 
						"CLEAR");
					HANDLE_RETURN(TEL_SUCCESS);
				}
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
	
static int parametersAreGood(char *mod, int party, char *filename, 
			int *recordTime, int *compression, int *overwrite, 
			int *lead_silence, int *trail_silence, int beep, 
			int *interruptOption, char terminate_char, 
			int synchronous)
{
	int errors=0;
	char partyValues[]="FIRST_PARTY, SECOND_PARTY";
	int  recordTimeMin=1;
	int  recordTimeMax=1000;
	int  recordTimeForever=-1;
	int  defaultRecordTime = 60;
	int  defaultLeadSilence = 3;
	int  defaultTrailSilence = 3;
	char interruptOptionValues[]="INTERRUPT,NONINTERRUPT, FIRST_PARTY_INTERRUPT, SECOND_PARTY_INTERRUPT";
        char overwriteValues[]="YES, NO, APPEND";   
        char synchronousValues[]="SYNC, ASYNC";   
        char beepValues[]="YES, NO";   
	char termCharValues[]="0-9, *, #";
	char tempBuf[80];
        
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

	if ((*recordTime > recordTimeMax) || 
	    (*recordTime < recordTimeMin && *recordTime != recordTimeForever))
	{
               telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"Invalid value for recordTime: %d. Valid values are %d to %d or %d (record until disconnect). Using default %d.",
		*recordTime, recordTimeMin, recordTimeMax, 
		recordTimeForever, defaultRecordTime);
		*recordTime = defaultRecordTime;
	} 

	if(*lead_silence <= 0)
	{
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
			"Invalid lead_silence parameter: %d. Must be greater "
			"than 0. Using default %d.",
			*lead_silence, defaultLeadSilence);
		*lead_silence = defaultLeadSilence;
	}

	if(*trail_silence <= 0)
	{
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
			"Invalid trail_silence parameter: %d. Must be greater "
			"than 0. Using default %d.",
			*trail_silence, defaultTrailSilence);
		*trail_silence = defaultTrailSilence;
	}

	switch(*compression)
	{
		case COMP_24ADPCM:
		case COMP_32ADPCM:
		case COMP_48PCM:
		case COMP_64PCM:
		case COMP_MVP:
   			telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
				"Invalid value for compression: %s. Only COMP_WAV is supported."
				"Using compression COMP_WAV.", arc_val_tostr(*compression));
			*compression = COMP_WAV;
			break;

		case COMP_WAV:
			break;

		default:
			errors++;
   			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"Invalid value for compression: %d. Only COMP_WAV "
				"is supported.", *compression);
			break;
	}
	
	switch(beep)
	{
		case YES:
		case NO:
			break;
		default:
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid value for beep: %d. Valid values are %s.", 
			beep, beepValues);
	}

	switch(terminate_char)
	{
		case '0': case '1': case '2': case '3': case '4': 
		case '5': case '6': case '7': case '8': case '9': 
		case '#': case '*': case '\0': case ' ':
			break;
	default:
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
	      		"Invalid value for termChar: %d. Valid values are %s.",
			terminate_char, termCharValues);
	}

	if(*filename == '\0')
	{
		memset(tempBuf, 0, sizeof(tempBuf));
		get_uniq_filename(tempBuf);
		sprintf(filename, "%s.wav", tempBuf);
		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Warn,
			"Parameter filename is NULL. Using autocreated "
			"filename (%s).", filename); 
	}
	
	switch(*overwrite)
        {
        case YES:
                break;

		case APPEND:
			if(access(filename, F_OK) != 0)
			{
				*overwrite = YES;
			}
			break;

        case NO:
		if(access(filename, F_OK) == 0)
		{
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"File (%s) exists and overwrite=NO. Cannot overwrite file.",
				filename);
		}
		break;
        default:
                errors++;
                telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
                	"Invalid value for overwrite: %d. Valid values are %s.",
                        *overwrite, overwriteValues);
                break;
        }
	
	switch(*interruptOption)
        {
       	case INTERRUPT:
		switch(party)
		{
		case FIRST_PARTY:
               		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
				"Invalid interrupt_option: INTERRUPT. "
                        	"Using FIRST_PARTY_INTERRUPT.");
                        *interruptOption = FIRST_PARTY_INTERRUPT;
			break;

		case SECOND_PARTY:
		case SECOND_PARTY_NO_CALLER: 
               		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
				"Invalid interrupt_option: INTERRUPT. "
                        	"Using SECOND_PARTY_INTERRUPT.");
                        *interruptOption = SECOND_PARTY_INTERRUPT;
			break;
		
		case BOTH_PARTIES:	
               		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
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
               		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
                        "Invalid interrupt_option: FIRST_PARTY_INTERRUPT "
                     	"when party is SECOND_PARTY or SECOND_PARTY_NO_CALLER. "
                        "Using SECOND_PARTY_INTERRUPT.");
                        *interruptOption = SECOND_PARTY_INTERRUPT;
                }
		break;

	case SECOND_PARTY_INTERRUPT:
                if(party == FIRST_PARTY)
                {
               		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
                        "Invalid interrupt_option: SECOND_PARTY_INTERRUPT "
                     	"when party is FIRST_PARTY. "
                        "Using FIRST_PARTY_INTERRUPT.");
                        *interruptOption = FIRST_PARTY_INTERRUPT;
                }
		break;

	default:
		errors++;
               	telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid value for interruptOption: %d. "
			"Valid values are %s", 
			*interruptOption, interruptOptionValues);
		break;
	} 

	switch(synchronous)
	{
	case SYNC:
	case ASYNC:
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

