#ident	"@(#)UTL_LogMessage 96/03/15 2.2.0 Copyright 1996 AT&T"
static char ModuleName[]="UTL_LogMessage";
/*-----------------------------------------------------------------------------
Program:	UTL_LogMessage.c
Purpose:	Log a message to the Log Manager system.
Author:		George Wenzel
Date:		02/26/96
-----------------------------------------------------------------------------
Copyright (c) 1996, AT&T Network Systems Information Services Platform (ISP)
All Rights Reserved.

This is an unpublished work of AT&T ISP which is protected under 
the copyright laws of the United States and a trade secret
of AT&T. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of AT&T.
------------------------------------------------------------------------------*/
#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h" 
#include "COM_Mnemonics.h" 
#include "monitor.h"

extern int LOGXXPRT(char *module, int requested_mode, int MessageID, char *bogus, ... );

int UTL_LogMessage(module, msg)
int	module;
const char *msg;
{
	int ret;
	char diag_mesg[256];

	sprintf(diag_mesg,"%s", msg);
	ret = send_to_monitor(MON_API, UTL_LOGMESSAGE, diag_mesg);
       
	/* Write the passed module name and message. */ 
	LOGXXPRT(module,REPORT_NORMAL,COM_ISPDEBUG, msg);
	return(0);
}
