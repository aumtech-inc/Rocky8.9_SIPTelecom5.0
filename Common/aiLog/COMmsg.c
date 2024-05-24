/*-----------------------------------------------------------------------------
Program:	COMmsg.c
Purpose:	ISP common message format definitions 
		This file provides :
		1) Provides message body format definitions for ISP common 
			messages. 
		2) Initializes COM_msg_num, total number of messages.
Author:		Jenny Huang
Date:		03/25/94
Update:		02/15/96 G. Wenzel removed "Debug" from COM_ISPDEBUG message
Update:		03/04/96 G. Wenzel added errno to COM_UTLFILE_OPEN msg
Update:		03/04/96 G. Wenzel removed unused messages.
Update:		04/01/96 G. Wenzel added COM_ERR_PARM_RANGE.
Update:		06/26/96 G. Wenzel updated copyright
Update:		06/26/96 G. Wenzel added key # and filename to COM_EKEYFIELD 
Update:		06/26/96 G. Wenzel added key val, key #, filename to 
Update:				COM_EKEYFNDNOFIELD
Update:		06/26/96 G. Wenzel added %d for variable size in COM_EFILSIZLIM
Update:		06/26/96 G. Wenzel added %s for filename in COM_EEMPTYBTREE
Update:		11/23/98 gpw removed brackets from COM_ERR_PARMSTR msg
Update:	06/07/98 mpb added errno & error msg string to COM_ERR_FILEACCESS.
------------------------------------------------------------------------------ 
Copyright (c) 1996, Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of AT&T.
-----------------------------------------------------------------------------*/

/* CAUTION: The message index must match value of its mnemonic in COMmsg.h */

char *	GV_COMmsg[] = {
"%s",						/* 0  COM_ISPDEBUG */
"", 	/* 1 */
"", 	/* 2 */
"", 	/* 3 */
"", 	/* 4 */
"", 	/* 5 */	 
"", 	/* 6 */
"Registered to ISP Common Logger.",		/* 7 COM_ILOGSTART */
"De-register from ISP Common Logger.",		/* 8 COM_ILOGEXIT */
"" , 	/* 9 */
"" , 	/* 10 */
"" ,	/* 11 */
"" ,	/* 12 */
"" ,	/* 13 */
"" ,	/* 14 */
"" ,	/* 15 */
"" ,	/* 16 */
"" ,	/* 17 */
"" ,	/* 18 */
"" ,	/* 19 */
"" ,	/* 20 */
"" ,	/* 21 */
"", /* 22 */
"", /* 23 */
"", /* 24 */
"", /* 25 */
"", /* 26 */
"Error mallocing buffer",			/* 27 COM_UTLFILE_MALLOC */
"Error opening file <%s>. errno=%d",		/* 28 COM_UTLFILE_OPEN */
"",	/* 29 */
"",	/* 30 */
/* General Module messages. */
"Invalid parameter %s: %d. Valid values: (%d,%d)",/* 31 COM_ERR_PARM_RANGE*/
"%s",						/* 32 COM_GENSTR */
"Invalid parameter %s: %d.",			/* 33 COM_ERR_PARMNUM */
"Invalid parameter %s: %s.",			/* 34 COM_ERR_PARMSTR */

/* General Diagnose Module messages. */
"%s module failed. Return code = %d.",		/* 35 COM_DIA_MODFAIL */
"%s module succeeded.",				/* 36 COM_DIA_MODSUCCEED */
"%s module succeeded. Data = %d.",		/* 37 COM_DIA_SUCCEEDNUM */
"%s module succeeded. Data = [%s].",		/* 38 COM_DIA_SUCCEEDSTR */

"", /* 39 */
"", /* 40 */
"Entering module: %s (%s)",			/* 41 COM_DIA_MODENTER */
"", /* 42 */
"", /* 43 */
"", /* 44 */
"", /* 45 */
"", /* 46 */
"", /* 47 */
"", /* 48 */
"", /* 49 */
"", /* 50 */
"", /* 51 */
"", /* 52 */
"", /* 53 */
"", /* 54 */
"", /* 55 */
"", /* 56 */
"", /* 57 */
"", /* 58 */
"", /* 59 */
"", /* 60 */
"", /* 61 */
"", /* 62 */
"", /* 63 */
"", /* 64 */
"", /* 65 */
"", /* 66 */
"", /* 67 */
"", /* 68 */
"Invalid SNMP trap value \"%s\". Used default value 90010.", /* 69 COM_ERR_CEVTRAP*/

"%s not initialized.",				/* 70 COM_ERR_NOTINIT */
"Starting position > source string length",	/* 71 COM_ERR_STRTSPOS */
"Unable to remove file <%s>. errno: %d",	/* 72 COM_ERR_FILEREMOVE */
"Unable to access file <%s>.errno=%d. [%s]",	/* 73 COM_ERR_FILEACCESS */
"File <%s> already exists.",			/* 74 COM_ERR_FILEEXISTS */
"File <%s> does not exist.",				/* 75 COM_ERR_NOFILE */
"Unable to copy <%s> to <%s>.",				/* 76 COM_ERR_COPY */
"Custom Event record > 200 chars. Data ignored.", 	/* 77 COM_ERR_CEVDATA */
"Appfind error, appmaint: [%s] rc = %d, status = %d ",	/* 78 COM_UTLLLMGR_ERR*/
"",							/* 79 */
"Module %s failed",					/* 80 COM_MOD_FAIL */

/* Indxary messages. */
"Failed to get status of file <%s>. errno=%d",  /* 81 COM_ESTATFAILED */
"File %s exceeds maximum length of %d bytes.", 	/* 82 COM_EFILSIZLIM */
"Empty btree index created/found in memory for file %s.",/* 83 COM_EEMPTYBTREE*/
"Warning: Ignoring duplicate key value <%s> in file %s.",/*84COM_EDUPKEYIGNORE*/
"Failed to insert node %s",                     /* 85 COM_EINSERTNODE */
"Failed to open data file %s. errno=%d.",       /* 86 COM_EOPENDATFIL */
"Key column %d doesn't exist in %s.",
				                /* 87 COM_EKEYFIELD */
"Key value <%s> found in key column %d, but retrieve column %d doesn't exist in %s.", 
						/* 88 COM_EKEYFNDNOFIELD */
"Empty file %s",                                /* 89 COM_EFILEMPTY */
"Unable to select SNMP trap enabling.",		/* 90 COM_SNMP_ENABLE */
"Unable to set SNMP trap identifier.",		/* 91 COM_SNMP_TRAP_ID */
"Unable to unselect SNMP trap enabling.",	/* 92 COM_SNMP_UNSET */
"Unable to unset SNMP trap identifier.",	/* 93 COM_SNMP_UNSET_ID */
};

int	GV_COM_msgnum = { sizeof( GV_COMmsg ) / sizeof( GV_COMmsg[0] ) };
