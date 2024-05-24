#include <stdio.h>
#include <com.h>

/*--------------------------------------------------------------------------
readFromSocket():
--------------------------------------------------------------------------*/
int readFromSocket(char *module, char *whatData, int timeout, 
						long dataSize, char *dataBuf)
{
	static char mod[]="readFromSocket";
	short 	done;
	long 	nbytes, bytesToRead, where;
	int		rc;
	struct pollfd	pollSet[1];

	pollSet[0].fd = GV_tfd;		/* fd of the fifo descriptor already opened */
	pollSet[0].events = POLLIN;
	if(timeout == 0)
		rc = poll(pollSet, 1, -1);
	else
		rc = poll(pollSet, 1, timeout*1000);
	if(rc < 0)
		{
//		sprintf(__log_buf, "poll failed errno %d", errno);
//		Write_Log(module, __log_buf);
		return(-1);
		}
	if(pollSet[0].revents == 0)
	{
//		sprintf(__log_buf, "Timed out while waiting for %s after %d "
//				     		"seconds", whatData, timeout);
//		Write_Log(module, __log_buf);
        return (ss_TIMEOUT);
	}
	where = 0;
	bytesToRead = 0;
	done = 0;
	while(!done)
	{
//		gaVarLog(mod, 1, "%s|%d|DJB; dataSize=%d where=%d bytesToRead=%d", __FILE__, __LINE__,
//						dataSize, where, bytesToRead);
		bytesToRead = dataSize - where;
//		gaVarLog(mod, 1, "%s|%d|DJB", __FILE__, __LINE__);

//		gaVarLog(mod, 1, "%s|%d|bytesToRead(%d) = dataSize(%d) - where(%d).  "
//			"Attempting to read %d bytes from fd %d.", 
//			__FILE__, __LINE__, bytesToRead, dataSize, where, bytesToRead, GV_tfd);
//
		if((nbytes = read(GV_tfd, &dataBuf[where], bytesToRead)) < 0)
		{
//			gaVarLog(mod, 0, "%s|%d|Failed to read %s. fd=%d  [%d,%s]",
					__FILE__, __LINE__, whatData, GV_tfd, errno, strerror(errno));
			shutdownSocket(module);
			return (ss_DISCONNECT);
		}

		gaVarLog(mod, 1, "%s|%d|%d = read(fd=%d)", __FILE__, __LINE__, nbytes, GV_tfd);
		if (nbytes == 0)
		{
			gaVarLog(mod, 1, "%s|%d|Returning (%d)", __FILE__, __LINE__, where);
			return(where);
		}
		where += nbytes;

		//gaVarLog(mod, 1, "%s|%d|DJB", __FILE__, __LINE__);
		if(where >= dataSize)
		{
			gaVarLog(mod, 1, "%s|%d|where=%d, dataSize=%d, done now.", __FILE__, __LINE__, where, dataSize);
			dataBuf[where] = '\0';
			done = 1; 
			break;
		}
//		gaVarLog(mod, 1, "%s|%d|DJB - end of while", __FILE__, __LINE__);
	} while(!done);

//	gaVarLog(mod, 1, "%s|%d|Returning (%d)", __FILE__, __LINE__, where);

	return(where);
} /* END: readFromSocket() */
/*--------------------------------------------------------------------------
writeToSocket():
--------------------------------------------------------------------------*/
int writeToSocket(char *module, char *whatData, long dataSize, char *dataBuf)
{
	int	written;

#ifdef DEBUG
/* 	fprintf(stderr, "Writing data to socket = >%s<\n", dataBuf); */
#endif


	if((written = write(GV_tfd, dataBuf, dataSize)) != dataSize)
	{
		sprintf(__log_buf,"Failed to write %s to socket, errno=%d (%s)",
					whatData, errno, strerror(errno));
		Write_Log(module, __log_buf);
		shutdownSocket(module);
		return(ss_DISCONNECT);
	}
//	snprintf(__log_buf, 255, "%s|%d|DJB: %d=write(%s)", __FILE__, __LINE__, written, dataBuf);
//	Write_Log(module, __log_buf);

	return(written);
} /* END: writeToSocket() */

/*--------------------------------------------------------------------------
ssInit() : This routine initiate the work station server.
---------------------------------------------------------------------------*/
int ssInit(int argc, char **argv)
{
	int	ret_code;
	int 	optvalp, status;
	long	time_sec;			/* time in seconds */
	struct	utsname name;			/* machine name */
	char	str_stat[10];			/* current status of app. */
	char	diag_mesg[100];

//	signal(SIGTERM, proc_sigterm);

	sprintf(LAST_API, "%s", ModuleName);

#if 0
	if (GV_SsConnected != 0)
	{
		Write_Log(ModuleName, "Client disconnected");
		return(ss_FAILURE);
	}

	GV_SysTimeout = 10;
#endif

	parse_arguments(argc, argv);

#if 0
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
#endif
	if(uname(&name) == -1)		/* get node name of the server */
	{
		sprintf(__log_buf, "Unable to get uname, errno=%d (%s)", 
						errno, strerror(errno));
		Write_Log(ModuleName, __log_buf);
		return(ss_FAILURE);
	}

#if 0
	GV_SsConnected = 1;
	GV_InitCalled = 1;
#endif

	optvalp = 1;
	status = setsockopt(GV_tfd, SOL_SOCKET, SO_KEEPALIVE, 
					(char *) &optvalp, sizeof(optvalp));
	if(status != 0)
	{
		sprintf(__log_buf,"setsocketopt errno=[%d, %s]", 
						errno, strerror(errno));
		Write_Log(ModuleName, __log_buf);
	}

	return (0);
} /* END: ssInit() */

/*------------------------------------------------------------------------------
Set the value of string passed based on the switch. This routine is used
to parse the command line arguments that were passed to the executable and
then passed on to this API.
------------------------------------------------------------------------------*/
arg_switch(int c, char *string)
{

	int	i;
	char	error[50];

	memset(error, 0, sizeof(error));

	switch(c)
	{
    		case 'p': /* port number */
#if 0
			strcpy(__port_name, string);
			if(atoi(__port_name) < 0)
			{
	    			sprintf(error,"Illegal port number %s",
								__port_name);
				Write_Log(ModuleName, error);
	    			return (-1);
			}
			break;
#endif
    		case 'P': /* application name */
				sprintf(ProgramName, "%s", string);
			break;


    		case 'I': /* shared memory index */
//			GV_shm_index = atoi(string);
			break;


#if 0
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
#endif
    		case 's': /* str_fd */
			GV_tfd = atoi(string);
			break;

#if 0
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
#endif
		case 'i':
			GV_ClientPid = atoi(string);
			break;

    		default:
			sprintf(error,"Illegal parameter -%c",(char)c);
			Write_Log(ModuleName, error);
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
	return(0);
} /* END: arg_switch() */ 
/*----------------------------------------------------------------------------
parse_arguments(): Parse command line args passed to the app.
---------------------------------------------------------------------------*/
int parse_arguments(int argc, char *argv[])
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
	return (1);
} /* parse_arguments() */ 
