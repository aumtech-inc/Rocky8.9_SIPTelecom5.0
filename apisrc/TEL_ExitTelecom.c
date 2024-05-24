/*-----------------------------------------------------------------------------
Program:        TEL_ExitTelecom.c
Purpose:        Perform exit functionality for a Telecom Services app.
Author:         Wenzel-Joglekar
Date:		09/13/2000
Update:		10/17/2000 gpw updated to use drop call op code
Update:		11/10/2000 gpw only pass allParties if both are connected.
Update:		03/27/2001 apj Code changes because DMOP_DROPCALL will never 
			       show up. only DMOP_DISCONNECT will come.
Update:		03/29/2001 apj Remove pipe before doing HANDLE_RETURN.
Update: 	05/30/2001 APJ Set 20 sec timeout for response from DM.
Update: 	05/31/2001 APJ Call closeChannelFromDynMgr if apiInit failed.
Update: 	10/04/2001 apj Remove speak list files in closeChannelFromDynMgr.
Update: 	07/25/2002 apj Check whether exit is already been called before.
Update: 	10/10/2003 apj Removed tts files.
Update: 	03/30/2004 apj In apiInit, send to monitor on success only.
Update:     12/28/2004 ddn Remove silence_(pid)_(count)_.64p files
Update:     09/10/2013 djb Added network transfer
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int closeChannelFromDynMgr();

int TEL_ExitTelecom() 
{
	static char mod[]="TEL_ExitTelecom";
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s()";
	int rc = 0;
	int requestSent;
	static int sExitAlreadyCalled = 0;

	struct MsgToApp response;
	struct Msg_DropCall lMsg;
	struct MsgToDM msg;

#ifdef NETWORK_TRANSFER
    if ( GV_NT_ResponseFifoFd > -1 )
    {
        close(GV_NT_ResponseFifoFd);
        GV_NT_ResponseFifoFd = -1;
    }
    if ( GV_NT_ResponseFifo[0] != '\0' )
    {
        unlink(GV_NT_ResponseFifo);
        memset((char *)GV_NT_ResponseFifo, '\0', sizeof(GV_NT_ResponseFifo));
    }
#endif

	if(sExitAlreadyCalled == 1)
	{
		telVarLog(mod, REPORT_DETAIL, TEL_BASE, GV_Warn,
			"This function has already been called. It can only be called once.");
		HANDLE_RETURN(-1);
	}
	sExitAlreadyCalled = 1;
		
//	sprintf(apiAndParms,apiAndParmsFormat, mod);
//	rc = apiInit(mod, TEL_EXITTELECOM, apiAndParms, 1, 0, 0);
  	if (rc != 0) 
	{
		closeChannelFromDynMgr();
		HANDLE_RETURN(rc);
	}

	if (!GV_Connected1 && !GV_Connected2)
	{
		closeChannelFromDynMgr();
		HANDLE_RETURN(TEL_DISCONNECTED);
	}

	lMsg.opcode = DMOP_DROPCALL;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;
	lMsg.allParties = (GV_Connected1 && GV_Connected2);

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_DropCall));
       	
	requestSent=0;
	while ( 1 )
	{
		if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response,
						sizeof(response));
			if(rc == -1) 
			{
				closeChannelFromDynMgr();
				HANDLE_RETURN(TEL_FAILURE);
			}
	   		if(rc == -2)
			{
				rc = sendRequestToDynMgr(mod, &msg);
				if (rc == -1)
				{
					closeChannelFromDynMgr();
					HANDLE_RETURN(TEL_FAILURE);
				}
				requestSent=1;
				rc = readResponseFromDynMgr(mod,20,
						&response,sizeof(response));
				if (rc == TEL_TIMEOUT)
				{
					telVarLog(mod,REPORT_NORMAL, TEL_BASE, 
					GV_Err, "Timeout waiting for response "
					"from Dynamic Manager.");
					closeChannelFromDynMgr();
					HANDLE_RETURN(TEL_TIMEOUT);
				}
				if(rc == -1)
				{
					closeChannelFromDynMgr();
					HANDLE_RETURN(TEL_FAILURE);
				}
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,20,
					&response,sizeof(response));
			if (rc == TEL_TIMEOUT)
			{
				telVarLog(mod,REPORT_NORMAL, TEL_BASE, 
				GV_Err, "Timeout waiting for response "
				"from Dynamic Manager.");
				closeChannelFromDynMgr();
				HANDLE_RETURN(TEL_TIMEOUT);
			}
			if(rc == -1)
			{
				closeChannelFromDynMgr();
				HANDLE_RETURN(TEL_FAILURE);
			}
                }
		GV_TerminatedViaExitTelecom=1;
	
		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_DISCONNECT:
			GV_Connected1=0;	
			GV_Connected2=0;
			closeChannelFromDynMgr();
			HANDLE_RETURN(0);
			break;
		case DMOP_GETDTMF:
			/* Ignore all DTMF characters. */
			break;
		default:
			/* Unexpected message. Logged in examineDynMgr... */
			continue;
			break;
		} /* switch rc */
			
	} /* while */
}

static int closeChannelFromDynMgr()
{
	int rc, i;
	char list[50];
	int pid;
	char speakTTSFile[30];
	char speakSilenceFile[128];

	/* Close & remove the response fifo; it is no longer necessary */
	close(GV_ResponseFifoFd);
	unlink(GV_ResponseFifo);

	for(i=1;;i++)
	{
 		sprintf(list, "/tmp/list.%d.%d", GV_AppPid, i);
		if(unlink(list) < 0)
			break;
	}

	pid = getpid();
	for(i=0;;i++)
	{
		sprintf(speakTTSFile, "speaktts@%d_%d.wav", pid, i);
		if(unlink(speakTTSFile) < 0)
			break;
	}

	for(i=0;;i++)
	{
		sprintf(speakSilenceFile, "silence@%d_%d.64p", pid, i);
		if(unlink(speakSilenceFile) < 0)
			break;
	}


	return(0);
}