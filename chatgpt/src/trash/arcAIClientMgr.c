/*----------------------------------------------------------------------------
Purpose     :  	AI Client Manager.
Author      :	Aumtech, Inc
------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <ctype.h>
#include <unistd.h>

#include "TEL_LogMessages.h"

#define MAX_CLIENTS 192

int 	gDMCount;
int 	gDebugLevel;
int 	gBufferInterval;
int		gInitialBlocks;
int		gTotalClients;

char	yStrDebug[10];
char	yStrBufferCount[10];
char	yStrDMCount[10];
char	yStrInitialBlocks[10];

char	gTryRule[30];

int		keepCheckingApps;

char	gAppPath[256];

struct PROCESSES
{
	pid_t 	pid;
	time_t	startTime;	
	char	stopFile[128];
	time_t	restartTime;
};

struct PROCESSES gProcesses[MAX_CLIENTS];


int killAllClients();
int arg_switch(char *mod, int parameter, char *value);
int parse_arguments(char *mod, int argc, char *argv[]);
static int  getNumResources(int *numResources);
static void myLog(int zLine, char *mod, int mode, char *messageFormat, ...);
static void startLicensing();
static int	setEnv();

int cleanUp();
/*
 *	Signal Handler routine.
 */
void sigTermHandler(int zSignal)
{
	char ySignalName[32];

	switch(zSignal)
	{
		case SIGTERM:
			sprintf(ySignalName, "%s", "SIGTERM");
			break;

		case SIGHUP:
			sprintf(ySignalName, "%s", "SIGHUP");
			break;

		case SIGINT:
			sprintf(ySignalName, "%s", "SIGINT");
			break;

		case SIGPIPE:
			sprintf(ySignalName, "%s", "SIGPIPE");
			break;

		default:	
			sprintf(ySignalName, "signal %d", zSignal);
			break;
	}

	keepCheckingApps = 0;
	killAllClients();

	sleep(5);
	
	exit(-1);

}/*END: int sigTermHandler*/


int killAllClients()
{
	char mod[] = "killAllClients";
	char	yErrMsg[128];
	int 	yCounter = 0;
	int 	yRc;

	for(yCounter = 0; yCounter < gTotalClients; yCounter++)
	{
		if(gProcesses[yCounter].pid != -1)
		{
			sprintf(yErrMsg, "Killing pid %d WITH SIGTERM", 
												gProcesses[yCounter].pid);

			yRc = kill(gProcesses[yCounter].pid, SIGTERM);
			if(yRc == -1)
			{
				if(errno == ESRCH)
				{
					myLog(__LINE__, mod, REPORT_DETAIL, "Failed to kill pid %d. No such process.",
								gProcesses[yCounter].pid);
					gProcesses[yCounter].pid = -1;
				}
			}

		}
	}

	sleep(3);

	for(yCounter = 0; yCounter < gTotalClients; yCounter++)
	{
		if(gProcesses[yCounter].pid != -1)
		{
			yRc = kill(gProcesses[yCounter].pid, 0);
			if(yRc == 0)
			{
				myLog(__LINE__, mod, REPORT_VERBOSE, "Killing pid %d WITH SIGKILL", 
											gProcesses[yCounter].pid);

				yRc = kill(gProcesses[yCounter].pid, SIGKILL);
			}

			gProcesses[yCounter].pid = -1;
		}
	}

	return (0);

}

int parse_arguments(char *mod, int argc, char *argv[])
{
    int c;
    int i;

    while( --argc > 0 )
    {
            *++argv;
            if(argv[1][0] != '-')
                {
                    arg_switch(mod, (*argv)[1], argv[1]);
                    *++argv;
                    --argc;
                }
            else
                    arg_switch(mod, (*argv)[1]," ");
    }

    return (1);

}/*END: int parse_arguments*/


int arg_switch(char *mod, int parameter, char *value)
{
    struct  tm  st;  /* time structure for maninpulating GV_RingEventTime */
    char    year_str[]="19yy";
    char    month_str[]="mm";
    char    day_str[]="dd";
    char    hour_str[]="hh";
    char    minute_str[]="mm";
    char    second_str[]="ss";

    switch(parameter)
    {
        case 'i': sscanf(value, "%d", &gBufferInterval);
        break;

        case 'n': sscanf(value, "%d", &gDMCount);
        break;

        case 'd': sscanf(value, "%d", &gDebugLevel);
        break;

        case 'b': sscanf(value, "%d", &gInitialBlocks);
        break;

// This is now handled in the main()  routine.
		case 'r': sscanf(value, "%d", &gTotalClients);
		break;

        case 't': sprintf(gTryRule, "%s", value);
        break;


        defaule:break;
    }

    return(0);

}/*END: arg_switch*/

int cleanUp()
{

	return (0);

}/*END: int cleanUp*/

int startClient(int zLine, int index)
{
		static char mod[]="startClient";
		int yTempPid;
		int yCounter = index;
		int yRc;
		char yErrMsg[128];
		char yStrIndex[10];

		sprintf(yStrIndex, "%d", index);

		sprintf(gAppPath, "%s", "/home/arc/djb/chatgpt/venv/bin/python arcAIClient.py");
		myLog(__LINE__, mod, REPORT_VERBOSE, "[Called from %d] index=%d", zLine, index);
		if((yTempPid = fork()) == 0)
		{

			myLog(__LINE__, mod, REPORT_VERBOSE, "Starting ArcAIClient.py");

			yRc = execl(gAppPath, (char *) 0);

			if(yRc < 0)
			{
				myLog(__LINE__, mod, REPORT_NORMAL, "Failed to execl (%s). [%d, %s]",
							gAppPath, errno, strerror(errno));
				gProcesses[yCounter].pid = yTempPid;

				yTempPid = -1;

				exit(-1);
			}

			exit(0);
		}

		if(yTempPid < 0)
		{
			myLog(__LINE__, mod, REPORT_NORMAL, "Failed to start mrcpClient2 fork:errno=%d", errno);

			killAllClients();

			sleep(2);

			exit(-1);
		}

		gProcesses[yCounter].pid = yTempPid;
		sprintf(gProcesses[yCounter].stopFile, "/tmp/mrcpStop.%d", yTempPid);
		gProcesses[yCounter].restartTime	= -1;
		time(&(gProcesses[yCounter].startTime));

		return (0);

}/*END int startClient */

int main(int argc, char * argv[])
{
	char mod[] = "main";

	int		yRc;
	int		rc;

	time_t	currentTime;

	char	yErrMsg[128];
	char	yStrVersion[20] = 	{"1.0.0"};
	char	yStrBuild[10] = 	{"1"};

	int		yCounter = 0;

	pid_t	yTempPid = 0;

	int timeCounter = 0;
	int numResources;
	char	stopFile[128];
	int		i;

	char	venvPath[256];
	FILE	*venvFD;

	/*SIGNAL HANDLER variables*/
	struct  sigaction   sig_act, sig_oact;

	sig_act.sa_handler = NULL;
	sig_act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;

	if(sigaction(SIGCHLD, &sig_act, &sig_oact) != 0)
	{
        myLog(__LINE__, mod, REPORT_NORMAL, "Failed sigaction(SIGCHLD, SA_NOCLDWAIT): errno=%d.", errno);
		sleep(3);
		exit(1);
	}

	if (signal(SIGCHLD, SIG_IGN) == SIG_ERR || sigset(SIGCLD, SIG_IGN) == -1)
	{
        myLog(__LINE__, mod, REPORT_NORMAL, "Failed signal(SIGCHLD, SIG_IGN): errno=%d.", errno);

		sleep(3);
		exit(1);
	}

	if(sigaction(SIGPIPE, &sig_act, &sig_oact) != 0)
	{
        myLog(__LINE__, mod, REPORT_NORMAL, "Failed sigaction(SIGPIPE, SA_NOCLDWAIT): errno=%d.", errno);
  
		sleep(3);
		exit(1);
	}

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR || sigset(SIGCLD, SIG_IGN) == -1)
	{
        myLog(__LINE__, mod, REPORT_NORMAL, "Failed signal(SIGPIPE, SIG_IGN): errno=%d.", errno);

		sleep(3);
		exit(1);
	}

	keepCheckingApps = 1;

	gBufferInterval = 4;
	gDebugLevel 	= 0;
	gDMCount 		= 0;
	gInitialBlocks	= 0;
	gTotalClients	= 2;

	sprintf(gTryRule, "%s", "2,10");


	if(argc == 2 && strcmp(argv[1], "-v") == 0)
	{
		fprintf(stdout,
		"Arc AI Client Manager.\n"
			"Program: %s\n"
			"Version: %s Build: %s\n"
			"Compiled on %s %s\n",
			argv[0], yStrVersion, yStrBuild, __DATE__, __TIME__);

		exit(0);
	}
	else
	if(argc == 2 && strcmp(argv[1], "-vb") == 0)
	{
		fprintf(stdout, "%s", yStrBuild);
		exit(0);
	}
	else
	if(argc == 2 && strcmp(argv[1], "-vv") == 0)
	{
		fprintf(stdout, "%s", yStrVersion);
		exit(0);
	}
	else
	if(argc == 2 && strcmp(argv[1], "-vd") == 0)
	{
		fprintf(stdout, "%s %s", __DATE__, __TIME__);
		exit(0);
	}
	else
	if(argc == 2 && strcmp(argv[1], "-?") == 0)
	{
		fprintf(stdout, "%s "
						"-r <total clients to run (1-12)>\n"
#if 0
						"-d <log level (0-4)>\n"
						"-n <# of ArcDynMgrs running (future use)>(1-8)>\n"
						"-i <buffering interval per request>\n"
						"-b <# of blocks to be fetched in first request.>\n"
						"-t <# of secondes to sleep if socket open fails.>\n"
#endif
						,argv[0]); fflush(stdout);
		exit(0);
	}
	/*END: Version information*/

	/*Asign signal handlers*/

	signal(SIGTERM, sigTermHandler);
	signal(SIGINT, 	sigTermHandler);
	signal(SIGHUP, 	sigTermHandler);
	//signal(SIGPIPE, sigTermHandler);

	if ( (rc = setEnv()) == -1 )
	{
		// message is logged in routine
		exit(3);
	}

	/*END: Asign signal handlers*/

	/*
	**	Parse and validate the arguments
	*/
	rc = getNumResources(&numResources);

	if ( numResources == 0 )
	{
		gTotalClients = 1;
	}
	else
	{
		if ( numResources % 12 == 0 )
		{
			gTotalClients = numResources / 12 ;
		}
		else
		{
			gTotalClients = numResources / 12 + 1;
		}
	}


	/*END:	Parse and validate the arguments	*/

	if(gTotalClients <= 0 || gTotalClients > 8)
	{
		numResources = 96;
		gTotalClients = 8;
	}

	parse_arguments(mod, argc, argv);

	myLog(__LINE__, mod, REPORT_VERBOSE, "numResources=%d;  gTotalClients=%d", numResources, gTotalClients);


	sprintf(yStrDebug, 			"%d", gDebugLevel);
	sprintf(yStrBufferCount, 	"%d", gBufferInterval);
	sprintf(yStrDMCount, 		"%d", gDMCount);
	sprintf(yStrInitialBlocks, 	"%d", gInitialBlocks);

	for(yCounter = 0; yCounter < gTotalClients; yCounter++)
	{
		gProcesses[yCounter].pid			= -1;
	}

	strcat(gAppPath, "python");

	for(yCounter = 0; yCounter < gTotalClients; yCounter++)
	{
		startClient(__LINE__, yCounter);
	}

	while(keepCheckingApps)
	{
		usleep(500 *1000);
		timeCounter++;

		for(yCounter = 0; yCounter < gTotalClients; yCounter++)
		{
			if(gProcesses[yCounter].pid > 0)
			{
#if 0
				if ( access(gProcesses[yCounter].stopFile, F_OK) == 0 )
				{
					myLog(__LINE__, mod, REPORT_DETAIL, "Stop file (%s) found for id %d.  Sending SIGTERM to pid %d...",
							gProcesses[yCounter].stopFile, yCounter, gProcesses[yCounter].pid);

					kill(gProcesses[yCounter].pid, SIGTERM);
					time(&gProcesses[yCounter].restartTime);
					gProcesses[yCounter].restartTime += 180;  // wait 3 minutes

					myLog(__LINE__, mod, REPORT_DETAIL, "gProcesses[%d].restartTime = %ld", 
						yCounter, gProcesses[yCounter].restartTime);

					unlink(gProcesses[yCounter].stopFile);

					usleep(500);
					i=0;
					continue;
				}
#endif			
				if(kill(gProcesses[yCounter].pid, 0) == -1)
				{
					if ( gProcesses[yCounter].restartTime != -1 ) 
					{
						time(&currentTime);
				
//						if ( i % 5 == 0 )
//						{
//							myLog(__LINE__, mod, REPORT_VERBOSE, "gProcesses[%d].pid (%d) not running.  Restart time=%ld, current time=%ld", 
//								yCounter, gProcesses[yCounter].pid,
//								gProcesses[yCounter].restartTime, currentTime);
//						}

						if ( currentTime > gProcesses[yCounter].restartTime )
						{
							myLog(__LINE__, mod, REPORT_DETAIL, "Three minutes are up.   Restart time=%ld, current time=%ld", 
								gProcesses[yCounter].restartTime, currentTime);

							gProcesses[yCounter].restartTime = -1;
							startClient(__LINE__, yCounter);
							usleep(500);
						}
					}
					else if(errno == ESRCH)
					{
						startClient(__LINE__, yCounter);
						usleep(500);
					}
				}
			}
		}/*END:for*/

		if(timeCounter > 120)
		{
			timeCounter = 0;
			cleanUp();
		}

	}/*END:while*/


	exit(0);

}/*END: main*/

/*-------------------------------------------------------------------
setEnv(): 
--------------------------------------------------------------------*/
static int setEnv()
{
	static char	mod[]="setEnv";
	char		*pEnvvar;
	char		*pLdlib;
	char		venv[256];
	int			rc;
	char		path[512];
	char		*p;

	if( (pEnvvar = getenv("ISPBASE")) == NULL)
	{
		myLog(__LINE__, mod, REPORT_NORMAL, "Failed to get ISPBASE from environment.  Correct and retry.");
		sleep(2);
		return(-1);
	}
	p=pEnvvar + strlen(pEnvvar);
	if ( *p == '/' )
	{
		*p = '\0';  // remove the trailing '/' if it exists
	}

	sprintf(venv, "%s/Telecom/Exec/chatgpt/venv/bin/activate", pEnvvar);

#if 0
	if ((rc = execl(venv, (char *) 0)) != 0)
    {
		myLog(__LINE__, mod, REPORT_NORMAL, "Failed to set virtual environment.  [%d, %s] Correct and retry.",
             errno, strerror(errno));
		sleep(2);
        return(-1);
    }
	//myLog(__LINE__, mod, REPORT_VERBOSE, "Successfully activated virtual environment.  [%d, %s] Correct and retry.",
	fprintf(stderr, "Successfully activated virtual environment.\n");
#endif

	if( (pLdlib = getenv("LD_LIBRARY_PATH")) == NULL)
	{
		myLog(__LINE__, mod, REPORT_NORMAL, "Failed to get LD_LIBRARY_PATH from environment.  Correct and retry.");

		sleep(2);
		return(-1);
	}

	p=pLdlib + strlen(pLdlib);
	if ( *p == ':' )
	{
		*p = '\0';  // remove the trailing ':' if it exists
	}

	if ( strstr(pLdlib, "chatgpt") != NULL )
	{
		return(0);
	}

	sprintf(path, "%s:%s/Telecom/Exec/chatgpt", pLdlib, pEnvvar);

	if ( (rc = setenv("LD_LIBRARY_PATH", path, 1)) != 0)
	{
		myLog(__LINE__, mod, REPORT_NORMAL, "Failed to set LD_LIBRARY_PATH to (%s). [%d, %s]  Correct and retry.",
				errno, strerror(errno));
		sleep(2);
		return(-1);
	}
	pLdlib = getenv("LD_LIBRARY_PATH");
	myLog(__LINE__, mod, REPORT_VERBOSE, "Successfully set LD_LIBRARY_PATH to (%s).", pLdlib);

	sprintf(path, "%s/Telecom/Exec/chatgpt", pEnvvar);
	if ( (rc = chdir(path)) != 0)
	{
		myLog(__LINE__, mod, REPORT_NORMAL, "Failed to change directory to (%s). [%d, %s]  Correct and retry.",
				errno, strerror(errno));
		sleep(2);
		return(-1);
	}

	return(0);
} // END: setEnv()
/*-------------------------------------------------------------------
getNumResources(): This Routine load resource table into memory.
--------------------------------------------------------------------*/
static int  getNumResources(int *numResources)
{
    static  char    mod[]="getNumResources";
    char    record[256];
    FILE    *fp;
    int     i;
    char    *p;
	char	*isp_base;
	char resource_file[128];

	if((isp_base=(char *)getenv("ISPBASE")) == NULL)
	{
	    myLog(__LINE__, mod, REPORT_DETAIL, "Environment variable ISPBASE is not found/set.  "
				"Defaulting to 48 ports.");
		*numResources = 48;
		return(0);
	}

	sprintf(resource_file, "%s/Telecom/Tables/ResourceDefTab", isp_base);
    *numResources = 0;
    i = 0;
    if((fp=fopen(resource_file, "r")) == NULL)
    {
        myLog(__LINE__, mod, REPORT_DETAIL, "Can't open file %s. [%d, %s]  Defaulting to 48 ports.",
                resource_file, errno, strerror(errno));
		return(0);
    }

    while( fgets(record, sizeof(record), fp) != NULL)
    {
        if( (p = strstr(record, "DCHANNEL")) != (char *)NULL)
        {
            continue;
        }
        i++;
    }

    (void) fclose(fp);
    *numResources = i;

    return (0);

} // END: getNumResources()

static void myLog(int zLine, char *mod, int mode, char *messageFormat, ...)
{
	static char		prog[64] = "aiClientMgr";
	va_list     ap;
	char        message[1024];
	char        tmpMessage[1024];

    va_start(ap, messageFormat);
    vsprintf(tmpMessage, messageFormat, ap);
    va_end(ap);

	sprintf(message, "[%d] %s", zLine, tmpMessage);

	LogARCMsg(mod, mode, "-1", "AI", prog, TTS_BASE, message);
}

/*EOF*/
