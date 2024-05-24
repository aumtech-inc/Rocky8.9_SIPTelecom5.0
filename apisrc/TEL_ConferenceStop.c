/*-----------------------------------------------------------------------------
Program:        TEL_ConferencStop.c
Purpose:        Perform ConferenceStart functionality for a Telecom Services app.
Author:         Aumtech, Inc.
Update:		04/27/2012 djb Created the file.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

int TEL_ConferenceStop(char *zConfId)
{
	char mod[]="TEL_ConferenceStop";
	char apiAndParms[MAX_LENGTH] = "";
	static char apiAndParmsFormat[]="%s(%s)";
	int rc;
	int requestSent;

	struct MsgToApp response;
	struct Msg_Conf_Stop msg;
	
	//sprintf(msg.confId, "%s", arcCDR.GV_CDR_Key);
	sprintf(msg.confId, "%s", zConfId);

	sprintf(apiAndParms,apiAndParmsFormat, mod, msg.confId);
	rc = apiInit(mod, TEL_CONFERENCESTOP, apiAndParms, 1, 0, 0);
	if (rc != 0) HANDLE_RETURN(rc);

	msg.opcode = DMOP_CONFERENCE_STOP;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;
	msg.appCallNum = GV_AppCallNum1;

	requestSent=0;
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
		if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response,
				sizeof(response));
            telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
                    "[%s, %d] %d = readResponseFromDynMgr()", __FILE__, __LINE__, rc);
			if(rc == -1)
			{
				HANDLE_RETURN(TEL_FAILURE);
			}
			if(rc == -2)
			{
				//rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&msg);
				rc = sendRequestToDynMgr(mod, &msg);
				if(rc == -1)
				{
					HANDLE_RETURN(TEL_FAILURE);
				}
			
				//RGD::If call is disconnected don't wait for response
//				if (!GV_Connected1)
//				{
//					HANDLE_RETURN(0);
//				}
				requestSent=1;
				rc = readResponseFromDynMgr(mod,10,
					&response,sizeof(response));
                telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
                    "[%s, %d] %d = readResponseFromDynMgr()", __FILE__, __LINE__, rc);
				if(rc == -1)
				{
					HANDLE_RETURN(TEL_FAILURE);
				}
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,10,
				&response,sizeof(response));
			if(rc == -1)
			{
				HANDLE_RETURN(TEL_FAILURE);
			}
			if (rc == TEL_TIMEOUT)
            {
                telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
                        "Timeout waiting for response from Dynamic Manager.");
                HANDLE_RETURN(TEL_TIMEOUT);
            }
		}

		//rc = examineDynMgrResponse(mod, (MsgToDM *)&msg, &response);
		rc = examineDynMgrResponse(mod, &msg, &response);
		switch(rc)
		{
			case DMOP_CONFERENCE_STOP:
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Got returnCode=%d.", response.returnCode);
				if(strstr(response.message, "error.conference.invalidconference"))
				{
					HANDLE_RETURN(TEL_CONFERENCE_INVALIDCONFERENCE);
					break;
				}
				memset(confId, 0, sizeof(confId));
				HANDLE_RETURN(response.returnCode);
				break;
			case DMOP_GETDTMF:
				saveTypeAheadData(mod,
					response.appCallNum,response.message);
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
				
	memset(confId, 0, sizeof(confId));
	HANDLE_RETURN(0);

}
