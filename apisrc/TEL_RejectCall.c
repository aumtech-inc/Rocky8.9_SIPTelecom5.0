#ident  "@(#)TEL_RejectCall 96/03/14 2.2.0 Copyright 1996 Aumtech"
static char ModuleName[] = "TEL_RejectCall";
/*-----------------------------------------------------------------------------
Program:	TEL_RejectCall.c
Purpose:	To reject the incoming call.
Author: 	Madhav Bhide
Date:		10/03/02
Update: 10/04/2002 ddn Changed the code to fit into IP structure.
Update: 11/25/2002 ddn	Added SNMP_SerRequest 
Update: 02/22/2003 apj Added TEL_CALL_SUSPENDED code.
Update: 02/24/2003 apj Call tellResp1stPartyDisconnected on success.
Update: 04/25/2003 apj Pass msgId of 20012 in writeCustomCDRRecord.
Update: 05/19/2003 apj Handle DMOOP_DISCONNECT from the DM.
Update: 03/29/2003 apj Make errBuf zise 512 from 128.
Update: 03/29/2003 apj If SNMP_SetRequest, log the error message.
Update:     08/28/2003 ddn  Added TEL_CALL_RESUMED code
Update:		09/03/2003 apj If SNMP_SetRequest has failed b'cause it is not 
						   configured then log a DETAIL msg instead of NORMAL.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, int numRings);

/*-----------------------------------------------------------------------------
This routine will reject the call.  (works for ISDN)
-----------------------------------------------------------------------------*/
int TEL_RejectCall(char * causeCode)
{
	static char mod[]="TEL_RejectCall";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;

	int		ret_code = 0;
	char	diag_mesg[128];
	int		yIntCauseCode = 0;
	int		value = 0;
	char	errBuf[512];
	int 	rcSNMP;

	struct MsgToApp response;
	struct Msg_RejectCall lMsg;
	struct MsgToDM msg; 

	lMsg.opcode 		= DMOP_REJECTCALL;
	lMsg.appCallNum 	= GV_AppCallNum1;
	lMsg.appPid 		= GV_AppPid;
	lMsg.appRef 		= GV_AppRef++;
	lMsg.appPassword 	= GV_AppPassword;
	
	sprintf(LAST_API,"%s",ModuleName);

	sprintf(apiAndParms, "%s(&CauseCode)", mod);

	rc = apiInit(mod, TEL_REJECTCALL, apiAndParms, 1, 0, 0);
	if (rc != 0) HANDLE_RETURN(rc);


	/*
	 *	Validate the cause code....
	 *  If it is an invalid code, set the causeCode to NORMAL_CLEARING
	 */

	if(causeCode == 0 || *causeCode == '\0')
	{
		sprintf(lMsg.causeCode, "%s", "GC_NORMAL_CLEARING");
		sprintf(diag_mesg,
			"Invalid cause code(%s). Setting it to NORMAL_CLEARING.",
			causeCode);
	}
	else if(strcmp(causeCode, "UNASSIGNED_NUMBER" ) == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_UNASSIGNED_NUMBER");
	else if(strcmp(causeCode, "NORMAL_CLEARING") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_NORMAL_CLEARING");
	else if(strcmp(causeCode, "CHANNEL_UNACCEPTABLE") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_CHANNEL_UNACCEPTABLE");
	else if(strcmp(causeCode, "USER_BUSY") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_USER_BUSY");
	else if(strcmp(causeCode, "CALL_REJECTED") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_CALL_REJECTED") ;
	else if(strcmp(causeCode, "DEST_OUT_OF_ORDER") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_DEST_OUT_OF_ORDER") ;
	else if(strcmp(causeCode, "NETWORK_CONGESTION") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_NETWORK_CONGESTION") ;
	else if(strcmp(causeCode, "REQ_CHANNEL_NOT_AVAIL") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_REQ_CHANNEL_NOT_AVAIL");
	else if(strcmp(causeCode, "REQ_CHANNEL_NOT_AVAILABLE") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_REQ_CHANNEL_NOT_AVAIL");
	else if(strcmp(causeCode, "SEND_SIT") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_SEND_SIT");
	else if(strcmp(causeCode, "SUBSCRIBER_ABSENT") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_SUBSCRIBER_ABSENT");
	else if(strcmp(causeCode, "NO_ANSWER_FROM_USER") == 0) 
				sprintf(lMsg.causeCode, "%s", "GC_NO_ANSWER_FROM_USER");
	else
	{
		sprintf(lMsg.causeCode, "%s", "GC_NORMAL_CLEARING");
		sprintf(diag_mesg,
			"Invalid cause code(%s). Setting it to NORMAL_CLEARING.",
			causeCode);
    	telVarLog(ModuleName, REPORT_NORMAL, TEL_LOGMGR_ERROR, GV_Err, diag_mesg); 
	} /*END: Validate the cause code...*/

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_RejectCall));
	rc = sendRequestToDynMgr(mod, &msg); 
	if (rc == -1) HANDLE_RETURN(-1);

	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,60,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Timeout waiting for response from Dynamic Manager.");
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);

		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
			case DMOP_REJECTCALL:
				if (response.returnCode == 0)
				{
					setTime(GV_ConnectTime, &GV_ConnectTimeSec);
					writeCustomCDRRecord(mod, "RC", GV_RingEventTime, 20012);

					rcSNMP = 1;

                     //SNMP_SetRequest("", "", SNMP_INCREMENT_VAR,
					//			TC_INBOUND_CALL_REJECTED, GV_AppCallNum1, 
					//			&value, errBuf);
					if(rcSNMP == 1)
					{
						telVarLog(mod,REPORT_DETAIL, TEL_BASE, GV_Err, errBuf);
					}
					else if(rcSNMP != 0)
					{
						telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err, errBuf);
					}

					tellResp1stPartyDisconnected(mod, 
						response.appCallNum, response.appPid);
				}
				HANDLE_RETURN(response.returnCode);
				break;
#if 0
			case DMOP_SUSPENDCALL:
            	HANDLE_RETURN(TEL_CALL_SUSPENDED);
				break;
			case DMOP_RESUMECALL:
            	HANDLE_RETURN(TEL_CALL_RESUMED);
				break;
#endif
			case DMOP_DISCONNECT:
//				HANDLE_RETURN(disc(mod, response.appCallNum, GV_AppCallNum1, GV_AppCallNum2));
				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;
			default:
				/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		} /* switch rc */
	} /* while */

	HNDL_RETURN(0);
}
