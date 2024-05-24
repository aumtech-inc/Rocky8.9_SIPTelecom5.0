/*------------------------------------------------------------------------------
File:		ocmUtil.c
Purpose:	Contains all utility routines that provide interfaces to the
		ARC Outbound Call Management processes and APIs.
Author:		Sandeep Agate
Update:		06/24/97 sja	Created the file
Update:		08/15/97 sja	Removed port references from all routines
Update:		08/18/97 sja	Increased line & rhs buffer sizes
Update:		08/18/97 sja	If fopen fails, return(-1)
Update:         08/19/98 ABK    chgd validateTime to editAndValidateTime (Y2K)
Update:         08/19/98 ABK    chgd editAndValidateTime now returns updted dat
Update:         10/23/98 gpw    chgd editAndValidateTime correctly this time
Update:         12/13/99 gpw    changed %d to %s in Phone Number error msg.
Update: 2001/01/31 sja	Added milliseconds to cdfName in makeCdfName
Update: 2001/02/15 sja	Replaced NULL with '\0' wherever relevant - for Linux
Update: 2001/02/15 sja	Added sequence number to CDF file name
Update: 2001/02/16 sja	Replaced sys_errlist with strerror()
Update: 2001/02/21 sja	Added Madhav's routine to get ISPBASE 
Update: 2001/04/04 apj	Allow IP as one of the informats.
Update: 2001/05/14 apj	Add valid ranges to parameter validation log message.
Update: 2003/01/24 sja	Added the ocmReportStatus() routine
Update: 2003/02/04 saj	Added callId, initialScript & statusUrl in 
						ocmLoadCallRequest & ocmCreateCallRequest.
Update: 2003/03/27 saj	Added applicationData in ocmLoadCallRequest & 
						ocmCreateCallRequest 
Update: 2003/10/08 apj	Allow H323 and SIP as informats.
Update: 2005/09/02 djb	Allow H323 and SIP as informats in 
						ocmCreateCallRequest().
Update: 2006/12/14 djb	Removed DDNDEBUG statement.
------------------------------------------------------------------------------*/
#include "ocmHeaders.h"
#include <string.h>
#include <ctype.h>
static int checkOcsDirs(char *failureReason);
int mkOcsDir(char *dir, char *failureReason);
int editAndValidateTime(char *callTime, char *failureReason);
static int mkdirp(char *zDir, int perms);
static int createDirIfNotPresent(char *zDir);
static int getDefaultISPBase(char *baseDirBuffer);

/*------------------------------------------------------------------------------
writeCdfInfo():	Write the information in the OCM_CDS structure to the CDF
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
int writeCdfInfo(const OCM_CDS *cds, char *dir, char *failureReason)
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
	if(checkOcsDirs(failureReason) < 0)
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
	sprintf(zeroFilePath, 	"%s/%s/0.%s", 	ispbase, dir, cds->cdf);

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

	fflush(fp);
	fclose(fp);

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

	return(0);
} /* END: writeCdfInfo() */
/*------------------------------------------------------------------------------
readCdfInfo():	Read the information into the OCM_CDS structure from the CDF
		specified in the cdf member of the structure.

Return Values:

0 	Read all information in the file successfully.
-1	Failed to read information to the CDF. failureReason contains a string
	indicating the reason why the read failed.
------------------------------------------------------------------------------*/
int readCdfInfo(OCM_CDS *cds, char *dir, char *failureReason)
{
	char	saveCdf[256], cdfPath[256];
	char	line[2048], lhs[25], rhs[1200], *ptr;
	char	*ispbase;
	FILE	*fp;
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

	memset(saveCdf, '\0', sizeof(saveCdf));
	sprintf(saveCdf, "%s", cds->cdf);

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

	memset(cdfPath, 0, sizeof(cdfPath));
	sprintf(cdfPath, "%s/%s/%s", ispbase, dir, saveCdf);

	/* Clear the structure */
	memset(cds, 0, sizeof(OCM_CDS));

	/* Restore the CDF that we just wiped out */
	sprintf(cds->cdf, "%s", saveCdf);

	if((fp = fopen(cdfPath, "r")) == NULL)
	{
		sprintf(failureReason,
			"Failed to open CDF file <%s> for reading, "
			"errno=%d [%s]",
			cdfPath, errno, strerror(errno));
		return(-1);
	}
	while(fgets(line, sizeof(line)-1, fp))
	{
		/* Ignore lines that don't have a '=' in them */
		if(! strchr(line, '='))
			continue;

		memset(lhs, 0, sizeof(lhs));
		memset(rhs, 0, sizeof(rhs));

		ptr = line;

		/* Even if strtok returns NULL, we don't care. 
	 	 * All it will do is give us a NULL lhs.
	 	 */
		
		sprintf(lhs, "%s", (char *)strtok(ptr, "=\n"));

		/* If LHS is NULL, don't even bother looking at RHS */
		if(lhs[0] == '\0')
		{
			continue;
		}

		sprintf(rhs, "%s", (char *)strtok(NULL, "\n"));

		if(strcmp(lhs, CDV_NRINGS) == 0)
		{
			sprintf(cds->nrings, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_PHONE) == 0)
		{
			sprintf(cds->phone, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_INFORMAT) == 0)
		{
			sprintf(cds->informat, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_TRIES) == 0)
		{
			sprintf(cds->tries, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_CALLTIME) == 0)
		{
			sprintf(cds->callTime, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_NEWAPP) == 0)
		{
			sprintf(cds->newApp, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_APP2APPINFO) == 0)
		{
			sprintf(cds->appToAppInfo, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_REQUESTER_PID) == 0)
		{
			sprintf(cds->requesterPid, "%s", rhs);
		}
		else 
		{
			/* We don't recognize the LHS. Just ignore it */
			continue;
		}
	} /* while */

	fclose(fp);
	return(0);
} /* END: readCdfInfo() */
/*------------------------------------------------------------------------------
checkOcsDirs():	Check if all necessary directories used by the OCM processes
		and APIs exists. If they don't create them.

Return Values:

0 	All directories exist, are readable, writeable and executable.
-1	Some or all directories do not exist and we could not create them.
	failureReason contains a string indicating the reason why create failed
------------------------------------------------------------------------------*/
static int checkOcsDirs(char *failureReason)
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

	sprintf(dir, "%s/%s", ispbase, OCS_WORK_DIR);
	if(mkOcsDir(dir, failureReason) < 0)
	{
		return(-1);
	}
	sprintf(dir, "%s/%s", ispbase, OCS_ERR_DIR);
	if(mkOcsDir(dir, failureReason) < 0)
	{
		return(-1);
	}
	sprintf(dir, "%s/%s", ispbase, OCS_CALLED_DIR);
	if(mkOcsDir(dir, failureReason) < 0)
	{
		return(-1);
	}
	sprintf(dir, "%s/%s", ispbase, OCS_LOCK_DIR);
	if(mkOcsDir(dir, failureReason) < 0)
	{
		return(-1);
	}

	return(0);
} /* END: checkOcsDirs() */
/*------------------------------------------------------------------------------
mkOcsDir():	Create an OCS dir if it does not exist.

Return Values:

0 	All directories exist, are readable, writeable and executable.
-1	The directory could not be created.
	failureReason contains a string indicating the reason why create failed
------------------------------------------------------------------------------*/
int mkOcsDir(char *dir, char *failureReason)
{
	int	flags;

	if(access(dir, F_OK) == -1)
	{
		flags = 	S_IWRITE|S_IREAD|S_IEXEC;
		flags |= 	S_IRGRP|S_IROTH;
		flags |=	S_IXGRP|S_IXOTH;

		if(mkdirp(dir, flags) == -1)
		{
			sprintf(failureReason,
					"Cannot make dir <%s>. errno=%d. [%s]",
					dir, errno, strerror(errno));
			return(-1);
		}
	}

#if 0
	if(access(dir, R_OK) == -1)
	{
		sprintf(failureReason,"Directory <%s> is not readable. errno=%d. [%s]",
					dir, errno, strerror(errno));
		return(-1);
	}
	if(access(dir, W_OK) == -1)
	{
		sprintf(failureReason,
			"Directory <%s> is not writable. errno=%d. [%s]",
					dir, errno, strerror(errno));
		return(-1);
	}
	if(access(dir, X_OK) == -1)
	{
		sprintf(failureReason,
			"Directory <%s> is not executable, errno=%d. [%s]",
					dir, errno, strerror(errno));
		return(-1);
	}
#endif /* COMMENT */

	return(0);
} /* END: mkOcsDir() */
/*------------------------------------------------------------------------------
makeCdfName(): 	Generate a CDF name based on the info in the CDF_NAME structure.

Input Required:	None

Output:
	All other members of the CDF_NAME structure.

Return Values:

0 	Generated valid CDF name
-1	Unable to generate valid CDF name. failureReason contains string 
	indicating reason for failure;
------------------------------------------------------------------------------*/
int makeCdfName(CDF_NAME *cdfName, char *failureReason)
{
	static unsigned long	ySequenceNumber = 1;
	time_t		currentTime;
	int		ret_code;
	struct timeval	tp;
	struct timezone	tzp;
	struct tm	*tm;
	char 		current_time[20];
	
	/* Clear the structure */
	memset(cdfName, 0, sizeof(CDF_NAME));

	time(&currentTime);

	if((ret_code = gettimeofday(&tp, &tzp)) == -1)
	{
		sprintf(failureReason, "gettimeofday() failed, [%d: %s].",
			errno, strerror(errno));
		return(-1);
	}

	tm = localtime(&tp.tv_sec);

	strftime(cdfName->timeStamp, sizeof(cdfName->timeStamp)-1, 
		"%Y%m%d%H%M%S",	tm);

	sprintf(cdfName->prefix, 	"%s", 		CDF_PREFIX);
	sprintf(cdfName->pid, 		"%d", 		getpid());

	sprintf(cdfName->cdf,	"%s.%s.%s.%02d.%03ld",
		cdfName->prefix, cdfName->timeStamp, cdfName->pid,
		tp.tv_usec / 10000, ySequenceNumber);

	ySequenceNumber++;

	return(0);
} /* END: makeCdfName() */
/*------------------------------------------------------------------------------
getCdfNameInfo(): Get all information based on the name of a CDF

Input Required:

	cdfName->cdf	Must be supplied by the process using this API.

Output:
	All other members of the CDF_NAME structure.

Return Values:

0 	Got all of the information regarding the CDF name.
-1	Unable to get all information regarding the CDF name.
	failureReason contains string indicating reason for failure.
------------------------------------------------------------------------------*/
int getCdfNameInfo(CDF_NAME *cdfName, char *failureReason)
{
	char	saveName[256];
	char	*ptr;

	if(cdfName->cdf[0] == '\0')
	{
		sprintf(failureReason, "%s",
				"Cannot get CDF name info. NULL cdf.");
		return(-1);
	}

	memset(saveName, 0, sizeof(saveName));
	sprintf(saveName, "%s", cdfName->cdf);

	if(! strchr(saveName, '.'))
	{
		sprintf(failureReason, "Cannot get CDF name info. "
				       "No '.' in cdf name <%s>.",
				       saveName);
		return(-1);
	}

	ptr = saveName;

	/* Even if strtok returns NULL, we don't care. 
	 * All it will do is NULL the member out. That does us no harm.
	 */
	sprintf(cdfName->prefix, 	"%s", (char *)strtok(ptr, "."));
	sprintf(cdfName->timeStamp, 	"%s", (char *)strtok(NULL, "."));
	sprintf(cdfName->pid, 		"%s", (char *)strtok(NULL, "."));
	
	return(0);
} /* END: getCdfNameInfo() */

/*-----------------------------------------------------------------------------
schedCall(): Schedule an outbound call at a specific time.
-----------------------------------------------------------------------------*/
int schedCall(
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
	char	*reason		/* Reason for failure			*/
	)
{
	char		failureReason[256];
	char		date_and_time[128];
	OCM_CDS		myCds;
	CDF_NAME	myCdf;


	reason[0] = '\0';
	
fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	if((nrings < 0) || (nrings > 15))
	{
    		sprintf(reason, "Invalid parameter no_of_rings: %d. "
				"Valid values are 0 to 15.", nrings);
    		return (-1);
	}
fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	if(tries <= 0)
	{
    		sprintf(reason,"Invalid parameter tries: %d. "
			"Valid values are greater than 0.", tries);
    		return (-1);
	}
fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	if((informat != NAMERICAN) && (informat != NONE) &&
		(informat != IP) && (informat != H323) && (informat != SIP))
	{
    		sprintf(reason,"Invalid parameter informat: %d."
			"Valid values are NAMERICAN, NONE, IP, H323, SIP.", informat);
    		return (-1);
	}
fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	if(phoneNumber[0] == '\0')
	{
    		sprintf(reason,"Invalid parameter phone_number: NULL.");
    		return (-1);
	}
fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	if(callTime[0] == '\0')
	{
    		sprintf(reason,"Invalid parameter call_time: NULL.");
    		return (-1);
	}
fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	if(newApp[0] == '\0')
	{
    		sprintf(reason,"Invalid parameter calling_app: NULL.");
    		return (-1);
	}
fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	if((int)strlen(appToAppInfo) > MAX_APP2APPINFO)
	{
    		sprintf(reason,"App to app information length (%d) must be "
				" < %d.",
				(int)strlen(appToAppInfo),MAX_APP2APPINFO);
    		return (-1);
	}
	strcpy(date_and_time, callTime);
fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	if(editAndValidateTime(date_and_time, reason) < 0)
	{
		return(-1);	/* Message written in sub-routine */
	}

	/* All parameters are OK */

fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	memset(&myCds, 		0, sizeof(OCM_CDS));
	memset(&myCdf, 		0, sizeof(CDF_NAME));
	memset(failureReason, 	0, sizeof(failureReason));
fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);

	if(makeCdfName(&myCdf, failureReason) < 0)
	{
    		sprintf(reason,
			"Failed to generate call definition filename <%s>.",
			failureReason);
    		return (-1);
	}

	memset(failureReason, 	0, sizeof(failureReason));

fprintf(stderr, "[%s, %d]\n", __FILE__, __LINE__);
	sprintf(myCds.cdf, 		"%s", myCdf.cdf);
	sprintf(myCds.nrings,		"%d", nrings);
	sprintf(myCds.phone,		"%s", phoneNumber);
	sprintf(myCds.informat,		"%d", informat);
	sprintf(myCds.tries,		"%d", tries);
	sprintf(myCds.callTime,		"%s", date_and_time);
	sprintf(myCds.newApp,		"%s", newApp);
	sprintf(myCds.appToAppInfo, 	"%s", appToAppInfo);
	sprintf(myCds.requesterPid, 	"%d", getpid());

	if(writeCdfInfo(&myCds, OCS_WORK_DIR, failureReason) < 0)
	{
    		sprintf(reason, "Failed to schedule outbound call. [%s]",
							failureReason);
    		return (-1);
	}

	/* Return the filename to the user */
	sprintf(cdf, "%s", myCds.cdf);

	return(TEL_SUCCESS);
} /* END: schedCall() */
/*-----------------------------------------------------------------------------
editAndValidateTime(): Make sure that the time format is correct and valid
	Note: 	This routine edits the parameter that was passed to it and may
		lengthen it.  The calling routine must pass a string which
		is long enough to accommodate the returned string.
Time formats supported: YYYYMMDDHHMMSS, YYMMDDHHMMSS, HHMMSS, HHMM, HH
-----------------------------------------------------------------------------*/
int editAndValidateTime(char *callTime, char *reason)
{
	register int	index;
	char	year[5], month[3], day[3];
	char	hour[3], minute[3], seconds[3];
	short	checkYear, checkMonth, checkDay;
	short	checkHour, checkMinute, checkSeconds;
	char 	yyyymmddhhmmss[30];
	char 	yyyymmdd[30]; 		/* year, mo., day to append to time */
	char 	temp[30];		/* Holds edited date/time value */
	char 	cc[30];			/* Century, e.g. "19" or "20" */
	time_t	tics;
	char valid_formats[]="YYYYMMDDHHMMSS YYMMDDHHMMSS, HHMMSS, HHMM, HH, 0";
	struct tm *yTimePtr;

	reason[0] = '\0';

	checkYear = checkMonth = checkDay = 0;
	checkHour = checkMinute = checkSeconds = 0;

	/* 
	 * memset all vars to NULL. This way we don't have to explicitly add 
	 * a NULL after we strncpy values into these vars.
	 */
	memset(year, 	0, sizeof(year));
	memset(month, 	0, sizeof(month));
	memset(day, 	0, sizeof(day));
	memset(hour, 	0, sizeof(hour));
	memset(minute, 	0, sizeof(minute));
	memset(seconds,	0, sizeof(seconds));

	/* Calculate */
	time(&tics);
	yTimePtr = localtime(&tics);
	strftime(yyyymmddhhmmss, sizeof(yyyymmddhhmmss)-1, "%Y%m%d%H%M%S", 
		yTimePtr);
	strcpy(yyyymmdd, yyyymmddhhmmss);
	strcpy(cc      , yyyymmddhhmmss);
	yyyymmdd[8]='\0';
	cc[2]='\0';

	switch(strlen(callTime))
	{
		case 14:	/* YYYYMMDDHHMMSS */
			checkYear = checkMonth = checkDay = 1;
			checkHour = checkMinute = checkSeconds = 1;
			strncpy(year, 		&callTime[0], 	4);
			strncpy(month, 		&callTime[4], 	2);
			strncpy(day, 		&callTime[6], 	2);
			strncpy(hour, 		&callTime[8], 	2);
			strncpy(minute, 	&callTime[10], 	2);
			strncpy(seconds, 	&callTime[12], 	2);
			sprintf(temp,"%s", callTime);
			break;
		case 12:	/* YYMMDDHHMMSS */
			checkYear = checkMonth = checkDay = 1;
			checkHour = checkMinute = checkSeconds = 1;
			strncpy(year, 		&callTime[0], 	2);
			strncpy(month, 		&callTime[2], 	2);
			strncpy(day, 		&callTime[4], 	2);
			strncpy(hour, 		&callTime[6], 	2);
			strncpy(minute, 	&callTime[8], 	2);
			strncpy(seconds, 	&callTime[10], 	2);
			sprintf(temp,"%s%s", cc, callTime);
			break;
		case 6:		/* HHMMSS */
			checkHour = checkMinute = checkSeconds = 1;
			strncpy(hour, 		&callTime[0], 	2);
			strncpy(minute, 	&callTime[2], 	2);
			strncpy(seconds, 	&callTime[4], 	2);
		 	sprintf(temp, "%s%s", yyyymmdd, callTime);
			break;
		case 4:		/* HHMM */
			checkHour = checkMinute = 1;
			strncpy(hour, 		&callTime[0], 	2);
			strncpy(minute, 	&callTime[2], 	2);
			sprintf(temp, "%s%s00", yyyymmdd, callTime);
			break;
		case 2:		/* HH */
			checkHour = 1;
			strncpy(hour, 		&callTime[0], 	2);
			sprintf(temp,"%s%s0000",yyyymmdd, callTime);
			break;

		case 1:		/* 0 => Call immediately */
			if(callTime[0] != '0')
			{
    				sprintf(reason, "Invalid callTime format (%s), "
					"Valid formats (%s)", callTime,
					valid_formats);
				return(-1);
			}
			strcpy(temp, callTime);
			break;
		default:
    			sprintf(reason, "Invalid callTime format (%s), "
				"Valid formats (%s)", callTime,
				valid_formats);
			return(-1);
	} /* switch */

	if(checkYear)
	{
		for(index=0; index<(int)strlen(year); index++)
		{
			if(!isdigit(year[index]))
			{
    				sprintf(reason, "Invalid year (%s) in callTime.",
									year);
				return(-1);
			}
		}
		/*
		 * We cannot check if year is valid with atoi() because:
		 *
		 * 1. atoi will never return anything < 0
		 * 2. Checking if the year is zero is dangerous because we'll
		 *    be introducing a millenium bug. Think about it!
		 * 3. Thus, we have to assume that whatever the year the user
		 *    sends us is valid.
		 */
	}
	if(checkMonth)
	{
		for(index=0; index<(int)strlen(month); index++)
		{
			if(!isdigit(month[index]))
			{
    				sprintf(reason,"Invalid month (%s) in callTime.",
									month);
				return(-1);
			}
		}
		if(atoi(month) < 1 || atoi(month) > 12)
		{
			sprintf(reason, "Invalid month (%s) in callTime",month);
			return(-1);
		}
	}
	if(checkDay)
	{
		for(index=0; index<(int)strlen(day); index++)
		{
			if(!isdigit(day[index]))
			{
    				sprintf(reason, "Invalid day (%s) in callTime.",
									day);
				return(-1);
			}
		}
		if(atoi(day) < 1 || atoi(day) > 31)
		{
			sprintf(reason, "Invalid day (%s) in callTime.",	day);
			return(-1);
		}
	}
	if(checkHour)
	{
		for(index=0; index<(int)strlen(hour); index++)
		{
			if(!isdigit(hour[index]))
			{
    				sprintf(reason, "Invalid hour (%s) in callTime.",
									hour);
				return(-1);
			}
		}
		/* Hour = 00 to 23 */
		if(atoi(hour) > 23)
		{
			sprintf(reason, "Invalid hour (%s) in callTime.", hour);
			return(-1);
		}
	}
	if(checkMinute)
	{
		for(index=0; index<(int)strlen(minute); index++)
		{
			if(!isdigit(minute[index]))
			{
    				sprintf(reason,
					"Invalid minutes (%s) in callTime.",
					minute);
				return(-1);
			}
		}
		/* Minute = 0 to 59 */
		if(atoi(minute) > 59)
		{
			sprintf(reason, "Invalid minutes (%s) in callTime.",
									minute);
			return(-1);
		}
	}
	if(checkSeconds)
	{
		for(index=0; index<(int)strlen(seconds); index++)
		{
			if(!isdigit(seconds[index]))
			{
    				sprintf(reason,
					"Invalid seconds (%s) in callTime.",
								seconds);
				return(-1);
			}
		}
		/* Seconds = 0 to 59 */
		if(atoi(seconds) > 59)
		{
			sprintf(reason, "Invalid seconds (%s) in callTime.",
								seconds);
			return(-1);
		}
	}
	/* Copy back the temporary date/time to the passed parameter */
	strcpy(callTime, temp);
	/*
	 * NOTE: We are NOT checking for things like day 31 in the month of Feb
	 * 	 etc. Do we want to add these checks?
	 */
	return(0);
} /* END: editAndValidateTime() */

static int mkdirp(char *zDir, int perms)
{
        char *pNextLevel;
        int rc;
        char lBuildDir[512];

        pNextLevel = (char *)strtok(zDir, "/");
        if(pNextLevel == NULL)
                return(-1);

        memset(lBuildDir, 0, sizeof(lBuildDir));
        if(zDir[0] == '/')
                sprintf(lBuildDir, "%s", "/");

        strcat(lBuildDir, pNextLevel);
        for(;;)
        {
                rc = createDirIfNotPresent(lBuildDir);
                if(rc != 0)
                        return(-1);

                if ((pNextLevel = (char *)strtok(NULL, "/")) == (char *)NULL)
                {
                        break;
                }
                strcat(lBuildDir, "/");
                strcat(lBuildDir, pNextLevel);
        }

        return(0);
}

static int createDirIfNotPresent(char *zDir)
{
        int rc;
        mode_t          mode;
        struct stat     statBuf;

        rc = access(zDir, F_OK);
        if(rc == 0)
        {
                /* Is it really directory */
                rc = stat(zDir, &statBuf);
                if (rc != 0)
                        return(-1);

                mode = statBuf.st_mode & S_IFMT;
                if ( mode != S_IFDIR)
                        return(-1);

                return(0);
        }

        rc = mkdir(zDir, 0755);
        return(rc);
}

/*
 * Madhav's routine - Should be moved to a common place in the 
 * isp2.2 directory structure.
 */
static int getDefaultISPBase(char *baseDirBuffer)
{
char	defaultConfigFileName[128];
char	str[80];
FILE	*fp;
char	*ptr;

sprintf(defaultConfigFileName, "%s", "/arc/arcEnv");
if((fp=fopen(defaultConfigFileName, "r")) != NULL)
{
	while(fgets(str, 80, fp) != NULL)
	{
		switch(str[0])
		{
			case '[':
			case ' ':
			case '\n':
			case '#':
				continue;
				break;
			default:
				break;
		}
		if(strncmp(str, "ISPBASE", strlen("ISPBASE")) == 0)
		{
 			if((ptr = (char *)strchr(str, '=')) != NULL)
                       	{
				ptr++;
				baseDirBuffer[0] = '\0';
				strcpy(baseDirBuffer, ptr);
				baseDirBuffer[strlen(baseDirBuffer) - 1] = '\0';
			}
			break;
		}
	}
fclose(fp);
}
return(0);
} /* END: getDefaultISPBase() */
/*-----------------------------------------------------------------------------
ocmCreateCallRequest(): Schedule an outbound call at a specific time with new 
						parameters like hunt group, preTagList, postTagList..
-----------------------------------------------------------------------------*/
int ocmCreateCallRequest(OCM_ASYNC_CDS *zCds)
{
	char			failureReason[256];
	char			date_and_time[128];
	CDF_NAME		myCdf;


	if((atoi(zCds->nrings) < 0) || (atoi(zCds->nrings) > 15))
	{
   		sprintf(zCds->reason, "Invalid parameter (Number of rings): %s.", 
				zCds->nrings);
   		return (-1);
	}
	if(atoi(zCds->tries) <= 0)
	{
   		sprintf(zCds->reason,"Invalid parameter (%s):  %s.",
				"tries", zCds->tries);
   		return (-1);
	}
	if( (strcmp(zCds->informat, "NAMERICAN") != 0 )&& 
		(strcmp(zCds->informat, "NONE") != 0) &&
		(strcmp(zCds->informat, "SIP") != 0) &&
		(strcmp(zCds->informat, "IP") != 0) &&
		(strcmp(zCds->informat, "H323") != 0) )
	{
   		sprintf(zCds->reason,"Invalid parameter (%s): %s.",
				"Informat", zCds->informat);
   		return (-1);
	}
	if(zCds->phone[0] == '\0')
	{
   		sprintf(zCds->reason,"Invalid parameter (%s): %s.",
				"Phone", zCds->phone);
   		return (-1);
	}
	if(zCds->callTime[0] == '\0')
	{
   		sprintf(zCds->reason,"Invalid parameter (%s): %s.",
				"Call Time", zCds->callTime);
   		return (-1);
	}
	if(zCds->newApp[0] == '\0')
	{
   		sprintf(zCds->reason,"Invalid parameter (%s): <%s>.",
				"New app. name", zCds->newApp);
   		return (-1);
	}
	if((int)strlen(zCds->appToAppInfo) > MAX_APP2APPINFO)
	{
   		sprintf(zCds->reason,"App to app information length (%d) must be "
				" < %d.",
				(int)strlen(zCds->appToAppInfo), MAX_APP2APPINFO);
   		return (-1);
	}
	strcpy(date_and_time, zCds->callTime);
	if(editAndValidateTime(date_and_time, zCds->reason) < 0)
	{
		return(-1);	/* Message written in sub-routine */
	}

	/* All parameters are OK */

	memset(&myCdf, 			0, sizeof(CDF_NAME));
	memset(failureReason, 	0, sizeof(failureReason));

	if(makeCdfName(&myCdf, failureReason) < 0)
	{
   		sprintf(zCds->reason,
				"Failed to generate call definition filename <%s>.",
				failureReason);
   		return (-1);
	}

	memset(failureReason, 	0, sizeof(failureReason));

	sprintf(zCds->cdf, 			"%s", myCdf.cdf);
	sprintf(zCds->callTime,		"%s", date_and_time);
	sprintf(zCds->requesterPid, "%d", getpid());

	if(ocmWriteCdfInfo(zCds, OCS_WORK_DIR, failureReason) < 0)
	{
   		sprintf(zCds->reason, "Failed to schedule outbound call. [%s]",
				failureReason);
   		return (-1);
	}

	return(TEL_SUCCESS);
} /* END: ocmCreateCallRequest() */
/*------------------------------------------------------------------------------
ocmWriteCdfInfo():	Write the information in the OCM_CDS structure to the CDF
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
int ocmWriteCdfInfo(const OCM_ASYNC_CDS *zCds, char *dir, char *failureReason)
{
	FILE	*fp;
	char	*ispbase, cdfPath[256], zeroFilePath[256];
	char	baseDirBuffer[256];

	/* Do we at least have a CDF name */
	if(zCds->cdf[0] == '\0')
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
	if(checkOcsDirs(failureReason) < 0)
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
	sprintf(cdfPath, 		"%s/%s/%s", 	ispbase, dir, zCds->cdf);
	sprintf(zeroFilePath, 	"%s/%s/0.%s", 	ispbase, dir, zCds->cdf);

	/* All is well. Now write the structure into the file */
	if((fp = fopen(cdfPath, "w")) == NULL)
	{
		sprintf(failureReason,
			"Failed to open CDF file (%s) for writing, [%d: %s]",
			cdfPath, errno, strerror(errno));
		return(-1);
	}

	fprintf(fp, "%s=%s\n",	CDV_NRINGS,				zCds->nrings);
	fprintf(fp, "%s=%s\n",	CDV_PHONE,				zCds->phone);
	fprintf(fp, "%s=%s\n",	CDV_INFORMAT,			zCds->informat);
	fprintf(fp, "%s=%s\n",	CDV_TRIES,				zCds->tries);
	fprintf(fp, "%s=%s\n",	CDV_CALLTIME,			zCds->callTime);
	fprintf(fp, "%s=%s\n",	CDV_NEWAPP,				zCds->newApp);
	fprintf(fp, "%s=%s\n",	CDV_APP2APPINFO,		zCds->appToAppInfo);
	fprintf(fp, "%s=%s\n",	CDV_REQUESTER_PID,		zCds->requesterPid);
	fprintf(fp, "%s=%s\n",	CDV_RETRY_INTERVAL,		zCds->retryInterval);
	fprintf(fp, "%s=%s\n",	CDV_TRUNK_GROUP,		zCds->trunkGroup);
	fprintf(fp, "%s=%s\n",	CDV_PRE_TAG_LIST,		zCds->preTagList);
	fprintf(fp, "%s=%s\n",	CDV_POST_TAG_LIST,		zCds->postTagList);
	fprintf(fp, "%s=%s\n",	CDV_OUTBOUND_CALL_PARAM,zCds->outboundCallParam);
	fprintf(fp, "%s=%s\n",	CDV_INITIAL_SCRIPT,		zCds->initialScript);
	fprintf(fp, "%s=%s\n",	CDV_CALL_ID,			zCds->callId);
	fprintf(fp, "%s=%s\n",	CDV_STATUS_URL,			zCds->statusUrl);
	fprintf(fp, "%s=%s\n",	CDV_APPLICATION_DATA,	zCds->applicationData);

	fflush(fp);
	fclose(fp);

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

	return(0);
} /* END: ocmWriteCdfInfo() */
/*------------------------------------------------------------------------------
ocmLoadCallRequest():	Read the information into the OCM_CDS structure from 
						the CDF specified in the cdf member of the structure.

Return Values:

0 	Read all information in the file successfully.
-1	Failed to read information to the CDF. failureReason contains a string
	indicating the reason why the read failed.
------------------------------------------------------------------------------*/
int ocmLoadCallRequest(OCM_ASYNC_CDS *zCds, char *dir, char *failureReason)
{
	char	saveCdf[256], cdfPath[256];
	char	line[2048], lhs[25], *rhs, *ptr;
	char	*ispbase;
	FILE	*fp;
	char	baseDirBuffer[256];

	/* Do we at least have a CDF name */
	if(zCds->cdf[0] == '\0')
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

	memset(saveCdf, 0, sizeof(saveCdf));
	sprintf(saveCdf, "%s", zCds->cdf);

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

	memset(cdfPath, 0, sizeof(cdfPath));
	sprintf(cdfPath, "%s/%s/%s", ispbase, dir, saveCdf);

	/* Clear the structure */
	memset(zCds, 0, sizeof(OCM_CDS));

	/* Restore the CDF that we just wiped out */
	sprintf(zCds->cdf, "%s", saveCdf);

	if((fp = fopen(cdfPath, "r")) == NULL)
	{
		sprintf(failureReason,
			"Failed to open CDF file <%s> for reading, "
			"errno=%d [%s]",
			cdfPath, errno, strerror(errno));
		return(-1);
	}
	while(fgets(line, sizeof(line)-1, fp))
	{
		/* Ignore lines that don't have a '=' in them */
		if(! strchr(line, '='))
			continue;

		memset(lhs, 0, sizeof(lhs));
		rhs = 0;

		ptr = line;

		/* Even if strtok returns NULL, we don't care. 
	 	 * All it will do is give us a NULL lhs.
	 	 */
		
		sprintf(lhs, "%s", (char *)strtok(ptr, "=\n"));

		/* If LHS is NULL, don't even bother looking at RHS */
		if(lhs[0] == '\0')
		{
			continue;
		}

		rhs = (char *)strtok(NULL, "\n");

		if(! rhs || ! *rhs)
		{
			rhs = "";
		}

		if(strcmp(lhs, CDV_NRINGS) == 0)
		{
			sprintf(zCds->nrings, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_PHONE) == 0)
		{
			sprintf(zCds->phone, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_INFORMAT) == 0)
		{
			sprintf(zCds->informat, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_TRIES) == 0)
		{
			sprintf(zCds->tries, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_CALLTIME) == 0)
		{
			sprintf(zCds->callTime, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_NEWAPP) == 0)
		{
			sprintf(zCds->newApp, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_APP2APPINFO) == 0)
		{
			sprintf(zCds->appToAppInfo, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_REQUESTER_PID) == 0)
		{
			sprintf(zCds->requesterPid, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_RETRY_INTERVAL) == 0)
		{
			sprintf(zCds->retryInterval, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_TRUNK_GROUP) == 0)
		{
			sprintf(zCds->trunkGroup, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_PRE_TAG_LIST) == 0)
		{
			sprintf(zCds->preTagList, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_POST_TAG_LIST) == 0)
		{
			sprintf(zCds->postTagList, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_OUTBOUND_CALL_PARAM) == 0)
		{
			sprintf(zCds->outboundCallParam, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_INITIAL_SCRIPT) == 0)
		{
			sprintf(zCds->initialScript, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_CALL_ID) == 0)
		{
			sprintf(zCds->callId, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_STATUS_URL) == 0)
		{
			sprintf(zCds->statusUrl, "%s", rhs);
		}
		else if(strcmp(lhs, CDV_APPLICATION_DATA) == 0)
		{
			sprintf(zCds->applicationData, "%s", rhs);
		}
		else 
		{
			/* We don't recognize the LHS. Just ignore it */
			continue;
		}
	} /* while */

	fclose(fp);
	return(0);
} /* END: ocmLoadCallRequest() */

/*------------------------------------------------------------------------------
ocmReportStatus(): Report the status of an outbound call to the statusUrl

Return Values:

1		Failed to report status, no status FIFO found.
0 		Successfully sent status info to FIFO
-1		Failed to report status, error message details reason for failure.
------------------------------------------------------------------------------*/
int ocmReportStatus(OCM_CALL_STATUS *zCallStatus, char *zErrorMsg)
{
	int		yFd;
	int		yFifoFd;
	long	yBytesWritten;

	/*
	** Clear Buffers
	*/
	*zErrorMsg = 0;

	/*
	** First check if the status FIFO exists.
	*/
	if(access(OCM_CALL_STATUS_FIFO, F_OK|W_OK) < 0)
	{
		/*
		** If it does not exist, return 1 - indicating that FIFO does not exist.
		*/
		if(errno == ENOENT)
		{
			sprintf(zErrorMsg, "Status not reported. Status FIFO not found.");

			return(1);
		}

		/*
		** System Error. Return failure.
		*/
		sprintf(zErrorMsg,
				"Failed to report status FIFO (%s), [%d: %s]",
				OCM_CALL_STATUS_FIFO, errno, strerror(errno));

		return(-1);
	}

	/*
	** Open the FIFO for writing.
	*/
	if((yFifoFd = open(OCM_CALL_STATUS_FIFO, O_WRONLY | O_NONBLOCK)) < 0)
	{	
		sprintf(zErrorMsg,
				"Failed to open report status FIFO (%s) for writing, [%d: %s]",
				OCM_CALL_STATUS_FIFO, errno,strerror(errno));

		return(-1);
	}

	/*
	** Write the status to the FIFO
	*/
	yBytesWritten = write(yFifoFd, zCallStatus, sizeof(OCM_CALL_STATUS));

	if(yBytesWritten != sizeof(OCM_CALL_STATUS))
	{
		sprintf(zErrorMsg,
				"Failed to write to call status FIFO (%s), [%d: %s]",
				OCM_CALL_STATUS_FIFO, errno,strerror(errno));

		close(yFifoFd);

		return(-1);
	}

	/*
	** Close the FIFO and return
	*/
	close(yFifoFd);

	return(0);

} // END: ocmReportStatus()

/*-----------------------------------------------------------------------------
ocmCreateCallRequestWithoutStruct(): 

Just a wrapper for ocmCreateCallRequest() - doesn't require structures to 
be passed in.
-----------------------------------------------------------------------------*/
int ocmCreateCallRequestWithoutStruct(
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
	)
{
	OCM_ASYNC_CDS	yCds;
	int				yRc;

	memset(&yCds, 0, sizeof(OCM_ASYNC_CDS));

	sprintf(yCds.nrings,			"%s", zRings);
	sprintf(yCds.informat,			"%s", zPhoneFormat);
	sprintf(yCds.phone,			"%s", zPhoneNumber);
	sprintf(yCds.callTime,			"%s", zCallTime);
	sprintf(yCds.tries,			"%s", zTries);
	sprintf(yCds.newApp,			"%s", zApplicationName);
	sprintf(yCds.appToAppInfo,		"%s", zApp2AppInfo);
	sprintf(yCds.retryInterval,	"%s", zRetryInterval);
	sprintf(yCds.trunkGroup,		"%s", zTrunkGroup);
	sprintf(yCds.postTagList,		"%s", zPostTagList);
	sprintf(yCds.preTagList,		"%s", zPreTagList);
	sprintf(yCds.outboundCallParam,"%s", zOutboundCallParam);
	sprintf(yCds.statusUrl,		"%s", zStatusUrl);
	sprintf(yCds.callId,			"%s", zCallId);
	sprintf(yCds.initialScript,	"%s", zInitialScript);

	if((yRc = ocmCreateCallRequest(&yCds)) != 0)
	{
		sprintf(zErrorString, 
				"Failed to schedule call for callId (%s), rc (%d), (%s)",
				zCallId, yRc, yCds.reason);

		return(yRc);
	}
	
	return(0);

} /* END: ocmCreateCallRequestWithoutStruct() */

/*------------------------------------------------------------------------------
ocmReportStatusWithoutStruct():

Just a wrapper for ocmReportStatus() - doesn't require structures to 
be passed in.
------------------------------------------------------------------------------*/
int ocmReportStatusWithoutStruct(
	int		zStatusCode,
	char	*zCallId,
	char	*zStatusMsg,
	char	*zStatusUrl,
	char	*zErrorString
	)
{
	OCM_CALL_STATUS	yCdsStatus;
	int				yRc;

	memset(&yCdsStatus, 0, sizeof(OCM_CALL_STATUS));

	yCdsStatus.statusCode = zStatusCode;
	sprintf(yCdsStatus.callId, 		"%s", zCallId);
	sprintf(yCdsStatus.statusMsg, 	"%s", zStatusMsg);
	sprintf(yCdsStatus.statusUrl, 	"%s", zStatusUrl);

	yRc = ocmReportStatus(&yCdsStatus, zErrorString);

	return(yRc);

} // END: ocmReportStatusWithoutStruct()

int ocmMoveCDF(const OCM_ASYNC_CDS *zCds, char *from, char*to, char * comments, char *failureReason)
{
    FILE    *fp;
    char    *ispbase, cdfFromPath[256], cdfToPath[256];
    char    baseDirBuffer[256];

    /* Do we at least have a CDF name */
    if(zCds->cdf[0] == '\0')
    {
        sprintf(failureReason, "%s", "CDF is NULL");
        return(-1);
    }


    if(!from || !from[0])
    {
        sprintf(failureReason, "%s", "From is NULL");
        return(-1);
    }

    if(!to || !to[0])
    {
        sprintf(failureReason, "%s", "TO is NULL");
        return(-1);
    }

    /* Make sure we have all the dirs we need */
    if(checkOcsDirs(failureReason) < 0)
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
    sprintf(cdfFromPath,    "%s/%s/%s",     ispbase, from,  zCds->cdf);
    sprintf(cdfToPath,      "%s/%s/%s",     ispbase, to,    zCds->cdf);
    
    if(comments && comments[0])
    {
        if((fp = fopen(cdfFromPath, "a")) == NULL)
        {
            sprintf(failureReason,
                "Failed to open CDF file (%s) for writing, [%d: %s]",
                cdfFromPath, errno, strerror(errno));
        }
        else
        {
            fprintf(fp, "#%s\n", comments);
            fflush(fp);
            fclose(fp);
        }
    }

    rename(cdfFromPath, cdfToPath);

    return(0);
}
