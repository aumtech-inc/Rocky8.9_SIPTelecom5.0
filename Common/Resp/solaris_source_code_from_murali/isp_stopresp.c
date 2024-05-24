/*----------------------------------------------------------------------------
Program:	isp_stopresp
Purpose: 	isp_stopresp takes the time in seconds as a command line 
		argument, to shut down ISP applications.
		It first kills all idle applications, then after waiting
		for the required number of seconds (passed as a command line
		argument), it kills the rest of the applications.
 
Author:		Mahesh Joshi.
Date:		12/05/95
Update:		04/03/97 sja	Changed WKS to WSS
Update:		03/23/98 gpw	Updated this header.
Update:		03/25/98 gpw	Added code to write the ports off file.
Update:		04/14/98 gpw	Took write_ports_off_file_entry out of loop.
Update:		01/14/98 gpw	Took write_ports_off_file_entry out of loop.
Update:		02/17/99 mpb	changed strcmp == NULL to 0.
Update:	2000/10/07 sja 4 Linux: Added function prototypes
----------------------------------------------------------------------------*/
#include "ISPSYS.h"
#include "BNS.h"
#include "resp.h"
#include "shm_def.h"
#include "resp_msg.h"
#include "ports_off.h"

#define	TEL_RESPONSIBILITY	"isp_telresp"
#define TCP_RESPONSIBILITY	"isp_tcpresp"
#define SNA_RESPONSIBILITY	"isp_snaresp"

static	long	que_no;
static	long	dyn_mgr_que=0L;
static	key_t	shm_key;
static	char	*ps	= "ps -ef";   
static	int	tot_application = 0;
static  char 	ports_off_pathname[256];
/* remove shared memory and message Queue */
static	int	ret_code;
static	char	appl_name[100];

static	void	kill_all_remaining_appl();
static	int 	kill_resp();
static	void 	shm_attach();
/* extern	void	LogMsg(char *modulename, int mode, char *str); */
static	void 	remove_shm_que();
static int write_ports_off_file_entry(int port_no, char *ports_off_pathname);

/*------------------------------------------------------------------------------
Kill the application in shared memory, that are still running.
------------------------------------------------------------------------------*/
void kill_all_remaining_appl()
{
static	char	ModuleName[]="kill_all_remaining_appl";
int	i;
char	field[7];

alarm(0);
LogMsg(ModuleName, ISP_DEBUG_DETAIL, "Shutting down all remaining application");
for(i=0; i < tot_application; i++)
	{
	read_fld(tran_tabl_ptr, i, APPL_STATUS, field);
	if(atoi(field) != STATUS_KILL)
		{
		read_fld(tran_tabl_ptr, i, APPL_PID, field);
			/* get the process id from the schd_tran_table */
		if(atoi(field) != 0)
			{
			read_fld(tran_tabl_ptr, i, APPL_NAME, appl_name);
			if (atoi(field) == 0)
				continue;
			ret_code = kill(atoi(field), SIGKILL);
			/* kill the process by sending SIGKILL signal */
			if(ret_code != 0)
				{
				sprintf(log_buf, ISP_EKILL, appl_name,atoi(field), errno);
				LogMsg(ModuleName, ISP_DEBUG_DETAIL, log_buf);
				}
			else
				{
				sprintf(log_buf, "Application %s (SIGKILL) terminated, pid: %d", appl_name, atoi(field));
				LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
				}
			}
		write_fld(tran_tabl_ptr, i, APPL_STATUS, STATUS_KILL);
		/* update the shared memory */
		}
	}
LogMsg(ModuleName, ISP_DEBUG_NORMAL, "Responsibility Shutdown completed.");
(void) remove_shm_que();
exit (0);
} /* kill_all_remaining_appl */

int main(argc, argv)
int	argc;
char	*argv[];
{
static	char	ModuleName[]="main";
int	i, first_time=1;
char	field[20];
char	*ispbase;

if(argc < 2)
	{
	(void)printf("Usage: %s <Object> <wait time in seconds>\n", argv[0]);
	exit(0);
	}
if (strcmp(argv[1], TELECOM_STR) == 0)
	{
	object = TEL;
	sprintf(object_name, "%s", "Telecom");
	sprintf(resp_name, "%s", TEL_RESPONSIBILITY);
	}
else if (strcmp(argv[1], SNA_STR) == 0)
	{
	object = SNA;
	sprintf(object_name, "%s", "SNA");
	sprintf(resp_name, "%s", SNA_RESPONSIBILITY);
	}
else if (strcmp(argv[1], TCP_STR) == 0)
	{
	object = TCP;
	sprintf(object_name, "%s", "TCP");
	sprintf(resp_name, "%s", TCP_RESPONSIBILITY);
	}
else
	{
	fprintf(stderr, "Object type %s not supported : \n", argv[1]);
	sprintf(log_buf, ISP_EOBJ, argv[1]);
	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	exit (1);
	}

sprintf(log_buf, "Shutdown %s Server started.", object_name);
LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
(void) signal(SIGALRM, kill_all_remaining_appl);	/* set the signal */

/* proc_id = getpid(); */

if (argc == 3)				/* set alarm if specified */
	(void) alarm(atoi(argv[2]));	/* set the alarm for specified time */
else
	(void) alarm(DEFAULT_ALARM);
ret_code = kill_resp(object);		/* kill the responsibility */
if(ret_code == -1)
	exit(0);

(void) shm_attach(object);		/* attach the shared memory tables */


if (object == TEL) 
{
	/* Delete the .ports_off file */
        if ((ispbase = getenv("ISPBASE")) == NULL)
        {
                sprintf(log_buf,"Cannot read ISPBASE environment variable");
                LogMsg(ModuleName, ISP_DEBUG_DETAIL, log_buf);
        }
	else
	{
		sprintf(ports_off_pathname, "%s/%s/%s", 
			ispbase, PORTS_OFF_DIR,	PORTS_OFF_FILE);
		unlink(ports_off_pathname);
	}
}

if (object == TEL)
	for (i=0; i<tot_application; i++)
	{
		read_fld(tran_tabl_ptr, i, APPL_STATUS, field);
		if (atoi(field) == STATUS_CHNOFF)
		     write_ports_off_file_entry(i, ports_off_pathname);
	}

for(;;)
	{
	for(i=0; i<tot_application; i++)
		{			/* Kill applications */
		read_fld(tran_tabl_ptr, i, APPL_STATUS, field);
		if(atoi(field) == STATUS_IDLE || atoi(field) == STATUS_CHNOFF)
			{	/* If the process is Idle */
			read_fld(tran_tabl_ptr, i, APPL_PID, field);
			if(atoi(field) != 0)
				{
				read_fld(tran_tabl_ptr, i, APPL_NAME,appl_name);
				ret_code = kill(atoi(field), SIGTERM);
					/* kill the process */
				if(ret_code != 0)
					{
					sprintf(log_buf, ISP_EKILL, appl_name, atoi(field), errno);
					LogMsg(ModuleName, ISP_DEBUG_DETAIL, log_buf);
					}
				else
					{
					sprintf(log_buf, "Application %s terminated, pid: %d\n", appl_name, atoi(field));
					LogMsg(ModuleName, ISP_DEBUG_DETAIL, log_buf);
					}
				}
			if (kill(atoi(field), 0) != 0)	/* process not exists */
				write_fld(tran_tabl_ptr, i, APPL_STATUS, STATUS_KILL);
			/* Update the status byte to KILL */
			} /* if application idle */
		else
			{
			/* for SNA send sigterm to all application */
			/* this allow application to logoff */
			if ((object == SNA || object == TCP) && (first_time == 1))
				{
				read_fld(tran_tabl_ptr, i, APPL_PID, field);
				if(atoi(field) != 0)
					{
					read_fld(tran_tabl_ptr, i, APPL_NAME,appl_name);
					ret_code = kill(atoi(field), SIGTERM);
						/* kill the process */
					if(ret_code != 0)
						{
						sprintf(log_buf, ISP_EKILL, appl_name, atoi(field), errno);
						LogMsg(ModuleName, ISP_DEBUG_DETAIL, log_buf);
						}
					else
						{
						sprintf(log_buf, "Application %s terminated, pid: %d\n", appl_name, atoi(field));
						LogMsg(ModuleName, ISP_DEBUG_DETAIL, log_buf);
						}
					}
				}
			}
		} /* for all applications */
	first_time = 0;
	} /* forever */
} /* main */
/*------------------------------------------------------------------------------
Kill Responsibility services.
------------------------------------------------------------------------------*/
int kill_resp()
{
static	char	ModuleName[]="kill_resp";
FILE	*fin;			/* file pointer for the ps pipe */
int	tmp_pid;
char	buf[BUFSIZE];
char	tmp[20];

if((fin = popen(ps, "r")) == NULL)
	{		/* open the process table */
	sprintf(log_buf, ISP_EOPEN_PIPE, "Can't verify responsibility running.", "ps -ef", errno);
	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return(-1);
	}
(void) fgets(buf, sizeof buf, fin); 	/* strip of the header */
/* get the responsibility */
while (fgets(buf, sizeof buf, fin) != NULL)
	{
	if(strstr(buf, resp_name) !=  0)
		{
		(void) sscanf(buf, "%s%d", tmp, &tmp_pid);
		ret_code = kill(tmp_pid, SIGTERM);	/* Kill the resp */
		if(ret_code != 0)
			{
			sprintf(log_buf, ISP_EKILL, resp_name, tmp_pid, errno);
			LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
			return(-1);
			}
		else
			{
			sprintf(log_buf, "Responsibility subsystem terminated, pid: %d", tmp_pid);
			LogMsg(ModuleName, ISP_DEBUG_DETAIL, log_buf);
			return (0);
			}
		}
	}
pclose(fin);
sprintf(log_buf, "Responsibility for %s server not running.", object_name);
LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
return (0);
} /* kill_resp */
/*------------------------------------------------------------------------------
Attach shared memory.
------------------------------------------------------------------------------*/
void shm_attach(obj)
int	obj;				/* object */
{
static	char	ModuleName[]="shm_attach";

/* set shared memory key and queue */
switch(obj)
	{
	case	TEL:
		shm_key = SHMKEY_TEL;
		que_no  = TEL_RESP_MQUE;
		dyn_mgr_que = TEL_DYN_MGR_MQUE;
		break;
	case	WSS:
		shm_key = SHMKEY_WSS;
		que_no  = WSS_RESP_MQUE;
		dyn_mgr_que = WSS_DYN_MGR_MQUE;
		break;
	case	SNA:
		shm_key = SHMKEY_SNA;
		que_no  = SNA_REQS_MQUE;
		break;
	case	TCP:
		shm_key = SHMKEY_TCP;
		que_no  = TCP_REQS_MQUE;
		break;
	default:
		sprintf(log_buf, ISP_EOBJ_NUM, obj);	
		LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
		exit (1);
	} /* switch */

if((tran_shmid = shmget(shm_key, SIZE_SCHD_MEM, 0))<0)
	{			 /* Get the key for the memory */
	(void)printf("Unable to get shared memory segment key = %ld error =  %d\n", (long)shm_key, errno);
	sprintf(log_buf, ISP_ESHMID, shm_key, errno);
	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	exit(1);
	}
					
tran_tabl_ptr = shmat(tran_shmid, 0, 0); /* attach the shared memory segment */
if(tran_tabl_ptr == (char *) -1) 
	{
	(void) printf("Unable to attach to shared memory key = %ld, errno =  %d\n", (long)shm_key, errno);
	sprintf(log_buf, ISP_ESHM_AT, shm_key, errno);
	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	exit(1);
	}
if(tran_tabl_ptr != (char *) -1)
	tot_application = (strlen(tran_tabl_ptr)/SHMEM_REC_LENGTH);	
				/* Total # of records in schd_tabl */
return;
} /* shm_attach() */
/*------------------------------------------------------------------------------
This routine removes the shared memory & message queues 
------------------------------------------------------------------------------*/
void remove_shm_que()
{
static	char	ModuleName[]="remove_shm_que";

/* get message id for the response queue */
resp_mqid = msgget(que_no, PERMS);
/* remove response queue */
ret_code = msgctl(resp_mqid, IPC_RMID, 0);	
if((ret_code < 0) && (errno != 22))
	{
	sprintf(log_buf, ISP_EMSGQ_RM, que_no, errno);
	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	}
if(dyn_mgr_que)
	{
	resp_mqid = msgget(dyn_mgr_que, PERMS);
	/* get message id for the dyn_mgr queue */
	ret_code = msgctl(resp_mqid, IPC_RMID, 0);	
	/* remove dyn_mgr queue */
	if((ret_code < 0) && (errno != 22))
		{
		sprintf(log_buf, ISP_EMSGQ_RM, dyn_mgr_que, errno);
		LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
		}
	}
/* get ids of shared memory segments */
tran_shmid = shmget(shm_key, SIZE_SCHD_MEM, PERMS);
ret_code = shmctl(tran_shmid, IPC_RMID, 0);
if((ret_code < 0) && (errno != 22))
	{
	sprintf(log_buf, ISP_ESHM_RM, shm_key, errno);
	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	}
return;
} /* remove_shm_que */

/*----------------------------------------------------------------------------
This function writes an entry into the "ports_off" file.  When it is called for
the first time it writes a header into the file, and appends a single line
with the port number; thereafter it just writes the single line. 
----------------------------------------------------------------------------*/
static int write_ports_off_file_entry(int port_no, char *ports_off_pathname)
{
#define LINE_1 "# Ports that were turned off when Telecom Services was last stopped"

	static	char	ModuleName[]="write_ports_off_file";
	FILE *fp;
	static int first_time =1;
	long time_date;
	char time_str[80];
	char day_index_str[10];
	int  day_index;
	struct tm	*pTime;
        static char *const wday[] = {
                "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "???" };
 
	if ( first_time )
	{
		first_time = 0;
		time(&time_date);
		pTime = localtime(&time_date);
		strftime(day_index_str, sizeof(day_index_str), "%w", pTime);
		day_index=atoi(day_index_str);
		strftime(time_str, sizeof(time_str), "%y/%m/%d at about %H:%M", pTime);
		if ((fp=fopen(ports_off_pathname, "w")) == NULL)
       		{
			sprintf(log_buf,
			"Unable to open %s for writing. errno=%d", errno);
			LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
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
			sprintf(log_buf,
			"Unable to open %s for appending. errno=%d", errno);
			LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
			return(-1);
       		}
	}
	fprintf(fp, "%d\n", port_no);
	fclose(fp);
}
