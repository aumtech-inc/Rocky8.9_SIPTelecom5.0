/*-----------------------------------------------------------------------------
File:		ResStat.h
Purpose:	This file defines constants and structures required by the
		isp_monitor.c program. 
		It looks like this file is defining constants, etc. that 
		are probably defined elsewhere and therefore this file is
		trying to stay in sync with .h files that may be changed
		without regard to the contents of this file. Whoever created
		this file did not bother to put a header on it, so we really
		don't know why this file was thought necessary or how 
		dangerous it may be.  This should be investigated. Time does
		not permit this investigation now (8/24/99) since we are trying
		to get the Telecom 2.9 release out. -gpw 
Author:		Unknown
Date:		Unknown
-----------------------------------------------------------------------------*/

#include <sys/ipc.h>
#include <sys/msg.h>

/* Message data size */
#define MSIZE	256

#define QUE_ID          6567L				/* Message queue */
#define PERMS           0666				/* permission */

/* Message data size */
#define MSIZE	256

#define QUE_ID          6567L				/* Message queue */
#define PERMS           0666				/* permission */

#define	GET_FILENAME	4

/* Message queue data structure */
typedef struct 	{
        	long    type;			/* message type */
        	char    data[MSIZE];  		/* message data */
		} Msg;

int     que_id;                                 /* message queue id  */

#define	TTS		1
#define	FAX		2
#define	SPEECH_REC	3
#define	SEND		9
