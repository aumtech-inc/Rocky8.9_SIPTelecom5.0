static char ModuleName[]="TEL_GetGlobalString";
/*-----------------------------------------------------------------------------
Program:	TEL_GetGlobalString.c
Purpose:	Get the value of a global string.
		Note: This API has no interaction with the dynamic manager.
Author:		Wenzel-Joglekar
Date:		10/12/2000
?? Doesn't have apiInit call
Update: 04/25/01 APJ Removed PLAY_BACK_CONTROL settings.
Update: 07/10/01 APJ Added CUST_DATA1, CUST_DATA2.
Update: 04/09/04 APJ Added $H323_OUTBOUND_GATEWAY.
Update: 06/15/04 ddn Changed GV_CDR_Key to GV_CDRKey
Update: 06/16/04 APJ Added RECORD_TERMINATE_KEYS (GV_RecordTerminateKeys).
Update: 08/25/04 djb Changed sys_errlist strerror(errno).
Update: 09/15/04 APJ Return on disconnected call.
Update: 10/21/04 APJ Added PROTOCOL and PROTOCOL_VERSION support.
Update:     11/10/2004 apj Save DTMFs in the buffer.
Update: 06/13/05 DDN Added $SIP_OUTBOUND_GATEWAY.
Update: 09/26/05 DDN Added $SIP_URI_VOICEXML.
Update: 11/07/05 DDN Added $AUDIO_CODEC.
Update: 04/21/06 DDN Added $SIP_MESSAGE.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

#define	LIST_TERMINATOR		"NULL"

/* Valid set of global variables. */
static	char *vars[]={
			"$ANI",		
			"$DNIS",
			"$PORT_NAME",
			"$DEFAULT_LANGUAGE",
			"$USER_TO_USER_INFO",
			"$CALLING_PARTY",
			"$APP_TO_APP_INFO",
			"$DV_DIGIT_TYPE",
			"$APP_PHRASE_DIR",
			"$RING_EVENT_TIME",
			"$INIT_TIME",
			"$CONNECT_TIME",
			"$DISCONNECT_TIME",
			"$EXIT_TIME", 	
			"$INBAND_DIGITS", 	
			"$CUST_DATA1", 	
			"$CUST_DATA2", 	
			"$CALL_CDR_KEY",
			"$INBOUND_CALL_PARM",
			"$TRUNK_GROUP",
			"$VOLUME_UP_KEY",
			"$VOLUME_DOWN_KEY",
			"$SPEED_UP_KEY",
			"$SPEED_DOWN_KEY",
			"$SKIP_FORWARD_KEY",
			"$SKIP_BACKWARD_KEY",
			"$SKIP_TERMINATE_KEY",
			"$BEEP_FILE",
			"$OUTBOUND_CALL_PARM",
			"$PAUSE_KEY",
			"$RESUME_KEY",
			"$DTMF_BARGEIN_DIGITS",
			"$FAX_FROM_STR",
			"$FAX_USER_DEFINED_STR",
			"$SKIP_REWIND_KEY",
			"$H323_OUTBOUND_GATEWAY",
			"$RECORD_TERMINATE_KEYS",
			"$PROTOCOL",
			"$PROTOCOL_VERSION",
			"$SIP_OUTBOUND_GATEWAY",
			"$SIP_URI_VOICEXML",
			"$AUDIO_CODEC",
			"$PLAYBACK_PAUSE_GREETING_DIRECTORY",
			"$SIP_MESSAGE",
			LIST_TERMINATOR /* Must be last item in list */
	 	      };

int TEL_GetGlobalString(var_name, var_value)
const	char	*var_name;	
char	var_value[];	
{
	int	index = 0;
	int	ret_code;

	static const char mod[]="TEL_GetGlobalString";
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s()";
 
	struct Msg_GetGlobalString msg;
	struct MsgToApp response;
	int requestSent;

	sprintf(LAST_API,"%s",ModuleName);

	var_value[0] = '\0';
	index=0;
	while((strcmp(vars[index], LIST_TERMINATOR)) != 0)
	{
		/* If we found the item we are looking for, exit the loop. */
		if (strcmp(var_name, vars[index]) == 0) break;
    		index++;
	}

	/* Check to see if InitTelecom API has been called. */
	if (GV_InitCalled != 1 && index != 13 )
	{
		telVarLog(ModuleName,REPORT_NORMAL,TEL_TELECOM_NOT_INIT, GV_Err,
			TEL_TELECOM_NOT_INIT_MSG);
    	HANDLE_RETURN(-1);
	}

	if((GV_IsCallDisconnected1 == 1) && (index != 13))
	{
		telVarLog(mod, REPORT_DETAIL, TEL_BASE, GV_Err,
			"Party 1 is disconnected.");
		HANDLE_RETURN(TEL_DISCONNECTED);
	}

	memset(msg.name, 0, sizeof(msg.name));

	switch(index)
	{
    	case 0: 	/* $ANI */
			strcpy(var_value, GV_ANI);
			HANDLE_RETURN(0);
			break;

    	case 1: 	/* $DNIS */
			strcpy(var_value, GV_DNIS);
			HANDLE_RETURN(0);
			break;

    	case 2: 	/* $PORT_NAME */
			sprintf(var_value, "%d", GV_AppCallNum1);
			HANDLE_RETURN(0);
			break;

    	case 3: 	/* $DEFAULT_LANGUAGE */
			strcpy(var_value, GV_Language);
			HANDLE_RETURN(0);
			break;

		case 4:		/* $USER_TO_USER_INFO */
        	strcpy(var_value, GV_UserToUserInfo);
			HANDLE_RETURN(0);
        	break;

    	case 5:		/* $CALLING_PARTY */
        	strcpy(var_value, GV_CallingParty);
			HANDLE_RETURN(0);
        	break;

    	case 6: 	/* $APP_TO_APP_INFO */
			sprintf(var_value, "%s", GV_AppToAppInfo);
			HANDLE_RETURN(0);
			break;

    	case 7: 	/* $DV_DIGIT_TYPE */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$DV_DIGIT_TYPE is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

    	case 8: 	/* $APP_PHRASE_DIR */
			sprintf(var_value, "%s", GV_AppPhraseDir);
			HANDLE_RETURN(0);
			break;

		case 9:		/* $RING_EVENT_TIME */
			sprintf(var_value, "%s", GV_RingEventTime);
			HANDLE_RETURN(0);
			break;

		case 10: 	/* $INIT_TIME */
			sprintf(var_value, "%s", GV_InitTime);
			HANDLE_RETURN(0);
			break;

		case 11: 	/* $CONNECT_TIME */
			sprintf(var_value, "%s", GV_ConnectTime);
			HANDLE_RETURN(0);
			break;

		case 12: 	/* $DISCONNECT_TIME */
			sprintf(var_value, "%s", GV_DisconnectTime);
			HANDLE_RETURN(0);
			break;

		case 13: 	/* $EXIT_TIME */
			sprintf(var_value, "%s", GV_ExitTime);
			HANDLE_RETURN(0);
			break;

		case 14: 	/* $INBAND_DIGITS */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$INBAND_DIGITS is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 15:    /* $CUST_DATA1 */
			sprintf(var_value, "%s", GV_CustData1);
			HANDLE_RETURN(0);
			break;

		case 16:    /* $CUST_DATA2 */
			sprintf(var_value, "%s", GV_CustData2);
			HANDLE_RETURN(0);
			break;

		case 17: 	/* $CALL_CDR_KEY */
			sprintf(var_value, "%s", GV_CDRKey);
			HANDLE_RETURN(0);
			break;

		case 18: 	/* $INBOUND_CALL_PARM */
			var_value[0] = '\0';
			sprintf(msg.name, "%s", "$INBOUND_CALL_PARM");
			break;

		case 19: 	/* $TRUNK_GROUP", */
			sprintf(var_value, "%s", GV_TrunkGroup);
			HANDLE_RETURN(0);
			break;

		case 20: 	/* "$VOLUME_UP_KEY" */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$VOLUME_UP_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 21:	/* "$VOLUME_DOWN_KEY" */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$VOLUME_DOWN_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 22:	/* "$SPEED_UP_KEY" */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$SPEED_UP_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 23:	/* "$SPEED_DOWN_KEY" */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$SPEED_DOWN_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 24:	/* "$SKIP_FORWARD_KEY" */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$SKIP_FORWARD_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 25:	/* "$SKIP_BACKWARD_KEY" */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$SKIP_BACKWARD_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 26:	/* "$SKIP_TERMINATE_KEY" */
			sprintf(msg.name, "%s", "$SKIP_TERMINATE_KEY");
			break;
	
		case 27:	/* "$BEEP_FILE" */
			sprintf(var_value, "%s", GV_BeepFile);
			HANDLE_RETURN(0);
			break;
	
		case 28:	/* "$OUTBOUND_CALL_PARM" */
			sprintf(msg.name, "%s", "$OUTBOUND_CALL_PARM");
			break;
	
		case 29:	/* "$PAUSE_KEY" */
			sprintf(msg.name, "%s", "$PAUSE_KEY");
			break;

		case 30:	/* "$RESUME_KEY" */
			telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
				"Any key pressed in the playback paused state will resume the "
				"playback. Ignoring the RESUME_KEY passed.");
			HANDLE_RETURN(0);
			break;

		case 31:	/* "$DTMF_BARGEIN_DIGITS" */
			sprintf(msg.name, "%s", "$DTMF_BARGEIN_DIGITS");
			break;
	
		case 32:	/* "$FAX_FROM_STR" */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$FAX_FROM_STR is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 33:	/* "$FAX_USER_DEFINED_STR" */
    		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
				"$FAX_USER_DEFINED_STR is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 34:	/* "$SKIP_REWIND_KEY" */
			sprintf(msg.name, "%s", "$SKIP_REWIND_KEY");
			break;
	
		case 35:	/* "$H323_OUTBOUND_GATEWAY" */
			sprintf(var_value, "%s", GV_DefaultGatewayIP);
			HANDLE_RETURN(0);
			break;
	
		case 36:	/* "$RECORD_TERMINATE_KEYS" */
			sprintf(var_value, "%s", GV_RecordTerminateKeys);
			HANDLE_RETURN(0);
			break;

		case 37: /* "$PROTOCOL" */
			sprintf(var_value, "%s", "h323");
			HANDLE_RETURN(0);
			break;

		case 38: /* "$PROTOCOL_VERSION" */
			sprintf(var_value, "%s", "0");
			HANDLE_RETURN(0);
			break;
	
		case 39:	/* "$SIP_OUTBOUND_GATEWAY" */
			sprintf(var_value, "%s", GV_DefaultGatewayIP);
			HANDLE_RETURN(0);
			break;
	
		case 40:	/* "$SIP_URI_VOICEXML" */
			sprintf(msg.name, "%s", "$SIP_URI_VOICEXML");
			break;
	
		case 41: /* "$AUDIO_CODEC" */
			sprintf(msg.name, "%s", "$AUDIO_CODEC");
			break;
	
		case 42:	/* "$PLAYBACK_PAUSE_GREETING_DIRECTORY" */
			sprintf(msg.name, "%s", "$PLAYBACK_PAUSE_GREETING_DIRECTORY");
			break;

		case 43:	/* "$SIP_MESSAGE" */
			sprintf(msg.name, "%s", "$SIP_MESSAGE");
			break;

 		default:
			telVarLog(ModuleName,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
	      		"Variable <%s> is not a defined global (string) variable name.",
				var_name);
			HANDLE_RETURN(-1);
			break;
	} /* switch */

	msg.opcode = DMOP_GETGLOBALSTRING;
	msg.appCallNum = GV_AppCallNum1;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;

	requestSent=0;
	while(1)
	{
		if(!requestSent)
		{
			ret_code = readResponseFromDynMgr(mod, -1, &response, 
							sizeof(response));
			if (ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
			if (ret_code == -2)
			{
				ret_code = sendRequestToDynMgr(mod, &msg);
				if (ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
				requestSent=1;
				ret_code = readResponseFromDynMgr(mod, 0, &response, 
								sizeof(response));
				if(ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
			}
		}
		else
		{
			ret_code = readResponseFromDynMgr(mod, 0, &response, 
							sizeof(response));
       		if(ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
		}
			
		ret_code = examineDynMgrResponse(mod, &msg, &response);	
		switch(ret_code)
		{
			case DMOP_GETGLOBALSTRING:
				if(response.returnCode == 0)
				{
					if(index == 28 /* OUTBOUND_CALL_PARM */)
					{
						ret_code = extractValue(response.message, var_value);
						if(ret_code != 0)
							HANDLE_RETURN(-1);
					}
					else
					if(index == 18 /* INBOUND_CALL_PARM */)
					{
						ret_code = extractValue(response.message, var_value);
						if(ret_code != 0)
							HANDLE_RETURN(-1);
					}
					else
					if(index == 43 /* SIP_MESSAGE */)
					{
						ret_code = extractValue(response.message, var_value);
						if(ret_code != 0)
							HANDLE_RETURN(-1);
					}
					else
						sprintf(var_value, "%s", response.message);
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
		} /* switch ret_code */
	} /* while */

	HANDLE_RETURN(0);		
}

int extractValue(char *zpFileName, char *zpValue)
{
	FILE *fp;

	if((fp = fopen(zpFileName, "r")) == NULL)
	{
		telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,
			"Failed to open file <%s>. errno=%d. [%s]", zpFileName, errno, 
			strerror(errno));
		return(-1);
	}

	fscanf(fp, "%s", zpValue);
	fclose(fp);
	unlink(zpFileName);

	return(0);
}
