/*


Update:		03/07/97 sja	Added this header
Update:		03/13/97 sja	Added fileOption to the API
Update:		03/27/97 sja	If disconnected, return WSC_DISCONNECT
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Create destination only after response is recv'd
Update:		03/31/97 sja	Added static char ModuleName[] = ...
Update:		04/08/97 sja	if fileOption = RF_PROTECT & file exists,
Update:				return WSC_FILE_EXISTS
Update:		04/16/97 sja	If file_size == 0, close fd, send conf & return
Update:		05/15/97 sja	Fixed stat() error message
Update:		05/21/97 sja	Put filename in write failed error message
Update:	06/11/98 mpb	changed GV_Connected to GV_ScConnected. 
Update:	11/30/00 sja	Changed sys_errlist to strerror()
Update:	02/20/02 mpb	if file_size == 0, do the same cleanup, if it wasn't 0.
Update:	2002/05/22 sja	Added sc_NewRecvFile()
Update:	2002/07/26 ddn	Chenged sc_NewRecvFile to receive the data in chunks
Update:	09/14/12 djb	Updated logging.

*/

#include <time.h>
#include "WSC_var.h"
#include "sc.h"

/* #define DEBUG */

static char ModuleName[] = "sc_RecvFile";

int secondsPast(time_t zStartTime);

/*--------------------------------------------------------------------------
sc_RecvFile(): This api receive the file specified.
---------------------------------------------------------------------------*/
int sc_RecvFile(char *destination, int fileOption)
{
	int	fd, flags, rc;
	long	file_size=0L, bytesRead = 0L;
	char	data[4096000 + 1024], response[30], result[30], srcfile[50];
	char	fileSizeBuf[20];
	long	recvdBytes, recvdataBytes;
	struct	stat 	stat_buf;
	short	done;

	if (GV_ScConnected == 0)
	{
		Write_Log(ModuleName, 2, "Server disconnected."); 
		return (sc_DISCONNECT);
	}
	switch(fileOption)
        {
        	case RF_APPEND:
                	flags = O_WRONLY|O_APPEND|O_CREAT;
                	break;

        	case RF_OVERWRITE:
                	flags = O_WRONLY|O_TRUNC|O_CREAT;
                	break;

        	case RF_PROTECT:
                	if(access(destination, F_OK) == 0)
                        {
                        	sprintf(__log_buf, "WARN: File (%s) exists, won't overwrite with RF_PROTECT.",
                                   	destination);
                        	Write_Log(ModuleName, 2, __log_buf);
                        	return(sc_FILE_EXISTS);
                        }
                	flags = O_WRONLY|O_CREAT;
                	break;

        	default:
                	sprintf(__log_buf, "Invalid file create option %d. Use "
                            "RF_APPEND, RF_OVERWRITE or RF_PROTECT. file=(%s).",
							fileOption, destination);
                	Write_Log(ModuleName, 2, __log_buf);
                	return (-1);
        } /* switch */

	memset(data, 0, sizeof(data));
	memset(srcfile, 0, sizeof(srcfile));
	memset(result, 0, sizeof(result));
	memset(response, 0, sizeof(response));

	if (sc_RecvData(GV_SysTimeout, -1, data, &recvdataBytes) < 0)
        {
        	sprintf(__log_buf, "Unable to get response from server, file=(%s).",destination);
            Write_Log(ModuleName, 0, __log_buf);
        	return (sc_FAILURE);
        }

	getField('|', data, 1, srcfile);
	getField('|', data, 2, result);
	getField('|', data, 3, response);

	if(srcfile[0] == '\0' || result[0] == '\0' || response[0] == '\0')
    {
       	sprintf(__log_buf, "Invalid response (%s) from server. file=(%s).", data, destination);
		Write_Log(ModuleName, 0, __log_buf);
        return (sc_FAILURE);
    }

	sscanf(response, "%ld", &file_size);

	if (strstr(result, "OK") == NULL)
	{
        	sprintf(__log_buf, "Failed to receive file (%s), %s.", 
							destination,response);
        	Write_Log(ModuleName, 2, __log_buf);
		return (sc_FAILURE);
	}
	if ((fd = open(destination, flags, 0777)) < 0)
	{
		sprintf(__log_buf, 
			"Failed to open file (%s) for writing, [%d, %s]",
			destination, errno, strerror(errno));
        	Write_Log(ModuleName, 0, __log_buf);
        	return (sc_FAILURE);
        }
	if(file_size == 0)
	{
#if 0
		close(fd);
		if(sendSize(ModuleName, 0) < 0)
		{
			return(sc_FAILURE); 	/* msg written in sub-routine */
		}
		return(sc_SUCCESS);
#endif
	}

else
{
	done = 0;
	while(!done)
	{
		memset(data, 0, sizeof(data));
		rc = sc_RecvData(GV_SysTimeout, -1, data, &recvdBytes);

		if(recvdBytes < 0)
		{
			sprintf(__log_buf,"Failed to receive file (%s).",
							destination);
			Write_Log(ModuleName, 0,__log_buf);
                       	close(fd);
			unlink(destination);
                        return(recvdBytes);
		}
		else if(recvdBytes == 0)
		{
                       	close(fd);
			done = 1;
			break;
		}
        	else                	/* write to file */
                {
			bytesRead += recvdBytes;
       
                       	if(write(fd, &data[0], recvdBytes) != recvdBytes)
			{
				sprintf(__log_buf, "Failed to write to file (%s). [%d:%s]",
                                        destination,errno,strerror(errno));
               	Write_Log(ModuleName, 0, __log_buf);
				close(fd);
                    	return (sc_FAILURE);
			}
		} /* else */

		/* check if file is completely transferred  or not */
		if (bytesRead >= file_size)
			break;
	} /* while */
}

	close(fd);

	if(stat(destination,  &stat_buf) < 0)
	{
		sprintf(__log_buf, "stat(%s) failed. [%d, %s]",
					destination, errno,strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
		return (sc_FAILURE);
	}	

	sprintf(__log_buf, "Size of destination file (%s) on disk = (%ld).",
                                        destination, stat_buf.st_size);
	Write_Log(ModuleName, 2, __log_buf);

	sprintf(fileSizeBuf, "%ld", stat_buf.st_size);
        if((rc = sc_SendData(strlen(fileSizeBuf), fileSizeBuf)) != sc_SUCCESS)
        {
		sprintf(__log_buf, 
			"Failed to send file (%s) receipt confirmation.", 
			destination);
		Write_Log(ModuleName, 0, __log_buf);
		return(sc_FAILURE);
	}

	return (sc_SUCCESS);
} /* sc_RecvFile */


/*--------------------------------------------------------------------------
sc_NewRecvFile(): This api receive the file specified.
---------------------------------------------------------------------------*/
int sc_NewRecvFile(char *destination, int fileOption, 
						time_t zStartTime, int zTimeOut, char appPid[8])
{
	static char	yMod[] = "sc_NewRecvFile";
	long	file_size=0L, recvdBytes = 0L, bytesRead; 
	long	recvdataBytes, remaining_bytes, bytesToBeRead;
	char	diag_mesg[256];
	char	data[1026], response[30], result[30], srcfile[50];
	char	fileSizeBuf[20];
	int	rc, flags, fd, readCounter = 0;
	struct	stat 	stat_buf;
	short	done;
	char	*yDataPtr;
	long 	oneMeg = 1000000l;
	int		secondsBeforeTimeOut;
	char	procPath[255];

	memset(procPath, 0, sizeof(procPath));
	sprintf(procPath, "/proc/%s", appPid);

	if(GV_ScConnected == 0)
	{
		Write_Log(yMod, 2, "Client disconnected");
		return(sc_DISCONNECT);
	}

	if(destination == NULL)
	{
		sprintf(__log_buf, "Destination filename cannot be NULL");
		Write_Log(yMod, 0, __log_buf);
       	return (sc_FAILURE);
	}

	switch(fileOption)
	{
		case RF_APPEND:
			flags = O_WRONLY|O_APPEND|O_CREAT;
			break;
	
		case RF_OVERWRITE:
			flags = O_WRONLY|O_TRUNC|O_CREAT;
			break;
	
		case RF_PROTECT:
			if(access(destination, F_OK) == 0)
			{
				sprintf(__log_buf, 
					"WARN: Destination file (%s) exists, "
					"won't overwrite", destination);
				Write_Log(yMod, 2, __log_buf);
				return(sc_FILE_EXISTS);
			}
			flags = O_WRONLY|O_CREAT;
			break;

		default:
			sprintf(__log_buf, 
				"Invalid file create option %d, file=(%s)",
							fileOption,destination);
			Write_Log(yMod, 2, __log_buf);
        		return (sc_FAILURE);
	} /* switch */

	memset(data, 0, sizeof(data));
	memset(srcfile, 0, sizeof(srcfile));
	memset(result, 0, sizeof(result));
	memset(response, 0, sizeof(response));

	rc = sc_RecvData(GV_SysTimeout, -1, data, &recvdataBytes);
	if(rc != sc_SUCCESS)
	{
		sprintf(__log_buf, "No response from client, file=(%s)",
							destination);
		Write_Log(yMod, 0, __log_buf);
		return(rc);
	}

	getField('|', data, 1, srcfile);
	getField('|', data, 2, result);
	getField('|', data, 3, response);

#ifdef DEBUG
	fprintf(stderr, "srcfile  = >%s<.", srcfile);
	fprintf(stderr, "result   = >%s<.", result);
	fprintf(stderr, "response = >%s<.", response);
#endif

	if(srcfile[0] == '\0' || result[0] == '\0' || response[0] == '\0')
    {
		sprintf(__log_buf, "Invalid response from client (%s)", data);
		Write_Log(yMod, 0, __log_buf);
		close(fd);
      	return(sc_FAILURE);
    }
	sscanf(response, "%ld", &file_size);

	if (strstr(result, "OK") == NULL)
	{
		sprintf(__log_buf, "Receive file protocol failed, file: %s, "
				   "reason: %s", destination,response); 
		Write_Log(yMod, 0, __log_buf);
		return (sc_FAILURE);
	}

	if((fd = open(destination, flags, 0777)) < 0)
	{
		sprintf(__log_buf, 
			"Failed to open file %s for writing. [%d, %s]",
					destination, errno, strerror(errno));
		Write_Log(yMod, 0, __log_buf);
       	return (sc_FAILURE);
    }
	if(file_size == 0)
	{
		close(fd);
		if(sendSize(yMod, 0) < 0)
		{
			return(sc_FAILURE); 	/* msg written in sub-routine */
		}
		return(sc_SUCCESS);
	}

	remaining_bytes = file_size;
	bytesToBeRead = oneMeg;

	if(remaining_bytes < oneMeg)
		bytesToBeRead = remaining_bytes;

	/*
	** Allocate the memory
	*/
	if((yDataPtr = (char *)malloc(bytesToBeRead+1)) == NULL)
	{
		sprintf(__log_buf,
				"Failed to malloc (oneMeg) bytes, [%d: %s]",
				file_size, errno, strerror(errno));
		Write_Log(yMod, 0, __log_buf);
		close(fd);
		return(sc_FAILURE);
	}
	while(1)
	{
		if(access(procPath, F_OK) == -1)
		{
			sprintf(__log_buf, "App <%s> died before downloading the file", appPid);
			Write_Log(yMod, 0, __log_buf);
			close(fd);
			free(yDataPtr);
			return(sc_TIMEOUT);
		}
		secondsBeforeTimeOut =  zTimeOut - secondsPast(zStartTime);

		if(secondsBeforeTimeOut <= 0)
		{
			sprintf(__log_buf, "Failed to receive all data before timeout.");
			Write_Log(yMod, 0, __log_buf);
			close(fd);
			free(yDataPtr);
			return(sc_TIMEOUT);
		}
	
		if(readSocketData(yMod, "file contents", secondsBeforeTimeOut,
							bytesToBeRead, yDataPtr) != bytesToBeRead)
		{
			close(fd);
			free(yDataPtr);
			return(sc_FAILURE);
		}
	
  		if(write(fd, yDataPtr, bytesToBeRead) != bytesToBeRead)
			{
			sprintf(__log_buf, "Failed to write data to file %s, [%d, %s]",
						destination, errno,strerror(errno)); 
			Write_Log(yMod, 0, __log_buf);
			close(fd);
			free(yDataPtr);
      		return (sc_FAILURE);
		}
		memset(yDataPtr, 0, bytesToBeRead+1);

		if(bytesToBeRead < oneMeg) break;

		remaining_bytes -= oneMeg;

		if(remaining_bytes < oneMeg)
			bytesToBeRead = remaining_bytes;
	}	
	free(yDataPtr);
	close(fd);

	if(stat(destination,  &stat_buf) < 0)
	{
		sprintf(__log_buf, "Failed to stat file %s. [%d, %s]",
					destination, errno, strerror(errno));
		Write_Log(yMod, 0, __log_buf);
		return(sc_FAILURE);
	}

	sprintf(fileSizeBuf, "%ld", stat_buf.st_size);
	if((rc = sc_SendData(strlen(fileSizeBuf), fileSizeBuf)) != sc_SUCCESS)
	{
		sprintf(__log_buf,
			"Failed to send file (%s) receipt confirmation",
			destination);
		Write_Log(yMod, 0, __log_buf);
		return(rc);
	}
	return (sc_SUCCESS);
} /* sc_NewRecvFile */

int secondsPast(time_t zStartTime)
{
	time_t yCurrentTime;

	time(&yCurrentTime);

	return (yCurrentTime - zStartTime);
	
}

