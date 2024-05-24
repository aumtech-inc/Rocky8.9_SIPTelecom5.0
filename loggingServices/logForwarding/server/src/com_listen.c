/*---------------------------------------------------------------------------
File:		isp_listen.c

Purpose:	Workstation Services Listener Component

Update:		01/24/97 sja	Changed name to isp_listen.c
Update:		03/14/97 sja	Added chdir if we are ISP_LOG
Update:		03/31/97 sja	Update APPL_API in shmem with N/A in cleanup_app
Update:		04/01/97 sja	Cleaned up messages 
Update:		04/08/97 sja	Added license checking
Update:		04/10/97 sja	Added timestamp to Write_Log
Update:		04/11/97 sja	using SIZE_SCHD_MEM_WSS instead of SIZE_SCHD_MEM
Update:		04/15/97 sja	Commented 2nd checkApps() call.
Update:		04/15/97 sja	Ported to WSS lite
Update:		04/21/97 sja	No code change
Update:		05/01/97 sja	For ISP_LOG, absolute path = Global/Exec
Update:		05/02/97 sja	Write sizeof(int) when sending confirmation
Update:		05/21/97 sja	If socket,bind,accept or listen fail, sleep SOCKET_ERROR_NAP
Update:		05/23/97 sja	Defined SOCKET_ERROR_NAP
Update:		05/28/97 mpb	added code to send SIGTERM to all the server processes
				when listener was shuting down.
Update:		05/28/97 mpb	added code to receive client machine name & pid from the
				client and pass it to the server program.
Update:		06/02/97 sja	Added the exitNap parameter to the listener
Update:		06/02/97 sja	Removed ISP from first command line parm
Update:		06/05/97 sja	Added getprotobyname()
Update:		07/22/97 sja	Added ARC to command line parser
Update:		10/06/97 sja	Added version information
Update:	11/01/99 mpb	Changed the application name restriction from 15 to 40,
			in updateShmem.
Update:	2000/11/30 sja	Changed sys_errlist[] to strerror()
Update:	2000/12/04 sja	Changed argc < 2 to argc < 3 for parameter check in main
Update:	2006/06/07 djb	Added dynamic debug logging.
Update: 12/09/14 djb    Updated logging.
--------------------------------------------------------------------------*/
#include "ISPSYS.h"
#include "com_listen.h"
#include "gaUtils.h"

/*DDN: 02032009 */
#ifndef GUINT32
#define GUINT32
typedef unsigned int guint32;
#endif

extern int        GV_tfd;


int     vec[10]     = {0, 9, 18, 36, 49, 55, 66, 117, 120, 127};
int     fld_siz[10] = {8, 8, 17, 12,  5, 10, 50,  2,  6,  2};

/*--------------------------------------------------------------------------
This routine is called when Signal SIGTERM is received.
---------------------------------------------------------------------------*/
void proc_sigterm()
{
	static char mod[]="proc_sigterm";
	time_t	t1;
	int	appl_no;
	char	field[7], appl_name[20];

	time(&t1);
	close (listen_fd);

	for(appl_no=0; appl_no<MAX_PROCESS; appl_no++)
	{
		read_fld(__tran_tabl_ptr, appl_no, APPL_PID, field);
		if(atoi(field) != 0)
		{
			kill(atoi(field), SIGTERM);
		}
	}
	shmdt(__tran_tabl_ptr);	/* detach the shared memory segment */
	gaVarLog(mod, 1, "Received SIGTERM ...Exiting on %s",ctime(&t1));

	exit (0);
} /* proc_sigterm() */

/*--------------------------------------------------------------------------
This routine is called when Signal SIGCHILD is received.
---------------------------------------------------------------------------*/
void proc_sigchild(int x)
{
	static char mod[]="proc_sigterm";

	gaVarLog(mod, 1, "Received SIGCHILD (%d); Ignoring", x);

	if(signal(SIGCHLD, SIG_DFL) == SIG_ERR || sigset(SIGCLD, SIG_DFL) == -1)
	{
		gaVarLog(mod, 0, "sigaction(SIGCHLD,SIG_IGN) failed. [%d, %s]"
		       	,errno,strerror(errno));
	} 

	signal(SIGCHLD, proc_sigchild);

	return;
} /* proc_sigchild() */
/*----------------------------------------------------------------------------
This routine is called when Signal SIGCLD is received.
---------------------------------------------------------------------------*/
int cleanup_app(int int_pid)
{
	static char mod[]="proc_sigterm";
	register int	i;
	int	pid_dead = 0;
	short	updateError;

	pid_dead = int_pid;

	gaVarLog(mod, 1, "Expired PID=%d", pid_dead);

	for (i=0; i <MAX_PROCESS; i++)
	{
		if (pid_dead == mem_map[i].pid)
		{
			updateError = 0;
			if(write_fld(mem_map[i].ptr, i, APPL_STATUS, KILL)< 0)
			{
				updateError = 1;
			}
		 	if(write_fld(mem_map[i].ptr,i,APPL_NAME,"Available")< 0)
			{
				updateError = 1;
			}
		   	if(write_fld(mem_map[i].ptr, i, APPL_PID, "0") < 0)
			{
				updateError = 1;
			}
		   	if(write_fld(mem_map[i].ptr, i, APPL_API, "N/A") < 0)
			{
				updateError = 1;
			}
			if(updateError)
			{
				gaVarLog(mod, 0, 
					"Failed to update status/name in shmem of PID=%d", mem_map[i].pid);
				break;
			}
			gaVarLog(mod, 1, "Freed memory slot = %d", i);
			mem_map[i].pid = -1;		/* free memory slot */
			break;
		} /* if */
	} /* for */

	return(0);
} /* cleanup_app */
/*--------------------------------------------------------------------
Main entry point
--------------------------------------------------------------------*/
main(int argc, char **argv)
{
	static char	mod[]="main";
	short	invalidExitNap;
	int	index, free_slot, ret, written = 0;
	char	program_name[50], path[256], str_fd[10], str_pid[10];
	char	shm_index[10], message[10], absolutePathName[100];
	struct	sigaction	sig_act, sig_oact;
	char 	*ispbase, globalExecDir[256];
	struct 	sockaddr_in clnt_addr;
	size_t 	clnt_len;
	char	client_machine[80], client_pid[8], recv_buf[256];
	char	*ptr;
	char	*ptr2;
	char	gaLogDir[256];

	if(argc == 2 && (strcmp(argv[1], "-v") == 0))
	{
		fprintf(stdout, 
			"Aumtech's Logging Services (%s).\n"
			"Version 2.3.  Compiled on %s %s.\n", argv[0], __DATE__, __TIME__);
		exit(0);
	}
	if(argc < 3)
	{
		fprintf(stderr, "Usage: %s LOG exitNap\n", argv[0]);
		exit(1);
	}

	if((ispbase = getenv("ISPBASE")) == NULL)
	{
		fprintf(stderr, "Env. var. ISPBASE not set.  Set it and retry.\n");
		exit(1);
	}

	sprintf(gaLogDir,"%s/LOG", ispbase);

	gaSetGlobalString("$PROGRAM_NAME", argv[0]);
	gaSetGlobalString("$LOG_DIR", gaLogDir);
	gaSetGlobalString("$LOG_BASE", argv[0]);

	if(getcwd(path, 80) == NULL)
	{
		gaVarLog(mod, 0, "getcwd failed, using ./. [%d, %s]",
						errno,strerror(errno));
		strcpy(path, "./");
	}
//	if(strcmp(argv[1], "ISP_LOG") == 0 || strcmp(argv[1], "LOG") == 0)
//	{
		strcpy(servType, argv[1]);
		shmKey = SHMKEY_LOG;
	
		sprintf(absolutePathName, "%s/Global/Exec/", ispbase);
		if(chdir(absolutePathName) == -1)
		{
			gaVarLog(mod, 0, "chdir(%s) failed. errno=%d (%s).",
				absolutePathName,errno,strerror(errno));
			exit(1);
		}
//	}
	invalidExitNap = 0;
	for(index=0; index<(int)strlen(argv[2]); index++)
	{
		if(!isdigit(argv[2][index]))
		{
			gaVarLog(mod, 0, "Invalid exitNap (%s). Using default (%d)\n", 
				argv[2], SOCKET_ERROR_NAP);
			invalidExitNap = 1;
		}
	}
	if(invalidExitNap)
	{
		exitNap = SOCKET_ERROR_NAP;
	}
	else
	{
		exitNap = atoi(argv[2]);
	}

	signal(SIGTERM, proc_sigterm);

	if (checkProcessStatus(argv[0]) < 0)
	{
		gaVarLog(mod, 0, "Process %s is already running",argv[0]);
		exit(0);
	}
	if(deleteIpc(shmKey) < 0)
	{
		exit (1);		/* Message written in sub-routine */
	}
	if(createIpc(shmKey) < 0)
	{
		exit (1);		/* Message written in sub-routine */
	}
	if(loadShmem() < 0)
	{
		exit (1);		/* Message written in sub-routine */
	}

	/* set death of child function */
	sig_act.sa_handler = NULL;
	sig_act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
	if (sigaction(SIGCHLD, &sig_act, &sig_oact) != 0)
	{
		gaVarLog(mod, 0, "sigaction(SIGCHLD,SA_NOCLDWAIT) failed, errno=%d (%s)"
		       	,errno,strerror(errno));
		exit(1);
	} 

//	signal(SIGCHLD, proc_sigchild);
//#if 0
	if(signal(SIGCHLD, SIG_IGN) == SIG_ERR || sigset(SIGCLD, SIG_IGN) == -1)
	{
		gaVarLog(mod, 0, 
		       "sigaction(SIGCHLD,SIG_IGN) failed, errno=%d (%s)",errno,strerror(errno));
		exit(1);
	} 
//#endif

	if(initSocketConnection() < 0)
	{
		exit(1);		/* Message written in subroutine */
	}

/* Main Loop, listening for connection requests,
   accepting call, and performing the services. */

	for (;;)					/* main loop */
	{
		gaVarLog(mod, 1, "Waiting for client connection request ...");

		free_slot = -1;
		clnt_len  = sizeof(clnt_addr);
		if((recfd = accept(listen_fd,(struct sockaddr *)&clnt_addr,
							(int *)&clnt_len)) < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			gaVarLog(mod, 0, "accept() failed. [%d, %s]", errno,strerror(errno));
			continue;
		}
		gaVarLog(mod, 1, "Got client request...");

		checkApps();

		memset(str_fd, 0, sizeof(str_fd));
		memset(recv_buf, 0, sizeof(recv_buf));

		if (recvApplicationName(recfd, recv_buf) < 0)
		{
			gaVarLog(mod, 0, "Failed to get application name from client");
			close(recfd);
			continue;
		}

		/* get free slot in shared memory for new application */
		if((free_slot = getShmemSlot()) < 0)
		{
			close(recfd);
			continue;
		}

		memset(program_name, 	0, sizeof(program_name));	
		memset(client_pid, 	0, sizeof(client_pid));	
		memset(client_machine, 	0, sizeof(client_machine));	

		ptr = recv_buf;
		if ( (ptr2 = (char *)strtok(ptr, "|\n")) == NULL) 
		{
			gaVarLog(mod, 0, 
				"%s|%d|strtok(%s) failed. returned NULL; unable to process.", __FILE__, __LINE__, recv_buf);
			close(recfd);
			continue;
		}
		sprintf(program_name, 	"%s", ptr2);

		if ( (ptr2 = (char *)strtok(NULL, "|\n")) == NULL) 
		{
			gaVarLog(mod, 0, 
				"%s|%d|strtok(NULL) failed. returned NULL; unable to process.", __FILE__, __LINE__);
			close(recfd);
			continue;
		}
		sprintf(client_pid,	"%s", ptr2);

		if ( (ptr2 = (char *)strtok(NULL, "|\n")) == NULL) 
		{
			gaVarLog(mod, 0, 
				"%s|%d|strtok(NULL) failed. returned NULL; unable to process.", __FILE__, __LINE__);
			close(recfd);
			continue;
		}
		sprintf(client_machine,	"%s", ptr2);

		if(client_pid[0] == '\0')
		{
			sprintf(client_pid, "%s", "0");
		}
		if(client_machine[0] == '\0')
		{
			sprintf(client_machine, "%s", "Unknown");
		}
		if(program_name[0] == '\0')
		{
			gaVarLog(mod, 0, "NULL program name passed by client%c",NET_EOF);

			written = write(recfd,__log_buf, strlen(__log_buf));
			if (written != strlen(__log_buf))
			{
				gaVarLog(mod, 0, "Unable to send message to client :%s", __log_buf);
			}
			close(recfd);
			continue;
		}

		/* update application information in shared memory */
		if (updateShmem(program_name, free_slot) < 0)
		{
			close(recfd);
			continue;
		}

		sprintf(str_fd, "%d", recfd);
		sprintf(shm_index, "%d", free_slot);
		switch (mem_map[free_slot].pid=fork())
		{
			case -1 :			/* fork failed */
				gaVarLog(mod, 0, "ERROR: Failed to fork app %s, errno=%d %c",
						program_name, errno, NET_EOF);
				
				written = write(recfd,__log_buf,	
							strlen(__log_buf));
				if (written != strlen(__log_buf))
				{
					gaVarLog(mod, 0, "Unable to send message to client :%s", __log_buf);
				}
				break;

			case 0 :				/* child */
				close(listen_fd);
				/* send ok message to client */

				memset(message, 0, sizeof(message));
				sprintf(message, "%s%c", "OK", NET_EOF);
				gaVarLog(mod, 1, "Sending OK message (%s) to client", message);
				written = write(recfd,message, strlen(message));
				if (written != strlen(message))
				{
					gaVarLog(mod, 0, "Unable to send message to client :%s", message);
				}
				sleep(1);
				strcat(absolutePathName, program_name);

				ret = execl(absolutePathName, program_name, 
						"-P", program_name, 
						"-p", shm_index, 
						"-s", str_fd,
						"-I", shm_index,
						"-m", client_machine,
						"-i", client_pid,
						(char *) 0);
				if(ret == -1)
				{
					gaVarLog(mod, 0, "Failed to exec app %s errno=%d (%s)",
							program_name,errno, strerror(errno));
					continue;
				}
				break;

			default :	/* Parent */
				gaVarLog(mod, 1, "Child PID = %d", mem_map[free_slot].pid);
				sprintf(str_pid, "%d", mem_map[free_slot].pid);
				/* update shared memory for application exec */
				if(write_fld(mem_map[free_slot].ptr, free_slot,
							APPL_PID, str_pid) < 0)
				{
					gaVarLog(mod, 0, "write_fld() failed");
				}
				close(recfd);			/* parent */
				break;
		} /* switch */

		/* No need to do this twice */
		/* checkApps(); */
	} /* forever */
} /* END: main() */
/*----------------------------------------------------------------------------
recvApplicationName():	This routine receives the data from the client.
----------------------------------------------------------------------------*/
int recvApplicationName(int fd, char *app)
{
	static char mod[]="recvApplicationName";
	int 	done = 0, where = 0, nbytes = 0;
	char	buf[MAXDATA], sizeStr[5], sizeBuf[10];
	guint32	nBytesRead, bytesToRead;

	memset(buf, 0, sizeof(buf));
	memset(sizeStr, 0, sizeof(sizeStr));

	where = 0;
	while ( ! done )
	{
		if((nbytes = read(fd, &sizeStr[where], 1)) < 0)
		{
			gaVarLog(mod, 0, "Failed to receive app. name size from client");
			return(-1);
		}

		if ( sizeStr[where] == '~' )
		{
			sizeStr[where] = '\0';
			done = 1; 
			continue;
		}
		where++;

		if ( where >= sizeof(sizeStr))
		{
			gaVarLog(mod, 0, 
				"Failed to receive app. name size from client; have not received termchar for size of data.");
			return(-1);
		}
	}
	gaVarLog(mod, 1, "recvApplicationName: sizeStr = (%s)", sizeStr);

	where = 0;
	done = 0;
	nBytesRead = 0;
	do
	{
		bytesToRead = atol(sizeStr) - where;

		if((nbytes = read(fd, &buf[where], bytesToRead)) < 0)
		{
			if (errno == EINTR)
				continue;
			if (errno == ECONNRESET)
			{
				gaVarLog(mod, 1, "Connection closed by client.");
				return (-2);
			}
			gaVarLog(mod, 0, "Unable to receive app. name, retcode=%d. [%d, %s]", 
							nbytes, errno, strerror(errno));
			return (-1);
		}
		nBytesRead += nbytes;
		where += nbytes;
		if (nbytes == 0)	
		{
			done = 1;
			continue;
		}
		if(atoi(sizeStr) == where)
		{
			done = 1;
		}

	} while(!done);

	/* send confirmation message to client   */
	sprintf(sizeBuf, "%ld~", nBytesRead);
	if(write(recfd, sizeBuf, sizeof(guint32)) < 0)
	{
		gaVarLog(mod, 0, "Failed to write confirmation to client.");
		return(-1);
	}
	gaVarLog(mod, 1, "Received Application Name from Server (%s).", buf);

	strcpy(app, buf);
	return(0);
}  /* recvApplicationName() */
/*--------------------------------------------------------------------------
updateShmem(): This routine check the apllication length name and update
		   the information in shared memory.
---------------------------------------------------------------------------*/
int updateShmem(char *app, int shmem_idx)
{
	static char mod[]="updateShmem";
	int	written;
	char	err_str1[80], err_str2[80], err_str3[80];

	sprintf(err_str1, "Server app. (%s) should be < 40 chars %c", 
								app, NET_EOF);
	sprintf(err_str2, "Server app. >%s< not found %c",app,NET_EOF);
	sprintf(err_str3, "No resources available to run server app >%s< %c",
								app,NET_EOF);
		  
	if ((int)strlen(app) >=40 )
	{
		gaVarLog(mod, 0, err_str1);
		/* send error message to client */
		written = write(recfd, err_str1, strlen(err_str1));
		if (written != strlen(err_str1))
		{
			gaVarLog(mod, 0, "Unable to send msg (%s) to client.",
								err_str1);
		}
		return (-1);
	}
	if (access(app, F_OK|X_OK) != 0)
	{
		gaVarLog(mod, 0, "Cannot access app. (%s), [%d, %s]",
						app,errno,strerror(errno));

		/* send error message to client */
		written = write(recfd, err_str2, strlen(err_str2));
		if (written != strlen(err_str2))
		{
			gaVarLog(mod, 0, "Unable to send msg (%s) to client.",
								err_str2);
		}
		return (-1);
	}

	/* update shared memory for application */
	if (shmem_idx < 0)
	{
		gaVarLog(mod, 0, "Invalid shmem index (%d).", shmem_idx);

		/* send error message to client */
		written = write(recfd, err_str3, strlen(err_str3));
		if (written != strlen(err_str3))
		{
			gaVarLog(mod, 0, "Unable to send msg (%s) to client", err_str3);
		}
		return (-1);
	}

	if(write_fld(mem_map[shmem_idx].ptr, shmem_idx, APPL_STATUS, INIT) < 0)
	{
		gaVarLog(mod, 0, 
		     "Failed to update app. status in shmem slot %d, app(%s).",
		     shmem_idx, app);

		/* send error message to client   */
		written = write(recfd, err_str3, strlen(err_str3));
		if (written != strlen(err_str3))
		{
			gaVarLog(mod, 0, "Unable to send msg (%s) to client", err_str3);
		}
		return(-1);
	}
	if(write_fld(mem_map[shmem_idx].ptr, shmem_idx, APPL_NAME, app) < 0)
	{
		gaVarLog(mod, 0, "Failed to update app. name (%s) in shmem slot %d.", 
			app, shmem_idx);

		/* send error message to client  */
		written = write(recfd, err_str3, strlen(err_str3));
		if (written != strlen(err_str3))
		{
			gaVarLog(mod, 0, "Unable to send msg (%s) to client", err_str3);
		}
	return (-1);
	}
	gaVarLog(mod, 1, "Updated app. name & status in shmem for (%s).",app);
	return (0);
} /* updateShmem */
/*------------------------------------------------------------------------------
This routine creates the shared memory segments and Message Queues 
------------------------------------------------------------------------------*/
int createIpc(key_t shmKey)
{
	static char mod[]="createIpc";
	tran_shmid = shmget(shmKey, SIZE_SCHD_MEM_WSS, PERMS);
	if(tran_shmid >= 0)
	{
		shmctl(tran_shmid, IPC_RMID, 0);
	}

	if((tran_shmid = shmget(shmKey, SIZE_SCHD_MEM_WSS, 
					PERMS|IPC_CREAT|IPC_EXCL)) < 0)
	{
		gaVarLog(mod, 0, "Unable to get shmem id. [%d, %s]",
						errno, strerror(errno));
		return (-1);
	}

	gaVarLog(mod, 1, "Shared memory and message queue created ...");
	return (0);
} /* createIpc() */
/*------------------------------------------------------------------------------
 this routine removes the shared memory & message queues 
------------------------------------------------------------------------------*/
int deleteIpc(key_t shmKey)
{
	static char mod[]="deleteIpc";
	int 	ret_code;

	tran_shmid = shmget(shmKey, SIZE_SCHD_MEM_WSS, PERMS);
					/* get ids of shared memory segments */
	ret_code = shmctl(tran_shmid, IPC_RMID, 0);
	if((ret_code < 0) && (errno != 22))
	{
		gaVarLog(mod, 0, "Failed to remove shmem id. [%d, %s]",
						errno, strerror(errno));
		exit(1);
	}
	return(0);
} /* deleteIpc() */
/*--------------------------------------------------------------------------
loadShmem()
--------------------------------------------------------------------------*/
int loadShmem()
{
	static char mod[]="loadShmem";
	register	int	i;
	char	*curr_ptr;
	char	buf[SHMEM_REC_LENGTH+1];

	memset(buf, 0, sizeof(buf));
	strcpy(buf, SHMEM_REC);

	__tran_tabl_ptr = shmat(tran_shmid, 0, 0);
	if(__tran_tabl_ptr == NULL)
	{
		gaVarLog(mod, 0, "shmat failed. [%d, %s]", errno,strerror(errno));
		return(-1);
	}
	curr_ptr = __tran_tabl_ptr;

	for(i=0; i<MAX_PROCESS; i++)
	{
		mem_map[i].pid = -1;
		strncpy(curr_ptr, buf, (int)strlen(buf));
		mem_map[i].ptr = curr_ptr;
		curr_ptr += strlen(buf);

		gaVarLog(mod, 1, "buf = %s", mem_map[i].ptr);
	}
	return (0);
} /* loadShmem() */
/*---------------------------------------------------------------------------
getShmemSlot(): this routine returns the index of free slot in shared memory
----------------------------------------------------------------------------*/
int getShmemSlot()
{
	static char mod[]="getShmemSlot";
	register	int	i;
	int	written;
	char	err_str[256];

	sprintf(err_str, "Too many connections, max=%d%c", MAX_PROCESS,NET_EOF);

	for(i=0; i<MAX_PROCESS; i++)
	{
		if (mem_map[i].pid == -1)		/* -1 free slot */
		{
			gaVarLog(mod, 1, 
				"getShmemSlot(): return empty slot = %d", i);
			return (i);
		} /* if */
	} /* for */

	gaVarLog(mod, 0, "%s", err_str);

	/* send error message to client */
	written = write(recfd, err_str, strlen(err_str));
	if (written != strlen(err_str))
	{
		gaVarLog(mod, 0, "Unable to send msg (%s) to client. [%d, %s]", 
			err_str, errno, strerror(errno));
	}

	return (-1);
} /* getShmemSlot() */
/*--------------------------------------------------------------------------
checkProcessStatus(): Check if work station server already running.
---------------------------------------------------------------------------*/
int checkProcessStatus(char *server_name)
{
	static char mod[]="checkProcessStatus";
	FILE	*fin;
	register int	i;
	char	buf[128];
	char	ps[256]	= "ps -ef";   

	sprintf(ps,
		"ps -ef | grep %s | grep -v grep | grep -v \"\\-ksh\"", server_name);

	if((fin = popen(ps, "r")) == NULL)
	{
		gaVarLog(mod, 0, "popen(%s) failed. [%d, %s]", errno,strerror(errno));
		return (-1);
	}

	fgets(buf, sizeof buf, fin);	/* strip of the header */
	i = 0;
	while (fgets(buf, sizeof buf, fin) != NULL)
	{
		if(strstr(buf, server_name) != NULL)	/* process found */
		{
			i = i + 1;
			if(i >= 2)
			{
				return (-1);
			}
		} /* if */
	} /* while */

	pclose(fin);

	return(0);
} /* checkProcessStatus() */
/*-----------------------------------------------------------------------------
checkApps():This routine check for all application dead and does cleanup.
-----------------------------------------------------------------------------*/
checkApps()
{
	static char mod[]="checkApps";
	register	int	appl_no;
	char	pid_str[10];
	int	int_pid;

	for(appl_no=0; appl_no<MAX_PROCESS; appl_no++)
	{
		read_fld(__tran_tabl_ptr, appl_no, APPL_PID, pid_str);    

		int_pid = atoi(pid_str);

		if (kill((long)int_pid, 0) == -1)   /* check if app. exists */
		{
			if (errno == ESRCH) /* No process */
			{
				/* application cleanup function */
				(void) cleanup_app(int_pid);
			}
		}
	} /* for */
	return;
} /* checkApps */
/*----------------------------------------------------------------------------
This routine updates an field in the shared memory tables with the given value.
------------------------------------------------------------------------------*/
int write_fld(char *ptr, int rec, int ind, char *stor_arry)
{
	static char mod[]="write_fld";
	char    tmp[60];
	char    *schd_tabl_ptr;

	if (rec < 0 || ind < 0 )
        {
			gaVarLog(mod, 0, 
						"write_fld(ptr,%d,%d,%s): Invalid value/not supported"
						,rec,ind,stor_arry);
        	return (-1);
        }
	schd_tabl_ptr = ptr;
	switch(ind)
        {
        	case APPL_API:               /* current api # */
        	case APPL_START_TIME:        /* application start time */
        	case APPL_NAME:              /* application name */
        	case APPL_STATUS:            /* status of the application */
        	case APPL_PID:               /* pid */
        	case APPL_SIGNAL:            /* signal to the application */
                	/* position the pointer to the field */
                	ptr += vec[ind-1];
                	sprintf(tmp, "%*s", fld_siz[ind-1], stor_arry); 
                	memcpy(ptr, tmp, strlen(tmp));
                	break;
		
        	default:
				gaVarLog(mod, 0, 
						"write_fld(ptr,%d,%d,%s): Invalid value/not supported"
						,rec,ind,stor_arry);
                break;
        } /* switch */
	ptr = schd_tabl_ptr;

	return(0);
} /* END: write_fld */
/*-----------------------------------------------------------------------------
This routine returns the value of the desired field from the shared memory.
-----------------------------------------------------------------------------*/
int read_fld(char *ptr, int rec, int ind, char *stor_arry)
{
	static char mod[]="read_fld";
	char	*schd_tabl_ptr;

	if (rec < 0 || ind < 0)
	{
			gaVarLog(mod, 0, "read_fld(ptr,%d,%d,output): Invalid value/not supported",rec,ind);
        	return (-1);
	}
	schd_tabl_ptr = ptr;
	switch(ind)
	{
		case APPL_START_TIME:	/* application start time */
		case APPL_RESOURCE:	/* application # */
		case APPL_API:		/* current api running */
		case APPL_FIELD5:	/* reserve name */
		case APPL_NAME:		/* application name */
		case APPL_STATUS:	/* status of the application */
		case APPL_PID:		/* proc_id of the application */
		case APPL_SIGNAL:	/* signal to the application */

			ptr += (rec*SHMEM_REC_LENGTH);
			/* position the pointer to the field */
			ptr += vec[ind-1];
			sscanf(ptr, "%s", stor_arry); /* read the field */
			break;

		default	:
			gaVarLog(mod, 0, "read_fld(ptr,%d,%d,output): Invalid value/not supported" ,rec,ind);
			return(-1);
	} /* switch */
	ptr = schd_tabl_ptr;

	return (0);
} /* read_fld */
/*-----------------------------------------------------------------------------
getPort():
-----------------------------------------------------------------------------*/
int getPort(char *servType, int *serverPort)
{
	static char mod[]="getPort";
#ifndef SERVER_PORT
	*serverPort = 6789;
#else	
	*serverPort = SERVER_PORT;
#endif

	gaVarLog(mod, 1, "getPort: serverPort=%d", *serverPort);

	return(0);
} /* getPort() */
/*-----------------------------------------------------------------------------
initSocketConnection():
-----------------------------------------------------------------------------*/
int initSocketConnection()
{
	static char mod[]="initSocketConnection";
	static int	firstTime = 0;
	struct 	sockaddr_in serv_addr;
	struct protoent	*p;
	int	protocol;

	if(firstTime == 0)
	{
		if((p = getprotobyname("TCP")) == NULL)
		{
			gaVarLog(mod, 0, "getprotobyname failed. [%d, %s]", errno, strerror(errno));
			exit(1);
		}
		protocol = p->p_proto;
		if((listen_fd = socket(AF_INET, SOCK_STREAM, protocol)) < 0)
		{
			gaVarLog(mod, 0, "socket failed, errno=%d (%s)", errno,strerror(errno));
			sleep(exitNap);
			exit (1);
		}

		if(getPort(servType, &serverPort) < 0)
		{
			gaVarLog(mod, 0, "Failed to get port");
			sleep(exitNap);
			exit(1);
		}

		memset((void *)&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family	   = AF_INET;
		serv_addr.sin_addr.s_addr  = htonl(INADDR_ANY);
		serv_addr.sin_port	   = htons(serverPort);
	
		if(bind(listen_fd,(struct sockaddr *)&serv_addr, 
							sizeof(serv_addr)) < 0)
		{
			gaVarLog(mod, 0, "bind failed. [%d, %s]", 
						errno,strerror(errno));
			sleep(exitNap);
			exit (1);
		}
		firstTime = 1;
	}
	/* set up socket for listening , with a queue length 9 */
	if (listen(listen_fd, 9) < 0)
	{
		gaVarLog(mod, 0, "listen failed. [%d, %s]", errno,strerror(errno));
		close(listen_fd);
		sleep(exitNap);
		exit (1);
	}

	return(0);
} /* END: initSocketConnection() */
