/*-----------------------------------------------------------------------------
Program:        TEL_ListenCall.c
Purpose:        Listen on going call in silent mode
Author:         Rahul
Date:			10/17/2000
Update: 11042015 djb    MR 4515
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "TEL_Globals.h"

extern int in_bridge_cdr;

int tel_listencall_parametersAreGood(char *mod, int option, int numRings,
					int informat, const char *destination,
					const char *resourceType, char *editedDestination);
int tel_ListenCall(char *mod, int *connected, int *appCallNum,
						int *retCode, int *channel, void *m);

extern int parseInitiateCallMessageResponse(char *responseMessage);

int TEL_ListenCall(	int option, 	int numRings,
					int informat, 	const char *destination,
					const char *resourceType, int *retcode,
					int *channel, int maxTime)
{
	char mod[]="TEL_ListenCall";
	int notifyParm;
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s(%s,%d,%s,%s,%s,%s,%s)";
	char editedDestination[MAX_LENGTH] = "";
	int  rc;
	char buf[100] = "";
	int  keepAliveCounter = 0;
	time_t t1, t2;

	struct Msg_ListenCall msg;
	struct MsgToApp response;
	struct Msg_DropCall lMsgDropCall;
			
	char yStrFileName[256];

	telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
		"Inside TEL_ListenCall, maxTime=%d, informat=%d", maxTime, informat);

	sprintf(apiAndParms,apiAndParmsFormat, mod, arc_val_tostr(option),
		numRings, arc_val_tostr(informat), destination,
		resourceType, "retcode", "channel");
	rc = apiInit(mod, TEL_LISTENCALL, apiAndParms, 1, 1, 0);
	if (rc != 0) 
	{
		HANDLE_RETURN(rc);
	}

	*retcode = 21;

	if (!tel_listencall_parametersAreGood(mod, option, numRings, informat,
		destination, resourceType, editedDestination))
	{
		HANDLE_RETURN (TEL_FAILURE);
	}

	msg.appCallNum  = GV_AppCallNum1;
	msg.appPid  = GV_AppPid;
	msg.appPassword = GV_AppPassword;
	msg.informat = informat;

	if(informat == INTERNAL_PORT)
	{
//		sprintf(msg.phoneNumber, "%s", destination);

		if ( strlen(destination) <= (sizeof(msg.phoneNumber) - 1) )
		{
			sprintf(msg.phoneNumber, "%s", destination);
		}
		else
		{
			sprintf(yStrFileName, "/tmp/phoneNumber.%s.%d.%d", mod, GV_AppCallNum1, getpid());
			if ( (rc = prepLongDestinationForFileIPC("phoneNumber", destination, msg.phoneNumber)) != 0 )
			{
				HANDLE_RETURN(-1);
			}
		}

		msg.opcode  = DMOP_LISTENCALL;
		msg.appRef  = GV_AppRef++;
		msg.numRings    = numRings;
		msg.whichOne    = 2;
		msg.appCallNum1 = GV_AppCallNum1;
		msg.appCallNum2 = atoi(destination);
			
		rc = tel_ListenCall(mod, &GV_Connected2,
					&GV_AppCallNum2, retcode, channel,
					(void *)&msg);

		return(rc);
	}

	switch(option)
	{
		case DIAL_OUT:
		case ALL:
		case LISTEN_ALL :
		case LISTEN_IGNORE  :

			msg.opcode  = DMOP_LISTENCALL;
			msg.appRef  = GV_AppRef++;
			msg.numRings    = numRings;

			if (informat == IP || informat == H323 || informat == SIP)
			{
				msg.informat = IP;
				memset(msg.phoneNumber, '\0', sizeof(msg.phoneNumber));

				if ( strlen(editedDestination) <= (sizeof(msg.ipAddress) - 1) )
				{
					sprintf(msg.ipAddress, "%s", editedDestination);
				}
				else
				{
					sprintf(yStrFileName, "/tmp/ipAddress.%s.%d.%d", mod, GV_AppCallNum1, getpid());
					if ( (rc = prepLongDestinationForFileIPC("ipAddress", editedDestination, msg.ipAddress)) != 0 )
					{
						HANDLE_RETURN(-1);
					}
				}
			}
			else
			{
				msg.informat = NONE;
				sprintf(msg.ipAddress, "%s", GV_DefaultGatewayIP);
//				sprintf(msg.phoneNumber, "%s", editedDestination);

				if ( strlen(editedDestination) <= (sizeof(msg.phoneNumber) - 1) )
				{
					sprintf(msg.phoneNumber, "%s", editedDestination);
				}
				else
				{
					sprintf(yStrFileName, "/tmp/phoneNumber.%s.%d.%d", mod, GV_AppCallNum1, getpid());
					if ( (rc = prepLongDestinationForFileIPC("phoneNumber", editedDestination, msg.phoneNumber)) != 0 )
					{
						HANDLE_RETURN(-1);
					}
				}
	
				if(strstr(editedDestination, "@") || strstr(editedDestination, "sip:"))
				{
//					sprintf(msg.ipAddress, "%s", editedDestination);
					memset(msg.phoneNumber, '\0', sizeof(msg.phoneNumber));
					msg.informat = IP;

					if ( strlen(editedDestination) <= (sizeof(msg.ipAddress) - 1) )
					{
						sprintf(msg.ipAddress, "%s", editedDestination);
					}
					else
					{
						sprintf(yStrFileName, "/tmp/ipAddress.%s.%d.%d", mod, GV_AppCallNum1, getpid());
						if ( (rc = prepLongDestinationForFileIPC("ipAddress", editedDestination, msg.ipAddress)) != 0 )
						{
							HANDLE_RETURN(-1);
						}
					}
				}
			}

			msg.whichOne    = 2;
			msg.appCallNum1 = GV_AppCallNum1;
			msg.appCallNum2 = GV_AppCallNum2;

			memset(msg.ani, 0, sizeof(msg.ani));
			strncpy(msg.ani, GV_CallingParty, sizeof(msg.ani)-1);
				
			rc = tel_ListenCall(mod, &GV_Connected2,
					&GV_AppCallNum2, retcode, channel,
					(void *)&msg);

			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"returned from tel_ListenCall, GV_AppCallNum2=%d, rc=%d",
				GV_AppCallNum2, rc);

			if(rc == TEL_SUCCESS)
			{
				GV_ListenCallActive = 1;
				*retcode = 0;

				writeFieldToSharedMemory(mod, GV_AppCallNum2,
					APPL_PID, GV_AppPid);

				writeFieldToSharedMemory(mod, GV_AppCallNum2,
					APPL_STATUS, STATUS_BUSY);

				writeFieldToSharedMemory(mod, GV_AppCallNum2,
					APPL_SIGNAL, 1);
			
				//tran_shmid = shmget(shm_key, SIZE_SCHD_MEM, PERMS);
				tran_shmid = shmget (SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);
				//tran_table_ptr = (char *)shmat(tran_shmid, 0, 0);
  				tran_tabl_ptr = shmat (tran_shmid, 0, 0);
				write_fld(tran_tabl_ptr, GV_AppCallNum2, APPL_API, TEL_LISTENCALL);

				//sprintf(buf, "ASSIGNED:%d", GV_AppCallNum1);

				//writeStringToSharedMemory(mod, GV_AppCallNum2,  buf);
			}

			if (option==DIAL_OUT || rc != TEL_SUCCESS) 
			{
				HANDLE_RETURN(*retcode);
			}
			

			break;
	}

	return(TEL_SUCCESS);

}/* int TEL_ListenCall */

int tel_ListenCall(char *mod, int *connected, int *appCallNum,
int *retCode, int *channel, void *m)
{
	int rc;
	int lCallNum;
	int requestSent;
	struct Msg_ListenCall msg;
	struct MsgToApp response;
	struct Msg_DropCall lMsgDropCall;
	time_t t1,t2;
	char 	portString[32];

	msg=*(struct Msg_ListenCall *)m;

	*retCode = 21;
	requestSent=0;
	time(&t1);
	memset(&GV_BridgeResponse, 0, sizeof(struct MsgToApp));
	
	telVarLog(mod,REPORT_VERBOSE,TEL_BASE, GV_Info,
				"informat=%d", msg.informat);

	while ( 1 )
	{
		if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response,
				sizeof(response));
			if(rc == -1) 
			{
				return(TEL_FAILURE);
			}
			if(rc == -2)
			{
				telVarLog(mod,REPORT_VERBOSE,TEL_BASE, GV_Info,
						"Sending request to CallManager.");

				rc = sendRequestToDynMgr(mod, &msg);
				if (rc == -1) 
				{
					return(-1);
				}
				requestSent=1;

				rc = readResponseFromDynMgr(mod,0,
						&response,sizeof(response));
				if(rc == -1) return(TEL_FAILURE);
			}
		}
		else
		{
			telVarLog(mod,REPORT_VERBOSE,TEL_BASE, GV_Info,
						"Reading response from DynMgr.");
			rc = readResponseFromDynMgr(mod,0,
				&response,sizeof(response));
			if(rc == -1) return(TEL_FAILURE);
		}
		rc = examineDynMgrResponse(mod, &msg, &response);
		switch(rc)
		{
			case DMOP_LISTENCALL:
				if (response.returnCode >= 0)
				{
					in_bridge_cdr = 1;
					*connected=1;
					lCallNum = parseInitiateCallMessageResponse(response.message);
					//lCallNum=atoi(response.message);
					*appCallNum=lCallNum;
					*channel=lCallNum;
					*retCode = response.returnCode;
                    char lMsg[512];
                    setTime(GV_BridgeConnectTime,&GV_BridgeConnectTimeSec);
                    if ( GV_BLegCDR == 1 )
                    {
                        sprintf (lMsg, "%s:BS:%s:%s:%s:%s:%s:%s",
                            GV_CDR_Key, GV_AppName, GV_ANI, GV_DNIS,
                           GV_BridgeConnectTime, GV_CustData1, GV_CustData2);
                        sprintf(portString,  "%d", lCallNum);
                        LogARCMsg (mod, REPORT_CDR, portString, "TEL", GV_AppName, 20013, lMsg);

                    }
					telVarLog(mod,REPORT_VERBOSE,TEL_BASE, GV_Info,
						"pid=%d assigned call # %d. lCallNum=%d",
						response.appPid, response.appCallNum, lCallNum);

					return(0);
				}
				else
				{
					if(response.returnCode != -1)
						*retCode = -1 * response.returnCode;
					else
						*retCode = response.returnCode;

					return(-1);
				}
				break;
			case DMOP_GETDTMF:
/* Save typeahead if it's for the other call leg. */
				if (msg.appCallNum != response.appCallNum)
					saveTypeAheadData(mod, response.appCallNum,
						response.message);
				break;
			case DMOP_DISCONNECT:
				return(disc(mod, response.appCallNum));
				break;
			default:
/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		}										  /* switch rc */
	}											  /* while */
}

/*-------------------------------------------------------------------------
This function verifies that all parameters are valid and sets any global
variables necessary for execution. It returns 1 (yes) if all parmeters are
good, 0 (no) if not.
-------------------------------------------------------------------------*/
int tel_listencall_parametersAreGood(char *mod, int option, int numRings,
					int informat, const char *destination,
					const char *resourceType, char *editedDestination)
{
	int  errors=0;
	char termCharHolder[2] = "";
	char optionValues[]="DIAL_OUT, CONNECT, ALL, LISTEN_ALL, LISTEN_IGNORE";
	int  numRingsMin=1;
	int  numRingsMax=100;
	char informatValues[]="NAMERICAN, NONE, E_164, SIP, H323";

	switch(option)
	{
		case ALL:
		case LISTEN_ALL:
		case DIAL_OUT:
		case LISTEN_IGNORE:
			break;

		case CONNECT:
			if(!GV_Connected2)
			{
				errors++;
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
					"Invalid value for option: CONNECT. No successful "
					"DIAL_OUT is currently in effect.");
			}
			break;

		default:
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"Invalid value for option: %d. Valid values are %s.",
				option, optionValues);
	}

	if ((numRings < numRingsMin) || (numRings > numRingsMax))
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid value for numRings: %d. Valid values are %d to %d.",
			numRings, numRingsMin, numRingsMax);
	}

	if(destination == NULL)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, "%s",
			"Invalid value for destination: NULL.");
	}

	if(resourceType == NULL)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, "%s",
			"Invalid value for resourceType: NULL.");
	}

	telVarLog(mod,REPORT_VERBOSE,TEL_BASE, GV_Info,
						"informat=%d errors=%d", informat, errors);
	switch(informat)
	{
		case NONE:
		case E_164:
		case NAMERICAN:
		case IP:
		case SIP:
		case H323:
			if (!goodDestination(mod,informat,destination,editedDestination))
			{
				errors++;
				telVarLog(mod,REPORT_VERBOSE,TEL_BASE, GV_Info,
						"informat=%d errors=%d", informat, errors);
/* Message written in subroutine. */
			}
			break;
		case INTERNAL_PORT:
			break;
		default:
			errors++;
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"Invalid value for informat: %d. Valid values are %s.",
				informat, informatValues);
			break;
	}
		
	telVarLog(mod,REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info, "3 informat=%d", informat);

	return( errors == 0);
}


#if 0  // commented out to source the fixed one from the other routines 

static int parseInitiateCallMessageResponse(char *responseMessage)
{
	static const char mod[] = "parseInitiateCallMessageResponse";
	char stringToFind[] = {"BANDWIDTH="};
	char    *zQuery = NULL;
	char    lhs[32];
	char    rhs[32];
	char    buf[MAX_APP_MESSAGE_DATA];	  
	int 	  index;
    char tmpVal[100];
	int rc = -1;

	if(!strstr(responseMessage, "VIDEO"))
	{
		return(atoi(responseMessage));
	}
	
	zQuery = (char*)strtok_r((char *)responseMessage, ",", (char**)&buf);

	
	if(zQuery == NULL )
	{
		return 0;
	}
	strcat(zQuery, "^");

	for(index = 0; index < strlen(zQuery); index++)
	{

        tmpVal[0] = '\0';

		if((strstr(zQuery + index, "AUDIO_CODEC") == zQuery + index )) 
		{


			getCodecValue(
							zQuery + index,
							tmpVal,
							"AUDIO_CODEC",
							'=',
							'^');
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"AUDIO_CODEC=%s.", tmpVal);

			if(strcmp(tmpVal, "711") == 0)
				sprintf(GV_Audio_Codec, "%d", 3);	
			else if(strcmp(tmpVal, "711r") == 0)
				sprintf(GV_Audio_Codec, "%d", 1);
			else if(strcmp(tmpVal, "729") == 0)
				sprintf(GV_Audio_Codec, "%d", 4);
			else
				sprintf(GV_Audio_Codec, "%d", 0);
			
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"GV_Audio_Codec=%s.", GV_Audio_Codec);
			index = index + strlen("AUDIO_CODEC") - 1;
		}
		else
		if(strstr(zQuery + index, "VIDEO_CODEC") == zQuery + index)
		{
			getCodecValue(
								zQuery + index,
								tmpVal,
								"VIDEO_CODEC",
								'=',
								'^');

			if(strstr(tmpVal, "263"))
				sprintf(GV_Video_Codec, "%d", 2);	

		}
		else
		if(strstr(zQuery + index, "CALL") == zQuery + index)
		{
			getCodecValue(
								zQuery + index,
								tmpVal,
								"CALL",
								'=',
								'^');

			rc = atoi(tmpVal);

		}

	}

	
	return(rc);
}

#endif 


