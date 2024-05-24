/*------------------------------------------------------------------------------
File:		TEL_ScheduleNTCall.c
Purpose:	Network Transfer: Schedule an outbound call to be placed at a specific time
Author:		Aumtech, Inc.
Date:		09/05/2012 djb	Created the file from TEL_ScheduleCall.c
-----------------------------------------------------------------------------
Copyright (c) 2001, Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "ocmHeaders.h"
#include "ocmNTHeaders.h"

static char	ModuleName[] = "TEL_ScheduleNTCall";

// static int tellRespToDoIt(struct MsgToRes *zMsg);
static int getTrunkGroup(char *zPort, char *zUsageField);
static int tellRespToDoIt(struct MsgToRes *zMsg);

/*-----------------------------------------------------------------------------
TEL_ScheduleNTCall(): Schedule an outbound call at a specific time.
-----------------------------------------------------------------------------*/
int TEL_ScheduleNTCall(
	int 	nrings,		/* # of rings before the call is 	*/
						/* declared a failure 			*/
	int 	tries,		/* # of tries before the call is 	*/
						/* considered a complete failure	*/
	int 	informat,	/* NAMERICAN / NONE			*/
	char 	*phoneNumber,	/* Phone number to call			*/
	char 	*callTime,	/* Time to call in :			*/
						/* YYMMDDHHMMSS, HHMMSS, HHMM, HH	*/
	char 	*newApp,	/* New app. to fire 			*/
	char	*appToAppInfo,	/* Any info that is passed to newApp	*/
	char	*cdf		/* Call definition file for this call	*/
	)
{
	static char	mod[]="TEL_ScheduleNTCall";
	struct MsgToRes		msg;
	char				apiAndParms[MAX_LENGTH] = "";
	char				reason[256];
	int					rc;
	char				myPort[10];
	char				trunkGroup[64];

	memset(reason, 0, sizeof(reason));
	memset(trunkGroup, 0, sizeof(trunkGroup));

	sprintf(apiAndParms,"%s(%d,%d,%s,%s,%s,%s,%s,CDF)",
				ModuleName,
				nrings, tries, arc_val_tostr(informat),
				phoneNumber, callTime, newApp, appToAppInfo);

	rc = apiInit(mod, TEL_SCHEDULENTCALL, apiAndParms, 1, 0, 0);
   	if (rc != 0) HANDLE_RETURN(rc);

	sprintf(myPort, "%d", GV_AppCallNum1);

	if ((rc = getTrunkGroup(myPort, trunkGroup)) == -1)
	{
		HANDLE_RETURN(rc);		// message logged in routine
	}

	rc = schedNTCall(nrings, tries, informat,phoneNumber,callTime,
						newApp,appToAppInfo,cdf,trunkGroup, reason);
	if(rc < 0)
	{
			telVarLog (mod, REPORT_NORMAL, TEL_DATA_NOT_FOUND, GV_Err,
				"[%s, %d] Failed to schedule call. %s", __FILE__, __LINE__, reason);
    		HANDLE_RETURN (TEL_FAILURE);
	}
	telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
			"[%s, %d] Successfully created cdf (%s).", __FILE__, __LINE__, cdf);
//	sleep(3);

	memset((struct MsgToRes *)&msg, '\0', sizeof(msg));
    msg.opcode = RESOP_START_NTOCS_APP;
    msg.appCallNum = GV_AppCallNum1;
    msg.appPid = GV_AppPid;
    msg.appRef = GV_AppRef++;
    msg.appPassword = GV_AppPassword;
	sprintf(msg.data, "%s", cdf);

//
//	return(0);
//
	rc = tellRespToDoIt(&msg);
	HANDLE_RETURN(rc);
} /* END: TEL_ScheduleNTCall() */

/*-----------------------------------------------------------------------------
tellRespToDoIt(): Get the app. from resp. based on DNIS
-----------------------------------------------------------------------------*/
static int tellRespToDoIt(struct MsgToRes *zMsg)
{
	static char		mod[]="tellRespToDoIt";
	int 			toRespFifoFd;
	struct MsgToApp response;
	int 			lGotResponseFromResp = 0;
	int 			rc;
	
	if ((mkfifo(GV_ResFifo, S_IFIFO | 0666) < 0) && (errno != EEXIST))
	{

		telVarLog(mod, REPORT_NORMAL, TEL_IPC_ERROR, GV_Err,
			"[%s, %d] Failed to create FIFO (%s). [%d, %s] "
			"Unable to read/write to responsibility.",
			__FILE__, __LINE__, GV_ResFifo, errno, strerror(errno));
	  	return(-1);
	}
	
	if ((toRespFifoFd = open(GV_ResFifo, O_RDWR)) < 0)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_IPC_ERROR, GV_Err,
			"[%s, %d] Failed to open resp FIFO (%s). [%d, %s] Unable to read/write to responsibility. ",
			__FILE__, __LINE__, GV_ResFifo, errno, strerror(errno));
		return(-1);
	}                 
	
	rc = write(toRespFifoFd, zMsg, sizeof(struct MsgToRes));
	if(rc == -1)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_IPC_ERROR, GV_Err,
	        "[%s, %d] Can't write message to FIFO (%s). [%d, %s]",
			__FILE__, __LINE__, GV_ResFifo, errno, strerror(errno));
		close(toRespFifoFd);
	   	return(-1);
	}
	
	telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
	        "[%s, %d] Sent %d bytes to (%s). msg={%d,%d,%d,%d,%d,%s}",
			__FILE__, __LINE__, rc, GV_ResFifo, zMsg->opcode, zMsg->appCallNum, 
			zMsg->appPid, zMsg->appRef, zMsg->appPassword, zMsg->data);
	
	close(toRespFifoFd);
	
	while(lGotResponseFromResp == 0)
	{
		// The readResponseFromDynMgr name is misleading. Here it reads the
		// the response sent by Responsibility to the App's response fifo.
		if(readResponseFromDynMgr(mod,2,&response,sizeof(response)) == -1)
			return(-1);
	
		switch(response.opcode)
		{
			case RESOP_START_NTOCS_APP:
				lGotResponseFromResp = 1;
				return(response.returnCode);
				break;

            case DMOP_DISCONNECT:
                return(TEL_DISCONNECTED);
                break;

            default:
                return(response.returnCode);
		} 
	}
	return(0);
} /* END: tellRespToDoIt() */

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int getTrunkGroup(char *zPort, char *zUsageField)
{
	static char	mod[]="getStaticDest";
	char		*basePath;
	char		resourceFile[128];
	int			i;
	char		record[256];
	FILE		*fp;
	char		*pStrtokPtr;
	char		chan[32];

	/* set ISPBASE path */
    if ((basePath = (char *) getenv ("ISPBASE")) == NULL)
    {
		telVarLog (mod, REPORT_NORMAL, TEL_INITIALIZATION_ERROR, GV_Err,
                "[%s, %d] Environment variable ISPBASE is not found/set. "
				"Unable to continue.  Set ISPBASE and retry.", __FILE__, __LINE__);
        return (-1);
    }
	sprintf(resourceFile, "%s/Telecom/Tables/ResourceDefTab", basePath);

    if ((fp = fopen(resourceFile, "r")) == NULL)
    {
		telVarLog (mod, REPORT_NORMAL, TEL_FILE_IO_ERROR, GV_Err,
                "[%s, %d] Failed to open file (%s). [%d, %s]  Unable to continue.",  __FILE__, __LINE__,
				resourceFile, errno, strerror (errno));
        return (-1);
    }

	memset((char *)chan, '\0', sizeof(chan));
    while (fgets (record, sizeof (record), fp) != NULL)
    {
		if ( ((pStrtokPtr = (char *)strtok(record, "|")) == (char *)NULL))
		{
			continue;
		}
		sprintf(chan, "%s", pStrtokPtr);
		if (strcmp(chan, zPort) != 0)
		{
			continue;
		}

		// now get the usage 
		for (i=0; i<3; i++)
		{
			if ((pStrtokPtr = (char *)strtok(NULL, "|")) == NULL)
			{
				break;
			}
		}
		sprintf(zUsageField, "%s", pStrtokPtr);

		break;
    }
    (void) fclose (fp);

	if (! zUsageField )
	{
		telVarLog (mod, REPORT_NORMAL, TEL_FILE_IO_ERROR, GV_Err,
                "[%s, %d] Failed to retrieve port (%s) from (%s). Unable to continue.",
				__FILE__, __LINE__, zPort, resourceFile);
        return (-1);
	}
    return (0);
} // END: getTrunkGroup()
