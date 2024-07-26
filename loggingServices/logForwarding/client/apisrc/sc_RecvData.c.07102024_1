/*


Update:		02/01/97 sja	Set done = 1 if nbytes = 0
Update:		03/18/97 sja	Write strlen(sizeBuf) when confirming recv
Update:		03/27/97 sja	If disconnected, return sc_DISCONNECT
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Added static char ModuleName[] = ...
Update:		06/05/97 sja	Removed Que related variables
Update:		06/05/97 sja	Added the dataBuf & dataSize parameter
Update:	06/11/98 mpb	changed GV_Connected to GV_ScConnected. 
Update:	11/30/00 sja	Changed sys_errlist to strerror()
Update:	07/20/02 mpb&ddn Returned rc in sc_RecvData for recvSize	
Update:	07/29/02 ddn Added sc_NewRecvData to call newRecvSize instead of recvSize.
Update:	09/14/12 djb	Updated logging.

*/
#define GLOBAL

#include "WSC_var.h"
#include "sc.h"

static char ModuleName[] = "sc_RecvData";

/*----------------------------------------------------------------------------
sc_RecvData(): 	This routine receives the data from the server.
----------------------------------------------------------------------------*/
int sc_RecvData(int timeout, long dataSize, char *dataBuf, long *readBytes)
{
	long	nbytes;
	int	rc;
	char	*recvbuf;

	if (GV_ScConnected == 0)
	{
		Write_Log(ModuleName, 1, "Server disconnected."); 
		return (sc_DISCONNECT);
	}
	if (timeout < 0)
	{ 
		sprintf(__log_buf, "Timeout (%d) is < 0.", timeout);
		Write_Log(ModuleName, 0, __log_buf);
        	return (sc_FAILURE);
	}

	/* dataSize should either be -1 or >= 0 */
	if (dataSize < 0 && dataSize != -1)
	{ 
		sprintf(__log_buf, "Data size (%ld) must be -1 or >= 0.",
								dataSize);
		Write_Log(ModuleName, 0, __log_buf);
        	return (sc_FAILURE);
	}

	/* Read how much data the server is going to send */
	rc = recvSize(ModuleName, &nbytes, timeout);

	if(rc < 0)
	{
		return(rc);	/* message written in sub-routine */
	}

	/* Malloc enough buffer to recv the data */
	if((recvbuf = (char *)malloc(nbytes)) == NULL)
	{
		sprintf(__log_buf, "malloc(%d) failed, errno=%d (%s).",
					nbytes, errno, strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
        	return(sc_FAILURE);
	}
	memset(recvbuf, 0, nbytes);

	/* Now read the data */
	rc = readSocketData(ModuleName,"data", timeout,nbytes,recvbuf);
	if(rc < 0)
	{
		free(recvbuf);
		return(rc);		/* message written in sub-routine */
	}
	if(sendSize(ModuleName, rc) < 0)
	{
		free(recvbuf);
		return(sc_FAILURE);	/* message written in sub-routine */
	}
	
	/* If we recv'd 0 bytes when we were not expecting 0 bytes, return -1 */
	if(dataSize != 0 && dataSize != -1)
	{
		if(rc == 0)
		{
			sprintf(__log_buf, "Read zero bytes.");
			Write_Log(ModuleName, 1, __log_buf);
/* 			return(sc_FAILURE); */
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

	return (sc_SUCCESS);
}  /* END: sc_RecvData() */


/*----------------------------------------------------------------------------
sc_NewRecvData(): 	This routine receives the data from the server.
----------------------------------------------------------------------------*/
int sc_NewRecvData(int timeout, long dataSize, char *dataBuf, long *readBytes)
{
	long	nbytes;
	int	rc;
	char	*recvbuf;

#if 0
	if (GV_ScConnected == 0)
	{
		Write_Log(ModuleName, 1, "Server disconnected."); 
		return (sc_DISCONNECT);
	}
	if (timeout < 0)
	{ 
		sprintf(__log_buf, "Timeout (%d) is < 0.", timeout);
		Write_Log(ModuleName, 0, __log_buf);
        	return (sc_FAILURE);
	}

	/* dataSize should either be -1 or >= 0 */
	if (dataSize < 0 && dataSize != -1)
	{ 
		sprintf(__log_buf, "Data size (%ld) must be -1 or >= 0.",
								dataSize);
		Write_Log(ModuleName, 0, __log_buf);
        	return (sc_FAILURE);
	}

	/* Read how much data the server is going to send */
	rc = newRecvSize(ModuleName, &nbytes, timeout);

	if(rc < 0)
	{
		return(rc);	/* message written in sub-routine */
	}

	/* Malloc enough buffer to recv the data */
	if((recvbuf = (char *)malloc(nbytes)) == NULL)
	{
		sprintf(__log_buf, "malloc(%d) failed, errno=%d (%s).",
					nbytes, errno, strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
        	return(sc_FAILURE);
	}
	memset(recvbuf, 0, nbytes);

	/* Now read the data */
	rc = readSocketData(ModuleName,"data", timeout,nbytes,recvbuf);
	if(rc < 0)
	{
		free(recvbuf);
		return(rc);		/* message written in sub-routine */
	}
	if(sendSize(ModuleName, rc) < 0)
	{
		free(recvbuf);
		return(sc_FAILURE);	/* message written in sub-routine */
	}
	
	/* If we recv'd 0 bytes when we were not expecting 0 bytes, return -1 */
	if(dataSize != 0 && dataSize != -1)
	{
		if(rc == 0)
		{
			sprintf(__log_buf, "Read zero bytes.");
			Write_Log(ModuleName, 0, __log_buf);
/* 			return(sc_FAILURE); */
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
#endif

	return (sc_SUCCESS);
}  /* END: sc_NewRecvData() */
