#ident "@(#)sc_ClearQue 97/03/28 2.2.0 Copyright 1996 Aumtech, Inc."

/*--------------------------------------------------------------------
Program:	sc_ClearQue.c
Purpose:	Set Global variables.

Author:		Aumtech Inc.

Update:		03/28/97 sja	Created the file
Update:		03/31/97 sja	Added static char ModuleName[] = ...
Update:		03/31/97 sja	if GV_Connected == 0 , return sc_DISCONNECT
Update:		04/15/97 sja	Ported to the "Lite" version
Update:		06/01/97 mpb	changed log_messages 
Update:	06/11/98 mpb	changed GV_Connected to GV_ScConnected. 
Update: 09/14/12 djb	Updated logging
 ---------------------------------------------------------------------
 - Copyright (c) 1996, Aumtech Inc. Information Services Platform (AISP)
 - All Rights Reserved.
 -
 - This is an unpublished work of Aumtech Inc. ISP which is protected under 
 - the copyright laws of the United States and a trade secret
 - of Aumtech Inc. It may not be used, copied, disclosed or transferred
 - other than in accordance with the written permission of Aumtech Inc.
Update:	09/14/12 djb	Updated logging.
 --------------------------------------------------------------------*/
#include "WSC_var.h"
#include "sc.h"

static char ModuleName[] = "sc_ClearQue";

/*--------------------------------------------------------------------
sc_ClearQue():
--------------------------------------------------------------------*/
int sc_ClearQue(int queType)
{
	if (GV_ScConnected == 0)
	{
		Write_Log(ModuleName, 2, "Server is already disconnected\n"); 
		return (sc_DISCONNECT);
	}
	switch(queType)
	{
		case SENDQUE:
			memset(S_mesg, 0, sizeof(S_mesg));
			S_mesg_ptr = &S_mesg[0];				
			break;
		
		case RECVQUE:
			memset(R_mesg, 0, sizeof(R_mesg));
			R_mesg_ptr = &R_mesg_ptr[0];				
			break;

		default:
			sprintf(__log_buf, "Invalid queType %d\n", queType);
			Write_Log(ModuleName, 0, __log_buf); 
			return(sc_FAILURE);
	} /* switch */

  	return(sc_SUCCESS);                          
} /* END: sc_ClearQue() */
