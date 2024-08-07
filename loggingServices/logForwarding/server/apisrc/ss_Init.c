/*-----------------------------------------------------------------------------
Program:	ss_Init
Purpose:
Author:
Update:		03/07/97 sja	Added this header 
Update:		03/26/97 sja	If disconnected, return WSS_DISCONNECT
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Added send_to_monitor
Update:		03/31/97 sja	Removed 2nd send_to_monitor
Update:		04/16/97 sja	Called send2monitor AFTER parse_arguments
Update:		06/05/97 sja	Removed WSS_VAR.h & Que related vars
Update:		01/08/98 vbk	added setsocketopt call with SO_KEPPALIVE
Update:         05/04/98 gpw 	removed .h files not needed for lite version
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
Update:	2000/11/30 sja	Changed sys_errlist[] to strerror()
Update: 09/14/12 djb  Updated/cleaned up logging.
------------------------------------------------------------------------------*/

#define	GLOBAL
#include "ssHeaders.h"
#include "WSS_Globals.h"

void clean_up();
static char ModuleName[]="ss_Init";

char	ProgramName[51];
char	originator[21];
char	destination[21];
int	diag_on_flag;
char	appl_name[60];				/* current status of appli. */

static int parse_arguments(int argc, char *argv[]);

/* #define DEBUG   */
/*----------------------------------------------------------------------------
This routine is called when Signal SIGTERM is sent.
---------------------------------------------------------------------------*/
static void proc_sigterm(int zSignalNumber)
{
	shutdownSocket("proc_sigterm");
	exit (0);
} /* proc_sigterm() */

/*--------------------------------------------------------------------------
ss_Init() : This routine initiate the work station server.
---------------------------------------------------------------------------*/
int ss_Init(int argc, char **argv)
{
	int	ret_code;
	int 	optvalp, status;
	long	time_sec;			/* time in seconds */
	struct	utsname name;			/* machine name */
	char	str_stat[10];			/* current status of app. */
	char	diag_mesg[100];

	GV_ShutdownDone = 0;

	signal(SIGTERM, proc_sigterm);

	sprintf(LAST_API, "%s", ModuleName);

 	/* Initialize shared memory index used for monitoring */
	GV_shm_index = -1;

	if (GV_SsConnected != 0)
	{
		sprintf(__log_buf, "%s|%d|Client disconnected", __FILE__, __LINE__);
		Write_Log(ModuleName, 1, __log_buf);
		return(ss_FAILURE);
	}

	GV_SysTimeout = 10;

	parse_arguments(argc, argv);

	/* Set the Global variables for application use. */
	GV_WSS_PortNumber = atoi(__port_name);

	memset(GV_WSS_PortName, 0, sizeof(GV_WSS_PortName));
	strcpy(GV_WSS_PortName, __port_name);

	GV_WSS_ANI[0] = '\0';
	strcpy(GV_WSS_ANI, originator);

	GV_WSS_DNIS[0] = '\0';
	strcpy(GV_WSS_DNIS, destination);

	GV_CDR_AppName[0] = '\0';
	strcpy(GV_CDR_AppName, ProgramName);

	if(uname(&name) == -1)		/* get node name of the server */
	{
		sprintf(__log_buf, "Unable to get uname. [%d, %s]", 
						errno, strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
		return(ss_FAILURE);
	}

	GV_SsConnected = 1;
	GV_InitCalled = 1;

	optvalp = 1;
	status = setsockopt(GV_tfd, SOL_SOCKET, SO_KEEPALIVE, 
					(char *) &optvalp, sizeof(optvalp));
	if(status != 0)
		{
		sprintf(__log_buf,"setsocketopt failed.  [%d, %s]", 
						errno, strerror(errno));
		Write_Log(ModuleName, 0, __log_buf);
		}

#ifdef DEBUG
	optlen = sizeof(level);
	status = getsockopt(GV_tfd, SOL_SOCKET, SO_KEEPALIVE, (char *) 
							&optvalp, &optlen);
	fprintf(stderr,"errno= [%d] , msg = [%s]\n", errno, strerror(errno));
	fprintf(stderr,"status  = [%d]\n", status);
	fprintf(stderr,"optvalp = [%d]\n", optvalp);
	fprintf(stderr,"optlen  = [%d]\n", optlen);
#endif

	atexit(clean_up);

	return (ss_SUCCESS);
} /* END: ss_Init() */

/*------------------------------------------------------------------------------
Set the value of string passed based on the switch. This routine is used
to parse the command line arguments that were passed to the executable and
then passed on to this API.
------------------------------------------------------------------------------*/
void arg_switch(int c, char *string)
{

	int	i;
	char	error[50];

	memset(error, 0, sizeof(error));

	switch(c)
	{
    		case 'p': /* port number */
			strcpy(__port_name, string);
			if(atoi(__port_name) < 0)
			{
	    			sprintf(error,"Illegal port number %s",
								__port_name);
					Write_Log(ModuleName, 0, error);
	    			return;
			}
			break;

    		case 'P': /* application name */
			strncpy(ProgramName, string, 50);
			strcpy(appl_name,ProgramName);
			ProgramName[50] = '\0';
			break;


    		case 'I': /* shared memory index */
			GV_shm_index = atoi(string);
			break;


    		case 'A': /* ANI */
			strncpy(originator, string, 15);
			originator[15] = '\0';
			break;

    		case 'D': /* DNIS */
			strncpy(destination, string, 15);
			destination[15] = '\0';
			/* RLT Initialization is done here in Linkon */
			break;

    		case 'S': /* Static or Dynamic Application */
			GV_StaticDynamic = atoi(string);
			break;

    		case 's': /* str_fd */
			GV_tfd = atoi(string);
			break;

    		case 'y': /* Accounting Code */
			strcpy(GV_AccountCode, string);
			break;

    		case 'h': /* HeartBeat Interval Default. */
/* 			GV_HeartBeatInterval = atoi(string); */
			GV_HeartBeatInterval = 25;
			break;
		
		case 'm':
			strcpy(GV_ClientHost, string);
			break;

		case 'i':
			GV_ClientPid = atoi(string);
			break;

    		default:
			sprintf(error,"Illegal parameter -%c",(char)c);
			Write_Log(ModuleName, 0, error);
  	} /* switch(c) */ 

#ifdef DEBUG_PARMS

	fprintf(stderr, "Port number = [%s].\n",__port_name);
	fprintf(stderr, "Program name = [%s].\n",ProgramName);
	fprintf(stderr, "Shared memory index = [%d].\n",GV_shm_index);
	fprintf(stderr, "Originator (ANI) = [%s].\n",originator);
	fprintf(stderr, "Destination (DNIS) = [%s].\n",destination);
	fprintf(stderr, "GV_AccountCode = %s.\n", GV_AccountCode);
	fprintf(stderr, "GV_HeartBeatInterval = %d.\n", GV_HeartBeatInterval);
	fflush(stderr);

#endif
	return;
} /* END: arg_switch() */ 
/*----------------------------------------------------------------------------
parse_arguments(): Parse command line args passed to the app.
---------------------------------------------------------------------------*/
static int parse_arguments(int argc, char *argv[])
{
	int c;

	while( --argc > 0 )
	{
    		*++argv;

		if(argv[1][0] != '-')
		{
        		arg_switch((*argv)[1], argv[1]);
        		*++argv;
        		--argc;
    		}
    		else
		{
        		arg_switch((*argv)[1]," ");
		}
  	}
	return(1);
} /* parse_arguments() */ 
/*-------------------------------------------------------------
clean_up() : This routine does clean up for network connection.
---------------------------------------------------------------*/
void clean_up()
{
	char	dummy[10];

	memset(dummy, 0, sizeof(dummy));
	while (read(GV_tfd, dummy, 5) > 0); 	/* Socket cleanup */

	shutdownSocket(ModuleName);

	return ;
} /* END: clean_up() */
