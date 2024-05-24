/*-----------------------------------------------------------------------------
Program:        TEL_DropCall.c
Purpose:        Drops a leg of a call.
Author:         Wenzel-Joglekar
Date:		10/23/2000
Update:		03/27/2001 apj Code changes because DMOP_DROPCALL will never 
			       show up. only DMOP_DISCONNECT will come.
Update:		04/09/2001 apj Changed comparison with NULL_APP_CALL_NUM not -99
Update: 	04/27/2001 apj In parameter validation, put the missing value
			       passed in the log message.
Update: 	08/06/2001 apj Allow second party drop while ringing.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, int party);

int TEL_DropCall(int party) 
{
	static char mod[]="TEL_DropCall";
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s(%s)";
	int rc;
	int requestSent;

	struct MsgToApp response;
	struct Msg_DropCall msg;
       
        sprintf(apiAndParms,apiAndParmsFormat, mod, arc_val_tostr(party));
	/* ?? Do we need to adjust apiInit parm for party ?? */
        rc = apiInit(mod, TEL_DROPCALL, apiAndParms, 1, 1, 0);
        if (rc != 0) HANDLE_RETURN(rc);

	if (!parametersAreGood(mod, party))
		HANDLE_RETURN (TEL_FAILURE);

	if((GV_InsideNotifyCaller == 1) &&
	   (GV_AppCallNum2 == NULL_APP_CALL_NUM))
	{
		GV_AgentDisct = YES;
		HANDLE_RETURN(TEL_SUCCESS);
	}

	msg.opcode = DMOP_DROPCALL;
	if (party == FIRST_PARTY)
		msg.appCallNum = GV_AppCallNum1;
	else
		msg.appCallNum = GV_AppCallNum2;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;
	
	requestSent=0;
	while ( 1 )
	{
		if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response,
							sizeof(response));
                        if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
                        if(rc == -2)
                        {
                                rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&msg);
                                if (rc == -1) HANDLE_RETURN(-1);
                                requestSent=1;
                                rc = readResponseFromDynMgr(mod,0,
                                                &response,sizeof(response));
                                if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
                        }
                }
                else
                {
                        rc = readResponseFromDynMgr(mod,0,
                                        &response,sizeof(response));
                        if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
                }

        GV_TerminatedViaExitTelecom=1;

		rc = examineDynMgrResponse(mod, (struct MsgToDM *)&msg, &response);	
		switch(rc)
		{
		case DMOP_DISCONNECT:
			/* Do we need to close fifos here ?? */
			HANDLE_RETURN(0);
			break;
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, 
				response.appCallNum,response.message);
			break;
		default:
			/* Unexpected message. Logged in examineDynMgr... */
			continue;
			break;
		} /* switch rc */
	} /* while */
}

static int parametersAreGood(char *mod, int party)
{
	int errors=0;
	char partyValidValues[]="FIRST_PARTY, SECOND_PARTY";

	switch(party)
	{
	case FIRST_PARTY:
		if (GV_AppCallNum1 == NULL_APP_CALL_NUM)
		{
			errors++;
		 	telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err,
				"No 1st party call up.");
		}
		break;
	case SECOND_PARTY:
		if((GV_InsideNotifyCaller != 1) &&
		   (GV_AppCallNum2 == NULL_APP_CALL_NUM))
		{
			errors++;
		 	telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err,
				"No 2nd party call up.");
		}
		break;
	default:
		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid value for party: %d. Valid values are %s.",
			party, partyValidValues);
		errors++;
      		break;	
	}

	return (errors == 0);
}
