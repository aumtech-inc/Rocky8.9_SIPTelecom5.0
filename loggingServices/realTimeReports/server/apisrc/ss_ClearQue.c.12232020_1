#ident "@(#)ss_ClearQue 97/03/28 2.2.0 Copyright 1996 Aumtech, Inc."

/*--------------------------------------------------------------------
Program:	ss_ClearQue.c
Purpose:	Set Global variables.
Author:		Aumtech Inc.
Update:		03/28/97 sja	Created the file
Update:		03/31/97 sja	Added send_to_monitor
Update:		04/02/97 sja	Changed #include WSS_Globals.h to WSS_Externs.h
Update:		04/15/97 sja	Ported to the "Lite" version
Update:		06/01/97 mpb	changed log messages
Update:         05/04/98 gpw    removed .h files not needed for lite version
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
Update: 09/14/12 djb  Updated/cleaned up logging.
 ---------------------------------------------------------------------
 - Copyright (c) 1996, Aumtech Inc. Information Services Platform (AISP)
 - All Rights Reserved.
 -
 - This is an unpublished work of Aumtech Inc. ISP which is protected under 
 - the copyright laws of the United States and a trade secret
 - of Aumtech Inc. It may not be used, copied, disclosed or transferred
 - other than in accordance with the written permission of Aumtech Inc.
 --------------------------------------------------------------------*/
#include "ss.h"
#include "ssHeaders.h"
#include "WSS_VAR.h"
#include "WSS_Externs.h"

static char ModuleName[] = "ss_ClearQue";

/*--------------------------------------------------------------------
ss_ClearQue():
--------------------------------------------------------------------*/
int ss_ClearQue(int queType)
{
	sprintf(LAST_API, "%s", ModuleName);

	if (GV_SsConnected == 0)
	{
		gaVarLog(ModuleName, 1, "Client is already disconnected");
		return (ss_DISCONNECT);
	}

	switch(queType)
	{
		case SENDQUE:
			memset(__S_clnt_mesg, 0, sizeof(__S_clnt_mesg));
			__S_clnt_mesg_ptr = &__S_clnt_mesg[0];
			break;
		
		case RECVQUE:
			memset(__R_clnt_mesg, 0, sizeof(__R_clnt_mesg));
			__R_clnt_mesg_ptr = &__R_clnt_mesg[0];
			break;

		default:
			sprintf(log_buf,
				"Invalid paramter %s: (%d), valid values %s",
				"queType",queType,"SENDQUE or RECVQUE");
			Write_Log(ModuleName, 0, log_buf);
			return(ss_FAILURE);
	} /* switch */

  	return(ss_SUCCESS);                          
} /* END: ss_ClearQue() */
