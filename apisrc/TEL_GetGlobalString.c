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
Update: 09/28/2007 djb Added $PLATFORM_TYPE.
Update: 05/18/11 djb Added $T38_FAX_STATION_ID and $T30_HEADER_INFO
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "arcSIPHeaders.h"

#define	LIST_TERMINATOR		"NULL"

int addEscapeChars(char *fileContent);
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
			"$INIT_TIME",			// 10
			"$CONNECT_TIME",
			"$DISCONNECT_TIME",
			"$EXIT_TIME", 	
			"$INBAND_DIGITS", 	
			"$CUST_DATA1", 	
			"$CUST_DATA2", 	
			"$CALL_CDR_KEY",
			"$INBOUND_CALL_PARM",
			"$TRUNK_GROUP",
			"$VOLUME_UP_KEY",		// 20
			"$VOLUME_DOWN_KEY",
			"$SPEED_UP_KEY",
			"$SPEED_DOWN_KEY",
			"$SKIP_FORWARD_KEY",
			"$SKIP_BACKWARD_KEY",
			"$SKIP_TERMINATE_KEY",
			"$BEEP_FILE",
			"$OUTBOUND_CALL_PARM",
			"$PAUSE_KEY",
			"$RESUME_KEY",				// 30
			"$DTMF_BARGEIN_DIGITS",
			"$FAX_FROM_STR",
			"$FAX_USER_DEFINED_STR",
			"$SKIP_REWIND_KEY",
			"$H323_OUTBOUND_GATEWAY",
			"$RECORD_TERMINATE_KEYS",
			"$PROTOCOL",
			"$PROTOCOL_VERSION",
			"$SIP_OUTBOUND_GATEWAY",
			"$SIP_URI_VOICEXML",			// 40
			"$AUDIO_CODEC",
			"$PLAYBACK_PAUSE_GREETING_DIRECTORY",
			"$SIP_MESSAGE",
			"$ORIGINAL_ANI",
			"$ORIGINAL_DNIS",
			"$PLATFORM_TYPE",
			"$SIP_REDIRECTED_CONTACTS",
			"$T38_FAX_STATION_ID",
			"$T30_HEADER_INFO",
			"$TEL_TRANSFER_AAI",			// 50
			"$BRIDGE_CONNECT_TIME",
			"$BRIDGE_DISCONNECT_TIME",
			"$SIP_HEADER:USER-TO-USER",
#ifdef VOICE_BIOMETRICS
			"$AVB_CONTSPEECH_RESULT",
#endif // VOICE_BIOMETRICS
			LIST_TERMINATOR /* Must be last item in list */
	 	      };

int extractValue(char *zpFileName, char *zpValue)
{
	FILE *fp;

	if((fp = fopen(zpFileName, "r")) == NULL)
	{
		telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,
			"Failed to open file (%s). errno=%d. [%s]", zpFileName, errno, 
			strerror(errno));
		return(-1);
	}

	fscanf(fp, "%s", zpValue);
	fclose(fp);
	unlink(zpFileName);

	return(0);
}

int extractFile(char *zpFileName, char *zpValue)
{
	FILE *fp;
	int yRc = 0;
	char fileContent[4096] = "";
	struct stat yStat;

	if( (yRc = stat(zpFileName, &yStat)) < 0)
    {
		telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,
			"Failed to do stat on file (%s). errno=%d. [%s]", zpFileName, errno, 
			strerror(errno));
        return(-1);
    }

	if((fp = fopen(zpFileName, "r")) == NULL)
	{
		telVarLog(ModuleName, REPORT_NORMAL, TEL_BASE, GV_Err,
			"Failed to open file (%s). [%d, %s]", zpFileName, errno, 
			strerror(errno));
		return(-1);
	}

	fread(fileContent, yStat.st_size, 1, fp);
	fclose(fp);
	unlink(zpFileName);

	sprintf(zpValue, "%s", fileContent);

	return(0);
}

int TEL_GetGlobalString(const char *var_name, char var_value[])
{
	int	index = 0;
	int	ret_code;
	int i = 0;
    int timeo = 0;
	int rc;

	static char mod[]="TEL_GetGlobalString";
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s()";

	struct Msg_GetGlobalString msg;
	struct MsgToApp response;
#ifdef VOICE_BIOMETRICS
	struct Msg_AVBMsgToApp	avbMsg;
#endif // VOICE_BIOMETRICS
	int requestSent;
	int responseReceived;
	int done;


    /* joes testing this */

	rc = apiInit(mod, TEL_GETGLOBALSTRING, apiAndParms, 1, 0, 0);

    /* end test */

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
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
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
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
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
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$VOLUME_UP_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 21:	/* "$VOLUME_DOWN_KEY" */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$VOLUME_DOWN_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 22:	/* "$SPEED_UP_KEY" */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$SPEED_UP_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 23:	/* "$SPEED_DOWN_KEY" */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$SPEED_DOWN_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 24:	/* "$SKIP_FORWARD_KEY" */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$SKIP_FORWARD_KEY is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 25:	/* "$SKIP_BACKWARD_KEY" */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
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
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
				"$FAX_FROM_STR is not supported. Ignoring.");
			HANDLE_RETURN(0);
			break;

		case 33:	/* "$FAX_USER_DEFINED_STR" */
    		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
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
			sprintf(var_value, "%s", "SIP");
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

		case 44:	/* "$ORIGINAL_ANI" */
			sprintf(msg.name, "%s", "$ORIGINAL_ANI");
			break;

		case 45:	/* "$ORIGINAL_DNIS" */
			sprintf(msg.name, "%s", "$ORIGINAL_DNIS");
			break;

		case 46:	/* "$PLATFORM_TYPE" */
			sprintf(msg.name, "%s", "$PLATFORM_TYPE");
			break;

		case 47:	/* "$SIP_REDIRECTED_CONTACTS" */
			sprintf(msg.name, "%s", "$SIP_REDIRECTED_CONTACTS");
			break;

		case 48:	/* "$T38_FAX_STATION_ID" */
			sprintf(msg.name, "%s", "$T38_FAX_STATION_ID");
			break;

		case 49:	/* "$T30_HEADER_INFO" */
			sprintf(msg.name, "%s", "$T30_HEADER_INFO");
			break;

		case 50:	/* "$TEL_TRANSFER_AAI" */
			sprintf(msg.name, "%s", "$TEL_TRANSFER_AAI");
			break;

        case 51:
			sprintf(msg.name, "%s", "$BRIDGE_CONNECT_TIME");
			sprintf(var_value, "%s", GV_BridgeConnectTime);
			HANDLE_RETURN(0);
            break;

        case 52:
			sprintf(msg.name, "%s", "$BRIDGE_DISCONNECT_TIME");
			sprintf(var_value, "%s", GV_BridgeDisconnectTime);
			HANDLE_RETURN(0);
            break;

		case 53:	/* "$SIP_HEADER:USER-TO-USER" */
			sprintf(msg.name, "%s", "$SIP_HEADER:USER-TO-USER");
			break;

#ifdef VOICE_BIOMETRICS
		case 54:	/* "$AVB_CONTSPEECH_RESULT" */
			sprintf(msg.name, "%s", "$AVB_CONTSPEECH_RESULT");
			break;
#endif // VOICE_BIOMETRICS
 		default:
			i = 0;
			while ( strcmp(GV_SipHeaderInfo[i].globalStringName, "") != 0 )
			{
				if (strcmp(GV_SipHeaderInfo[i].globalStringName, var_name) == 0)
				{
					telVarLog(ModuleName,REPORT_DETAIL,TEL_INVALID_PARM,GV_Info,
						"Variable (%s) is not supported, assigning dummy value.",
						var_name);

					//sprintf(var_value, "DUMMY_%s", var_name);
                    var_value[0] = '\0';

					HANDLE_RETURN(0);
				}
				i++;
			}
					
            var_value[0] = '\0';

			telVarLog(ModuleName,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
	      		"Variable (%s) is not a defined global (string) variable name. returning empty string",
				var_name);
			HANDLE_RETURN(0);

			telVarLog(ModuleName,REPORT_NORMAL,TEL_INVALID_PARM,GV_Err,
	      		"Variable (%s) is not a defined global (string) variable name.",
				var_name);
			HANDLE_RETURN(-1);
			break;
	} /* switch */

	msg.opcode = DMOP_GETGLOBALSTRING;
	msg.appCallNum = GV_AppCallNum1;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;

	/* 
       flags for loop entry and 
	   for optionally re-reading from the fifo 
	   due to unexpected messages 
    */

	requestSent=0;
	responseReceived=0; 
	done=0; 

	while(!done)
	{

		if(!requestSent)
		{
			ret_code = readResponseFromDynMgr(mod, -1, &response, 
							sizeof(response));
			if (ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
			if (ret_code == -2)
			{
				ret_code = sendRequestToDynMgr(mod, (struct MsgToDM *)&msg);
				if (ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
				requestSent=1;
				ret_code = readResponseFromDynMgr(mod, 10, &response, 
								sizeof(response));

				switch(ret_code){
                  case -1:
					HANDLE_RETURN(TEL_FAILURE);
                    break;
	 			  case -2:
					HANDLE_RETURN(TEL_TIMEOUT);
                    break;
			      default:	
				    responseReceived=1;
                    break;
                }
#if 0
				if(ret_code == -1 || ret_code == -2){ 
					HANDLE_RETURN(TEL_FAILURE);
                } else {
				  responseReceived=1;
                }
#endif
			}
		}


		if(requestSent && !responseReceived){
			ret_code = readResponseFromDynMgr(mod, 10, &response, 
							sizeof(response));
				switch(ret_code){
                  case -1:
					HANDLE_RETURN(TEL_FAILURE);
                    break;
	 			  case -2:
					HANDLE_RETURN(TEL_TIMEOUT);
                    break;
			      default:	
				    responseReceived=1;
                    break;
                }
#if 0
       		if(ret_code == -1){
			 HANDLE_RETURN(TEL_FAILURE);
            } else {
			  responseReceived = 1;
            }
#endif 
		}
			
		ret_code = examineDynMgrResponse(mod, (struct MsgToDM *)&msg, &response);	
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
						//ret_code = extractValue(response.message, var_value);
						ret_code = extractFile(response.message, var_value);
						if(ret_code != 0)
							HANDLE_RETURN(-1);
					}
					else
					if(index == 47 /* SIP_MESSAGE SIP REDIRECTS --  */)
					{
						//ret_code = extractValue(response.message, var_value);
						ret_code = extractFile(response.message, var_value);
						if(ret_code != 0)
							HANDLE_RETURN(-1);
					}
					else 
					if(index == 51 ){
                      sprintf(var_value, GV_BridgeConnectTime);
					  HANDLE_RETURN(0);
                    } 
                    else 
                    if(index == 52){
                      sprintf(var_value, GV_BridgeDisconnectTime);
					  HANDLE_RETURN(0);
                    }
                    if(index == 53){			// $SIP_HEADER:USER-TO-USER
					  sprintf(var_value, "%s", response.message);
					  HANDLE_RETURN(0);
                    }
#ifdef VOICE_BIOMETRICS
					else
					if(index == 54 /* AVB_CONTSPEECH_RESULT */)
					{
						memcpy(&avbMsg, &response, sizeof(struct MsgToApp));
						telVarLog(ModuleName, REPORT_VERBOSE, TEL_BASE, GV_Err,
							"avbMsg.returnCode = %d" 
			                "msg_avbMsgToApp.score=%f; "
           			    	"msg_avbMsgToApp.indThreshold=%f; "
               				"msg_avbMsgToApp.confidence=%f; ",
							avbMsg.returnCode,
			                avbMsg.score,
           			    	avbMsg.indThreshold,
               				avbMsg.confidence);


						HANDLE_RETURN(avbMsg.returnCode);
					}
#endif // VOICE_BIOMETRICS
				}
#ifdef VOICE_BIOMETRICS
				else
				if(response.returnCode == -50)
				{
					HANDLE_RETURN(response.returnCode);
				}
#endif // VOICE_BIOMETRICS
				done++;
				break;
	
			case DMOP_GETDTMF:
				saveTypeAheadData(mod, response.appCallNum, response.message);
				responseReceived = 0;
				break;
	
			case DMOP_DISCONNECT:
				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;
	
			default:
				/* Unexpected message. Logged in examineDynMgr... */
				responseReceived = 0;
				continue;
				break;
		} /* switch ret_code */
	} /* while */

	HANDLE_RETURN(0);		
}
