/*-----------------------------------------------------------------------------
Program:        TEL_AIInit.c 
Purpose:        To do Speech Rec initialization.
Author:         Anand Joglekar
Date:			09/27/2002
-----------------------------------------------------------------------------*/

#include <stdlib.h>

#include "TEL_Common.h"

int TEL_AIInit()
{
	static char mod[]="TEL_AIInit";
	int rc;

	char apiAndParms[MAX_LENGTH] = "";
	struct MsgToApp response;
	struct Msg_AIInit lMsg;
	struct MsgToDM msg; 
       
	rc = apiInit(mod, TEL_AIINIT, apiAndParms, 1, 0, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	rc=openChannelToAIClient(mod, "");
	if (rc == -1) HANDLE_RETURN(TEL_FAILURE);

	lMsg.opcode = DMOP_AIINIT;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppCallNum1;
	sprintf(lMsg.appFifo, "%s", GV_ResponseFifo);
	
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_AIInit));

	rc = sendRequestToAIClient(mod, &msg);

	if (rc < 0)
	{
		HANDLE_RETURN(-1);
	}

	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,60,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
					"Timeout waiting for response from AIClient.");
			
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) 
		{
			HANDLE_RETURN(TEL_FAILURE);
		}

		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_AIINIT:
			if(response.returnCode == 0 && response.message[0]) // message is new AIClient thread fifo
			{
				closeChannelToAIClient(mod);
				if( openChannelToAIClient(mod, response.message) != 0)
				{
					HANDLE_RETURN(-1);
				}
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
