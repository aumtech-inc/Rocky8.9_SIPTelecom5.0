/*-----------------------------------------------------------------------------
Program:        TEL_ConfAddUser.c
Purpose:        Perform ConferenceStart functionality for a Telecom Services app.
Author:         Aumtech, Inc.
Update:		04/27/2012 djb Created the file.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

#define INACTIVE    100
#define SENDONLY    101
#define RECVONLY    102
#define SENDRECV    103


int TEL_ConfAddUser(char *zConfId, int isBeep, char *duplex, char *numOfChannel)
{
	char mod[]="TEL_ConfAddUser";
	char apiAndParms[MAX_LENGTH] = "";
	static char apiAndParmsFormat[]="%s(%s)";
	int rc;
	int requestSent;
	char lBeepFileName[256] = "";

	struct MsgToApp response;
	struct Msg_Conf_Add_User msg;
#ifdef NETWORK_TRANSFER
    struct Msg_SendMsgToApp msgToApp;
#endif
	
	sprintf(msg.confId, "%s", zConfId);
	sprintf(confId, "%s", zConfId);

	sprintf(apiAndParms,apiAndParmsFormat, mod, msg.confId);
	rc = apiInit(mod, TEL_CONFERENCEADDUSER, apiAndParms, 1, 1, 0);
	if (rc != 0) HANDLE_RETURN(rc);

	msg.opcode = DMOP_CONFERENCE_ADD_USER;
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
				telVarLog(mod,REPORT_DETAIL, TEL_BASE, GV_Warn,
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
				telVarLog(mod,REPORT_DETAIL, TEL_BASE, GV_Warn,
						"Beep phrase (%s) doesn't exist. errno=%d. Setting "
						" beep to NO.", lBeepFileName, errno);
			msg.isBeep = NO;
			memset(msg.beepFile, 0, sizeof(msg.beepFile));
		}
	}
	else
	{
		memset(msg.beepFile, 0, sizeof(msg.beepFile));
	}


	if(strcmp(duplex, "full") == 0)
	{
		msg.duplex = SENDRECV;
	}
	else if(strcmp(duplex, "half") == 0)
	{
		msg.duplex = SENDONLY;
	}
	else
	{
		msg.duplex = SENDRECV;
	}
				
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"msg.isBeep=%d, msg.beepFile=%s",msg.isBeep, msg.beepFile);

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
			rc = readResponseFromDynMgr(mod, 3, &response,
				sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			if(rc == -2)
			{
				// rc = sendRequestToDynMgr(mod, (MsgToDM *)&msg);
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
			case DMOP_CONFERENCE_ADD_USER:
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Got returnCode=%d, no. of channels=%s.", 
					response.returnCode, response.message);

				if(strstr(response.message, "error.conference.duplicateuser"))
				{
					HANDLE_RETURN(TEL_CONFERENCE_DUPLICATEUSER);
					break;
				}
				else if(strstr(response.message, "error.conference.invalidconference"))
				{
					HANDLE_RETURN(TEL_CONFERENCE_INVALIDCONFERENCE);
					break;
				}
				else if(response.returnCode == -1)
				{
					HANDLE_RETURN(TEL_CONFERENCE_INVALIDCONFERENCE);
					break;
				}

				isConferenceOn = YES;
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
                telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
                        "[%s, %d] Received DMOP_SENDMSGTOAPP.",
                __FILE__, __LINE__);
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
	HANDLE_RETURN(0);

}
