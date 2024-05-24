/*-----------------------------------------------------------------------------
 * CDRmsg.c -- ISP Call Detail Record message definitions 
 *
 * Copyright (c) 1994, AT&T Network Systems Information Services Platform (ISP)
 * All Rights Reserved.
 *
 * This is an unpublished work of AT&T ISP which is protected under 
 * the copyright laws of the United States and a trade secret
 * of AT&T. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of AT&T.
 *
 * ISPID            : 
 * Author	    : J.Huang
 * Created On	    : 4/26/94
 * Last Modified By : R. Schmidt
 * Last Modified On : 8/31/94
 * Update Count	    : 
 * Status	    : 
 * 
 * HISTORY
 * 4/26/94	J. Huang	file created
 * 5/26/94	R. Schmidt	Added messages.
 * 8/31/94	R. Schmidt	Changed STARTCALL mesg. to have 8 strings.
 *				Changed USERBILLING mesg. to have 8 strings.
 * 8/12/02	MPB	Added CDR_CUSTOMSERVICE_MSG.
 *
 * PURPOSE
 *
 *      This file provides :
 *      - Message body format definitions for ISP call detail messages. 
 *	  The index of a message definition is the numeric value of its
 *	  mnemonic defined in CDRmsg.h.
 *	- initialize CDR_msg_num, the total number of messages which are 
 *	  currently defined.
 *-----------------------------------------------------------------------------
 */


/* CAUTION:
 * The message index must match to the numeric value of its mnemonic 
 * defined in CDRmsg.h
 */

char *	GV_CDRmsg[] = {
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"%s:%s:%s:%s:%s:%s:%s:%s",	/* CDR_STARTCALL_MSG	- 20010*/
"%s:%s:%s:%s:%s:%s",		/* CDR_ENDCALL_MSG	- 20011*/
"",
"",
"",
"",
"",
"",
"",
"",
"%s:%s:%s:%s:%s:%s:%s",		/* CDR_STARTSERVICE_MSG	- 20020*/
"%s:%s:%s:%s:%s:%s:%s:%s",	/* CDR_ENDSERVICE_MSG	- 20021*/
"%s:%s:%s:%s:%s:%s:%s",	/* CDR_CUSTOMSERVICE_MSG	- 20022*/
"",
"",
"",
"",
"",
"",
"",
"%s:%s:%s:%s:%s:%s:%s",		/* CDR_STARTNET_MSG	- 20030*/
"%s:%s:%s:%s:%s:%s:%s:%s",	/* CDR_ENDNET_MSG	- 20031*/
"",
"",
"",
"",
"",
"",
"",
"",
"%s:%s:%s:%s:%s:%s:%s",		/* CDR_STARTDIVERSION_MSG - 20040*/
"%s:%s:%s:%s:%s:%s",		/* CDR_ENDDIVERSION_MSG	- 20041*/
"",
"",
"",
"",
"",
"",
"",
"",
"%s:%s:%s:%s:%s:%s:%s:%s",	/* CDR_USERBILLING_MSG	- 20050*/
};

int	GV_CDR_msgnum = { sizeof( GV_CDRmsg ) / sizeof( GV_CDRmsg[0] ) };
