/*-----------------------------------------------------------------------------
Program:	TEL_SetGlobalString.c
Purpose:	Set Global string variables.
Author: 	Wenzel_Joglekar
Date: 		10/27/00
?? Need apiInit here
Update: 04/25/01 APJ Removed PLAY_BACK_CONTROL settings.
Update: 06/27/01 APJ Changed some of the strcpys to sprintfs.
Update: 07/10/01 APJ Added CUST_DATA1, CUST_DATA2.
Update: 08/08/01 APJ Added $BEEP_FILE.
Update: 04/09/04 APJ Added $H323_OUTBOUND_GATEWAY.
Update: 06/16/04 APJ Added $RECORD_TERMINATE_KEYS.
Update: 09/15/04 APJ Return on disconnected call.
Update:     11/10/2004 apj Save DTMFs in the buffer.
Update: 06/13/05 DDN Added $SIP_OUTBOUND_GATEWAY.
Update: 11/29/05 DDN Added $SIP_PREFERRED_CODEC.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

#define	LIST_TERMINATOR	 "NULL"

static int checkForInvalidPhoneKey(char *mod, const char *parmCheckString);

/* valid set of global variables to be used in api */
static	char	*vars[] = {
				"$DIAL_STRING",
				"$USER_TO_USER_INFO",
				"$CALLING_PARTY",
				"$APP_TO_APP_INFO",
				"$APP_PHRASE_DIR",
				"$CUST_DATA1", 	
				"$CUST_DATA2", 	
				"$BEEP_FILE",
				"$VOLUME_UP_KEY",
				"$VOLUME_DOWN_KEY",
				"$SPEED_UP_KEY",
				"$SPEED_DOWN_KEY",
				"$SKIP_FORWARD_KEY",
				"$SKIP_BACKWARD_KEY",
				"$SKIP_TERMINATE_KEY",
				"$SKIP_REWIND_KEY",
				"$OUTBOUND_CALL_PARM",
				"$PAUSE_KEY",
				"$RESUME_KEY",
				"$DTMF_BARGEIN_DIGITS",
				"$FAX_FROM_STR",
				"$FAX_USER_DEFINED_STR",
				"$PLAYBACK_PAUSE_GREETING_DIR",
				"$H323_OUTBOUND_GATEWAY",
				"$RECORD_TERMINATE_KEYS",
				"$SIP_OUTBOUND_GATEWAY",
				"$SIP_PREFERRED_CODEC",
                "$SIP_AUTH_USER",
                "$SIP_AUTH_PASS",
                "$SIP_AUTH_IDENTITY",
                "$SIP_AUTH_REALM",
				"$SIP_FROM",
				"$PLAYBACK_PAUSE_GREETING_DIRECTORY",
				LIST_TERMINATOR  /* Must be last item in list */
			  };
/*------------------------------------------------------------------------------
This routine sets the variable from the variable table to the given value. 
Variable # is the index of the variable in the "vars" static vector. 
------------------------------------------------------------------------------*/
int TEL_SetGlobalString( const char *var_name, const char *var_value)
{
static char mod[]="TEL_SetGlobalString";
int	index;
int	ret_code;
char    errString[50];
struct Msg_SetGlobalString msg;
struct MsgToApp response;
int  requestSent;
FILE    *fp;
char    yStrFileName[50];
int     valueLen = strlen(var_value);

msg.opcode = DMOP_SETGLOBALSTRING;
msg.appCallNum = GV_AppCallNum1;
msg.appPid = GV_AppPid;
msg.appRef = GV_AppRef++;
msg.appPassword = GV_AppPassword;
memset(msg.name, 0, sizeof(msg.name));
memset(msg.value, 0, sizeof(msg.value));

sprintf(LAST_API,"%s",mod);
#ifdef DEBUG
	printf("variable name: %s\n", var_name);
#endif

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

/* Find the variable. */
index=0;
while((strcmp(vars[index], LIST_TERMINATOR)) != 0)
{
	/* If we found item we are looking for, exit the loop */
	if (strcmp(var_name, vars[index]) == 0) break;
    	index++;
}

switch(index)
{
	case 0: 	/* $DIAL_STRING */
    	telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
			"$DIAL_STRING is not supported. Ignoring.");
		HANDLE_RETURN(0);
		break;

	case 1: 	/* $USER_TO_USER_INFO */
		sprintf(GV_UserToUserInfo, "%s", var_value);
		HANDLE_RETURN(0);
		break;

	case 2:		/* $CALLING_PARTY */
		sprintf(msg.name, "%s", "$CALLING_PARTY");
		sprintf(msg.value, "%s", var_value);
		sprintf(GV_CallingParty, "%s", var_value);
		break;

	case 3: 	/* $APP_TO_APP_INFO */
		if((int)strlen(var_value) > MAX_APP_TO_APP_INFO_LENGTH)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_APP_TO_APP_INFO_TOO_LONG, GV_Err, 
				"Invalid $APP_TO_APP_INFO. It must be less that %d bytes.",
				MAX_APP_TO_APP_INFO_LENGTH);
			HANDLE_RETURN(-1);
		}
		memset(GV_AppToAppInfo, 0, sizeof(GV_AppToAppInfo));
		sprintf(GV_AppToAppInfo, "%s", var_value);
		HANDLE_RETURN(0);
		break;

	case 4:		/* $APP_PHRASE_DIR */
		sprintf(GV_AppPhraseDir, "%s", var_value);
		HANDLE_RETURN(0);
		break;

	case 5:		/* $CUST_DATA1 */
		sprintf(msg.name, "%s", "$CUST_DATA1");
		if(strlen(var_value) < 9)
		{
			memset(GV_CustData1, 0, sizeof(GV_CustData1));
			strncpy(GV_CustData1, var_value, strlen(var_value));
		}
		else
		{
			telVarLog(mod, REPORT_NORMAL, TEL_CUST_DATA_TOO_LONG, GV_Warn, 
				"Using first 8 characters of (%s), for $CUST_DATA1", var_value);
			memset(GV_CustData1, 0, sizeof(GV_CustData1));
			strncpy(GV_CustData1, var_value, 8);
		}
		sprintf(msg.value, "%s", GV_CustData1);
		break;

	case 6:		/* $CUST_DATA2 */
		sprintf(msg.name, "%s", "$CUST_DATA2");
		if(strlen(var_value) < 101)
		{
			memset(GV_CustData2, 0, sizeof(GV_CustData2));
			strncpy(GV_CustData2, var_value, strlen(var_value));
		}
		else
		{
			telVarLog(mod, REPORT_NORMAL, TEL_CUST_DATA_TOO_LONG, GV_Warn, 
				"Using first 100 characters of (%s), for $CUST_DATA2", 
				var_value);
			memset(GV_CustData2, 0, sizeof(GV_CustData2));
			strncpy(GV_CustData2, var_value, 100);
		}
		sprintf(msg.value, "%s", GV_CustData2);
		break;

	case 7:		/* $BEEP_FILE */
		if(strlen(var_value) < 256)
		{
			if(access(var_value, F_OK) != 0)
			{
				telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,
					GV_Err, "Failed to access beep file (%s). "
					"errno=%d.", var_value, errno);
				HANDLE_RETURN(-1);
			}
			sprintf(GV_BeepFile, "%s", var_value);
			HANDLE_RETURN(0);
		}
		else
		{
			telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,
				"Length of $BEEP_FILE is %d, should be < 256 "
				"characters", strlen(var_value));
			HANDLE_RETURN(-1);
		}
		break;

	case 8: /* $VOLUME_UP_KEY */
    	telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
			"$VOLUME_UP_KEY is not supported. Ignoring.");
		HANDLE_RETURN(0);
		break;

	case 9: /* $VOLUME_DOWN_KEY */
    	telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
			"$VOLUME_DOWN_KEY is not supported. Ignoring.");
		HANDLE_RETURN(0);
		break;

	case 10: /* $SPEED_UP_KEY */
    	telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
			"$SPEED_UP_KEY is not supported. Ignoring.");
		HANDLE_RETURN(0);
		break;

	case 11: /* $SPEED_DOWN_KEY */
    	telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
			"$SPEED_DOWN_KEY is not supported. Ignoring.");
		HANDLE_RETURN(0);
		break;

	case 12: /* $SKIP_FORWARD_KEY */
		sprintf(msg.name, "%s", "$SKIP_FORWARD_KEY");
		if(strlen(var_value) != 0)
		{
			if(checkForInvalidPhoneKey(mod, var_value) != 0)
			{
				HANDLE_RETURN(-1);
			}
			else
				sprintf(msg.value, "%s", var_value);
		}
		break;

	case 13: /* $SKIP_BACKWARD_KEY */
		sprintf(msg.name, "%s", "$SKIP_BACKWARD_KEY");
		if(strlen(var_value) != 0)
		{
			if(checkForInvalidPhoneKey(mod, var_value) != 0)
			{
				HANDLE_RETURN(-1);
			}
			else
				sprintf(msg.value, "%s", var_value);
		}
		break;

	case 14: /* $SKIP_TERMINATE_KEY */
		sprintf(msg.name, "%s", "$SKIP_TERMINATE_KEY");
		if(strlen(var_value) != 0)
		{
			if(checkForInvalidPhoneKey(mod, var_value) != 0)
			{
				HANDLE_RETURN(-1);
			}
			else
				sprintf(msg.value, "%s", var_value);
		}
		break;

	case 15: /* $SKIP_REWIND_KEY */
		sprintf(msg.name, "%s", "$SKIP_REWIND_KEY");
		if(strlen(var_value) != 0)
		{
			if(checkForInvalidPhoneKey(mod, var_value) != 0)
			{
				HANDLE_RETURN(-1);
			}
			else
				sprintf(msg.value, "%s", var_value);
		}
		break;

	case 16: /* $OUTBOUND_CALL_PARM */
		sprintf(msg.name, "%s", "$OUTBOUND_CALL_PARM");

		if(valueLen >= 512)
		{
			telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
				"Length of $OUTBOUND_CALL_PARM is %d, "
				"should be < 512 characters", valueLen);
			HANDLE_RETURN(-1);
		}

		/* Write this data to a file, so that dynmgr can read it. */
		sprintf(yStrFileName, "/tmp/outboundCallParm.%d", getpid());
		if((fp = fopen(yStrFileName, "w+")) == NULL)
		{
			telVarLog(mod,REPORT_NORMAL,TEL_BASE,GV_Err,
				"Failed to open file (%s), [%d, %s]",
				yStrFileName, errno);
			HANDLE_RETURN(-1);
		}

		if(var_value == NULL)
		{
			fprintf(fp, "%s", "\0");
		}
		else
		{
			fprintf(fp, "%s", var_value);
		}

		fclose(fp);
		sprintf(msg.value, "%s", yStrFileName);
		
		break;

	case 17: /* $PAUSE_KEY */
		sprintf(msg.name, "%s", "$PAUSE_KEY");
		if(strlen(var_value) != 0)
		{
			if(checkForInvalidPhoneKey(mod, var_value) != 0)
			{
				HANDLE_RETURN(-1);
			}
			else
				sprintf(msg.value, "%s", var_value);
		}
		break;

	case 18: /* $RESUME_KEY */
		telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
			"Any key pressed in the playback paused state will resume the "
			"playback. Ignoring the RESUME_KEY passed.");
		HANDLE_RETURN(0);
		break;

	case 19: /* $DTMF_BARGEIN_DIGITS */
		sprintf(msg.name, "%s", "$DTMF_BARGEIN_DIGITS");
		if(strlen(var_value) != 0)
		{
			if(checkForInvalidPhoneKey(mod, var_value) != 0)
			{
				HANDLE_RETURN(-1);
			}
			else
				sprintf(msg.value, "%s", var_value);
		}
		break;

	case 20: /* $FAX_FROM_STR */
    	telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
			"$FAX_FROM_STR is not supported. Ignoring.");
		HANDLE_RETURN(0);
		break;

	case 21: /* $FAX_USER_DEFINED_STR */
    	telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
			"$FAX_USER_DEFINED_STR is not supported. Ignoring.");
		HANDLE_RETURN(0);
		break;

	case 22: /* $PLAYBACK_PAUSE_GREETING_DIR */
		sprintf(msg.name, "%s", "$PLAYBACK_PAUSE_GREETING_DIR");
		if(!var_value || !*var_value)
		{
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_CONTROL_KEYS, GV_Err,
				"Empty $PLAYBACK_PAUSE_GREETING_DIR passed. "
				"It must be a valid directory name.");
			HANDLE_RETURN(-1);
		}

		if(access(var_value, R_OK) != 0)
		{
			char  yTempErrMsg[256];

			yTempErrMsg[0] = '\0';

			switch(errno)
			{
				case EACCES:
					sprintf(yTempErrMsg, "Access denied to (%s).", var_value);
					break;

				case ENAMETOOLONG:
					sprintf(yTempErrMsg, "Name (%s) is too long.", var_value);
					break;

				case ENOTDIR:
					sprintf(yTempErrMsg, "(%s) is not a directory.", var_value);
					break;

				case ENOENT:
				default:
					sprintf(yTempErrMsg, 
						"Name (%s) does not exist.", var_value);
					break;
			}

			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"%s", yTempErrMsg);

			HANDLE_RETURN(-1);
		}

		sprintf(msg.value, "%s", var_value);

		valueLen = strlen(msg.value);

		if(msg.value[valueLen -1] == '/')
		{
			msg.value[valueLen -1] = '\0';
		}
		break;

    case 23:    /*H323_OUTBOUND_GATEWAY*/
		sprintf(GV_DefaultGatewayIP, "%s", var_value);
		HANDLE_RETURN(0);
		break;

    case 24:    /*RECORD_TERMINATE_KEYS*/
		sprintf(GV_RecordTerminateKeys, "%s", var_value);
		HANDLE_RETURN(0);
		break;

    case 25:    /*SIP_OUTBOUND_GATEWAY*/
		sprintf(GV_DefaultGatewayIP, "%s", var_value);
		HANDLE_RETURN(0);
		break;

	case 26:    /*SIP_PREFERRED_CODEC*/
		sprintf(msg.name, "%s", "$SIP_PREFERRED_CODEC");
		sprintf(msg.value, "%s", var_value);
		break;

    case 27:    /*$SIP_AUTHENTICATION_USER*/
        sprintf(msg.name, "%s", "$SIP_AUTHENTICATION_USER");
        sprintf(msg.value, "%s", var_value);
        break;

    case 28:    /*SIP_AUTHENTICATION_PASSWORD*/
        sprintf(msg.name, "%s", "$SIP_AUTHENTICATION_PASSWORD");
        sprintf(msg.value, "%s", var_value);
        break;

    case 29:    /*SIP_AUTHENTICATION_ID*/
        sprintf(msg.name, "%s", "$SIP_AUTHENTICATION_ID");
        sprintf(msg.value, "%s", var_value);
        break;

    case 30:    /*SIP_AUTHENTICATION_REALM*/
        sprintf(msg.name, "%s", "$SIP_AUTHENTICATION_REALM");
        sprintf(msg.value, "%s", var_value);
        break;

    case 31:    /*SIP_FROM*/
        sprintf(msg.name, "%s", "$SIP_FROM");
        sprintf(msg.value, "%s", var_value);
        break;

	case 32:    /*PLAYBACK_PAUSE_GREETING_DIRECTORY*/
		sprintf(msg.name, "%s", "$PLAYBACK_PAUSE_GREETING_DIRECTORY");
		if(!var_value || !*var_value)
		{
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_CONTROL_KEYS, GV_Err,
				"Empty $PLAYBACK_PAUSE_GREETING_DIRECTORY passed. "
				"It must be a valid directory name.");
			HANDLE_RETURN(-1);
		}

		if(access(var_value, R_OK) != 0)
		{
			char  yTempErrMsg[256];

			yTempErrMsg[0] = '\0';

			switch(errno)
			{
				case EACCES:
					sprintf(yTempErrMsg, "Access denied to (%s).", var_value);
					break;

				case ENAMETOOLONG:
					sprintf(yTempErrMsg, "Name (%s) is too long.", var_value);
					break;

				case ENOTDIR:
					sprintf(yTempErrMsg, "(%s) is not a directory.", var_value);
					break;

				case ENOENT:
				default:
					sprintf(yTempErrMsg, 
						"Name (%s) does not exist.", var_value);
					break;
			}

			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"%s", yTempErrMsg);

			HANDLE_RETURN(-1);
		}

		sprintf(msg.value, "%s", var_value);

		valueLen = strlen(msg.value);

		if(msg.value[valueLen -1] == '/')
		{
			msg.value[valueLen -1] = '\0';
		}
		break;

	default:
		telVarLog(mod,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
		"Variable (%s) is not a defined global (string) variable name.",
			var_name);
		HANDLE_RETURN(-1);
		break;
}

requestSent=0;
while(1)
{
	if(!requestSent)
	{
		ret_code = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
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
		ret_code = readResponseFromDynMgr(mod, 0, &response, sizeof(response));
       	if(ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
	}
		
	ret_code = examineDynMgrResponse(mod, &msg, &response);	
	switch(ret_code)
	{
		case DMOP_SETGLOBALSTRING:
			if(index == 15 /* $OUTBOUND_CALL_PARM */)
				unlink(yStrFileName);
			HANDLE_RETURN(response.returnCode);
			break;

		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum, response.message);
			break;

		case DMOP_DISCONNECT:
			if(index == 15 /* $OUTBOUND_CALL_PARM */)
				unlink(yStrFileName);
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

static int checkForInvalidPhoneKey(char *mod, const char *parmCheckString)
{
	int i;
	char invalidKeysString[20];
	char buf[5];

	memset(invalidKeysString, 0, sizeof(invalidKeysString));

	for(i=0; i<strlen(parmCheckString); i++)
	{
		switch(parmCheckString[i])
		{
			case '0': case '1': case '2': case '3': 
			case '4': case '5': case '6': case '7': 
			case '8': case '9': case '*': case '#':
				break;
			
			default:
				sprintf(buf, "%c", parmCheckString[i]);
				strcat(invalidKeysString, buf);
				break;
		}
	} /* End for */

	if(strlen(invalidKeysString) > 0)
	{
               	telVarLog(mod, REPORT_NORMAL, TEL_INVALID_CONTROL_KEYS, GV_Err,
"Invalid keys (%s) in value parameter (%s). Valid keys are 0-9, * and #.", 
			invalidKeysString, parmCheckString);
		return(-1);
	}

	return(0);
} /* END checkForInvalidPhoneKey() */
