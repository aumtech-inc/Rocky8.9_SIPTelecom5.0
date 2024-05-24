/*------------------------------------------------------------------------------
File:		ocmNTUtil.c
Purpose:	Contains all utility routines that provide interfaces to the
			ARC Outbound Call Management processes and APIs for network transfer.
Author:		Aumtech, Inc.
Update:		09/06/12 djb	Created the file
------------------------------------------------------------------------------*/
#include "ocmHeaders.h"
#include "ocmNTHeaders.h"
#include <string.h>

extern int mkOcsDir(char *dir, char *failureReason);
extern int editAndValidateTime(char *callTime, char *reason);
extern int mkdirp(char *zDir, int perms);
extern int createDirIfNotPresent(char *zDir);
extern int getDefaultISPBase(char *baseDirBuffer);

static int checkNTOcsDirs(char *failureReason);

/*------------------------------------------------------------------------------
writeNTCdfInfo():	Write the information in the OCM_CDS structure to the CDF
		specified in the cdf member of the structure.
Input
	cds->cdf	CDF name should be provided by the user
	dir		What dir should we look in? 
Output
	All other members of the CDS structure.
Return Values:
0 	Wrote all information in the file successfully.
-1	Failed to write information to the CDF. failureReason contains a string
	indicating the reason why the write failed.
------------------------------------------------------------------------------*/
int writeNTCdfInfo(const NT_OCM_CDS *cds, char *dir, char *failureReason)
{
	FILE	*fp;
	char	*ispbase, cdfPath[256], zeroFilePath[256];
	char	baseDirBuffer[256];

	/* Do we at least have a CDF name */
	if(cds->cdf[0] == '\0')
	{
		sprintf(failureReason, "%s", "CDF is NULL");
		return(-1);
	}

	/* Do we have a dir name */
	if(dir[0] == '\0')
	{
		sprintf(failureReason, "%s", "Dir is NULL");
		return(-1);
	}

	/* Make sure we have all the dirs we need */
	if(checkNTOcsDirs(failureReason) < 0)
	{
		return(-1);
	}

	/*
	 * Get the ISPBASE env. var. value.
	 */
	if((ispbase = (char *)getenv("ISPBASE")) == (char *)NULL)
	{
		getDefaultISPBase(baseDirBuffer);

		if(baseDirBuffer[0] == '\0')
		{
			sprintf(failureReason, "Env. var. ISPBASE not set.");
			return(-1);
		}

		ispbase = baseDirBuffer;
	}

	/* Formulate the fullPath of the CDF and the zeroFilePath */
	sprintf(cdfPath, 	"%s/%s/%s", 	ispbase, dir, cds->cdf);
//	sprintf(zeroFilePath, 	"%s/%s/0.%s", 	ispbase, dir, cds->cdf);

	/* All is well. Now write the structure into the file */
	if((fp = fopen(cdfPath, "w")) == NULL)
	{
		sprintf(failureReason,
			"Failed to open CDF file (%s) for writing, [%d: %s]",
			cdfPath, errno, strerror(errno));
		return(-1);
	}

	fprintf(fp, "%s=%s\n",	CDV_NRINGS,		cds->nrings);
	fprintf(fp, "%s=%s\n",	CDV_PHONE,		cds->phone);
	fprintf(fp, "%s=%s\n",	CDV_INFORMAT,		cds->informat);
	fprintf(fp, "%s=%s\n",	CDV_TRIES,		cds->tries);
	fprintf(fp, "%s=%s\n",	CDV_CALLTIME,		cds->callTime);
	fprintf(fp, "%s=%s\n",	CDV_NEWAPP,		cds->newApp);
	fprintf(fp, "%s=%s\n",	CDV_APP2APPINFO,	cds->appToAppInfo);
	fprintf(fp, "%s=%s\n",	CDV_REQUESTER_PID,	cds->requesterPid);
	fprintf(fp, "%s=%s\n",	CDV_TRUNK_GROUP,	cds->trunkGroup);

	fflush(fp);
	fclose(fp);

#if 0
		// djb - don't need the zero file.
	/*
	 * Now, create the trigger file for this CDF. This is just an empty 
	 * file.
	 */
	if((fp = fopen(zeroFilePath, "w")) == NULL)
	{
		sprintf(failureReason,
			"Failed to open CDF trigger file (%s) for writing, "
			"[%d: %s]",
			cdfPath, errno, strerror(errno));
		return(-1);
	}
	fclose(fp);
#endif

	return(0);
} /* END: writeNTCdfInfo() */

/*------------------------------------------------------------------------------
checkNTOcsDirs():	Check if all necessary directories used by the OCM processes
		and APIs exists. If they don't create them.

Return Values:

0 	All directories exist, are readable, writeable and executable.
-1	Some or all directories do not exist and we could not create them.
	failureReason contains a string indicating the reason why create failed
------------------------------------------------------------------------------*/
static int checkNTOcsDirs(char *failureReason)
{
	char	*ispbase;
	char	dir[256];
	char	baseDirBuffer[256];

	/*
	 * Get the ISPBASE env. var. value.
	 */
	if((ispbase = (char *)getenv("ISPBASE")) == (char *)NULL)
	{
		getDefaultISPBase(baseDirBuffer);

		if(baseDirBuffer[0] == '\0')
		{
			sprintf(failureReason, "Env. var. ISPBASE not set.");
			return(-1);
		}

		ispbase = baseDirBuffer;
	}

	sprintf(dir, "%s/%s", ispbase, NT_OCS_WORK_DIR);

	if(mkOcsDir(dir, failureReason) < 0)
	{
		return(-1);
	}

	sprintf(dir, "%s/%s", ispbase, NT_OCS_ERR_DIR);
	if(mkOcsDir(dir, failureReason) < 0)
	{
		return(-1);
	}
	sprintf(dir, "%s/%s", ispbase, NT_OCS_CALLED_DIR);
	if(mkOcsDir(dir, failureReason) < 0)
	{
		return(-1);
	}
	sprintf(dir, "%s/%s", ispbase, NT_OCS_LOCK_DIR);
	if(mkOcsDir(dir, failureReason) < 0)
	{
		return(-1);
	}

    //inprogress folder
    sprintf(dir, "%s/%s", ispbase, NT_OCS_INPROGRESS_DIR);
    if(mkOcsDir(dir, failureReason) < 0)
    {
        return(-1);
    }

	return(0);
} /* END: checkNTOcsDirs() */

/*-----------------------------------------------------------------------------
schedNTCall(): Schedule a network transfer outbound call at a specific time.
-----------------------------------------------------------------------------*/
int schedNTCall(
	int 	nrings,		/* # of rings before the call is 	*/
				/* declared a failure 			*/
	int 	tries,		/* # of tries before the call is 	*/
				/* considered a complete failure	*/
	int 	informat,	/* NAMERICAN / NONE / IP / H323 / SIP */
	char 	*phoneNumber,	/* Phone number to call			*/
	char 	*callTime,	/* Time to call in :			*/
				/* YYMMDDHHMMSS, HHMMSS, HHMM, HH	*/
	char 	*newApp,	/* New app. to fire 			*/
	char	*appToAppInfo,	/* Any info that is passed to newApp	*/
	char	*cdf,		/* Call definition file for this call	*/
	char	*trunkGroup,		/* Usage field */
	char	*reason		/* Reason for failure			*/
	)
{
	char		failureReason[256];
	char		date_and_time[128];
	NT_OCM_CDS	myCds;
	CDF_NAME	myCdf;

	reason[0] = '\0';
	
	if((nrings < 0) || (nrings > 15))
	{
    		sprintf(reason, "Invalid parameter no_of_rings: %d. "
				"Valid values are 0 to 15.", nrings);
    		return (-1);
	}
	if(tries <= 0)
	{
    		sprintf(reason,"Invalid parameter tries: %d. "
			"Valid values are greater than 0.", tries);
    		return (-1);
	}
	if((informat != NAMERICAN) && (informat != NONE) &&
		(informat != IP) && (informat != H323) && (informat != SIP))
	{
    		sprintf(reason,"Invalid parameter informat: %d."
			"Valid values are NAMERICAN, NONE, IP, H323, SIP.", informat);
    		return (-1);
	}
	if(phoneNumber[0] == '\0')
	{
    		sprintf(reason,"Invalid parameter phone_number: NULL.");
    		return (-1);
	}
	if(callTime[0] == '\0')
	{
    		sprintf(reason,"Invalid parameter call_time: NULL.");
    		return (-1);
	}
	if(newApp[0] == '\0')
	{
    		sprintf(reason,"Invalid parameter calling_app: NULL.");
    		return (-1);
	}
	if((int)strlen(appToAppInfo) > MAX_APP2APPINFO)
	{
    		sprintf(reason,"App to app information length (%d) must be "
				" < %d.",
				(int)strlen(appToAppInfo),MAX_APP2APPINFO);
    		return (-1);
	}
	strcpy(date_and_time, callTime);
	if(editAndValidateTime(date_and_time, reason) < 0)
	{
		return(-1);	/* Message written in sub-routine */
	}

	/* All parameters are OK */
	memset(&myCds, 		0, sizeof(OCM_CDS));
	memset(&myCdf, 		0, sizeof(CDF_NAME));
	memset(failureReason, 	0, sizeof(failureReason));

	if(makeCdfName(&myCdf, failureReason) < 0)
	{
    		sprintf(reason,
			"Failed to generate call definition filename <%s>.",
			failureReason);
    		return (-1);
	}

	memset(failureReason, 	0, sizeof(failureReason));

	sprintf(myCds.cdf, 		"%s", myCdf.cdf);
	sprintf(myCds.nrings,		"%d", nrings);
	sprintf(myCds.phone,		"%s", phoneNumber);
	sprintf(myCds.informat,		"%d", informat);
	sprintf(myCds.tries,		"%d", tries);
	sprintf(myCds.callTime,		"%s", date_and_time);
	sprintf(myCds.newApp,		"%s", newApp);
	sprintf(myCds.appToAppInfo, 	"%s", appToAppInfo);
	sprintf(myCds.requesterPid, 	"%d", getpid());
	sprintf(myCds.trunkGroup, 	"%s", trunkGroup);

	if(writeNTCdfInfo(&myCds, NT_OCS_WORK_DIR, failureReason) < 0)
	{
    		sprintf(reason, "Failed to schedule outbound call. [%s]",
							failureReason);
    		return (-1);
	}

	/* Return the filename to the user */
	sprintf(cdf, "%s", myCds.cdf);

	return(TEL_SUCCESS);
} /* END: schedNTCall() */

