/*-----------------------------------------------------------------------------
Program:	ss_RecvFile.c
Purpose:	To receive a file over a socket connection
Author:		Aumtech, Inc.
Update:		03/07/97 sja	Added this header
Update:		03/13/97 sja	Added fileOption to ss_RecvFile
Update:		03/26/97 sja	If disconnected, return ss_DISCONNECT
Update:		03/27/97 sja	Added alarm() to read while recv'ng file.
Update:		03/31/97 sja	Create destination only after response is recv'd
Update:		03/31/97 sja	Added send_to_monitor
Update:		04/02/97 sja	Added WSS_Com_parm to send_to_monitor
Update:		04/02/97 sja	Changed #include WSS_Globals.h to WSS_Externs.h
Update:		04/08/97 sja	if fileCreateOption = RF_PROTECT & file exists
Update:				return WSS_FILE_EXISTS
Update:		04/16/97 sja	If we read 0 bytes, set done = 1 & break.
Update:		04/16/97 sja	If file_size == 0, close fd, send conf & return
Update:		05/15/97 sja	Fixed stat() error message
Update:		06/05/97 sja	Removed WSS_VAR.h
Update:		06/05/97 sja	Used recvSize, sendSize & readSocketData
Update:         05/04/98 gpw    removed .h files not needed for lite version
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
Update:	2000/11/30 sja	Changed sys_errlist[] to strerror()
Update:	2002/05/22 sja	Added ss_NewRecvFile()
-----------------------------------------------------------------------------*/

#include "ssHeaders.h"
#include "WSS_Externs.h"

/* #define DEBUG */

static char ModuleName[] = "ss_RecvFile";

/*--------------------------------------------------------------------------
ss_RecvFile(): This api receive the file specified.
---------------------------------------------------------------------------*/
int ss_RecvFile(char *destination, int fileOption)
{
	char	mod[]="ss_RecvFile";
	long	file_size=0L, recvdBytes = 0L, bytesRead, recvdataBytes;
	char	diag_mesg[256];
	char	data[4096000], response[30], result[30], srcfile[50];
	char	fileSizeBuf[20];
	int	rc, flags, fd;
	struct	stat 	stat_buf;
	short	done;

	sprintf(LAST_API, "%s", ModuleName);

	if(GV_SsConnected == 0)
	{
		Write_Log(ModuleName, "Client disconnected");
		return(ss_DISCONNECT);
	}

	if(destination == NULL)
	{
		sprintf(__log_buf, "Destination filename cannot be NULL");
		Write_Log(ModuleName, __log_buf);
        	return (ss_FAILURE);
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
				Write_Log(ModuleName, __log_buf);
				return(ss_FILE_EXISTS);
			}
			flags = O_WRONLY|O_CREAT;
			break;

		default:
			sprintf(__log_buf, 
				"Invalid file create option %d, file=(%s)",
							fileOption,destination);
			Write_Log(ModuleName, __log_buf);
        		return (ss_FAILURE);
	} /* switch */

	memset(data, 0, sizeof(data));
	memset(srcfile, 0, sizeof(srcfile));
	memset(result, 0, sizeof(result));
	memset(response, 0, sizeof(response));

	rc = ss_RecvData(GV_SysTimeout, -1, data, &recvdataBytes);
	if(rc != ss_SUCCESS)
	{
		sprintf(__log_buf, "No response from client, file=(%s)",
							destination);
		Write_Log(ModuleName, __log_buf);
		return(rc);
	}
	gaVarLog(mod, 1,"%s|%d|ss_RecvData(%s) succeeded.",
                                __FILE__, __LINE__, data);


	getField('|', data, 1, srcfile);
	getField('|', data, 2, result);
	getField('|', data, 3, response);

#ifdef DEBUG
	fprintf(stderr, "srcfile  = >%s<\n", srcfile);
	fprintf(stderr, "result   = >%s<\n", result);
	fprintf(stderr, "response = >%s<\n", response);
#endif

	if(srcfile[0] == '\0' || result[0] == '\0' || response[0] == '\0')
	{
		sprintf(__log_buf, "Invalid response from client (%s)", data);
		Write_Log(ModuleName, __log_buf);
		close(fd);
		return(ss_FAILURE);
	}
	sscanf(response, "%ld", &file_size);
	gaVarLog(mod, 1,"%s|%d|Receiving file (%s), size=%d",
                                __FILE__, __LINE__, srcfile, file_size);

	if (strstr(result, "OK") == NULL)
	{
		sprintf(__log_buf, "Receive file protocol failed, file: %s, "
				   "reason: %s", destination,response); 
		Write_Log(ModuleName, __log_buf);
		return (ss_FAILURE);
	}

	if((fd = open(destination, flags, 0777)) < 0)
	{
		sprintf(__log_buf, 
			"Failed to open file %s for writing, errno=%d (%s)",
					destination, errno, strerror(errno));
		Write_Log(ModuleName, __log_buf);
        	return (ss_FAILURE);
	}

	if(file_size == 0)
	{
		close(fd);
		if(sendSize(ModuleName, 0) < 0)
		{
			return(ss_FAILURE); 	/* msg written in sub-routine */
		}
		return(ss_SUCCESS);
	}

	alarm(GV_SysTimeout);

	bytesRead = 0;
	done = 0;
	while(done == 0)
    {
		memset(data, 0, sizeof(data));
		rc = ss_RecvData(GV_SysTimeout, -1, data, &recvdBytes);
		
		if(recvdBytes < 0)
		{
			gaVarLog(mod, 0,"%s|%d|ss_RecvData failed, rc=%d.  Unable to receive file",
                                __FILE__, __LINE__, rc);
           	close(fd);
			unlink(destination);
            return(rc);
		}
		else if(recvdBytes == 0)
		{
			gaVarLog(mod, 1,"%s|%d|ss_RecvData recieved 0 bytes. Completed.",
                                __FILE__, __LINE__, rc);
           	close(fd);
			done = 1;
			break;
		}
       	else                	/* write to file */
        {
			bytesRead += recvdBytes;
			gaVarLog(mod, 1,"%s|%d|Read %d bytes; total=%d", 
                                __FILE__, __LINE__, recvdBytes, bytesRead);
       
           	if(write(fd, &data[0], recvdBytes) != recvdBytes)
			{
				sprintf(__log_buf, "Failed to write data to file %s, "
					"[%d, %s]", destination, errno,strerror(errno)); 
				Write_Log(ModuleName, __log_buf);
				close(fd);
               	return (ss_FAILURE);
			}
		} /* else */

		if(bytesRead >= file_size)
		{
			gaVarLog(mod, 1,"%s|%d|Read all %d bytes",
                                __FILE__, __LINE__, bytesRead);
			done = 1;
			break;
		}
	} /* while */
	close(fd);
	alarm(0);

	if(stat(destination,  &stat_buf) < 0)
	{
		sprintf(__log_buf, "Failed to stat file %s, errno=%d (%s)",
					destination, errno, strerror(errno));
		Write_Log(ModuleName, __log_buf);
		return(ss_FAILURE);
	}

	sprintf(fileSizeBuf, "%ld", stat_buf.st_size);
	if((rc = ss_SendData(strlen(fileSizeBuf), fileSizeBuf)) != ss_SUCCESS)
	{
		sprintf(__log_buf,
			"Failed to send file (%s) receipt confirmation",
			destination);
		Write_Log(ModuleName, __log_buf);
		return(rc);
	}

	return (ss_SUCCESS);
} /* ss_RecvFile */

/*--------------------------------------------------------------------------
ss_NewRecvFile(): This api receive the file specified.
---------------------------------------------------------------------------*/
int ss_NewRecvFile(char *destination, int fileOption)
{
	static char	yMod[] = "ss_NewRecvFile";
	long	file_size=0L, recvdBytes = 0L, bytesRead, recvdataBytes;
	char	diag_mesg[256];
	char	data[1026], response[30], result[30], srcfile[50];
	char	fileSizeBuf[20];
	int	rc, flags, fd;
	struct	stat 	stat_buf;
	short	done;
	char	*yDataPtr;

	sprintf(LAST_API, "%s", yMod);

	if(GV_SsConnected == 0)
	{
		Write_Log(yMod, "Client disconnected");
		return(ss_DISCONNECT);
	}

	if(destination == NULL)
	{
		sprintf(__log_buf, "Destination filename cannot be NULL");
		Write_Log(yMod, __log_buf);
        	return (ss_FAILURE);
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
				Write_Log(yMod, __log_buf);
				return(ss_FILE_EXISTS);
			}
			flags = O_WRONLY|O_CREAT;
			break;

		default:
			sprintf(__log_buf, 
				"Invalid file create option %d, file=(%s)",
							fileOption,destination);
			Write_Log(yMod, __log_buf);
        		return (ss_FAILURE);
	} /* switch */

	memset(data, 0, sizeof(data));
	memset(srcfile, 0, sizeof(srcfile));
	memset(result, 0, sizeof(result));
	memset(response, 0, sizeof(response));

	rc = ss_RecvData(GV_SysTimeout, -1, data, &recvdataBytes);
	if(rc != ss_SUCCESS)
	{
		sprintf(__log_buf, "No response from client, file=(%s)",
							destination);
		Write_Log(yMod, __log_buf);
		return(rc);
	}

	getField('|', data, 1, srcfile);
	getField('|', data, 2, result);
	getField('|', data, 3, response);

#ifdef DEBUG
	fprintf(stderr, "srcfile  = >%s<\n", srcfile);
	fprintf(stderr, "result   = >%s<\n", result);
	fprintf(stderr, "response = >%s<\n", response);
#endif

	if(srcfile[0] == '\0' || result[0] == '\0' || response[0] == '\0')
    {
		sprintf(__log_buf, "Invalid response from client (%s)", data);
		Write_Log(yMod, __log_buf);
		close(fd);
      	return(ss_FAILURE);
    }
	sscanf(response, "%ld", &file_size);

	if (strstr(result, "OK") == NULL)
	{
		sprintf(__log_buf, "Receive file protocol failed, file: %s, "
				   "reason: %s", destination,response); 
		Write_Log(yMod, __log_buf);
		return (ss_FAILURE);
	}

	if((fd = open(destination, flags, 0777)) < 0)
	{
		sprintf(__log_buf, 
			"Failed to open file %s for writing, errno=%d (%s)",
					destination, errno, strerror(errno));
		Write_Log(yMod, __log_buf);
       	return (ss_FAILURE);
    }
	if(file_size == 0)
	{
		close(fd);
		if(sendSize(yMod, 0) < 0)
		{
			return(ss_FAILURE); 	/* msg written in sub-routine */
		}
		return(ss_SUCCESS);
	}

	/*
	** Allocate the memory
	*/
	if((yDataPtr = (char *)malloc(file_size)) == NULL)
	{
		sprintf(__log_buf,
				"Failed to malloc (%d) bytes, [%d: %s]",
				file_size, errno, strerror(errno));

		Write_Log(yMod, __log_buf);

		close(fd);

		return(ss_FAILURE);
	}
			
	if(readSocketData(yMod, "file contents", GV_SysTimeout,
						file_size, yDataPtr) != file_size)
	{
		close(fd);

		free(yDataPtr);

		return(ss_FAILURE);
	}

  	if(write(fd, yDataPtr, file_size) != file_size)
	{
		sprintf(__log_buf, "Failed to write data to file %s, "
				"errno=%d, (%s)", destination, errno,strerror(errno)); 
		Write_Log(yMod, __log_buf);

		close(fd);

		free(yDataPtr);

      	return (ss_FAILURE);
	}

	close(fd);

	free(yDataPtr);

	if(stat(destination,  &stat_buf) < 0)
	{
		sprintf(__log_buf, "Failed to stat file %s, errno=%d (%s)",
					destination, errno, strerror(errno));
		Write_Log(yMod, __log_buf);
		return(ss_FAILURE);
	}

	sprintf(fileSizeBuf, "%ld", stat_buf.st_size);
	if((rc = ss_SendData(strlen(fileSizeBuf), fileSizeBuf)) != ss_SUCCESS)
	{
		sprintf(__log_buf,
			"Failed to send file (%s) receipt confirmation",
			destination);
		Write_Log(yMod, __log_buf);
		return(rc);
	}

	return (ss_SUCCESS);

} /* ss_NewRecvFile */

