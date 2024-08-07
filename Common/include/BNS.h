#ifndef BNS_DOT_H
#define BNS_DOT_H


/*----------------------------------------------------------------------+
 |				AT&T Network Application Systems        |
 |	Copyright 1992     	5040 Sadler Road Suite 200      	|
 |				Glen Allen, VA. 23060			|
 |				804 270-1185				|
 |									|
 | This source code is proprietary and confidential.			|
 |									|
 +----------------------------------------------------------------------+
 |			      B N S . h					|
 +----------------------------------------------------------------------+
 |			    Definitions of				|
 |		       Shared Memory Sizes     				|
 |		      Definition of Message Structure                   |
 +----------------------------------------------------------------------*/

/* static char *scss=
 | "BNS.h 1.0 \
 | created   07/01/92 15:12:19 \
 | retrieved 1/25/92 00:49:28";
 |
 | static char *whatstr="@(#)BNS.h	1.0 - 92/07/01 15:12:19	Copyright 1991 AT&T";
Update	02/26/97 M. Bhide added REQUEST_PORT_23
Update	04/19/97 sja	Added REQUEST_OVERLAY
Update	04/28/97 sja	Changed MQUE_CLEANUP_TIME to 300 (from 600)
Update	09/19/98 mpb	Changed MAX_SLOT from 100 to 125 to 130
Update	05/03/99 mpb	Changed MAX_SLOT from 130 to 200
Update	02/22/00 apj	included libgen.h	
Update	04/05/06 mpb	Changed MAX_SLOT 300
Update	02/16/07 mpb	Changed MAX_SLOT 400
 */

#ident "@(#)BNS.h	1.0 - 92/07/01	Copyright 1992 Aumtech, Inc."


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <memory.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <libgen.h>

#define LogMsg(a,b,c) 	log_msg(__LINE__, a, b, c)
#define	MINUTE		60		/* # of seconds / minute */
#define	BUFSIZE		256		/* tmp buffer size */
#define	MSIZE		256		/* size of the message */
#define MAX_SLOT	1000	/* Maxium slot in shared memory */
#define NAP_TIME	5		/* time responsibility sleep's in 
						watch dog mode */
#define MQUE_CLEANUP_TIME	300

static int tran_shmid;	/* id for the shared memory table */
static int resp_mqid;	/* id for the communication message queue */

static	void	*tran_tabl_ptr; /* pointers to the shared memory table */

typedef struct {			/* message structure */
	long	mesg_type;		/* message type */
	char	mesg_data[MSIZE];	/* message text */
} Mesg;

static	Mesg	mesg;

/* following are commnad use on dynamic manager message queue */
#define REQUEST_OVERLAY		5
#define REQUEST_APPLICATION	6
#define REQUEST_PORT		8
#define RELEASE_PORT		7
#define REQUEST_PORT_23		9
#define RSM_REQUEST_PORT	4
#define RSM_RELEASE_PORT	3
/*********************************** eof() *******************************/
#endif  //  BNS_DOT_H
