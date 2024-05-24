#ident	"@(#)UTL_SendToMonitor 96/02/26 2.2.0 Copyright 1996 AT&T"
static char ModuleName[]="UTL_SendToMonitor";
/*-----------------------------------------------------------------------------
Program:	UTL_SendToMonitor.c
Purpose:	Send a message to the monitor. This routine is mostly for
		programmers to help debug their code.
		NOTE: This routine always returns a 0, even if there is
			a problem sending to monitor. Since this API is for
			debugging, we don't want any error processing to be
			done (i.e., processing of a -1 return code) if we
			can't send the debug message to the monitor.
		NOTE: The first param to pass must be determined - gpw

Author:		George Wenzel
Date:		02/26/96
Update:		04/10/96 G. Wenzel removed logging of error.
-----------------------------------------------------------------------------
Copyright (c) 1996, AT&T Network Systems Information Services Platform (ISP)
All Rights Reserved.

This is an unpublished work of AT&T ISP which is protected under 
the copyright laws of the United States and a trade secret
of AT&T. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of AT&T.
-----------------------------------------------------------------------------*/
#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h" 
#include "monitor.h"

/*------------------------------------------------------------------------------
This routine gets the value of desired field from the buffer.
------------------------------------------------------------------------------*/
int UTL_SendToMonitor(id, msg)
int	id;	/* id number used to specify a specify source code location*/
const char *msg;
{
	int ret;
	
	ret = send_to_monitor(MON_CUSTOM, id, msg);
	return(0);
}
