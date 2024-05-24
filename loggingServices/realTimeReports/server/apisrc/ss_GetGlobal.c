#ident "@(#)ss_GetGlobal 97/03/28 2.2.0 Copyright 1996 Aumtech, Inc."

/*--------------------------------------------------------------------
Program:	ss_GetGlobal
Purpose:	Set Global variables.
Author:		Aumtech Inc.
Update:		03/28/97 sja	Created the file
Update:		04/02/97 sja	Changed #include WSS_Globals.h to WSS_Externs.h
Update:		06/05/97 sja	Removed WSS_VAR.h, $SYS_DELIMITER
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
#include "ssHeaders.h"
#include "WSS_Externs.h"

static char ModuleName[] = "ss_GetGlobal";

#define	NUM_NOUNS	2	/* must match number of entries in Nouns */

static char *Nouns[] = { 
		 	"$SYS_TIMEOUT",
			"$CLIENT_PID"
                       };
/*--------------------------------------------------------------------
ss_GetGlobal():
--------------------------------------------------------------------*/
int ss_GetGlobal(char *GlobalName, int *GlobalValue)
{
	int	Index = 0;
	char  	diag_mesg[100];

	sprintf(LAST_API, "%s", ModuleName);

	if (GV_SsConnected == 0)
	{
		sprintf(__log_buf, "%s|%d|Client disconnected", __FILE__, __LINE__);
		Write_Log(ModuleName, 2, __log_buf);
		return (ss_DISCONNECT);
	}

	/* Find the index of the noun passed. */
  	while( ( strcmp(Nouns[Index], GlobalName) ) )
	{
    		Index++;
    		if(Index >= NUM_NOUNS)
		{
			sprintf(__log_buf, 
				"Invalid global variable: <%s> passed",
				GlobalName);
			Write_Log(ModuleName, 0, __log_buf);
        		return(ss_FAILURE);
    		}
  	} 
  	switch(Index) 
	{
    		case 0:		/* $SYS_TIMEOUT */
      			*GlobalValue = GV_SysTimeout;
    			break;

    		case 1: 	/* $CLIENT_PID */
			*GlobalValue = GV_ClientPid;
    			break;

    		default:
			sprintf(__log_buf, 
				"Invalid global variable: <%s> passed",
				GlobalName);
			Write_Log(ModuleName, 0, __log_buf);
      			return(ss_FAILURE);
	}
  	return(ss_SUCCESS);                          
} /* END: ss_GetGlobal() */
