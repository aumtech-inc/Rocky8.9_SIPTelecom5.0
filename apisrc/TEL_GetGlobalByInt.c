/*-----------------------------------------------------------------------------
Program:	TEL_GetGlobal.c
Purpose:	To get the value of an integer global variable, given a 
		variable name.
Author: 	Aumtech, Inc
Date:		10/03/00	ddn created the file
----------------------------------------------------------------------------- */
#include "TEL_Common.h"

static char ModuleName[]="TEL_GetGlobal";
#define	LIST_TERMINATOR 	"NULL"

/* valid set of global variables */
static	int	vars[] = {
			 $PHR_OFFSET ,			
			 $RETRY ,
			 $TIMEOUT ,			
			 $DELIMITER ,
			 $VOLUME ,			
			 $BEEP ,
			 $NOTIFY_CALLER_INTERVAL ,	
			 $CALLING_PRESENTATION ,
			 $DIAG_ENCRYPT , 		
			 $CALLING_SCREENING ,
			 $ANSWER_TIMEOUT ,		
			 $INPHR_DTMF ,
			 $TERM_KEY ,			
			 $LANGUAGE ,
			 $LEAD_SILENCE , 		
			 $TRAIL_SILENCE ,
			 $COLLECT_ANI ,			
			 $PULSE_DIALING ,
			 $TONE_DETECTION ,		
			 $DISCONNECT_DETECTION ,
			 $FIRST_DIGIT_TIMEOUT ,		
			 $INTER_DIGIT_TIMEOUT ,
			 $TOTAL_INPUT_TIMEOUT ,		
			 $AGENT_DISCONNECT ,
			 $SECONDS_FROM_ACCEPTANCE ,	
			 $SECONDS_FROM_CONNECT ,
			 $KILL_APP_GRACE_PERIOD ,       
			 $DEFAULT_COMPRESSION ,
			 $AGENT_COMMUNICATION , 
			 $OUTBOUND_RET_CODE ,
			 $CALL_PROGRESS , 
			 $BANDWIDTH ,
			 $SPEED ,
			 $BRIDGE_PORT ,
			 $RECORD_TERM_CHAR ,
			 $PLAY_BACK_BEEP_INTERVAL ,
			 $MAX_PAUSE_TIME ,
			 $FAX_HDR_FMT ,
			 $LAST_PROMPT_DURATION ,
			 $RELEASE_CAUSE_CODE ,
			 $LAST_STREAM_DURATION ,
			 $SR_DTMF_ONLY ,
			 $AUDIO_SAMPLING_RATE ,
			 $PLAY_BACK_BEEP_INTERVAL ,
			 $MAX_PAUSE_TIME ,
			 LIST_TERMINATOR /* Must be last item in list. */
			};

/*------------------------------------------------------------------------------
This routine gets the value for the noun in the noun table. Noun # is 
the index of the noun in the "nouns" static vector. 
------------------------------------------------------------------------------*/
int TEL_GetGlobalFromInt(int var_name, int *var_value)
{
	static char mod[]="TEL_GetGlobalFromInt";
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s(%d,&value)";
	int	index = 0;
	int rc;
	time_t	now;
	struct Msg_GetGlobal msg;
	struct MsgToApp response;
	int  requestSent;

	*var_value = 0;

	sprintf(apiAndParms,apiAndParmsFormat, mod, var_name);
	rc = apiInit(mod, TEL_GETGLOBAL, apiAndParms, 1, 0, 0);
	if (rc != 0) HANDLE_RETURN(rc);

	msg.opcode = DMOP_GETGLOBALBYINT;
	msg.appCallNum = GV_AppCallNum1;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;
	memset(msg.name, 0, sizeof(msg.name));

	switch(var_name)
	{
    	case 0: 		/* $PHR_OFFSET */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$PHR_OFFSET is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 1: 		/* $RETRY */
			*var_value = GV_Retry;
			HANDLE_RETURN(0);
			break;

    	case 2: 		/* $TIMEOUT */
			*var_value = GV_Timeout;
			HANDLE_RETURN(0);
			break;

    	case 3: 		/* $DELIMITER */
			*var_value = (int)GV_IndexDelimit;
			HANDLE_RETURN(0);
			break;

    	case 4: 		/* $VOLUME */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$VOLUME is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 5: 		/* $BEEP */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$BEEP is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 6:		/* $NOTIFY_CALLER_INTERVAL */
			*var_value = GV_NotifyCallerInterval;
			HANDLE_RETURN(0);
			break;

    	case 7:		/* $CALLING_PRESENTATION */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$CALLING_PRESENTATION is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 8:		/* $DIAG_ENCRYPT */
			*var_value = GV_HideData;
			HANDLE_RETURN(0);
			break;

    	case 9:		/* $CALLING_SCREENING */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$CALLING_SCREENING is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 10:	/* $ANSWER_TIMEOUT */ 
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$ANSWER_TIMEOUT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 11:	/* $INPHR_DTMF */ 
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$INPHR_DTMF is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 12: 	/* $TERM_KEY */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$TERM_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 13:	/* Get $LANGUAGE */
			*var_value = GV_LanguageNumber ;
			HANDLE_RETURN(0);
			break;

    	case 14:	/* $LEAD_SILENCE */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$LEAD_SILENCE is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 15:	/* $TRAIL_SILENCE */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$TRAIL_SILENCE is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 16:        /* $COLLECT_ANI */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$COLLECT_ANI is not supported. Ignoring.");
			HANDLE_RETURN(0);
        	break;

		case 17:        /* $PULSE_DIALING */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$PULSE_DIALING is not supported. Ignoring.");
			HANDLE_RETURN(0);
       		break;

		case 18:        /* $TONE_DETECTION */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$TONE_DETECTION is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;                                                   

		case 19:        /* $DISCONNECT_DETECTION */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$DISCONNECT_DETECTION is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 20: 	/* $FIRST_DIGIT_TIMEOUT */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$FIRST_DIGIT_TIMEOUT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 21: 	/* $INTER_DIGIT_TIMEOUT */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$INTER_DIGIT_TIMEOUT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 22: 	/* $TOTAL_INPUT_TIMEOUT */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$TOTAL_INPUT_TIMEOUT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 23: 	/* $AGENT_DISCONNECT */
			*var_value = GV_AgentDisct;
			HANDLE_RETURN(0);
			break;

    	case 24: 	/* $SECONDS_FROM_ACCEPTANCE */
			time(&now);
			*var_value = now - GV_RingEventTimeSec;
			HANDLE_RETURN(0);
			break;

    	case 25: 	/* $SECONDS_FROM_CONNECT */
			time(&now);
			*var_value = now - GV_ConnectTimeSec; 
			HANDLE_RETURN(0);
			break;

    	case 26: 	/* $KILL_APP_GRACE_PERIOD */
			sprintf(msg.name, "%s", "$KILL_APP_GRACE_PERIOD");
			break;

    	case 27:	/* $DEFAULT_COMPRESSION */ 
			*var_value = COMP_WAV;
			HANDLE_RETURN(0);
			break;

    	case 28:	/* $AGENT_COMMUNICATION */ 
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$AGENT_COMMUNICATION is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 29:	/* $OUTBOUND_RET_CODE */ 
			sprintf(msg.name, "%s", "$OUTBOUND_RET_CODE");
			break;

    	case 30:	/* $CALL_PROGRESS */ 
			sprintf(msg.name, "%s", "$CALL_PROGRESS");
			break;

    	case 31: 	/* $BANDWIDTH */
			*var_value = GV_BandWidth;
			HANDLE_RETURN(0);
			break;

		case 32: /* $SPEED */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$SPEED is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 33: /* $BRIDGE_PORT */
			sprintf(msg.name, "%s", "$BRIDGE_PORT");
			break;

		case 34: /* $RECORD_TERM_CHAR */
			*var_value = GV_RecordTermChar;
			HANDLE_RETURN(0);
			break;

		case 35: /* $PLAY_BACK_BEEP_INTERVAL */
			sprintf(msg.name, "%s", "$PLAY_BACK_BEEP_INTERVAL");
			break;

		case 36: /* $MAX_PAUSE_TIME */
			sprintf(msg.name, "%s", "$MAX_PAUSE_TIME");
			break;

		case 37: /* $FAX_HDR_FMT */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$FAX_HDR_FMT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 38: /* $LAST_PROMPT_DURATION */
			sprintf(msg.name, "%s", "$LAST_PROMPT_DURATION");
			break;

		case 39: /* $RELEASE_CAUSE_CODE */
			*var_value = GV_ReleaseCauseCode;
			HANDLE_RETURN(0);
			break;

		case 40: /* $LAST_STREAM_DURATION */
			sprintf(msg.name, "%s", "$LAST_STREAM_DURATION");
			break;

		case 41: /* $SR_DTMF_ONLY */
			sprintf(msg.name, "%s", "$SR_DTMF_ONLY");
			break;

		case 42: /* $AUDIO_SAMPLING_RATE */
			sprintf(msg.name, "%s", "$AUDIO_SAMPLING_RATE");
			break;

		case 43: /* $PLAY_BACK_BEEP_INTERVAL */
			sprintf(msg.name, "%s", "$PLAY_BACK_BEEP_INTERVAL");
			break;

		case 44: /* $MAX_PAUSE_TIME */
			sprintf(msg.name, "%s", "$MAX_PAUSE_TIME");
			break;

    	default:
			telVarLog(ModuleName,REPORT_NORMAL,TEL_INVALID_PARM, GV_Err,
				"Variable <%s> is not a defined global variable name.",
				var_name);
			HANDLE_RETURN(-1);
			break;
	} /* switch */

	requestSent=0;
	while(1)
	{
		if(!requestSent)
		{
			rc = readResponseFromDynMgr(mod, -1, &response, 
							sizeof(response));
			if (rc == -1) HANDLE_RETURN(TEL_FAILURE);
			if (rc == -2)
			{
				rc = sendRequestToDynMgr(mod, &msg);
				if (rc == -1) HANDLE_RETURN(TEL_FAILURE);
				requestSent=1;
				rc = readResponseFromDynMgr(mod, 0, &response, 
								sizeof(response));
				if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod, 0, &response, 
							sizeof(response));
       		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		}
				
		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
			case DMOP_GETGLOBAL:
				if(response.returnCode == 0)
					*var_value = atoi(response.message);					
				HANDLE_RETURN(response.returnCode);
				break;
	
			case DMOP_GETDTMF:
				saveTypeAheadData(mod, response.appCallNum, response.message);
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

	HANDLE_RETURN(0);
}
