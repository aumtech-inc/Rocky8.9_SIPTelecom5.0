#ident	"@(#)ss_GetGlobalString 96/06/14 2.2.0 Copyright 1996 Aumtech, Inc."
static char ModuleName[]="ss_GetGlobalString";
/*-----------------------------------------------------------------------------
Program:	ss_GetGlobalString.c
Purpose:	Get Global string variables.
Author:		Aumtech Inc.
Update:		06/10/97 sja	Created the file
Update:         05/04/98 gpw    removed .h files not needed for lite version
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
Update: 09/14/12 djb  Updated/cleaned up logging.
-----------------------------------------------------------------------------
 *
 * Copyright (c) 1996, Aumtech, Inc.
 * All Rights Reserved.
 *
 * This is an unpublished work of Aumtech which is protected under 
 * the copyright laws of the United States and a trade secret
 * of Aumtech. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include "ssHeaders.h"
#include "WSS_Externs.h"

#define	NUM_GLOBALS	1

/* valid set of global variables to be used in api */
static	char	*globals[] = { 
				"$CLIENT_HOST",
				""
			     };
			
/*------------------------------------------------------------------------------
This routine gets the value for the variable in the "globals" table. 
Global variable # is the index of the variable in the "globals" static vector. 
------------------------------------------------------------------------------*/
int ss_GetGlobalString(const char *var_name, char *var_value)
{
	int	index = 0;
	int	ret_code;
	char	diag_mesg[128];

	sprintf(LAST_API,"%s",ModuleName);

	if (GV_SsConnected == 0)
	{
		sprintf(__log_buf, "%s|%d|Client disconnected", __FILE__, __LINE__);
		Write_Log(ModuleName, 1, __log_buf);
		return (ss_DISCONNECT);
	}

	/* Get the variable index in globals[]. */
	while((strcmp(globals[index], var_name)) != 0)
	{
    		index++;

    		/* If noun is not found */
    		if(index >= NUM_GLOBALS)
    		{
			sprintf(__log_buf, 
				"Invalid global variable: <%s> passed",
				var_name);
			Write_Log(ModuleName, 0, __log_buf);
			return(-1);
    		}
	}

	/* Switch on globals[] index. */
	switch(index)
	{
    		case 0: 		/* $CLIENT_HOST */
			strcpy(var_value, GV_ClientHost);
			break;

    		default:
			sprintf(__log_buf, 
				"Invalid global variable: <%s> passed",
				var_name);
			Write_Log(ModuleName, 0, __log_buf);
			return(ss_FAILURE);
	}

	return(ss_SUCCESS);		
} /* ss_GetGlobalString() */
