/*-----------------------------------------------------------------------------
Program:        TEL_SRLogEvent.c 
Purpose:        Ask DM to log the SR event.
Author:         Anand Joglekar
Date:			03/30/2004

ddn: 05-14-2004 createSrLogEventFile checks
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int createSrLogEventFile(char *mod, const char *text, 
	char *file);

int TEL_SRLogEvent(char *zpText)
{
	if ( GV_SRType == GOOGLE_SR )
	{
		telVarLog("TEL_SRLogEvent",REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Google SR is enabled. Returning with no action.");
		return(0);
	}

#ifndef MRCP_SR
	static const char mod[]="TEL_SRLogEvent";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;
	int requestSent = 0;

	struct Msg_SRLogEvent lMsg;
	struct MsgToDM msg; 

	char m[101];

	if(zpText == NULL)
	{
		return(-1);
	}

	if(strlen(zpText) < 100)
	{
		sprintf(m, "%s", zpText);
	}
	else
	{
		strncpy(m, zpText, 100);
		m[100] = '\0';
	}

	sprintf(apiAndParms,"%s(%s)", mod, m);
	rc = apiInit(mod, TEL_SRLOGEVENT, apiAndParms, 0, 0, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	lMsg.opcode = DMOP_SRLOGEVENT;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;

	memset(lMsg.logEventFile, 0, sizeof(lMsg.logEventFile));
	rc = createSrLogEventFile(mod, zpText, lMsg.logEventFile);
	if (rc != 0) 
	{
		unlink(lMsg.logEventFile);
		HANDLE_RETURN(rc);
	}
	
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRLogEvent));

	rc = sendRequestToDynMgr(mod, &msg);
	if (rc == -1) HANDLE_RETURN(-1);
#endif
	HANDLE_RETURN(0);
}

static int createSrLogEventFile(char *mod, const char *text, 
	char *file)
{
	FILE *fp;
	char fileName[20];
	char	m[100];

	*file = '\0';
	sprintf(fileName, "SrLogEvent.%d.%d", GV_AppPid, GV_AppRef);

	fp = fopen(fileName, "w+");
	if(fp == NULL)
	{
		sprintf(m, "Failed to open file (%s) for writing. [%d, %s]",
			fileName, errno, strerror(errno));
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
		return(-1);
	}

	fprintf(fp, "%s\n", text);

	fclose(fp);

	sprintf(file, "%s", fileName);
	return(0);
}
