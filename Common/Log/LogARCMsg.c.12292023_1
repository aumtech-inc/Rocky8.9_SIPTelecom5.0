/*------------------------------------------------------------------------------
   File Name : LogARCMsg.c
   Purpose   : This file contains some functions which are necessary
	       for logging a data to a log file.
   Date	     : 03/18/99
Update: 03/23/00 mpb Added getAndSetLogMode();
Update: 05/03/00 apj Restore GV_Resource_Name value before returning.
Update: 05/03/00 apj Added code for REPORT_DETAIL mode.
Update: 05/03/00 apj Added code to make the logging decision based also on the 
		     files like <ApplicationName>.detail or 
		     <ApplicationName>.verbose. 
Update: 05/04/00 apj Allow check_and_send_snmp_trap for messages of all types. 
Update: 05/11/2000 sja Removed redundant sprintf() statements per MPB
Update: 01/18/2001 mpb Added REPORT_CDR & REPORT_CEV.
Update: 02/01/2001 mpb Added getDefaultISPBase() routine.
Update: 02/13/2001 mpb Changed MSGLENGTH to 1024.
Update: 02/22/2001 mpb Changed arcEnv to arcEnv.cfg.
Update: 01/27/2003 mpb Commented lm_log_id.
Update: 04/16/2003 mpb Added code to support DATE_FORMAT=MILLISECONDS.
Update: 04/16/2003 mpb Added code to support REPORT_DIAGNOSE.
Update: 04/28/03 mpb Changed lm_message call to newLmMsg.
Update: 07/28/04 mpb Changed parmMsg[1204] = '\0' to 960.
Update: 08/17/04 mpb Changed parmMsg[960] = '\0' to 920.
Update: 04/24/07 ddn Added counter mechanism to avoid CPU usage caused by access()
-----------------------------------------------------------------------------*/

#include<stdio.h>
#include<time.h>
#include "ispinc.h"
#include <string.h>

#define VERBOSE_DECISION_FILE_EXTENSION "verbose"
#define DETAIL_DECISION_FILE_EXTENSION "detail"
#define GLOBAL_DIR "Global"
#define GLOBAL_CONFIG_FILE ".Global.cfg"
#define LOG_CONTROL_DIR "Log"

static int STDWrite(char *, char *);
static int first_time = 0;

extern int getDefaultISPBase(char *baseDirBuffer);
extern int newLmMsg( int mode, char    *PT_object, int MessageID, char    *module, int version, char    *format, char    *PT_prefix, int m_id, char    *resourceName, char *msgbuf);

int set_logparm_values(char *parmConfigReportMode, char *parmAppLogDecisionDir);

int LogARCMsg(char *parmModule, int parmMode, char *parmResourceName, 
	char *parmPT_Object, char *parmApplName, int parmMsgId,  char *parmMsg)
{
int  version = 1;
static int intConfigReportMode;
char	PT_prefix[10];
char	tempGV_Resource_Name[MAX_APPID_LEN];
int 	rc;
static char AppLogDecisionDir[256];
static char	configReportMode[10];
char	AppLogDecisionFile[256];

static int yIntVerboseCounter 	= 100;
static int yIntVerboseFlag 		= 0;
static int yIntDetailFlag 		= 0;

//static int yIntDetailCounter 	= 100;

#if 0
fprintf(stderr, "LogARCMsg:parmModule(%s), parmMode(%d), parmResourceName(%s), "
		"parmPT_Object(%s), parmApplName(%s), parmMsgId(%d) "
		"parmMsg(%s)\n", parmModule, parmMode, parmResourceName,
		parmPT_Object, parmApplName, parmMsgId,  parmMsg);
fflush(stderr);
#endif

/* Save original value to some temporary place and then restore it 
   before returning. */
sprintf(tempGV_Resource_Name, "%s", GV_Resource_Name);	

if(first_time == 0)
{
	if(parmApplName && parmApplName[0])
	{
		first_time = 1;
	}

	GV_DateFormat = 0;
/*	lm_log_id("ISP"); */
	sprintf(PT_prefix, "%s", "AUX");
	memset(configReportMode, 0, sizeof(configReportMode));

	set_logparm_values(configReportMode, AppLogDecisionDir);

	if(strcmp(configReportMode, "VERBOSE") == 0)
		intConfigReportMode = REPORT_VERBOSE;
	else if(strcmp(configReportMode, "DETAIL") == 0)
		intConfigReportMode = REPORT_DETAIL;
	else if(strcmp(configReportMode, "DIAGNOSE") == 0)
		intConfigReportMode = REPORT_DIAGNOSE;
	else
		intConfigReportMode = REPORT_NORMAL;

sprintf(GV_Resource_Name, "%s", parmResourceName);
sprintf(GV_Application_Name, "%s", parmApplName);


	sprintf(AppLogDecisionFile, "%s/%s.%s", 
				AppLogDecisionDir, parmApplName,
				VERBOSE_DECISION_FILE_EXTENSION); 


//printf("DDNDEBUG: %s %s:%d:%d VCounter(%d) VerboseFlag(%d), DetailFlag(%d) Checking Decision(%s)\n", parmApplName, __FILE__, __LINE__, getpid(), yIntVerboseCounter, yIntVerboseFlag, yIntDetailFlag, AppLogDecisionFile);fflush(stdout);

	if(access(AppLogDecisionFile, F_OK) == 0)
	{
	yIntVerboseFlag = 1;
	}

	sprintf(AppLogDecisionFile, "%s/%s.%s", 
		AppLogDecisionDir, parmApplName,
		DETAIL_DECISION_FILE_EXTENSION); 

//printf("DDNDEBUG: %s %s:%d:%d VCounter(%d) VerboseFlag(%d), DetailFlag(%d) Checking Decision(%s)\n", parmApplName, __FILE__, __LINE__, getpid(), yIntVerboseCounter, yIntVerboseFlag, yIntDetailFlag, AppLogDecisionFile);fflush(stdout);
	if(access(AppLogDecisionFile, F_OK) == 0)
	{
		yIntDetailFlag = 1;
	}
	
}
sprintf(GV_Resource_Name, "%s", parmResourceName);
sprintf(GV_Application_Name, "%s", parmApplName);

switch (parmMode)
	{
	case REPORT_NORMAL:
		if(intConfigReportMode == REPORT_DIAGNOSE)
		{
			return(-1);
		}
		break;

	case REPORT_VERBOSE:
		if(intConfigReportMode != REPORT_VERBOSE)
		{
			if(intConfigReportMode == REPORT_DIAGNOSE)
			{
				return(-1);
			}
			/* Go ahead and log the message if 
			   <ApplicationName>.<VERBOSE_DECISION_FILE_EXTENSION>
			   file exists. */
			sprintf(AppLogDecisionFile, "%s/%s.%s", 
					AppLogDecisionDir, parmApplName,
					VERBOSE_DECISION_FILE_EXTENSION); 


			yIntVerboseCounter++;

			if(yIntVerboseCounter > 100)
			{
				yIntVerboseCounter = 0;
				if(access(AppLogDecisionFile, F_OK) != 0)
				{
//printf("DDNDEBUG: %s %s:%d:%d VCounter(%d) VerboseFlag(%d), DetailFlag(%d) Decision(%s)\n", parmApplName, __FILE__, __LINE__, getpid(), yIntVerboseCounter, yIntVerboseFlag, yIntDetailFlag, AppLogDecisionFile);fflush(stdout);
					sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
					yIntVerboseFlag = 0;
					return(-1);
				}
				yIntVerboseFlag = 1;
			}

			if(yIntVerboseFlag == 0)
			{
				sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
				return(-1);
			}
		}
		break;

	case REPORT_DETAIL:

		if(intConfigReportMode == REPORT_DIAGNOSE)
		{
			return(-1);
		}

		if(intConfigReportMode == REPORT_NORMAL)
		{
			/* Go ahead and log the message if either 
			  <ApplicationName>.<DETAIL_DECISION_FILE_EXTENSION> or
			  <ApplicationName>.<VERBOSE_DECISION_FILE_EXTENSION> 
			  files exist. */
			yIntVerboseCounter++;

//printf("DDNDEBUG: %s %s:%d:%d VCounter(%d) VerboseFlag(%d), DetailFlag(%d)\n", parmApplName, __FILE__, __LINE__, getpid(), yIntVerboseCounter, yIntVerboseFlag, yIntDetailFlag);fflush(stdout);

			if(yIntVerboseCounter > 100)
			{
				sprintf(AppLogDecisionFile, "%s/%s.%s", 
					AppLogDecisionDir, parmApplName,
					DETAIL_DECISION_FILE_EXTENSION); 

				yIntVerboseCounter = 0;

				if(access(AppLogDecisionFile, F_OK) != 0)
				{
//printf("DDNDEBUG: %s %s:%d:%d VCounter(%d) VerboseFlag(%d), DetailFlag(%d) File(%s) not accessible.\n", parmApplName, __FILE__, __LINE__, getpid(), yIntVerboseCounter, yIntVerboseFlag, yIntDetailFlag, AppLogDecisionFile);fflush(stdout);
					yIntDetailFlag = 0;

					sprintf(AppLogDecisionFile, "%s/%s.%s", 
						AppLogDecisionDir, parmApplName,
						VERBOSE_DECISION_FILE_EXTENSION); 

					if(access(AppLogDecisionFile, F_OK) != 0)
					{
//printf("DDNDEBUG: %s %s:%d:%d VCounter(%d) VerboseFlag(%d), DetailFlag(%d)\n", parmApplName, __FILE__, __LINE__, getpid(), yIntVerboseCounter, yIntVerboseFlag, yIntDetailFlag);fflush(stdout);
						sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
						yIntVerboseFlag = 0;
						return(-1);
					}
					else
					{
						yIntVerboseFlag = 1;
					}
				}
				else
				{
					yIntDetailFlag = 1;
				}

			}

			if(yIntVerboseFlag == 0 && yIntDetailFlag == 0)
			{
//printf("DDNDEBUG: %s %s:%d:%d VCounter(%d) VerboseFlag(%d), DetailFlag(%d)\n", parmApplName, __FILE__, __LINE__, getpid(), yIntVerboseCounter, yIntVerboseFlag, yIntDetailFlag);fflush(stdout);
				sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
				return(-1);
			}
//printf("DDNDEBUG: %s %s:%d:%d VCounter(%d) VerboseFlag(%d), DetailFlag(%d)\n", parmApplName, __FILE__, __LINE__, getpid(), yIntVerboseCounter, yIntVerboseFlag, yIntDetailFlag);fflush(stdout);
		}
		break;

   	case REPORT_STDWRITE:
		switch(parmMsgId)
		{
		  case PRINTF_MSG:
			rc = STDWrite(parmModule, parmMsg);
			sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
			return(rc);

		  default:
			rc = STDWrite(parmModule, 
				"REPORT_STDWRITE Unknown Message Format");
			fprintf(stderr, "LogARCMsg:%s\n", parmMsg);
			fflush(stderr);

			sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
			return(rc);
		}
		break;

   	case REPORT_EVENT:
		if ( GV_CevEventLogging == 0 )
		{
			return(-1);
		}
   	case REPORT_CEV:
		break;

   	case REPORT_CDR:
		break;

   	case REPORT_DIAGNOSE:
		if(intConfigReportMode == REPORT_DIAGNOSE)
			break;
		else
		{
			/* Go ahead and log the message if 
			   <ApplicationName>.<VERBOSE_DECISION_FILE_EXTENSION>
			   file exists. */
			sprintf(AppLogDecisionFile, "%s/%s.%s", 
					AppLogDecisionDir, parmApplName,
					VERBOSE_DECISION_FILE_EXTENSION); 
			if(access(AppLogDecisionFile, F_OK) != 0)
			{
				sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
				return(-1);
			}
		}
		break;

   	default:
		fprintf(stderr, "LogARCMsg(WARNING): Invalid request mode(%d)."
				" Using REPORT_NORMAL.\n", parmMode);
		fflush(stderr);
		parmMode = REPORT_NORMAL;
		break;
	}

/* if (strlen(parmMsg) > MAXMSGLEN)   changed MAXMSGLEN to 1024 */
if (strlen(parmMsg) > 920) 
{
	fprintf(stderr, "LogARCMsg(WARNING):Only first %d characters of "
			"message <%s> are written to the system log.\n",
			920, parmMsg);
	fflush(stderr);

	parmMsg[920]='\0';
}

#if 0
if (lm_message(parmMode, parmPT_Object, parmMsgId, parmModule, 
	   version, "%.3s%05d- %s", PT_prefix, parmMsgId, parmMsg) == -1 )
	{
	sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
	return (ISP_FAIL);
	}
#endif
if (newLmMsg(parmMode, parmPT_Object, parmMsgId, parmModule, 
	   version, "%.3s%05d- %s", PT_prefix, parmMsgId, 
	   parmResourceName, parmMsg) == -1 )
	{
	sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
	return (ISP_FAIL);
	}

sprintf(GV_Resource_Name, "%s", tempGV_Resource_Name);
return(ISP_SUCCESS);
} /* LogARCMsg */

int STDWrite(char *module, char *message)
{    
	int  	ret;
    time_t DateTime;	
    int    CharCnt;		
    size_t StrLength;
    char   LogLine[133];
    struct tm *PT_Time;
    pid_t  ProcessID;

    	DateTime = time(NULL);		/* Obtain local date & time */
    	PT_Time = localtime(&DateTime);	/* Convert to structure */

    	CharCnt = strftime(LogLine,132,"%y.%j-%H:%M:%S-", PT_Time);
    
	ProcessID = getpid();
    	sprintf(LogLine+CharCnt,"PID=%d-%s-", ProcessID , module); 
    	CharCnt = strlen(LogLine);

    	sprintf(LogLine+CharCnt,"%s\n", message); 

    	if((ret = printf(LogLine)) == 0) return(ISP_FAIL);	

   	return(ISP_SUCCESS);
}

int set_logparm_values(char *parmConfigReportMode, char *parmAppLogDecisionDir)
{
FILE	*fp;
char	str[80];
char	tmpBuffer[80];
int	i;
char	*ptr, *BaseDir;
char	globalConfigFileName[256];
char	globalConfigParameterName[80];
char	baseDirBuffer[80];
char	defaultConfigFileName[256];

memset(baseDirBuffer, 0, sizeof(baseDirBuffer));

BaseDir=(char *)getenv("ISPBASE");
if(BaseDir == NULL)
{
	getDefaultISPBase(baseDirBuffer);
	if(baseDirBuffer[0] == '\0')
	{
		fprintf(stderr, "LogARCMsg:Environment variable ISPBASE is "
			"set to NULL.\n");
		fflush(stderr);
	}
}
else
{
	sprintf(baseDirBuffer, "%s", BaseDir);
}

sprintf(globalConfigFileName,"%s/%s/%s", baseDirBuffer, GLOBAL_DIR, 
						GLOBAL_CONFIG_FILE);
sprintf(parmAppLogDecisionDir, "%s/%s/%s",baseDirBuffer,GLOBAL_DIR,
					LOG_CONTROL_DIR);

if((fp=fopen(globalConfigFileName, "r")) == NULL)
{
	fprintf(stderr, "LogARCMsg:Failed to open Global Configuration "
			"file <%s>. errno=%d.\n", globalConfigFileName, errno);
	fflush(stderr);
	return(-1);
}

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

	sprintf(globalConfigParameterName, "%s", "LOG_PRIMARY_SERVER");
	if(strncmp(str, globalConfigParameterName, 
			strlen(globalConfigParameterName)) == 0)
	{
 		if((ptr = strchr(str, '=')) != NULL)
		{
			ptr++;
			GV_LogConfig.Log_PrimarySrv[0] = '\0';
			strcpy(GV_LogConfig.Log_PrimarySrv, ptr);
			GV_LogConfig.Log_PrimarySrv[strlen(GV_LogConfig.Log_PrimarySrv) - 1] = '\0';
		}
		continue;
	}

	sprintf(globalConfigParameterName, "%s", "LOG_x_LOCAL_FILE"); 
	if(strncmp(str, globalConfigParameterName, 
			strlen(globalConfigParameterName)) == 0)
	{
 		if((ptr = strchr(str, '=')) != NULL)
		{
			ptr++;
			GV_LogConfig.Log_x_local_file[0] = '\0';
			strcpy(GV_LogConfig.Log_x_local_file, ptr);
			GV_LogConfig.Log_x_local_file[strlen(GV_LogConfig.Log_x_local_file) - 1] = '\0';
		}
		continue;
	}

	sprintf(globalConfigParameterName, "%s", "REPORT_MODE"); 
	if(strncmp(str, globalConfigParameterName, 
			strlen(globalConfigParameterName)) == 0)
	{
 		if((ptr = strchr(str, '=')) != NULL)
		{
			ptr++;
			parmConfigReportMode[0] = '\0';
			strcpy(parmConfigReportMode, ptr);
			parmConfigReportMode[strlen(parmConfigReportMode) - 1] = '\0';
		}
		continue;
	}

	sprintf(globalConfigParameterName, "%s", "DATE_FORMAT"); 
	if(strncmp(str, globalConfigParameterName, 
			strlen(globalConfigParameterName)) == 0)
	{
 		if((ptr = strchr(str, '=')) != NULL)
		{
			ptr++;
			tmpBuffer[0] = '\0';
			strcpy(tmpBuffer, ptr);
			tmpBuffer[strlen(tmpBuffer) - 1] = '\0';
			if(strcmp(tmpBuffer, "MILLISECONDS") == 0)
				GV_DateFormat = 1;
		}
		continue;
	}

	sprintf(globalConfigParameterName, "%s", "CEV_EVENT_LOGGING"); 
	if(strncmp(str, globalConfigParameterName, 
			strlen(globalConfigParameterName)) == 0)
	{
 		if((ptr = strchr(str, '=')) != NULL)
		{
			ptr++;
			sprintf(tmpBuffer, "%s", ptr);

			if ( (strncmp(tmpBuffer, "ON", 2) == 0) ||
			     (strncmp(tmpBuffer, "On", 2) == 0) ||
			     (strncmp(tmpBuffer, "on", 2) == 0) )
			{
				GV_CevEventLogging = 1;
			}
		}
		continue;
	}
}

fclose(fp);
return(0);
}
int getAndSetLogMode()
{
first_time = 0;
return(0);
}
int getDefaultISPBase(char *baseDirBuffer)
{
char	defaultConfigFileName[128];
char	str[80];
FILE	*fp;
char	*ptr;

sprintf(defaultConfigFileName, "%s", "/arc/arcEnv.cfg");
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
 			if((ptr = strchr(str, '=')) != NULL)
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
}
