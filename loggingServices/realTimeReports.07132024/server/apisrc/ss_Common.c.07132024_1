/*-----------------------------------------------------------------------------
Program:	ss_Common.c
Purpose:	Common routines
Author:		Aumtech, Inc.
Update:		03/07/97 sja	Added this header 
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Added mod parm. to getQueData
Update:		04/02/97 sja	Changed #include WSS_Globals.h to WSS_Externs.h
Update:		04/15/97 sja	Ported to the "Lite" version
Update:		05/21/97 sja	Put app. name in log messages
Update:		05/30/97 sja	Added client pid & machine name to Write_Log
Update:         05/04/98 gpw 	removed .h files not needed for lite version
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
Update:	2000/11/30 sja	Changed sys_errlist[] to strerror()
Update:	2000/12/04 mpb	if timeout == 0, set poll timeout to -1.
Update:	2006/11/29 djb	Removed log message which was core dumping.
Update: 09/14/12 djb  Updated/cleaned up logging.
-----------------------------------------------------------------------------*/

#include "ss.h"
#include "ssHeaders.h"
#include "WSS_Externs.h"
#include <sys/poll.h>

/* static char ModuleName[]="ss_Common"; */

/*--------------------------------------------------------------------
Write_Log():
--------------------------------------------------------------------*/
int Write_Log(char *mod, int debugMode, char *msg)
{
	gaVarLog(mod, debugMode, msg);
	return(0);

#if 0
	FILE	*fp;
	time_t	currentTime;
	char	timebuf[80];
	char	outfile[80];
	struct tm *pTime;

	time(&currentTime);
	pTime = localtime(&currentTime);
	strftime(timebuf, sizeof(timebuf), "%y%m%d", pTime);

	sprintf(outfile, "ss%s", timebuf);

	strftime(timebuf, sizeof(timebuf), "%y/%m/%d %H:%M:%S", pTime);

	if((fp = fopen(outfile, "a+")) == NULL)
	{
		fprintf(stderr, "%s|", timebuf);
		fprintf(stderr, "%s|", GV_CDR_AppName);
		fprintf(stderr, "%s|", mod);
		fprintf(stderr, "%d|", getpid());
		fprintf(stderr, "%s|", GV_ClientHost);
		fprintf(stderr, "%d|", GV_ClientPid);
		fprintf(stderr, " %s\n", msg);
		fflush(stderr);

		return(0);
	}

	fprintf(fp, "%s|", timebuf);
	fprintf(fp, "%s|", GV_CDR_AppName);
	fprintf(fp, "%s|", mod);
	fprintf(fp, "%d|", getpid());
	fprintf(fp, "%s|", GV_ClientHost);
	fprintf(fp, "%d|", GV_ClientPid);
	fprintf(fp, " %s\n", msg);
	fflush(fp);

	fclose(fp);

	return(0);
#endif
} /* END: Write_Log() */
/*--------------------------------------------------------------------------
shutdownSocket():
--------------------------------------------------------------------------*/
int shutdownSocket(char *module)
{
	if(GV_ShutdownDone == 1)
		return(0);

	shutdown (GV_tfd, 2);
	close(GV_tfd);

	GV_ShutdownDone = 1;
	GV_SsConnected = 0;		/* client disconnected */

	return(0);
} /* END: shutdownSocket() */

/*--------------------------------------------------------------------------
sendSize():
--------------------------------------------------------------------------*/
int sendSize(char *module, guint32 size)
{
	char	sizeBuf[20];
	int	written, rc;

	sprintf(sizeBuf, "%ld~", size);

#ifdef DEBUG
	fprintf(stderr, "%s: Sending size >%s<\n", module, sizeBuf);
#endif

	rc = writeSocketData(module, "size", strlen(sizeBuf), sizeBuf);
	if(rc < 0)
	{
		return(rc);		/* message written in sub-routine */
	}

	return(0);
} /* END: sendSize() */
/*--------------------------------------------------------------------------
recvSize():
--------------------------------------------------------------------------*/
int recvSize(char *module, size_t *size, int timeout)
{
	char	sizeBuf[20];
	int	nbytes, rc;

	*size = -1;
	memset(sizeBuf, 0, sizeof(sizeBuf));
	
	rc = readSocketData(module,"size", timeout, sizeof(guint32), sizeBuf);
	if(rc < 0)
	{
		return(rc);		/* message written in sub-routine */
	}

	*size = atol(sizeBuf);

	return(0);
} /* END: recvSize() */

/*--------------------------------------------------------------------------
readSocketData():
--------------------------------------------------------------------------*/
int readSocketData(char *module, char *whatData, int timeout, 
						size_t dataSize, char *dataBuf)
{
	short 	done;
	guint32 	nbytes, bytesToRead, where;
	int		rc;
	struct pollfd	pollSet[1];

	pollSet[0].fd = GV_tfd;		/* fd of the fifo descriptor already opened */
	pollSet[0].events = POLLIN;
	if(timeout == 0)
	{
		sprintf(__log_buf, "%s|%d|Calling poll(, 1, -1)", 
				__FILE__, __LINE__, rc);
		Write_Log(module, 2, __log_buf);
		rc = poll(pollSet, 1, -1);
		sprintf(__log_buf, "%s|%d|poll() %d = poll(, 1, -1)", 
				__FILE__, __LINE__, rc);
		Write_Log(module, 2, __log_buf);
	}
	else
	{
		sprintf(__log_buf, "%s|%d|Calling poll(, 1, %d)", 
				__FILE__, __LINE__, rc, timeout*1000);
		Write_Log(module, 2, __log_buf);
		rc = poll(pollSet, 1, timeout*1000);
		sprintf(__log_buf, "%s|%d|poll() %d = poll(, 1, %d)", 
				__FILE__, __LINE__, rc, timeout*1000);
		Write_Log(module, 2, __log_buf);
	}

	if(rc < 0)
		{
		sprintf(__log_buf, "%s|%d|poll() system call failed. [%d, %s]",
				__FILE__, __LINE__, errno, strerror(errno));
		Write_Log(module, 0, __log_buf);
		return(-1);
		}

	if(pollSet[0].revents == 0)
	{
		sprintf(__log_buf, "%s|%d|Timed out while waiting for %s after %d seconds",
				__FILE__, __LINE__, whatData, timeout);
		Write_Log(module, 0, __log_buf);
        return (ss_TIMEOUT);
	}
	where = 0;
	bytesToRead = 0;
	done = 0;
	while(!done)
	{
		if ( strcmp(whatData, "size") == 0 )
		{
		sprintf(__log_buf, "%s|%d|Attempting to read size.", 
				__FILE__, __LINE__);
		Write_Log(module, 2, __log_buf);
			if((nbytes = read(GV_tfd, &dataBuf[where], 1)) < 0)
			{
				sprintf(__log_buf, "Failed to read %s. [%d, %s]",
							whatData, errno, strerror(errno));
				Write_Log(module, 0, __log_buf);
				shutdownSocket(module);
				return (ss_DISCONNECT);
			}
			if (nbytes == 0)
			{

				return(where);
			}
			if ( dataBuf[where] == '~' )
			{
				dataBuf[where] = '\0';
				done = 1; 
				break;
			}
			where += nbytes;
			continue;
		}
		bytesToRead = dataSize - where;

		if((nbytes = read(GV_tfd, &dataBuf[where], bytesToRead)) < 0)
		{
			sprintf(__log_buf, 0, "%s|%d|Failed to read %s. [%d, %s]",
					__FILE__, __LINE__, whatData, errno, strerror(errno));
			Write_Log(module, 0, __log_buf);
			shutdownSocket(module);
			return (ss_DISCONNECT);
		}
		sprintf(__log_buf, "%s|%d|%d = read(%d, %.*s, %d); where=%d.",
			__FILE__, __LINE__, nbytes, GV_tfd, 15, &dataBuf[where], bytesToRead, where);
		Write_Log(module, 2, __log_buf);

		if (nbytes == 0)
		{
			return(where);
		}
		where += nbytes;

		if(where >= dataSize)
		{
			done = 1; 
			break;
		}
	} while(!done);

	return(where);
} /* END: readSocketData() */
/*--------------------------------------------------------------------------
writeSocketData():
--------------------------------------------------------------------------*/
int writeSocketData(char *module, char *whatData, guint32 dataSize, char *dataBuf)
{
	int	written;

	if((written = write(GV_tfd, dataBuf, dataSize)) != dataSize)
	{
		sprintf(__log_buf,"Failed to write %s to socket, errno=%d (%s)",
					whatData, errno, strerror(errno));
		Write_Log(module, 0, __log_buf);
		shutdownSocket(module);
		return(ss_DISCONNECT);
	}
	sprintf(__log_buf,"%s|%d|%d=write(%d, %s, %d)",
		__FILE__, __LINE__, written, GV_tfd, dataBuf, dataSize);
	Write_Log(module, 2, __log_buf);

	return(written);
} /* END: writeSocketData() */

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
} /* END: getField() */
