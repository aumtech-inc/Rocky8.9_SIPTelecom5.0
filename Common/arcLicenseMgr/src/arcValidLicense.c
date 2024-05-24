/*------------------------------------------------------------------------------
File:		arcValidLicense.c

Purpose:	Contains ARC license validation routines.

Author:		Sandeep Agate, Aumtech Inc.

Update:		2002/06/05 sja 	Created the file.
------------------------------------------------------------------------------*/

/*
** Include Files
*/
#include <stdio.h>
#include <time.h>

/*
** Function Prototypes
*/

/*---------------------------------------------------------------------------- 
This function checks the mac address of the machine, then looks for and
counts the requested feature. If the feature is not found, and turnkey_allowed 
is set to 1, the then "TURN" is appended to the requested feature and a check 
is done to see if that feature is licensed. If so, the turkey_found is set to
1 and the function returns 0 (success).
----------------------------------------------------------------------------*/
int arcValidLicense(char *start_feature, char *feature_requested, 
					int turnkey_allowed, int *turnkey_found, 
					int *feature_count, char *msg)
{
	int rc;
	int count, mac_tries;
	char feature[64];
	char err_msg[512];

/* 4/10/2001 gpw Start of variables added for checking license start date */
	int year, month, day;
	long now;
	long earliest_date;
	struct tm  start_date;
/* 4/5/2001 gpw End of variables added for checking license start date */

	/* Initialize return values */
	*turnkey_found = 0;
	*feature_count = 0;
	strcpy(msg,""); 

	strcpy(feature,feature_requested);
	rc = lic_get_license(feature, err_msg);
	switch(rc)
	{
	case 0:
		strcpy(msg,"License granted (%s).");
		break;
	case -1:
		if (!turnkey_allowed)
		{
			strcpy(msg, err_msg);
			return(-1);
		}
		sprintf(feature,"%sTURN",feature_requested);
		rc = lic_get_license(feature, err_msg);
		if (rc == 0)
		{
			sprintf(msg,"Turnkey license granted (%s).", feature);
			*turnkey_found = 1;
		}
		else
		{
			sprintf(msg,"Failed to get license (%s).", 
							feature_requested);
			return(-1);
		}
		break;
	default:
		sprintf(msg,"Unknown license return code %d.", rc);
		return(-1);
	}

/* 4/10/2001 gpw Start of new code added for checking license start date */
	rc = lic_get_license(start_feature, err_msg);
	if (rc != 0)
	{
		strcpy(msg, err_msg);
		return(-1);
	}
	rc = lic_get_feature_count_as_yymmdd(start_feature, 
					&year, &month, &day, err_msg);
	fflush(stdout);
	if (rc != 0)
	{
		strcpy(msg, err_msg);
		return(-1);
	}

	/* Convert the start date feature's elements to tics */
	start_date.tm_sec=0;
	start_date.tm_min=0;
	start_date.tm_hour=0;
	start_date.tm_mday=day;
	start_date.tm_mon=month-1;
	start_date.tm_year=year-1900;
	earliest_date=mktime(&start_date);
	
	time(&now);
	if (now < earliest_date)
	{
		strcpy(msg,"License failure: premature usage");
		return(-1);
	}	
/* 4/10/2001 gpw End of new code added for checking license start date */

	rc = lic_count_licenses(feature, &count, err_msg);
	if (rc == -1)
	{
		strcpy(msg,err_msg);
	 	return(-1);
	}

	*feature_count = count;
	return(0);
} /* END: arcValidLicense() */



