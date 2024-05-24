/*-----------------------------------------------------------------------------
Program:        TEL_SRGetXMLResult.c 
Purpose:        To get the speech recognition results.
Author:         Anand Joglekar
Date:			10/01/2002
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

int TEL_SRGetXMLResult(char *zXmlResultFile)
{
	static char mod[]="TEL_SRGetXMLResult";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;

	struct MsgToApp response;
	struct Msg_SRGetXMLResult lMsg;
	struct MsgToDM msg; 

	sprintf(apiAndParms,"%s(%s)", mod, zXmlResultFile);

	if ( GV_SRType == GOOGLE_SR )
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Google SR is enabled. Returning with no action.");
		return(0);
	}

	rc = apiInit(mod, TEL_SRGETXMLRESULT, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	lMsg.opcode = DMOP_SRGETXMLRESULT;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;

	if(*zXmlResultFile == '\0')
	{
		sprintf(zXmlResultFile, "xml_%d_%d_%d.xml", 
					GV_AppCallNum1, GV_AppPid, GV_AppRef);
	}

	sprintf(lMsg.fileName, "%s", zXmlResultFile);

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRGetXMLResult));

#ifdef MRCP_SR
	rc = sendRequestToSRClient(mod, &msg);
#else
	rc = sendRequestToDynMgr(mod, &msg); 
#endif

	if (rc == -1) HANDLE_RETURN(-1);

	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,10,&response,sizeof(response));
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
		case DMOP_SRGETXMLRESULT:
			if(response.returnCode >= 0)
			{
				response.returnCode = 0;
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
}
