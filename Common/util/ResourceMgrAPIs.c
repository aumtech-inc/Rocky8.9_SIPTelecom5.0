/*------------------------------------------------------------------------------
File:		ResourceMgrAPIs.c
Author:		Aumtech, Inc.

Purpose:	Object file which is to be linked into our (Aumtech)
		internal APIs (it will most likely go into a library,
		not determined yet).  The customer applications will
		not call the functions defined in this object, which
		are: Request_Resource(), Free_Resource(), and
		Status_Resources().  These functions are described
		in detail below

Update:	10/28/98 djb	Created.
Update:	11/20/98 djb	Added NO_LOGX_MSG log so monitor would not LOGXXPRT()
Update:	06/08/98 mpb	Added port and appName to Free_Resource()
Update:	08/24/99 gpw	added log_msg parameter to APIs; removed logging
Update:	08/24/99 gpw	added messages when no resources config'd or available.
Update:	05/01/00 apj	added setSlotNumberInResourceMgr.
Update:	10/17/02 apj	Changed getpid() with GV_AppPid.
------------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include "ResourceMgr.h"
#include "ispinc.h" 

extern int GV_AppPid;

/*-------------------  Function Prototypes  ----------------------------------*/
static int getMsgOffQueue(int queId, ResourceMsg *msg, long pid, char *logMsg);
static void	utilSleep(int Seconds, int Milliseconds);
static int	sendMsgToQueue(int queId, long pid, char *message, 
							char *logMsg);

/*------------------------------------------------------------------------------
	Accepts the resource type and subType, port number, and application
	name, then makes request to the resource manager for a resource.
	Success: 	0
	Failure:       -1
------------------------------------------------------------------------------*/
int Request_Resource(char *type, char *subType, int port, char *appName,
			char *resourceName, char *slotNumber, char *logMsg)
{
	static char	mod[]="Request_Resource";
	pid_t		pid;
	int		queId;
	int		rc;
	char		buf[256];
	char		charRet1[32];
	char		charRet2[32];
	ResourceMsg	msg;

	//pid = getpid();
	pid = GV_AppPid;
	if((queId = msgget(RESM_MSG_QUE, RESM_PERMS)) < 0)
	{
		sprintf(logMsg, "Unable to make resource request.  "
				"%d=msgget failed. errno=%d. "
				"Check if ResourceMgr is running.", 
				queId, errno);
		return(RESM_UNABLE_TO_SEND);
	}
	
	sprintf(buf,"%s %s %d %d %d %s", type, subType, RESM_REQ_ALLOCATE, 
					pid, port, appName);

	if((rc=sendMsgToQueue(queId, pid, buf, logMsg)) != 0)
	{
		return(rc);
	}

	memset((ResourceMsg *)&msg, 0, sizeof(msg));
	if((rc=getMsgOffQueue(queId, &msg, pid, logMsg)) != 0)
	{
		return(rc);
	}

	/* This code is determining based on the content of the data message 
	   successfully received, whether or not some error occurred other
    	   than the failure to transmit/receive the message itself. gpw*/ 
	if (msg.data[0] == '-')
	{
		sprintf(charRet1, "%d", RESM_RESOURCE_NOT_AVAIL);
		sprintf(charRet2, "%d", RESM_NO_RESOURCES_CONFIG);
		if (strcmp(msg.data, charRet1) == 0)
		{
			rc=(int)atoi(charRet1);
			sprintf(logMsg,
	"No resource of type <%s>, subtype <%s> available. Internal rc=%d.",
			type, subType, rc);
			return(rc);
		}
		if (strcmp(msg.data, charRet2) == 0)
		{
			rc=(int)atoi(charRet2);
			sprintf(logMsg,
	"No resource of type <%s>, subtype <%s> configured. Internal rc=%d.",
			type, subType, rc);
			return(rc);
		}
	}
		
	sscanf(msg.data, "%s %s", resourceName, slotNumber);
	return(0);
} /* END: Request_Resource() */
	
/*------------------------------------------------------------------------------
Free a resource based on the resource type, subType, and resourceName).
------------------------------------------------------------------------------*/
int Free_Resource(char *type, char *subType, int port, 
			char *appName, char *resourceName, char *logMsg)
{
	static char	mod[]="Free_Resource";
	int		queId;
	pid_t		pid;
	int		rc;
	char		buf[256];

	//pid = getpid();
	pid = GV_AppPid;
	if((queId = msgget(RESM_MSG_QUE, RESM_PERMS)) < 0) 
	{
		sprintf(logMsg, "Unable to make resource request.  "
				"%d=msgget failed. errno=%d. "
				"Check if ResourceMgr is running.", 
				queId, errno);
		return(RESM_UNABLE_TO_SEND);
	}

	sprintf(buf,"%s %s %d %d %d %s %s", type, subType, RESM_REQ_FREE, pid, 
						port, appName, resourceName);

	rc=sendMsgToQueue(queId, pid, buf, logMsg);

	return(rc);
} /* END: Free_Resource() */

/*------------------------------------------------------------------------------
Request the resource manager a status of all configured resources,		which will be listed in the returned fileName.
------------------------------------------------------------------------------*/
int Status_Resources(char *fileName, char *logMsg)
{
	static char	mod[]="Status_Resources";
	pid_t		pid;
	int		queId;
	int		rc;
	char		buf[256];
	char		charRet1[32];
	char		charRet2[32];
	ResourceMsg	msg;
	
	//pid = getpid();
	pid = GV_AppPid;
	if((queId = msgget(RESM_MSG_QUE, RESM_PERMS)) < 0)
	{
		sprintf(logMsg, "Unable to make resource request.  "
				"%d=msgget failed. errno=%d. "
				"Check if ResourceMgr is running", 
				queId, errno);
		return(RESM_UNABLE_TO_SEND);
	}
	
	sprintf(buf,"%s %s %d %d", "0", "0", RESM_REQ_STATUS, pid);

	if((rc=sendMsgToQueue(queId, pid, buf, logMsg)) != 0)
	{
		return(rc);
	}
	
	memset((ResourceMsg *)&msg, 0, sizeof(msg));
	if((rc=getMsgOffQueue(queId, &msg, pid, logMsg)) != 0)
	{
		return(rc);
	}

	if (msg.data[0] == '-')
	{
		sprintf(charRet1, "%d", RESM_GENERAL_FAILURE);
		sprintf(charRet2, "%d", RESM_NO_RESOURCES_CONFIG);
		if (strcmp(msg.data, charRet1) == 0)
		{
			rc=(int)atoi(charRet1);
			return(rc);
		}
		if (strcmp(msg.data, charRet2) == 0)
		{
			rc=(int)atoi(charRet2);
			return(rc);
		}
	}
		
	sprintf(fileName, "%s", msg.data);
	return(0);
} /* END: Status_Resources() */

/*------------------------------------------------------------------------------
	Send a message to the message queue, based on the pid.

	If msgsnd() fails with a return code of -1 and errno == EAGAIN,
	a sleep(0.3) will be performed before trying again.  If after
	10 retries a successful attempt has not been made,
	a message is logged, and a 0 is returned.  This means the message
	is discarded.

	If msgsnd() fails with a return code of -1 and errno != EAGAIN,
	there is probably a serious problem: a message is logged, and
	a -1 is returned.
------------------------------------------------------------------------------*/
int 	sendMsgToQueue(int queId, long pid, char *message, char *logMsg) 
{
	static	char	mod[] = "sendMsgToQueue";
	ResourceMsg	msg;
	int		rc;
	int		i;
	char		buf[128];

	sprintf(msg.data, "%s", message);
	msg.type = RESM_MSG_TYPE;
	for (i=0; i<10; i++)
	{
		if((rc=msgsnd(queId, &msg, RESM_MSG_DATASIZE, IPC_NOWAIT))
									== 0) 
		{
			return(0);
		}
		utilSleep(0, 300);
	}

	sprintf(logMsg, "Failed to send message <%s> to Resource Manager. "
			"pid=%d. errno=%d.", message, pid, errno);
	return(RESM_UNABLE_TO_SEND);

} /* END: sendMsgToQueue() */

/*------------------------------------------------------------------------------
	Retrieve the message off the message queue.
------------------------------------------------------------------------------*/
int getMsgOffQueue(int queId, ResourceMsg *msg, long pid, char *logMsg)
{
	static char	mod[]="getMsgOffQueue";
	int		rc;
	int		i;
	char		buf[128];

	msg->type=pid;
	for (i=0; i<10; i++)
	{
		if((rc=msgrcv(queId, msg, RESM_MSG_DATASIZE, (long)pid,
							IPC_NOWAIT)) < 0) 
		{
			utilSleep(0, 300);
			continue;
		}
		return(0);
	}

	sprintf(logMsg, "Failed to receive message from Resource Manager. "
		     "errno=%d.", errno);
	return(RESM_NO_RESPONSE);
} /* END: getMsgOffQueue() */

/*------------------------------------------------------------------------------
utilSleep():
------------------------------------------------------------------------------*/
void utilSleep(int Seconds, int Milliseconds)
{
	static struct timeval SleepTime;

	SleepTime.tv_sec = Seconds;
	SleepTime.tv_usec = Milliseconds * 1000;

	select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &SleepTime);

} /* END: utilSleep() */

/* This routine sets the time slot value in the internal structure of 
   <resourceName> resource to <slotNumber> */
int setSlotNumberInResourceMgr(char *type, char *subType, int port, 
			char *appName, char *resourceName, 
			char *slotNumber, char *logMsg)
{
	static char	mod[]="setSlotNumberInResourceMgr";
	int		queId;
	int		rc;
	char		buf[256];
	pid_t		pid;

	//pid = getpid();
	pid = GV_AppPid;
	if((queId = msgget(RESM_MSG_QUE, RESM_PERMS)) < 0)
	{
		sprintf(logMsg, "Unable to make resource request.  "
				"%d=msgget failed. errno=%d. "
				"Check if ResourceMgr is running.", 
				queId, errno);
		return(RESM_UNABLE_TO_SEND);
	}
	
	sprintf(buf,"%s %s %d %d %d %s %s %s", type, subType, 
	RESM_SET_SLOT_NUMBER, pid, port, appName, resourceName, slotNumber);

	if((rc=sendMsgToQueue(queId, pid, buf, logMsg)) != 0)
	{
		/* logMsg contains the error message text. */ 
		return(rc);
	}

	return(0);
}

