/*-----------------------------------------------------------------------------
Program:	ss_SendFile.c
Purpose:	To send a file over a socket connection.
Author:		Aumtech, Inc. 
Update:		03/07/97 sja	Added this header 
Update:		03/26/97 sja	If disconnected, return ss_DISCONNECT
Update:		03/31/97 sja	Added send_to_monitor
Update:		04/02/97 sja	Changed #include WSS_Globals.h to WSS_Externs.h
Update:		06/05/97 sja	Removed WSS_VAR.h
Update:		06/05/97 sja	Used sendSize, recvSize, writeSocketData
Update:         05/04/98 gpw    removed .h files not needed for lite version
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
Update:	2000/11/30 sja	Changed sys_errlist[] to strerror()
Update:	2002/05/22 sja	Added ss_NewSendFile()
Update: 09/14/12 djb  Updated/cleaned up logging.
-----------------------------------------------------------------------------*/

#include "ssHeaders.h"
#include "WSS_Externs.h"

static char ModuleName[]="ss_SendFile";

/*--------------------------------------------------------------------
ss_SendFile():
--------------------------------------------------------------------*/
int ss_SendFile(char *srcfile)
{
	int	rc, written, done;
	struct	stat 	stat_buf;
	//guint32	fileSize;
	off_t	fileSize;
	FILE	*fd;
	size_t	nbytes;
	long	recvdBytes;
	char	data[4096000], result[80];
	char	diag_mesg[256];
	char	sizeStr[20];

	sprintf(LAST_API, "%s", ModuleName);

	if(GV_SsConnected == 0)
	{
		sprintf(__log_buf, "%s|%d|Client disconnected", __FILE__, __LINE__);
		Write_Log(ModuleName, 2, __log_buf);
		return(ss_DISCONNECT);
	}

	/* send "OK" String and file size */
	if (access (srcfile, F_OK | R_OK) == 0)
	{
		if(stat(srcfile,  &stat_buf) < 0)
		{
			sprintf(__log_buf, 
				"Failed to stat() file %s. [%d, %s]", srcfile, errno, strerror(errno));
			Write_Log(ModuleName, 0, __log_buf);
			return(ss_FAILURE);
		}
		fileSize = stat_buf.st_size;
	}	
	else
	{
		sprintf(__log_buf, "Failed to access file %s. [%d, %s]",
					srcfile, errno, strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
		return (ss_FAILURE);
	}
	sprintf(result, "%s|%s|%llu", srcfile, "OK", fileSize);
	Write_Log(ModuleName, 2, __log_buf);

	if (ss_SendData(strlen(result), result) < 0)
	{
		sprintf(__log_buf, "Failed to send file info to client (%s).", "SS_SendData failed", srcfile);
		Write_Log(ModuleName, 0, __log_buf);
		return(ss_FAILURE);
	}

	if ((fd = fopen(srcfile, "rb")) == (FILE *) NULL)
	{
		sprintf(__log_buf, "Failed to open file %s for reading. [%d, %s]",
					srcfile, errno, strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
       	return (ss_FAILURE);
    }
	done = 0;
	while(!done)
        {
		memset(data, 0, sizeof(data));
        	nbytes = fread(data, 2, sizeof(data), fd);

        	switch(nbytes)
                {
                	case 0:
                        	fclose(fd);
				done = 1;
				break;

                	case -1:
                        	fclose(fd);
				done = 1;
				sprintf(__log_buf, "Failed to read %s. [%d. %s]",
					srcfile, errno,strerror(errno));
				Write_Log(ModuleName, 0, __log_buf);
              	return (ss_FAILURE);

			default:
				rc = ss_SendData(nbytes, data);
				if(rc != ss_SUCCESS)
				{
					fclose(fd);
					sprintf(__log_buf, "Failed to send file (%s)", srcfile);
					Write_Log(ModuleName, 0, __log_buf);
					return(rc);
				}
			break;
		} /* switch */
	} /* while */

	memset(data, 0, sizeof(data));
	rc = ss_RecvData(GV_SysTimeout, -1, data, &recvdBytes);
	if(rc != ss_SUCCESS)
	{
		sprintf(__log_buf, "Failed to read confirmation. [%d, %s]",
						errno, strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
		return(rc);
	}
	if(atol(data) != fileSize)
        {
		sprintf(__log_buf, 
			"Client recv'd %d bytes when server sent %d, file=(%s)",
							nbytes,written,srcfile);
		Write_Log(ModuleName, 2, __log_buf);
       	return(ss_FAILURE);
	}
	return(ss_SUCCESS);
} /* END: ss_SendFile */

/*--------------------------------------------------------------------
ss_NewSendFile():
--------------------------------------------------------------------*/
int ss_NewSendFile(char *srcfile)
{
	static char	yMod[] = "ss_NewSendFile";
#if 0
	int	rc, written, fd, nbytes, done;
	struct	stat 	stat_buf;
	guint32	fileSize, yBytesRead;
	long	recvdBytes;
	char	data[1024], result[80];
	char	diag_mesg[256];
	char	sizeStr[20];
	char	*yDataPtr;

	sprintf(LAST_API, "%s", yMod);

	if(GV_SsConnected == 0)
	{
		Write_Log(yMod, 0, "Client disconnected");
		return(ss_DISCONNECT);
	}

	/* send "OK" String and file size */
	if (access (srcfile, F_OK | R_OK) == 0)
	{
		if(stat(srcfile,  &stat_buf) < 0)
		{
			sprintf(__log_buf, 0, "Failed to stat file %s, errno=%d (%s)",
					srcfile, errno, strerror(errno));
			Write_Log(yMod, __log_buf);
			return(ss_FAILURE);
		}
		fileSize = stat_buf.st_size;
	}	
	else
	{
		sprintf(__log_buf, 0, "Failed to access file %s, errno=%d (%s)",
					srcfile, errno, strerror(errno));
		Write_Log(yMod, __log_buf);
		return (ss_FAILURE);
	}

	/*
	** Allocate the memory
	*/
	if((yDataPtr = (void *)malloc(fileSize)) == NULL)
	{
		sprintf(__log_buf, 0, "Failed to malloc (%d) bytes, [%d: %s]",
				fileSize, errno, strerror(errno));
		Write_Log(yMod, __log_buf);
		return(ss_FAILURE);
	}

	if ((fd = open(srcfile, O_RDONLY)) < 0)
	{
		sprintf(__log_buf, 
			"Failed to open file %s for reading, errno=%d (%s)",
					srcfile, errno, strerror(errno));
		Write_Log(yMod, 0, __log_buf);
		free(yDataPtr);
       	return (ss_FAILURE);
	}
	/*
	 * Read the file
	 */
	if((yBytesRead = read(fd, yDataPtr, fileSize)) != fileSize)
	{
		sprintf(__log_buf, 
				"Failed to read (%d) bytes from file (%s), [%d: %s]",
				fileSize, srcfile, errno, strerror(errno));
		Write_Log(yMod, 0, __log_buf);
		free(yDataPtr);
		close(fd);
		return(ss_FAILURE);
	}
	close(fd);

	sprintf(result, "%s|%s|%ld", srcfile, "OK", fileSize);
	if (ss_SendData(strlen(result), result) < 0)
	{
		sprintf(__log_buf, "Failed to send file info to client (%s), "
				   "file=(%s)", "WSS_SendData failed",srcfile);
		Write_Log(yMod, 0, __log_buf);
		free(yDataPtr);
		return(ss_FAILURE);
	}
	/* * Write the contents of the file to the socket */
	if(writeSocketData(yMod, "file contents", fileSize, yDataPtr) != fileSize)
	{ // error msg written in sub-routine
		free(yDataPtr);
		return(ss_FAILURE);
	}

	free(yDataPtr);

	memset(data, 0, sizeof(data));
	rc = ss_RecvData(GV_SysTimeout, -1, data, &recvdBytes);
	if(rc != ss_SUCCESS)
	{
		sprintf(__log_buf, "Failed to read confirmation, errno=%d (%s)",
						errno, strerror(errno));
		Write_Log(yMod, 0, __log_buf);
		return(rc);
	}
	if(atol(data) != fileSize)
        {
		sprintf(__log_buf, 
			"Client recv'd %d bytes when server sent %d, file=(%s)",
							nbytes,written,srcfile);
		Write_Log(yMod, 0, __log_buf);
       	return(ss_FAILURE);
	}
	return(ss_SUCCESS);
#endif

} /* END: ss_NewSendFile */
