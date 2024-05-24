/*-----------------------------------------------------------------------------
:        log_remote.c
Purpose:        Dummy the IView routines and use Aumtech's own logging of 
		LogLite.
Author:         M. Joshi
Date:           03/04/96
Update:02/19/97 D. Barto replaced loglite output mechanism with the new log
			 	  routines to allow remote logging.
Update:03/13/97 D. Barto declared extern 'GV_LogConfig' to get the server
				  name and LOG_x_LOCAL_FILE rather than open and
				  read from the config file.
Update:11/04/97 D. Barto imbedded log_routine functions Write...()
Update:11/23/98 gpw put 'N' in Reporting mode for REPORT_SPECIAL, CDR, CEV.
Update:11/23/98 gpw Removed version, PT_prefix, messageID.
Update:11/25/98 mpb Added GV_Application_Name & GV_Resource_Name.
Update:04/03/99 mpb Moved check for primary host in first_time. It was in 
		wrong place and was never executed.
Update:	04/07/99 djb	changed cftime() to strftime(); SNA EXPRESS
Update:	05/24/00 apj In lm_message, added a case for REPORT_DETAIL.
Update: 02/15/01 mpb In WriteToFifo & GetLogPaths, added code to read
		default ISPBASE.
Update: 02/11/02 mpb In GetLogPaths, close fp only if opened successfully.
Update: 01/27/03 mpb Replaced log_id with lLogId.
Update: 02/04/03 mpb In REPORT_CDR & REPORT_CEV, set report mode to N.
Update: 04/16/03 mpb Added code to support DATE_FORMAT=MILLISECONDS, by
			adding GV_DateFormat check.
Update: 04/28/03 mpb Added newLmMsg.
Update: 03/13/07 djb Added CDR log file logic.
Update: 08/18/2009 ddn Replaced strtok with thread safe strtok_r
Update: 08/28/2009 ddn TBD: replace localtime, getpwuid, etc with thread safe
Update: 07/16/2010 rg,ddn Replaced localtime with thread safe localtime_r
---------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <time.h>
#include <sys/utsname.h>
#include <errno.h>
#include <pwd.h>
#include "log_defs.h"
#include "ispinc.h"
#include "loginc.h"
#include "COMmsg.h"
#include "LOGmsg.h"
#include "CDRmsg.h"
#include "CEVmsg.h"
#include "NETmsg.h"

#define	CDR_FILE_NO			1
#define	CDR_FILE_YES_BOTH	2
#define	CDR_FILE_YES_ONLY	3

#define ERR_FILE_NO			0
#define ERR_FILE_YES		1

extern	LogAttr_t	GV_LogConfig;
extern	char		GV_AppId[];
extern	char		GV_Application_Name[];
extern	char		GV_Log_Directory_Name[];
extern	char		GV_Resource_Name[];
extern	int			GV_DateFormat;
extern	char		GV_CDRKey[];
static  char		log_id[5];
static  char		appl_id[30];
static  char		logBuf[512];
static	int			firstTime=0;

static	char		gMyLogConfig[256];
static	int			gLogCDR = -1;
static	int			gLogERR = -1;
static	int			gSpecialLog;			// if set and remote logging send cdr/cev records
            		            			// to remote system in realtime

static int	WriteLocal(char *LogPath_1, char *Message,
					int forward_sw);
static int	WriteLocal_2(char *Message, int forward_sw);
static int	WriteToFifo(char *logpath1, char *message);
static void	GetLogPaths(char *LogPath_1, char *LogPath_2);
static int	util_sleep(int Seconds, int Milliseconds);
static int	lr_readline(char *the_line, int maxlen, FILE *fp);
static int	lr_strip_str(char *str);
static void ChkPath(char *PathName, char *NewPath);
static int	removeWhiteSpace(char *buf);
static int my_get_name_value(char *section, char *name, char *defaultVal,
			char *value, int bufSize, char *file, char *err_msg);
static int my_trim_whitespace(char *string);

static void	setLogCDRs();


/*--------------------------------------------------------
lm_message() : Allow applicatin to log messages.
----------------------------------------------------------*/
int lm_message( mode, PT_object, MessageID, module, version, format, PT_prefix, m_id, msgbuf)
int	mode;
char	*PT_object;
int	MessageID;
char	*module;
int	version;
char	*format;
char	*PT_prefix;
int	m_id;
char	*msgbuf;
{
FILE	*fp;
char	rep_mode;
char	message[1025];
time_t	the_time;
char	timebuf[64];
static	int	first_time = 0, rc;
static	struct	utsname	sys_info;
static	int	pid;
static	char	logname[40];
static uid_t	the_uid;
static struct	passwd	*the_passwd;
struct tm	*PT_time;
char lLogId[5];

char fifoname[BUFSZ], logpath[BUFSZ];
char server[32], Nodename[32];
static char     mod[] = "LogIt";
char		*p;

struct timeval	tp;
struct timezone	tzp;
struct tm	*tm;

// fprintf(stdout, "RGDEBUG:%s::%d:: OINK\n", __FILE__, __LINE__);fflush(stdout);
memset(gMyLogConfig, '\0', sizeof(gMyLogConfig));
if (first_time == 0)
	{
	uname (&sys_info);
	pid = getpid();
	the_uid = getuid();	/* Cannot fail */
	if((the_passwd = getpwuid(the_uid)) == (struct passwd *)NULL)
		strcpy(logname, "NULL");
	else
		sprintf(logname, "%s", the_passwd->pw_name);

	rc=removeWhiteSpace(GV_LogConfig.Log_PrimarySrv);

	if (strlen(GV_LogConfig.Log_PrimarySrv) == 0)
	{
		sprintf(GV_LogConfig.Log_PrimarySrv, "%s", sys_info.nodename);
	}
	first_time = 1;
	}

sprintf(lLogId, "%s", "ISP");

switch (mode)
	{
	case	REPORT_NORMAL :
		rep_mode =  'N';
		break;
	case	REPORT_VERBOSE:
		rep_mode =  'V';
		break;
	case	REPORT_DETAIL:
		rep_mode =  'D';
		break;
	case	REPORT_DEBUG  :
	case	REPORT_DIAGNOSE :
		rep_mode =  'd';
		break;
	case	REPORT_STDWRITE:
		rep_mode =  'E';
		break;
   	case REPORT_CEV:
		rep_mode =  'N';
		sprintf(lLogId, "%s", "CEV");
		if(strlen(GV_Log_Directory_Name) == 0) 		// BT-71
		{
			sprintf(gMyLogConfig, "%s", GV_LogConfig.Log_x_local_file);
		}
		else
		{
			sprintf(gMyLogConfig, "%s", GV_Log_Directory_Name);
		}
		p = &(gMyLogConfig[strlen(gMyLogConfig) - 3]);
		sprintf(p, "%s", "CDR");
		break;
   	case REPORT_CDR:
		rep_mode =  'N';
		sprintf(lLogId, "%s", "CDR");
		if(strlen(GV_Log_Directory_Name) == 0)
		{
			sprintf(gMyLogConfig, "%s", GV_LogConfig.Log_x_local_file);
		}
		else
		{
			sprintf(gMyLogConfig, "%s", GV_Log_Directory_Name);
		}
		p = &(gMyLogConfig[strlen(gMyLogConfig) - 3]);
		sprintf(p, "%s", "CDR");
		break;
	default:
		rep_mode = 'N';
		break;
	}

memset((char *)timebuf, 0, sizeof(timebuf));

if(GV_DateFormat == 1)
{
	struct tm tempTM;
	gettimeofday(&tp, &tzp);

	tm = localtime_r(&tp.tv_sec, &tempTM);
	sprintf(timebuf, "%02d:%02d:%04d %02d:%02d:%02d:%03d", 
    		tm->tm_mon + 1, tm->tm_mday, tm->tm_year+1900, tm->tm_hour, 
    		tm->tm_min, tm->tm_sec, tp.tv_usec / 1000);
}
else
{
	struct tm tempTM;
	time(&the_time);
	PT_time=localtime_r(&the_time, &tempTM);
	strftime(timebuf, sizeof(timebuf) ,"%h %d %T %Y", PT_time);
}

/*
sprintf(message, "%s|%c|%s|%s|%d|%s|%d|%s|%s|%s|%d!%s%d - %s\n",
	log_id, rep_mode, timebuf, PT_object, MessageID,
	sys_info.nodename, pid, logname, module, GV_AppId,
	version, PT_prefix, MessageID, msgbuf);
*/
sprintf(message, "%s|%c|%s|%s|%d|%s|%d|%s|%s|%s|%s\n",
	lLogId, rep_mode, timebuf, PT_object, MessageID, sys_info.nodename,
	 pid, GV_Resource_Name, module, GV_Application_Name, msgbuf);


	//dfp=fopen("/tmp/logoutput.txt", "a+");
	
if (strcmp(GV_LogConfig.Log_PrimarySrv, sys_info.nodename))
{
	if ( gMyLogConfig[0] != '\0' )
	{
		if ( gLogCDR == CDR_FILE_YES_BOTH )
		{
			rc=WriteToFifo(gMyLogConfig, message);
			rc=WriteToFifo(GV_LogConfig.Log_x_local_file, message);
		}
		else if ( gLogCDR == CDR_FILE_YES_ONLY )
		{
			rc=WriteToFifo(gMyLogConfig, message);
		}
		if ( gLogCDR == CDR_FILE_NO )
		{
			rc=WriteToFifo(GV_LogConfig.Log_x_local_file, message);
		}

	}
	else
	{
		rc=WriteToFifo(GV_LogConfig.Log_x_local_file, message);
	}
}
else
{
	if ( gMyLogConfig[0] != '\0' )
	{
		rc=WriteLocal(gMyLogConfig, message, LOCAL_LOG);
		if ( gLogCDR == CDR_FILE_YES_BOTH )
		{
			if(strlen(GV_Log_Directory_Name) == 0)
			{
				rc=WriteLocal(GV_LogConfig.Log_x_local_file, message,
							LOCAL_LOG);
			}
			else
			{
				rc=WriteLocal(GV_Log_Directory_Name, message, LOCAL_LOG);
			}
		}
	}
	else
	{
		if(strlen(GV_Log_Directory_Name) == 0)
		{
			rc=WriteLocal(GV_LogConfig.Log_x_local_file, message, LOCAL_LOG);
		}
		else
		{
			rc=WriteLocal(GV_Log_Directory_Name, message, LOCAL_LOG);
		}
	}
}

return(rc);
} /* lm_message */

int newLmMsg( mode, PT_object, MessageID, module, version, format, PT_prefix, m_id, resourceName, msgbuf)
int	mode;
char	*PT_object;
int	MessageID;
char	*module;
int	version;
char	*format;
char	*PT_prefix;
int	m_id;
char	*resourceName;
char	*msgbuf;
{
FILE	*fp;
char	rep_mode;
char	message[1025];
time_t	the_time;
char	timebuf[64];
static	int	first_time = 0, rc;
static	struct	utsname	sys_info;
static	int	pid;
static	char	logname[40];
static uid_t	the_uid;
static struct	passwd	*the_passwd;
struct tm	*PT_time;
char lLogId[5];

char fifoname[BUFSZ], logpath[BUFSZ];
char server[32], Nodename[32];
static char     mod[] = "LogIt";

struct timeval	tp;
struct timezone	tzp;
struct tm	*tm;
char		*p;
char		logMsgERRConfig[256];

if (first_time == 0)
	{
	uname (&sys_info);
	pid = getpid();
	the_uid = getuid();	/* Cannot fail */
	if((the_passwd = getpwuid(the_uid)) == (struct passwd *)NULL)
		strcpy(logname, "NULL");
	else
		sprintf(logname, "%s", the_passwd->pw_name);

	rc=removeWhiteSpace(GV_LogConfig.Log_PrimarySrv);

	if (strlen(GV_LogConfig.Log_PrimarySrv) == 0)
	{
		sprintf(GV_LogConfig.Log_PrimarySrv, "%s", sys_info.nodename);
	}
	first_time = 1;
	}

gSpecialLog = 0;
if ( gLogCDR == -1 )
{
	setLogCDRs();
}
memset(gMyLogConfig, '\0', sizeof(gMyLogConfig));
memset(logMsgERRConfig, '\0', sizeof(logMsgERRConfig));

sprintf(lLogId, "%s", "ISP");
switch (mode)
	{
	case	REPORT_NORMAL :
		rep_mode =  'N';
		break;
	case	REPORT_VERBOSE:
		rep_mode =  'V';
		break;
	case	REPORT_DETAIL:
		rep_mode =  'D';
		break;
	case	REPORT_DEBUG  :
	case	REPORT_DIAGNOSE :
		rep_mode =  'd';
		break;
	case	REPORT_STDWRITE:
		//rep_mode =  'E';
		rep_mode = ' '; // AT-2
		break;
   	case REPORT_CEV:
	case REPORT_EVENT:
		gSpecialLog = 1;
		if ( mode == REPORT_EVENT)	// AT-2
		{
			rep_mode =  'E';
		}
		else
		{
			rep_mode =  'N';
		}
		sprintf(lLogId, "%s", "CEV");
		if ( gLogCDR > CDR_FILE_NO )		// BT-71
		{
			if(strlen(GV_Log_Directory_Name) == 0)
			{
				sprintf(gMyLogConfig, "%s", GV_LogConfig.Log_x_local_file);
			}
			else
			{
				sprintf(gMyLogConfig, "%s", GV_Log_Directory_Name);
			}
			p = &(gMyLogConfig[strlen(gMyLogConfig) - 3]);
			sprintf(p, "%s", "CDR");
		}
		break;
   	case REPORT_CDR:
		rep_mode =  'N';
		gSpecialLog = 1;
		sprintf(lLogId, "%s", "CDR");
		if ( gLogCDR > CDR_FILE_NO )
		{
			if(strlen(GV_Log_Directory_Name) == 0)
			{
				sprintf(gMyLogConfig, "%s", GV_LogConfig.Log_x_local_file);
			}
			else
			{
				sprintf(gMyLogConfig, "%s", GV_Log_Directory_Name);
			}
			p = &(gMyLogConfig[strlen(gMyLogConfig) - 3]);
			sprintf(p, "%s", "CDR");
		}
		break;
	default:
		rep_mode = 'N';
		break;
	}

	if ( ( gLogERR == ERR_FILE_YES ) &&
	     ( strncmp(msgbuf, "ERR:", 4) == 0 ) )
	{
		if(strlen(GV_Log_Directory_Name) == 0)
		{
			sprintf(logMsgERRConfig, "%s", GV_LogConfig.Log_x_local_file);
		}
		else
		{
			sprintf(logMsgERRConfig, "%s", GV_Log_Directory_Name);
		}
		p = &(logMsgERRConfig[strlen(logMsgERRConfig) - 3]);
		sprintf(p, "%s", "ERR");
	}

memset((char *)timebuf, 0, sizeof(timebuf));

if(GV_DateFormat == 1)
{
	struct tm tempTM;
	gettimeofday(&tp, &tzp);

	tm = localtime_r(&tp.tv_sec, &tempTM);
	sprintf(timebuf, "%02d:%02d:%04d %02d:%02d:%02d:%03d", 
    		tm->tm_mon + 1, tm->tm_mday, tm->tm_year+1900, tm->tm_hour, 
    		tm->tm_min, tm->tm_sec, tp.tv_usec / 1000);
}
else
{
	struct tm tempTM;
	time(&the_time);
	PT_time=localtime_r(&the_time, &tempTM);
	strftime(timebuf, sizeof(timebuf) ,"%h %d %T %Y", PT_time);
}

/*
sprintf(message, "%s|%c|%s|%s|%d|%s|%d|%s|%s|%s|%d!%s%d - %s\n",
	log_id, rep_mode, timebuf, PT_object, MessageID,
	sys_info.nodename, pid, logname, module, GV_AppId,
	version, PT_prefix, MessageID, msgbuf);
*/
sprintf(message, "%s|%c|%s|%s|%d|%s|%d|%s|%s|%s|%s\n",
	lLogId, rep_mode, timebuf, PT_object, MessageID, sys_info.nodename,
	 pid, resourceName, module, GV_Application_Name, msgbuf);

if (strcmp(GV_LogConfig.Log_PrimarySrv, sys_info.nodename))
{
	if ( logMsgERRConfig[0] != '\0' )
	{
		if ( gSpecialLog == 1 )  // only send CDR/CEV records to remote system
		{
			rc=WriteToFifo(logMsgERRConfig, message);
		}
		else
		{
			rc=WriteLocal(logMsgERRConfig, message, FORWARD_LOG);
		}
	}

	if ( gMyLogConfig[0] != '\0' )
	{
		if ( gLogCDR == CDR_FILE_YES_BOTH )
		{
	        rc=WriteToFifo(gMyLogConfig, message);
			rc=WriteToFifo(GV_LogConfig.Log_x_local_file, message);
		}
        else if ( gLogCDR == CDR_FILE_YES_ONLY )
        {
            rc=WriteToFifo(gMyLogConfig, message);

        }
	}
	else
	{
		if ( gSpecialLog != 1 )  // only send CDR/CEV records to remote system
		{
			rc=WriteLocal(GV_LogConfig.Log_x_local_file, message, FORWARD_LOG);
		}
		else
		{
			if ( gLogCDR == CDR_FILE_YES_BOTH )
			{
	            rc=WriteToFifo(gMyLogConfig, message);
				rc=WriteToFifo(GV_LogConfig.Log_x_local_file, message);
			}
	        else if ( gLogCDR == CDR_FILE_YES_ONLY )
	        {
	            rc=WriteToFifo(gMyLogConfig, message);
	        }
	        if ( gLogCDR == CDR_FILE_NO )
	        {
	            rc=WriteToFifo(GV_LogConfig.Log_x_local_file, message);
	        }
		}
	}
}
else
{
	if ( logMsgERRConfig[0] != '\0' )
	{
		if ( gSpecialLog == 1 )  // only send CDR/CEV records to remote system
		{
			rc=WriteToFifo(logMsgERRConfig, message);
		}
		else
		{
			rc=WriteLocal(logMsgERRConfig, message, LOCAL_LOG);
		}
	}

	if ( gMyLogConfig[0] != '\0' )
	{
		if ( gLogCDR == CDR_FILE_YES_BOTH )
		{

			rc=WriteLocal(gMyLogConfig, message, LOCAL_LOG);
			rc=WriteLocal(GV_LogConfig.Log_x_local_file, message, LOCAL_LOG);

//	        rc=WriteToFifo(gMyLogConfig, message);
//			rc=WriteToFifo(GV_LogConfig.Log_x_local_file, message);
		}
        else if ( gLogCDR == CDR_FILE_YES_ONLY )
        {
			rc=WriteLocal(gMyLogConfig, message, LOCAL_LOG);
        }
	}
	else
	{
		if ( gSpecialLog != 1 )  // only send CDR/CEV records to remote system
		{
			rc=WriteLocal(GV_LogConfig.Log_x_local_file, message, LOCAL_LOG);
		}
		else
		{
			if ( gLogCDR == CDR_FILE_YES_BOTH )
			{
//	            rc=WriteToFifo(gMyLogConfig, message);
//				rc=WriteToFifo(GV_LogConfig.Log_x_local_file, message);
				rc=WriteLocal(gMyLogConfig, message, LOCAL_LOG);
				rc=WriteLocal(GV_LogConfig.Log_x_local_file, message, LOCAL_LOG);
			}
	        else if ( gLogCDR == CDR_FILE_YES_ONLY )
	        {
	            //rc=WriteToFifo(gMyLogConfig, message);
				rc=WriteLocal(gMyLogConfig, message, LOCAL_LOG);
	        }
	        if ( gLogCDR == CDR_FILE_NO )
	        {
	            //rc=WriteToFifo(GV_LogConfig.Log_x_local_file, message);
				rc=WriteLocal(GV_LogConfig.Log_x_local_file, message, LOCAL_LOG);
	        }
		}
	}
	return(rc);
}
} /* newLmMsg */
/*--------------------------------------------------------
lm_init() : initialize the application logging .
----------------------------------------------------------*/
int lm_init(log_opt) 
char	*log_opt;
{
return (0);
} /* lm_init */
/*--------------------------------------------------------
lm_log_id() : set log identification for application.
----------------------------------------------------------*/
int	lm_log_id(id)
char	*id;
{
sprintf(log_id, "%s", id);
return (0);
} /* lm_log_id */
/*--------------------------------------------------------
lm_key_function() : set key function.
----------------------------------------------------------*/
int	lm_key_function( ISP_ApplicationId, max_len )
char	*ISP_ApplicationId;
int	max_len;
{
appl_id[0] = '\0';
sprintf(appl_id, "%s", GV_AppId);
return (0);
} /* lm_key_function */
/*--------------------------------------------------------
lm_select() : Allow applicatin to set log options.
----------------------------------------------------------*/
int 	lm_select() 
{
return (0);
} /* lm_select */
/*--------------------------------------------------------
lm_exit() : applicatin to exit logging .
----------------------------------------------------------*/
lm_exit()
{
return (0);
} /* lm_exit */
/*--------------------------------------------------------
lm_unselect() : applicatin to exit logging .
----------------------------------------------------------*/
lm_unselect(unsigned int rep_mode, unsigned int log_option)
{
return (0);
}

static int WriteLocal(LogPath_1, Message, forward_sw)
/*------------------------------------------------------*/
/* Purpose: Output Message to a local log file		*/
/*	    LogPath_1.	This appends the date and time	*/
/*	    to log path	and links it to the ISP.cur	*/
/*	    file. If forward_sw is set to FORWARD_LOG,	*/
/*	    EXTENSION is also appended to the log path. */
/*	    If the open or write fails, call		*/
/*	    WriteLocal_2() to write to the secondary	*/
/*	    file.					*/
/*------------------------------------------------------*/
char *LogPath_1, *Message; 
int forward_sw;
{
int	LinkFile = 0, rc;
FILE	*fp;
char	CurFile[PATHSZ], LogFile[PATHSZ], DateTime[PATHSZ];
time_t	t;
static char     mod[] = "WriteLocal";
struct tm	*PT_time;
struct tm tempTM;

time(&t);
PT_time=localtime_r(&t, &tempTM);
strftime(DateTime, sizeof(DateTime) ,"%Y-%h%d-%H", PT_time);

//fprintf(stdout, "RGDEBUG:%s::%d:: Log Path %s\n", __FILE__, __LINE__, LogPath_1);fflush(stdout);

if (forward_sw == FORWARD_LOG)
	sprintf(LogFile,"%s.%s.%s",LogPath_1,DateTime,EXTENSION);
else
	sprintf(LogFile,"%s.%s",LogPath_1,DateTime);

sprintf(CurFile,"%s.cur",LogPath_1);

if (access(LogFile, F_OK) == -1)
	{
	/* if new log file */
	/* remove old link and link ISP.cur file to new file */
	unlink(CurFile);
	LinkFile = 1;		/* link file after creation */
	}

if((fp=fopen(LogFile, "a+")) == NULL)
	{
#ifdef DEBUG
	sprintf(logBuf,"Unable to open log file %s errno = %d", 
							LogFile, errno);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);

	sprintf(logBuf,"Failed to log Message = %s", Message);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);

#endif
	return(WriteLocal_2(Message,forward_sw));
	}

if ((rc=fprintf(fp, "%s", Message)) < 0)
	{
	fclose(fp);
#ifdef DEBUG
	sprintf(logBuf,"Unable to write to log file %s errno = %d",
			LogFile, errno);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
	sprintf(logBuf,"Failed to log Message = %s", Message);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
#endif
        return(WriteLocal_2(Message,forward_sw));
	}

if ((rc=fflush(fp)) < 0)
	{
	fclose(fp);
#ifdef DEBUG
	sprintf(logBuf,"Unable to flush output to log file %s errno = %d",
			LogFile, errno);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
	sprintf(logBuf,"Failed to log Message = %s", Message);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
#endif
        return(WriteLocal_2(Message,forward_sw));
	}

fclose(fp);

if (LinkFile == 1)
	symlink(LogFile, CurFile);
}

static int WriteLocal_2(Message, forward_sw)
/*------------------------------------------------------*/
/* Purpose: Output Message to a local log file		*/
/*	    LogPath_2.	This appends the date and time	*/
/*	    to log path	and links it to the ISP.cur	*/
/*	    file.  If forward_sw is set to FORWARD_LOG,	*/
/*	    EXTENSION is also appended to the log path. */
/*------------------------------------------------------*/
char	*Message; 
int	forward_sw;
{
int	LinkFile = 0, rc;
char	LogPath_1[PATHSZ], LogPath_2[PATHSZ];
FILE	*fp2;
char	CurFile[PATHSZ], LogFile[PATHSZ], DateTime[PATHSZ];
time_t	t;
static char     mod[] = "WriteLocal_2";
struct tm	*PT_time;
struct tm tempTM;

time(&t);
PT_time=localtime_r(&t, &tempTM);
strftime(DateTime, sizeof(DateTime) ,"%Y-%h%d-%H", PT_time);

GetLogPaths(LogPath_1,LogPath_2);

//fprintf(stdout, "RGDEBUG:%s::%d:: Log Path 1 %s, Log Path 2 %s\n", __FILE__, __LINE__, LogPath_1, LogPath_2);fflush(stdout);
if (forward_sw == FORWARD_LOG)
	sprintf(LogFile,"%s.%s.%s",LogPath_2,DateTime,EXTENSION);
else
	sprintf(LogFile,"%s.%s",LogPath_2,DateTime);

//fprintf(stdout, "RGDEBUG:%s::%d:: Log File %s\n", __FILE__, __LINE__, LogFile);fflush(stdout);
sprintf(CurFile,"%s.cur",LogPath_2);

//fprintf(stdout, "RGDEBUG:%s::%d:: Cur File %s\n", __FILE__, __LINE__, CurFile);fflush(stdout);

if (access(LogFile, F_OK) == -1)
	{
	/* if new log file */
	/* remove old link and link CurFile file to new file */
	unlink(CurFile);
	LinkFile = 1;		/* link file after creation */
	}

if((fp2=fopen(LogFile, "a+")) == NULL)
	{
#ifdef DEBUG
	/* This is the last chance. Send an alarm here ???? */
	sprintf(logBuf,"Unable to open log file %s errno = %d", 
							LogFile, errno);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
	sprintf(logBuf,"Failed to log Message = %s", Message);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
#endif
	return;
	}

if ((rc=fprintf(fp2, "%s", Message)) < 0)
	{
	fclose(fp2);
#ifdef DEBUG
	/* This is the last chance. Send an alarm here ???? */
	sprintf(logBuf,"Unable to write to log file %s errno = %d",
			LogFile, errno);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
	sprintf(logBuf,"Failed to log Message = %s", Message);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
#endif
	return;
	}

if ((rc=fflush(fp2)) < 0)
	{
	fclose(fp2);
#ifdef DEBUG
	/* This is the last chance. Send an alarm here ???? */
	sprintf(logBuf,"Unable to flush output to log file %s errno = %d",
			LogFile, errno);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
	sprintf(logBuf,"Failed to log Message = %s", Message);
	LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
#endif
        return;
	}

fclose(fp2);

if (LinkFile == 1)
	symlink(LogFile, CurFile);

}

int WriteToFifo(logpath1, message)
/*------------------------------------------------------*/
/* Purpose: Write message to fifo, where it will be	*/
/*	    read and sent to the log server.  If the	*/
/*	    write fails for any reason, the will be	*/
/*	    written locally.				*/
/*------------------------------------------------------*/
char *logpath1, *message;
{
char 	fifoname[BUFSZ];
int	wfifo_fd, i, j, bytesout, ret_code;
static char	mod[]="WriteToFifo";
char	baseDirBuffer[80];
char	*BaseDir;

memset(baseDirBuffer, 0, sizeof(baseDirBuffer));

BaseDir=(char *)getenv("ISPBASE");
if(BaseDir == NULL)
{
	getDefaultISPBase(baseDirBuffer);
	if(baseDirBuffer[0] == '\0')
	{
		fprintf(stderr, "Env variable ISPBASE is set to NULL.\n");
		fflush(stderr);
	}
}
else
{
	sprintf(baseDirBuffer, "%s", BaseDir);
}

sprintf(fifoname,"%s/Global/%s", baseDirBuffer,FIFONAME);

/* sprintf(fifoname,"%s/Global/%s",(char *)getenv("ISPBASE"),FIFONAME); */

if(access( fifoname, W_OK ) == -1 )
	{
	ret_code=WriteLocal(logpath1, message, FORWARD_LOG);
	return(ret_code);
	}

if((wfifo_fd=open(fifoname,O_WRONLY|O_NONBLOCK)) == -1 )
	{
	ret_code=WriteLocal(logpath1, message, FORWARD_LOG);
	return(ret_code);
	}

if ((bytesout=write(wfifo_fd, message, strlen(message))) < 0 )
	{
	if ( errno == EAGAIN )
	{
		j=0;
		while (j<2)
			{
			util_sleep(0,100);
			if ( (bytesout=write(wfifo_fd, message,
					strlen(message)))<0 )
				j++;
			else
				{
				ret_code=0;
				break;
				}
			}
		if (j==2)
		{
			ret_code=WriteLocal(logpath1, message, FORWARD_LOG);
		}
	}
	else
	{
		ret_code=WriteLocal(logpath1, message, FORWARD_LOG);
	}
	}

close(wfifo_fd);

return(ret_code);
}

void GetLogPaths(char *LogPath_1, char *LogPath_2)
/*------------------------------------------------------*/
/* Purpose: Reads the global configuration file		*/
/*	    ($ISPBASE/Global/.Global.cfg, to get the	*/
/*	    get the paths for the log files.		*/
/*------------------------------------------------------*/
{
FILE	*fp;
char	ConfigPath[256], *BaseDir;
char	line[1024];
char	*tmpbuff, *subject, *basename;
static char     mod[] = "GetLogPaths";
//void	ChkPath(char *);
char	NewLogPath[256];
char	baseDirBuffer[80];

char    *yStrTok = NULL;

memset(baseDirBuffer, 0, sizeof(baseDirBuffer));

BaseDir=(char *)getenv("ISPBASE");
if(BaseDir == NULL)
{
	getDefaultISPBase(baseDirBuffer);
	if(baseDirBuffer[0] == '\0')
	{
		fprintf(stderr, "Env variable ISPBASE is set to NULL.\n");
		fflush(stderr);
	}
}
else
{
	sprintf(baseDirBuffer, "%s", BaseDir);
}

sprintf(ConfigPath,"%s/Global/.Global.cfg", baseDirBuffer);

if((fp=fopen(ConfigPath, "r")) == NULL)
	{
	sprintf(LogPath_1, "%s/LOG/ISP.", baseDirBuffer);
	sprintf(logBuf,"Unable to open %s.  errno = %d", ConfigPath, errno);
	//LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
	//sprintf(logBuf,"Using %s to log.",LogPath_1);
	//LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
	}
else
	{
	LogPath_1[0]='\0';
	LogPath_2[0]='\0';
	while(lr_readline(line, BUFSZ, fp) == 1)
		{
		tmpbuff=line;
		subject=(char *)strtok_r(tmpbuff, "=", &yStrTok);
		tmpbuff += strlen(subject) + 1;
		if (! strcmp(subject,LOG_X_FILE) )
			{
			basename=(char *)strtok_r(tmpbuff, "=", &yStrTok);

			ChkPath(basename, NewLogPath);
			sprintf(basename,"%s", NewLogPath);

			sprintf(LogPath_1, "%s", basename);
			}
		else if (! strcmp(subject,LOG_2X_FILE) )
			{
			basename=(char *)strtok_r(tmpbuff, "=", &yStrTok);

			ChkPath(basename, NewLogPath);
			sprintf(basename,"%s", NewLogPath);

			sprintf(LogPath_2, "%s", basename);
			}
		}

	if ( LogPath_1[0] == '\0' )
		{
		sprintf(LogPath_1, "%s/LOG/ISP", baseDirBuffer);

		sprintf(logBuf,"%s not found in %s", LOG_X_FILE, ConfigPath);
		LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
		sprintf(logBuf,"Using %s to log.",LogPath_1);
		LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
		}
	else
		lr_strip_str(LogPath_1);

	if ( LogPath_2[0] == '\0' )
		{
		sprintf(LogPath_2, "%s/temp/ISP", baseDirBuffer);
		sprintf(logBuf,"%s not found in %s", LOG_2X_FILE, ConfigPath);
		LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
		sprintf(logBuf,"Using %s to log.",LogPath_2);
		LOGXXPRT(mod,REPORT_NORMAL,COM_ISPDEBUG, logBuf);
		}
	else
		lr_strip_str(LogPath_2);
	}

if(fp != NULL)
	fclose(fp);
}


int lr_strip_str(char *str)
/*--------------------------------------------------------------------
lr_strip_str(): 	Strip the leading and trailing white space off of the
		string.
--------------------------------------------------------------------*/
{
static char	*ptr;
int i=0;
static char	mod[]="lr_strip_str";

if(!(int)strlen(str))
	return(0);

ptr = str;
while (*ptr != '\0')
	if (! isspace(*ptr))
		str[i++]=*ptr++;
	else
		ptr++;

str[i]='\0';

return(0);
} /* lr_strip_str() */


void ChkPath(char *PathName, char *NewPath)
{
//static char	NewPath[PATHSZ];
char		TmpPath[PATHSZ];
int		slen, i;
struct stat	sbuf;

stat(PathName, &sbuf);

slen=strlen(PathName);
if ( (sbuf.st_mode & S_IFDIR) == S_IFDIR )
	{
	if (PathName[slen-1] == '/')
		sprintf(NewPath, "%s%s", PathName, DEF_BASENAME);
	else
		sprintf(NewPath, "%s/%s", PathName, DEF_BASENAME);
	}
else
	sprintf(NewPath, "%s", PathName);
	
//return((char *)NewPath);
}
/*-----------------------------------------------------------------------------
Purpose:        To provide a routine that can sleep for a specified number
                of seconds and milliseconds.
-----------------------------------------------------------------------------*/
static int util_sleep(int Seconds, int Milliseconds)
{
        static struct timeval SleepTime;
	static char	mod[]="util_sleep";

        SleepTime.tv_sec = Seconds;
        SleepTime.tv_usec = Milliseconds * 1000;

        select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &SleepTime);
}

int lr_readline(char *the_line, int maxlen, FILE *fp)
/*--------------------------------------------------------------------
lr_readline(): Read a line from the file pointer, strip leading spaces,
		make sure it is not a comment (#) and return it.
	    The fp is presumed to be a pointer to an open file.
--------------------------------------------------------------------*/
{
static char	*ptr;
static char	mybuf[BUFSZ];
static char	mod[]="lr_readline";

the_line[0]='\0';
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
	sprintf(the_line, "%s", mybuf);
	return(1);
	} /* while */
return(0);
} /* lr_readline() */

/*--------------------------------------------------------------------
int removeWhiteSpace(): Remove all whitespace characters from sting.
--------------------------------------------------------------------*/
int removeWhiteSpace(char *buf)
{
        char    *ptr;
        char    workBuf[1024];
        int     rc;
        int     i=0;

        memset(workBuf, 0, sizeof(workBuf));

        ptr=buf;

        while ( *ptr != '\0' )
        {
                if (isspace(*ptr))
                        ptr++;
                else
                        workBuf[i++]=*ptr++;
        }
        workBuf[i]=*ptr;

        sprintf(buf,"%s", workBuf);

        return(strlen(buf));
}

static void setLogCDRs()
{
	int			rc;
	char		globalCfgFile[256];
	char 		*ispbase;
	char		value[32];
	char		buf[128];
	char		msg[256];

	gLogCDR = CDR_FILE_NO;
	gLogERR = ERR_FILE_NO;

    /* Determine base pathname. */
    ispbase= (char *)getenv("ISPBASE");
	if(ispbase == NULL)
	{
		return;
	}
	sprintf(globalCfgFile, "%s/Global/.Global.cfg", ispbase);

    sprintf(value, "%s", "LOG_CDR_FILE");
    memset((char *)buf, '\0', sizeof(buf));
    rc = my_get_name_value("", value, "",
    			    buf, sizeof(buf), globalCfgFile, msg);
    if (rc == 0)
    {
	    if(strcmp(buf, "YES_ONLY") == 0)
		{
			gLogCDR = CDR_FILE_YES_ONLY;
		}
		else if(strcmp(buf, "YES_BOTH") == 0)
		{
			gLogCDR = CDR_FILE_YES_BOTH;
		}
	}

    sprintf(value, "%s", "LOG_ERR_FILE");
    memset((char *)buf, '\0', sizeof(buf));
    rc = my_get_name_value("", value, "",
    			    buf, sizeof(buf), globalCfgFile, msg);
    if (rc == 0)
    {
    	if(strncmp(buf, "YES", 3) == 0)
		{
			gLogERR = ERR_FILE_YES;
		}
	}


	return;

} // END: setLogCDRs() 
/*------------------------------------------------------------------------------
my_get_name_value():
------------------------------------------------------------------------------*/
static int my_get_name_value(char *section, char *name, char *defaultVal,
			char *value, int bufSize, char *file, char *err_msg)
{
	static char	mod[] = "util_get_name_value";
	FILE		*fp;
	char		line[512];
	char		currentSection[80],lhs[256], rhs[256];
	int		found, inDesiredSection=0, sectionSpecified=1;

	sprintf(value, "%s", defaultVal);
	err_msg[0]='\0';

	if(section[0] == '\0')
	{
		/* If no section specified, we're always in desired section */
		sectionSpecified=0;
		inDesiredSection=1;	
	}
	if(name[0] == '\0')
	{
		sprintf(err_msg,"No name passed to %s.", mod);
		return(-1);
	}
	if(file[0] == '\0')
	{
		sprintf(err_msg,"No file name passed to %s.", mod);
		return(-1);
	}
	if(bufSize <= 0)
	{
		sprintf(err_msg,
		"Return bufSize (%d) passed to %s must be > 0.", bufSize, mod);
		return(-1);
	}

	if((fp = fopen(file, "r")) == NULL)
	{
		sprintf(err_msg,"Failed to open file <%s>. errno=%d. [%s]",
				file, errno, strerror(errno));
		return(-1);
	}

	memset(line, 0, sizeof(line));

	found = 0;
	while(fgets(line, sizeof(line)-1, fp))
	{
		/*  Strip \n from the line if it exists */
		if(line[(int)strlen(line)-1] == '\n')
		{
			line[(int)strlen(line)-1] = '\0';
		}

		my_trim_whitespace(line);

		if(!inDesiredSection)
		{
			if(line[0] != '[')
			{
				memset(line, 0, sizeof(line));
				continue;
			}

			memset(currentSection, 0, sizeof(currentSection));
			sscanf(&line[1], "%[^]]", currentSection);

			if(strcmp(section, currentSection) != 0)
			{
				memset(line, 0, sizeof(line));
				continue;
			}
			inDesiredSection = 1;
			memset(line, 0, sizeof(line));
			continue;
		} 
		else
		{
			/* If we are already in a section and have encountered
			  another section AND a section was specified then
			   get out of the loop.  */
			if( line[0] == '[' && sectionSpecified )
			{
				memset(line, 0, sizeof(line));
				break;
			}
		} 

		memset(lhs, 0, sizeof(lhs));
		memset(rhs, 0, sizeof(rhs));

		if(sscanf(line, "%[^=]=%[^=]", lhs, rhs) < 2)
		{
			memset(line, 0, sizeof(line));
			continue;
		}
		my_trim_whitespace(lhs);
		if(strcmp(lhs, name) != 0)
		{
			memset(line, 0, sizeof(line));
			continue;
		}
		found = 1;
		sprintf(value, "%.*s", bufSize, rhs);
		break;
	} /* while fgets */
	fclose(fp);
	if (!found)
	{
		if (sectionSpecified)
			sprintf(err_msg,
			"Failed to find <%s> under section <%s> in file <%s>.", 
				name, section, file);
		else
			sprintf(err_msg,
			"Failed to find <%s> entry in file <%s>.", name, file);
		return(-1);
	}
	return(0);
} // END: my_get_name_value()

/*----------------------------------------------------------------------------
Trim white space from the front and back of a string.
----------------------------------------------------------------------------*/
static int my_trim_whitespace(char *string)
{
	int sLength;
	char *tmpString;
	char *ptr;

	if((sLength = strlen(string)) == 0) return(0);

	tmpString = (char *)calloc((size_t)(sLength+1), sizeof(char));
	ptr = string;

	if ( isspace(*ptr) )
	{
		while(isspace(*ptr)) ptr++;
	}

	sprintf(tmpString, "%s", ptr);
	ptr = &tmpString[(int)strlen(tmpString)-1];
	while(isspace(*ptr))
	{
		*ptr = '\0';
		ptr--;
	}

	sprintf(string, "%s", tmpString);
	free(tmpString);
	return(0);
} // END: my_trim_whitespace()
