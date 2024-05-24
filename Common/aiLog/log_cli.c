/*-----------------------------------------------------------------------
Program:	log_cli.c
Purpose:	Client routine which will attempt to read lines from
		a fifo and send the data to the primary server.  If it unable to 
		send the data (ie. lost connection) to the primary server,
		it attempts writes to a local file.
Author:		Dan Barto

Update:		03/04/97 sja	Commented out signal(SIGSEGV, Terminate);
Update:		03/14/97 djb	Removed references to LOG_2x_LOCAL_FILE.
Update:		05/12/97 djb	If connection is lost, write local for 
					$REMOTE_SLEEPTIME, then try again.
Update:		10/23/97 djb	Uncommented #define DEBUG
Update:		10/24/97 djb	commented #define DEBUG
Update:		10/30/97 djb	changed all logging to gaVarLog()
Update:		11/05/97 djb	added memset for server; exits if local
Update:		11/06/97 djb	inserted memsets
Update:	    03/16/04 djb	Removed TmpLocalWrite() logic.  Once a connection was
						lost, this would only write locally.  Now it 
						re-established the connection.
------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <rpc/rpc.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/signal.h>

#include "log_defs.h"
#include "sc.h"
#include "gaUtils.h"

#define MAXBUFF			512
#define	REMOTE_SLEEPTIME	600	/* 10 minutes */
/* #define DEBUG */

void	Terminate();
void	SigPipe();
int	InitFifo();

int	*result;
FILE	*rfifo_fd, *wfifo_fd;
char	data[MAXBUFF];

static char server[PATHSZ]; /* store primary and secondary server */
static char fifoname[PATHSZ];
static char logpaths[MAXLOGDIRS][PATHSZ];

main (argc, argv)
int argc;
char *argv[];
{
int i;
static char	mod[]="main";
char		buff[MAXBUFF];
char		nodename[32];
int		ret_code, first_time = 1, forward_sw;
int		LocalRoutine();
int		RemoteRoutine();
static char	gaLogDir[256];
int		rc;

signal(SIGTERM, Terminate);
signal(SIGHUP, Terminate);
signal(SIGINT, Terminate);
/* signal(SIGSEGV, Terminate); */
signal(SIGPIPE, SigPipe);

sprintf(gaLogDir,"%s/LOG", (char *)getenv("ISPBASE"));
rc=gaSetGlobalString("$PROGRAM_NAME", argv[0]);
rc=gaSetGlobalString("$LOG_DIR", gaLogDir);
rc=gaSetGlobalString("$LOG_BASE", argv[0]);

memset((char *)logpaths[0], 0, PATHSZ);
memset((char *)logpaths[1], 0, PATHSZ);

GetLogPaths(logpaths[0],logpaths[1]);

memset((char *)server, 0, sizeof(server));
memset((char *)nodename, 0, sizeof(nodename));
if ((ret_code=GetLogServer(server, nodename)) == -1)
	forward_sw=LOCAL_LOG;
else
	if (strcmp(server, nodename))
		forward_sw=FORWARD_LOG;
	else
		forward_sw=LOCAL_LOG;

if (forward_sw == LOCAL_LOG)
	{
	exit(0);
/*	LocalRoutine(); */
	}
else
	{
	sprintf(fifoname,"%s/Global/%s",(char *)getenv("ISPBASE"),FIFONAME);

	while ((ret_code=InitFifo(fifoname)) != 0)
		sleep(30);

	ret_code = RemoteRoutine();
	}

Terminate();
}

int LocalRoutine()
/*--------------------------------------------------------------*/
/* Purpose: Read from fifo and write to local log file.		*/
/*--------------------------------------------------------------*/
{
int		ret_code;
static char	mod[]="LocalRoutine";

while (fgets(data, MAXBUFF, rfifo_fd) != NULL)
	{
	if ((ret_code=WriteLocal(logpaths[0], data,
					LOCAL_LOG)) < 0)
		return(-1);
	}

}

int RemoteRoutine()
/*--------------------------------------------------------------*/
/* Purpose: Read from fifo and write to remote log file.	*/
/*--------------------------------------------------------------*/
{
int		ret_code;
int		InitConnection();
static char	mod[]="RemoteRoutine";
int		connectionEstablished = 0;

	if ((ret_code=InitConnection()) != 0)
	{
//		return(-1);
		connectionEstablished = 0;
	}
	else
	{
		connectionEstablished = 1;
	}

	while (fgets(data, MAXBUFF, rfifo_fd) != NULL)
	{
		if ( ! connectionEstablished )
		{
			if ((ret_code=InitConnection()) != 0)
			{
				connectionEstablished = 0;
			}
			else
			{
				connectionEstablished = 1;
			}
		}

		if ( connectionEstablished )
		{
			gaVarLog(mod, 1,"Read %d bytes from fifo.  "
						"Attempting to send (sc_SendData).", strlen(data));
			if ( (ret_code = sc_SendData(strlen(data), data)) != 0 )
			{
				gaVarLog(mod,0,"sc_SendData failed to send %d bytes.  rc=%d.  "
						"Attempting to write locally.",
						strlen(data), ret_code);
				if ((ret_code=WriteLocal(logpaths[0],
						data,FORWARD_LOG)) < 0)
				{
					gaVarLog(mod,0,"Unable to write locally. rc=%d.  Returning",
						ret_code);
					return(-1);
				}
				gaVarLog(mod, 1, "Successfully wrote %d bytes locally. rc = %d",
						strlen(data), ret_code);
		
				ret_code = sc_Exit();
				
				if ((ret_code=InitConnection()) != 0)
				{
					continue;
				}
			}
			gaVarLog(mod, 1, "sc_SendData succeeded.  rc=%d.", ret_code);
		}
		else
		{
			gaVarLog(mod, 1,"[%d] Calling WriteLocal()", __LINE__);
			if ((ret_code=WriteLocal(logpaths[0],
						data,FORWARD_LOG)) < 0)
			{
				gaVarLog(mod,0,"Unable to write locally. rc=%d.  Returning",
						ret_code);
				return(-1);
			}
			gaVarLog(mod, 1, "Successfully wrote %d bytes locally. rc = %d",
						strlen(data), ret_code);
		}
	}
}

int InitConnection()
/*--------------------------------------------------------------*/
/* Purpose: Initialize socket connection and establish data	*/
/*	    transfer.						*/
/*	    If sc_Init() fails, will go into loop for		*/
/*	    REMOTE_SLEEPTIME seconds, reading from the fifo	*/
/*	    and writing to the forward file.			*/
/* Returns:  0  - when sc_Init() succeeds			*/
/*	    -1  - when sc_Init() and WriteLocal() fails.	*/
/*	    Note when WriteLocal fails, that means it cannot	*/
/*	    write to neither of the two (LOCAL_X_FILE and	*/
/*	    LOCAL_2X_FILE) files.  All bets are off at that 	*/
/*	    and this process will die.				*/
/*--------------------------------------------------------------*/
{
int	ret_code;
int	done=0;
static char	mod[]="InitConnection";

while ( ! done )
{
	gaVarLog(mod, 0,"Attempting to initialize connection with the server.");
	if ((ret_code = sc_Init("log_svc", server)) != 0)
	{
		gaVarLog(mod,0,"sc_Init failed.  rc=%d", ret_code);
		return(-1);
	}
	gaVarLog(mod,0,"Successful initialization.  sc_Init succeeded.  rc=%d",
			ret_code);
	
	if ((ret_code = sc_SendData(strlen(XFER_DATA), XFER_DATA)) != sc_SUCCESS)
	{
		gaVarLog(mod,0,"SendData of <%s> to %s failed. rc=%d",
				XFER_DATA,server,ret_code);
		sc_Exit();
		return(-1);
	}
	else
	{
		gaVarLog(mod,0,"SendData of <%s> to %s succeeded. rc=%d",
				XFER_DATA,server,ret_code);
		done=1;
	}
}

return(ret_code);
}

int InitFifo(fifoname)
/*--------------------------------------------------------------*/
/* Purpose: create and open fifo.				*/
/*--------------------------------------------------------------*/
char *fifoname;
{
	static char	mod[]="InitFifo";
	
	if ( access( fifoname, R_OK | W_OK ) == -1 )
	{
		if ( (mknod(fifoname, S_IFIFO | PERMS, 0) < 0) && (errno != EEXIST))
		{
			gaVarLog(mod,0,"Unable to create FIFO (%s); errno=%d",
						fifoname, errno);
			return(-1);
		}
		gaVarLog(mod,1,"Successfully created fifo (%s).", fifoname);
	}

	if ((rfifo_fd=fopen(fifoname,"r")) == NULL)
	{
		gaVarLog(mod,0,"Unable to open fifo (%s).  errno=%d",fifoname,errno);
		return(-1);
	}
	gaVarLog(mod,1,"Successfully opened fifo (%s) for reading.", fifoname);
	/* Open for writing so I don't have to keep
	   reopening the fifo when no other process have
	   it open for reading */

	if ((wfifo_fd=fopen(fifoname,"w")) == NULL)
	{
		gaVarLog(mod,0,"Unable to open fifo %s.  errno=%d",fifoname,errno);
		return(-1);
	}
	gaVarLog(mod,1,"Successfully opened fifo (%s) for writing.", fifoname);

	return(0);

}

void SigPipe()
{
int rc;
static char	mod[]="SigPipe";

rc=InitFifo(fifoname);

signal(SIGPIPE,SigPipe);

return;
}

void Terminate()
/*--------------------------------------------------------------*/
/* Purpose: Perform cleanup and exit                            */
/*--------------------------------------------------------------*/
{
static char     mod[] = "Terminate";

signal(SIGTERM, SIG_IGN);
signal(SIGSEGV, SIG_IGN);
signal(SIGHUP, SIG_IGN);
signal(SIGINT, SIG_IGN);

fclose(rfifo_fd);
fclose(wfifo_fd);
sc_Exit();

gaVarLog(mod, 1,"Exiting.");
exit(0);

}