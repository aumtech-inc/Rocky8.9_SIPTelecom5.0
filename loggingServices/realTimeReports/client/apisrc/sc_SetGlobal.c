#ident "@(#)sc_SetGlobal 97/03/28 2.2.0 Copyright 1996 Aumtech, Inc."
/*--------------------------------------------------------------------
Program:	sc_SetGlobal
Purpose:	Set Global variables.

Author:		Aumtech Inc.

Update:		03/28/97 sja	Created the file
Update:		03/31/97 sja	Added static char ModuleName[] = ...
Update:		06/05/97 sja	Removed $SYS_DELIMITER
Update:	06/11/98 mpb	changed GV_Connected to GV_ScConnected. 
Update:	09/14/12 djb	Updated logging.
 ---------------------------------------------------------------------
 - Copyright (c) 1996, Aumtech Inc. Information Services Platform (AISP)
 - All Rights Reserved.
 -
 - This is an unpublished work of Aumtech Inc. ISP which is protected under 
 - the copyright laws of the United States and a trade secret
 - of Aumtech Inc. It may not be used, copied, disclosed or transferred
 - other than in accordance with the written permission of Aumtech Inc.
 --------------------------------------------------------------------*/
#include "WSC_var.h"
#include "sc.h"

static char ModuleName[] = "sc_SetGlobal";

#define	NUM_NOUNS	1	/* must match number of entries in Nouns */

static char *Nouns[] = { 
		 	"$SYS_TIMEOUT"
                       };
/*--------------------------------------------------------------------
sc_SetGlobal():
--------------------------------------------------------------------*/
int sc_SetGlobal(char *GlobalName, int GlobalValue)
{
	int	ret_code;
	int	Index = 0;
	char  string[100];

	if (GV_ScConnected == 0)
	{
		Write_Log(ModuleName, 2, "Server disconnected."); 
		return (sc_DISCONNECT);
	}

	/* Find the index of the noun passed. */
  	while( ( strcmp(Nouns[Index], GlobalName) ) )
	{
    		Index++;
    		if(Index >= NUM_NOUNS)
		{
			sprintf(__log_buf,"Invalid global var (%s) passed.", GlobalName);
			Write_Log(ModuleName, 0, __log_buf);
       		return(sc_FAILURE);
   		}
  	} 
  	switch(Index) 
	{
    		case 0:		/* $SYS_TIMEOUT */
      			if (GlobalValue <= 0)
				{
				sprintf(__log_buf, 
				     "Value (%d) not valid for global var. %s",
        				GlobalValue,"$SYS_TIMEOUT");
				Write_Log(ModuleName, 0, __log_buf);
       			return(sc_FAILURE);
      			}
      			GV_SysTimeout = GlobalValue;
    			break;

    		default:
			sprintf(__log_buf,"Invalid global var (%s) passed.",
        							GlobalName);
			Write_Log(ModuleName, 0, __log_buf);
      			return(sc_FAILURE);
    			break;
	}
  	return(sc_SUCCESS);                          
} /* END: sc_SetGlobal() */
