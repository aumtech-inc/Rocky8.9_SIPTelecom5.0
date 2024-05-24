#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

static char gAppName[16] = "goLoad";
// static char gOutputFile[256];

static void myLog(char *messageFormat, ...);
static int amIAlreadyRunning ();
static int loadConfig(char *zCdfFile, int *zIterations, int *zSleepTime, int *zPortLoad);
static int copyFile(char *zSrcFile, char *zDestFile);
static int touchFile(char *zSrcFile);

int main(int argc, char *argv[])
{
	int		rc;
	char	cdfFile[128];
	int		iterations;
	int		sleepTime;
	int		portLoad;
	char	workDir[128] = "/home/arc/.ISP/Telecom/OCS/work";
	char	ocsDir[128] = "/home/arc/.ISP/Telecom/OCS";
	int		loopAgain;
	int		i, j;
	char	newFile[256];
	char	newFileZero[256];
	int		fp;

	rc = open("nohup.out", O_CREAT|O_WRONLY|O_TRUNC);
	
	if (amIAlreadyRunning()) 
	{
		myLog("%s is already running. Only one load test at a time.  Exiting.", gAppName);
	}

	if ( (rc = loadConfig( cdfFile, &iterations, &sleepTime, &portLoad)) != 0 )
	{
		exit(3);
	}
	if ( iterations > 0 )
	{
		myLog("Load succeeded: cdfFile(%s) iterations(%d) sleepTime(%d) portLoad(%d)",
			cdfFile, iterations, sleepTime, portLoad);
	}
	else
	{
		myLog("Load succeeded: cdfFile(%s) iterations(%d - run indefinitely) sleepTime(%d) portLoad(%d)",
			cdfFile, iterations, sleepTime, portLoad);
	}

	i = 0; 
	j = 0;
	for(;;)
	{
		myLog("*** Start OCS calls then sleep %d seconds.", sleepTime);
		loopAgain = 1;
		while ( loopAgain )
		{
			sprintf(newFile, "%s/cdf_%d.kd", workDir, i);
			sprintf(newFileZero, "%s/0.cdf_%d.kd", workDir, i);

			if ((rc = copyFile(cdfFile, newFile)) != 0 )
			{
				continue;  // message is logged in copyFile()
			}
			if ((rc = touchFile(newFileZero)) != 0 )
			{
				continue;  // message is logged in touchFile()
			}
			i++;
			if ( i == portLoad )
			{
				loopAgain = 0;
				i = 0;
				sleep(1);
			}
		}

		j++;
		if ( iterations > 0 )
		{
			myLog("Completed %d of %d iterations.", j, iterations);
			if ( j >= iterations )
			{
				break;
			}
		}
		else
		{
			myLog("Completed %d iterations.", j);
		}
		sleep(sleepTime);
	}

} // END: main()

//-----------------------------------------------------
//-----------------------------------------------------
static int copyFile(char *zSrcFile, char *zDestFile)
{
	FILE *fpSrc;
	FILE *fpDest;
	int c;
	int bytes = 0;
	
	if ( (fpDest = fopen(zDestFile, "w")) == NULL)
	{
		myLog("Failed to open (%s) for writing.  Unable to process", zDestFile);
		return(-1);
	}
	if ( (fpSrc = fopen(zSrcFile, "r")) == NULL)
	{
		myLog("[%d] Failed to open (%s).  Unable to process", __LINE__, zSrcFile);
		return(-1);
	}

	/* Copy bytes until End Of File. */
	while((c =getc(fpSrc)) != EOF)
	{
		putc(c, fpDest);
		bytes++;
	}
	
	fclose(fpSrc);
	fclose(fpDest);

	return(0);
} // END: copyFile()

//-----------------------------------------------------
//-----------------------------------------------------
static int touchFile(char *zSrcFile)
{
	FILE *fpSrc;
	if ( (fpSrc = fopen(zSrcFile, "w")) == NULL)
	{
		myLog("Failed to open/create (%s).  Unable to process", zSrcFile);
		return(-1);
	}
	fclose(fpSrc);

	return(0);
} // END: copyFile()

//-----------------------------------------------------
//-----------------------------------------------------
static int loadConfig(char *zCdfFile, int *zIterations, int *zSleepTime, int *zPortLoad)
{
	FILE	*fp;
	int		i;
	char	buf[128];
	int		telecomPort;
	int		mrcpPort;
	char	*p;
	static char	cfgFile[128]="/home/arc/.ISP/Telecom/OCS/goLoad.cfg";

	if ((fp = fopen(cfgFile, "r")) == NULL )
	{
		myLog( "Unable to open (%s). [%d, %s] Correct and retry.",
			cfgFile, errno, strerror(errno));
		return(-1);
	}

	*zCdfFile = '\0';
	*zIterations = -1;
	*zSleepTime = -1;
	*zPortLoad = -1;
	while (fgets(buf, 128, fp))
	{
		if ( buf[0] == '#' )
		{
			continue;
		}
		if ( buf[strlen(buf)-1] == '\n' )
		{
			buf[strlen(buf)-1] = '\0';
		}
		if ( (p=strchr(buf, '=' )) == NULL )
		{
			continue;
		}

		*p = '\0';
		p++;
		if (strcmp(buf, "cdfFile") == 0)
		{
			sprintf(zCdfFile, "%s", p);
		}
		else if (strcmp(buf, "iterations") == 0)
		{
			*zIterations = atoi(p);
		}
		else if (strcmp(buf, "sleepTime") == 0)
		{
			*zSleepTime = atoi(p);
		}
		else if (strcmp(buf, "portLoad") == 0)
		{
			*zPortLoad = atoi(p);
		}
	}
	fclose(fp);

	if ( zCdfFile[0] == '\0' )
	{
		myLog("cdfFile entry does not exist in %s.  Correct and retry.", cfgFile);
		return(-1);
	}
	if ( *zIterations == -1 )
	{
		myLog("iterations entry does not exist in %s.  Correct and retry.", cfgFile);
		return(-1);
	}
	if ( *zSleepTime == -1 )
	{
		myLog("sleepTime entry does not exist in %s.  Correct and retry.", cfgFile);
		return(-1);
	}
	if ( *zPortLoad == -1 )
	{
		myLog("portLoad entry does not exist in %s.  Correct and retry.", cfgFile);
		return(-1);
	}

	return(0);
} // END: loadConfig()

//-----------------------------------------------------
//-----------------------------------------------------
static int amIAlreadyRunning ()
{

    static char mod[] = "amIAlreadyRunning";
    FILE *fin;                                    /* file pointer for the ps pipe */
    int i;
    char buf[2048];
	static char ps[] = "ps -ef | grep -v grep";

    if ((fin = popen (ps, "r")) == NULL)          /* open the process table */
    {
        myLog ("Failed to verify that %s is running; assuming now....", gAppName);
        return (0);
    }

    (void) fgets (buf, sizeof buf, fin);          /* strip off the header */

    i = 0;
    while (fgets (buf, sizeof buf, fin) != NULL)
    {
        if (strstr (buf, gAppName) != NULL)
        {
        	if (strstr (buf, "vi") == NULL)
			{
				i = i + 1;
				if ( i >= 2 )
				{
					(void) pclose (fin);
					return (1);
				}
            }
        }
    }
    (void) pclose (fin);

    return (0);

}	// END: amIAlreadyRunning ()

//-----------------------------------------------------
//-----------------------------------------------------
static void myLog(char *messageFormat, ...)
{
	va_list     ap;
	struct tm   *pTime;
	char        timeBuf[64];
	char        message[2048];
	static FILE	*fp = NULL;
	time_t		myTime;
	
	time(&myTime);

	pTime = localtime(&myTime);

	strftime(timeBuf, sizeof(timeBuf)-1, "%m:%d:%y %H:%M:%S", pTime);

	va_start(ap, messageFormat);
	vsprintf(message, messageFormat, ap);
	va_end(ap);

	printf("goLoad|%-6d|%s|%s\n", (int)getpid(), timeBuf, message);
	fflush(stdout);
	return;

} // END: myLog()

