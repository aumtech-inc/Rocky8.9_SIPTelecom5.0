/*-----------------------------------------------------------------------------
Program:	SNA_Mnemonics.h
Purpose:	To provide a #define Mnemonic for each of the SNA APIs
		to be used for monitoring. 
		NOTE: This file uses externs GV_API_SNA[] and GV_API_SNA_num.
Author:		D. Stephens
Date:		06/28/94
Update:		02/09/96 G. Wenzel added this header
Update:		02/12/96 M. Joshi added SNA_API_NAME array
Update:		03/13/96 G. Wenzel added SNA_SCREENDUMP
 ------------------------------------------------------------------------------
 - Copyright (c) 1996, AT&T Network Systems Information Services Platform (ISP)
 - All Rights Reserved.
 -
 - This is an unpublished work of AT&T ISP which is protected under 
 - the copyright laws of the United States and a trade secret
 - of AT&T. It may not be used, copied, disclosed or transferred
 - other than in accordance with the written permission of AT&T.
 ----------------------------------------------------------------------------*/

#ifndef	__SNA_MNEMONICS_H__
#define	__SNA_MNEMONICS_H__	

#define SNA_API_BASE 3000

enum SNA_Mnemonics
{
      SNA_NOTHING = SNA_API_BASE,
      SNA_CHECK,
      SNA_EXIT,
      SNA_GETDATA,
      SNA_GETGLOBAL,
      SNA_HOSTEVENT,
      SNA_INIT,
      SNA_POSCURSOR,
      SNA_PUTDATA,
      SNA_SCREENDUMP,
      SNA_SENDKEY,
      SNA_SETGLOBAL,
      SNA_RETURN,
      SNA_GETSTATINFO,
      SNA_GETSTATUSVALUE,
};

static char *SNA_API_NAME[] = 
	{
	"N/A",
	"SNA_Check",
      	"SNA_Exit",
      	"SNA_GetData",
      	"SNA_GetGlobal",
      	"SNA_HostEvent",
      	"SNA_Init",
      	"SNA_PosCursor",
      	"SNA_PutData",
      	"SNA_ScreenDump",
      	"SNA_SendKey",
      	"SNA_SetGlobal",
      	"SNA_Return",
      	"SNA_GetStatInfo",
      	"SNA_GetStatusValue" 
	} ;

static int  tot_sna_api = (sizeof(SNA_API_NAME))/(sizeof(SNA_API_NAME[0]));

/* extern char *GV_API_SNA[]; */
/* 	extern int  GV_API_SNA_num; */

#endif /* __SNA_MNEMONICS_H__ */
