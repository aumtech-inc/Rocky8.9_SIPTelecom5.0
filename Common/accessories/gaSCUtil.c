/*------------------------------------------------------------------------------
File:		gaSCUtil.c

Purpose:	Service Creation string manipulation accessories

Update:		01/21/98 djb	Created the file
Update:		02/04/99 djb	changed cftime() to strftime(); SNA EXPRESS
------------------------------------------------------------------------------*/
#include "gaIncludes.h"
#include "gaUtils.h"

static int mkdirp(char *zDir, int perms);
static int createDirIfNotPresent(char *zDir);

/*------------------------------------------------------------------------------
 * gaStrCpy():
 * 	Copy one string to another (strcpy)
 *----------------------------------------------------------------------------*/
int gaStrCpy(char *dest, char *source)
{
	strcpy(dest, source);
	return(0);
} /* END: gaStrCpy() */

/*------------------------------------------------------------------------------
 * gaStrNCpy():
 *	Copy the first n characters of one string ot another (strncpy)
 *----------------------------------------------------------------------------*/
int gaStrNCpy(char *dest, char *source, int len)
{
	strncpy(dest, source, len);
	return(0);
} /* END: gaStrNCpy() */

/*------------------------------------------------------------------------------
 * gaStrCmp():
 * 	Compare string 1 to string 2 (strcmp)
 *----------------------------------------------------------------------------*/
int gaStrCmp(char *string1, char *string2)
{
	return(strcmp(string1, string2));
} /* END: gaStrCmp() */

/*------------------------------------------------------------------------------
 * gaStrNCmp():
 * 	Compare the rifst n characters of string 1 and string 2 (strncmp)
 *----------------------------------------------------------------------------*/
int gaStrNCmp(char *string1, char *string2, int len)
{
	return(strncmp(string1, string2, len));
} /* END: gaStrNCmp() */

/*------------------------------------------------------------------------------
 * gaStrCat():
 * 	Append string 2 to the end of string 1 (strcat)
 *----------------------------------------------------------------------------*/
int gaStrCat(char *string1, char *string2)
{
	strcat(string1, string2);
	return(0);
} /* END: gaStrCat() */

/*------------------------------------------------------------------------------
 * gaGetDateTime():
 * 	Obtain today's date and time in a specified format
 *----------------------------------------------------------------------------*/
int gaGetDateTime(char *date, char *dateFormat)
{
	char	msgbuf[256];
	time_t	mytime;
	struct tm	*PT_time;

	time(&mytime);

	PT_time=localtime(&mytime);
	return(strftime(date, sizeof(date),dateFormat, PT_time));

} /* END: gaGetDateTime() */

/*------------------------------------------------------------------------------
 * gaMakeDirectory(char *dirName,	input: dir name to be created
 *		   mode_t permissions)	input:  permissions (i.e. 0755)
 * 
 * 	Purpose:
 * 		Attempts to create directory <dirName> with permissions
 * 		<permissions>.  If the directory already exists, the
 * 		permissions are verified/set.
 * 
 * 	Return Codes:
 * 	0	Success
 * 	-1	Failure: <dirName> could not be created or the modification
 * 			 could not be set correctly.
 *----------------------------------------------------------------------------*/
int gaMakeDirectory(char *dirName, mode_t permissions)
{
	int		rc;
	static char	mod[]="gaMakeDirectory";

	if ((rc=mkdirp(dirName, permissions)) == -1)
	{
		if (errno == EEXIST)
		{
			if ((rc=chmod(dirName, permissions)) == -1)
			{
				gaVarLog(mod, 0,
					"chmod(%s,%o) failed. errno=%d",
						dirName, permissions, errno);
				return(-1);
			}
			return(0);

		}
		else
		{
			gaVarLog(mod, 0,
				"mkdirp %s failed. errno=%d",dirName, errno);
			return(-1);
		}
	}

	return(0);
} /* END: gaMakeDirectory() */

static int mkdirp(char *zDir, int perms)
{
        char *pNextLevel;
        int rc;
        char lBuildDir[512];
		char    *yStrTok = NULL;

        pNextLevel = (char *)strtok_r(zDir, "/", &yStrTok);
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

                if ((pNextLevel=strtok_r(NULL, "/", &yStrTok)) == NULL)
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
