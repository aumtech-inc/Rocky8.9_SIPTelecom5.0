/*

Update:		03/07/97 sja	Added this header
Update:		03/27/97 sja	If disconnected, return sc_DISCONNECT
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Added static char ModuleName[] = ...
Update:		04/01/97 sja	Cleaned Write_Log messages
Update:		10/24/97 mpb	closed socket and GV_Connected on failure.
Update:	06/11/98 mpb	changed GV_Connected to GV_ScConnected. 
Update:	11/30/00 sja	Changed sys_errlist to strerror()
Update:	09/14/12 djb	Updated logging.
*/

#include "WSC_var.h"
#include "sc.h"

static char ModuleName[] = "sc_Exit";

/*----------------------------------------------------------------------
sc_Exit(): This routine close the connection and do cleanup
------------------------------------------------------------------------*/
int sc_Exit()
{
	if (GV_ScConnected == 0)
	{
		Write_Log(ModuleName, 2, "Server disconnected\n"); 
		return (sc_DISCONNECT);
	}

	if(shutdown(sockfd, 2) == -1)		/* shutdown the connection */
	{
		sprintf(__log_buf, "shutdown failed. [%d, %s]", errno,
							strerror(errno));
		Write_Log(ModuleName, 2, __log_buf);
		close(sockfd);
		GV_ScConnected = 0;
		return (sc_FAILURE);
	}
	close(sockfd);
	GV_ScConnected = 0;
	return (sc_SUCCESS);
} /* sc_Exit() */
