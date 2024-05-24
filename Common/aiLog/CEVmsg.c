/*-----------------------------------------------------------------------------
 * CEVmsg.c -- ISP Customer Call Event message definitions 
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
 * Last Modified By : 
 * Last Modified On : 
 * Update Count	    : 
 * Status	    : 
 * 
 * HISTORY
 * 4/26/94	J. Huang	file created
 *
 * PURPOSE
 *
 *      This file provides :
 *      - Message body format definitions for ISP call event messages. 
 *	  The index of a message definition is the numeric value of its
 *	  mnemonic defined in CEVmsg.h.
 *	- initialize CEV_msg_num, the total number of messages which are 
 *	  currently defined.
 *-----------------------------------------------------------------------------
 */


/* CAUTION:
 * The message index must match to the numeric value of its mnemonic 
 * defined in CEVmsg.h
 */

char *	GV_CEVmsg[] = {
"%s",						/* CEV_MESSAGE */
};

int	GV_CEV_msgnum = { sizeof( GV_CEVmsg ) / sizeof( GV_CEVmsg[0] ) };
