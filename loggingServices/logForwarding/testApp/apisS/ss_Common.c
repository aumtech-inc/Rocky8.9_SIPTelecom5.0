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
-----------------------------------------------------------------------------*/

#include "ss.h"
#include "ssHeaders.h"
#include "WSS_Externs.h"
#include <sys/poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* static char ModuleName[]="ss_Common"; */

/*--------------------------------------------------------------------
Write_Log():
--------------------------------------------------------------------*/
int Write_Log(char *mod, char *msg)
{
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
#endif
	 gaVarLog(mod, 0, msg);

	return(0);
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
int sendSize(char *module, long size)
{
	char	sizeBuf[20];
	int	written, rc;

	sprintf(sizeBuf, "%ld~", size);

//	sprintf(__log_buf, "%s|%d|DJB: Sending size (%s)", __FILE__, __LINE__, sizeBuf);
//	Write_Log(module, __log_buf);
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
int recvSize(char *module, long *size, int timeout)
{
	static char mod[]="recvSize";
	char	sizeBuf[20];
	int	nbytes, rc;
	int	where;

	*size = -1;
	memset(sizeBuf, 0, sizeof(sizeBuf));
	
	where = 0;
	for(;;)
	{
		rc = readSocketData(module, "size", timeout, 1, &sizeBuf[where]);
		if(rc < 0)
		{
			return(rc);		/* message written in sub-routine */
		}

		if ( sizeBuf[where] == '~' )
		{
			sizeBuf[where] = '\0';
			break;
		}

		where += rc;
	}

	*size = atol(sizeBuf);

	gaVarLog(mod, 1,"%s|%d|Read size %d", __FILE__, __LINE__, *size);

	return(0);
} /* END: recvSize() */
/*--------------------------------------------------------------------------
readSocketData():
--------------------------------------------------------------------------*/
int readSocketData(char *module, char *whatData, int timeout, 
						long dataSize, char *dataBuf)
{
	static char mod[]="readSocketData";
	short 	done;
	long 	nbytes, bytesToRead, where;
	int		rc;
	struct pollfd	pollSet[1];

	pollSet[0].fd = GV_tfd;		/* fd of the fifo descriptor already opened */
	pollSet[0].events = POLLIN;
	if(timeout == 0)
		rc = poll(pollSet, 1, -1);
	else
		rc = poll(pollSet, 1, timeout*1000);
	if(rc < 0)
		{
		sprintf(__log_buf, "poll failed errno %d", errno);
		Write_Log(module, __log_buf);
		return(-1);
		}
	if(pollSet[0].revents == 0)
	{
		sprintf(__log_buf, "Timed out while waiting for %s after %d "
				     		"seconds", whatData, timeout);
		Write_Log(module, __log_buf);
        return (ss_TIMEOUT);
	}
	where = 0;
	bytesToRead = 0;
	done = 0;
	while(!done)
	{
//		gaVarLog(mod, 1, "%s|%d|DJB; dataSize=%d where=%d bytesToRead=%d", __FILE__, __LINE__,
//						dataSize, where, bytesToRead);
		bytesToRead = dataSize - where;
//		gaVarLog(mod, 1, "%s|%d|DJB", __FILE__, __LINE__);

//		gaVarLog(mod, 1, "%s|%d|bytesToRead(%d) = dataSize(%d) - where(%d).  "
//			"Attempting to read %d bytes from fd %d.", 
//			__FILE__, __LINE__, bytesToRead, dataSize, where, bytesToRead, GV_tfd);
//
		if((nbytes = read(GV_tfd, &dataBuf[where], bytesToRead)) < 0)
		{
			gaVarLog(mod, 0, "%s|%d|Failed to read %s. fd=%d  [%d,%s]",
					__FILE__, __LINE__, whatData, GV_tfd, errno, strerror(errno));
			Write_Log(module, __log_buf);
			shutdownSocket(module);
			return (ss_DISCONNECT);
		}

		gaVarLog(mod, 1, "%s|%d|%d = read(fd=%d)", __FILE__, __LINE__, nbytes, GV_tfd);
		if (nbytes == 0)
		{
			gaVarLog(mod, 1, "%s|%d|Returning (%d)", __FILE__, __LINE__, where);
			return(where);
		}
		where += nbytes;

		//gaVarLog(mod, 1, "%s|%d|DJB", __FILE__, __LINE__);
		if(where >= dataSize)
		{
			gaVarLog(mod, 1, "%s|%d|where=%d, dataSize=%d, done now.", __FILE__, __LINE__, where, dataSize);
			dataBuf[where] = '\0';
			done = 1; 
			break;
		}
//		gaVarLog(mod, 1, "%s|%d|DJB - end of while", __FILE__, __LINE__);
	} while(!done);

//	gaVarLog(mod, 1, "%s|%d|Returning (%d)", __FILE__, __LINE__, where);

	return(where);
} /* END: readSocketData() */
/*--------------------------------------------------------------------------
writeSocketData():
--------------------------------------------------------------------------*/
int writeSocketData(char *module, char *whatData, long dataSize, char *dataBuf)
{
	int	written;

#ifdef DEBUG
/* 	fprintf(stderr, "Writing data to socket = >%s<\n", dataBuf); */
#endif


	if((written = write(GV_tfd, dataBuf, dataSize)) != dataSize)
	{
		sprintf(__log_buf,"Failed to write %s to socket, errno=%d (%s)",
					whatData, errno, strerror(errno));
		Write_Log(module, __log_buf);
		shutdownSocket(module);
		return(ss_DISCONNECT);
	}
//	snprintf(__log_buf, 255, "%s|%d|DJB: %d=write(%s)", __FILE__, __LINE__, written, dataBuf);
//	Write_Log(module, __log_buf);

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
