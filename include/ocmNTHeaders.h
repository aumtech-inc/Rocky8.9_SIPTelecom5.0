/*------------------------------------------------------------------------------
File:		ocmNTHeaders.h

Purpose:	Contains all header information used by Network Transfer 
		o The OCS process
		o The TEL_ScheduleCall and the TEL_UnscheduleCall APIs

Author:		Aumtech, Inc.

Update:		09/05/12 djb	Created the file
------------------------------------------------------------------------------*/

#ifndef _ocmNTHeaders_H__			/* Just preventing multiple includes */
#define _ocmNTHeaders_H__

/* Include Files */
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include "Telecom.h"

#define MAX_APP2APPINFO		512

/* Note: All directories are w.r.t $ISPBASE */

#define NT_OCS_WORK_DIR	"Telecom/NT_OCS/work"      /* Contains current CDFs */
#define NT_OCS_ERR_DIR	"Telecom/NT_OCS/errors"	/* Contains failed CDFs	 */
#define NT_OCS_CALLED_DIR	"Telecom/NT_OCS/called"	/* Contains successfull CDFs */
#define NT_OCS_LOCK_DIR	"Telecom/NT_OCS/lock"	/* Contains lock files	*/
#define NT_OCS_INPROGRESS_DIR  "Telecom/NT_OCS/inprogress"    /* Contains inprogress CDFs */

#define CDF_PREFIX	"cdf"		/* Prefix for all CDFs		*/

/* Names used in the name=value pairs in the CDFs */

/* Call Definition Structure */
typedef struct nt_callDefStruct
{
	char	cdf[256];
	char	nrings[3];
	char	informat[10];
	char	phone[256];
	char	callTime[20];
	char	tries[3];
	char	newApp[50];
	char	appToAppInfo[1200];
	char	requesterPid[10];
	char	trunkGroup[50];
} NT_OCM_CDS;

extern int schedNTCall(
	int 	nrings,		/* # of rings before the call is 	*/
				/* declared a failure 			*/
	int 	tries,		/* # of tries before the call is 	*/
				/* considered a complete failure	*/
	int 	informat,	/* NAMERICAN / NONE			*/
	char 	*phoneNumber,	/* Phone number to call			*/
	char 	*callTime,	/* Time to call in :			*/
				/* YYMMDDHHMMSS, HHMMSS, HHMM, HH	*/
	char 	*newApp,	/* New app. to fire 			*/
	char	*appToAppInfo,	/* Any info that is passed to newApp	*/
	char	*cdf,		/* Call definition file for this call	*/
	char	*trunkGroup,		/* Usage field from resourceDefTab */
	char	*reason		/* Reason for failure			*/
	);

#endif /* _ocmNTHeaders_H__ */
