/*-----------------------------------------------------------------------------
 * LOGmsg.h -- ISP common logger message mnemonic name definitions 
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
 * Author	    : 
 * Created On	    : 
 * Last Modified By : 
 * Last Modified On : 
 * Update Count	    : 
 * Status	    : 
 * 
 * HISTORY
 * 4/1/94	J. Huang		file created
 *
 * PURPOSE
 *
 *	This file provides :
 *	version number, base number and mnemonic name definitions 
 * 	for LOG object.
 *-----------------------------------------------------------------------------
 */

#ifndef _ISP_LOGMSG_H		/* avoid multiple include problems */
#define _ISP_LOGMSG_H

#define	LOG_MSG_VERSION		1
#define LOG_MSG_BASE		9000

/* Mnemonic name definitions for common messages */

#define	LOG_EREPORT_MODE		9000
#define	LOG_EMESSAGE_ID			9001
#define	LOG_ELM_SELECT			9002
#define	LOG_TEST_MIN			9997
#define	LOG_TEST_MAJ			9998
#define	LOG_TEST_CRI			9999

#endif	/* _ISP_LOGMSG_H */
