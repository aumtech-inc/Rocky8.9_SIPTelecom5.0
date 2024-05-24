/*------------------------------------------------------------------------------
File:		TEL_ScheduleCall.c
Purpose:	Schedule an outbound call to be placed at a specific time
Author:		Sandeep Agate
Date:		Unknown, author didn't specify
Update:		03/08/01 gpw updated for IP IVR
-----------------------------------------------------------------------------
Copyright (c) 2001, Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "ocmHeaders.h"

static char	ModuleName[] = "TEL_ScheduleCall";

/*-----------------------------------------------------------------------------
TEL_ScheduleCall(): Schedule an outbound call at a specific time.
-----------------------------------------------------------------------------*/
int TEL_ScheduleCall(
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
	char	*cdf		/* Call definition file for this call	*/
	)
{
        static char mod[]="TEL_ScheduleCall";
        char apiAndParms[MAX_LENGTH] = "";
	char		failureReason[256];
	OCM_CDS		myCds;
	CDF_NAME	myCdf;
	char		reason[256];
	int		rc;

	memset(reason, 0, sizeof(reason));


	sprintf(apiAndParms,"%s(%d,%d,%s,%s,%s,%s,%s,CDF)", ModuleName,
				nrings, tries, arc_val_tostr(informat),
				phoneNumber, callTime, newApp, appToAppInfo);

        rc = apiInit(mod, TEL_SCHEDULECALL, apiAndParms, 1, 0, 0);
        if (rc != 0) HANDLE_RETURN(rc);

	/*
	 * This routine contains all the intelligence to schedule a call 
	 * based on the information passed. I have kept this independent of 
	 * Telecom Services so that programs such as CGIs may use this in the
	 * future to schedule calls through the WWW.
	 *
	 * schedCall() is defined in ocmUtil.c
	 *
	 * sja - 97/07/30
	 */
	rc = schedCall(nrings, tries, informat,phoneNumber,callTime,
						newApp,appToAppInfo,cdf,reason);
	if(rc < 0)
	{
    		//tel_log(ModuleName,REPORT_NORMAL, 
				//TEL_SCHEDULE_CALL_FAILED, reason);

                telVarLog(mod,REPORT_NORMAL, TEL_SCHEDULE_CALL_FAILED, 
			GV_Err, reason);
    		HANDLE_RETURN (TEL_FAILURE);
	}

	/* gpw - is this useful? Why do we need it? */
	send_to_monitor(MON_DATA, 0, cdf);

	HANDLE_RETURN(TEL_SUCCESS);
} /* END: TEL_ScheduleCall() */
