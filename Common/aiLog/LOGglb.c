/*-----------------------------------------------------------------------------
Program:	LOGglb.c
Purpose:	To define all global variables used by the logging system.
		And to Set the default values for GV_LogConfig structure.

 		NOTE: The corresponding 'extern' decalrations are in loginc.h.
Author:		J. Huang
Date:		05/04/94
Update:	02/19/96 G. Wenzel updated this header, added comments
Update:	06/29/94 J. Huang changed default value of Log_x_remote=True &
Update:			 Log_x_local=False;
Update:	03/20/96 G. Wenzel added  Log_Block_int,Log_block_timeout
Update:			to the structure with defaults
Update:	11/24/98 mpb added GV_Application_Name & GV_Resource_Name. 
Update:	01/07/03 mpb added 	GV_Log_Directory_Name[256].
Update:	04/16/03 mpb added GV_DateFormat.
-----------------------------------------------------------------------------
Copyright (c) 1996, 1997 Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
--------------------------------------------------------------------------- */
#include	"ispinc.h"
#include	"loginc.h"

LogAttr_t		GV_LogConfig= {
	/* CentralMgr */		False,
	/* LogID */			DEFAULT_LOG_ID,
	/* ReportingMode */		(Int32) 0,
	/* Log_x_remote */		True,
	/* Log_PrimarySrv */		{'\0'},
	/* Log_SecondarySrv */		{'\0'},
	/* Log_x_stderr */		False,
	/* Log_x_stdout */		False,
	/* Log_x_local */		False,
	/* Log_x_local_file */		DEFAULT_ISP_LOG,
	/* Log_Retention */		DEFAULT_RETENTION,
	/* Log_Block_int */		20,
	/* Log_block_timeout */		1000
};

MsgSet_t	GV_LOG_MsgDB[MAX_MSGSETS];
char		GV_AppId[MAX_APPID_LEN]={'\0'};
Bool		GV_LogInitialized=False;
char		GV_Application_Name[MAX_APPID_LEN];
char		GV_Resource_Name[MAX_APPID_LEN];
char		GV_Log_Directory_Name[256]={'\0'};
int			GV_DateFormat;
int			GV_CevEventLogging = 0;
