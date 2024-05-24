/*-----------------------------------------------------------------------------
 * CDRmsg.h -- ISP common message mnemonic name definitions 
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
 * 3/25/94	J. Huang		file created
 * 5/26/94	R. Schmidt		Added new messages
 * 8/12/02	MPB	Added CDR_CUSTOMSERVICE_MSG
 *
 * PURPOSE
 *
 *	This file provides :
 *	version number, base number and mnemonic name definitions 
 * 	for messages which are common across all objects.
 *-----------------------------------------------------------------------------
 */

#ifndef _ISP_CDRMSG_H		/* avoid multiple include problems */
#define _ISP_CDRMSG_H

#define	CDR_MSG_VERSION		1
#define CDR_MSG_BASE		20000

/* Mnemonic name definitions for common messages */

#define CDR_STARTCALL_MSG	20010
#define CDR_ENDCALL_MSG		20011
#define CDR_STARTSERVICE_MSG	20020
#define CDR_ENDSERVICE_MSG	20021
#define CDR_CUSTOMSERVICE_MSG	20022
#define CDR_STARTNET_MSG	20030
#define CDR_ENDNET_MSG		20031
#define CDR_STARTDIVERSION_MSG	20040
#define CDR_ENDDIVERSION_MSG	20041
#define CDR_USERBILLING_MSG	20050

#endif	/* _ISP_CDRMSG_H */
