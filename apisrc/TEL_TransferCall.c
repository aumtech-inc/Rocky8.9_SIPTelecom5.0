/*For SIP ONLY*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, int option, int numRings, 
				int informat, const char *destination, 
				const char *resourceType, 
				char *editedDestination);

int TEL_TransferCall(int option, int numRings,
                     int informat, const char *destination,
                     const char *resourceType, int *retcode,
                        int *channel)
{
	static char mod[]="TEL_TransferCall";
	int notifyParm;
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s(%s,%d,%s,%s,%s,%s,%s)";
	char editedDestination[MAX_LENGTH];
        int  rc;
	char buf[10];
	int  keepAliveCounter = 0;
	int  requestSent = 0;
	time_t t1, t2;
	
	struct Msg_InitiateCall msg;
	struct MsgToApp response;
	struct Msg_DropCall lMsgDropCall;

	char yStrFileName[256];

	telVarLog(mod,REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
			"Got TEL_TransferCall with <option=%d><numRings=%d>"
			"<informat=%d><destination=%s><resourceType=%s><retcode=%d><channel=%d>.", 
			option, numRings,
			informat, destination,
			resourceType, *retcode,
			*channel);


	sprintf(apiAndParms,apiAndParmsFormat, mod, arc_val_tostr(option),
                numRings, arc_val_tostr(informat), destination,
	resourceType, "retcode", "channel");

	rc = apiInit(mod, TEL_TRANSFERCALL, apiAndParms, 1, 1, 0);
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

		msg.opcode 		= DMOP_TRANSFERCALL;
    	msg.appRef 		= GV_AppRef++;
		msg.numRings 	= numRings;

		if (informat == IP)
		{
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
			memset(msg.phoneNumber, '\0', sizeof(msg.phoneNumber));
		}
		else
		{
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

       	msg.whichOne 	= 2;
       	msg.appCallNum1 = GV_AppCallNum1;
       	msg.appCallNum2 = GV_AppCallNum2;

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
				case DMOP_TRANSFERCALL:
						
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

#if 0
	if(resourceType == NULL)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, "%s",
		"Invalid value for resourceType: NULL.");
	}
#endif
	
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
