/*-----------------------------------------------------------------------------
Program:        TEL_ConferencStart.c
Purpose:        Perform ConferenceStart functionality for a Telecom Services app.
Author:         Aumtech, Inc.
Update:		04/27/2012 djb Created the file.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"


int TEL_ConferenceStart(char *zConfId)
{
	char mod[]="TEL_ConferenceStart";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;
	struct MsgToApp response;
	struct Msg_Conf_Start lMsg;
	struct MsgToDM msg;
#ifdef NETWORK_TRANSFER
    struct Msg_SendMsgToApp msgToApp;
#endif
	
	sprintf(lMsg.confId, "%s", zConfId);

	sprintf(apiAndParms,"%s(%s)", mod, lMsg.confId);

	rc = apiInit(mod, TEL_CONFERENCESTART, apiAndParms, 1, 0, 0);

	if (rc != 0) HANDLE_RETURN(rc);

	lMsg.opcode = DMOP_CONFERENCE_START;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_Conf_Start));

	rc = sendRequestToDynMgr(mod, &msg);
	if (rc == -1) HANDLE_RETURN(-1);

	while ( 1 )
	{
#ifdef NETWORK_TRANSFER
        rc = readNTResponse(mod, -1, &response, sizeof(response));
        if ( (rc == -1) && (rc == -2) )
        {
            rc = examineDynMgrResponse(mod, &msg, &response);
             if ( rc == DMOP_SENDMSGTOAPP )
            {
                memcpy (&msgToApp, &response, sizeof(msgToApp));
                telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
                        "[%s, %d] Received DMOP_SENDMSGTOAPP [%d, %d].",
                        __FILE__, __LINE__,
                        msgToApp.opcode, msgToApp.opcodeCommand);
                 rc = addSendMsgToApp(&msgToApp);
            }
        }
#endif
		rc = readResponseFromDynMgr(mod,10,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"Timeout waiting for response from Dynamic Manager.");
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);

		rc = examineDynMgrResponse(mod, &msg, &response);
		switch(rc)
		{
			case DMOP_CONFERENCE_START:
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Got returnCode=%d, message=%s.", response.returnCode, response.message);
				
				if(strstr(response.message, "error.conference.duplicateconference"))
				{
					HANDLE_RETURN(TEL_CONFERENCE_DUPLICATECONFERENCE);
					break;
				}

				HANDLE_RETURN(response.returnCode);
				break;
			case DMOP_GETDTMF:
				saveTypeAheadData(mod, response.appCallNum, response.message);
				break;
			case DMOP_DISCONNECT:
				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;
#ifdef NETWORK_TRANSFER
            case DMOP_SENDMSGTOAPP:
                memcpy (&msgToApp, &response, sizeof(msgToApp));
                rc = addSendMsgToApp(&msgToApp);
                break;
#endif
			default:
/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		}										  /* switch rc */
	}											  /* while */
	return 0;
}
