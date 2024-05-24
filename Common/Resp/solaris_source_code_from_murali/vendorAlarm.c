/*------------------------------------------------------------------------------
File:		vendorAlarm.c

Purpose:	Determine whether a vendor alarm needs to be sent

Author:		Sandeep Agate

Update:		10/02/97 sja	Created the file
------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include "arc_snmpdefs.h"

//Update: 08/18/2009 ddn Replaced strtok with thread safe strtok_r

/* #define DEBUG */

#define ISP_DEBUG_NORMAL 	1
#define LogMsg(a,b,c) 	log_msg(__LINE__, a, b, c)
#define MAXSTRINGS	10
#define MAXSTRLEN	128

static char	vendorStrings[MAXSTRINGS][MAXSTRLEN];
static int	vendorMsgNo[MAXSTRINGS];

/* Global Function Prototypes */
int vendorAlarm();

/* Static Function Prototypes */
static int loadTraps(int *nStrings);
static int gaStripString(char *str);
static int ulReadLine(char *the_line, int maxlen, FILE *fp);

/* Externs */
extern int	errno;

char	failureReason[128];

/*------------------------------------------------------------------------------
vendorAlarm(): Do we need to send a vendor alarm?

Return Codes:	
0  	SUCCESS 	Determined whether alarm is to be sent
-1 	FAILURE 	Unable to determine whether alarm needs to be sent
------------------------------------------------------------------------------*/
int vendorAlarm()
{
	static short	tableLoaded = 0;
	static short	tableLoadError = 0;
	static long	lastFileSize 	= 0;
	static time_t	lastAccessTime 	= 0;
	static long	lastLineCount 	= 0;
	static char	lastLogfile[256] = "";
	static int	nStrings = 0;

	register int	i;
	long		newLineCount;
	int		tailLines;
	FILE		*pfp;
	struct stat	statBuf;
	char		logfile[256];
	char		cmd[256];
	char		line[256];
	char		theString[128];
	time_t		theTime;

	if(tableLoaded == 0)
	{
		tableLoaded = 1;
		if(loadTraps(&nStrings) < 0)
		{
			tableLoadError = 1;
			return(-1);
		}
	}

	if(tableLoadError)
	{
		sprintf(failureReason, "Vendor traps not loaded");
		LogMsg("vendorAlarm", ISP_DEBUG_NORMAL, failureReason);
		return(-1); 
	}
	if(nStrings <= 0)
	{
		return(0); 
	}

	time(&theTime);
 	cftime(logfile, "/usr/tvox/TSL%y%m%d", &theTime);
/*	cftime(logfile, "tsl", &theTime); */

	/*
	 * If logfile name has changed, reset all static variables. The file
	 * name will change with the day.
	 */
	if(strcmp(lastLogfile, logfile) != 0)
	{
#ifdef DEBUG
		fprintf(stderr, "File name has changed; old=%s; new=%s;\n",
						lastLogfile, logfile);
#endif
		lastFileSize 	= 0;
		lastAccessTime 	= 0;
		lastLineCount 	= 0;

		sprintf(lastLogfile, "%s", logfile);
	}

	/* Do we even have a logfile yet? */
	if(access(logfile, F_OK|R_OK) != 0)
	{
		return(0);
	}
	if(stat(logfile, &statBuf) < 0)
	{
		sprintf(failureReason, "Failed to stat %s; errno=%d (%s)",
					logfile, errno, strerrno(errno));
		LogMsg("vendorAlarm", ISP_DEBUG_NORMAL, failureReason);
		return(-1);
	}
	/*
	 * if file size has not increased, return. 
	 * NOTE: if file is truncated using the command 
	 * "$ > /usr/tvox/TSLyymmdd" the current size will be less than the 
	 * last saved size. In such a case, we will not process further.
	 */
	if(statBuf.st_size <= lastFileSize)
	{
		return(0);
	}

#ifdef DEBUG
	fprintf(stderr, "File size has increased; old=%ld; new=%ld;\n",
						lastFileSize, statBuf.st_size);
#endif
	lastFileSize = statBuf.st_size;

	/* Get line count. */
	memset(line, 0, sizeof(line));
	sprintf(cmd, "wc -l %s 2>/dev/null", logfile);
	if((pfp = popen(cmd, "r")) == NULL)
	{
		sprintf(failureReason, "popen(%s) failed; errno=%d (%s)",
					cmd, errno, strerrno(errno));
		LogMsg("vendorAlarm", ISP_DEBUG_NORMAL, failureReason);
		return(-1);
	}
	fgets(line, sizeof(line)-1, pfp);
	pclose(pfp);

	/* Did we get anything? */
	if(line[0] == '\0')
	{
		sprintf(failureReason, "Failed to get line count of %s",
								logfile);
		LogMsg("vendorAlarm", ISP_DEBUG_NORMAL, failureReason);
		return(-1);
	}

	sscanf(line, "%ld", &newLineCount);
	
	/*
	 * if line count has not increased, return. 
	 * NOTE: if file is truncated using the command 
	 * "$ > /usr/tvox/TSLyymmdd" the current size will be less than the 
	 * last saved size. In such a case, we will not process further.
	 */
	if(newLineCount <= lastLineCount)
	{
		return(0);
	}

#ifdef DEBUG
	fprintf(stderr, "Line count has increased; old=%ld; new=%ld;\n",
						lastLineCount, newLineCount);
#endif
	tailLines = newLineCount - lastLineCount;

	lastLineCount = newLineCount;

#ifdef DEBUG
	fprintf(stderr, "Going to tail %ld lines of >%s<\n", tailLines,logfile);
#endif

	sprintf(cmd, "tail -%ld %s 2>/dev/null", tailLines, logfile);
	if((pfp = popen(cmd, "r")) == NULL)
	{
		sprintf(failureReason, "popen(%s) failed; errno=%d (%s)",
					cmd, errno, strerrno(errno));
		LogMsg("vendorAlarm", ISP_DEBUG_NORMAL, failureReason);
		return(-1);
	}
	memset(line, 0, sizeof(line));
	while(fgets(line, sizeof(line)-1, pfp))
	{
		/* Remember: nStrings goes from 1 to n */
		for(i=0; i<nStrings; i++)
		{ 
			sprintf(theString, "%s", vendorStrings[i]);
#ifdef DEBUG
	fprintf(stderr, "Searching for >%s<\n", theString);
#endif
				
			if(strstr(line, theString) == (int)NULL)
			{
				continue;
			}
			/* Found the search string. Send an alarm. */
#ifdef DEBUG
			fprintf(stderr, "ALARM: %s\n", line);
			fprintf(stderr, "ALARM: msgNo is %d\n", vendorMsgNo[i]);
#endif
			line[strlen(line) - 1] = '\0';
			sprintf(failureReason, "Vendor Alarm Sent: %s", line);
			LogMsg("vendorAlarm", ISP_DEBUG_NORMAL, failureReason);
			check_and_send_snmp_trap(vendorMsgNo[i], line, 
								SNMPG_VENDOR);
		}
	}
	pclose(pfp);

	return(0);
} /* END: vendorAlarm() */
/*------------------------------------------------------------------------------
loadTraps(): Load traps

Return Codes:	
0  	SUCCESS 	Traps loaded
-1 	FAILURE 	Traps not loaded
------------------------------------------------------------------------------*/
static int loadTraps(int *nStrings)
{
	FILE	*fp;
	char	*ispbase, *ptr;
	char	cfgfile[256], line[256];
	char	msgNumber[10], msgString[MAXSTRLEN];
	int	index;
	char *yStrTok = NULL;

	*nStrings = 0;
	memset(vendorStrings, 0, sizeof(vendorStrings));

	if((ispbase = getenv("ISPBASE")) == (char *)NULL)
	{
		sprintf(failureReason, "Env. var. ISPBASE not set");
		LogMsg("loadTraps", ISP_DEBUG_NORMAL, failureReason);
		return(-1);
	}
 	sprintf(cfgfile, "%s/Global/Tables/snmp_arc_traps.cfg", ispbase);
/*	sprintf(cfgfile, "traps");		*/

	if((fp = fopen(cfgfile, "r")) == NULL)
	{
		sprintf(failureReason, "Failed to open traps cfg. file (%s) "
					"for reading, errno=%d (%s)",
					cfgfile, errno, strerrno(errno));
		LogMsg("loadTraps", ISP_DEBUG_NORMAL, failureReason);
		return(-1);
	}
	index = 0;
	while(ulReadLine(line, sizeof(line)-1, fp))
	{
		memset(msgString, 	0, sizeof(msgString));	
		memset(msgNumber,	0, sizeof(msgNumber));	

		ptr = (char *)strtok_r(line, " \t\n", &yStrTok);	/* msg number 	*/
		if(ptr == NULL)
		{
			continue;
		}
		sprintf(msgNumber, "%s", ptr);

		if(msgNumber[0] != '9')
		{
			continue;
		}

		ptr = (char *)strtok_r(NULL, " \t", &yStrTok);	/* service 	*/
		ptr = (char *)strtok_r(NULL, " \t", &yStrTok);	/* yesNo 	*/
		if(ptr == NULL)
		{
			continue;
		}

		if(toupper(ptr[0]) != 'Y')
		{
			continue;
		}

		ptr = (char *)strtok_r(NULL, " \t", &yStrTok);	/* mnemonic 	*/
		ptr = (char *)strtok_r(NULL, "\n", &yStrTok);	/* msgString 	*/
		if(ptr == NULL)
		{
			continue;
		}

		sprintf(msgString, "%s", ptr);
		gaStripString(msgString);
		sprintf(vendorStrings[index], "%s", msgString);
		sscanf(msgNumber, "%d", &vendorMsgNo[index]);

#ifdef DEBUG
		fprintf(stderr, "Found active msgString >%s<, msgNo %d\n", 
				vendorStrings[index], vendorMsgNo[index]);
#endif

		index++;
	} /* while */
	fclose(fp);

	*nStrings = index;

#ifdef DEBUG
	fprintf(stderr, "Loaded %d active vendor strings\n", *nStrings);
#endif
} /* END: loadTraps() */
/*--------------------------------------------------------------------
ulReadLine(): 	Read a line from a file pointer
--------------------------------------------------------------------*/
static int ulReadLine(char *the_line, int maxlen, FILE *fp)
{
	char	*ptr;
	char	mybuf[BUFSIZ];

	strcpy(the_line, "");
	while(fgets(mybuf,maxlen,fp))
	{
		if(!strcmp(mybuf,""))
			continue;
		mybuf[(int)strlen(mybuf)-1] = '\0';
		ptr = mybuf;
		while(isspace(*ptr++))
			;
		--ptr;
		if(!strcmp(ptr,""))
			continue;
		if(ptr[0] == '#')
			continue;
		strcpy(the_line, mybuf);
		return(1);
	} /* while */
	return((int)NULL);
} /* END: ulReadLine() */
/*--------------------------------------------------------------------
gaStripString():Strip the leading and trailing white space off of the
		string.
--------------------------------------------------------------------*/
static int gaStripString(char *str)
{
	char	*ptr;

	if(!(int)strlen(str))
		return(0);
	ptr = str;
	while(isspace(*(ptr++))) 
		;
	ptr--;
	strcpy(str, ptr);
	ptr = &str[(int)strlen(str)]-1;
	while(isspace(*ptr))
	{
		*ptr = '\0';	
		ptr--;
	}
	return(0);
} /* END: gaStripString() */
