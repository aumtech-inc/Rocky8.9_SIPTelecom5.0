#ident	"@(#)FAX_Common 99/06/15 2.9 Copyright 1999 Aumtech"
/*-----------------------------------------------------------------------------
Program:	FAX_Common.c
Purpose:	Common routines used by FAX APIs.
Author: 	D. Barto
Update:		05/21/99 djb Created the file.
Update:		12/26/02 apj Changes necessary for Single DM structure.
Update:		05/27/02 apj Put debug messages inside DEBUG define.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "arcFAX.h"

#if 0
/*------------------------------------------------------------------------------
getAFaxResource():
	Requests a resource and it's slot number from the resource manager 
	and handles the return code.  If a success (0) is not returned, 
	the a descriptive log message is logged, and failure is returned.

  Return:
		0	Success
		-1	Failure
------------------------------------------------------------------------------*/
int getAFaxResource(parmMod, faxResource, faxResourceSlotNumber)
char *parmMod;
char *faxResource;
char *faxResourceSlotNumber;
{
	int		rc;
	static char	logMsg[256];

	sprintf(logMsg,
		"About to request of Resource Manager a resource of "
		"type <%s>, subtype <%s> from port number=%d.",
			RESOURCE_TYPE, RESOURCE_SUB_TYPE, GV_AppCallNum1);
	fax_log(parmMod, REPORT_VERBOSE, 0, logMsg);

	GV_LastFaxError = 0;

	rc = Request_Resource(RESOURCE_TYPE, RESOURCE_SUB_TYPE,
		GV_AppCallNum1, GV_AppName, faxResource, 
		faxResourceSlotNumber, logMsg);

    	sprintf(logMsg,
		"Got from Resource Manager: rc=%d. Resource=<%s>. Slot=<%s>.", 
		rc, faxResource, faxResourceSlotNumber);
        fax_log(parmMod,REPORT_VERBOSE, TEL_BASE, logMsg);

	GV_LastFaxError = rc;

	if (rc != 0)
	{
		/* First log the message passed back from the subroutine */
	      fax_log(parmMod,REPORT_NORMAL,TEL_BASE,logMsg);
	}
	
	/* Note: the RESM_* return codes are defined in ResourceMgr.h */
	switch (rc)
	{
	    case 0:	
			return(0);
			break;
	    case RESM_GENERAL_FAILURE:
			sprintf(logMsg,"General failure occurred while "
				"requesting a fax resource for type=<%s>, "
				"subtype=<%s>. rc=%d."
				RESOURCE_TYPE, RESOURCE_SUB_TYPE, rc);
			fax_log(parmMod,REPORT_NORMAL,
				TEL_BASE,logMsg);
			return (-1);
			break;
	    case RESM_UNABLE_TO_SEND:
			sprintf(logMsg,
				"Unable to send request to Resource Manager "
				"for resource type=<%s>, subtype=<%s>. rc=%d",
				RESOURCE_TYPE, RESOURCE_SUB_TYPE, rc);
			fax_log(parmMod,REPORT_NORMAL,
				TEL_BASE, logMsg);
			return (-1);
			break;
	    case RESM_NO_RESPONSE:
			sprintf(logMsg,
				"No response from Resource Manager for "
				"resource request of type=<%s>, subtype=<%s>. "
				"rc=%d.",
				RESOURCE_TYPE, RESOURCE_SUB_TYPE, rc);
			fax_log(parmMod,REPORT_NORMAL,
				TEL_BASE, logMsg);
			return (-1);
			break;
	    case RESM_RESOURCE_NOT_AVAIL:
			sprintf(logMsg,
			"All resources of type=<%s>, subtype=<%s> are in use.",
				RESOURCE_TYPE, RESOURCE_SUB_TYPE);
			fax_log(parmMod,REPORT_NORMAL, TEL_BASE, logMsg);
			return(-1);
			break;
	    case RESM_NO_RESOURCES_CONFIG:
			sprintf(logMsg,"No resources of type=<%s> subtype=<%s> "
				"are configured for the Resource Manager."
				RESOURCE_TYPE, RESOURCE_SUB_TYPE);
			fax_log(parmMod,REPORT_NORMAL, TEL_BASE,logMsg);
			return(-1);
			break;
	    default:
			sprintf(logMsg,"Failed to get resource of type=<%s>, "
				"subtype=<%s> from Resource Manager for "
				"Unknown reason. rc=%d", 
				RESOURCE_TYPE, RESOURCE_SUB_TYPE, rc);
			fax_log(parmMod,REPORT_NORMAL,TEL_BASE,logMsg);
			return(-1);
			break;
	}

} /* END: getAFaxResource() */

/*------------------------------------------------------------------------------
validateFaxPageParams()
	Validates the startPage and numberOfPages values.
------------------------------------------------------------------------------*/
void validateFaxPageParams(parmModuleName, faxFile, startPage, numberOfPages)
char 	*parmModuleName;
char	*faxFile;
int	*startPage;
int	*numberOfPages;
{
	char	logMsg[512];

	if (*startPage < 0)
	{
		sprintf(logMsg, "Warning: Invalid start page parameter: %d.  "
				"Entire tiff file <%s> will be sent.",
				startPage, faxFile);
		fax_log(parmModuleName, REPORT_NORMAL, TEL_BASE,logMsg);
		*startPage=0;
	}
	
	if (*numberOfPages < 0)
	{
		sprintf(logMsg,
			"Warning: Invalid number of pages parameter: %d. "
			"All remaining pages of tiff file <%s> will be sent.",
			numberOfPages, faxFile);
		fax_log(parmModuleName, REPORT_NORMAL, TEL_BASE,logMsg);
		*numberOfPages=-1;
	}

	return;
} /* END: validateFaxPageParams() */

void getFileTypeString(int parmFileType, char *parmFileTypeString)
{
	switch (parmFileType)
	{
		case LIST:
			sprintf(parmFileTypeString, "%s", "LIST");
			break;

		case MEM:
			sprintf(parmFileTypeString, "%s", "MEM");
			break;

		case TEXT:
			sprintf(parmFileTypeString, "%s", "TEXT");
			break;

		case TIF:
			sprintf(parmFileTypeString, "%s", "TIF");
			break;

		default:
			sprintf(parmFileTypeString, "%d", parmFileType);
			break;
	}

	return;
} /* END getFileTypeString() */
#endif

int createDirIfNotPresent(char *zDir)
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

int mkdirp(char *zDir, int perms)
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

                if ((pNextLevel=strtok(NULL, "/")) == NULL)
                {
                        break;
                }
                strcat(lBuildDir, "/");
                strcat(lBuildDir, pNextLevel);
        }

        return(0);
}

/*----------------------------------------------------------------------------
Fax logging routine. Absolutely all logging with the fax subsystem should
log messages by calling this routine. NO EXCEPTIONS!
----------------------------------------------------------------------------*/
int fax_log(mod, mode, id, msg)
char *mod;
int mode;
int id;
char *msg;
{
	char portString[5];

	sprintf(portString, "%d", GV_AppCallNum1);

	LogARCMsg(mod, mode, portString, "TEL", GV_AppName, id, msg);
	return(0);
}

/*-----------------------------------------------------------------------------
Get the configuration information for outbound faxes. 
-----------------------------------------------------------------------------*/
get_fax_config(base_fax_dir, timeout, default_tries, fail_on_busy, retry_interval)
char *base_fax_dir;
int *timeout;
int *default_tries;
int *fail_on_busy; 
int *retry_interval;
{
char 	*base;
char 	table_dir[]="Telecom/Tables";
FILE 	*fp;
int 	line_no;	
int 	def_timeout=20;	
int 	def_default_tries=3;
int 	def_fail_on_busy=1;
int 	def_retry_interval=10; /* minutes */
char 	config_file[128]; 
char 	line[256]; 
char 	string[128]; 

/* Set all the defaults */
base 		= getenv("ISPBASE");
*timeout 	= def_timeout;
*default_tries 	= def_default_tries;
*fail_on_busy 	= def_fail_on_busy;
*retry_interval = def_retry_interval;	

sprintf(base_fax_dir,"%s/Telecom/Fax",base);

#ifdef DEBUG
fprintf(stdout,"DEBUG get_fax_config: base_fax_dir is %s\n", base_fax_dir); fflush(stdout);
#endif

sprintf(config_file,"%s/%s/FaxServer.cfg", base, table_dir);

#ifdef DEBUG
fprintf(stdout,"DEBUG get_fax_config: config file is %s\n", config_file); fflush(stdout);
#endif

if ((fp = fopen(config_file, "r")) == NULL)
	{
	/* all defaults are set */
	return;
	}

line_no = 1;
while (fgets(line, 256, fp) != NULL)
        {
	if ( line[0] == '#') continue;
	switch(line_no)
		{
		case 1: 
			sscanf(line,"%s", base_fax_dir);
			line_no++;
			break; 
		case 2: 	
			sscanf(line,"%s", string);
			*timeout = atoi(string);
			line_no++;
			break;
		case 3: 	
			sscanf(line,"%s", string);
			*default_tries = atoi(string);
			line_no++;
			break;
		case 4: 	
			sscanf(line,"%s", string);
			*fail_on_busy = atoi(string);
			line_no++;
			break;
		case 5: 	
			sscanf(line,"%s", string);
			*retry_interval = atoi(string);
			line_no++;
			break;
		}
	}

fclose(fp);	
return;
}

void getFileTypeString(int parmFileType, char *parmFileTypeString)
{
	switch (parmFileType)
	{
		case LIST:
			sprintf(parmFileTypeString, "%s", "LIST");
			break;

		case MEM:
			sprintf(parmFileTypeString, "%s", "MEM");
			break;

		case TEXT:
			sprintf(parmFileTypeString, "%s", "TEXT");
			break;

		case TIF:
			sprintf(parmFileTypeString, "%s", "TIF");
			break;

		default:
			sprintf(parmFileTypeString, "%d", parmFileType);
			break;
	}

	return;
} /* END getFileTypeString() */
