/*-----------------------------------------------------------------------
Program:	arcRTRServer.c
Author:		Aumtech, Inc
Date:		12-9-2020
-----------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
// #include <rpc/des_crypt.h>

#include "ss.h"
#include "gaUtils.h"

#include "log_defs.h"

#define START_STR					"## START ##"
#define END_MORE_COMING_LATER_STR	"## END_MORE"			// partial string
#define DONE_WITH_FILE_STR			"## DONE_WITH"	 		// partial string
#define EXITING_STR					"## EXITING" 		 		// partial string

#define START_CMD       1
#define END_CMD         2
#define DONE_CMD        3
#define EXIT_CMD        4

#define MAXBUF	4096

//#define	DIRECTORY_CREATION_PERMS	0755
//#define	OPEN_MASK			0000

/* #define DEBUG */
//static char	LogFile1[PATHSZ];
//static char	LogFile2[PATHSZ];
//static char	Dir1[PATHSZ];
//static char	BaseDir[PATHSZ];
//char		msgbuf[256];


typedef struct
{
	char		workDir[128];
	char		processedDir[128];
	char		uname[32];
	char		filename[128];
	char		adjustedFilename[256];
	char		shadowFilename[256];
	char		prevShadowFilename[256];
	int			shadowIndex;
	int			command;			// 1=start, 0=stop
} SInfo;

char		*gIspBase;
SInfo		sInfo;

/*---------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void exitServer()
{
	static char		mod[]="exitServer";
	char			line[64];
	int				rc;

	if ((rc = ss_Exit()) != 0)
	{
		gaVarLog(mod, 0,"[%d] ss_Exit() failed.  rc=%d. Exiting.", __LINE__, rc);
	}
	else
	{
		gaVarLog(mod, 1,"[%d] ss_Exit() succeeded.  rc=%d. Exiting.", __LINE__, rc);
	}

	sleep(2);

	gaVarLog(mod, 1, "[%d] Exiting.", __LINE__);
	exit(1);
} // END: exitServer()

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
    	case SIGABRT:
    		sprintf(sigStr, "%s", "SIGABRT");
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

	exitServer();

} // END: sigExit()

//void		Exit();
//static int	lValidateDirectory(char *iDirectory, char *oErrorMsg);
static int parseCmdStr(char *zData);
static int 	init();
static int 	processFiles();
static int createAdjustedFilename();
static void printSInfo();
static int storeWorkFile(int zLine, char *zWorkFile, char *zProcessedFile);
static int handleShadowFile(int zLine, char *zData);

main(int argc, char *argv[])
{
	static char	mod[]="main";
	char		databuf[256];

	int			rc, i;
	long		recvdBytes;

	static char	gaLogDir[256];
	
	if(argc == 2 && (strcmp(argv[1], "-v") == 0))
	{
		fprintf(stdout, 
			"Aumtech's Real-time Reporting Server (%s).\n"
			"Version 3.7.  Compiled on %s %s.\n", argv[0], __DATE__, __TIME__);
		exitServer();
	}
	sprintf(gaLogDir,"%s/LOG", (char *)getenv("ISPBASE"));
	rc=gaSetGlobalString("$PROGRAM_NAME", argv[0]);
	rc=gaSetGlobalString("$LOG_DIR", gaLogDir);
	rc=gaSetGlobalString("$LOG_BASE", argv[0]);

	signal(SIGABRT, sigExit);
    signal(SIGINT, sigExit);
    signal(SIGQUIT, sigExit);
    signal(SIGHUP, sigExit);
    signal(SIGKILL, sigExit);
    signal(SIGTERM, sigExit);

	if ((rc = init()) != 0)
	{
		exitServer();
	}
	
	if ((rc = ss_Init(argc, argv)) != ss_SUCCESS)
	{
		gaVarLog(mod, 0,"[%d] ss_Init() failed.  rc=%d.", __LINE__, rc);
		exitServer();
	}
	gaVarLog(mod, 1,"[%d] ss_Init() succeeded.  rc=%d.", __LINE__, rc);
	
	rc = processFiles();

	exitServer();
	
}

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int processFiles()
{
	static char	mod[]="processFiles";
	char		data[MAXBUF+1024];
	long		numBytes;
	char		*p;
	int			rc;
	FILE		*fpAdjusted = NULL;
	char		workFile[256];
	char		processedFile[256];
	int			getMoreDataForFile;
	int			exitOuterLoop;
	int			gotSomething;

	exitOuterLoop = 0;
	while ( ! exitOuterLoop )
	{
		memset(data, 0, sizeof(data));
		gaVarLog(mod, 1,"[%d] Calling ss_RecvData() for command.", __LINE__);
		rc=ss_RecvData(180, 512, data, &numBytes);
		if ( ( rc == -2 ) || ( numBytes == 0 ) )
		{
			gaVarLog(mod, 1,"[%d]  ss_RecvData() timed out. Exiting.", __LINE__);
			return(-1);
		}
		if ( rc < 0 ) 
		{
			gaVarLog(mod, 0,"[%d]  ss_RecvData() failed.  rc=%d", __LINE__, rc);
			break;
		}

		gaVarLog(mod, 1,"[%d] Successfully received (%d:%s) from server.", __LINE__, numBytes, data);
	
		if ( (rc = parseCmdStr(data)) != 0)
		{
			; // TODO: handle error
		}
	
		if ( sInfo.command == START_CMD)
		{
			if ( (rc = createAdjustedFilename()) != 0 )
			{
				; // TODO: handle error
			}
			printSInfo();
	
			sprintf(workFile, "%s/%s", 
					sInfo.workDir, sInfo.adjustedFilename);
			sprintf(processedFile, "%s/%s", 
					sInfo.processedDir, sInfo.adjustedFilename);
			if ((fpAdjusted = fopen(workFile, "w")) == NULL)
			{
				gaVarLog(mod, 0, "[%d] Unable to open %s for output.  [%d, %s] ",
					"Unable to receive (%s).", __LINE__,  errno, strerror(errno), workFile);
				return(-1);
			}
			gaVarLog(mod, 1, "[%d] Successfully opened destination file (%s).", __LINE__, workFile);
	
			// Now receive the data and write it
			getMoreDataForFile=1;
			gotSomething = 0;

			while (getMoreDataForFile)
			{
				memset(data, 0, sizeof(data));

				if ((rc=ss_RecvData(180, sizeof(data), data, &numBytes)) != ss_SUCCESS)
				{
					gaVarLog(mod, 0,"[%d] ss_RecvData() of transfer type failed.  rc=%d.", __LINE__, rc);
					if ( fpAdjusted ) 
					{
						fclose(fpAdjusted);
						fpAdjusted = NULL;
						gaVarLog(mod, 1,"[%d] Closed (%s).", __LINE__, sInfo.adjustedFilename);
					}
					if ( gotSomething )
					{
						if ( (rc = storeWorkFile(__LINE__, workFile, processedFile)) != 0 )
						{
							gaVarLog(mod, 0,"[%d] CRITICAL - Workfile (%s) not handled properly. Requires attention. ", __LINE__, workFile);
							return(-1);
						}
						
						sprintf(data, "%s", END_MORE_COMING_LATER_STR);
						if ((rc = handleShadowFile(__LINE__, data)) != 0)
						{
							gaVarLog(mod, 0,"[%d] CRITICAL - Shadow file not handled properly. Requires attention. ", __LINE__);
						}
					}
					return(-1);
				}
				gotSomething = 1;
				if ( numBytes < 150 )
				{
					gaVarLog(mod, 1,"[%d] Successfully received (%s:%d) bytes from server.", __LINE__, data, numBytes);
					if ( (strstr(data, END_MORE_COMING_LATER_STR))  ||
					     (strstr(data, DONE_WITH_FILE_STR)) ||
					     (strstr(data, EXITING_STR)) )
					{
						// we have the end string.  close it up.
						if ( fpAdjusted )
						{
							fclose(fpAdjusted);
							fpAdjusted = NULL;
							gaVarLog(mod, 1,"[%d] Closed (%s).", __LINE__, sInfo.adjustedFilename);
						}

						if ( (rc = storeWorkFile(__LINE__, workFile, processedFile)) != 0 )
						{
							gaVarLog(mod, 0,"[%d] CRITICAL - Workfile (%s) not handled properly. Requires attention. ", __LINE__, workFile);
							return(-1);
						}
						
						if ((rc = handleShadowFile(__LINE__, data)) != 0)
						{
							gaVarLog(mod, 0,"[%d] CRITICAL - Shadow file not handled properly. Requires attention. ", __LINE__, workFile);
						}

						if ( strstr(data, EXITING_STR ) )
						{
							getMoreDataForFile = 0;
							exitOuterLoop = 1;
						}
						else
						{
							// DONE_WITH_FILE_STR - get out of this loop
							getMoreDataForFile = 0;
						}
		
					}
					else
					{
						fprintf(fpAdjusted, "%s", data);	// this is CDR data
					}
				}
				else
				{
					gaVarLog(mod, 1,"[%d] Successfully received %d bytes from server.", __LINE__, numBytes);
					fprintf(fpAdjusted, "%s", data);	// this is CDR data
				}
			}
		}
	}

	return(0);
} // END: processFiles

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int handleShadowFile(int zLine, char *zData)
{
	static char	mod[]="handleShadowFile";
	int			rc;
	char		shadowFile[256];
	FILE		*fpShadow = NULL;

	if ( sInfo.prevShadowFilename[0] != '\0' )
	{
		sprintf(shadowFile, "%s/%s", sInfo.workDir, sInfo.prevShadowFilename);
		if ( access(shadowFile, F_OK) == 0 )
		{
			unlink(shadowFile);
			gaVarLog(mod, 1, "[%d] Removed (%s).", __LINE__, shadowFile);
		}
	}

	if ( ( strstr(zData, END_MORE_COMING_LATER_STR ) ) ||	// Not done with the file.
	     ( strstr(zData, EXITING_STR ) ) )
	{
		sprintf(shadowFile, "%s/%s", sInfo.workDir, sInfo.shadowFilename);
		if ((fpShadow = fopen(shadowFile, "w")) == NULL)
		{
			gaVarLog(mod, 0, "[%d] Unable to open (%s) for output.  [%d, %s] ", __LINE__,
					"Unable to create shadow file (%s)", shadowFile);
		}
		else
		{
			fclose(fpShadow);
			fpShadow = NULL;
		}
	}

	gaVarLog(mod, 1, "[%d] Created shadow file (%s).", __LINE__, shadowFile);
	return(0);

} // END: handleShadowFile()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int storeWorkFile(int zLine, char *zWorkFile, char *zProcessedFile)
{
	int			rc;
	static char	mod[]="storeWorkFile";
	char		shadowFile[256];

	if ( access(zWorkFile, F_OK) < 0 )
	{
		gaVarLog(mod, 0, "[%d, %d] Failed to access workFile (%s). Unable to move to processed directory. [%d, %s]", __LINE__, zLine,
					zWorkFile, errno, strerror(errno));
		return(-1);
	}

	if ( (rc = rename(zWorkFile, zProcessedFile)) != 0)
	{
		gaVarLog(mod, 0, "[%d] Failed to rename (%s) to (%s).  [%d, %s]", __LINE__,
					zWorkFile, zProcessedFile, errno, strerror(errno));
		return(-1);
	}
	gaVarLog(mod, 1, "[%d] Moved (%s) to (%s).", __LINE__, zWorkFile, zProcessedFile);

	return(0);

} // END: storeWorkFile()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int createAdjustedFilename()
{
	static char		mod[]="createAdjustedFilename";
	int				rc;
 	DIR				*dir;
	struct dirent	*dirEntry;     
	char			searchFile[256];
	int				index;
	char			*p;

	if ( (dir = opendir(sInfo.workDir)) == NULL)
	{
		gaVarLog(mod, 0, "[%d] Unable to open directory (%s).  Cannot process (%s)", __LINE__,
			sInfo.workDir, sInfo.filename);
		return(-1);
	}

	//
	// ex.  looking for shadow.testsys.CDR.2020-Dec07-13.for.<index>
	//
	sprintf(searchFile, "shadow.%s.%s", sInfo.uname, sInfo.filename);
	index = -1;
	gaVarLog(mod, 1, "[%d] Searching %s for %s", __LINE__, sInfo.workDir, searchFile);
    while (dirEntry = readdir (dir))
	{
		if (strstr(dirEntry->d_name, searchFile) != (char *)NULL)
		{
			// found it.  Now get the index.
			p=strrchr(dirEntry->d_name, '.');
			p++;
			index=atoi(p);
			break;
		}
	}
    (void) closedir (dir);

	if ( index == -1 ) // not found
	{
		sInfo.shadowIndex = 0;
		sprintf(sInfo.adjustedFilename, "%s.%s.%d",
				sInfo.uname, sInfo.filename, sInfo.shadowIndex);
		sprintf(sInfo.shadowFilename, "shadow.%s", sInfo.adjustedFilename);
		sInfo.prevShadowFilename[0] = '\0';
		gaVarLog(mod, 1, "[%d] Not found - sInfo.adjustedFilename(%s)", __LINE__, sInfo.adjustedFilename);
	}
	else
	{
		sInfo.shadowIndex = index + 1;
		sprintf(sInfo.adjustedFilename, "%s.%s.%d",
				sInfo.uname, sInfo.filename, sInfo.shadowIndex);

		sprintf(sInfo.shadowFilename, "shadow.%s", sInfo.adjustedFilename);
		sprintf(sInfo.prevShadowFilename, "shadow.%s.%s.%d",
				sInfo.uname, sInfo.filename, sInfo.shadowIndex - 1);
		gaVarLog(mod, 1, "[%d] Found - index=%d, sInfo.shadowIndex=%d  sInfo.adjustedFilename(%s) "
						"sInfo.prevShadowFilename(%s)", __LINE__, 
				index, sInfo.shadowIndex, sInfo.adjustedFilename, sInfo.prevShadowFilename);
	}

	return(0);
} // END: createAdjustedFilename()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int parseCmdStr(char *zData)
{
	static char	mod[]="parseCmdStr";
	char		buf[256];
	char		*p;

	sprintf(buf, "%s", zData);

	if ( strstr(buf, START_STR) )
	{
		sInfo.command = START_CMD;
		if ( (p=strchr(buf, '|')) == (char *)NULL)
		{
			fprintf(stderr, "[%d] Invalid command string received (%s).  Unable to parse.  Where's the uname?", __LINE__,
				zData);
			return(-1);
		}
		p++;
		sprintf(buf, "%s", p);

		if ( (p=strchr(buf, '|')) == (char *)NULL)
		{
			fprintf(stderr, "[%d] Invalid command string received (%s).  Unable to parse.  Where's the uname?", __LINE__,
				zData);
			return(-1);
		}
		*p='\0';
		sprintf(sInfo.uname, "%s", buf);
		p++;
	
		sprintf(sInfo.filename, "%s", p);
	}
	else if ( strstr(buf, END_MORE_COMING_LATER_STR) )
	{
		sInfo.command = END_CMD;
	}
	else if ( strstr(buf, DONE_WITH_FILE_STR) )
	{
		sInfo.command = DONE_CMD;
	}
	else if ( strstr(buf, EXITING_STR) )
	{
		sInfo.command = EXIT_CMD;
	}
	else
	{
		fprintf(stderr, "[%d] Invalid command string received (%s).  Unable to parse. What's the command?", __LINE__,
			zData);
		return(-1);
	}
	return(0);

} // END: parseCmdStr()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int init()
{
	static char	mod[]="init";
	int			rc;
	char		buf[256];

    if ((gIspBase = getenv ("ISPBASE")) == NULL)
    {
		gaVarLog(mod, 0, "[%d] Unable to get ISPBASE env variable. Set it and retry.  Unable to continue.", __LINE__);
        return (-1);
    }
	gaVarLog(mod, 1, "[%d] Starting.", __LINE__);

	if ( gIspBase[strlen(gIspBase) - 1] == '/' )
	{
		gIspBase[strlen(gIspBase) - 1] = '\0';
	}

	memset((SInfo *)&sInfo, '\0', sizeof(sInfo));

	sprintf(buf, "%s/RTR", gIspBase);
	if (access (buf, F_OK|R_OK ) < 0)
	{
		if(errno == ENOENT)
		{
			if (mkdir(buf, 0755) < 0)
			{
				gaVarLog(mod, 0, "[%d] mkdir (%s) failed. Unable to create directory. [%d, %s].  Correct and retry.", __LINE__,
						buf, errno, strerror(errno));
				return(-1);
			}
		}
        else
        {
            gaVarLog(mod, 0, "[%d] Unable to access directry (%s). [%d, %s].  Correct and retry.", __LINE__,
                        buf, errno, strerror(errno));
            return(-1);
        }
	}

	
	sprintf(sInfo.workDir, "%s/work", buf);
	sprintf(sInfo.processedDir, "%s/processed", buf);

	if (access (sInfo.workDir, F_OK|R_OK ) < 0)
	{
		if(errno == ENOENT)
		{
			if (mkdir(sInfo.workDir, 0755) < 0)
			{
				gaVarLog(mod, 0, "[%d] mkdir (%s) failed. Unable to create directory. [%d, %s].  Correct and retry.", __LINE__,
						sInfo.workDir, errno, strerror(errno));
				return(-1);
			}
		}
        else
        {
            gaVarLog(mod, 0, "[%d] Unable to access directry (%s). [%d, %s].  Correct and retry.", __LINE__,
                        sInfo.workDir, errno, strerror(errno));
            return(-1);
        }
	}
	if (access (sInfo.processedDir, F_OK|R_OK ) < 0)
	{
		if(errno == ENOENT)
		{
			if (mkdir(sInfo.processedDir, 0755) < 0)
			{
				gaVarLog(mod, 0, "[%d] mkdir (%s) failed. Unable to create directory. [%d, %s].  Correct and retry.", __LINE__,
						sInfo.processedDir, errno, strerror(errno));
				return(-1);
			}
		}
        else
        {
            gaVarLog(mod, 0, "[%d] Unable to access directry (%s). [%d, %s].  Correct and retry.", __LINE__,
                        sInfo.processedDir, errno, strerror(errno));
            return(-1);
        }
	}

	return(0);
} // END: init()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void printSInfo()
{
	static char mod[] = "printSInfo";

	gaVarLog(mod, 1, "[%d] workDir(%s) processedDir(%s), uname(%s), filename(%s), adjustedFilename(%s), ",
			"shadowFilename(%s), prevShadowFilename(%s), shadowIndex(%d), command(%d)", 
			sInfo.workDir, 
			sInfo.processedDir,
			sInfo.uname,
			sInfo.filename,
			sInfo.adjustedFilename,
			sInfo.shadowFilename,
			sInfo.prevShadowFilename,
			sInfo.shadowIndex,
			sInfo.command);

} // END: printSInfo()
