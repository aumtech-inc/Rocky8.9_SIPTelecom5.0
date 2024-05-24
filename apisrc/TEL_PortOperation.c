/*-----------------------------------------------------------------------------
Program:        TEL_PortOperation.c
Purpose:        Perform an operation on a port, actually on a call number.
Author:         Wenzel-Joglekar
Date:		10/28/2000
Update: 05/07/2001 apj Removed some of the unwanted messages.
Update: 03/09/2004 apj Make party1Required.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, int opCode, int callNum);

int TEL_PortOperation(int opCode, int callNum, int *status)
{
	static char mod[]="TEL_PortOperation";
	char apiAndParms[MAX_LENGTH] = "";
	char editedDestination[MAX_LENGTH];
	char apiAndParmsFormat[]="%s(%d,%d,status)";
	int rc;
	int requestSent;

 	struct MsgToApp response; 
	struct Msg_PortOperation msg;

	sprintf(apiAndParms,apiAndParmsFormat, mod, opCode, callNum);
	rc = apiInit(mod, TEL_PORTOPERATION, apiAndParms, 1, 1, 0);
	if (rc != 0) HANDLE_RETURN(rc);

	if (!parametersAreGood(mod, opCode, callNum))
		HANDLE_RETURN (TEL_FAILURE);
	
	msg.opcode = DMOP_PORTOPERATION;
	msg.appCallNum = GV_AppCallNum1;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
   	msg.appPassword = GV_AppPassword;   
	msg.operation = opCode;
	msg.callNum = callNum;

	/*DDN: 07272011 For Global Crossing*/
	switch(msg.operation)
	{
		case GET_CONNECTION_STATUS:
				break;
		case GET_FUNCTIONAL_STATUS:
				break;
		case GET_QUEUE_STATUS:
				break;
		case CLEAR_DTMF:
#ifdef CLIENT_GLOBAL_CROSSING
			memset(GV_TypeAheadBuffer1, '\0', sizeof(GV_TypeAheadBuffer1));
#endif
				break;
		case WAIT_FOR_PLAY_END:
				break;
		case STOP_ACTIVITY:
				break;
	}

	requestSent=0;
  	while ( 1 )
	{
		if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response, sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			if(rc == -2)
			{
				rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&msg);
				if (rc == -1) HANDLE_RETURN(-1);
				requestSent=1;
				rc = readResponseFromDynMgr(mod,0, &response,sizeof(response));
				if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,0, &response,sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		}

		rc = examineDynMgrResponse (mod, (struct MsgToDM *)&msg, &response);
		switch(rc)
		{
			case DMOP_PORTOPERATION:
				switch(msg.operation)
				{
					case GET_CONNECTION_STATUS:
					case GET_FUNCTIONAL_STATUS:
					case GET_QUEUE_STATUS:
						if (response.returnCode==0)
						{
							sscanf(response.message,"%d", status); ; 
						}
						HANDLE_RETURN(response.returnCode);
						break;

					case CLEAR_DTMF:
#ifdef CLIENT_GLOBAL_CROSSING
						memset(GV_TypeAheadBuffer1, '\0', sizeof(GV_TypeAheadBuffer1));
#endif
					case WAIT_FOR_PLAY_END:
					case STOP_ACTIVITY:
						HANDLE_RETURN(response.returnCode);
						break;
				}
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
}


static int parametersAreGood(char *mod,int opCode, int callNum)
{
	char m[MAX_LENGTH];         
	int errors=0;
	
	char opCodeValues[]="GET_CONNECTION_STATUS, GET_FUNCTIONAL_STATUS, CLEAR_DTMF, STOP_ACTIVITY, WAIT_FOR_PLAY_END";

	char callNumValues[]="0 or greater";
       
	switch(opCode)
	{
		case GET_CONNECTION_STATUS:
		case CLEAR_DTMF:
		case GET_FUNCTIONAL_STATUS:
		case STOP_ACTIVITY:
		case WAIT_FOR_PLAY_END:
		case GET_QUEUE_STATUS:
			break;
		default:
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"Invalid value for opCode: %d. Valid values are %s.", 
				opCode, opCodeValues);
			break;
	}

	if (callNum < 0)
	{
		errors++;
 		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        	"Invalid value for callNum: %d. Valid values are %s.",
		callNum, callNumValues);
	}

	return( errors == 0);
}
