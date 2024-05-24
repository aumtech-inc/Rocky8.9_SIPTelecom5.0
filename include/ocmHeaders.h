/*------------------------------------------------------------------------------
File:		ocmHeaders.h

Purpose:	Contains all header information used by 
		o The OCS process
		o The TEL_ScheduleCall and the TEL_UnscheduleCall APIs

Author:		Sandeep Agate

Update:		06/24/97 sja	Created the file
Update:		08/15/97 sja	Removed port from struct cdfNameStruct
Update:		08/15/97 sja	Removed port from schedCall() fn. prototype
Update:		08/18/97 sja	Added MAX_APP2APPINFO
Update:	2001/01/31 sja	Increased size of pid and time members in structs
Update:	2001/01/31 sja	Added #include <sys/time.h>
Update:	2001/02/15 sja	Removed sys_errlist[] 
Update:	2003/01/24 sja	Added statusUrl to OCM_ASYNC_CDS struct
Update:	2003/01/24 sja	Added OCM_CALL_STATUS struct and function prototype.
Update:	2003/01/24 sja	Added OCM_STATUS_FIFO
Update:	2003/02/04 saj	Added callId & initialScript to OCM_ASYNC_CDS struct,
						added CDV_INITIAL_SCRIPT, CDV_CALL_ID, CDV_STATUS_URL
Update:	2003/02/06 sja	Removed OCM_ASYNC_CDS from the prototype of 
						ocmReportCallStatus()
Update:	2003/03/27 saj	Added applicationData to OCM_ASYNC_CDS 
Update: 2005/06/07 djb  mr # 373.  change phone size from 20 to 256.
------------------------------------------------------------------------------*/

#ifndef _ocmHeaders_H__			/* Just preventing multiple includes */
#define _ocmHeaders_H__

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

#define OCS_WORK_DIR	"Telecom/OCS/work"      /* Contains current CDFs */
#define OCS_ERR_DIR	"Telecom/OCS/errors"	/* Contains failed CDFs	 */
#define OCS_CALLED_DIR	"Telecom/OCS/called"	/* Contains successfull CDFs */
#define OCS_LOCK_DIR	"Telecom/OCS/lock"	/* Contains lock files	*/
#define OCS_INPROGRESS_DIR  "Telecom/OCS/inprogress"    /* Contains inprogress CDFs */

#define CDF_PREFIX	"cdf"		/* Prefix for all CDFs		*/

/* Names used in the name=value pairs in the CDFs */

#define CDV_NRINGS				"cdv_nrings"
#define	CDV_PHONE				"cdv_phone"
#define CDV_INFORMAT			"cdv_informat"
#define CDV_TRIES				"cdv_tries"
#define CDV_CALLTIME			"cdv_callTime"
#define CDV_NEWAPP				"cdv_newApp"
#define CDV_APP2APPINFO			"cdv_appToAppInfo"
#define CDV_REQUESTER_PID		"cdv_requesterPid"
#define CDV_RETRY_INTERVAL		"cdv_retryInterval"
#define CDV_TRUNK_GROUP			"cdv_trunkGroup"
#define CDV_PRE_TAG_LIST		"cdv_preTagList"
#define CDV_POST_TAG_LIST		"cdv_postTagList"
#define CDV_OUTBOUND_CALL_PARAM	"cdv_outboundCallParam"
#define CDV_INITIAL_SCRIPT		"cdv_initialScript"
#define CDV_CALL_ID				"cdv_callId"
#define CDV_STATUS_URL			"cdv_statusUrl"
#define CDV_APPLICATION_DATA	"cdv_applicationData"

#define OCM_CALL_STATUS_FIFO	"/tmp/ocmCallStatus.fifo"

/* CDF Name Structure */
typedef struct cdfNameStruct 
{
	char	cdf[256];
	char	prefix[5];
	char	timeStamp[20];
	char	pid[10];
} CDF_NAME;

/* Call Definition Structure */
typedef struct callDefStruct
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
} OCM_CDS;
typedef struct asyncCallDefStruct
{
	char	cdf[256];
	char	nrings[3];
	char	informat[10];
	char	phone[256];
	char	callTime[20];
	char	tries[3];
	char	newApp[50];
	char	applicationData[512];
	char	appToAppInfo[1200];
	char	requesterPid[10];
	char	retryInterval[30];
	char	trunkGroup[50];
	char	preTagList[50];
	char	postTagList[50];
	char	outboundCallParam[256];
	char	statusUrl[256];
	char	reason[256];
	char	callId[50];
	char	initialScript[256];
} OCM_ASYNC_CDS;

typedef struct
{
	int		statusCode;
	char	callId[50];
	char	statusMsg[200];
	char	statusUrl[256];

	/*
	** THE SUM OF THE SIZES OF ALL MEMBERS IN THIS STRUCT MUST BE < 512.
	** THIS IS BECAUSE THIS ENTIRE STRUCT IS READ FROM THE FIFO AND
	** 512 IS THE MAX SIZE OF A GUARANTEED ATOMIC READ()
	**
	** SIZES > 512 MAY NOT BE ATOMIC AND HENCE DATA FROM ONE REQUEST MAY 
	** CORRUPT OTHER REQUESTS ON THE FIFO.
	*/

} OCM_CALL_STATUS;

/* Extern Vars */
extern int	errno;

/* OCM Utility Function Prototypes */
extern int getCdfNameInfo(CDF_NAME *cdfName, char *failureReason);
extern int makeCdfName(CDF_NAME *cdfName, char *failureReason);

extern int readCdfInfo(OCM_CDS *cds, char *dir, char *failureReason);
extern int schedCall(
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
	char	*reason		/* Reason for failure			*/
	);
extern int writeCdfInfo(const OCM_CDS *cds, char *dir, char *failureReason);
extern int ocmLoadCallRequest(OCM_ASYNC_CDS *zCds, char *dir, char *failureReason);
extern int ocmWriteCdfInfo(const OCM_ASYNC_CDS *zCds, char *dir, char *failureReason);
extern int ocmCreateCallRequest(OCM_ASYNC_CDS *zCds);
extern int ocmReportStatus(OCM_CALL_STATUS *zCallStatus, char *zErrorMsg);
extern int ocmCreateCallRequestWithoutStruct(
	char	*zRings,
	char	*zPhoneFormat,
	char	*zPhoneNumber,
	char	*zCallTime,
	char	*zTries,
	char	*zApplicationName,
	char	*zApp2AppInfo,
	char	*zRetryInterval,
	char	*zTrunkGroup,
	char	*zPreTagList,
	char	*zPostTagList,
	char	*zOutboundCallParam,
	char	*zStatusUrl,
	char	*zCallId,
	char	*zInitialScript,
	char	*zErrorString
	);
int ocmReportStatusWithoutStruct(
	int		zStatusCode,
	char	*zCallId,
	char	*zStatusMsg,
	char	*zStatusUrl,
	char	*zErrorString
	);

#endif /* _ocmHeaders_H__ */
