/*-----------------------------------------------------------------------------
Program:	TEL_SRUnloadGrammar.c 
Author:		Aumtech
Update:	djb 08/10/2005 Created the file.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

int TEL_SRUnloadGrammar(char *zGrammarName)
{
	static char mod[]="TEL_SRUnloadGrammar";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;
	char m[MAX_LENGTH];

	struct MsgToApp response;
	struct Msg_SRUnloadGrammar lMsg;
	struct MsgToDM msg; 
       
	if ( GV_SRType == GOOGLE_SR )
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Google SR is enabled. Returning with no action.");
		return(0);
	}

	sprintf(apiAndParms,"%s(zGrammarData)", mod, zGrammarName);
	rc = apiInit(mod, TEL_SRUNLOADGRAMMAR, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	if(zGrammarName == (char*)0)
	{
		sprintf(m, "%s", "TEL_SRUnloadGrammar failed.  "
						"Grammar name must not be empty.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
		HANDLE_RETURN(-1);
	}


	lMsg.opcode = DMOP_SRUNLOADGRAMMAR;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;
	sprintf(lMsg.grammarName, "%s", zGrammarName);
	
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRUnloadGrammar));
	rc = sendRequestToSRClient(mod, &msg); 
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
		case DMOP_SRUNLOADGRAMMAR:
			HANDLE_RETURN(response.returnCode);
			break;
		case DMOP_GETDTMF:
			/* Ingore all DTMF characters. */
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
