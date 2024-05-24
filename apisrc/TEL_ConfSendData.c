/*-----------------------------------------------------------------------------
Program:        TEL_ConferencStop.c
Purpose:        Perform ConferenceStart functionality for a Telecom Services app.
Author:         Aumtech, Inc.
Update:		04/27/2012 djb Created the file.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

int TEL_ConfSendData(char *zConfId, char *zConfData)
{
	char mod[]="TEL_ConfSendData";
	char apiAndParms[MAX_LENGTH] = "";
	static char apiAndParmsFormat[]="%s(%s)";
	int rc;
	int requestSent;

	struct MsgToApp response;
	struct Msg_Conf_Send_Data msg;
	
	sprintf(msg.confId, "%s", zConfId);
	sprintf(msg.confOpData, "%s", zConfData);

	sprintf(apiAndParms,apiAndParmsFormat, mod, msg.confId);
	rc = apiInit(mod, TEL_CONFERENCESENDDATA, apiAndParms, 1, 1, 0);
	if (rc != 0) HANDLE_RETURN(rc);

	msg.opcode = DMOP_CONFERENCE_SEND_DATA;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPasswd = GV_AppPassword;
	msg.appCallNum = GV_AppCallNum1;
	sprintf(GV_ConfData, "%s", zConfData);

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
				//rc = sendRequestToDynMgr(mod, (MsgToDM *)&msg);
				rc = sendRequestToDynMgr(mod, &msg);
				if (rc == -1) HANDLE_RETURN(-1);
				requestSent=1;
				rc = readResponseFromDynMgr(mod,0,
					&response,sizeof(response));
				if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,10,
				&response,sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		}

		rc = examineDynMgrResponse(mod, &msg, &response);
		switch(rc)
		{
			case DMOP_CONFERENCE_SEND_DATA:
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Got returnCode=%d.", response.returnCode);
				HANDLE_RETURN(response.returnCode);
				break;
			case DMOP_CONFERENCE_REMOVE_USER:
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Got returnCode=%d.", response.returnCode);
				isConferenceOn = NO;
				HANDLE_RETURN(response.returnCode);
				break;
			case DMOP_CONFERENCE_STOP:
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Got returnCode=%d.", response.returnCode);
				isConferenceOn = NO;
				HANDLE_RETURN(response.returnCode);
				break;
			case DMOP_CONFERENCE_ADD_USER:
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Got returnCode=%d.", response.returnCode);
				isConferenceOn = YES;
				HANDLE_RETURN(response.returnCode);
				break;
			case DMOP_GETDTMF:
				saveTypeAheadData(mod,
					response.appCallNum,response.message);
				break;
			case DMOP_DISCONNECT:
				return(disc(mod, response.appCallNum));
				break;
			default:
/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		}										  /* switch rc */
	}											  /* while */
	HANDLE_RETURN(0);

}
