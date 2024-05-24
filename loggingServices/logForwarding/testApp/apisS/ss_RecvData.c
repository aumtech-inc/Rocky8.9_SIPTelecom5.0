/*----------------------------------------------------------------------------
Program:	ss_RecvData
Purpose:	To receive data over a socket connection
Author:		Aumtech, Inc.
Update:		01/22/97 sja	Added code to return fail when 0 bytes are read
Update:		02/01/97 sja	Made "Read 0 bytes" message NORMAL (was VERBOSE)
Update:		03/18/97 sja	Write strlen(sizeBuf) when confirming read
Update:		03/27/97 sja	If disconnected, return WSS_DISCONNECT
Update:		03/31/97 sja	Added send_to_monitor
Update:		03/31/97 sja	Removed 2nd send_to_monitor
Update:		04/02/97 sja	Changed #include WSS_Globals.h to WSS_Externs.h
Update:		06/05/97 sja	Removed WSS_VAR.h
Update:		06/05/97 sja	Used recvSize, sendSize & readSocketData
Update:         05/04/98 gpw    removed .h files not needed for lite version
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
Update:	2000/11/30 sja	Changed sys_errlist[] to strerror()
----------------------------------------------------------------------------*/
#include "ssHeaders.h"
#include "WSS_Externs.h"

static char ModuleName[]="ss_RecvData";

/* #define DEBUG */

/*----------------------------------------------------------------------------
ss_RecvData(): 	This routine receives the data from the client.
----------------------------------------------------------------------------*/
int ss_RecvData(int timeout, long dataSize, char *dataBuf, long *readBytes)
{
	char	diag_mesg[100];
	int	rc;
	long 	nbytes;
	char	*recvbuf;
	static char mod[]="ss_RecvData";

	sprintf(LAST_API, "%s", ModuleName);

	if(GV_SsConnected == 0)
	{
		Write_Log(ModuleName, "Client disconnected");
		return (ss_DISCONNECT);
	}
	if(timeout < 0)
	{ 
		sprintf(__log_buf, "Invalid paramter %s: (%d), valid range %s",
						"Timeout", timeout, "> zero");
		Write_Log(ModuleName, __log_buf);
        	return (ss_FAILURE);
	}
	/* dataSize should either be -1 or >= 0 */
	if(dataSize < 0 && dataSize != -1)
	{ 
		sprintf(__log_buf, "Invalid paramter %s: (%d), valid range %s",
					"dataSize", dataSize, "> zero");
		Write_Log(ModuleName, __log_buf);
        	return (ss_FAILURE);
	}

	gaVarLog(mod, 1,"%s|%d|Calling recvSize", __FILE__, __LINE__);
	/* Read how much data the server is going to send */
	if(recvSize(ModuleName, &nbytes, timeout) < 0)
	{
		return(ss_FAILURE);	/* message written in sub-routine */
	}
//	sprintf(__log_buf, "%s|%d|DJB: received size %d", __FILE__, __LINE__, nbytes);
//	Write_Log(ModuleName, __log_buf);

	/* Malloc enough buffer to recv the data */
	if((recvbuf = (char *)malloc(nbytes + 10)) == NULL)
	{
		sprintf(__log_buf, "malloc(%ld) failed, errno=%d (%s)",
					nbytes, errno, strerror(errno));
		Write_Log(ModuleName, __log_buf);
       	return(ss_FAILURE);
	}
	memset((char *)recvbuf, '\0', nbytes);

	//gaVarLog(mod, 1,"%s|%d|Calling readSocketData(); nbytes=%d", __FILE__, __LINE__);
	/* Now read the data */
	rc = readSocketData(ModuleName,"data", timeout, nbytes, recvbuf);
	if(rc < 0)
	{
		gaVarLog(mod, 0,"%s|%d|readSocketData() failed.", __FILE__, __LINE__);
		free(recvbuf);
		return(rc);		/* message written in sub-routine */
	}
	gaVarLog(mod, 1,"%s|%d|readSocketData() read %d bytes.", __FILE__, __LINE__, rc);

	if(sendSize(ModuleName, rc) < 0)
	{
		free(recvbuf);
		return(ss_FAILURE);	/* message written in sub-routine */
	}
	
	/* If we recv'd 0 bytes when we were not expecting 0 bytes, return -1 */
	if(dataSize != 0 && dataSize != -1)
	{
		if(rc == 0)
		{
			Write_Log(ModuleName, "Read zero bytes");
		}
	}

	if(dataSize == -1)
	{
		/* Copy as many bytes as we recv'd into the user's buffer */
		*readBytes = nbytes;
	}
	else
	{
		/* Copy the lower of dataSize & nbytes into the user's buffer */
		if(dataSize < nbytes)
		{
			*readBytes = dataSize;
		}
		else
		{
			*readBytes = nbytes;
		}
	}
	memcpy(dataBuf, recvbuf, *readBytes);

	free(recvbuf);

	return (ss_SUCCESS);
} /* END: ss_RecvData() */
