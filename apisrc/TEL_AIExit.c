/*-----------------------------------------------------------------------------
Program:        TEL_AIExit.c 
Author:         Aumtech, Inc.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

int TEL_AIExit(void)
{
	static char mod[]="TEL_AIExit";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;

	struct MsgToApp response;
	struct Msg_AIExit lMsg;
	struct MsgToDM msg; 
       
	sprintf(apiAndParms,"%s()", mod);
	rc = apiInit(mod, TEL_AIEXIT, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	lMsg.opcode = DMOP_AIEXIT;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;
	
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_AIExit));

	rc = sendRequestToAIClient(mod, &msg);

	if (rc == -1) HANDLE_RETURN(-1);

	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,60,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Timeout waiting for AI response.");
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);

		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_AIEXIT:
			HANDLE_RETURN(response.returnCode);
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
