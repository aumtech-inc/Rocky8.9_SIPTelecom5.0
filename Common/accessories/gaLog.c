/*--------------------------------------------------------------------
File:		gaLog.c

Purpose:	This file contains logging routines

Author:		Sandeep Agate

Update:	11/06/97 sja	Created the file
Update:	02/11/98 djb	Changed 'count' to 'count+1' for SetGlobalString
Update:	08/04/98 sja	If logBaseLength == 0, write to stderr
Update:	04/29/99 djb	Fixed bug of parsing filename from fullpath; added
				parseFilenameFromFullPathname() routine
Update:	2000/11/30 sja	Changed sys_errlist[] to strerror()
Update:	2001/05/15 sja	Per Madhav, added gaLog()
Update:	2001/05/15 mpb	Changed cftime to strftime.
Update:	2001/05/24 sja	Added fflush() to gaVarLog() and gaLog()
Update:	2002/09/11 djb	Removed the nulling of trailing '/' - gaSetGlobalString.
Update:	2003/05/05 apj&ddn Added MilliSec timing in gaLog.
Update: 2006/12/14 rbm	Changed timestamp format to default to MILLISECONDS
						if the ISPBASE cannot be determined. 
--------------------------------------------------------------------*/
#include <sys/timeb.h>
#include "gaIncludes.h"		
#include "gaUtils.h"

	/* declare and initialize static variables to defaults */
static char	programName[64]	=	"";
static char	logDir[256]	=	".";
static char	logBase[64]	=	"";
static long	logBaseLength	=	0;
static char	resourceName[8]	=	"0";
static char	debugDir[256]	=	"/tmp";

char gMsgFormat[50] = {"%4s|%10.10s|%5d|%8s|%15.15s|%s\n"};

static void	parseFilenameFromFullPathname(char *str);

/*----------------------------------------------------------------------------
gaVarLog(): Log messages on stderr and/or a log file. (Takes variable length
	 argument lists.
----------------------------------------------------------------------------*/
int gaVarLog(char *moduleName, int debugLevel, char *messageFormat, ...)
{

	va_list		ap;
	static int	logFirstTime = 1;
	static int	rVal;
	FILE		*fpOut;
	time_t		myTime;
	char		today[20];
	char		timeBuf[64];
	char		outFile[256];
	char		message[2048];
	static int	currentDebugLevel=0;
	struct tm 	*pTime;
	static int sFirstTime = 1;
	static int dateFormatMilliSec = 0;

	if(sFirstTime == 1)
	{
		char lBuf[128];
		char lFileName[256];
		int rc;
		char *pISPBASE;

		sFirstTime = 0;

		pISPBASE = getenv("ISPBASE");
		sprintf(lFileName, "%s/Global/.Global.cfg", pISPBASE);
		lBuf[0] = '\0';

		if(!pISPBASE || !pISPBASE[0])
		{
			dateFormatMilliSec = 1;
			sprintf(gMsgFormat, "%s",  "%4s|%10.10s|%5d|%12s|%15.15s|%s\n");
		}
		else
		{
			rc = gaGetProfileString("Settings", "DATE_FORMAT", "",
	               	lBuf, sizeof(lBuf)-1, lFileName);
			if((rc >= 0) && (strcmp(lBuf, "MILLISECONDS") == 0))
			{
				dateFormatMilliSec = 1;
				sprintf(gMsgFormat, "%s","%4s|%10.10s|%5d|%12s|%15.15s|%s\n");
			}
		}
	}

	if ( debugLevel > 0 )
	{
		if(logFirstTime)
		{
			logFirstTime = 0;
			currentDebugLevel=gaGetDebugLevel();
		}

		if (debugLevel > currentDebugLevel)
			return(0);

	} /* if */

	time(&myTime);

	pTime = localtime(&myTime);

	strftime(timeBuf, sizeof(timeBuf)-1, "%H:%M:%S", pTime);
	strftime(today, sizeof(today)-1, "%y%m%d", pTime);

	if(dateFormatMilliSec == 1)
	{
		char lMilli[10];
		struct timeb lTimeB;

		ftime(&lTimeB);

		sprintf(lMilli, ":%d", lTimeB.millitm);
		strcat(timeBuf, lMilli);
	}

	/*
	cftime(timeBuf, "%H:%M:%S", &myTime);
	cftime(today, "%y%m%d", &myTime);
	*/

	memset(message, 0, sizeof(message));
	
	va_start(ap, messageFormat);
	vsprintf(message, messageFormat, ap);
	va_end(ap);

	if(logBaseLength == 0)
	{
		fprintf(stderr, gMsgFormat,
				resourceName,programName,getpid(),timeBuf,
				moduleName,message);
		fflush(stderr);
		return(0);
	}
	
	sprintf(outFile, "%s/log.%s.%s", logDir, logBase, today);

	if((fpOut = fopen(outFile, "a")) == NULL)
	{
		fprintf(stderr, "Failed to open %s for writing.  errno=%d\n",
							 outFile,errno);
		fprintf(stderr, gMsgFormat,
				resourceName,programName,getpid(),timeBuf,
				moduleName,message);
		fflush(stderr);
		return(-1);
	}

	fprintf(fpOut,gMsgFormat,resourceName,programName,
				getpid(),timeBuf,moduleName,message);
	fflush(fpOut);
	fclose(fpOut);
	return(rVal);
} /* END: gaVaLog() */

/*----------------------------------------------------------------------------
gaSetGlobalString(): Set global variables
----------------------------------------------------------------------------*/
int gaSetGlobalString(char *globalVar, char *newValue)
{
	int		rc;
	int		count;
	static char	slash[]="/";
	char		tmpValue[128];

	memset((char *)tmpValue, 0, sizeof(tmpValue));

	if(strcmp(globalVar, "$PROGRAM_NAME") == 0)
	{		/* '/<whatever>/<whatever>/<xxx>' becomes <xxx> */
		parseFilenameFromFullPathname(newValue);
		sprintf(programName, "%s", newValue);
	}

	else if(strcmp(globalVar, "$LOG_DIR") == 0)
	{
		sprintf(logDir, "%s", newValue);
	}

	else if(strcmp(globalVar, "$LOG_BASE") == 0)
	{		/* '/<whatever>/<whatever>/<xxx>' becomes <xxx> */
		parseFilenameFromFullPathname(newValue);
		sprintf(logBase, "%s", newValue);
		logBaseLength = (int)strlen(logBase);
	}

	else if(strcmp(globalVar, "$RESOURCE_NAME") == 0)
	{
		sprintf(resourceName, "%s", newValue);
	}

	else if(strcmp(globalVar, "$DEBUG_DIR") == 0)
	{
			/* remove trailing '/', if exist */
		while ( newValue[strlen(newValue)-1] == '/' )
		{
			newValue[strlen(newValue)-1] = '\0';
		}
		sprintf(debugDir, "%s", newValue);
	}

	else
	{
/*		fprintf(stderr, "Unsupported globalVarialble. (%s)\n",
								globalVar); */
		return(-1);
	}

	return(0);

} /* END: gaSetGlobalString() */

/*----------------------------------------------------------------------------
gaGetGlobalString(): Get global variables
----------------------------------------------------------------------------*/
int gaGetGlobalString(char *globalVar, char *value)
{
	if(strcmp(globalVar, "$PROGRAM_NAME") == 0)
	{
		sprintf(value, "%s", programName);
	}

	else if(strcmp(globalVar, "$LOG_DIR") == 0)
	{
		sprintf(value, "%s", logDir);
	}

	else if(strcmp(globalVar, "$LOG_BASE") == 0)
	{
		sprintf(value, "%s", logBase);
	}

	else if(strcmp(globalVar, "$RESOURCE_NAME") == 0)
	{
		sprintf(value, "%s", resourceName);
	}

	else if(strcmp(globalVar, "$DEBUG_DIR") == 0)
	{
		sprintf(value, "%s", debugDir);
	}

	else
	{
/*		fprintf(stderr, "Unsupported globalVarialble. (%s)\n",
								globalVar); */
		return(-1);
	}

	return(0);

} /* END: gaGetGlobalString() */

/*----------------------------------------------------------------------------
gaGetDebugLevel(char *debugDir, char *programName) 

	Searches <debugDir> for debug control files.  If either
	<debugDir>/debug.<programName>.<n> exists, or 
	<debugDir/debug.all.<n> exists, <n> is returned; otherwise,
	0 is returned.
----------------------------------------------------------------------------*/
int gaGetDebugLevel(void)
{
	int		rc;
	DIR             *dirPtr;
	struct dirent   *dirEntry;
	char		field1[64];
	char		field2[64];
	char		field3[64];
	int		intDebugLevel=0;
	short		allFound=0;
	short		programNameFound=0;

	if ((rc=access(debugDir, W_OK)) == -1)
	{
		fprintf(stderr, "Unable to access %s. %s.\n",
				debugDir, strerror(errno) );
		return(0);
	}

	if ((dirPtr=opendir(debugDir)) == NULL)
		{
		fprintf(stderr,"Unable to open directory %s. %s.\n",
					debugDir, strerror(errno));
		return(0);
		}

	while ((dirEntry=readdir(dirPtr)) != NULL)
		{
		memset((char *)field1, 0, sizeof(field1));
		if ((rc=gaGetField('.', dirEntry->d_name, 1,
							field1)) == -1)
			{
			continue;
			}

		if ( strcmp(field1, "debug") != 0) 
			{
			continue;	/* if not a debug file, continue */
			}
		
		memset((char *)field2, 0, sizeof(field2));
		if ((rc=gaGetField('.', dirEntry->d_name, 2,
							field2)) == -1)
			{
			continue;
			}

		memset((char *)field3, 0, sizeof(field3));
		if ((rc=gaGetField('.', dirEntry->d_name, 3,
							field3)) == -1)
			{
			continue;
			}

		if ( strcmp(field2, programName) == 0 )
			{
			if ( allFound )
				{
				intDebugLevel=0;
				}

			programNameFound=1;
			rc=atoi(field3);
			if ( rc > intDebugLevel)
				{
				intDebugLevel=rc;
				}
			continue;
			}

		if ( strcmp(field2, "all") == 0 )
			{
			if (! programNameFound)
				{
				allFound=1;
				rc=atoi(field3);
				if ( rc > intDebugLevel)
					{
					intDebugLevel=rc;
					}
				continue;
				}
			}
		}

	closedir(dirPtr);

	return(intDebugLevel);

} /* gaGetDebugLevel() */

/*------------------------------------------------------------------------------
parseFilenameFromFullPathname():
	Parse the filename from the full pathname of str.  Overwrite the
	str parameter with the result.
------------------------------------------------------------------------------*/
static void	parseFilenameFromFullPathname(char *str)
{
	char		tmpBuf[256];
	int		i;
	
	for (i=strlen(str)-1; i>0; i--)
	{
		if (str[i] == '/')
		{
			break;
                }
	}

	if (i == 0)
	{
		if (str[i] == '/')
		{
			sprintf(tmpBuf, "%s", &(str[i+1]));
		}
		else
		{
			/* no '/' found */
			return;
		}
	}
	else
	{
		sprintf(tmpBuf, "%s", &(str[i+1]));
	}

	sprintf(str, "%s", tmpBuf);

} /* END: parseFilenameFromFullPathname() */

/*----------------------------------------------------------------------------
gaLog(): 		Log messages on stderr and/or a log file. 
----------------------------------------------------------------------------*/
int gaLog(char *moduleName, int debugLevel, char *message)
{
	static int	logFirstTime = 1;
	static int	rVal;
	FILE		*fpOut;
	time_t		myTime;
	char		today[20];
	char		timeBuf[64];
	char		outFile[256];
	char		trigger[256];
	static int	currentDebugLevel=0;
	struct tm 	*pTime;
	
	if ( debugLevel > 0 )
	{
		if(logFirstTime)
		{
			logFirstTime = 0;
			currentDebugLevel=gaGetDebugLevel();
		}

		if (debugLevel > currentDebugLevel)
			return(0);

	} /* if */
	
	time(&myTime);
	pTime = localtime(&myTime);
	strftime(timeBuf, sizeof(timeBuf)-1, "%H:%M:%S", pTime);
	strftime(today, sizeof(today)-1, "%y%m%d", pTime);
/*
	cftime(timeBuf, "%H:%M:%S", &myTime);
	cftime(today, "%y%m%d", &myTime);
*/
	if(logBaseLength == 0)
	{
		fprintf(stderr, gMsgFormat,
				resourceName,programName,getpid(),timeBuf,
				moduleName,message);
		fflush(stderr);
		return(0);
	}

	sprintf(outFile, "%s/log.%s.%s", logDir, logBase, today);

	if((fpOut = fopen(outFile, "a")) == NULL)
	{
		fprintf(stderr, "Failed to open %s for writing.\n", outFile);
		fprintf(stderr, gMsgFormat,
			resourceName,programName,getpid(),timeBuf,
			moduleName,message);
		fflush(stderr);
		return(-1);
	}
	fprintf(fpOut,gMsgFormat,resourceName,programName,getpid(),
						timeBuf,moduleName,message);
	fflush(fpOut);
	fclose(fpOut);
	return(rVal);
} /* END: gaLog() */
