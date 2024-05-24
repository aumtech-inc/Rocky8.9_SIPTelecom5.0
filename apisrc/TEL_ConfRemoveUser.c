/*-----------------------------------------------------------------------------
Program:        TEL_ConfRemoveUser.c
Purpose:        Perform ConferenceStart functionality for a Telecom Services app.
Author:         Aumtech, Inc.
Update:		04/27/2012 djb Created the file.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

int TEL_ConfRemoveUser(char *zConfId, int isBeep, char *numOfChannel)
{
	char mod[]="TEL_ConfRemoveUser";
	char apiAndParms[MAX_LENGTH] = "";
	static char apiAndParmsFormat[]="%s(%s)";
	int rc;
	int requestSent;
	char lBeepFileName[256] = "";
	time_t t1, t2;

	struct MsgToApp response;
	struct Msg_Conf_Remove_User msg;
#ifdef NETWORK_TRANSFER
    struct Msg_SendMsgToApp msgToApp;
#endif

	if(strcmp(zConfId, confId) != 0)
	{
		HANDLE_RETURN(TEL_CONFERENCE_INVALIDCONFERENCE);
	}
	
	sprintf(msg.confId, "%s", zConfId);

	sprintf(apiAndParms,apiAndParmsFormat, mod, msg.confId);
	rc = apiInit(mod, TEL_CONFERENCEREMOVEUSER, apiAndParms, 1, 1, 0);
    if ( (rc != 0) && (rc != -3) )
    {
        HANDLE_RETURN(rc);
    }

	msg.opcode = DMOP_CONFERENCE_REMOVE_USER;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPasswd = GV_AppPassword;
	msg.appCallNum = GV_AppCallNum1;
	msg.isBeep = isBeep;

	if(msg.isBeep == YES)
	{
		sprintf(lBeepFileName, "%s/%s", GV_SystemPhraseDir,
				BEEP_FILE_NAME);

		if(GV_BeepFile[0])
		{
			rc = access(GV_BeepFile, F_OK);
			if(rc == 0)
				sprintf(lBeepFileName, "%s", GV_BeepFile);
			else
			{
				telVarLog(mod, REPORT_DETAIL, TEL_BASE, WARN,
							"Failed to access beep file (%s). errno=%d. "
							"Using default beep file.", GV_BeepFile, errno);
			}
		}

		rc = access(lBeepFileName, F_OK);
		if(rc == 0)
		{
			sprintf(msg.beepFile, "%s", lBeepFileName);
		}
		else
		{
			telVarLog(mod, REPORT_DETAIL, TEL_BASE, WARN,
						"Beep phrase (%s) doesn't exist. errno=%d. Setting "
						" beep to NO.", lBeepFileName, errno);
			msg.isBeep = NO;
		}
	}

	requestSent=0;
	time(&t1);

    rc = sendRequestToDynMgr(mod, &msg);
    if (rc == -1) HANDLE_RETURN(-1);
    requestSent=1;

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
			rc = readResponseFromDynMgr(mod,3,&response,
				sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			if(rc == -2)
			{
				//rc = sendRequestToDynMgr(mod, (MsgToDM *)&msg);
				rc = sendRequestToDynMgr(mod, &msg);
				if (rc == -1) HANDLE_RETURN(-1);
				requestSent=1;
				rc = readResponseFromDynMgr(mod,10,
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
			case DMOP_CONFERENCE_REMOVE_USER:
				telVarLog(mod, REPORT_VERBOSE, TEL_BASE, INFO,
					"Got returnCode=%d, message=%s.", 	
					response.returnCode, response.message);
				if(strstr(response.message, "error.conference.invaliduser"))
				{
					HANDLE_RETURN(TEL_CONFERENCE_INVALIDUSER);
					break;
				}
				else if(strstr(response.message, "error.conference.invalidconference"))
				{
					HANDLE_RETURN(TEL_CONFERENCE_INVALIDCONFERENCE);
					break;
				}
				else if(response.returnCode == -1)
				{
					telVarLog(mod, REPORT_NORMAL, TEL_BASE, INFO,
						"Got returnCode=%d, no. of channels=%s returning TEL_CONFERENCE_INVALIDCONFERENCE.", 
						response.returnCode, response.message);
					
					HANDLE_RETURN(TEL_CONFERENCE_INVALIDCONFERENCE);
					break;
				}

				isConferenceOn = NO;
				memset(confId, 0, sizeof(confId));
				sprintf(numOfChannel, "%s", response.message);
				HANDLE_RETURN(response.returnCode);
				break;
			case DMOP_GETDTMF:
				saveTypeAheadData(mod,
					response.appCallNum,response.message);
				break;
			case DMOP_DISCONNECT:
				return(disc(mod, response.appCallNum));
				break;
#ifdef NETWORK_TRANSFER
            case DMOP_SENDMSGTOAPP:
                memcpy (&msgToApp, &response, sizeof(msgToApp));
                rc = addSendMsgToApp(&msgToApp);
                break;
#endif
            case -1:
                telVarLog(mod, REPORT_NORMAL, TEL_BASE, INFO, "Returning Failure.");
                HANDLE_RETURN(TEL_FAILURE);
                break;

			default:
/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		}										  /* switch rc */

		time(&t2);
		if((t2-t1) > 2)
		{
			telVarLog(mod, REPORT_NORMAL, TEL_BASE, INFO,
				"Did not receive a response from the media manager.  Returning TEL_TIMEOUT.");
			HANDLE_RETURN(TEL_TIMEOUT);
		}
	}											  /* while */
	HANDLE_RETURN(0);

}
