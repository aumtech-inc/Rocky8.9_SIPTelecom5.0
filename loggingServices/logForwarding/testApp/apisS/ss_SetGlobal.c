#ident "@(#)ss_SetGlobal 97/03/28 2.2.0 Copyright 1996 Aumtech, Inc."
/*--------------------------------------------------------------------
Program:	ss_SetGlobal
Purpose:	Set Global variables.
Author:		Aumtech Inc.
Update:		03/28/97 sja	Created the file
Update:		03/31/97 sja	Added send_to_monitor
Update:		04/02/97 sja	Changed #include WSS_Globals.h to WSS_Externs.h
Update:		06/05/97 sja	Removed WSS_VAR.h, $SYS_DELIMITER
Update:         05/04/98 gpw    removed .h files not needed for lite version
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
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

static char ModuleName[] = "ss_SetGlobal";

#define	NUM_NOUNS	1	/* must match number of entries in Nouns */

static char *Nouns[] = { 
		 	"$SYS_TIMEOUT"
                       };
/*--------------------------------------------------------------------
ss_SetGlobal():
--------------------------------------------------------------------*/
int ss_SetGlobal(char *GlobalName, int GlobalValue)
{
	int	Index = 0;
	char  	diag_mesg[100];

	sprintf(LAST_API, "%s", ModuleName);

	if (GV_SsConnected == 0)
	{
		Write_Log(ModuleName, "Client disconnected");
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
			Write_Log(ModuleName, __log_buf);
        		return(ss_FAILURE);
    		}
  	} 
  	switch(Index) 
	{
    		case 0:		/* $SYS_TIMEOUT */
      			if (GlobalValue <= 0)
			{
				sprintf(__log_buf,
				"Value (%d) not valid for Global Variable %s",
					GlobalValue,"$SYS_TIMEOUT");
				Write_Log(ModuleName, __log_buf);
        			return(ss_FAILURE);
      			}
      			GV_SysTimeout = GlobalValue;
    			break;
    		default:
			sprintf(__log_buf, 
				"Invalid global variable: <%s> passed",
				GlobalName);
			Write_Log(ModuleName, __log_buf);
      			return(ss_FAILURE);
    			break;
	}
  	return(ss_SUCCESS);                          
} /* END: ss_SetGlobal() */
