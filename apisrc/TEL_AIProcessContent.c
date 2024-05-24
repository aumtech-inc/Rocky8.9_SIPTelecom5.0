/*-----------------------------------------------------------------------------
Program:        TEL_AIGetIntent.c 
Author:         Aumtech, Inc.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

int gAltRetCode;

#define 	FILESTR	 "#FILE#:"
static int parametersAreGood(char *zMod, int zOpcode, char *zContent);

int TEL_AIProcessContent(int aiOpcode, char *content, char *intent)
{
	static char mod[]="TEL_AIProcessContent";
	int rc;
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s(%d,argv)";
	struct MsgToApp response;
	struct Msg_AIProcessContent lMsg;
	struct MsgToDM msg;
	int requestSent = 0;
	char		yStrFileName[128];
	time_t		tm;
	FILE		*fp;
	int			fd;
	char		tmpContent[2048];

	rc = apiInit(mod, DMOP_AIPROCESS_CONTENT, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	if(strlen(content) >= 10)
	{
		tm = 5839;
		time(&tm);
		/* Write this data to a file, so that dynmgr can read it. */
		sprintf(yStrFileName, "/tmp/aiContent_fromApp.%d.%d", GV_AppCallNum1, tm % 1000);
		if((fp = fopen(yStrFileName, "w")) == NULL)
		{
			telVarLog(mod,REPORT_NORMAL,TEL_BASE,GV_Err,
				"Failed to open file (%s) for output. [%d, %s]",
				yStrFileName, errno, strerror(errno));
			HANDLE_RETURN(-1);
		}

		fprintf(fp, "%s", content);
		fclose(fp);

		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Wrote contect request (%s) to file (%s) for IPC.", content, yStrFileName);
		sprintf(tmpContent, "%s%s", FILESTR, yStrFileName);
	}
	else
	{
		sprintf(tmpContent, "%s", tmpContent);
	}

	if( !parametersAreGood(mod, aiOpcode, tmpContent))
	{
		HANDLE_RETURN(-1);
	}

	gAltRetCode = 0;
	sprintf(apiAndParms,"%s(%d, %s)", mod, aiOpcode, content);

	memset(&msg, 0, sizeof(struct MsgToDM));
	memset((struct Msg_AIProcessContent *)&lMsg, '\0', sizeof(struct Msg_AIProcessContent));

	lMsg.opcode = DMOP_AIPROCESS_CONTENT;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;
	lMsg.aiOpcode	= aiOpcode;
	sprintf(lMsg.content, "%s",	tmpContent);

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_AIProcessContent));

	requestSent = 0;
	while(1)
	{
		if(!requestSent)
		{
			rc = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			if(rc == -2)
			{
				rc = sendRequestToAIClient(mod, &msg);
        		if (rc == -1) HANDLE_RETURN(-1);
				requestSent=1;
				rc = readResponseFromDynMgr(mod,600,
						&response,sizeof(response));
				if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,600,&response,sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		}

		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
					"Timeout waiting for response from AIClient.");
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);

		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
			case DMOP_AIPROCESS_CONTENT:
			case AI_ADD_CONTENT_AND_PROCESS: 
				if ( strstr(response.message, FILESTR) )
				{
					char	fle[64];
					int		locate;
					
					locate = strlen(FILESTR);
					sprintf(tmpContent, "%s", &(response.message[locate]));

					response.message[locate] = '\0';

					sprintf(yStrFileName, "%s", tmpContent);
					if((fd = open(yStrFileName, O_RDONLY)) <= 0 )
					{
						telVarLog(mod,REPORT_NORMAL,TEL_BASE,GV_Err,
							"Failed to open file (%s) for input. [%d, %s]",
							yStrFileName, errno, strerror(errno));
						HANDLE_RETURN(-1);
					}
					memset((char *)tmpContent, '\0', sizeof(tmpContent));
					if ((rc = read(fd, tmpContent, sizeof(tmpContent)-1)) <= 0)
					{
						telVarLog(mod,REPORT_NORMAL,TEL_BASE,GV_Err,
							"Failed to read file (%s) for input. rc=%d [%d, %s]",
							yStrFileName, rc, errno, strerror(errno));
						HANDLE_RETURN(-1);
					}
					close(fd);
					sprintf(intent, "%s", tmpContent);
				}
				else
				{
					sprintf(intent, "%s", response.message);
				}

				HANDLE_RETURN(response.returnCode);
				break;

			case DMOP_GETDTMF:
				if(response.appCallNum != msg.appCallNum)
				{
					/* Save data for the other leg of the call */
					saveTypeAheadData(mod, response.appCallNum,
						response.message);
					break;
				}


			case DMOP_DISCONNECT:
   				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;

			default:
				/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		} /* switch rc */
	} /* while */
} // END: TEL_AIProcessContent

/*-------------------------------------------------------------------------
This function verifies that all parameters are valid.
It returns 1 (yes) if all parmeters are good, 0 (no) if not.
-------------------------------------------------------------------------*/
static int parametersAreGood(char *zMod, int zAIOpcode, char *zContent)
{
	static char mod[]="parametersAreGood";

	char m[MAX_LENGTH];
	int  errors=0;

	switch(zAIOpcode)
	{
		case AI_CLEAR_CONTENT:
		case AI_PROCESS_LOADED:
			break;

		case AI_ADD_CONTENT_AND_PROCESS:
		case AI_ADD_CONTENT:
			if ( zContent[0] == '\0' || ! zContent )
			{
				errors ++;
				sprintf(m, "Empty content parameter. " 
					"For AI_ADD_CONTENT_AND_PROCESS(%d) or AI_ADD_CONTENT(%d), "
					"content must be non-empty. ", AI_ADD_CONTENT_AND_PROCESS, AI_ADD_CONTENT);
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, m);
			}
			break;
		default:
			errors ++;
			sprintf(m, "Invalid AIopcode parameter: %d. "
				"Valid values: AI_ADD_CONTENT(%d) AI_CLEAR_CONTENT(%d) AI_AI_PROCESS_LOADED(%d) AI_ADD_CONTENT_AND_PROCESS(%d)",
				zAIOpcode, AI_ADD_CONTENT, AI_CLEAR_CONTENT, AI_PROCESS_LOADED, AI_ADD_CONTENT_AND_PROCESS);
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, m);
			break;
	}


	return(errors == 0);   
}
