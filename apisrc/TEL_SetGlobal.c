/*-----------------------------------------------------------------------------
Program:	TEL_SetGlobal.c
Purpose:	Set Global variables and change channel parameters.
Author: 	Wenzel-Joglekar
Date:		10/03/00
Update:		10/03/00 gpw converted for IP-IVR
Need apiInit here ??
Update: 051601 APJ Added support for AGENT_DISCONNECT.
Update: 071301 APJ Write error message for SECONDS_FROM_ACCEPTANCE and
		   SECONDS_FROM_CONNECT.
Update: 071801 APJ Added support for DELIMITER.
Update: 03/04/2004 APJ When GV_Language changes, change it's dependant variables
Update: 05/17/2004 APJ Added SR_DTMF_ONLY.
Update: 05/21/2004 APJ Added TIMEOUT & RETRY(for backward compatibility RUSS).
Update: 07/08/2004 APJ Added Call Progress functionality.
Update: 09/15/04 APJ Return on disconnected call.
Update:     11/10/2004 apj Save DTMFs in the buffer.
Update: 11/21/2005 ddn added LAST_STREAM_DURATION
Update: 03/21/2007 ddn added MAX_CALL_DURATION
Update: 10/09/2013 djb added LAST_PLAY_POSITION
Update: 09/15/15 djb Added $HIDE_DTMF
Update: 10/13/17 djb MR 4877 - corrected
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

#define	INTERVAL_MAX	1440	/* 1 day for Normal heartbeats. */
#define	LIST_TERMINATOR	 "NULL"

/* valid set of global variables to be used in api */
static char *vars[] = {
			"$PHR_OFFSET", 			
			"$RETRY",
			"$TIMEOUT",			
			"$DELIMITER",
			"$VOLUME", 			
			"$BEEP", 						// 5
			"$NOTIFY_CALLER_INTERVAL",	
			"$CALLING_PRESENTATION",
			"$DIAG_ENCRYPT", 		
			"$CALLING_SCREENING",
			"$ANSWER_TIMEOUT",				// 10
			"$INPHR_DTMF",
			"$TERM_KEY", 			
			"$LANGUAGE",
			"$LEAD_SILENCE", 		
			"$TRAIL_SILENCE",				// 15
			"$COLLECT_ANI",			
			"$PULSE_DIALING",
			"$TONE_DETECTION",		
			"$DISCONNECT_DETECTION",
			"$FIRST_DIGIT_TIMEOUT",         // 20
			"$INTER_DIGIT_TIMEOUT",
			"$TOTAL_INPUT_TIMEOUT", 	
			"$AGENT_DISCONNECT",
			"$SECONDS_FROM_ACCEPTANCE", 	
			"$SECONDS_FROM_CONNECT",		// 25
			"$KILL_APP_GRACE_PERIOD",       
			"$DEFAULT_COMPRESSION",
			"$AGENT_COMMUNICATION", 
			"$OUTBOUND_RET_CODE",
			"$CALL_PROGRESS",				// 30
			"$SPEED",
			"$PLAY_BACK_BEEP_INTERVAL",
			"$MAX_PAUSE_TIME",
			"$FAX_HDR_FMT",
			"$LAST_STREAM_DURATION",		// 35
			"$SR_DTMF_ONLY",
			"$MAX_CALL_DURATION",
			"$FAX_DEBUG",
			"$FLUSH_DTMF_BUFFER",
			"$LAST_PLAY_POSITION",			// 40
			"$HIDE_DTMF",
			"$SR_TYPE",
			"$GOOGLE_RECORD",
			LIST_TERMINATOR /* Must be last item in the list. */ 	
			};

/*------------------------------------------------------------------------------

------------------------------------------------------------------------------*/
int TEL_SetGlobal(const char *var_name, int var_value)
{
static char mod[]="TEL_SetGlobal";
int	index = 0;
int	rc;
char	tmpGV_Language[64];
int	tmpGV_LanguageNumber;
struct Msg_SetGlobal msg;
struct MsgToApp response;
int  requestSent;
int KillAppGracePeriodMin=1;
int KillAppGracePeriodMax=1000;
char charValue[32] = "";

sprintf(LAST_API,"%s",mod);

/* Check to see if InitTelecom API has been called. */
if(GV_InitCalled != 1)
{
	telVarLog(mod,REPORT_NORMAL,TEL_TELECOM_NOT_INIT, GV_Err,
		TEL_TELECOM_NOT_INIT_MSG);
    	HANDLE_RETURN (-1);
}

if(GV_IsCallDisconnected1 == 1)
{
	telVarLog(mod, REPORT_DETAIL, TEL_BASE, GV_Err,
		"Party 1 is disconnected.");
	HANDLE_RETURN(TEL_DISCONNECTED);
}

msg.opcode = DMOP_SETGLOBAL;
msg.appCallNum = GV_AppCallNum1;
msg.appPid = GV_AppPid;
msg.appRef = GV_AppRef++;
msg.appPassword = GV_AppPassword;
memset(msg.name, 0, sizeof(msg.name));
msg.value = var_value;

index=0;
while((strcmp(vars[index], LIST_TERMINATOR)) != 0)
{
	/* If we find the item we are looking for, exit the loop. */
    	if (strcmp(var_name, vars[index]) == 0) break;	
	index++;
}

switch(index)
{
    	case 0: 	/*  $PHR_OFFSET */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$PHR_OFFSET is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 1: 	/* $RETRY */
			GV_Retry = var_value;
			HANDLE_RETURN(0);
			break;

    	case 2: 	/* $TIMEOUT */
			GV_Timeout = var_value;
			HANDLE_RETURN(0);
			break;

    	case 3: 	/*  $DELIMITER */
			GV_IndexDelimit = (char)var_value;
			HANDLE_RETURN(0);
			break;

    	case 4: 	/* $VOLUME */
			sprintf(msg.name, "%s", "$VOLUME");
			break;

    	case 5:		/* $BEEP */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$BEEP is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 6: 	/* $NOTIFY_CALLER_INTERVAL */
			if (var_value < 400 || var_value > 1000000)
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM, GV_Err,
					"Invalid value for $NOTIFY_CALLER_INTERVAL: %d. "
					"Valid values are 400 to 1000000.");
				HANDLE_RETURN(-1);                                      
			}
			GV_NotifyCallerInterval = var_value;	
			HANDLE_RETURN(0);
			break;

    	case 7: 	/* $CALLING_PRESENTATION */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$CALLING_PRESENTATION is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 8:		/* $DIAG_ENCRYPT */
			if(var_value == 0)
				GV_HideData = 0;
			else if(var_value == 1)
				GV_HideData = 1;
			else
			{
    			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
					"Invalid value for $DIAG_ENCRYPT: %d. Valid "
					"values are 0 and 1.", var_value);
				HANDLE_RETURN(-1);
			}
			HANDLE_RETURN(0);
			break;

    	case 9:		/* $CALLING_SCREENING */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$CALLING_SCREENING is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 10:	/* $ANSWER_TIMEOUT */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$ANSWER_TIMEOUT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 11: 	/* $INPHR_DTMF */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$INPHR_DTMF is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 12: 	/* $TERM_KEY */ 
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$TERM_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 13:	/* $LANGUAGE */
			/* Note: change only 'temporary' variables until we are sure */
			tmpGV_LanguageNumber = var_value;	
			switch(var_value)
			{
				case	S_AMENG :
					sprintf(tmpGV_Language, "%s", "AMENG"); break;
				case	S_FRENCH :
					sprintf(tmpGV_Language, "%s", "FRENCH"); break;
				case	S_GERMAN :
					sprintf(tmpGV_Language, "%s", "GERMAN"); break;
				case	S_DUTCH :
					sprintf(tmpGV_Language, "%s", "DUTCH"); break;
				case	S_SPANISH :
					sprintf(tmpGV_Language, "%s", "SPANISH"); break;
				case	S_FLEMISH : 
					sprintf(tmpGV_Language, "%s", "FLEMISH"); break; 
				case	S_BRITISH :
					sprintf(tmpGV_Language, "%s", "BRITISH"); break; 
				default : 
					if (var_value < S_AMENG) 
					{
						/* Do we want to support Flemish ? */
    					telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
							"Invalid language parameter: %d. Valid values are: "
							"S_AMENG, S_FRENCH, S_GERMAN, S_DUTCH, S_SPANISH, "
							"S_FLEMISH, S_BRITISH, or a value greater than 20", 
							var_value); 
	  					HANDLE_RETURN(-1);
					}
				/* Create a "custom" language using the # passed. */
				sprintf(tmpGV_Language,"LANG%d", var_value);
				tmpGV_LanguageNumber=var_value;	
				break;
			} /* switch */

			sprintf(GV_Language, "%s", tmpGV_Language);
			GV_LanguageNumber = tmpGV_LanguageNumber;

			sprintf(GV_SystemPhraseDir, "%s/%s", GV_SystemPhraseRoot,
				GV_Language);
			get_first_phrase_extension(mod, GV_SystemPhraseDir,
				GV_FirstSystemPhraseExt);

			HANDLE_RETURN(0);
			break;

    	case 14:	/* $LEAD_SILENCE */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$LEAD_SILENCE is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 15:	/* $TRAIL_SILENCE */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$TRAIL_SILENCE is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 16:	/* $COLLECT_ANI */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$COLLECT_ANI is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 17:	/* $PULSE_DIALING */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$PULSE_DIALING is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 18:	/* $TONE_DETECTION */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$TONE_DETECTION is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 19:	/* $DISCONNECT_DETECTION */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$DISCONNECT_DETECTION is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 20:	/* $FIRST_DIGIT_TIMEOUT */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$FIRST_DIGIT_TIMEOUT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 21:	/* $INTER_DIGIT_TIMEOUT */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$INTER_DIGIT_TIMEOUT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 22:	/* $TOTAL_INPUT_TIMEOUT */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$TOTAL_INPUT_TIMEOUT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 23:	/* $AGENT_DISCONNECT */
			if((var_value != YES) && (var_value != NO))
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for AGENT_DISCONNECT: %d. Valid values are "
					"YES and NO.", var_value);
				HANDLE_RETURN(-1);          
			}
			GV_AgentDisct = var_value;
			HANDLE_RETURN(0);
			break;

		case 24:	/* $SECONDS_FROM_ACCEPTANCE */
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        		"$SECONDS_FROM_ACCEPTANCE is read-only and cannot be "
				"set to %d.", var_value);
			HANDLE_RETURN(-1);          
			break;

		case 25:	/* $SECONDS_FROM_CONNECT */
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        		"$SECONDS_FROM_CONNECT is read-only and cannot be "
				"set to %d.", var_value);
			HANDLE_RETURN(-1);          
			break;

		case 26:	/* $KILL_APP_GRACE_PERIOD */
			sprintf(msg.name, "%s", "$KILL_APP_GRACE_PERIOD");
			if (var_value < KillAppGracePeriodMin || 
				var_value > KillAppGracePeriodMax)
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for grace period: %d. Valid values are %d "
					"to %d.", var_value, KillAppGracePeriodMin, 
					KillAppGracePeriodMax);
				HANDLE_RETURN(-1);          
			}
			break;

		

		case 27:	/* $DEFAULT_COMPRESSION */
			if(var_value == COMP_WAV || var_value == COMP_MVP || var_value == COMP_64PCM ||
				(var_value >= COMP_711 && var_value <= COMP_729BR_H263R_QCIF ))
			{
				HANDLE_RETURN(0);
			}
			else
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for $DEFAULT_COMPRESSION : %d.");
				HANDLE_RETURN(-1);          
			}
			break;
		
#if 0
		case 27:	/* $DEFAULT_COMPRESSION */

			if(var_value == COMP_MVP)
			{
				var_value = COMP_WAV;
			}

			if(var_value != COMP_WAV)
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for $DEFAULT_COMPRESSION : %d. Only"
					" COMP_WAV is supported.");
				HANDLE_RETURN(-1);          
			}
			HANDLE_RETURN(0);          
			break;
#endif


		case 28:	/* $AGENT_COMMUNICATION */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$AGENT_COMMUNICATION is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 30:	/* $CALL_PROGRESS */
			sprintf(msg.name, "%s", "$CALL_PROGRESS");
			if((var_value != YES) && (var_value != NO) &&
				(var_value != ENABLE) && (var_value != DISABLE) &&
				(var_value != 0) && (var_value != 1) && (var_value != 2) &&
				(var_value != 3) && (var_value != 4))
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for $CALL_PROGRESS : %d. Valid values are "
					"YES, NO, ENABLE, DISABLE, 0, 1, 2, 3, 4.", var_value);
				HANDLE_RETURN(-1);
			}
			break;

		case 31: 	/* $SPEED */
			sprintf(msg.name, "%s", "$SPEED");
			if((var_value < -10) || (var_value > 10))
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for $SPEED : %d. Valid values are "
					"-10 to 10.", var_value);
				HANDLE_RETURN(-1);
			}
#if 0
			//RGDEBUG for ORCC
			if(var_value < -5)
			{
				var_value = -5;
			}
			else if(var_value > 5)
			{
				var_value = 5;
			}
			var_value += 5;
#endif
			break;

		case 32: 	/* $PLAY_BACK_BEEP_INTERVAL */
			sprintf(msg.name, "%s", "$PLAY_BACK_BEEP_INTERVAL");
			if((var_value <= 0))
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for $PLAY_BACK_BEEP_INTERVAL : %d. "
					"Valid values are 1 and up.", var_value);
				HANDLE_RETURN(-1);
			}
			break;

		case 33: 	/* $MAX_PAUSE_TIME */
			sprintf(msg.name, "%s", "$MAX_PAUSE_TIME");
			if(var_value < 0 || var_value > 150)
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Warn,
					"Invalid value for $MAX_PAUSE_TIME : %d. "
					"Valid values are from 0 to 150. Using default as 120.",
					var_value);
				var_value = 120;
			}
			break;

		case 34: 	/* $FAX_HDR_FMT */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$FAX_HDR_FMT is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 35: 	/* $LAST_STREAM_DURATION */

			sprintf(msg.name, "%s", "$LAST_STREAM_DURATION");
			if(var_value != -1)
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Warn,
					"Invalid value for $LAST_STREAM_DURATION : %d. "
					"Valid value is -1. Using default as -1.",
					var_value);
				var_value = -1;
			}
			break;

		case 36:	/* $SR_DTMF_ONLY */
			sprintf(msg.name, "%s", "$SR_DTMF_ONLY");
			if((var_value != YES) && (var_value != NO))
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for SR_DTMF_ONLY: %d. Valid values are "
					"YES and NO.", var_value);
				HANDLE_RETURN(-1);          
			}
			break;

		case 37:	/* $MAX_CALL_DURATION */
			sprintf(msg.name, "%s", "$MAX_CALL_DURATION");
			if(var_value < 0)
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for MAX_CALL_DURATION: %d. Must be >= 0.",
					var_value);
				HANDLE_RETURN(-1);          
			}
			break;

		case 38:	/* $FAX_DEBUG */
			sprintf(msg.name, "%s", "$FAX_DEBUG");
			if( (var_value != 0) && (var_value != 1) )
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for FAX_DEBUG: %d. Must be either 0 or 1.",
					var_value);
				HANDLE_RETURN(-1);          
			}
			break;

		case 39:	/* $FLUSH_DTMF_BUFFER */
			sprintf(msg.name, "%s", "$FLUSH_DTMF_BUFFER");
			if( (var_value != 0) && (var_value != 1) )
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for FLUSH_DTMF_BUFFER: %d. Must be either 0 or 1.",
					var_value);
				HANDLE_RETURN(-1);          
			}

			break;

		case 40:	/* $LAST_PLAY_POSITION */
			sprintf(msg.name, "%s", "$LAST_PLAY_POSITION");
			if( (var_value != 0) && (var_value != 1) )
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for LAST_PLAY_POSITION: %d. Must be either 0 or 1.",
					var_value);
				HANDLE_RETURN(-1);          
			}

        case 41:    /* $HIDE_DTMF */
            sprintf(msg.name, "%s", "$HIDE_DTMF");
            if( (var_value != 0) && (var_value != 1) )
            {
                telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
                    "Invalid value for HIDE_DTMF: %d. Must be either 0 or 1.",
                    var_value);
                HANDLE_RETURN(-1);
            }
            GV_HideDTMF = var_value;
            sprintf(charValue, "%d", var_value);

			break;
        case 42:    /* $SR_TYPE */
            sprintf(msg.name, "%s", "$SR_TYPE");
            if( (var_value != GOOGLE_SR) && (var_value != MS_SR) )
            {
                telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
                    "Invalid value for SR_TYPE: %d. Must be either GOOGLE_SR(%d) or MS_SR(%d).",
                    var_value, GOOGLE_SR, MS_SR);
                HANDLE_RETURN(-1);
            }
            GV_SRType = var_value;
            sprintf(charValue, "%d", var_value);

			break;

        case 43:    /* $GOOGLE_RECORD */
            sprintf(msg.name, "%s", "$GOOGLE_RECORD");
			if((var_value != YES) && (var_value != NO))
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
					"Invalid value for $GOOGLE_RECORD : %d. Valid values are "
					"YES, NO. Setting to NO (%d)", var_value, NO);
				HANDLE_RETURN(-1);
			}

			GV_GoogleRecord = var_value;
			sprintf(charValue, "%d", var_value);

			break;

    	default:
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
				"[%s, %d] Invalid variable name (%s) received, "
				"returning failure.",  __FILE__, __LINE__,
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
				rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&msg);
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
				
		rc = examineDynMgrResponse(mod, (struct MsgToDM *)&msg, &response);	
		switch(rc)
		{
			case DMOP_SETGLOBAL:
                if ( index == 41 )  // HIDE_DTMF
                {
					int retval = 0;
					extern int  GV_SRFifoFd;

					if ( GV_SRFifoFd == -1 )
					{
	                    (void)TEL_SRInit("", &retval);
						(void)TEL_SRReserveResource(&retval);
						retval = 1;
					}

                    rc = TEL_SRSetParameter("$HIDE_DTMF", charValue);
					if ( retval == 1 )
					{
						(void)TEL_SRReleaseResource(&retval);
					}
                }
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
