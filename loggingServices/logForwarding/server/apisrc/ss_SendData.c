/*-----------------------------------------------------------------------------
Program:	ss_SendData.c
Purpose:	Send data over a sockets interface
Author:		Aumtech, Inc.
Update:		03/07/97 sja	Added this header 
Update:		03/18/97 sja	If nbytes != written, return -1
Update:		03/26/97 sja	If disconnected, return ss_DISCONNECT
Update:		03/31/97 sja	Added send_to_monitor
Update:		04/02/97 sja	Added WSS_Com_parm to send_to_monitor
Update:		04/02/97 sja	Changed #include WSS_Globals.h to WSS_Externs.h
Update:		06/05/97 sja	Removed WSS_VAR.h
Update:		06/05/97 sja	Used recvSize, sendSize & writeSocketData
Update:         05/04/98 gpw    removed .h files not needed for lite version
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
Update: 09/14/12 djb  Updated/cleaned up logging.
-----------------------------------------------------------------------------*/

#include "ssHeaders.h"
#include "WSS_Externs.h"

/* #define DEBUG */

static char ModuleName[] = "ss_SendData";

/*-------------------------------------------------------------------------
ss_SendData(): This routine send the data to work station client
-------------------------------------------------------------------------*/
int ss_SendData(guint32 dataSize, char *data)
{
	size_t 	written, nbytes;
	char	diag_mesg[1024];

	sprintf(LAST_API, "%s", ModuleName);

	if (GV_SsConnected == 0)
	{
		sprintf(__log_buf, "%s|%d|Client disconnected", __FILE__, __LINE__);
		Write_Log(ModuleName, 1, __log_buf);
		return (ss_DISCONNECT);
	}
	if(dataSize < 0)
	{
		sprintf(__log_buf, "%s|%d|Invalid paramter %s: (%d), valid range %s",
					"data size", __FILE__, __LINE__, (int)dataSize, ">= 0");
		Write_Log(ModuleName, 0, __log_buf);
		return (ss_FAILURE);
	}

	if(dataSize == 0)
	{
		Write_Log(ModuleName, 0, "%s|%d|Sending NULL data", __FILE__, __LINE__);
	}

	if(sendSize(ModuleName, dataSize) < 0)
	{
		return(ss_FAILURE);	/* message written in sub-routine */
	}
	written = writeSocketData(ModuleName, "data", dataSize, data);
	if(written < 0)
	{
		sprintf(__log_buf, "%s|%d|writeSocketData(%d, data...) failed.  rc=%d",
			__FILE__, __LINE__, written, dataSize);
		Write_Log(ModuleName, 0, __log_buf);
		return(written);	/* message written in sub-routine */
	}
	sprintf(__log_buf, "%s|%d|%d=writeSocketData(%d, data...)",
			__FILE__, __LINE__, written, dataSize);
	Write_Log(ModuleName, 1, __log_buf);

	if(recvSize(ModuleName, &nbytes, GV_SysTimeout) < 0)
	{
		return(ss_FAILURE);	/* message written in sub-routine */
	}

	/* If the user is trying to send NULL data, don't return a failure */
	if (nbytes == 0 && dataSize != 0)
	{
		sprintf(__log_buf, "%s|%d|Sent zero bytes.", __FILE__, __LINE__);
		Write_Log(ModuleName, 1, __log_buf);
		shutdownSocket(ModuleName);
        	return(ss_FAILURE);
	}
	if (nbytes != written)
        {
		sprintf(__log_buf, "%s|%d|Client recv'd %d bytes when server sent %d.",
				__FILE__, __LINE__, nbytes,written);
		Write_Log(ModuleName, 0, __log_buf);
		shutdownSocket(ModuleName);
        	return(ss_FAILURE);
        }

	return (ss_SUCCESS);
} /* END: ss_SendData() */
