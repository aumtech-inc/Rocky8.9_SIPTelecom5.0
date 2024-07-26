/*


Update:		03/07/97 sja	Added this header
Update:		03/27/97 sja	If disconnected, return WSC_DISCONNECT
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Added hostStatus() before connect
Update:		03/31/97 sja	Added static char ModuleName[] = ...
Update:		04/04/97 sja	If pclose() == -1, log an error message
Update:		04/09/97 sja	if buf=='\0', say No response from server
Update:		05/02/97 sja	Moved GV_Connected = 1 just before returning
Update:		05/05/97 sja	Re-directed stderr of ping to stdout
Update:		05/23/97 sja	On SCO, wait for packet loss message from ping
Update:		05/23/97 sja	On UNIX_SV, wait for "is alive" from ping
Update:		05/29/97 sja	Set R_mesg_ptr to &R_mesg[0]
Update:		06/05/97 sja	Removed MYDATA from WSC_SendData call
Update:		06/05/97 sja	Removed Que related variables
Update:		06/05/97 sja	Removed GV_SysDelimiter
Update:		12/08/97 mpb	Removed check on pclose
Update:	06/11/98 mpb	changed GV_Connected to GV_ScConnected. 
Update:	04/26/99 apj	if connect to ttsServer fails, log the message for
			the first try only.
Update:		10/28/98 mpb	added setsocketopt call with SO_KEEPALIVE
Update:		10/29/98 mpb	added GV_hostName. 
Update:	2000/09/27 sja	Added support for Linux to hostStatus()
Update:         11/14/00 apj    Added newer version on hostStatus() for SCO_SV
Update:         11/17/00 apj    Added newer version on hostStatus() for Linux
Update:	11/30/00 sja	Changed sys_errlist to strerror()
Update:	01/20/04 mpb	Changed GV_hostName from 16 to 64.
Update:	02/05/04 mpb	added case for "loss" to work with Linux 7.3.
Update:	09/14/12 djb	Updated logging.
*/

#include "WSC_var.h"
#include "sc.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/utsname.h>

static char ModuleName[] = "sc_Init";
char	GV_hostName[64];
void	clean_up();				/* clean up function */
static int getPort(int *servPort);
int hostStatus(char *host, int timeout);


/*----------------------------------------------------------------------------
This routine is called when Signal SIGTERM is sent.
---------------------------------------------------------------------------*/
static void proc_sigterm(int zSignalNumber)
{
	shutdown (sockfd, 2);
	close(sockfd);
	return;
} /* proc_sigterm() */
/*----------------------------------------------------------------------------
This routine is called when Signal SIGALRM is sent.
---------------------------------------------------------------------------*/
static void proc_sigalrm(int zSignalNumber)
{
	signal(SIGALRM, proc_sigalrm);

	return;
} /* proc_sigalrm() */
/*-------------------------------------------------------------
clean_up() : This routine does clean up for network connection.
---------------------------------------------------------------*/
void clean_up()
{
	close(sockfd);			/* close file descriptor */
	return ;
} /* clean_up */
/*--------------------------------------------------------------------------
sc_Init() : This routine initiate the client (establish 
	   	  connection) of the work station server.
---------------------------------------------------------------------------*/
int sc_Init(char *application, char *host)
{
	int 	ret, retry;
	long	time_sec;
	extern 	struct hostent *gethostbyname();
	int	servPort, done = 0, where = 0, nbytes = 0;
	char    buf[256];
	struct utsname	myhost;
	struct protoent	*p;
	int	protocol;
	int 	optvalp, status;

	if (GV_ScConnected != 0)
	{
		sprintf(__log_buf, "Client already initialized");
		Write_Log(ModuleName, 0, __log_buf);
		return (sc_FAILURE);
	}

	signal(SIGALRM, proc_sigalrm);                  /* signal alarm clock */
	signal(SIGTERM, proc_sigterm);                  /* signal terminate */

	GV_ScConnected = 0;
	GV_SysTimeout = 10;

	__proc_id = getpid();

	if(application == (char *)NULL)
	{
		sprintf(__log_buf, "App. name can't be NULL");
		Write_Log(ModuleName, 0, __log_buf);
		return (sc_FAILURE);
	}
	if(host == (char *)NULL)
	{
		sprintf(__log_buf, "Host name can't be NULL");
		Write_Log(ModuleName, 0, __log_buf);
		return (sc_FAILURE);
	}
	sprintf(GV_hostName, "%s", host);
	if(hostStatus(host, 2) < 0)
	{
		sprintf(__log_buf,"Host %s is unreachable",host);
		Write_Log(ModuleName, 0, __log_buf);
		return (sc_FAILURE);
	}

	retry = 0;
	for(;;)
	{
		if((p = getprotobyname("TCP")) == NULL)
		{
			sprintf(__log_buf, 
				"getprotobyname failed. [%d, %s]",
				errno, strerror(errno));
			Write_Log(ModuleName, 0, __log_buf);
			return(sc_FAILURE);
		}
		protocol = p->p_proto;
		/* Get socket into TCP/IP */
		if ((sockfd = socket(AF_INET, SOCK_STREAM, protocol)) < 0)
		{
			sprintf(__log_buf, "socket failed. [%d, %s]",
						errno, strerror(errno));
			Write_Log(ModuleName, 0, __log_buf);
			return (sc_FAILURE);
		}

		getPort(&servPort);
		sprintf(__log_buf, "after getPort servPort is %d", servPort);
		Write_Log(ModuleName, 1, __log_buf);
	
		(void)memset((void *)&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port   = htons(servPort);
	
		hp = gethostbyname(host);
		if (hp == 0)
		{
			sprintf(__log_buf,"Could not obtain address of %s", host);
			Write_Log(ModuleName, 0, __log_buf);
			shutdown(sockfd, 2);
			close(sockfd);
			return (sc_FAILURE);
		}
		sprintf(__log_buf, "gethostbyname() returned (%s).", hp->h_name);
	
		memcpy((caddr_t)&serv_addr.sin_addr, hp->h_addr_list[0],
								hp->h_length);

		ret = connect(sockfd, (struct sockaddr *)&serv_addr,
							sizeof(serv_addr));
		if (ret == 0)
		{
			break;
		}
		else
		{
			++retry;
		}
		if(retry == 1)
		{
			sprintf(__log_buf, "Unable to connect to WSS server %s, [%d, %s]",
					host, errno, strerror(errno));
			Write_Log(ModuleName, 0, __log_buf);
		}
		if (retry > MAX_RETRY)
		{
			shutdown (sockfd, 2);
			close(sockfd);
			return(sc_FAILURE);
		}
		else
		{
			close(sockfd);
			sleep (1);
		}
	} /* for */

	GV_ScConnected = 1;

	optvalp = 1;
	status = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, 
					(char *) &optvalp, sizeof(optvalp));
	if(status != 0)
		{
		sprintf(__log_buf,"setsocketopt failed. [%d, %s]", 
						errno, strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
		}

    memset(buf, 0, sizeof(buf));
	if(uname(&myhost) == -1)
	{
		sprintf(__log_buf, "uname() failed. [%d, %s]",
						errno,strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
		return(-1);
	}
	sprintf(buf, "%s|%d|%s|", application, getpid(), myhost.nodename);
	if (sc_SendData(strlen(buf), buf) < 0)
	{
		sprintf(__log_buf, "Failed to send app. name %s to server %s",
							application, host);
		Write_Log(ModuleName, 0, __log_buf);
		GV_ScConnected = 0;
		close(sockfd);
		return(sc_FAILURE);
	}

    memset(buf, 0, sizeof(buf));
	do
	{
       	if((nbytes = read(sockfd, &buf[where], 1)) < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			if (errno == ECONNRESET)
			{
				Write_Log(ModuleName, 0, "Connection closed by server");
				GV_ScConnected = 0;
				close(sockfd);
				return (sc_FAILURE);
			}
			sprintf(__log_buf, "Failed to receive response from server. [%d, %s]", errno, strerror(errno));
			Write_Log(ModuleName, 0, __log_buf);

			GV_ScConnected = 0;
			close(sockfd);
			return (sc_FAILURE);
		}
		sprintf(__log_buf, "%s|%d|%d=read((%s), 1)", __FILE__, __LINE__, nbytes, buf);
		Write_Log(ModuleName, 1, __log_buf);

		if (nbytes == 0)			/* end of file */
		{
				done = 1;
				continue;
		}
	
      	if(buf[where] == NET_EOF)       	/* message terminator */
		{
			buf[where] = '\0';
            done = 1;
		}
		if ( (isspace(buf[where])) || ( ! isprint(buf[where])) )
		{
			sprintf(__log_buf, "%s|%d|Read an unexpected character(0x%x). Ignoring.", __FILE__, __LINE__, buf[where]);
			Write_Log(ModuleName, 1, __log_buf);
		}
		else
		{
        	where += nbytes;
		}
	} while(!done);

//	atexit(clean_up);

	if (strstr(buf, "OK") != NULL)
	{
		GV_ScConnected = 1;
		return (sc_SUCCESS);
	}
	if(buf[0] == '\0')
	{
		strcpy(__log_buf, "No response from server");
	}
	else
	{
		sprintf(__log_buf, "Server error: (%s)", buf);
	}
	Write_Log(ModuleName, 0, __log_buf);
	GV_ScConnected = 0;
	close(sockfd);
	return (sc_FAILURE);
} /* sc_Init() */
/*----------------------------------------------------------------------------
getPort():
---------------------------------------------------------------------------*/
static int getPort(int *servPort)
{

#ifndef SERVER_PORT			/* DEFAULT case */
	*servPort = SERV_TCP_PORT;
#else
	*servPort = SERVER_PORT;
#endif

	return(0);
} /* getPort() */
/*--------------------------------------------------------------------
hostStatus():
--------------------------------------------------------------------*/
int hostStatus(char *host, int timeout)
{
	FILE	*pfp;
	char	command[80], buf[80];
	struct utsname	myhost;

	memset(&myhost, 0, sizeof(myhost));
	if(uname(&myhost) == -1)
	{
		sprintf(__log_buf, "uname() failed. [%d, %s]",
						errno,strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
		return(-1);
	}

	if(strcmp(myhost.sysname,  "SCO_SV") == 0)
	{
		sprintf(command, "/usr/bin/ping -c 1 %s 2>&1", host);
		if((pfp = popen(command, "r")) == NULL)
		{
			sprintf(__log_buf, "popen(%s) failed. [%d, %s]",
					command,errno,strerror(errno));
			Write_Log(ModuleName, 0, __log_buf);
			return(-1);
		}

                while((fgets(buf, sizeof(buf)-1, pfp)) != NULL)
                {
                        if(strstr(buf, "No route to host") != NULL)
                                break;
                        else
                        if(strstr(buf, "packet loss") != NULL)
                        {
                                if(strstr(buf, "100%") == NULL)
                                {
                                        pclose(pfp);
                                        return(0);
                                }
                                else
                                        break;
                        }
                }

                pclose(pfp);
                return(-1);
	}
	else if(strcmp(myhost.sysname,  "UNIX_SV") == 0 ||
		strcmp(myhost.sysname,  "SunOS") == 0)
	{
		sprintf(command, "/usr/sbin/ping %s %d 2>&1", host, timeout);
		if((pfp = popen(command, "r")) == NULL)
		{
			sprintf(__log_buf, "popen(%s) failed. [%d, %s]",
					command,errno,strerror(errno));
			Write_Log(ModuleName, 0, __log_buf);
			return(-1);
		}

		memset(buf, 0, sizeof(buf));

		/* Get ping info */
		fgets(buf, sizeof(buf)-1, pfp);
		pclose(pfp);
		
		if(strstr(buf, "is alive") != NULL)
		{
			return(0);
		}
		else
		{
			return(-1);
		}
	}
	else if(strcmp(myhost.sysname,  "Linux") == 0)
	{
		sprintf(command, "/bin/ping -c 1 %s 2>&1", host);
		if((pfp = popen(command, "r")) == NULL)
		{
			sprintf(__log_buf, "popen(%s) failed. [%d, %s]",
					command,errno,strerror(errno));
			Write_Log(ModuleName, 0, __log_buf);
			return(-1);
		}

                while((fgets(buf, sizeof(buf)-1, pfp)) != NULL)
                {
                        if(strstr(buf, "No route to host") != NULL)
                                break;
                        else
                        if(strstr(buf, "packet loss") != NULL)
                        {
                                if(strstr(buf, "100%") == NULL)
                                {
                                        pclose(pfp);
                                        return(0);
                                }
                                else
                                        break;
                        }
                        else
                        if(strstr(buf, "loss") != NULL)
                        {
                                if(strstr(buf, "100%") == NULL)
                                {
                                        pclose(pfp);
                                        return(0);
                                }
                                else
                                        break;
                        }
                }

                pclose(pfp);
                return(-1);
	}
	else
	{
		sprintf(__log_buf, "Unsupported OS (%s)", myhost.sysname);
		Write_Log(ModuleName, 0, __log_buf);
		return(-1);
	}
} /* END: hostStatus() */
