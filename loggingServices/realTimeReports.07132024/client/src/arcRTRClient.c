/*-----------------------------------------------------------------------
Program:	arcRTRClient.c
Date:		12/03/2020
------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <dirent.h>
// #include <rpc/des_crypt.h>

#include "log_defs.h"
#include "gaUtils.h"
#include "sc.h"

#define	START_STR					"## START ##"
#define	END_MORE_COMING_LATER_STR	"## END_MORE_COMING_LATER ##"
#define	DONE_WITH_FILE_STR			"## DONE_WITH_FILE ##"
#define	EXITING_STR					"## EXITING ##"

#define EXIT_SLEEP				5
#define CDR_EOF					1
#define MAXBUF_LEN				4096

#define SUCCESS_EOF_REACHED				0
#define	FAILURE_SEND_RECEIVE_FAILED		-1
#define	FAILURE_HALTED_HAD_TO_STOP		-2
#define	FAILURE_DONE_NOT_SENT			-3
#define	FAILURE_CDR_FILE_ISSUE			-4

typedef struct
{
	char	destIP[128];
	int		delayOnEOF;
} Cfgs;

typedef struct
{
	char	cdrFileName[32];
	char	cdrFullPath[256];
	int		month;
	char	charMonth[32];
	int		day;
	int		year;
	int		hour;
	int		doy;
	long	offset;
	int		isItInThePast;
	int		doesItExist;
} CDRInfo;

typedef struct
{
	int		day;
	int		year;
	int		month;
	int		hour;
	int		doy;
} CurrentDateTime;

static Cfgs		gCfg;
static char		gUname[64];

char		*gIspBase;
char		gRTRDir[128];
char		gLogMsg[256];
char		gMarkerFile[256];
char		gSkippedFileList[256];
CDRInfo		gCDRInfo;
CurrentDateTime	gCurDateInfo;
int			gStartSent = 0;

static int			gKeepGoing = 1;
static int			gIsConnected = 0;

static pthread_t	processCDRThreadId;
static int			gCanProcessCDRThread = 0;

static int init();
static int dateToDayOfYear(int zMon, int zDay, int zYear, int *zDoy);
static int dayOfYearToDate(int zDoy, int zYear, int *zMon, char *zCharMonth, int *zDay);
static int establishServerConnection();
static int whereDoIStartFrom();
static int goFindFile(int zLine, char *zFileToFind, int *zIsItInThePast, int *zDoesItExist, char *zFullPath);
static int updateMarkerFile(int zLine, char *zFile, int zLastLine, int zJustClose);
static int getLineInLogFromCurrentTime(int *zLineNumber);
static void createCDRLogFromTime(int zMon, int zDay, int zYear, int zHour);
static void printCDRInfo(int zLine);
static void popFieldsFromLogFile(char *zLogFile);
static int processAllRecordsFromFile(int zLine);
static int isCurrentFileInThePast(int zLine);
static int setNextCDRFile();
void *mainThread();
static int scExitAndSleep();
static int processAllCDRFiles();
static int writeSkippedMsg(int zLine, char *zCDRFile, char *zMsg);
static int validateDir(char *zDir);
static void sendTheExit(int zLine);

/*---------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void exitClient()
{
	static char		mod[]="exitClient";
	char			line[64];
	int				rc;

	gKeepGoing = 0;

	rc = updateMarkerFile(__LINE__, "", 0, 1); // close it
	if ( gIsConnected )
	{
		sendTheExit(__LINE__);
		sc_Exit();
	}

	gaVarLog(mod, 1, "[%d] Sleeping 3...", __LINE__);
	sleep(3);

	gaVarLog(mod, 1, "[%d] Exiting.", __LINE__);
	exit(1);
} // END: exitClient()


/*---------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void sigExit(int arg)
{
	static char	mod[]="sigExit";
	char		sigStr[32];

	switch(arg)
	{
    	case SIGINT:
    		sprintf(sigStr, "%s", "SIGINT");
			break;
    	case SIGQUIT:
    		sprintf(sigStr, "%s", "SIGQUIT");
			break;
    	case SIGHUP:
    		sprintf(sigStr, "%s", "SIGHUP");
			break;
    	case SIGKILL:
    		sprintf(sigStr, "%s", "SIGKILL");
			break;
    	case SIGTERM:
    		sprintf(sigStr, "%s", "SIGTERM");
			break;
	}
	gaVarLog(mod, 1, "[%d] Received signal %s.  Exiting.", __LINE__, sigStr);

	exitClient();

} // END: sigExit()

int main(int argc, char *argv[])
{
	int					rc;
	
	static char		mod[]="main";
	static char		gaLogDir[256];
	
	int m, d, y=2020, doy;
	char		dateStr[64] = "";

	if(argc == 2 && (strcmp(argv[1], "-v") == 0))
	{
	    fprintf(stdout, 
	        "Aumtech's Real-Time Reporting Client (%s).\n"
	        "Version 5.0.  Compiled on %s %s.\n", argv[0], __DATE__, __TIME__);
	    exit(0);
	}
	
    signal(SIGINT, sigExit);
    signal(SIGQUIT, sigExit);
    signal(SIGHUP, sigExit);
    signal(SIGKILL, sigExit);
    signal(SIGTERM, sigExit);

	sprintf(gaLogDir,"%s", "/tmp");
	
	rc=gaSetGlobalString("$PROGRAM_NAME", argv[0]);
	rc=gaSetGlobalString("$LOG_DIR", gaLogDir);
	rc=gaSetGlobalString("$LOG_BASE", argv[0]);
	
	if ( (rc=init()) != 0 )
	{
		gaVarLog(mod, 0, "[%d] %d=init()", __LINE__, rc);

		gaVarLog(mod, 0, "[%d] Sleeping %d. Exiting.", __LINE__, EXIT_SLEEP);
		sleep(3);
		exit(0);
	}
	gaVarLog(mod, 1, "[%d] %d=init(). Starting.", __LINE__, rc);


	memset( (CDRInfo *)&gCDRInfo, '\0', sizeof(gCDRInfo));
	if ((rc = pthread_create(&processCDRThreadId, NULL,  mainThread, NULL)) != 0)
	{
		gaVarLog(mod, 0, "[%d] pthread_create(%d) failed. rc=%d. [%d, %s] "
					"Unable to create outbound thread.", __LINE__, 
					processCDRThreadId, rc, errno, strerror(errno));
	}

	gCanProcessCDRThread = 1;
	while ( gCanProcessCDRThread )
	{
		if ((rc = pthread_kill(processCDRThreadId, 0)) == ESRCH)
		{
			gCanProcessCDRThread = 0;
			break;
		}
		sleep(3);

	}

	exitClient();

} // END: main()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
void *mainThread()
{
	static char		mod[]="mainThread";
	int				rc;

	while ( gKeepGoing )
	{
		if ((rc = establishServerConnection()) != 0)	
		{
			gaVarLog(mod, 1, "[%d] Sleeping %d. Exiting.", __LINE__, EXIT_SLEEP);
			sleep(30);
			break;
		}
	
		rc = processAllCDRFiles();

		if ( gIsConnected )
		{
			sendTheExit(__LINE__);
			if ((rc = sc_Exit()) != sc_SUCCESS)
			{
				gaVarLog(mod, 0, "[%d] sc_Exit() failed. rc=%d", __LINE__, rc);
			}
			gIsConnected = 0;
		}

		sleep(3);
	}

	if ( gIsConnected )
	{
		sendTheExit(__LINE__);
		if ((rc = sc_Exit()) != sc_SUCCESS)
		{
			gaVarLog(mod, 0, "[%d] sc_Exit() failed. rc=%d.", __LINE__, rc);
		}
		gIsConnected = 0;
	}

	pthread_detach(pthread_self());
	pthread_exit(NULL);

} // END: mainThread()

/*---------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int processAllCDRFiles()
{
	static char		mod[]="processAllCDRFiles";
	int				rc;
	char			line[64];
	FILE			*fp;
	int				moreCDRFiles;
	int				keepLoopingInnerLoop;

	if ((rc = whereDoIStartFrom()) != 0)
	{
		rc=scExitAndSleep();
		gaVarLog(mod, 1, "[%d] whereDoIStartFrom  returned %d.", __LINE__, rc);
		return(-1);
	}
	printCDRInfo(__LINE__);
		
	moreCDRFiles=1;
	while ( moreCDRFiles )
	{
		if ( gCDRInfo.doesItExist == 1 )
		{
			rc = processAllRecordsFromFile(__LINE__);
			gaVarLog(mod, 1, "[%d] %d=processAllRecordsFromFile", __LINE__, rc);
		}
		else
		{
			if ( gCDRInfo.isItInThePast )
			{
				rc = writeSkippedMsg(__LINE__, gCDRInfo.cdrFileName, "Did not exist");
			}
			else
			{
				rc=scExitAndSleep();
				moreCDRFiles=0;
				continue;
			}
		}

		switch (rc)
		{
			case SUCCESS_EOF_REACHED:			// eof reached in current file
				if ( ! gCDRInfo.isItInThePast )	 // read to the current hour's log file
				{
					rc=scExitAndSleep();
					moreCDRFiles=0;
				}
				else
				{
					//
					// The CDR file is from the past
					//
					keepLoopingInnerLoop = 1;
					while (keepLoopingInnerLoop)
					{
						rc = setNextCDRFile();
						updateMarkerFile(__LINE__, gCDRInfo.cdrFileName, 0, 0);
						printCDRInfo(__LINE__);
						if ( gCDRInfo.doesItExist == 0 )
						{
							if ( gCDRInfo.isItInThePast )
							{
								rc = writeSkippedMsg(__LINE__, gCDRInfo.cdrFileName, "Did not exist");
								continue;
							}
							else
							{
								keepLoopingInnerLoop=0;
								moreCDRFiles=0;
								rc=scExitAndSleep();
							}
						}
						keepLoopingInnerLoop=0;
					}
				}
				break;
			case FAILURE_HALTED_HAD_TO_STOP:
				updateMarkerFile(__LINE__, gCDRInfo.cdrFileName, gCDRInfo.offset, 0);
				rc = writeSkippedMsg(__LINE__, gCDRInfo.cdrFileName, "Shutdown while sending.");
				moreCDRFiles=0;
				break;
			case FAILURE_CDR_FILE_ISSUE:
				if ( gCDRInfo.isItInThePast )
				{
					rc = writeSkippedMsg(__LINE__, gCDRInfo.cdrFileName, "Failed to open / does not exist.");
				}
				break;
			case FAILURE_SEND_RECEIVE_FAILED:
				moreCDRFiles=0;
				break;
			case FAILURE_DONE_NOT_SENT:
				gaVarLog(mod, 0, "[%d] Failed to send the DONE/END string for CDR file (%s).  The entire file was sent, "
					"but the entire file was sent.  Handle this manually.",
					__LINE__, gCDRInfo.cdrFileName);
				moreCDRFiles=0;
				break;
		}
	}

	return(0);

} // END: processAllCDRFiles()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int writeSkippedMsg(int zLine, char *zCDRFile, char *zMsg)
{
	FILE		*fp;
	static char	mod[]="writeSkippedMsg";
	time_t		t;
	struct tm	*pTime;
	char		curDateStr[64];
	
	time(&t);
	pTime = localtime(&t);
	gCurDateInfo.day	= pTime->tm_mday;
	gCurDateInfo.month	= pTime->tm_mon + 1;
	gCurDateInfo.year	= pTime->tm_year + 1900;
	gCurDateInfo.hour	= pTime->tm_hour;
	gCurDateInfo.doy	= pTime->tm_yday;

	sprintf(curDateStr, "%02d-%02d-%02d %02d:%02d:%02d", 
			pTime->tm_mon + 1, pTime->tm_mday, pTime->tm_year + 1900,
			pTime->tm_hour, pTime->tm_min, pTime->tm_sec);

	if((fp=fopen(gSkippedFileList, "a")) == NULL)
	{
		gaVarLog(mod, 0, "[%d, %d] Failed open file (%s) to update with CDR file (%s). [%d, %s]", zLine, __LINE__,
				gSkippedFileList, zCDRFile, errno, strerror(errno));
	}
	else
	{
		fprintf(fp, "%s | %s | %s\n", zCDRFile, curDateStr, zMsg);
		fclose(fp);
		gaVarLog(mod, 1, "[%d] Successfully updated (%s) with (%s)",
			__LINE__, gSkippedFileList, zCDRFile);
	}

	return(0);
} // END: writeSkippedMsg()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int scExitAndSleep()
{
	static char	mod[]="scExitAndSleep";
	int			rc;
	char		line[64];

	sendTheExit(__LINE__);
	if ((rc = sc_Exit()) != sc_SUCCESS)
	{
		gaVarLog(mod, 0, "[%d] sc_Exit() failed. rc=%d.", __LINE__, rc);
		return(-1);
	}
	gaVarLog(mod, 1, "[%d] sc_Exit() succeeded.", __LINE__);
	gIsConnected = 0;
	rc = updateMarkerFile(__LINE__, "", 0, 1); // close it
	gaVarLog(mod, 1, "[%d] Sleeping %d...", __LINE__, gCfg.delayOnEOF);
	sleep(gCfg.delayOnEOF);
	return(0);


} // END: scExitAndSleep()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int setNextCDRFile()
{
	static char	mod[]="setNextCDRFile";
	time_t		t;
	int			rc;
	int			keepTrying = 1;
	FILE		*fp;

	if ( gCDRInfo.hour < 23 )
	{
		gCDRInfo.hour++;
	}
	else
	{
		gCDRInfo.hour = 0;
		if ( gCDRInfo.month == 12 && gCDRInfo.day == 31 )
		{
			gCDRInfo.month = 1;
			sprintf(gCDRInfo.charMonth, "%s", "Jan");
			gCDRInfo.day = 1;
			gCDRInfo.year++;
			gCDRInfo.doy = 1;
		}
		else // next day
		{

			rc = dateToDayOfYear(gCDRInfo.month, gCDRInfo.day, gCDRInfo.year, &gCDRInfo.doy);
			gCDRInfo.doy++;
			rc = dayOfYearToDate(gCDRInfo.doy, gCDRInfo.year, &gCDRInfo.month, gCDRInfo.charMonth, &gCDRInfo.day);
		}
	}
	sprintf(gCDRInfo.cdrFileName, "CDR.%d-%s%02d-%02d.for",
			gCDRInfo.year, gCDRInfo.charMonth, gCDRInfo.day, gCDRInfo.hour);
	gCDRInfo.offset = 0;

	rc=isCurrentFileInThePast(__LINE__);

	rc = goFindFile(__LINE__, gCDRInfo.cdrFileName, 
						&gCDRInfo.isItInThePast, &gCDRInfo.doesItExist, gCDRInfo.cdrFullPath);
	return(0);
	
} // END: setNextCDRFile()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int isCurrentFileInThePast(int zLine)
{
	static char	mod[]="isCurrentFileInThePast";
	time_t		t;
	struct tm	*pTime;
	
	time(&t);
	pTime = localtime(&t);
	gCurDateInfo.day	= pTime->tm_mday;
	gCurDateInfo.month	= pTime->tm_mon + 1;
	gCurDateInfo.year	= pTime->tm_year + 1900;
	gCurDateInfo.hour	= pTime->tm_hour;
	gCurDateInfo.doy	= pTime->tm_yday;

	gaVarLog(mod, 1, "[%d, %d] Current date info: %d-%d-%d hour %d, day-of-year %d", zLine, __LINE__,
			gCurDateInfo.month, gCurDateInfo.day, gCurDateInfo.year, gCurDateInfo.hour, gCurDateInfo.doy);

	if ( gCurDateInfo.year > gCDRInfo.year )
	{
		gCDRInfo.isItInThePast = 1;
		return(1);
	}
	else if ( gCurDateInfo.year < gCDRInfo.year )
	{
		gaVarLog(mod, 1, "[%d, %d] Warning: Invalid current year (%d); future dates are not allowed. Defaulting to current time.",
			zLine, __LINE__,  gCDRInfo.year);
		createCDRLogFromTime(-1, 0, 0, 0); 
		gCDRInfo.isItInThePast = 0;
		return(0);
	}

	if ( gCurDateInfo.month > gCDRInfo.month )
	{
		gCDRInfo.isItInThePast = 1;
		return(1);
	} else if ( gCurDateInfo.month < gCDRInfo.month )
	{
		gaVarLog(mod, 1, "[%d, %d] Warning: Invalid current month (%d); future dates are not allowed. Defaulting to current time.",
			zLine, __LINE__,  gCDRInfo.month);
		createCDRLogFromTime(-1, 0, 0, 0); 
		gCDRInfo.isItInThePast = 0;
		return(0);
	}

	if ( gCurDateInfo.day > gCDRInfo.day )
	{
		gCDRInfo.isItInThePast = 1;
		return(1);
	}
	else if ( gCurDateInfo.day < gCDRInfo.day )
	{
		gaVarLog(mod, 1, "[%d, %d] Warning: Invalid current day (%d); future dates are not allowed. Defaulting to current time.",
			zLine, __LINE__,  gCDRInfo.day);
		createCDRLogFromTime(-1, 0, 0, 0); 
		gCDRInfo.isItInThePast = 0;
		return(0);
	}

	if ( gCurDateInfo.hour > gCDRInfo.hour )
	{
		gCDRInfo.isItInThePast = 1;
		return(1);
	}
	else if ( gCurDateInfo.hour < gCDRInfo.hour )
	{
		gaVarLog(mod, 1, "[%d, %d] Warning: Invalid current hour (%d); future dates are not allowed. Defaulting to current time.",
			zLine, __LINE__,  gCDRInfo.hour);
		createCDRLogFromTime(-1, 0, 0, 0); 
		gCDRInfo.isItInThePast = 0;
		return(0);
	}
	gCDRInfo.isItInThePast = 0;

	return(0);
} // END: isCurrentFileInThePast()

/*-----------------------------------------------------------------------------
processAllRecordsFromFile()
	Return values:
		0:	Success - all records sent to the end-of-file
		2:	Told from above to stop
		-1:	Failure - send data failed
		-7: Failure - could not open file
  ---------------------------------------------------------------------------*/
static int processAllRecordsFromFile(int zLine)
{
	static char		mod[]="processAllRecordsFromFile";
	static FILE		*fp = NULL;
	int				rc;
	int				i;
	char			line[MAXBUF_LEN + 1];
	int				counter;
	int				eof;
	char			smallStr[128];
	char			startStr[128];
	struct stat		statBuf;
	int				retCode = 0;
	int				bytesRead;
	if ( (rc = stat(gCDRInfo.cdrFullPath, &statBuf)) == -1)
	{
		gaVarLog(mod, 0, "[%d, %d] Failed stat() file (%s). [%d, %s] Unable to process.", zLine, __LINE__,
			gCDRInfo.cdrFullPath, errno, strerror(errno));
		return(FAILURE_CDR_FILE_ISSUE);
	}

	if ( statBuf.st_size == 0 )
	{
		return(0);		// empty file.  Nothing to process
	}

	if((fp=fopen(gCDRInfo.cdrFullPath, "r")) == NULL)
	{
		gaVarLog(mod, 0, "[%d, %d] Failed open file (%s). [%d, %s] Unable to process.", zLine, __LINE__,
			gCDRInfo.cdrFullPath, errno, strerror(errno));
		return(FAILURE_CDR_FILE_ISSUE);
	}
	sprintf(startStr, "%s|%s|%s", START_STR, gUname, gCDRInfo.cdrFileName);

	if ( gCDRInfo.offset > 0 )
	{
		if ( gCDRInfo.offset > statBuf.st_size )
		{
			if ( ! gCDRInfo.isItInThePast )
			{
				gaVarLog(mod, 1, "[%d, %d] offset(%ld) > size (%d) of file (%s).  "
						"Nothing to send; correcting offset and returning success.",
						zLine, __LINE__, gCDRInfo.offset, statBuf.st_size, gCDRInfo.cdrFullPath);
				updateMarkerFile(__LINE__, gCDRInfo.cdrFileName, statBuf.st_size, 0);
			}
			else
			{
				gaVarLog(mod, 1, "[%d, %d] offset(%ld) > size (%d) of file (%s).  "
						"Nothing to send; returning success.",
						zLine, __LINE__, gCDRInfo.offset, statBuf.st_size, gCDRInfo.cdrFullPath);
			}
			fclose(fp);
			return(0);
		}

		if ( gCDRInfo.offset == statBuf.st_size )
		{
			gaVarLog(mod, 1, "[%d, %d] offset(%ld) >= size (%d) of file (%s).  Nothing to send; returning success.",
					zLine, __LINE__, gCDRInfo.offset, statBuf.st_size, gCDRInfo.cdrFullPath);
			fclose(fp);
			return(SUCCESS_EOF_REACHED);
		}
		if ( (rc = fseek(fp, gCDRInfo.offset, SEEK_SET)) < 0 )
		{
			gaVarLog(mod, 0, "[%d, %d] fseek(%d, SEEK_SET) failed. Unable to set starting point in file (%s) Unable to process. [%d, %s] ",
				zLine, __LINE__, gCDRInfo.offset, gCDRInfo.cdrFullPath, errno, strerror(errno));
			fclose(fp);
			return(FAILURE_CDR_FILE_ISSUE);
		}
	}

	eof = 0; 
	gStartSent = 0;

	retCode = 0;
	counter=gCDRInfo.offset;
	memset((char *)line, '\0', sizeof(line));

	while ( ! eof )
	{
		// if ( fgets(line, sizeof(line), fp) != NULL)
		if ((bytesRead = fread(line, 1, MAXBUF_LEN, fp)) < 0)
		{
			gaVarLog(mod, 0, "[%d, %d] Failed to read data from (%s). rc=%d [%d, %s]", zLine, __LINE__,
							gCDRInfo.cdrFullPath, rc, errno, strerror(errno));
			eof = 1;
			retCode = FAILURE_SEND_RECEIVE_FAILED;
			break;
		}

		if ( bytesRead > 0 )
		{
			snprintf(smallStr, 40, "%s", line);
			gaVarLog(mod, 1, "[%d, %d] Read %d bytes (%s...)", zLine, __LINE__, bytesRead, smallStr);
			if ( ! gStartSent  )
			{
	 			if ((rc = sc_SendData(strlen(startStr), startStr)) != sc_SUCCESS)
				{
					gaVarLog(mod, 0, "[%d, %d] Failed to send data (%s) to (%s).  rc=%d", zLine, __LINE__,
							startStr, gCfg.destIP, rc);
					retCode = FAILURE_SEND_RECEIVE_FAILED;
					eof = 1;
					gKeepGoing = 0;
					continue;
				}
				gStartSent = 1;
				gaVarLog(mod, 1, "[%d, %d] Sent (%s)", zLine, __LINE__, startStr);
			}

			if ((rc = sc_SendData(strlen(line), line)) != sc_SUCCESS)
			{
				gaVarLog(mod, 0, "[%d, %d] Failed to send data (%s). rc=%d.  Shutting down.", zLine, __LINE__, smallStr, rc);
				eof = 1;
				gKeepGoing = 0;
				return(FAILURE_SEND_RECEIVE_FAILED);	// no need attempting to send the 'END'
			}
			else
			{
				counter += bytesRead;

				gaVarLog(mod, 1, "[%d, %d] Sent %d bytes.", zLine, __LINE__, bytesRead);
				updateMarkerFile(__LINE__, gCDRInfo.cdrFileName, counter, 0);
		
				if ( ! gKeepGoing )
				{
					gCDRInfo.offset = counter;
					gaVarLog(mod, 1, "[%d, %d] Breaking; gettting out. gKeepGoing=%d.", zLine, __LINE__, gKeepGoing);
					retCode=FAILURE_HALTED_HAD_TO_STOP;
					eof = 1;
					break;
				}
			}
			memset((char *)line, '\0', sizeof(line));
		}
		else
		{
			eof = 1;
			retCode = SUCCESS_EOF_REACHED;
			gaVarLog(mod, 1, "[%d, %d] %d bytes read from (%s). EOF reached.", zLine, __LINE__, bytesRead, gCDRInfo.cdrFullPath);
		}
	}
	
	fclose(fp);

	if ( ! gStartSent )
	{
		return(retCode);		// nothing was sent.  nevermind.
	}

	if ( ( ! gCDRInfo.isItInThePast ) || ( ! gKeepGoing ) )
	{
		sprintf(line, "%s", END_MORE_COMING_LATER_STR);
	}
	else
	{
		sprintf(line, "%s", DONE_WITH_FILE_STR);
	}
	if ((rc = sc_SendData(strlen(line), line)) != sc_SUCCESS)
	{
		gaVarLog(mod, 0, "[%d, %d] ERROR: Failed to END data (%s). rc=%d. ",
				zLine, __LINE__, line, rc);
		if ( retCode == 0 )
		{
			return(FAILURE_DONE_NOT_SENT);
		}
	}
	else
	{
		gaVarLog(mod, 1, "[%d, %d] Successfully sent end string (%s). rc=%d", zLine, __LINE__, line, rc);
	}

	return(retCode);

} // END: processAllRecordsFromFile()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int establishServerConnection()
{
	static char		mod[]="establishServerConnection";
	int				rc;
	struct utsname	myhost;
	char			buf[256];
    long 			recvdBytes;

	if ( ! gIsConnected )
	{
		if (( rc = sc_Init("arcRTRServer", gCfg.destIP)) != 0 )
		{
			gaVarLog(mod, 0, "[%d] Failed to initialize session [arcRTRServer / %s].  rc=%d.  Unable to continue.", __LINE__, gCfg.destIP,  rc);
			return(-1);
		}
		gIsConnected = 1;
		gaVarLog(mod, 1, "[%d] sc_Init() succeeded.", __LINE__);
	}

	memset(buf, 0, sizeof(buf));
	if((rc = uname(&myhost)) == -1)
	{
		gaVarLog(mod, 0, "[%d] Failed to get uname of system rc=%d. [%d, %s]  Unable to continue.", __LINE__,
			rc, errno, strerror(errno));
		return(-1);
	}
	sprintf(gUname, "%s", myhost.nodename);


	return(0);
} // END: establishServerConnection()

/*-----------------------------------------------------------------------------
	Sets the cdrFullPath and offset of the gCDRInfo structure.
	Return Values:
		0:	Success
		-1:	Failure
  ---------------------------------------------------------------------------*/
static int whereDoIStartFrom()
{
	static char		mod[]="whereDoIStartFrom";
	int				rc;
	char			filename[256];
	char			dirName[128];
	char			line[256];
	FILE			*fp;
	char			cdrFilename[64];
	char			charLastLine[32];
	char			*p;

	rc = validateDir(gRTRDir);


	if ( access(gMarkerFile, F_OK) == 0 )
	{
		if((fp=fopen(gMarkerFile, "r")) != NULL)
		{
			if ( fgets(line, sizeof(line), fp) != NULL)	// gMarkerFile exists and has data in it.
			{
				if ( line[strlen(line) - 1] == '\n' )
				{
					line[strlen(line) - 1] = '\0';
				}
				gaVarLog(mod, 1, "[%d] read (%s) from (%s)", __LINE__, line, gMarkerFile);
			
	
				if( (p = strstr(line, "|")) != (char *)NULL)
				{
					*p='\0';
					sprintf(gCDRInfo.cdrFileName, "%s", line);
					p++;
					sprintf(charLastLine, "%s", p);
					if ( charLastLine[strlen(charLastLine)-1] == '\n' )
					{
						charLastLine[strlen(charLastLine)-1] = '\0';
					}
					gCDRInfo.offset=atol(charLastLine);
					gaVarLog(mod, 1, "[%d] Set offset to %ld.", __LINE__, gCDRInfo.offset);
					fclose(fp);
					popFieldsFromLogFile(gCDRInfo.cdrFileName);
					rc = goFindFile(__LINE__, gCDRInfo.cdrFileName,
							&gCDRInfo.isItInThePast, &gCDRInfo.doesItExist, gCDRInfo.cdrFullPath);
					return(rc);
				}
				else
				{
					gaVarLog(mod, 0, "[%d] Warning: Invalid data (%s) found in (%s).  Assuming current hour.",
						__LINE__, line, gMarkerFile);
					
				}
			}
			else
			{
				gaVarLog(mod, 0, "[%d] There is no data in (%s).  Assuming current hour.", __LINE__, gMarkerFile);
			}
		}
		else
		{
			gaVarLog(mod, 0, "[%d] Failed open file (%s). [%d, %s] Assuming current hour.", __LINE__,
				gMarkerFile, errno, strerror(errno));
		}
		if ( fp != NULL )
		{
			fclose(fp);
		}
	}
	else
	{
		gaVarLog(mod, 0, "[%d] Failed access file (%s). [%d, %s] Assuming current hour.", __LINE__,
				gMarkerFile, errno, strerror(errno));
	}

	//
	createCDRLogFromTime(-1, 0, 0, 0);
	rc = goFindFile(__LINE__, gCDRInfo.cdrFileName, 
							&gCDRInfo.isItInThePast, &gCDRInfo.doesItExist, gCDRInfo.cdrFullPath);
	gCDRInfo.offset = 0;
	return(rc);

} // END: whereDoIStartFrom()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int getLineInLogFromCurrentTime(int *zLineNumber)
{
	static char		mod[]="getLineInLogFromCurrentTime";

	time_t		t;
	char		timeBuf[64];
	struct tm	*pTime;
	FILE		*fp;
	char		line[256];
	char		*p;
	int			counter=0;
	int			rc;

	time(&t);
	pTime = localtime(&t);
	strftime(timeBuf, sizeof(timeBuf)-1, "%C%y %H:%M:%S", pTime);

	gaVarLog(mod, 1, "[%d] Reading %s to get line for time %s", __LINE__, gCDRInfo.cdrFullPath, timeBuf);
	if((fp=fopen(gCDRInfo.cdrFullPath, "r")) == NULL)
	{
		gaVarLog(mod, 0, "[%d] Failed open file (%s). [%d, %s] Correct and retry", __LINE__,
			gMarkerFile, errno, strerror(errno));
		return(-1);
	}

	memset((char *)line, '\0', sizeof(line));
	*zLineNumber = 0;
	while ( fgets(line, sizeof(line), fp) != NULL)
	{
		p=&line[12];
			
//		gaVarLog(mod, 1, "[%d] strncmp((%s), (%s), %d)", __LINE__, timeBuf, p, strlen(timeBuf));
		if ( (rc =strncmp(timeBuf, p, strlen(timeBuf))) > 0)
		{
//			gaVarLog(mod, 1, "[%d] Yep. rc=%d  Got it - counter is %d.", __LINE__, rc, counter);
			*zLineNumber = counter;
			break;
		}
		counter++;
	}
	fclose(fp);

} // END: getLineInLogFromCurrentTime()

/*-----------------------------------------------------------------------------
  goFileFile:
  ---------------------------------------------------------------------------*/
static int goFindFile(int zLine, char *zFileToFind, int *zIsItInThePast, int *zDoesItExist, char *zFullPath)
{
	static char mod[]="goFindFile";
	int			rc;
	char		searchDirs[2][128];
	int			i;
	char		fullPath[256];
	char		noDorFor[256];
	char		*p;
	FILE		*fp;
	
	sprintf(searchDirs[0], "%s/LOG", gIspBase);
	sprintf(searchDirs[1], "%s/LOG/load/loaded.%d%02d%02d", gIspBase, gCDRInfo.year, gCDRInfo.month, gCDRInfo.day);
	*zFullPath = '\0';

	for (i=0; i<2; i++)
	{
		sprintf(fullPath, "%s/%s", searchDirs[i], gCDRInfo.cdrFileName);
		if (access (fullPath, F_OK|R_OK ) == 0 )
		{
			sprintf(zFullPath, "%s", fullPath);
			*zDoesItExist = 1;
			gaVarLog(mod, 1, "[%d %d] (%s) is found.", zLine, __LINE__, fullPath);
			return(0);
		}
		gaVarLog(mod, 1, "[%d, %d] (%s) not found.", zLine, __LINE__, fullPath);
	}

	for (i=0; i<2; i++)
	{
		// 
		// now adjust for the .for extension and search
		//
		if ( strstr(gCDRInfo.cdrFileName, ".for" ) )		// the .for is there, remove it.
		{
			sprintf(fullPath, "%s/%s", searchDirs[i], gCDRInfo.cdrFileName);
			p=rindex(fullPath, '.');
			*p='\0';
		}
		else
		{			// the .for is not there, so add it.
			sprintf(fullPath, "%s/%s.for", searchDirs[i], gCDRInfo.cdrFileName);
		}
	
		if (access (fullPath, F_OK|R_OK ) == 0 )
		{
			sprintf(zFullPath, "%s", fullPath);
			*zDoesItExist = 1;
			p=rindex(gCDRInfo.cdrFileName, '.');
			*p='\0';
			gaVarLog(mod, 1, "[%d, %d] (%s) is found.", zLine, __LINE__, fullPath);

			return(0);
		}
		gaVarLog(mod, 1, "[%d, %d] (%s) not found.", zLine, __LINE__, fullPath);
	}

	*zDoesItExist = 0;

	return(0);	// 
} // END: goFindFile()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void popFieldsFromLogFile(char *zLogFile)
{
	static char		mod[]="popFieldsFromLogFile";
	int				rc;
	char			*p;
	char			buf[64];
	char			buf2[32];
	
	sprintf(buf, "%s", zLogFile);
	
	sprintf(buf2, "%s", zLogFile+4);
	p=buf2+4;
	*p = '\0';
	gCDRInfo.year = atoi(buf2);
	p++;

	sprintf(gCDRInfo.charMonth, "%s", p);
	gCDRInfo.charMonth[3] = '\0';

	p+=3;

	sprintf(buf2, "%s", p);
	p+=2;
	*p='\0';
	gCDRInfo.day=atoi(buf2);


	p++;
	sprintf(buf, "%s", p);
	p+=2;
	if ( *p != '\0' )
	{
		*p='\0';
	}
	gCDRInfo.hour=atoi(buf);
	
	if ( strcmp(gCDRInfo.charMonth, "Jan") == 0 )
	{
		gCDRInfo.month = 1;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Feb") == 0 )
	{
		gCDRInfo.month = 2;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Mar") == 0 )
	{
		gCDRInfo.month = 3;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Apr") == 0 )
	{
		gCDRInfo.month = 4;
	}
	else if ( strcmp(gCDRInfo.charMonth, "May") == 0 )
	{
		gCDRInfo.month = 5;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Jun") == 0 )
	{
		gCDRInfo.month = 6;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Jul") == 0 )
	{
		gCDRInfo.month = 7;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Aug") == 0 )
	{
		gCDRInfo.month = 8;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Sep") == 0 )
	{
		gCDRInfo.month = 9;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Oct") == 0 )
	{
		gCDRInfo.month = 10;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Nov") == 0 )
	{
		gCDRInfo.month = 11;
	}
	else if ( strcmp(gCDRInfo.charMonth, "Dec") == 0 )
	{
		gCDRInfo.month = 12;
	}

	rc = dateToDayOfYear(gCDRInfo.month, gCDRInfo.day, gCDRInfo.year, &gCDRInfo.doy);

	rc = isCurrentFileInThePast(__LINE__); // sets isItInThePast
	return;
} // END: popFieldsFromLogFile

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void createCDRLogFromTime(int zMon, int zDay, int zYear, int zHour)
{
	static char	mod[]="createCDRLogFromTime";
	time_t		t;
	char		timeBuf[64];
	struct tm	*pTime;

	char	charMonth[32];
	if (zMon == -1)
	{
		time(&t);
		pTime = localtime(&t);
		strftime(timeBuf, sizeof(timeBuf)-1, "CDR.%C%y-%b%d-%H.for", pTime);
		sprintf(gCDRInfo.cdrFileName, "%s", timeBuf);
		gCDRInfo.month = pTime->tm_mon + 1;
		gCDRInfo.day = pTime->tm_mday;
		gCDRInfo.year = pTime->tm_year + 1900;
		gCDRInfo.hour = pTime->tm_hour;

		switch(gCDRInfo.month)
		{
			case 1: sprintf(gCDRInfo.charMonth, "%s", "Jan");
					break;
			case 2: sprintf(gCDRInfo.charMonth, "%s", "Feb");
					break;
			case 3: sprintf(gCDRInfo.charMonth, "%s", "Mar");
					break;
			case 4: sprintf(gCDRInfo.charMonth, "%s", "Apr");
					break;
			case 5: sprintf(gCDRInfo.charMonth, "%s", "May");
					break;
			case 6: sprintf(gCDRInfo.charMonth, "%s", "Jun");
					break;
			case 7: sprintf(gCDRInfo.charMonth, "%s", "Jul");
					break;
			case 8: sprintf(gCDRInfo.charMonth, "%s", "Aug");
					break;
			case 9: sprintf(gCDRInfo.charMonth, "%s", "Sep");
					break;
			case 10: sprintf(gCDRInfo.charMonth, "%s", "Oct");
					break;
			case 11: sprintf(gCDRInfo.charMonth, "%s", "Nov");
					break;
			case 12: sprintf(gCDRInfo.charMonth, "%s", "Dec");
					break;
		}


		return;
	}
	
	
	

} // END: createCDRLogFromTime()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int dateToDayOfYear(int zMon, int zDay, int zYear, int *zDoy)
{
	static char	mod[]="dateToDayOfYear";

	int		daysInFeb = 28;
   // check for leap zYear

	*zDoy=zDay;

    if( (zYear % 4 == 0 && zYear % 100 != 0 ) || (zYear % 400 == 0) )
    {
        daysInFeb = 29;
    }

    switch(zMon)
    {
        case 2:
            *zDoy += 31;
            break;
        case 3:
            *zDoy += 31+daysInFeb;
            break;
        case 4:
            *zDoy += 31+daysInFeb+31;
            break;
        case 5:
            *zDoy += 31+daysInFeb+31+30;
            break;
        case 6:
            *zDoy += 31+daysInFeb+31+30+31;
            break;
        case 7:
            *zDoy += 31+daysInFeb+31+30+31+30;
            break;            
        case 8:
            *zDoy += 31+daysInFeb+31+30+31+30+31;
            break;
        case 9:
            *zDoy += 31+daysInFeb+31+30+31+30+31+31;
            break;
        case 10:
            *zDoy += 31+daysInFeb+31+30+31+30+31+31+30;            
            break;            
        case 11:
            *zDoy += 31+daysInFeb+31+30+31+30+31+31+30+31;            
            break;                        
        case 12:
            *zDoy += 31+daysInFeb+31+30+31+30+31+31+30+31+30;            
            break;                                    
    }

    return(0);
} // END: dateToDayOfYear()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int dayOfYearToDate(int zDoy, int zYear, int *zMon, char *zCharMonth, int *zDay)
{
	static char	mod[]="dateToDayOfYear";
	int		leapIdx = 0;
	int		i;
   // check for leap zYear

    static const int days[2][12] = {
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
    };

	static char mons[12][8] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" }; 

    if( (zYear % 4 == 0 && zYear % 100 != 0 ) || (zYear % 400 == 0) )
    {
        leapIdx = 1;
    }

	for (i=11; i>=0; i--)
	{
		if ( zDoy > days[leapIdx][i] )
		{
			*zMon = i+1;
			*zDay = zDoy - days[leapIdx][i];
			sprintf(zCharMonth, "%s", mons[i]);
			return(0);
		}
	}

	return(0);
	
} // END: dayOfYearToDate()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int init()
{
	static char mod[]="init";
	int			rc;
	char		filename[128];
	char		ispbase[32]="ISPBASE";
	char		name[64];
	char		value[64];
	char		defaultValue[64];
	static char	gaLogDir[256];

    if ((gIspBase = getenv (ispbase)) == NULL)
    {
		gaVarLog(mod, 0, "[%d] Unable to get ISPBASE env variable. Set it and retry.  Unable to continue.", __LINE__);
        return (-1);
    }
	sprintf(gaLogDir, "%s/LOG", gIspBase);
	rc=gaSetGlobalString("$LOG_DIR", gaLogDir);

	memset((Cfgs *)&gCfg, '\0', sizeof(gCfg));

	if ( gIspBase[strlen(gIspBase) - 1] == '/' )
	{
		gIspBase[strlen(gIspBase) - 1] = '\0';
	}
	sprintf(filename, "%s/Global/.Global.cfg", gIspBase);

	sprintf(name, "%s", "LOG_PRIMARY_SERVER");
	memset((char *)defaultValue, '\0', sizeof(defaultValue));
	memset((char *)value, '\0', sizeof(value));
	if ((rc = gaGetProfileString ("Settings", name, defaultValue, value, sizeof(value), filename)) != 0)
	{
		gaVarLog(mod, 0, "[%d] Unable to get LOG_PRIMARY_SERVER from %s.  Cannot continue; correct and retry.",
										__LINE__, filename);
		return(-1);
	}
	sprintf(gCfg.destIP, "%s", value);
	gaVarLog(mod, 1, "[%d] gCfg.destIP:(%s)", __LINE__, gCfg.destIP);
	
	sprintf(name, "%s", "LOG_CDR_FILE");
	sprintf(defaultValue, "%s", "YES_BOTH");
	memset((char *)value, '\0', sizeof(value));
	if ((rc = gaGetProfileString ("Settings", name, defaultValue, value, sizeof(value), filename)) == 0)
	{
		if (strncmp(value, "YES", 3) != 0 )
		{
			gaVarLog(mod, 0, "[%d] LOG_CDR_FILE in %s is not enabled. Real-time reports are disabled.", __LINE__, filename);
			return(-1);
		}
	}
	
	sprintf(name, "%s", "REALTIME_REPORTS");
	sprintf(defaultValue, "%s", "YES");
	memset((char *)value, '\0', sizeof(value));
	if ((rc = gaGetProfileString ("Settings", name, defaultValue, value, sizeof(value), filename)) == 0)
	{
		if (strncmp(value, "YES", 3) != 0 )
		{
			gaVarLog(mod, 0, "[%d] REALTIME_REPORTS in %s is not enabled.", __LINE__, filename);
			return(-1);
		}
	}

	sprintf(name, "%s", "RTR_DELAY_ON_EOF");
	sprintf(defaultValue, "%s", "120");
	memset((char *)value, '\0', sizeof(value));
	if ((rc = gaGetProfileString ("Settings", name, defaultValue, value, sizeof(value), filename)) != 0)
	{
		gaVarLog(mod, 0, "[%d] Failed to get delayOnEOF from %s. Defaulting to (%s).", __LINE__, filename, defaultValue);
		gCfg.delayOnEOF = atoi(value);
	}
	else
	{
		gCfg.delayOnEOF = atoi(value);
		gaVarLog(mod, 1, "[%d] gCfg.delayOnEOF:(%d)", __LINE__, gCfg.delayOnEOF);
	}


	sprintf(filename, "%s/Global/Tables/realtimeReports.cfg", gIspBase);

	sprintf(gRTRDir, "%s/RTR", gIspBase);
	if ( (rc = validateDir(gRTRDir)) != 0 )
	{
		gaVarLog(mod, 0, "[%d] Failed to validate/create directory (%s). Unable to continue; correct and retry.", __LINE__);
		return(-1);
	}

	sprintf(gMarkerFile, "%s/.rtrCDRFileMarker.txt", gRTRDir);
	sprintf(gSkippedFileList, "%s/skippedCDRFileList.txt", gRTRDir);

	return(0);


} // END: init()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void sendTheExit(int zLine)
{
	static char		mod[]="sendTheExit";
	int				rc;
	char			buf[128];
	
	if ( ( ! gIsConnected ) || ( ! gStartSent ) )
	{
		return;
	}

	sprintf(buf, "%s", EXITING_STR);
	if ((rc = sc_SendData(strlen(buf), buf)) != sc_SUCCESS)
	{
		gaVarLog(mod, 0, "[%d, %d] Failed to exit string (%s) to (%s).  rc=%d", zLine, __LINE__,
						buf, gCfg.destIP, rc);
		return;
	}

	gaVarLog(mod, 1, "[%d, %d] Sent (%s).", zLine, __LINE__, buf);
	gStartSent = 0;
	return;

} // END: sendTheExit()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int updateMarkerFile(int zLine, char *zFile, int zLastLine, int zJustClose)
{
	static char	mod[]="updateMarkerFile";
	static FILE	*fp = NULL;

	if ( zJustClose )
	{
		if ( fp )
		{
			fclose(fp);
			fp = NULL;
		}
		return(0);
	}

	if ( ! fp )
	{
		if((fp=fopen(gMarkerFile, "w")) == NULL)
		{
			gaVarLog(mod, 0, "[%d, %d] Failed open file (%s) to update. [%d, %s]", zLine, __LINE__,
				gMarkerFile, errno, strerror(errno));
			return(-1);
		}
	}
	else
	{
		rewind(fp);
	}

	fprintf(fp, "%s|%d     \n", zFile, zLastLine);
	gaVarLog(mod, 1, "[%d, %d] Successfully updated (%s) to (%s|%d).", zLine, __LINE__,
				gMarkerFile, zFile, zLastLine);

	return(0);
} // END: updateMarkerFile()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int validateDir(char *zDir)
{
	static char	mod[]="validateDir";

	if (access (zDir, F_OK|R_OK ) < 0)
	{
		if(errno == ENOENT)
		{
			if (mkdir(zDir, 0755) < 0)
			{
				gaVarLog(mod, 0, "[%d] mkdir (%s) failed. Unable to create directory. [%d, %s].  Correct and retry.", __LINE__,
						zDir, errno, strerror(errno));
				return(-1);
			}
		}
		else
		{
			gaVarLog(mod, 0, "[%d] Unable to access directry (%s). [%d, %s].  Correct and retry.", __LINE__,
						zDir, errno, strerror(errno));
			return(-1);
		}
	}
	return(0);
} // END: validateDir()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void printCDRInfo(int zLine)
{
	static char mod[]="printCDRInfo";

	gaVarLog(mod, 1, "[%d] [%d] cdrFile(%s) fullPath(%s) doesItExist(%d) "
			"month(%d:%s) day(%d) year(%d) hour(%d) doy(%d) offset(%ld) isItInThePast(%d) ", __LINE__, zLine,
			gCDRInfo.cdrFileName, gCDRInfo.cdrFullPath, gCDRInfo.doesItExist, gCDRInfo.month, gCDRInfo.charMonth, gCDRInfo.day,
			gCDRInfo.year, gCDRInfo.hour, gCDRInfo.doy, gCDRInfo.offset, gCDRInfo.isItInThePast);


} // END: printCDRInfo();
