/*-----------------------------------------------------------------------------
Program:	COMmsg.h
Purpose:	To define mnemonic names for common messages.
		NOTE: This file must be in sync with COMmsg.c
Author:		J. Huang
Date:		03/25/94
Update:		08/09/94 R. Schmidt added INDXARY messages.
Update:		08/17/94 R. Schmidt removed all of DIAG_MDSD type messages.
Update:		12/15/94 P. Bruno added messages for SNMP trap selection.
Update:		04/01/96 G. Wenzel added COM_ERR_PARM_RANGE.
----------------------------------------------------------------------------- -
 * Copyright (c) 1996, AT&T Network Systems Information Services Platform (ISP)
 * All Rights Reserved.
 *
 * This is an unpublished work of AT&T ISP which is protected under 
 * the copyright laws of the United States and a trade secret
 * of AT&T. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of AT&T.
------------------------------------------------------------------------------*/

#ifndef _ISP_COMMSG_H		/* avoid multiple include problems */
#define _ISP_COMMSG_H

#define	COM_MSG_VERSION		1
#define COM_MSG_BASE		0

/* Mnemonic name definitions for common messages */

#define COM_ISPDEBUG		0
#define COM_FENTER		3
#define	COM_ILOGSTART		7
#define	COM_ILOGEXIT		8 /* used only once */
#define COM_UTLFILE_MALLOC 	27
#define COM_UTLFILE_OPEN	28 

/* General Module messages. */
#define COM_ERR_PARM_RANGE	31  
#define COM_GENSTR		32  
#define COM_ERR_PARMNUM		33 
#define COM_ERR_PARMSTR		34 

/* General Diagnose Module messages. */
#define COM_DIA_MODFAIL		35
#define COM_DIA_MODSUCCEED	36
#define COM_DIA_SUCCEEDNUM	37
#define COM_DIA_SUCCEEDSTR	38

#define COM_DIA_MODENTER	41

#define COM_ERR_CEVTRAP 	69
#define COM_ERR_NOTINIT		70
#define COM_ERR_STRTSPOS 	71
#define COM_ERR_FILEREMOVE	72 /* used */
#define COM_ERR_FILEACCESS	73
#define COM_ERR_FILEEXISTS	74
#define COM_ERR_NOFILE		75
#define COM_ERR_COPY		76
#define COM_ERR_CEVDATA		77
#define COM_UTLLLMGR_ERR	78
#define COM_MOD_FAIL		80 /* Anywhere this is used - it doesn't give enough info gpw */

/* Indxary */
#define COM_ESTATFAILED		81
#define COM_EFILSIZLIM		82
#define COM_EEMPTYBTREE		83
#define COM_EDUPKEYIGNORE	84
#define COM_EINSERTNODE		85
#define COM_EOPENDATFIL		86
#define COM_EKEYFIELD		87
#define COM_EKEYFNDNOFIELD	88
#define COM_EFILEMPTY		89

#define COM_SNMP_ENABLE		90
#define COM_SNMP_TRAP_ID	91
#define COM_SNMP_UNSET		92


#endif	/* _ISP_COMMSG_H */


