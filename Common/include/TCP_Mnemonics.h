#ident  "@(#)TCP_Mnemonics.h     96/02/09 2.2.0 Copyright 1996 Aumtech Inc."
/*---------------------------------------------------------------------
Program:	TCP_Mnemonics.h
Purpose:        To define a set of #defines which index into an array of
                TCP Server API names. Used by the monitoring system.
Author: 	M. Joshi
Date:		02/01/96
Update:		02/12/96 G. Wenzel changed TCP_API_NAME entries to upper/lower
Update:		03/13/96 G. Wenzel added TCP_COMINIT
Update:		10/13/98 mpb added TCP_GETCURSORPOS & TCP_GetCursorPos
----------------------------------------------------------------------
 - Copyright (c) 1996, AT&T Network Systems Information Services 
 - Platform (ISP)
 -
 - All Rights Reserved.
 -
 - This is an unpublished work of AT&T ISP which is protected under 
 - the copyright laws of the United States and a trade secret
 - of AT&T. It may not be used, copied, disclosed or transferred
 - other than in accordance with the written permission of AT&T.
 --------------------------------------------------------------------*/
#ifndef	__TCP_MNEMONICS_H__
#define	__TCP_MNEMONICS_H__	

#define TCP_API_BASE 2000

enum TCP_Mnemonics
{
      TCP_NOTHING = TCP_API_BASE,
      TCP_CHECK, 
      TCP_COMINIT, 
      TCP_EXIT,
      TCP_GETDATA,
      TCP_GETGLOBAL,
      TCP_HOSTEVENT,
      TCP_INIT,
      TCP_POSCURSOR,
      TCP_PUTDATA,
      TCP_SENDKEY,
      TCP_SETGLOBAL,
      TCP_GETCURSORPOS,
};

static char *TCP_API_NAME[] = {
      	"N/A" ,
      	"TCP_Check" ,
      	"TCP_ComInit" ,
      	"TCP_Exit",
      	"TCP_GetData",
      	"TCP_GetGlobal",
      	"TCP_HostEvent",
      	"TCP_Init",
      	"TCP_PosCursor",
      	"TCP_PutData",
      	"TCP_SendKey",
      	"TCP_SetGlobal",
      	"TCP_GetCursorPos"
	};
static int  tot_tcp_api = (sizeof(TCP_API_NAME))/(sizeof(TCP_API_NAME[0]));

#endif /* __TCP_MNEMONICS_H__ */
