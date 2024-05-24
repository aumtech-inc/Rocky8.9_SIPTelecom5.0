/*----------------------------------------------------------------------------
Program:	ArcIPStopResp
Purpose: 	Take the time in seconds as a command line argument, to shut 
		down IP Responsibility and all the IP Telecom applications.
		It first kills all idle applications, then after waiting
		for the required number of seconds (passed as a command line
		argument), it kills the rest of the applications.
 
Author:		George Wenzel 
Date:		12/04/00
Update: 08/05/2002	mpb In killAppRemainingApps Sending SIGTERM first 
				and then SIGKILL.
Update: 08/05/2002	mpb Added timeToWait variable. 
Update: 04/24/2007	mpb MR#1844. Checking to see if it is dynamic manager in
			killAllRemainingApps routine. 
Update: 04/25/2007	mpb MR#1844. Checking to see if it is dynamic manager in
			killInactiveApps routine.
----------------------------------------------------------------------------*/
#include "ISPSYS.h"
#include "BNS.h"
#include "shm_def.h"
#include "ArcIPRespMsg.h" 
#include "ports_off.h"
#include "ArcIPResp.h"
#include <unistd.h>

#define	TEL_RESPONSIBILITY	"ArcIPResp"
#define	TEL_DYNMGR		"ArcIPDynMgr"
#define	JUST_ONE_INSTANCE	0
#define	ALL_INSTANCES		1
#define	RESP_BASE		100 /* ?? This needs to be changed. */

static	long	que_no;
static	long	dyn_mgr_que=0L;
static	key_t	shm_key;
static	int	tot_application = 0;
int	timeToWait = 5;

/* Function prototypes */
static	void 	setAlarmForKillingApps(int argc, int time_specified);
static	int	killAllRemainingApps();
static	void	finalCleanup();
static	int 	killProcessByName(char *processName, int allInstances);
static  int	shm_attach(int *tot_application);
static	void 	remove_shm_que();
static int write_ports_off_file_entry(int port_no, char *ports_off_pathname);
static void killInactiveApps(int tot_application);
static int Log(char *mod, int mode, int msgId, int msgType, char *msgFormat, ...);

int determineObjectType(char *arg, char *object_name, char *resp_name);
int updatePortsOffFile(int tot_application);
extern int write_fld(char *ptr, int rec, int ind, int);
extern int read_fld(char *ptr, int rec, int ind, char *stor_arry);
extern int LogARCMsg(char *, int , char *, char *, char *, int ,  char *);
extern int util_sleep(int Seconds, int Milliseconds);


int main(int argc, char *argv[])
{
	static	char	mod[]="main";
	int	i, rc;
	char 	resp_name[128];
	char	record[1024];
	FILE	*fp;
	int respPid = -1;
	char    ispBase[]="ISPBASE";
	char    *ispbase;
	char    yStrPidFile[256];

	if ((ispbase = getenv(ispBase)) == NULL)
	{
		Log(mod, REPORT_NORMAL, RES_EENV, ERR, RES_EENV_MSG, ispBase);
	}

	if(argc < 2)
	{
		printf("Usage: %s <Service> <grace period in seconds>\n", 
								argv[0]);
		exit(0);
	}

	rc=determineObjectType(argv[1], object_name, resp_name);
	if (rc != 0) exit(1);

	Log(mod, REPORT_DETAIL, RESP_BASE, INFO,
			"Shutdown of %s Server started.", object_name);

	if (argc == 3)				/* set alarm if specified */
		timeToWait = atoi(argv[2]);

	setAlarmForKillingApps(argc, atoi(argv[2]));

	sprintf(yStrPidFile, "%s/Telecom/Exec/ArcIPResp.pid", ispbase);

	/* open ArcIPResp.pid file and kill responsibility from the pid */
	if((fp=fopen(yStrPidFile, "r")) == NULL)
	{
		Log(mod, REPORT_NORMAL, RESP_BASE, ERR,
			"Can't open ArcIPresp.pid, [%d, %s].  Unable to attempt to kill ArcIPResp.",
			errno, strerror(errno));
	}
	else
	{
		while(fgets(record, sizeof(record), fp) != NULL)
		{
			respPid = atoi(record);
			rc = kill(respPid, SIGTERM);
			Log(mod, REPORT_VERBOSE, RESP_BASE, INFO,
					"Successfully terminated ArcIPResp. pid=%d.",
			 		respPid);

			sleep(2);

		}
	}

	sleep(10);

	/* make sure it is killed 	*/
	for(i = 0; i < 4; i++)
	{

		rc = killProcessByName(resp_name, ALL_INSTANCES);	
		if(rc == 1)
			break;
		sleep(1);
#if 0
		if(rc == -1) 
			exit(0);
#endif
	}

	/* Attach shared memory table & get total # of apps. */
	rc= shm_attach(&tot_application);
	if(rc != 0) 
	{
		killProcessByName(TEL_DYNMGR, ALL_INSTANCES);
		exit(-1);
	}

	Log(mod, REPORT_VERBOSE, RESP_BASE, INFO,
		"Before updatePortsOffFile.");
	updatePortsOffFile(tot_application);

	Log(mod, REPORT_VERBOSE, RESP_BASE, INFO,
		"Before killInactiveApps.");
	killInactiveApps(tot_application);

	Log(mod, REPORT_VERBOSE, RESP_BASE, INFO,
		"Before killProcessByName.");
	killProcessByName(TEL_DYNMGR, ALL_INSTANCES);

	exit(0);

} /* main */

/*----------------------------------------------------------------------------
This function kills runs in a continuous loop killing all "inactive 
applications". Eventually it will be interrupted when the alarm goes off 
and another function will kill all the remaining apps, regardless of their 
status.
----------------------------------------------------------------------------*/
static void killInactiveApps(int tot_application)
{
	int i, rc;
	static char mod[]="killInactiveApps";
	char	appl_name[100];
	char field[7];

	while ( 1 )
	{
		for(i=0; i<tot_application; i++)
		{		
			read_fld(tran_tabl_ptr, i, APPL_STATUS, field);
			if(atoi(field) == STATUS_IDLE || 
				atoi(field) == STATUS_CHNOFF)
			{
				read_fld(tran_tabl_ptr, i, APPL_PID, field);
				if(atoi(field) != 0)
				{
					read_fld(tran_tabl_ptr, i, APPL_NAME,appl_name);
					if (strncmp(appl_name, "ArcIP", 5) == 0)
						continue;
					rc = kill(atoi(field), SIGTERM);
					if(rc != 0)
					{
						Log(mod, REPORT_DETAIL, 
						RES_EKILL, WARN, RES_EKILL_MSG, 
						appl_name, atoi(field), errno);
					}
					else
					{
					Log(mod, REPORT_DETAIL, RESP_BASE, WARN,
					"Application %s terminated. pid=%d.", 
					appl_name, atoi(field));
					}
				}
				/* Update status to KILL if app is dead. */
				util_sleep(0, 10);
				if (atoi(field) > 0)
				{
					if (kill(atoi(field), 0) != 0)	
						write_fld(tran_tabl_ptr, i, 
						APPL_STATUS, STATUS_KILL);
				}
			} /* if application idle */
		} /* for i= */
		util_sleep(0, 250);
	} /* while */
}

/*----------------------------------------------------------------------------
Verify that the object type passed was acceptable & set parameters based on it.
Return -1 if not supported.
----------------------------------------------------------------------------*/
int determineObjectType(char *arg, char *object_name, char *resp_name)
{
	char mod[]="determineObjectType";

	if (strcmp(arg, TELECOM_STR) == 0)
	{
		sprintf(object_name, "%s", "Telecom");
		sprintf(resp_name, "%s", TEL_RESPONSIBILITY);
		return(0);
	}
	else
	{
		Log(mod, REPORT_NORMAL, RESP_BASE, ERR,
			"Object type (%s) is not supported.", arg);
		return(-1);
	}
}
/*------------------------------------------------------------------------------
Set the alarm to go off according to the grace period value passed as a
command line argument. If no value was specified, use the default. 
------------------------------------------------------------------------------*/
static void setAlarmForKillingApps(int argc, int time_specified)
{
	signal(SIGALRM, finalCleanup);	

	if (argc == 3)
		alarm(time_specified);
	else
		alarm(5);
}

/*------------------------------------------------------------------------------
This function will kill a process (or all processes) of the given name. 
------------------------------------------------------------------------------*/
static int killProcessByName(char *processName, int killAllInstances)
{
	static	char	mod[]="killProcessByName";
	int 	rc;
	FILE	*fin;	
	int	tmp_pid;
	char	buf[BUFSIZE];
	char	tmp[20];
	int 	foundAnInstance=0;

	Log(mod, REPORT_VERBOSE, RESP_BASE, ERR,
		"Calling killProcessByName with name=(%s)", processName);

	fin = popen("ps -ef", "r");
	if (fin == NULL)
	{
		Log(mod, REPORT_NORMAL, RESP_BASE, ERR,
			"Failed to list processes looking for (%s). errno=[%d, %s]",
			 processName, errno, strerror(errno));
		return(-1);
	}

	/* Discard the header line */
	fgets(buf, sizeof buf, fin); 

	while (fgets(buf, sizeof buf, fin) != NULL)
	{
		if(strstr(buf, processName) !=  0)
		{
			sscanf(buf, "%s%d", tmp, &tmp_pid);
			rc = kill(tmp_pid, SIGTERM);
			if (rc != 0)
			{
				Log(mod, REPORT_NORMAL, RESP_BASE, ERR,
				"Failed to kill (%s). pid=%d. [%d, %s].",
			 		processName, tmp_pid, errno, strerror(errno));
				return(-1);
			}
			else
			{
				foundAnInstance=1;
				Log(mod, REPORT_DETAIL, RESP_BASE, INFO,
					"Successfully terminated (%s). pid=%d.",
			 		processName, tmp_pid);
				if (!killAllInstances) return (0);
			}
		}
	}
	pclose(fin);
	if (!foundAnInstance)
	{
		Log(mod, REPORT_DETAIL, RESP_BASE, WARN,
			"No instances of (%s) found running.", processName);
		return(1);
	}
	return (0);
} 
/*------------------------------------------------------------------------------
Attach shared memory and determine the total number of possible applications.
------------------------------------------------------------------------------*/
int shm_attach(int *numApplications)
{
	static char mod[]="shm_attach";

	shm_key = SHMKEY_TEL;
	que_no  = TEL_RESP_MQUE;
	dyn_mgr_que = TEL_DYN_MGR_MQUE;

	/* Get the key for the memory */
	tran_shmid = shmget(shm_key, SIZE_SCHD_MEM, 0);
	if( tran_shmid <0)
	{	
		Log(mod, REPORT_NORMAL, RES_ESHMID, ERR, 
			RES_ESHMID_MSG, (long)shm_key, errno);
		return(-1);
	}
					
	/* Attach the shared memory segment */
	tran_tabl_ptr=shmat(tran_shmid, 0, 0); 
	if(tran_tabl_ptr == (char *) -1) 
	{
		Log(mod, REPORT_NORMAL, RES_ESHMID, ERR, 
				RES_ESHM_AT_MSG, shm_key, errno);
		return(-1);
	}

	/* Determine total # of records. */
	if(tran_tabl_ptr != (char *) -1)
		*numApplications = (strlen(tran_tabl_ptr)/SHMEM_REC_LENGTH);	
	return(0);
} 

/*------------------------------------------------------------------------------
This routine removes the shared memory & message queues 
------------------------------------------------------------------------------*/
void remove_shm_que()
{
	static	char mod[]="remove_shm_que";
	int rc;

	/* Get message id for the response queue */
	resp_mqid = msgget(que_no, PERMS);

	/* remove response queue */
	rc = msgctl(resp_mqid, IPC_RMID, 0);	
	if((rc < 0) && (errno != 22))
	{
		Log(mod, REPORT_NORMAL, RES_EMSGQ_RM, ERR,
			RES_EMSGQ_RM_MSG, que_no, errno);
	}

	if(dyn_mgr_que)
	{
		resp_mqid = msgget(dyn_mgr_que, PERMS);
		/* Get message id for the dynamic manager queue */
		rc = msgctl(resp_mqid, IPC_RMID, 0);	
		/* remove dyn_mgr queue */
		if((rc < 0) && (errno != 22))
		{
			Log(mod, REPORT_NORMAL, RES_EMSGQ_RM, ERR,
					RES_EMSGQ_RM_MSG, dyn_mgr_que, errno);
		}
	}

	/* Get ids of shared memory segments */
	tran_shmid = shmget(SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);
	rc = shmctl(tran_shmid, IPC_RMID, 0);

	Log(mod, REPORT_VERBOSE, RESP_BASE, INFO, "Marked shm key (%d) id(%d) for removal. rc=%d", shm_key, tran_shmid, rc);

	if((rc < 0) && (errno != 22))
	{
		Log(mod, REPORT_NORMAL, RES_ESHM_RM, ERR,
				RES_ESHM_RM_MSG, shm_key, errno);
	}
	return;
} 

/*----------------------------------------------------------------------------
This function writes an entry into the "ports_off" file.  When it is called for
the first time it writes a header into the file, and appends a single line
with the port number; thereafter it just writes the single line. 
----------------------------------------------------------------------------*/
static int write_ports_off_file_entry(int port_no, char *ports_off_pathname)
{
#define LINE_1 "# Ports that were turned off when Telecom Services was last stopped"

	static	char	mod[]="write_ports_off_file";
	FILE *fp;
	static int first_time =1;
	long time_date;
	char time_str[80];
	char day_index_str[10];
	int  day_index;
	struct tm	*PT_time;
    static char *const wday[] = {
                "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "???" };
 
	if ( first_time )
	{
		first_time = 0;
		time(&time_date);
		PT_time = localtime(&time_date);
		strftime(day_index_str, sizeof(day_index_str),"%w", PT_time);
		day_index = atoi(day_index_str);
		strftime(time_str, sizeof(time_str),
				"%y/%m/%d at about %H:%M", PT_time);
		if ((fp=fopen(ports_off_pathname, "w")) == NULL)
       		{
			Log(mod, REPORT_NORMAL, RESP_BASE, ERR,
				"Failed to open (%s) for writing. [%d, %s].  "
				"Unable to write off ports to file.", 
				ports_off_pathname, errno, strerror(errno));
			return(-1);
       		}
		fprintf(fp, "# %s\n", ports_off_pathname);
		fprintf(fp, "%s\n", LINE_1);
		fprintf(fp, "# on %s %s\n", wday[day_index], time_str);
	}
	else
	{
		if ((fp=fopen(ports_off_pathname, "a")) == NULL)
       		{
			Log(mod, REPORT_NORMAL, RESP_BASE, ERR,
			"Failed to open (%s) for appending. [%d, %s].  "
				"Unable to write off ports to file.", 
				ports_off_pathname, errno, strerror(errno));
			return(-1);
       		}
	}
	fprintf(fp, "%d\n", port_no);
	fclose(fp);

	return(0);
}

/*------------------------------------------------------------------------------
Kill the applications that are still running. 
This function gets executed as a result the alarm expiring on the grace period. 
------------------------------------------------------------------------------*/
int killAllRemainingApps()
{
	static	char	mod[]="killAllRemainingApps";
	int	i, try; 
	int	maxTries=2;  /* Number of tries to kill all the apps. */
	char	field[7];
	char	appl_name[100];

	alarm(0);
	Log(mod, REPORT_DETAIL, RESP_BASE, INFO,
		"Shutting down all remaining applications.");

	for (try = 0; try < maxTries; try++)
	{
		for(i=0; i < tot_application; i++)
		{
			read_fld(tran_tabl_ptr, i, APPL_STATUS, field);
			if(atoi(field) != STATUS_KILL)
			{
				read_fld(tran_tabl_ptr, i, APPL_PID, field);
				/* get process id from the schd_tran_table */
				if(atoi(field) != 0)
				{
					read_fld(tran_tabl_ptr, i, APPL_NAME, appl_name);
					if (atoi(field) == 0)
						continue;
					if (strncmp(appl_name, "ArcIP", 5) == 0)
						continue;
					if(try == 0)
						ret_code = kill(atoi(field), SIGTERM);
					else
					{
						if(kill(atoi(field),0) != 0)/* process does not exist */
							;
						else
							ret_code = kill(atoi(field), SIGKILL);
					}
					/* send SIGKILL to kill the process */
					if(ret_code != 0)
					{
						Log(mod, REPORT_DETAIL, RESP_BASE, WARN, RES_EKILL_MSG, 
						appl_name, atoi(field), errno);
					}
					else
					{
						if(try == 0)
						{
							Log(mod, REPORT_DETAIL, RESP_BASE, WARN,
				"Application (%s) (SIGTERM) terminated. pid=%d.", 
							appl_name, atoi(field));
						}
						else
						{
						Log(mod, REPORT_DETAIL, RESP_BASE, WARN,
			"Application (%s) (SIGKILL) terminated. pid=%d.", 
						appl_name, atoi(field));
						}
					}
				}
			/*	write_fld(tran_tabl_ptr, i, APPL_STATUS, STATUS_KILL); */
				/* update the shared memory */
			}
		} /* for i */
		sleep(timeToWait);
	} /* for try */
	Log(mod, REPORT_NORMAL, RESP_BASE, INFO,
		"IP Telecom Services shutdown completed.");
	return(0);
} 

/* ----------------------------------------------------------------------------
Update the .ports_off file.
 ----------------------------------------------------------------------------*/
int updatePortsOffFile(int tot_application)
{
	char 	mod[]="updatePortsOffFile";
	char 	ports_off_pathname[256];
	char 	field[7];
	char 	*ispbase;
	char	ispBase[]="ISPBASE";
	int i;

    if ((ispbase = getenv(ispBase)) == NULL)
    {
		Log(mod, REPORT_NORMAL, RES_EENV, ERR, RES_EENV_MSG, ispBase);
    }
	else
	{
		sprintf(ports_off_pathname, "%s/%s/%s", 
			ispbase, PORTS_OFF_DIR,	PORTS_OFF_FILE);
		unlink(ports_off_pathname);
	}

	for (i=0; i<tot_application; i++)
	{
		read_fld(tran_tabl_ptr, i, APPL_STATUS, field);
		if (atoi(field) == STATUS_CHNOFF)
		     write_ports_off_file_entry(i, ports_off_pathname);
	}

	return(0);
}


/*-----------------------------------------------------------------------------
Performs the final cleanup after the grace period expires. This consists of:
1) Killing all remaining applications regardless of their state.
2) Killing the IP dynamic manager, including all it's threads.
3) Removing the shared memory queue. 
4) Exiting the program.
-----------------------------------------------------------------------------*/
static	void	finalCleanup()
{
	int rc;

	Log("finalCleanUp", REPORT_VERBOSE, RESP_BASE, INFO, "In finalCleanUp");
	rc=killAllRemainingApps();
	Log("finalCleanUp", REPORT_VERBOSE, RESP_BASE, INFO, "About to kill dynamic manager");
	rc=killProcessByName(TEL_DYNMGR, ALL_INSTANCES);
	remove_shm_que();
	exit(0);
}

static int Log(char *mod, int mode, int msgId, int msgType, char *msgFormat, ...)
{
        va_list ap;
        char m[1024];
        char type[16];
        int rc;

        switch(msgType)
        {
        case 0:         strcpy(type,"ERR: "); break;
        case 1:         strcpy(type,"WRN: "); break;
        case 2:         strcpy(type,"INF: "); break;
        default:        strcpy(type,""); break;
        }

        memset(m, '\0', sizeof(m));
        va_start(ap, msgFormat);
        vsprintf(m, msgFormat, ap);
        va_end(ap);

        LogARCMsg(mod, mode, "0", "RES", "ArcIPStopResp", msgId, m);
        return(0);
}

