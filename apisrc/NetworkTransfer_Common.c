/*------------------------------------------------------------------------------
File:		NetworkTransfer_Common.c
Purpose:	Network Transfer: Common routines only used for network transfer
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
#include <pthread.h>
#include "TEL_Common.h"

struct SendMsgToApp_linkedList
{
	struct Msg_SendMsgToApp		msgInfo;
	struct SendMsgToAppInfo		*next;
};

static struct SendMsgToApp_linkedList     *firstSendMsgToApp = (struct SendMsgToApp_linkedList *) NULL;
static struct SendMsgToApp_linkedList     *lastSendMsgToApp  = (struct SendMsgToApp_linkedList *) NULL; 

static void printSendMsgToApp();

#if 0
/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void initSendMsgToApp()
{
	firstSendMsgToApp = (struct SendMsgToApp_linkedList *) NULL;
	lastSendMsgToApp  = (struct SendMsgToApp_linkedList *) NULL; 

} // END: initSendMsgToApp()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void termSendMsgToApp()
{
	firstSendMsgToApp = (struct SendMsgToApp_linkedList *) NULL;
	lastSendMsgToApp  = (struct SendMsgToApp_linkedList *) NULL; 

} // END: termSendMsgToApp()
#endif

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int addSendMsgToApp(struct Msg_SendMsgToApp *zSendMsgToApp)
{
	static const char			mod[]="addSendMsgToApp";
	struct SendMsgToApp_linkedList		*tmpSendMsgToApp;

    if ((tmpSendMsgToApp = (struct SendMsgToApp_linkedList *)
                        calloc(1, sizeof(struct SendMsgToApp_linkedList))) == NULL)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_MEMORY_ERROR, GV_Err,
			"[%s, %d] Unable to allocate memory, failed to add app message to queue. "
			"[%d, %s]", __FILE__, __LINE__, errno, strerror(errno));
		return(-1);
	}

	memcpy((struct Msg_SendMsgToApp  *) &tmpSendMsgToApp->msgInfo, zSendMsgToApp,
                               sizeof(struct Msg_SendMsgToApp));
	tmpSendMsgToApp->next = (struct SendMsgToApp_linkedList *) NULL;

	if ( firstSendMsgToApp == NULL )
	{
		firstSendMsgToApp	= tmpSendMsgToApp;
		lastSendMsgToApp	= tmpSendMsgToApp;
	}
	else
	{
		lastSendMsgToApp->next	= tmpSendMsgToApp;
		lastSendMsgToApp		= tmpSendMsgToApp;
	}

	printSendMsgToApp();
	return(0);
} // END: addSendMsgToApp()

/*---------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int getNextSendMsgToApp(struct Msg_SendMsgToApp *zSendMsgToApp)
{
	static const char					mod[]="getNextSendMsgToApp";
	struct SendMsgToApp_linkedList		*tmpSendMsgToAppLL;

	//  zSendMsgToApp = (struct SendMsgToAppInfo *)NULL;	
	if ( firstSendMsgToApp == (struct SendMsgToAppInfo *) NULL )
	{
		telVarLog(mod, REPORT_DETAIL, TEL_DATA_NOT_FOUND, GV_Warn,
			"[%s, %d] No message to app found.", __FILE__, __LINE__);
		return (-1);
	}

#if 0
struct SendMsgToApp_linkedList
{
	struct Msg_SendMsgToApp		msgInfo;
	struct SendMsgToAppInfo		*next;
};
#endif
	
    tmpSendMsgToAppLL = firstSendMsgToApp;
    if ( tmpSendMsgToAppLL != NULL )
    {
		memcpy((struct Msg_SendMsgToApp  *) zSendMsgToApp,
		        (struct Msg_SendMsgToApp *) &(tmpSendMsgToAppLL->msgInfo),
		       sizeof(struct Msg_SendMsgToApp));

        firstSendMsgToApp = tmpSendMsgToAppLL->next;
        free(tmpSendMsgToAppLL);
    }

	return(0);

} // END: getNextSendMsgToApp()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static void printSendMsgToApp()
{
	static const char				mod[]="printSendMsgToApp";
	struct SendMsgToApp_linkedList	*tmpSendMsgToAppLL;
	struct Msg_SendMsgToApp			*tmpSendMsgToApp;

	if ( firstSendMsgToApp == NULL )
	{
		return;
	}

    tmpSendMsgToAppLL = firstSendMsgToApp;

	while ( tmpSendMsgToAppLL )
	{
		tmpSendMsgToApp = &tmpSendMsgToAppLL->msgInfo;
		telVarLog(mod, REPORT_VERBOSE, TEL_BASE, WARN,
				"[%s, %d] msgInfo=[appCallNum=%d, appPid=%d, opCode=%d, rc=%d, data=(%s)]",
				__FILE__, __LINE__,
				tmpSendMsgToApp->sendingAppCallNum,
				tmpSendMsgToApp->sendingAppPid,
				tmpSendMsgToApp->opcodeCommand,
				tmpSendMsgToApp->rc,
				tmpSendMsgToApp->data);

		tmpSendMsgToAppLL = tmpSendMsgToAppLL->next;
	}
	
} // END: printSendMsgToApp()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int readNTResponse(char *mod, int timeout, void *pMsg, int zMsgLength)
{
	int rc;
	struct MsgToApp *response;
	struct pollfd pollset[1];
	
	memset (pMsg, 0, sizeof (struct MsgToApp));

	if ( GV_NT_ResponseFifoFd == -1 )
	{
		sprintf(GV_NT_ResponseFifo, "%s/NT_ResponseFifo.%d", GV_FifoDir, GV_AppPid);
		if ((mkfifo (GV_NT_ResponseFifo, S_IFIFO | 0666) < 0) && (errno != EEXIST))
		{
			telVarLog (mod, REPORT_NORMAL, TEL_CANT_CREATE_FIFO, GV_Err,
				TEL_CANT_CREATE_FIFO_MSG, GV_NT_ResponseFifo,
				errno, strerror (errno));
			return (-1);
		}
		if ((GV_NT_ResponseFifoFd = open (GV_NT_ResponseFifo, O_RDWR)) < 0)
		{
			telVarLog (mod, REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
				TEL_CANT_OPEN_FIFO_MSG, GV_NT_ResponseFifo,
				errno, strerror (errno));
			return (-1);
		}
		telVarLog(mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%s, %d] Successfully opened NT fifo (%s), fd=%d", 
			__FILE__, __LINE__, GV_NT_ResponseFifo, GV_NT_ResponseFifoFd);
	}
	pollset[0].fd = GV_NT_ResponseFifoFd;
	pollset[0].events = POLLIN;

	rc = poll (pollset, 1, 0);
	if (rc < 0)
	{
		telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
			"[%s, %d] Error in poll() for fifo (%s), rc=%d. [%d, %s]",
			__FILE__, __LINE__, GV_NT_ResponseFifo, rc, errno, strerror (errno));
		return (-1);
	}
	if (pollset[0].revents == 0)
	{
		return (-2);
	}
	else if (pollset[0].revents == 1)
	{
		/* There is data to read. */
	}
	else
	{
		/* Unexpected return code from poll */
		telVarLog (mod, REPORT_NORMAL, TEL_BASE, GV_Err,
			"[%s, %d] Unexpected return code on poll: %d. errno=%d. [%s]",
			__FILE__, __LINE__, rc, errno, strerror (errno));
		return (-1);
	}

	if (access (GV_NT_ResponseFifo, F_OK) != 0)
	{
		telVarLog (mod, REPORT_NORMAL, TEL_CANT_READ_FIFO, GV_Err,
			TEL_CANT_READ_FIFO_MSG, GV_NT_ResponseFifo,
			errno, strerror (errno));
		return (-1);
	}
	
	rc = read (GV_NT_ResponseFifoFd, (char *) pMsg, zMsgLength);
	
	response = (struct MsgToApp *) pMsg;
	if (rc == -1)
	{
		telVarLog (mod, REPORT_NORMAL, TEL_CANT_READ_FIFO, GV_Err,
				"[%s, %d] Failed to read from fifo (%s).  rc=%d.  [%s, %d]",
				__FILE__, __LINE__, GV_NT_ResponseFifo,
				errno, strerror (errno));
		return (-1);
	}
	telVarLog (mod, REPORT_DETAIL, TEL_BASE, GV_Info,
		"[%s, %d] Received %d bytes from (%s). "
		"msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,rc:%d,data:%s}",
		__FILE__, __LINE__,  rc,
		GV_NT_ResponseFifo,
		response->opcode,
		response->appCallNum,
		response->appPid,
		response->appRef,
		response->appPassword, response->returnCode, response->message);
	
	return (0);
} // END: readNTResponse()
