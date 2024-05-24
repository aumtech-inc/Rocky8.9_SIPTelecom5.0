#ident	"@(#)COM_Mnemonics.h 99/04/14 2.8.0 Copyright 1999 Aumtech, Inc"
/*-----------------------------------------------------------------------------
Program:	COM_Mnemonics.h
Purpose:	Mnemonics list of all Common APIs.
Author:		R. Schmidt
Date:		07/27/94
Update:		01/20/95 R. Schmidt MR799 - Mnemonic for UTL_BusinessHours API.
Update:		03/15/96 G. Wenzel added UTL_LOGMESSAGE
Update:		04/14/99 G. Wenzel updated copyright, comments; no other changes

-----------------------------------------------------------------------------
Copyright (c) 1999, Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/

#define COM_API_BASE 10000

enum COM_Mnemonics {
/* Call Detail APIs */
CDR_CUSTOMEVENT = COM_API_BASE,
CDR_SERVICETRAN,

/* Index Array APIs */
ARY_INDXARYINIT,
ARY_INDXARYRETRIEVE,

/* Utilities APIs */
UTL_ADDTOFILE,
UTL_BUSINESSHOURS,
UTL_COPYFILE,
UTL_CREATEFILE,
UTL_DELETEFILE,
UTL_GETFIELD,
UTL_LOGMESSAGE,
UTL_SUBSTRING,
UTL_SENDTOMONITOR
};

static	char	*COM_API_NAME[] =
	{
		"CDR_CustomEvent",
		"CDR_ServiceTran",
		"ARY_IndxaryInit",
		"ARY_IndxaryRetrieve",
		"UTL_AddToFile",
		"UTL_BusinessHours",
		"UTL_CopyFile",
		"UTL_CreateFile",
		"UTL_DeleteFile",
		"UTL_GetField",
		"UTL_LogMessage",
		"UTL_Substring",
		"UTL_SendToMonitor"
	};

static int  tot_com_api = (sizeof(COM_API_NAME))/(sizeof(COM_API_NAME[0]));
extern char *GV_API_COM[];
extern int  GV_API_COM_num;
