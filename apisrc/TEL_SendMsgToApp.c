/*------------------------------------------------------------------------------
File:		TEL_SendMsgToApp.c
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

static char	ModuleName[] = "TEL_SendMsgToApp";

static int parametersAreGood(char *mod, int zReceivingAppPid, int zReceivingAppPort, char *zData);

/*-----------------------------------------------------------------------------
TEL_SendMsgToApp(): Schedule an outbound call at a specific time.
-----------------------------------------------------------------------------*/
int TEL_SendMsgToApp(int zReceivingAppPid, int zReceivingAppPort, int zOpcode, int zRc, char *zData)
{
	static char		mod[]="TEL_SendMsgToApp";
	struct Msg_SendMsgToApp	response;
	struct MsgToApp			msg;

	char				apiAndParms[MAX_LENGTH] = "";
	char				reason[256];
	int					rc;
	char				responseFifo[256];
	int					fd;

	memset(reason,					0, sizeof(reason));

	sprintf(apiAndParms, "%s(%d,%d,%s)",
			ModuleName, zReceivingAppPid, zReceivingAppPort, zData);
	rc = apiInit(mod, TEL_SENDMSGTOAPP, apiAndParms, 1, 0, 0);
   	if (rc != 0)
	{
		HANDLE_RETURN(rc);
	}

	if ( (rc = parametersAreGood(mod, zReceivingAppPid, zReceivingAppPort, zData)) > 0 ) 
	{
		HANDLE_RETURN(-1);
	}

	memset((struct MsgToApp *)&response,	0, sizeof(response));
	response.opcode = DMOP_SENDMSGTOAPP;
	response.appCallNum = zReceivingAppPort;
	response.appPid = zReceivingAppPid;
	response.appRef = 0;
	response.appPassword = 0;

	response.sendingAppCallNum	= GV_AppCallNum1;
	response.sendingAppPid		= getpid();
	response.opcodeCommand		= zOpcode;
	response.rc					= zRc;
	sprintf(response.data, "%s", zData);

	if ( kill(zReceivingAppPid, 0) == -1 ) // does app exist?
	{
		if ( errno == ESRCH )
		{
			telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Info,
                "[%s, %d] Application pid %d doesn't exist, can't send msg={op:%d,c#:%d,pid:%d,:msg:(%s)",
                __FILE__, __LINE__, response.appPid, response.opcode, response.appCallNum,
                response.appPid, response.data);
			HANDLE_RETURN(-1);
		}
	}

	//sprintf(responseFifo, "%s/ResponseFifo.%d", GV_FifoDir, zReceivingAppPid);
	sprintf(responseFifo, "%s/NT_ResponseFifo.%d", GV_FifoDir, zReceivingAppPid);
	if ( access(responseFifo, F_OK) != 0 )
	{
		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
                "[%s, %d] Application response fifo (%s) doesn't exist, can't send msg={op:%d,c#:%d,pid:%d,:msg:(%s)",
				__FILE__, __LINE__, responseFifo, response.opcode, response.appCallNum,
                response.appPid, response.data);
		HANDLE_RETURN(-1);
	}

	if ((fd = open (responseFifo, O_RDWR | O_NONBLOCK)) < 0)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
                "[%s, %d] Unable to open fifo (%s), can't send msg={op:%d,c#:%d,pid:%d,cmd:%d,rc=%d}",
				__FILE__, __LINE__, responseFifo, response.opcode, response.appCallNum,
                response.appPid, response.opcodeCommand, response.rc);
		HANDLE_RETURN(-1);
	}

	memcpy(&msg, &response, sizeof(msg));


	rc = write (fd, (struct MsgToApp *) &msg, sizeof (msg));
	if (rc == -1)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
                "[%s, %d] Failed to write to fifo (%s), can't send msg={op:%d,c#:%d,pid:%d,cmd:%d,rc=%d}",
				__FILE__, __LINE__, responseFifo, msg.opcode, msg.appCallNum,
                msg.appPid, response.opcodeCommand, response.rc);
		return (-1);
	}

	telVarLog(mod, REPORT_VERBOSE, RESP_BASE, GV_Info,
		"[%s, %d]Sent %d bytes to (%s).  msg={op:%d,c#:%d,pid:%d,rc:%d:msg:(op=%d|port=%d|pid=%d|rc=%d)",
		__FILE__, __LINE__, rc, responseFifo, msg.opcode, msg.appCallNum,
		msg.appPid, response.rc, 
		response.opcodeCommand, response.sendingAppCallNum, response.sendingAppPid,
		response.rc);


	close(fd);

	HANDLE_RETURN(rc);

} /* END: TEL_SendMsgToApp() */

static int parametersAreGood(char *mod, int zReceivingAppPid, int zReceivingAppPort, char *zData)
{
	int		errors=0;
	char	*p;
      
	if (zReceivingAppPid <= 0)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"[%s, %d] Invalid value for app pid (%d): Must be a value pid > 0.", __FILE__, __LINE__, zReceivingAppPid);
	}

	if (zReceivingAppPort < 0)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"[%s, %d] Invalid value for app port (%d): Must be a valid telecom port (>= 0).", __FILE__, __LINE__, zReceivingAppPort);
	}

#if 0
	if (zData[0] == '\0')
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"[%s, %d] Empty value for data.", __FILE__, __LINE );
	}

    if ( (p=strstr(zData, "opCode")) == (char *)NULL)
    {
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        	"[%s, %d] Invalid data received(%s); \"opCode\" parameter not found.  "
			"Must be in format of "
            "opCode=<opcode>;rc=<rc>;data=<optional data>", __FILE__, __LINE__, zData);
    }

    if ( (p=strstr(zData, "rc")) == (char *)NULL)
    {
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        	"[%s, %d] Invalid data received(%s); \"rc\" parameter not found.  "
			"Must be in format of "
            "opCode=<opcode>;rc=<rc>;data=<optional data>", __FILE__, __LINE__, zData);
    }

    if ( (p=strstr(zData, "data")) == (char *)NULL)
    {
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
        	"[%s, %d] Invalid data received(%s); \"data\" parameter not found.  "
			"Must be in format of "
            "opCode=<opcode>;rc=<rc>;data=<optional data>", __FILE__, __LINE__, zData);
    }
#endif

	return(errors);   
}
