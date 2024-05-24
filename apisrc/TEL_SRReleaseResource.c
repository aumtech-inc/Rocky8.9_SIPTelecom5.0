/*-----------------------------------------------------------------------------
Program:        TEL_SRReleaseResource.c 
Purpose:        Release both server side and client side licenses.
Author:         Anand Joglekar
Date:			10/01/2002
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

int TEL_SRReleaseResource(int *zpVendorErrorCode)
{
	static char mod[]="TEL_SRReleaseResource";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;

	struct MsgToApp response;
	struct Msg_SRReleaseResource lMsg;
	struct MsgToDM msg; 
       
	if ( GV_SRType == GOOGLE_SR )
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Google SR is enabled. Returning with no action.");
		return(0);
	}

	*zpVendorErrorCode = 0;
	sprintf(apiAndParms,"%s(&VendorErrorCode)", mod);
	rc = apiInit(mod, TEL_SRRELEASERESOURCE, apiAndParms, 1, 0, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	lMsg.opcode = DMOP_SRRELEASERESOURCE;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;
	
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRReleaseResource));

#ifdef MRCP_SR
	rc = sendRequestToSRClient(mod, &msg);
#else
	rc = sendRequestToDynMgr(mod, &msg); 
#endif

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
		case DMOP_SRRELEASERESOURCE:
			*zpVendorErrorCode = response.vendorErrorCode;
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
