/*

Update:		03/07/97 sja	Added this header
Update:		03/27/97 sja	If disconnected, return WSC_DISCONNECT
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Added static char ModuleName[] = ...
Update:		04/01/97 sja	if GV_Connected == 0 , return WSC_DISCONNECT
Update:		06/05/97 sja	Changed SendData parms
Update:		06/05/97 sja	Used writeSocketData, readSocketData
Update:	06/11/98 mpb	changed GV_Connected to GV_ScConnected. 
Update:	11/30/00 sja	Changed sys_errlist to strerror()
Update:	2002/05/22 sja	Added sc_NewSendFile()

*/

#include "WSC_var.h"
#include "sc.h"
/* #define DEBUG */

static char ModuleName[] = "sc_SendFile";

/*--------------------------------------------------------------------

--------------------------------------------------------------------*/
int sc_SendFile(char *srcfile)
{
	int	rc, written, done;
	size_t	nbytes;
	FILE	*fd;
	struct	stat64 	stat_buf;
	char	data[4096000];
//	char	data[96];
	char	result[200];
	long	recvdBytes, recvdSize, totalBytesSent;
	off_t	fileSize;
	int		count;

	if (GV_ScConnected == 0)
	{
		Write_Log(ModuleName, "Server disconnected\n"); 
		return (sc_DISCONNECT);
	}
	if(srcfile == (char *)NULL)
	{
		sprintf(__log_buf, "Source file name is NULL\n");
		Write_Log(ModuleName, __log_buf);
		return (sc_FAILURE);
	}
	if (access (srcfile, F_OK | R_OK) == -1)
	{
		sprintf(__log_buf, "Cannot access file (%s), errno=%d (%s)\n",
				srcfile, errno, strerror(errno));
		Write_Log(ModuleName, __log_buf);
		return (sc_FAILURE);
	}
	if(stat64(srcfile,  &stat_buf) < 0)
	{
		sprintf(__log_buf, "stat64(%s) failed, errno=%d (%s)\n",
					srcfile,errno,strerror(errno));
		Write_Log(ModuleName, __log_buf);
		return (sc_FAILURE);
	}	
	fileSize = stat_buf.st_size;
	sprintf(result, "%s|%s|%llu|", srcfile, "OK", fileSize);

#ifdef DEBUG
	sprintf(__log_buf, "Sending result >%s<\n", result);
	Write_Log(ModuleName, __log_buf);
#endif

	if((rc = sc_SendData(strlen(result), result)) != sc_SUCCESS)
	{
		sprintf(__log_buf, "Failed to send file (%s) info (%s)\n", 
							srcfile, result);
		Write_Log(ModuleName, __log_buf);
		return (rc);
	}
	if((fd = fopen64(srcfile, "rb")) == (FILE *)NULL)
	{
		sprintf(__log_buf, 
				"Failed to open file for read file (%s), "
				"errno=%d (%s)\n", 
				srcfile, errno, strerror(errno));
        	Write_Log(ModuleName, __log_buf);
        	return (sc_FAILURE);
        }
	totalBytesSent = 0;
	done = 0;
	count= 0;
	while(!done)
	{
		memset(data, 0, sizeof(data));
        nbytes = fread(data, 1, sizeof(data), fd);

//		sprintf(__log_buf, 
//				"%s|%d|%d=read((%.*s)) from file.", 
//				__FILE__, __LINE__, nbytes, 100, data);
//		Write_Log(ModuleName, __log_buf);

		switch(nbytes)
		{
			case    0:
				fclose(fd);
//				sprintf(__log_buf, "%s|%d|COUNT-%d, nbytes=%d",
//						__FILE__, __LINE__, count++, nbytes);
//				Write_Log(ModuleName, __log_buf);
				done = 1;
				break;
			case    -1:
				fclose(fd);
				sprintf(__log_buf, "%s|%d|Failed to read file (%s); [%d, %s] "
						"COUNT-%d, nbytes=%d",
						__FILE__, __LINE__, srcfile, errno, strerror(errno),
						count++, nbytes);
				Write_Log(ModuleName, __log_buf);
				return (sc_FAILURE);
			default:
//				sprintf(__log_buf, "%s|%d|COUNT-%d, nbytes=%d",
//						__FILE__, __LINE__, count++, nbytes);
//				Write_Log(ModuleName, __log_buf);
				rc = sc_SendData(nbytes, data);
				if(rc != sc_SUCCESS)
				{
					fclose(fd);
					sprintf(__log_buf, "Failed to send file (%s)\n",srcfile);
					Write_Log(ModuleName, __log_buf);
					return(rc);
				}
				break;
		} /* switch */
	} /* while */

	memset(data, 0, sizeof(data));
	rc = sc_RecvData(GV_SysTimeout, -1, data, &recvdBytes);
	if(rc != sc_SUCCESS)
	{
		sprintf(__log_buf, 
			"Failed to recv. confirmation of file (%s) receipt\n", 
			srcfile);
		Write_Log(ModuleName, __log_buf);
		return(rc);
	}
	if(atol(data) != fileSize)
	{
		sprintf(__log_buf, 
			"Server recv'd %ld bytes when client sent %ld\n",
							atol(data),fileSize);
		Write_Log(ModuleName, __log_buf);
		return(sc_FAILURE);
	}
	return (sc_SUCCESS);
} /* sc_SendFile */


/*--------------------------------------------------------------------
sc_NewSendFile():
--------------------------------------------------------------------*/
int sc_NewSendFile(char *srcfile)
{
	static char	yMod[] = "sc_NewSendFile";
	int	rc, written, fd, nbytes, done;
	struct	stat 	stat_buf;
	long	fileSize, recvdBytes, yBytesRead;
	char	data[1024], result[80];
	char	diag_mesg[256];
	char	sizeStr[20];
	char	*yDataPtr;

	if(GV_ScConnected == 0)
	{
		Write_Log(yMod, "Server disconnected");
		return(sc_DISCONNECT);
	}

	/* send "OK" String and file size */
	if (access (srcfile, F_OK | R_OK) == 0)
	{
		if(stat(srcfile,  &stat_buf) < 0)
		{
			sprintf(__log_buf, 
				"Failed to stat file (%s), errno=%d (%s)",
					srcfile, errno, strerror(errno));
			return(sc_FAILURE);
		}
		fileSize = stat_buf.st_size;
	}	
	else
	{
		sprintf(__log_buf, "Failed to access file (%s), errno=%d (%s)",
					srcfile, errno, strerror(errno));
		Write_Log(yMod, __log_buf);
		return (sc_FAILURE);
	}

	/*
	** Allocate the memory
	*/
	if((yDataPtr = (void *)malloc(fileSize)) == NULL)
	{
		sprintf(__log_buf, 
				"Failed to malloc (%d) bytes, [%d: %s]",
				fileSize, errno, strerror(errno));
		Write_Log(yMod, __log_buf);

		return(sc_FAILURE);
	}

	if ((fd = open(srcfile, O_RDONLY)) < 0)
	{
		sprintf(__log_buf, 
			"Failed to open file (%s) for reading, errno=%d (%s)",
					srcfile, errno, strerror(errno));
		Write_Log(yMod, __log_buf);

		free(yDataPtr);

       	return (sc_FAILURE);
	}
			
	/*
	 * Read the file
	 */
	if((yBytesRead = read(fd, yDataPtr, fileSize)) != fileSize)
	{
		sprintf(__log_buf, 
				"Failed to read (%d) bytes from file (%s), [%d: %s]",
				fileSize, srcfile, errno, strerror(errno));
		Write_Log(yMod, __log_buf);

		free(yDataPtr);

		close(fd);

		return(sc_FAILURE);
	}

	close(fd);

	sprintf(result, "%s|%s|%ld", srcfile, "OK", fileSize);
	if (sc_SendData(strlen(result), result) < 0)
	{
		sprintf(__log_buf, "Failed to send file info to client (%s), "
				   "file=(%s)", "WSS_SendData failed",srcfile);
		Write_Log(yMod, __log_buf);

		free(yDataPtr);
		return(sc_FAILURE);
	}

	/*
	 * Write the contents of the file to the socket
	 */
	if(writeSocketData(yMod, "file contents", fileSize, yDataPtr) != fileSize)
	{
		// error msg written in sub-routine

		free(yDataPtr);

		return(sc_FAILURE);
	}

	free(yDataPtr);

	memset(data, 0, sizeof(data));
	rc = sc_RecvData(GV_SysTimeout, -1, data, &recvdBytes);
	if(rc != sc_SUCCESS)
	{
		sprintf(__log_buf, "Failed to read confirmation, errno=%d (%s)",
						errno, strerror(errno));
		Write_Log(yMod, __log_buf);
		return(rc);
	}
	if(atol(data) != fileSize)
        {
		sprintf(__log_buf, 
			"Client recv'd %d bytes when server sent %d, file=(%s)",
							nbytes,written,srcfile);
		Write_Log(yMod, __log_buf);
        	return(sc_FAILURE);
	}
	return(sc_SUCCESS);

} /* END: sc_NewSendFile */

