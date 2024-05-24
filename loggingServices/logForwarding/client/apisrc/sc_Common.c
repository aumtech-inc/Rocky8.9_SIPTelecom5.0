/*
Update:		03/07/97 sja	Added this header
Update:		03/18/97 sja	Changed Write_Log to write a msg in 1 line
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Added mod parm. to getQueData
Update:		06/05/97 sja	Added sendSize & recvSize
Update:		11/05/97 mpb	Added gaVarLog
Update:	06/11/98 mpb	changed GV_Connected to GV_ScConnected. 
Update:	04/26/99 apj	Added isSocketStillConnected.
Update:	10/29/99 mpb	Added hostStatus check if readSocketData got timeout.
Update:	10/21/00 mpb	Changed alarm code to poll.
Update:	11/30/00 sja	Changed sys_errlist to strerror()
Update: 12/04/00 mpb	if timeout == 0 poll is set to -1.
Update:	03/29/01 apj	Initialise size before call to isSocketStillConnected.
Update:	07/29/02 ddn	Added newRecvSize which calls readSocketData with whatData=NULL.
Update:	07/29/02 ddn	In readSocketData, Timeout message is not logged if whatData=NULL
Update:	09/14/12 djb	Updated logging.

*/

#include "WSC_var.h"
#include "sc.h"
#include <sys/poll.h>
#include <string.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <sys/socket.h> 

// #define DEBUG
#define GAVARLOG 

void	clean_up();
extern char GV_hostName[];

/*--------------------------------------------------------------------------
Write_Log(): Error routine. Write error to the WSCyymmdd file.
--------------------------------------------------------------------------*/
int Write_Log(char *mod, int debugMode, char *ptr)
{
	long    theTime;
	FILE    *fp_log;
	char	today[10], file_name[20], tmp_time[80];


	gaVarLog(mod, debugMode, ptr);

#if 0
	time(&theTime);
	cftime(today, "%y%m%d", &theTime);
	cftime(tmp_time, "%m/%d/%y %H:%M:%S", &theTime);

	sprintf(file_name, "sc%s", today);
	if((fp_log = fopen(file_name, "a+")) == NULL)
	{
        	printf("Error in opening %s, errno=%d (%s).", 
					file_name, errno, strerror(errno));
		printf("%s", ptr);
		return(-1);
	}

	fprintf(fp_log,"%s|%5d|%15.15s| %s",tmp_time,getpid(),mod,ptr);
	fflush(fp_log);
	fclose(fp_log);
#endif

	return (0);
} /* END: Write_Log() */

/*--------------------------------------------------------------------------
sendSize():
--------------------------------------------------------------------------*/
int sendSize(char *module, long size)
{
	char	sizeBuf[20];
	int	written, rc;

	sprintf(sizeBuf, "%ld~", size);


	rc = writeSocketData(module, "size", strlen(sizeBuf), sizeBuf);
	if(rc < 0)
	{
		return(rc);		/* message written in sub-routine */
	}
	sprintf(__log_buf, "%s|%d|Wrote size (%s)", __FILE__, __LINE__, sizeBuf);
	Write_Log(module, 1, __log_buf);

	return(0);
} /* END: sendSize() */
/*--------------------------------------------------------------------------
recvSize():
--------------------------------------------------------------------------*/
int recvSize(char *module, long *size, int timeout)
{
	char	sizeBuf[20];
	int	nbytes, rc;

	*size = -1;
	memset(sizeBuf, 0, sizeof(sizeBuf));
	
	rc = readSocketData(module,"size", timeout, 5, sizeBuf);
	if(rc < 0)
	{
		return(rc);		/* message written in sub-routine */
	}

	sprintf(__log_buf, "%s|%d|Received size (%s).", __FILE__, __LINE__, sizeBuf);
	Write_Log(module, 1, __log_buf);

	*size = atol(sizeBuf);

	sprintf(__log_buf, "%s: Recv'd size (%ld) bytes.", module, *size);
	Write_Log(module, 1, __log_buf);

	return(0);
} /* END: recvSize() */

int newRecvSize(char *module, long *size, int timeout)
{
	char	sizeBuf[20];
	int	nbytes, rc;

	*size = -1;
	memset(sizeBuf, 0, sizeof(sizeBuf));
	
	rc = readSocketData(module, NULL, timeout, sizeof(long), sizeBuf);
	if(rc < 0)
	{
		return(rc);		/* message written in sub-routine */
	}

	*size = atol(sizeBuf);

	sprintf(__log_buf, "%s: Recv'd size (%ld) bytes.", module, *size);
	Write_Log(module, 1, __log_buf);

	return(0);
} /* END: newRecvSize() */
/*--------------------------------------------------------------------------
readSocketData():
--------------------------------------------------------------------------*/
int readSocketData(char *module, char *whatData, int timeout, 
						long dataSize, char *dataBuf)
{
short 	done;
int 	nbytes, bytesToRead, where;
int		rc;
struct pollfd	pollSet[1];

pollSet[0].fd = sockfd;		/* fd of the fifo descriptor already opened */
pollSet[0].events = POLLIN;
if(timeout == 0)
	rc = poll(pollSet, 1, -1);
else
	rc = poll(pollSet, 1, timeout*1000);
if(rc < 0)
	{
	sprintf(__log_buf, "poll() failed. [%d, %s]", errno, strerror(errno));
	Write_Log(module, 0, __log_buf);
	return(-1);
	}
if(pollSet[0].revents == 0)
	{

	if(whatData != NULL &&  *whatData != '\0')
	{
		sprintf(__log_buf,"Timed out waiting for %s.", whatData);
		Write_Log(module, 0, __log_buf);
	}

	if(hostStatus(GV_hostName, 2) < 0)
		{
		sprintf(__log_buf, "Host %s is unreachable",GV_hostName);
		Write_Log(module, 0, __log_buf);
		shutdown(sockfd, 2);
		close(sockfd);
		GV_ScConnected = 0;
		return(sc_DISCONNECT);
		}
	else
    	return (sc_TIMEOUT);
	}

where = 0;
bytesToRead = 0;
done = 0;
while(!done)
	{
        if ( strcmp(whatData, "size") == 0 )
        {
            if((nbytes = read(sockfd, &dataBuf[where], 1)) < 0)
            {
                sprintf(__log_buf, "Failed to read %s, [%d, %s]",
                            whatData, errno, strerror(errno));
                Write_Log(module, 0, __log_buf);
                shutdown(sockfd, 2);
				close(sockfd);
				GV_ScConnected = 0;
                return (sc_DISCONNECT);
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
			if ( (isspace(dataBuf[where])) || ( ! isprint(dataBuf[where])) )
			{
				sprintf(__log_buf, "%s|%d|Read an unexpected character(0x%x). Ignoring.",
											__FILE__, __LINE__, dataBuf[where]);
				Write_Log(module, 1, __log_buf);
                dataBuf[where] = '\0';
			}
			else
			{
            	where += nbytes;
			}
            continue;
        }

	bytesToRead = dataSize - where;

	if((nbytes = read(sockfd, &dataBuf[where], bytesToRead)) < 0)
	{
		sprintf(__log_buf, "Failed to read %s. [%d, %s]", 
				whatData, errno, strerror(errno));
		Write_Log(module, 0, __log_buf);

		shutdown(sockfd, 2);
		close(sockfd);
		GV_ScConnected = 0;
		return (sc_DISCONNECT);
	}
	if(nbytes == 0)
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

	sprintf(__log_buf, "%s|%d|Data read from socket = (%s), nbytes=%d", __FILE__, __LINE__, 
				dataBuf, nbytes);
	Write_Log(module, 1, __log_buf);

return(where);
} /* END: readSocketData() */

/*--------------------------------------------------------------------------
writeSocketData():
--------------------------------------------------------------------------*/
int writeSocketData(char *module, char *whatData, long dataSize, char *dataBuf)
{
int	written;


if((written = write(sockfd, dataBuf, dataSize)) != dataSize)
	{
    sprintf(__log_buf, "%s|%d|Failed to write %s, [%d,%s]",
					__FILE__, __LINE__, whatData, errno, strerror(errno));
	Write_Log(module, 0, __log_buf);
	shutdown(sockfd, 2);
	close(sockfd);
	Write_Log(module, 0, "Server disconnected");
	GV_ScConnected = 0;
	return(sc_DISCONNECT);
	}

   	sprintf(__log_buf, "%s|%d|wrote (%.*s)", __FILE__, __LINE__, 25, dataBuf);
	Write_Log(module, 1, __log_buf);

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

int isSocketStillConnected(char *parmModuleName, int *parmErrNo)
{
	int ret;
	struct	sockaddr_in remoteEndAddr;	/* the server's full address */
	int size;

	size = sizeof(struct  sockaddr_in);	
	*parmErrNo = 0;
	ret = getpeername(sockfd, (struct sockaddr *)&remoteEndAddr,
							&size);
	if(ret != 0) 
	{
		*parmErrNo = errno;
		return(ret);
	}

	return(0);
} /* END: isSocketStillConnected() */
