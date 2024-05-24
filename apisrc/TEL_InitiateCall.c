/*-----------------------------------------------------------------------------
Program:        TEL_InitiateCall.c 
Purpose:        Direct the dynamic manager to initiate the call.
Author:         Wenzel-Joglekar
Date:		09/12/2000
Update:		10/17/2000 gpw broke out intiateCall for use by Bridge API
Update:		03/01/2001 gpw added code to send "busy" to monitor
Update: 04/09/01 APJ Added destination NULL validation.
Update: 04/09/01 APJ Added numOfRings validation.
Update: 04/27/2001 apj In parameter validation, put the missing value
			       passed in the log message.
Update: 05/02/2001 apj Added code to modify retcode for second party channel.
Update: 05/03/2001 apj Make 1-100 as valid range for numOfRings.
Update: 05/11/2001 apj Added code to modify retcode for second party channel.
Update: 06/27/2001 apj Populate ani field in Initiate call structure..
Update: 07/12/2001 apj Set GV_ConnectTime on success.
Update: 08/03/2001 APJ Changed GV_GatekeeperIP -> GV_DefaultGatewayIP.
Update: 08/02/2001 apj Call NotifyCaller from inside of tel_InitiateCall.
Update: 08/20/2008 js added SIP enum lookup functionality.
Update: 05/13/2010 djb removed declaration for parseInitiateCallMessageResponse
                       for TEL_SpeedDial
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

#ifdef CLIENT_GLOBAL_CROSSING
#warning "GLOBAL CROSSING ONLY"
#endif



/* max read timeout */
static int max_seconds = 0;

extern int in_bridge_cdr;

int parse_int(char *str, int *val)
{

   int rc = -1;
   char buff[32];
   int len = 0;

   while(*str && isdigit((int) *str) && (len < 32)){
        buff[len] = *str;
        len++; str++;
   }

   if(len){
      buff[len] = '\0';
      *val = atoi(buff);
      rc = 0;
   } 

//   if(rc == -1){
//     fprintf(stderr, " %s() could not parse number from string, string=[%s]\n", __func__, buff);
//   }

   return rc;
}

int parseInitiateCallMessageResponse(char *responseMessage)
{
	static char mod[] = "parseInitiateCallMessageResponse";
	char stringToFind[] = {"BANDWIDTH="};
	char    *zQuery = NULL;
	char    buf[MAX_APP_MESSAGE_DATA];	  
    char tmpVal[100];
	int rc = -1;
	char *ptr = NULL;
	int status;

	zQuery = (char*)strtok_r((char *)responseMessage, ",", (char**)&buf);

	if(zQuery == NULL)
	{
//	    fprintf(stderr, " %s() strtok returned null, returning -1. response message=[%s]\n", __func__, responseMessage);	
		return rc; // bail 
	}
	strcat(zQuery, "^");

    tmpVal[0] = '\0';
	ptr = strstr(zQuery, "AUDIO_CODEC"); 
    if(ptr){

		getCodecValue( ptr, tmpVal, "AUDIO_CODEC", '=', '^');

		if(strcmp(tmpVal, "711") == 0)
				sprintf(GV_Audio_Codec, "%d", 3);	
		else if(strcmp(tmpVal, "711r") == 0)
				sprintf(GV_Audio_Codec, "%d", 1);
		else if(strcmp(tmpVal, "729") == 0)
				sprintf(GV_Audio_Codec, "%d", 4);
		else
				sprintf(GV_Audio_Codec, "%d", 0);
			
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"GV_Audio_Codec=%s; AUDIO_CODEC=%s.",
					GV_Audio_Codec, tmpVal);
    }

    tmpVal[0] = '\0';
    ptr = strstr(zQuery, "VIDEO_CODEC");
	if(ptr){

	    getCodecValue( ptr, tmpVal, "VIDEO_CODEC", '=', '^');
		if(strstr(tmpVal, "263"))
			sprintf(GV_Video_Codec, "%d", 2);	

    }

    tmpVal[0] = '\0';
	ptr = strstr(zQuery, "CALL");
	if(ptr){
			getCodecValue(ptr,tmpVal,"CALL",'=','^');
			status = parse_int(tmpVal, &rc);
			if(status != 0){
			   rc = -99;
//fprintf(stderr, " %s() parse int returned %d, call str={%s} rc=%d\n", __func__, status, tmpVal, rc);
            }
    }  else {
         // we were sent only a number try and get it... joes Thu Apr 16 05:34:50 EDT 2015
         status = parse_int(zQuery, &rc);
         if(status == 0){
            return rc;
         }
    }

	return(rc);
}

int tel_InitiateCall(char *mod, int *connected, int *appCallNum, 
				int *retCode, int *channel, void *m,
				int callNotifyCaller)
{	
	int rc = 0;
	int lCallNum;
	int requestSent;
	struct Msg_InitiateCall msg;
	struct MsgToApp response;
	struct Msg_DropCall lMsgDropCall;
	time_t t1,t2;
    char portString[32];
    char lMsg[512];
	char *ptr = NULL;
	long tmpSecs;

	msg=*(struct Msg_InitiateCall *)m;

	*retCode = 21;
	requestSent=0;
	time(&t1);	
	memset(&GV_BridgeResponse, 0, sizeof(struct MsgToApp));
	while ( 1 )
	{
		if(callNotifyCaller == 1)
		{
			time(&t2);	
			if((t2 - t1)*1000 >= GV_NotifyCallerInterval) 
			{
				t1 = t2;
				GV_InsideNotifyCaller = 1;

#ifndef CLIENT_GLOBAL_CROSSING
				if(NotifyCaller)
#endif
				{
					rc = NotifyCaller(GV_Connected1 && GV_Connected2);
				}

				if(rc == -1)
	            {
                    telVarLog(mod,REPORT_DETAIL,TEL_BASE, GV_Info,
                        "NotifyCaller returned -1. Treating as far end disconnect. ");

                    GV_AgentDisct = NO;
                    GV_InsideNotifyCaller = 0;
                    return (-10);   /* DDN: 02/09/2010 */
                }

                if(!GV_Connected1)
                {
                    telVarLog(mod,REPORT_DETAIL,TEL_BASE, GV_Info,
                        "A Leg disconnected while B Leg was ringing. ");

                    return(disc(mod, msg.appCallNum));
                }


				GV_InsideNotifyCaller = 0;
			}

			if(GV_AgentDisct == YES)
			{
				lMsgDropCall.opcode = DMOP_DROPCALL;
				lMsgDropCall.appCallNum = GV_AppCallNum1;
				lMsgDropCall.appPid = GV_AppPid;
				lMsgDropCall.appRef = GV_AppRef++;
				lMsgDropCall.appPassword = GV_AppPassword;
				lMsgDropCall.allParties = 2;
			
		     		rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&lMsgDropCall);
                        	if(rc == -1) return(TEL_FAILURE);
				GV_AgentDisct = NO;
				return(-10);
			}
		}
	
		if(GV_BridgeResponse.opcode == DMOP_INITIATECALL)
		{
			memcpy(&response, &GV_BridgeResponse, 
				sizeof(struct MsgToApp));
			memset(&GV_BridgeResponse, 0, sizeof(struct MsgToApp));
		}
		else if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response,
							sizeof(response));
			if(rc == -1) return(TEL_FAILURE);
                        if(rc == -2)
			{
				rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&msg);
				if (rc == -1) return(-1);
				requestSent=1;

				if(callNotifyCaller == 1)
				{
					rc = readResponseFromDynMgr(mod,-1,
						&response,sizeof(response));
					if(rc == -1) return(TEL_FAILURE);
					if(rc == -2)
					{
						usleep(400);
						continue;
					}
				}
				else
				{
					rc = readResponseFromDynMgr(mod,max_seconds,
						&response,sizeof(response));
					if(rc == -1) return(TEL_FAILURE);
					if(rc == -2) return(TEL_TIMEOUT);
				}
			}
		}
		else
		{
			if(callNotifyCaller == 1)
			{
				rc = readResponseFromDynMgr(mod,-1,
					&response,sizeof(response));
				if(rc == -1) return(TEL_FAILURE);
				if(rc == -2)
				{
					usleep(400);
					continue;
				}
			}
			else
			{
				rc = readResponseFromDynMgr(mod,max_seconds,
					&response,sizeof(response));
				if(rc == -1) return(TEL_FAILURE);
				if(rc == -2) return(TEL_TIMEOUT);
			}
		}
		rc = examineDynMgrResponse(mod, (struct MsgToDM *)&msg, &response);	

		switch(rc)
		{
		case DMOP_INITIATECALL:
			if (response.returnCode >= 0)
			{
				lCallNum = parseInitiateCallMessageResponse(response.message);
				*connected=1;    
				//lCallNum=atoi(response.message); 
                // MR 4384 Video was removed so the string passed 
				// had to be parsed a different way Joe S. 
				*appCallNum = lCallNum;
				*channel = response.appCallNum;
				*retCode = response.returnCode;
        		telVarLog(mod,REPORT_VERBOSE,TEL_BASE, GV_Info,
        			"pid=%d assigned call # %d. appCallnum=%d",
                	response.appPid, response.appCallNum, *appCallNum);

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
        case DMOP_CALL_CONNECTED:
             //fprintf(stderr, " %s() get DMOP CALL CONNECTED writing CDR \n", __func__);
			 snprintf(GV_ANI, sizeof(GV_ANI), "%s", "Undefined");
		     ptr = strstr(response.message, "ANI=");
			 if(ptr){
               ptr += strlen("ANI=");
			   switch(*ptr){
                   case ' ':
					  snprintf(GV_ANI, sizeof(GV_ANI), "%s", "Undefined");
					  break;
				   default:
					  snprintf(GV_ANI, sizeof(GV_ANI), "%s", ptr);
					  break;
               }
             }
		     setTime(GV_ConnectTime, &GV_ConnectTimeSec);
		     writeStartCDRRecord(mod); // moved joe S 
             break;
		case DMOP_BRIDGE_CONNECTED:
             //fprintf(stderr, " %s() get DMOP BRIDGE CONNECTED writing CDR \n", __func__);
             setTime(GV_BridgeConnectTime,&tmpSecs);
             GV_BridgeConnectTimeSec = (int) tmpSecs;

			 if ( GV_BLegCDR == 1 )
			 {
             	sprintf (lMsg, "%s:BS:%s:%s:%s:%s:%s:%s",
	                   GV_CDRKey, GV_AppName, GV_ANI, GV_DNIS,
	                   GV_BridgeConnectTime, GV_CustData1, GV_CustData2);
	             sprintf (portString, "%d", response.appCallNum);
//				 LogARCMsg (mod, REPORT_CDR, portString, "TEL", GV_AppName, 20013, lMsg);
				 telVarLog(mod, REPORT_CDR, 20013, GV_Info, lMsg);
				 in_bridge_cdr = 1;
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
		} /* switch rc */
	} /* while */
}


/*
  Returns 0 for sucess and a -1 for a failure 
*/


static int parametersAreGood(char *mod, int numRings,
			int informat, const char *destination, char *editedDestination);

int TEL_InitiateCall(int numRings, int informat, const char *destination, 
		int *retCode, int *channel)
{
	static char mod[]="TEL_InitiateCall";
        char apiAndParms[MAX_LENGTH] = "";
        char editedDestination[MAX_LENGTH];
        char apiAndParmsFormat[]="%s(%d,%s,%s,%s,%s)";
	int rc;

	struct Msg_InitiateCall msg;

	char	yStrFileName[256];

	sprintf(apiAndParms,apiAndParmsFormat, mod, numRings,
		arc_val_tostr(informat), destination, "retcode", "channel");
        rc = apiInit(mod, TEL_INITIATECALL, apiAndParms, 1, 0, 0);
        if (rc != 0) HANDLE_RETURN(rc);

       if (!parametersAreGood(mod, numRings, informat, 
				destination, editedDestination))
                HANDLE_RETURN (TEL_FAILURE);
	
	msg.opcode = DMOP_INITIATECALL;
	msg.appCallNum = GV_AppCallNum1;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
   	msg.appPassword = GV_AppPassword;   
	msg.numRings = numRings;
	msg.resourceType[0] = 0;

    if(numRings > 0){
       max_seconds = numRings * 10;
    }

	if (informat==IP)
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
	else if(informat == E_164)
	{
		msg.informat = E_164;
		sprintf(msg.ipAddress, "%s", "\0");

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
	}
	else
	{
		msg.informat = NONE;
		sprintf(msg.ipAddress, "%s", GV_DefaultGatewayIP);

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
	}

	msg.whichOne = 1;	
	msg.appCallNum1 = GV_AppCallNum1;
	msg.appCallNum2 = GV_AppCallNum2;
	memset(msg.ani, 0, sizeof(msg.ani));

	strncpy(msg.ani, GV_CallingParty, sizeof(msg.ani)-1);

	sprintf(GV_ANI, "%s", msg.ani);
	telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
			 "Setting GV_ANI=%s", GV_ANI);

	sprintf(GV_DNIS, "%s", editedDestination);
	
	rc = tel_InitiateCall(mod, &GV_Connected1, &GV_AppCallNum1, 
				retCode, channel, (void *)&msg, 0);
	if (rc == 0) 
	{
		// setTime(GV_ConnectTime, &GV_ConnectTimeSec);
		if(GV_SipFrom[0] != '\0')
		{
			char *yPtrChrAt = NULL;
			sprintf(GV_ANI, "%s", GV_SipFrom);

			yPtrChrAt = strchr(GV_ANI, '@');
			if(yPtrChrAt != NULL)
			{
				yPtrChrAt[0] = 0;
			}

			telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
			 "Setting GV_ANI=%s", GV_ANI);
		}

		// writeStartCDRRecord(mod); moved joe S 
  		sprintf (GV_CDR_Key, "%s", GV_CDRKey);
		if (GV_DiAgOn_ == 0)/* application is not under monitor */
				send_to_monitor(MON_STATUS, STATUS_BUSY, "");
	}
	HANDLE_RETURN(rc);
}


static int parametersAreGood(char *mod,int numRings,int informat,
			const char *destination, char *editedDestination)
{
	char m[MAX_LENGTH];         
	int errors=0;
	int numRingsMin = 1;
	int numRingsMax = 100;
	char informatValues[]="NAMERICAN, NONE";

	if ((numRings < numRingsMin) || (numRings > numRingsMax))
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"Invalid value for numRings: %d. Valid values are %d to %d",
		numRings, numRingsMin, numRingsMax);
	}

	if(destination == NULL)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, "%s",
		"Invalid value for destination: NULL.");
	}
	
        switch(informat)
        {
        case NONE:
			if ( (strchr(destination, '@') != NULL ) &&
			     (strchr(destination, '.') != NULL ) )
			{
				errors++;
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
				 "Invalid informat (NONE) with url destination (%s). "
				 "Option NONE requires pstn format.", destination);
			}
        case NAMERICAN:
		case IP:
		case SIP:
        case E_164:
                if (!goodDestination(mod,informat,(char *)destination,editedDestination))
                {
                        errors++;
			/* Message written in subroutine */
                }
                break;
        default:
                errors++;
 	telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        	"Invalid value for informat: %d. Valid values are %s.",
		informat, informatValues);
		break;
        }

	return( errors == 0);
}
