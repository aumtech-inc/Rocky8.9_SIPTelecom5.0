/*For SIP ONLY*/
#include "TEL_Common.h"
#include "TEL_Globals.h"

static int parametersAreGood(char *mod, int option, int numRings, 
				int informat, const char *destination, 
				const char *resourceType, 
				char *editedDestination);

int TEL_BridgeThirdLeg(int option, int numRings,
					int informat, const char *destination,
					const char *resourceType, int *retcode,
					int *channel)
{
	static char mod[]="TEL_BridgeThirdLeg";
	int notifyParm;
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s(%s,%d,%s,%s,%s,%s,%s)";
	char editedDestination[MAX_LENGTH];
	int  rc;
	char buf[10];
	int  keepAliveCounter = 0;
	int  requestSent = 0;
	time_t t1, t2;
	
	struct Msg_BridgeThirdLeg msg;
	struct MsgToApp response;
	struct Msg_DropCall lMsgDropCall;


	if(GV_AppCallNum1 < 0 || GV_AppCallNum2 < 0)
	{
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
					"Failed to call TEL_BridgeThirdLeg"
					" GV_AppCallNum1=%d, GV_AppCallNum2=%d",
					GV_AppCallNum1, GV_AppCallNum2);
		HANDLE_RETURN (TEL_FAILURE);
	}


	telVarLog(mod,REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
			"Got TEL_BridgeThirdLeg with <option=%d><numRings=%d>"
			"<informat=%d><destination=%s><resourceType=%s><retcode=%d><channel=%d>", 
			option, numRings,
			informat, destination,
			resourceType, *retcode,
			*channel);


	sprintf(apiAndParms,apiAndParmsFormat, mod, arc_val_tostr(option),
			numRings, arc_val_tostr(informat), destination,
			resourceType, "retcode", "channel");

	rc = apiInit(mod, TEL_BRIDGECALL, apiAndParms, 1, 1, 0);
        
	if (rc != 0) HANDLE_RETURN(rc);
	
	*retcode = 21;
	if (!parametersAreGood(mod, option, numRings, informat,
                        destination, resourceType, editedDestination))
        	HANDLE_RETURN (TEL_FAILURE);
        	
	msg.appCallNum 	= GV_AppCallNum1;
	msg.appPid 	= GV_AppPid;
	msg.appPassword = GV_AppPassword;
	msg.listenType = 0;
	msg.informat = informat;

	switch(option)
	{ 
		case DIAL_OUT:
		case ALL:
		case LISTEN_ALL:
		case LISTEN_IGNORE:
		case BLIND:
		case ANALOG_HAIRPIN:
		case CONNECT:

			msg.opcode 		= DMOP_BRIDGE_THIRD_LEG;
    		msg.appRef 		= GV_AppRef++;
			msg.numRings 	= numRings;

			if (informat == IP)
			{
				sprintf(msg.ipAddress, "%s", editedDestination);
                	memset(msg.phoneNumber, '\0', sizeof(msg.phoneNumber));
			}
			else
			{
				sprintf(msg.ipAddress, "%s", GV_DefaultGatewayIP);
				sprintf(msg.phoneNumber, "%s", editedDestination);
			}

			msg.whichOne 	= 2;
			msg.appCallNum1 = GV_AppCallNum1;
			msg.appCallNum2 = GV_AppCallNum2;
			msg.appCallNum3 = GV_AppCallNum3;

			memset(msg.ani, 0, sizeof(msg.ani));
			strncpy(msg.ani, GV_CallingParty, sizeof(msg.ani)-1);

			if(option == DIAL_OUT)
			{
				msg.listenType = DIAL_OUT;
			}
			else if(option == CONNECT)
			{
				msg.listenType = CONNECT;
			}
			else
			{
				msg.listenType = BLIND;
			}

			//sprintf(msg.resourceType, "%s", "");
	}

	requestSent=0;
	int appCallNum1[3] = {-99, -99, -99};
	int i = 0;;
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
			case DMOP_BRIDGE_THIRD_LEG:

				GV_AppCallNum3 = -1;

				sscanf(response.message, "^%d^%d^%d", &appCallNum1[0], &appCallNum1[1], &appCallNum1[2]);

				telVarLog(mod,REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
					"appCallNum1=%d , appCallNum2=%d, appCallNum3=%d", 
					appCallNum1[0], appCallNum1[1], appCallNum1[2]);

				for(i=0; i<3; i++)
				{
					if(appCallNum1[i] != GV_AppCallNum1)
					{
						if(appCallNum1[i] != GV_AppCallNum2)
						{
							GV_AppCallNum3 = appCallNum1[i];
							break;
						}
					}
				}

				*channel = GV_AppCallNum3;	//DDN: Added MR2961 04/02/2010
				
				if(GV_AppCallNum3 == -99)
				{
					telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"Invalid third leg GV_AppCallNum3=%d", GV_AppCallNum3);

					if(response.returnCode != -1)
					{
						*retcode = -1 * response.returnCode;
					}
					else
					{
						*retcode = response.returnCode;
					}

					HANDLE_RETURN(-1);
				}

				*retcode = 0;

				if(GV_AppCallNum3 >= 0 && response.returnCode >= 0)
				{

					writeFieldToSharedMemory(mod, GV_AppCallNum3, 
						APPL_PID, GV_AppPid);

					writeFieldToSharedMemory(mod, GV_AppCallNum3, 
						APPL_STATUS, STATUS_BUSY);

					writeFieldToSharedMemory(mod, GV_AppCallNum3, 
						APPL_SIGNAL, 1);

					sprintf(buf, "ASSIGNED:%d", GV_AppCallNum1);

					writeStringToSharedMemory(mod, GV_AppCallNum3,  buf);
				}

				
				if (response.returnCode >= 0)
				{
					*retcode = response.returnCode;
				}
				else
				{
					if(response.returnCode != -1)
						*retcode = -1 * response.returnCode;
					else
						*retcode = response.returnCode;

					HANDLE_RETURN(-1);
				}

				//HANDLE_RETURN(response.returnCode);
				continue;
				//break;
		
			case DMOP_GETDTMF:
				saveTypeAheadData(mod, response.appCallNum, response.message);
				break;
		
			case DMOP_DISCONNECT:
				if(response.appCallNum == GV_AppCallNum2)
				{
					GV_AppCallNum2 = GV_AppCallNum3;
				}
				GV_AppCallNum3 = NULL_APP_CALL_NUM;
					
				//writeFieldToSharedMemory(mod, response.appCallNum, 
				//		APPL_PID, 0);
				//writeFieldToSharedMemory(mod, response.appCallNum, 
				//		APPL_STATUS, STATUS_INIT);
				writeFieldToSharedMemory(mod, response.appCallNum, 
						APPL_STATUS, STATUS_IDLE);
				writeFieldToSharedMemory(mod, response.appCallNum, 
						APPL_SIGNAL, 1);
				//sprintf(buf, "%s", "N/A");
				//writeStringToSharedMemory(mod, response.appCallNum,
  				//				buf);

				HANDLE_RETURN(TEL_SUCCESS);	
				//HANDLE_RETURN(disc(mod, response.appCallNum));
				break;
		
			default:
				/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		} /* switch rc */
	} /* while */

	HANDLE_RETURN(0);
}

/*-------------------------------------------------------------------------
This function verifies that all parameters are valid and sets any global
variables necessary for execution. It returns 1 (yes) if all parmeters are
good, 0 (no) if not.
-------------------------------------------------------------------------*/
static int parametersAreGood(char *mod, int option, int numRings, 
			int informat, const char *destination, 
			const char *resourceType, char *editedDestination)
{
	int  errors=0;
	char termCharHolder[2];	
	char optionValues[]="BLIND";
	int  numRingsMin=1;
	int  numRingsMax=100;
	char informatValues[]="NAMERICAN, NONE";

	switch(option)
	{
		case BLIND:
		case CONNECT:
		case DIAL_OUT:
			break;
	
		case ALL:
		case LISTEN_ALL:
		case LISTEN_IGNORE:
		case ANALOG_HAIRPIN:
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

	switch(informat)
	{
		case NONE:
		case NAMERICAN:
		case IP:
		case SIP:
			if (!goodDestination(mod,informat,destination,editedDestination))
			{
				errors++;
			/* Message written in subroutine. */
			}	
			break;
		default:
			errors++;
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
					"Invalid value for informat: %d. Valid values are %s.", 
					informat, informatValues);
			break;
	}

	return( errors == 0);
}
