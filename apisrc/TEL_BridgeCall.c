/*-----------------------------------------------------------------------------
Program:        TEL_BridgeCall.c
Purpose:        Bridge two calls together
Author:         Wenzel-Joglekar
Date:		10/17/2000
Update: 03/29/01 APJ Added code for showing B-Leg on monitor.
Update: 04/09/01 APJ Added destination and resourceType NULL validation.
Update: 04/27/2001 apj In parameter validation, put the missing value
		       passed in the log message.
Update: 05/02/2001 apj Added code to modify retcode for second party channel.
Update: 05/03/2001 apj Make 1-100 as valid range for numOfRings.
Update: 05/07/2001 apj For CONNECT, first check for successful DIAL_OUT.
Update: 05/16/2001 apj Added code to act on GV_AgentDisct setting.
Update: 06/01/2001 mpb NotifyCaller called after GV_NotifyCallerInterval.
Update: 06/27/2001 apj Populate ani field in Initiate call structure..
Update: 08/03/2001 APJ Changed GV_GatekeeperIP -> GV_DefaultGatewayIP.
Update: 08/02/2001 apj Pass notifyCaller parameter to tel_InitiateCall.
Update: 06/18/2004 apj Modified APPL_SIGNAL for caller port also.
Update: 07/20/2005 ddn No INIT, N/A when GV_Connected2 becomes 0
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "TEL_Globals.h"

extern int tel_InitiateCall(char *mod, int *connected, int *appCallNum,
                int *retCode, int *channel, void *m, int callNotifyCaller);


extern int in_bridge_cdr;

static int parametersAreGood(char *mod, int option, int numRings, 
				int informat, const char *destination, 
				const char *resourceType, 
				char *editedDestination);

int TEL_BridgeCall(int option, int numRings,
                        int informat, const char *destination,
                        const char *resourceType, int *retcode,
                        int *channel)
{
	static char mod[]="TEL_BridgeCall";
	int notifyParm;
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s(%s,%d,%s,%s,%s,%s,%s)";
	char editedDestination[MAX_LENGTH];
        int  rc;
	char buf[10];
	int  keepAliveCounter = 0;
	time_t t1, t2;
	
	struct Msg_InitiateCall msg;
	struct MsgToApp response;
	struct Msg_DropCall lMsgDropCall;
	char	yStrFileName[256];

	sprintf(apiAndParms,apiAndParmsFormat, mod, arc_val_tostr(option),
                numRings, arc_val_tostr(informat), destination,
		resourceType, "retcode", "channel");

	rc = apiInit(mod, TEL_BRIDGECALL, apiAndParms, 1, 1, 0);
        if (rc != 0) HANDLE_RETURN(rc);
	
	*retcode = 21;
	if (!parametersAreGood(mod, option, numRings, informat,
                        destination, resourceType, editedDestination))
        	HANDLE_RETURN (TEL_FAILURE);
        
	msg.resourceType[0] = 0;
	msg.appCallNum 	= GV_AppCallNum1;
       	msg.appPid 	= GV_AppPid;
       	msg.appPassword = GV_AppPassword;

printf("%s %d TEL_BridgeCall destination=(%s) editted=(%s)\n", __FILE__, __LINE__, 
			destination, editedDestination);
fflush(stdout);

	switch(option)
	{ 
	case DIAL_OUT:
	case ALL:
	case LISTEN_ALL	:
	case LISTEN_IGNORE	:
		msg.opcode 	= DMOP_INITIATECALL;
   		msg.appRef 	= GV_AppRef++;
		msg.numRings 	= numRings;
		if (informat == IP)
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
		*retcode = 0;
		if(option == DIAL_OUT)	
		{
// printf("%s %d msg.phoneNumber=(%s)\n", __FILE__, __LINE__, msg.phoneNumber);
			rc = tel_InitiateCall(mod, &GV_Connected2, 
				&GV_AppCallNum2, retcode, channel, 
				(void *)&msg, 0);
		}
		else
		{
			rc = tel_InitiateCall(mod, &GV_Connected2, 
				&GV_AppCallNum2, retcode, channel, 
				(void *)&msg, 1);
		}

//printf("%s %d TEL_BridgeCall\n", __FILE__, __LINE__);fflush(stdout);

		if(rc == TEL_SUCCESS)
		{
			writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_PID, GV_AppPid);
			writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_STATUS, STATUS_BUSY);
			writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_SIGNAL, 1);
			sprintf(buf, "ASSIGNED:%d", GV_AppCallNum1);
			writeStringToSharedMemory(mod, GV_AppCallNum2,  buf);
//fprintf(stderr, " %s() writing assigned string to shm on app call num2[%d]\n", __func__, GV_AppCallNum2);
		}
        	if (option==DIAL_OUT || rc != TEL_SUCCESS) 
			{
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"BridgeCall returning rc=%d option=%d, con1=%d con2=%d retcode=%d  channel=%d.",
					rc, option, GV_Connected1, GV_Connected2, *retcode, *channel);
				HANDLE_RETURN(rc); 
			}
		/* No break should be here */
	case CONNECT:
		msg.opcode 	= DMOP_BRIDGECONNECT;
       		msg.appRef 	= GV_AppRef++;
		rc = connectCalls(mod, (void *)&msg);
        	if (rc != TEL_SUCCESS) 
			HANDLE_RETURN(rc); 
		*retcode = 0;
		break;
	}

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"BridgeCall entering NotifyCaller C1=%d C2=%d.",
			GV_Connected1, GV_Connected2);
 

	time(&t1);	
	while (GV_Connected1 && GV_Connected2)
	{
		usleep(400);
		if(GV_AgentDisct == YES)
		{
			lMsgDropCall.opcode = DMOP_DROPCALL;
			lMsgDropCall.appCallNum = GV_AppCallNum2;
			lMsgDropCall.appPid = GV_AppPid;
			lMsgDropCall.appRef = GV_AppRef++;
			lMsgDropCall.appPassword = GV_AppPassword;
			lMsgDropCall.allParties = 0;
			
		     	rc = sendRequestToDynMgr(mod, (struct MsgToDM *) &lMsgDropCall);
                        if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			GV_AgentDisct = NO;
		}

		keepAliveCounter++;
		if(keepAliveCounter >= 25)
		{
			writeFieldToSharedMemory(mod, GV_AppCallNum1, 
					APPL_SIGNAL, 1);
			writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_SIGNAL, 1);
			keepAliveCounter = 0;
		}

		time(&t2);	
		if((t2 - t1)*1000 >= GV_NotifyCallerInterval) 
		{
			t1 = t2;

			GV_InsideNotifyCaller = 1;

#ifndef CLIENT_GLOBAL_CROSSING
			if(NotifyCaller)
#endif
				NotifyCaller(GV_Connected1 && GV_Connected2);

			GV_InsideNotifyCaller = 0;
		}

		if (!GV_Connected1) HANDLE_RETURN(TEL_DISCONNECTED);
		if (!GV_Connected2) 
		{
			writeFieldToSharedMemory(mod, GV_AppCallNum2, APPL_SIGNAL, 1);
			GV_AppCallNum2 = NULL_APP_CALL_NUM;
			HANDLE_RETURN(-10);
		}

		rc = readResponseFromDynMgr(mod,-1,&response,sizeof(response));

		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		if(rc == -2) continue;
		/* Note: The msg here is actually irrelevant, but serves
		the appropriate purpose in checking for dtmf of disc. */
		rc = examineDynMgrResponse(mod, (struct MsgToDM *)&msg, &response);	
		switch(rc)
		{
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum, response.message);
			break;

        /*
            This DMOP message was added to set some vars and write a CDR with the right timestamp on it 
			Joe S.  Thu Jul 16 07:33:25 EDT 2015
        */
        case DMOP_BRIDGE_CONNECTED:
            // fprintf(stderr, " %s() writing bridge start CDR ----  \n", __func__);
			in_bridge_cdr = 1;
            break;
		case DMOP_DISCONNECT:
			rc = disc(mod, response.appCallNum);
			if(rc == -10)
			{
				//writeFieldToSharedMemory(mod, GV_AppCallNum2, 
				//	APPL_PID, 0);
				writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_STATUS, STATUS_IDLE);
				writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_SIGNAL, 1);
				//sprintf(buf, "%s", "N/A");
				//writeStringToSharedMemory(mod, GV_AppCallNum2,
  				//			buf);
				GV_AppCallNum2 = NULL_APP_CALL_NUM;
			}
                        HANDLE_RETURN(rc);
			break;
		default:
			/* Unexpected message. Logged in examineDynMgr... */
			break;
		} /* switch rc */
	}

	if (!GV_Connected1) 
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"BridgeCall exitting NotifyCaller C1=%d C2=%d.",
			GV_Connected1, GV_Connected2);

		HANDLE_RETURN(TEL_DISCONNECTED);
	}

	if (!GV_Connected2) 
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"BridgeCall exiting NotifyCaller C1=%d C2=%d.",
			GV_Connected1, GV_Connected2);

		writeFieldToSharedMemory(mod, GV_AppCallNum2, 
				APPL_SIGNAL, 1);

		GV_AppCallNum2 = NULL_APP_CALL_NUM;
		HANDLE_RETURN(-10);
	}

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"BridgeCall exitting NotifyCaller C1=%d C2=%d.",
		GV_Connected1, GV_Connected2);
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
	char optionValues[]="DIAL_OUT, CONNECT, ALL, LISTEN_ALL, LISTEN_IGNORE";
	int  numRingsMin=1;
	int  numRingsMax=100;
	char informatValues[]="NAMERICAN, NONE";

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
	
//	if(resourceType == NULL)
//	{
//		errors++;
//		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, "%s",
//		"Invalid value for resourceType: NULL.");
//	}
	
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
		if (!goodDestination(mod,informat,(char *)destination,editedDestination))
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
/*---------------------------------------------------------------------------
This function connects two calls together. 
---------------------------------------------------------------------------*/
int connectCalls(char *mod, void *m)
{	
	int rc;
	int lCallNum;
	struct Msg_BridgeConnect msg;
	struct MsgToApp response;

	msg=*(struct Msg_BridgeConnect *)m;

        rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&msg);
        if (rc == -1) return(-1);
        
	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,0,&response,sizeof(response));
		if(rc == -1) return(TEL_FAILURE);
	
		rc = examineDynMgrResponse(mod, (struct MsgToDM *)&msg, &response);	
		switch(rc)
		{
		case DMOP_BRIDGECONNECT:
			if (response.returnCode == 0)
			{
        			telVarLog(mod,REPORT_VERBOSE,TEL_BASE,GV_Info,
        				"pid=%d connected calls %d & %d.",
                			response.appPid, 
					GV_AppCallNum1, GV_AppCallNum2);
			}
			return(response.returnCode);
			break;
		case DMOP_GETDTMF:
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

