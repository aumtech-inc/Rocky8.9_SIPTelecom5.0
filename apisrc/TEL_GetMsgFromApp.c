/*------------------------------------------------------------------------------
File:		TEL_GetMsgFromApp.c
Purpose:	Network Transfer: Send a message go an existing application.
Author:		Aumtech, Inc.
Date:		09/25/2012 djb	Created the file.
-----------------------------------------------------------------------------
Copyright (c) 2012, Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "TEL_Globals.h"
//#include "ocmHeaders.h"
//#include "ocmNTHeaders.h"

static char	ModuleName[] = "TEL_GetMsgFromApp";

static int parametersAreGood(char *mod, int zReceivingAppPid, int zReceivingAppPort, char *zData);

/*-----------------------------------------------------------------------------
TEL_GetMsgFromApp(): Schedule an outbound call at a specific time.
-----------------------------------------------------------------------------*/
int TEL_GetMsgFromApp(int *zRemoteAppPID, int *zRemoteAppPort, int *zOpcode, int *zRc, char *zData)
{
	static const char	mod[]="TEL_GetMsgFromApp";
	struct Msg_SendMsgToApp	response;
	char				apiAndParms[MAX_LENGTH] = "";
	char				reason[256];
	int					rc;
	char				responseFifo[256];
	int					fd;

	memset(reason,					0, sizeof(reason));

	sprintf(apiAndParms, "%s()", ModuleName);

	rc = apiInit(mod, TEL_GETMSGFROMAPP, apiAndParms, 1, 0, 0);
   	if (rc != 0)
	{
		HANDLE_RETURN(rc);
	}

	memset((struct Msg_SendMsgToApp *)&response, '\0', sizeof(response));
	if ((rc = getNextSendMsgToApp(&response)) == -1 )
	{
		HANDLE_RETURN(rc);
	}

	*zRemoteAppPID	= response.sendingAppPid;
	*zRemoteAppPort	= response.sendingAppCallNum;
	*zOpcode		= response.opcodeCommand;
	*zRc			= response.rc;
	sprintf(zData, "%s", response.data);
	telVarLog(mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"[%s, %d] Received appPid(%d), appPort(%d), opcode(%d), rc(%d), data(%s)",
		__FILE__, __LINE__,
		response.sendingAppPid, response.sendingAppCallNum, response.opcodeCommand,
		response.rc, response.data);

	HANDLE_RETURN(0);

} /* END: TEL_GetMsgFromApp() */
