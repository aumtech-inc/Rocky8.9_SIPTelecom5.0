/*


Update:		03/07/97 sja	Added this header
Update:		03/18/97 sja	If nbytes != written, return -1
Update:		03/27/97 sja	If disconnected, return sc_DISCONNECT
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Added static char ModuleName[] = ...
Update:		06/05/97 sja	removed the MYDATA/QUEDATA parameter
Update:		06/05/97 sja	Added the dataSize parameter
Update:	06/11/98 mpb	changed GV_Connected to GV_ScConnected. 
Update:	10/22/99 mpb	passing sc_DISCONNECT return code back. 
Update:	09/14/12 djb	Updated logging.

*/

#include "WSC_var.h"
#include "sc.h"

/* #define DEBUG */

static char ModuleName[] = "sc_SendData";

/*-------------------------------------------------------------------------
sc_SendData(): This routine send the data to work station server
-------------------------------------------------------------------------*/
int sc_SendData(long dataSize, char *data)
{
	long 	written = 0, nbytes;
	char	sizeBuf[10], sizeStr[10];
	int	rc;

	if (GV_ScConnected == 0)
	{
		Write_Log(ModuleName, 2, "Server disconnected."); 
		return (sc_DISCONNECT);
	}
	if(dataSize < 0)
	{
		sprintf(__log_buf, "Data size (%ld) cannot be < 0", dataSize);
		Write_Log(ModuleName, 0, __log_buf);
		return(sc_FAILURE);
	}
	if(dataSize == 0)
	{
		Write_Log(ModuleName, 0, "Sending NULL data.");
	}
	if(sendSize(ModuleName, dataSize) < 0)
	{
		return(sc_FAILURE);	/* message written in sub-routine */
	}
	sprintf(__log_buf, "%s|%d|successfully sent size (%d)", __FILE__, __LINE__, dataSize);
	Write_Log(ModuleName, 2, __log_buf);

	written = writeSocketData(ModuleName, "data", dataSize, data);
	if(written < 0)
	{
		return(written);	/* message written in sub-routine */
	}
	sprintf(__log_buf, "%s|%d|%d = writeSocketData(%.*s)", __FILE__, __LINE__, written, 20, data);
	Write_Log(ModuleName, 2, __log_buf);

	rc = recvSize(ModuleName, &nbytes, GV_SysTimeout);
	if(rc < 0)
	{
		if(rc == sc_DISCONNECT)
			return(rc);
		else
			return(sc_FAILURE);/* message written in sub-routine */
	}

	if(nbytes != written)
	{
		sprintf(__log_buf, "Server recv'd %d bytes when client sent %d",
							nbytes,written);
		Write_Log(ModuleName, 2, __log_buf);
		return(sc_FAILURE);
	}
	return (sc_SUCCESS);
} /* END: sc_SendData() */
