/*
Program: log_routines.c

Purpose:
	Common routines used by the socket-based logging
	mechanism.

  void GetLogPaths(char *LogPath_1, char *LogPath_2)
	      - Reads the global configuration file
		($ISPBASE/Global/.Global.cfg, to get the
		get the pathnames and prefix for the log files.
		
  int GetLogServer(char *Server, char *NodeName)
	      - Reads the global configuration file
		($ISPBASE/Global/.Global.cfg, to get the
		get the LOG_PRIMARY_SERVER entry.
		Returns the LOG_PRIMARY_SERVER name and system
		nodename.
		
  int WriteLocal((char *)LogPath_1, (char *)Message, int forward_sw)
	      - Output Message to a local log file LogPath_1.
		This appends the date and time to log path
		and links it to the ISP.cur file.  If forward_sw is set
		to FORWARD_LOG, EXTENSION is also appended to the log path.
		If it fails to write to LogPath_1, calls WriteLocal_2().
		LogPath_2.

  int WriteLocal_2((char *)Message, int forward_sw)
	      - Calls GetLogPaths to get the LOG_2x_LOCAL_FILE entry, then
		outputs Message to a local log file LogPath_2.	This
		appends the date and time to log path and links it to
		the ISP.cur file.  If forward_sw is set to FORWARD_LOG,
		EXTENSION is also appended to the log path. 

  int WriteToFifo((char *)logpath1, (char *)message)
	      - Purpose: Write message to fifo, where it will be
		read and sent to the log server.  If the write fails
		for any reason, the will be written locally.
		
  int lr_readline(the_line,maxlen,fp)
	      - Read a line from the file pointer, strip leading spaces,
		make sure it is not a comment (#) and return it.


Update: 	03/03/97 djb	Inserted fclose() statements for the .Global.cfg file.
Update:		03/14/97 djb	Cleaned up log messages.
Update:		05/01/97 sja	Made util_sleep static. Also added prototype.
Update:		05/12/97 djb	Added validation of pathnames in GetLogPaths().
Update:		05/13/97 djb	Added DelReadRecords() so log_svc can use it.
Update:		05/21/97 mpb	added %s to sprintf.
Update:		10/30/97 djb	changed all logging to gaVarLog()
Update:		11/04/97 djb	included function declarations
Update:		11/05/97 djb	fixed the SCO NULL issue in GetLogServer()
Update:		11/06/97 djb	replaced * pointer logic with [] pointer logic
Update:		11/06/97 djb	remove lr_strip_str
Update:	02/04/99 djb	changed cftime() to strftime(); SNA EXPRESS
Update:	11/29/99 gpw	removed fprintf to std out , debug msg
Update:	2000/12/04 sja	Appended return(0) to WriteLocal() and WriteLocal_2()
Update:	2002/04/09 djb	changed mktemp to mkstemp for linux os migration
Update: 07/16/2010 rg,ddn Replaced localtime with thread safe localtime_r
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/select.h>
#include "gaUtils.h"

#define LOG_SERVER_CLIENT
#include "log_defs.h"

static int	getField(char delim, char *buffer, int fld_num, char *field);
static int	util_sleep(int Seconds, int Milliseconds);
static int	WriteLocal_2(char *Message, int forward_sw);
static int	lr_readline(char *the_line, int maxlen, FILE *fp);
static int	ChkPath(char *PathName, char *defaultPath, char *outPath);
static int	removeWhiteSpace(char *buf);

int GetLogForwardEncryption(int *zLogForwardEncryption)
/*------------------------------------------------------*/
/* Purpose: Reads the global configuration file		*/
/*	    ($ISPBASE/Global/.Global.cfg, to get the	*/
/*	    get the LOG_FORWARD_ENCRYPTION name.            */
/*      Returns *zLogForwardEncryption set to 		*/
/*      0 (off-default) or 1 (on).						*/
/*------------------------------------------------------*/
{
FILE		*fp;
char		ConfigPath[256], *BaseDir;
char		line[1024], keyWord[128], value[128];
static char	mod[] = "GetLogForwardEncryption";
int		rc;

*zLogForwardEncryption = 0;

BaseDir=(char *)getenv("ISPBASE");

sprintf(ConfigPath,"%s/Global/.Global.cfg", BaseDir);

if((fp=fopen(ConfigPath, "r")) == NULL)
{
	gaVarLog(mod, 0,"Unable to open %s.  errno = %d", ConfigPath, errno);
	gaVarLog(mod, 0,"Setting encryption to OFF (%d).", *zLogForwardEncryption);
	return(0);
}
else
{
	memset((char *)line, 0, sizeof(line));
	while(lr_readline(line, 1024, fp) == 1)
	{
		memset((char *)keyWord, 0, sizeof(keyWord));
		memset((char *)value, 0, sizeof(value));
		rc=removeWhiteSpace(line);

		if ((rc=getField('=', line, 1, keyWord)) == -1)
		{
			memset((char *)line, 0, sizeof(line));
			continue;
		}
		
		if ( strcmp(keyWord, "LOG_FORWARD_ENCRYPTION") == 0 )
		{
			if ((rc=getField('=', line, 2, value)) == -1)
			{
				memset((char *)line, 0, sizeof(line));
				continue;
			}
			if ( (strcmp(value, "ON") == 0) ||
			     (strcmp(value, "On") == 0) ||
			     (strcmp(value, "on") == 0) )
			{
				*zLogForwardEncryption = 1;
				gaVarLog(mod, 0, "Found LOG_FORWARD_ENCRYPTION as (%s) in (%s).  "
						"Setting encryption to ON (%d).",
						value, ConfigPath, *zLogForwardEncryption);
			}
				
		}
	}

}

fclose(fp);
return(0);

}
int GetLogServer(char *Server, char *NodeName)
/*------------------------------------------------------*/
/* Purpose: Reads the global configuration file		*/
/*	    ($ISPBASE/Global/.Global.cfg, to get the	*/
/*	    get the LOG_PRIMARY_SERVER name.  Returns	*/
/*	    the LOG_PRIMARY_SERVER name and system	*/
/*	    nodename.					*/
/*------------------------------------------------------*/
{
FILE		*fp;
char		ConfigPath[256], *BaseDir;
char		line[1024], keyWord[128], value[128];
static char	mod[] = "GetLogServer";
struct utsname	SysInfo;
int		rc;

BaseDir=(char *)getenv("ISPBASE");

sprintf(ConfigPath,"%s/Global/.Global.cfg", BaseDir);

if ((rc=uname(&SysInfo)) == -1)
	{
	gaVarLog(mod, 0,"Unable to get uname. errno=%d.", errno);
	SysInfo.nodename[0]='\0';
	}

sprintf(NodeName, "%s", SysInfo.nodename);

if((fp=fopen(ConfigPath, "r")) == NULL)
	{
	if (SysInfo.nodename[0]=='\0')
		return(-1);

	sprintf(Server, "%s", SysInfo.nodename);
	gaVarLog(mod, 0,"Unable to open %s.  errno = %d", ConfigPath, errno);
	gaVarLog(mod, 0,"Logging locally to %s.",Server);
	}
else
	{
	memset((char *)line, 0, sizeof(line));
	while(lr_readline(line, 1024, fp) == 1)
	{
		memset((char *)keyWord, 0, sizeof(keyWord));
		memset((char *)value, 0, sizeof(value));
		rc=removeWhiteSpace(line);

		if ((rc=getField('=', line, 1, keyWord)) == -1)
		{
			memset((char *)line, 0, sizeof(line));
			continue;
		}
		
		if ( strcmp(keyWord, LOG_PRI_SERVER) == 0 )
		{
			if ((rc=getField('=', line, 2, value)) == -1)
			{
				memset((char *)line, 0, sizeof(line));
				continue;
			}
			sprintf(Server, value);
		}
	}

	if ( strlen(Server) == 0 )
		{
		if (SysInfo.nodename[0]=='\0')
			return(-1);

		sprintf(Server, "%s", SysInfo.nodename);
		gaVarLog(mod, 0,"%s not found in %s",
						LOG_PRI_SERVER, ConfigPath);
		gaVarLog(mod, 0,"Logging locally to %s.",Server);
		}
	}

fclose(fp);
return(0);

}

void GetLogPaths(char *LogPath_1, char *LogPath_2)
/*------------------------------------------------------*/
/* Purpose: Reads the global configuration file		*/
/*	    ($ISPBASE/Global/.Global.cfg, to get the	*/
/*	    get the paths for the log files.		*/
/*------------------------------------------------------*/
{
FILE		*fp;
char		ConfigPath[256];
char		defaultLog[256];
char		line[1024], keyWord[128], value[128];
char		*baseDir;
static char     mod[] = "GetLogPaths";
int		rc;
int		counter=0;

baseDir=(char *)getenv("ISPBASE");

sprintf(ConfigPath,"%s/Global/.Global.cfg", baseDir);
sprintf(defaultLog,"%s/LOG/ISP",baseDir);

if((fp=fopen(ConfigPath, "r")) == NULL)
	{
	sprintf(LogPath_1, "%s/LOG/ISP.", baseDir);
	gaVarLog(mod, 0,"Unable to open %s.  errno = %d", ConfigPath, errno);
	gaVarLog(mod, 0,"Using %s to log.",LogPath_1);
	}
else
	{
	memset((char *)line, 0, sizeof(line));
	while(lr_readline(line, 1024, fp) == 1)
	{
		memset((char *)keyWord, 0, sizeof(keyWord));
		memset((char *)value, 0, sizeof(value));
		rc=removeWhiteSpace(line);

		if ((rc=getField('=', line, 1, keyWord)) == -1)
		{
			memset((char *)line, 0, sizeof(line));
			continue;
		}
		
		if ( strcmp(keyWord, LOG_X_FILE) == 0 )
		{
			if ((rc=getField('=', line, 2, value)) == -1)
			{
				memset((char *)line, 0, sizeof(line));
				continue;
			}

			rc=ChkPath(value, defaultLog, LogPath_1);

			if (++counter == 2)
				break;
			else
			{
				memset((char *)line, 0, sizeof(line));
				continue;
			}
		}
		if ( strcmp(keyWord, LOG_2X_FILE) == 0 )
		{
			if ((rc=getField('=', line, 2, value)) == -1)
			{
				memset((char *)line, 0, sizeof(line));
				continue;
			}

			rc=ChkPath(value, defaultLog, LogPath_2);

			if (++counter == 2)
				break;
			else
			{
				memset((char *)line, 0, sizeof(line));
				continue;
			}
		}
		
	}

	if ( strlen(LogPath_1) == 0 )
		{
		sprintf(LogPath_1, "%s/LOG/ISP", baseDir);
		gaVarLog(mod, 0,"%s not found in %s", LOG_X_FILE, ConfigPath);
		gaVarLog(mod, 0,"Using %s to log.",LogPath_1);
		}

	if ( strlen(LogPath_2) == 0 )
		{
		sprintf(LogPath_2, "%s/temp/ISP", baseDir);
		gaVarLog(mod, 0,"%s not found in %s", LOG_2X_FILE, ConfigPath);
		gaVarLog(mod, 0,"Using %s to log.",LogPath_2);
		}
	}

fclose(fp);
}

int WriteLocal(char *LogPath_1, char *Message, int forward_sw)
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
{
int	LinkFile = 0, rc;
int	WriteLocal_2(char *, int);
FILE	*fp;
char	CurFile[256], LogFile[256], DateTime[256];
time_t	t;
static char     mod[] = "WriteLocal";
struct tm	*PT_time;
struct tm	temp_PT_time;

time(&t);
PT_time=localtime_r(&t, &temp_PT_time);
strftime(DateTime, sizeof(DateTime) ,"%Y-%h%d-%H", PT_time);

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
	gaVarLog(mod, 0,"Unable to open log file %s errno = %d", 
							LogFile, errno);
	gaVarLog(mod, 0,"Failed to log Message = %s", Message);
#endif
	return(WriteLocal_2(Message,forward_sw));
	}

if ((rc=fprintf(fp, "%s", Message)) < 0)
	{
	fclose(fp);
#ifdef DEBUG
	gaVarLog(mod, 0,"Unable to write to log file %s errno = %d",
			LogFile, errno);
	gaVarLog(mod, 0,"Failed to log Message = %s", Message);
#endif
        return(WriteLocal_2(Message,forward_sw));
	}

if ((rc=fflush(fp)) < 0)
	{
	fclose(fp);
#ifdef DEBUG
	gaVarLog(mod, 0,"Unable to flush output to log file %s errno = %d",
			LogFile, errno);
	gaVarLog(mod, 0,"Failed to log Message = %s", Message);
#endif
        return(WriteLocal_2(Message,forward_sw));
	}

fclose(fp);

if (LinkFile == 1)
	symlink(LogFile, CurFile);

	return(0);
}

int WriteLocal_2(char *Message, int forward_sw)
/*------------------------------------------------------*/
/* Purpose: Output Message to a local log file		*/
/*	    LogPath_2.	This appends the date and time	*/
/*	    to log path	and links it to the ISP.cur	*/
/*	    file.  If forward_sw is set to FORWARD_LOG,	*/
/*	    EXTENSION is also appended to the log path. */
/*------------------------------------------------------*/
{
int	LinkFile = 0, rc;
char	LogPath_1[256], LogPath_2[256];
FILE	*fp2;
char	CurFile[256], LogFile[256], DateTime[256];
time_t	t;
static char     mod[] = "WriteLocal_2";
struct tm	*PT_time;
struct tm	temp_PT_time;

time(&t);
PT_time=localtime_r(&t, &temp_PT_time);
strftime(DateTime, sizeof(DateTime) ,"%Y-%h%d-%H", PT_time);

GetLogPaths(LogPath_1,LogPath_2);

if (forward_sw == FORWARD_LOG)
	sprintf(LogFile,"%s.%s.%s",LogPath_2,DateTime,EXTENSION);
else
	sprintf(LogFile,"%s.%s",LogPath_2,DateTime);

sprintf(CurFile,"%s.cur",LogPath_2);

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
	gaVarLog(mod, 0,"Unable to open log file %s errno = %d", 
							LogFile, errno);
	gaVarLog(mod, 0,"Failed to log Message = %s", Message);
#endif
	return;
	}

if ((rc=fprintf(fp2, "%s", Message)) < 0)
	{
	fclose(fp2);
#ifdef DEBUG
	/* This is the last chance. Send an alarm here ???? */
	gaVarLog(mod, 0,"Unable to write to log file %s errno = %d",
			LogFile, errno);
	gaVarLog(mod, 0,"Failed to log Message = %s", Message);
#endif
	return;
	}

if ((rc=fflush(fp2)) < 0)
	{
	fclose(fp2);
#ifdef DEBUG
	/* This is the last chance. Send an alarm here ???? */
	gaVarLog(mod, 0,"Unable to flush output to log file %s errno = %d",
			LogFile, errno);
	gaVarLog(mod, 0,"Failed to log Message = %s", Message);
#endif
        return;
	}

fclose(fp2);

if (LinkFile == 1)
	symlink(LogFile, CurFile);


	return(0);
}

int DelReadRecords(char *filename, FILE *fp, char *buf)
/*--------------------------------------------------------------*/
/* Purpose: For the already open file desciptor fp, write all   */
/*          remaining records to a unique temporary file, then  */
/*          rename the temporary file to the original filename. */
/*--------------------------------------------------------------*/
{
char    tmpfile[256];
FILE    *tmpfp;
int     retcode;
static char	mod[]="DelReadRecords";

sprintf(tmpfile,"%s.XXXXXX",filename);

mkstemp(tmpfile);
if ((tmpfp=fopen(tmpfile,"w")) == NULL)
        {
#ifdef DEBUG
        gaVarLog(mod, 0,"Unable to open %s. errno=%d", tmpfile,errno);
#endif
        return(-1);
        }

fprintf(tmpfp,"%s",buf);
while(fgets(buf,1024,fp))
        {
        if ((retcode=fprintf(tmpfp,"%s",buf)) < 0)
                {
#ifdef DEBUG
                gaVarLog(mod, 0,"Unable to write to %s. errno=%d",
                				                tmpfile,errno);
#endif
                return(-1);
                }

        if ((retcode=fflush(tmpfp)) != 0)
                {
#ifdef DEBUG
                gaVarLog(mod, 0,"Unable to flush output to %s. errno=%d",
				                                tmpfile,errno);
#endif
                return(-1);
                }
        }

fclose(tmpfp);
fclose(fp);

if ((retcode=rename(tmpfile,filename)) == -1)
        {
#ifdef DEBUG
        gaVarLog(mod, 0,"Unable to rename %s to %s. errno=%d", tmpfile,
							filename,errno);
#endif
        return(-1);
        }

return(0);
}

int WriteToFifo(char *logpath1, char *message)
/*------------------------------------------------------*/
/* Purpose: Write message to fifo, where it will be	*/
/*	    read and sent to the log server.  If the	*/
/*	    write fails for any reason, the will be	*/
/*	    written locally.				*/
/*------------------------------------------------------*/
{
char 	fifoname[1024];
int	wfifo_fd, i, j, bytesout, ret_code;
static char	mod[]="WriteToFifo";

sprintf(fifoname,"%s/Global/%s",(char *)getenv("ISPBASE"),FIFONAME);

if ( access( fifoname, W_OK ) == -1 )
	{
	ret_code=WriteLocal(logpath1, message, FORWARD_LOG);
	return(ret_code);
	}

if ( (wfifo_fd=open(fifoname,O_WRONLY|O_NONBLOCK)) == -1 )
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
			ret_code=WriteLocal(logpath1, message, FORWARD_LOG);
		}
	else
		ret_code=WriteLocal(logpath1, message, FORWARD_LOG);
	}

close(wfifo_fd);

return(ret_code);
}

int lr_readline(char *the_line, int maxlen, FILE *fp)
/*--------------------------------------------------------------------
lr_readline(): Read a line from the file pointer, strip leading spaces,
		make sure it is not a comment (#) and return it.
	    The fp is presumed to be a pointer to an open file.
--------------------------------------------------------------------*/
{
static char	*ptr;
static char	mybuf[1024];
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

/*--------------------------------------------------------------------
int ChkPath(): verify PathName and return proper in outPath
--------------------------------------------------------------------*/
int ChkPath(char *PathName, char *defaultPath, char *outPath)
{
char		tmpPath[256];
int		i;
struct stat	sbuf;
static char	mod[]="ChkPath";
int		rc;

memset((struct stat *)&sbuf, 0, sizeof(sbuf));
memset((char *)tmpPath, 0, sizeof(tmpPath));

rc=stat(PathName, &sbuf);

if ( (sbuf.st_mode & S_IFDIR) == S_IFDIR )	/* PathName is a directory */
{
	if (PathName[strlen(PathName)-1] == '/')
		sprintf(outPath, "%s%s", PathName, DEF_BASENAME);
	else
		sprintf(outPath, "%s/%s", PathName, DEF_BASENAME);
}
else
{
	for (i=strlen(PathName)-1; i>0; i--)
	{
		if (PathName[i] == '/')
			break;
	}

	sprintf(tmpPath,"%.*s",i,PathName);

	memset((struct stat *)&sbuf, 0, sizeof(sbuf));
	if ((rc=stat(tmpPath, &sbuf)) == -1)
	{
		sprintf(outPath, "%s", defaultPath);
		return(0);
	}
		
	if ( (sbuf.st_mode & S_IFDIR) != S_IFDIR ) /* PathName is a directory */
	{
		sprintf(outPath, "%s", defaultPath);
		return(0);
	}

	sprintf(outPath, "%s", PathName);
}
	
return(0);
}
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

/*--------------------------------------------------------------------
getField(): 	This routine extracts the value of the desired field 
		from the data record.

Dependencies: 	None.

Input:
	delim	: that separates fields in the buffer.
	buffer	: containing the line to be parsed.
	fld_num	: Field # to be extracted from the buffer.
	field	: Return buffer containing extracted field.

Output:
	-1	: Error
	>= 0	: Length of field extracted
--------------------------------------------------------------------*/
int getField(char delim, char *buffer, int fld_num, char *field)
{
	register int     i;
	int     fld_len, state, wc;

	fld_len = state = wc = 0;

	field[0] = '\0';
	if(fld_num < 0)
        	return (-1);

	for(i=0;i<(int)strlen(buffer);i++) 
        {
        	if(buffer[i] == delim || buffer[i] == '\n')
                {
                	state = 0;
                	if(buffer[i] == delim && buffer[i-1] == delim)
                        	++wc;
                }
        	else if (state == 0)
                {
                	state = 1;
                	++wc;
                }
        	if (fld_num == wc && state == 1)
		{
                	field[fld_len++] = buffer[i];
		}
        	if (fld_num == wc && state == 0)
		{
                	break;
		}
        } /* for */
	if(fld_len > 0)
        {
        	field[fld_len] = '\0';
        	while(field[0] == ' ')
                {
                	for (i=0; field[i] != '\0'; i++)
                        	field[i] = field[i+1];
                }
        	fld_len = strlen(field);
        	return (fld_len);
        }
	return(-1);
} /* END: GetField() */
