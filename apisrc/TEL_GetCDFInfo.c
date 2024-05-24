#ident	"@(#)TEL_GetCDFInfo 97/06/24 2.2.0 Copyright 1996 Aumtech, Inc."
/*------------------------------------------------------------------------------
File:		TEL_GetCDFInfo.c

Purpose:	Get call definition information based on a CDF name

Author:		Sandeep Agate

Update:		04/04/01 APJ Updated for IP-IVR.
-----------------------------------------------------------------------------
Copyright (c) 1996, Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "ocmHeaders.h"

static char	ModuleName[] = "TEL_GetCDFInfo";

/*-----------------------------------------------------------------------------
TEL_GetCDFInfo(): Get call definition information based on a CDF name
-----------------------------------------------------------------------------*/
int TEL_GetCDFInfo(
	char	*cdf,		/* Call definition file for this call	*/
	int 	*nrings,	/* # of rings before the call is 	*/
				/* declared a failure 			*/
	int 	*tries,		/* # of tries before the call is 	*/
				/* considered a complete failure	*/
	int 	*informat,	/* NAMERICAN / NONE			*/
	char 	*phoneNumber,	/* Phone number to call			*/
	char 	*callTime	/* Time to call in :			*/
				/* YYMMDDHHMMSS, HHMMSS, HHMM, HH	*/
		  )
{
	OCM_CDS		mycds;
	char		failureReason[256];
	char		cdfPath[256];
	char		*ispbase;
        char 		apiAndParms[MAX_LENGTH] = "";
	int		rc;

	sprintf(apiAndParms,"%s(%s,%s,%s,%s,%s,%s)", ModuleName,
				cdf,"&nrings","&tries","&informat",
				"phone_number", "call_time");

        rc = apiInit(ModuleName, TEL_GETCDFINFO, apiAndParms, 1, 0, 0);
        if (rc != 0) HANDLE_RETURN(rc);

	if(cdf[0] == '\0')
	{
		sprintf(Msg,"CDF file name is an empty string. Returning failure.");
       		telVarLog(ModuleName, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
       			Msg);
    		HANDLE_RETURN (-1);
	}

	if((ispbase = getenv("ISPBASE")) == NULL)
	{
                telVarLog(ModuleName, REPORT_NORMAL, TEL_CANT_GET_ISPBASE, 
				GV_Err, TEL_CANT_GET_ISPBASE_MSG);
    		HANDLE_RETURN (-1);
	}

	sprintf(cdfPath, "%s/%s/%s", ispbase, OCS_CALLED_DIR, cdf);
	if(access(cdfPath, F_OK) == -1)
	{
                telVarLog(ModuleName, REPORT_NORMAL, TEL_ECDFNOTFOUND, 
				GV_Err, TEL_ECDFNOTFOUND_MSG, cdfPath);
		HANDLE_RETURN(TEL_CDF_NOT_FOUND);
	}

	memset(&mycds, 0, sizeof(OCM_CDS));
	memset(failureReason, 0, sizeof(failureReason));

	sprintf(mycds.cdf, "%s", cdf);
	if(readCdfInfo(&mycds, OCS_CALLED_DIR, failureReason) < 0)
	{
                telVarLog(ModuleName, REPORT_NORMAL, TEL_ECDFINFO, 
			GV_Err, TEL_ECDFINFO_MSG, failureReason);
    		HANDLE_RETURN (-1);
	}

	*nrings 	= atoi(mycds.nrings);
	*tries 		= atoi(mycds.tries);
	*informat 	= atoi(mycds.informat);

	sprintf(phoneNumber, 	"%s",	mycds.phone);
	sprintf(callTime, 	"%s",	mycds.callTime);

	sprintf(apiAndParms,"nrings=%d, tries=%d, informat=%d, phone=%s, "
		"time=%s;" ,*nrings, *tries, *informat, phoneNumber,
		callTime);
	send_to_monitor(MON_DATA, 0, apiAndParms);

	HANDLE_RETURN(TEL_SUCCESS);
} /* END: TEL_GetCDFInfo() */
