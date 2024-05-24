/*-----------------------------------------------------------------------------
Program:	isp_baseresp.c
Purpose: This is general purpose scheduler to schedule TCP/IP Services 
		application, SNA Services application.
Author:		Madhav Bhide.
Date created :	10/17/01
Update:	10/23/2013 D. Narsay Added SSH/SSH2 support
--------------------------------------------------------------------------------
Files		: BNS.h, ISPSYS.h, shm_def.h, resp.h, resp_msg.h, support.c
Modules Called	: see prototype section.
Libararies Used	: support.c  : read_fld(), write_fld().
------------------------------------------------------------------------------*/
#include "ISPSYS.h"			/* Shared memory/Message Queue id */
#include "BNS.h"			/* shared memory variables */
#include "resp.h"			/* global variables definations */
//#include "shm_def.h"			/* shared memory field information */
//#include "arc_snmpdefs.h"		/* ARC SNMP group & object defines */
/*
#define	DEBUG_TABLES
#define DEBUG_RULES
#define DEBUG
*/
#define REPORT_NORMAL 1
#define REPORT_DETAIL 128
#define REPORT_VERBOSE 2
/* external function prototype */
extern  int write_arry();
extern	int write_fld();
extern	int read_arry();
extern	int read_fld();

/* local function prototype */
static	void	resp_shutdown();
static	void	cleanup_app(int, int);
static	void	init_array();
static	void	update_shmem();
static	int 	check_environment(int);
static	int 	set_object_path(int);
static	int 	check_configuration_tables(int);
static	int 	fill_vecs();
static	int	launch_applications(int, char *, int[], int[]);
static	int	check_responsibility();
static	int	clean_msgs_for_dead_appl();
static	int	load_tables();
static	int	load_shmem_tabl();
static	int	load_resource_table();
static	int	load_schedule_table();
static	int	load_appref_table();
static	int	load_pgmref_table();
static	int	find_application();
static	int	check_data_token();
static	int	check_date_time_rule();
static	int	check_update_table_request();
static	int	get_field();
static	int	remove_msgs();
static	int	mesg_recv();
static	int 	application_instance_manager();
static	int 	set_resource_state_flag();
static	int	update_application();
static	int	find_pu_name();
static	void 	load_parameter_configuration();
static	int	kill_all_app();
static	void 	check_kill_app();
static	int	start_netserv();
static	void	check_net_config();
static int arcValidLicense( char *feature_requested, int turnkey_allowed, int  *turnkey_found, int  *feature_count, char *msg);
static int get_license_file_name( char *license_file, char *err_msg);

static  int     Licensed_Resources, Turnkey_License;

/*------------------------------------------------------------------------------
This routine is called when the application gets killed/terminated. This module updates the variable indicating the death of the application.
------------------------------------------------------------------------------*/
void cleanup_app(app_pid, appl_no)
{
int	stat, i, j;
char	program_name[MAX_PROGRAM];
static	char	mod[] = "cleanup_app";
char	res_id[30];
char	field_name[30];

sprintf(log_buf,"Entering cleanup_app, app_pid %d, appl_no %d",app_pid,appl_no);
resp_log(mod, REPORT_DETAIL, 3020, log_buf);

pid_dead = app_pid;

if(pid_dead == appl_pid[appl_no])
	{		/* found it */
	appl_stat_arry[appl_no] = 0;	/* update the field to indicate */
 					/* so that it can be re-started */
 	for(j = 0; j < MAX_PROCS; j++)
 		{      
 		if(dead_application[j] == -1)
 			{
      			dead_application[j] = pid_dead;
 		  		/* store the application pid */
			break;
 			}
 		}
	/* read program name and resource */
	(void)read_fld(tran_tabl_ptr, appl_no, APPL_NAME, program_name);     
	(void)read_fld(tran_tabl_ptr, appl_no, APPL_RESOURCE, res_id);     
	(void)application_instance_manager(ISP_DELETE_INSTANCE,program_name,res_id);
	(void)write_fld(tran_tabl_ptr, appl_no, APPL_STATUS, STATUS_KILL);
	(void)write_fld(tran_tabl_ptr, appl_no, APPL_API, 0); 
	(void)write_fld(tran_tabl_ptr, appl_no, APPL_PID, 0); 
	}
return;
} /* cleanup_app */

int main(int argc, char	*argv[])
{
static	char	mod[] = "main";
int	i, j;
int	tran_sig[MAX_PROCS];	/* (field 10) to store the signal status */
int	tran_status[MAX_PROCS];	/* to store the status of application */
time_t	hbeat_time;
time_t	curr_time;			/* to store the current time */
time_t	que_time;			/* to store the queue time */
time_t	appl_off_time;			/* to store the hang test time */
int	hbeat_interval=0;
char	off_appl_name[512];
char	off_res_name[30];
char	last_api_name[30];
struct	sigaction	sig_act, sig_oact;
int rc;

if(argc == 2 && (strcmp(argv[1], "-v") == 0))
{
	fprintf(stdout, "%s - %s - %s\n", argv[0], __DATE__, __TIME__);
	exit(0);
}

/* set heartbeat seconds and set object name */
switch(argc)
	{
	case	3:
		if((atoi(argv[2]) > ISP_MIN_HBEAT) && (atoi(argv[2]) < ISP_MAX_HBEAT))
			{
			hbeat_interval = (atoi(argv[2])) * 60;  /* secs */
			}
		else
			{
			hbeat_interval = ISP_DEFAULT_HBEAT * 60; /* default */
			sprintf(log_buf, "WARNING : kill time should be  %d < kill time < %d, Setting kill time to default = %d Min.",  ISP_MIN_HBEAT, ISP_MAX_HBEAT, ISP_DEFAULT_HBEAT);
			resp_log(mod, REPORT_NORMAL, 3001, log_buf);
			}
		/* please don't put break here */
	case	2:
		if (strcmp(argv[1], SNA_STR) == 0)
			{
			object = SNA;
			}
		else if (strcmp(argv[1], TCP_STR) == 0)
			{
			object = TCP;
			}
		else
			{
			fprintf(stderr, "Server type %s not supported : Usage: %s <SNA/TCP> [<kill time min>]\n", argv[1], argv[0]);
			sprintf(log_buf, "Server type %s not supported : Usage: %s <SNA/TCP> [<kill time min>]", argv[1], argv[0]);
			resp_log(mod, REPORT_NORMAL, 3002, log_buf);
			exit (1);
			}
		break;
	default :
		fprintf(stderr, "Usage : %s <Object Name> <kill time interval>\n",argv[0]);
		sprintf(log_buf,"Usage: %s <Object Name> <kill time interval>",argv[0]);
		resp_log(mod, REPORT_NORMAL, 3007, log_buf);
		exit (1);
	} /* switch */

rc = arcValidLicense(argv[1],1,&Turnkey_License,&Licensed_Resources,log_buf);
if (rc != 0)
{
        fprintf(stderr, "%s: LICENSE FAILURE. %s\n", argv[1], log_buf);
		resp_log(mod, REPORT_NORMAL, 3008, "License failure. See nohup.out.");
        exit(0);
}
sprintf(resp_name, "%s", argv[0]);
if (Turnkey_License)
        sprintf(log_buf,
        "Starting Responsibility for %s server in turnkey mode...", argv[1]);
else
	sprintf(log_buf, "Starting Responsibility for %s server...", argv[1]);
resp_log(mod, REPORT_NORMAL, 3021, log_buf);

if (check_responsibility() != ISP_SUCCESS)	/* check if object is running */
	exit (1);

sprintf(object_code, "%s", argv[1]);
sprintf(log_buf, "Heartbeat interval is %d", hbeat_interval);
resp_log(mod, REPORT_VERBOSE, 3004, log_buf);

/* check environment for given object */
if (check_environment(object) != ISP_SUCCESS)
	exit (1);

/* set directory structure for object */
if (set_object_path(object) != ISP_SUCCESS)
	exit (1);

/* check if all configuration file for the given object exists */
if (check_configuration_tables(object) != ISP_SUCCESS)
	exit (1);

/* initialize the table to maintain application terminated pid's and 
dynamic manager application pid array */
init_array();

/* Remove any existing shared memory segment. */
if(removeSharedMem(object) != ISP_SUCCESS)
	exit (1);

/* Create required shared memory segment.  */
if(createSharedMem(object) != ISP_SUCCESS)
	exit (1);

/* set shutdown function */
(void) signal(SIGTERM, resp_shutdown);
/* set death of child function */
sig_act.sa_handler = NULL;
sig_act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
if (sigaction(SIGCHLD, &sig_act, &sig_oact) != 0)
	{
	sprintf(log_buf, 
	"sigaction(SIGCHLD, SA_NOCLDWAIT): system call failed. errno=%d.", errno);
	resp_log(mod, REPORT_NORMAL, 3051, log_buf);
	exit(-1);
	} 
if (signal(SIGCHLD, SIG_IGN) == SIG_ERR || sigset(SIGCLD, SIG_IGN) == -1)
	{
	sprintf(log_buf, 
	"signal(SIGCHLD, SIG_IGN): system call failed. errno=%d.", errno);
	resp_log(mod, REPORT_NORMAL, 3006, log_buf);
	exit(-1);
	} 

/* attach shared memory */
if ((tran_tabl_ptr = shmat(tran_shmid, 0, 0)) == (char *) -1)
	{			 /* attach the shared memory segment */
	sprintf(log_buf,
	"Can't attach shared memory segment, errno = %d", GV_shm_key, errno);
	resp_log(mod, REPORT_NORMAL, 3003, log_buf);
	exit(-1);
	}

(void) check_update_table_request();	
/* this will store the time file last modified */

/* Load all configuration tables into memory */
if (load_tables(schedule_tabl, resource_tabl, appref_tabl, pgmref_tabl) != ISP_SUCCESS)
	{
	exit (1);
	}
/* start network services if configured */
check_net_config();
if (start_net_serv == 1)	/* network services exists */
	{
	start_netserv();
	}

/* Load the resource and scheduling tables into the shared memory segments */
if (load_shmem_tabl(tran_shmid) != ISP_SUCCESS)
	exit (1);

tran_proc_num = strlen(tran_tabl_ptr)/SHMEM_REC_LENGTH;

for(i=0; i<tran_proc_num; i++)	/* initialize the status vector */
	appl_stat_arry[i] = 0;  /* to indicate no application started */

/* Update the status bit fields in the shared memory */
(void)write_arry(tran_tabl_ptr, APPL_STATUS, appl_stat_arry);

/* This routine reads fields from shared memory table into given variables. */
(void) fill_vecs(tran_tabl_ptr, appl_pid);
					/* reads pid from shared memory */

/* initialize all start time process */
for(i=0; i < MAX_PROCS; i++)	
	count[i] = 0;

/* start application(s) */	
(void) launch_applications(tran_proc_num, tran_tabl_ptr, 
			appl_stat_arry, appl_pid);

if (time(&hbeat_time) == -1)
	{
	sprintf(log_buf, "unable to get system time errno = %d", errno);
	resp_log(mod, REPORT_NORMAL, 3022, log_buf);
	}
que_time = hbeat_time;
appl_off_time = hbeat_time;
	
for(;;)						/* the watchdog */
	{
	/* following routine check for all dead application and update info */
	(void) check_kill_app();

	/* This routine update pid fields from shared memory to array */
	(void) fill_vecs(tran_tabl_ptr, appl_pid);

	/* update the shared memory for application newly schedule or change */
	update_shmem();
	/* start application(s).  */
	(void) launch_applications(tran_proc_num, tran_tabl_ptr,
			appl_stat_arry, appl_pid);

	if (time(&curr_time) == -1) 		/* get the system time */
		{
		sprintf(log_buf, "unable to get system time errno = %d", errno);
		resp_log(mod, REPORT_NORMAL, 3022, log_buf);
		}
	/* Message Queue cleanup for application being terminated */
	if((curr_time-que_time) >= MQUE_CLEANUP_TIME)
		{
		(void) clean_msgs_for_dead_appl();
		que_time = curr_time;
		}
	if((curr_time-appl_off_time) >= MAX_FIRE_LAPS_SEC)
		{
		for(j=0; j < tran_proc_num; j++)
			{
			if(count[j] > MAX_NUM_APPL_FIRE) 
				{
				(void)write_fld(tran_tabl_ptr, j, APPL_STATUS, STATUS_OFF);
				(void)read_fld(tran_tabl_ptr, j, APPL_NAME, off_appl_name);     
				sprintf(log_buf, "Application <%s> (%d) started %d times in %d seconds. Spawning too rapidly.", off_appl_name, j, count[j], MAX_FIRE_LAPS_SEC);
				resp_log(mod, REPORT_NORMAL, 3009, log_buf);
				/* Application stopped */
				}
			count[j] = 0; 
			}
		appl_off_time = curr_time;
		}
	/* after every hbeat_interval seconds, check application health */
	if((int)(curr_time-hbeat_time) >= hbeat_interval)
		{		
		/* Initialize the signal vector */
		(void)read_arry(tran_tabl_ptr, APPL_SIGNAL, tran_sig);
		(void)read_arry(tran_tabl_ptr, APPL_STATUS, tran_status);
		for(i=0; i<tran_proc_num; i++)
			{
			if(tran_sig[i] == 0)
				{
				if((tran_status[i] != STATUS_IDLE)&&(tran_status[i] != STATUS_OFF)&&(tran_status[i] != STATUS_CHNOFF))
					{	
					if(appl_pid[i] != 0)
						{
						(void) kill((pid_t)appl_pid[i], SIGTERM);
						(void)read_fld(tran_tabl_ptr, i, APPL_RESOURCE, off_res_name);     
						(void)read_fld(tran_tabl_ptr,i,APPL_NAME,off_appl_name);
						last_api_name[0] = '\0';
						(void)read_fld(tran_tabl_ptr,i,APPL_API,last_api_name);
						sprintf(log_buf,"No heartbeat from application <%s>, Resource = <%s>, pid=%d in %d seconds. It appears to be hung. Application terminated. Last activity = %s", off_appl_name, off_res_name, appl_pid[i], hbeat_interval, last_api_name); 
						resp_log(mod, REPORT_NORMAL, 3010, log_buf);
						/* no activity for hang test */
						} /* pid != 0 */
					} /* status not idle and off */
				}
			} /* for all applications */
		/* reset sig */
		for(i=0; i<tran_proc_num; i++)
			tran_sig[i] = 0;

		hbeat_time = curr_time;

		/* update the signal vector into the shared memory */
		(void)write_arry(tran_tabl_ptr, APPL_SIGNAL, tran_sig);
		} /* hang_test */

	if(Turnkey_License == 1)
	{
		checkValidTurnkeyApp();
	}

	/* check for update configuration table request */
	database_hit = 0;
	if (check_update_table_request() == 0)	/* request for table updation */
		{
		resp_log(mod, REPORT_NORMAL, 3024, "____ Reloading the database ____");
		if (load_tables(schedule_tabl, resource_tabl, appref_tabl, pgmref_tabl) != ISP_SUCCESS)
			{
			exit (0);  /* decision making exit */
			}

		(void)read_arry(tran_tabl_ptr, APPL_STATUS, tran_status);
		resp_log(mod, REPORT_NORMAL, 3025, "Reloading the database done.");
		database_hit = 1;
		} /* check_update_table_request */
	(void) sleep(2);		/* sleep time */
	} /* watchdog */
} /* main */
/*------------------------------------------------------------------------------
 this routine reads the vectors from the shared memory 
------------------------------------------------------------------------------*/
int fill_vecs(prv_ptr, pid)
char	*prv_ptr;
int	pid[];
{
(void)read_arry(prv_ptr, APPL_PID, pid);		/* read the proc_id s */
return (ISP_SUCCESS);
} /* fill_vecs */
/*------------------------------------------------------------------------------
 This routine launch the application based on the information in shared memory
 and scheduling table. if the value for the corresponding element is 0 
------------------------------------------------------------------------------*/
int launch_applications(proc_num, mem_ptr, stat_arry, pid)
int	proc_num;			/* # of processes */
char	*mem_ptr;			/* pointer to the shared mem */
int	stat_arry[];			/* process status array */
int	pid[];				/* proc_ids */
{
int	i, k, resource_found = 0, ret=0, t, dir_index=0;
char	*pvt_ptr, *ptr;		/* temp storage of shared memory  */
char	appl_name[256];		/* Name of the application */
char	dir_name[256];		/* Name of the directory */
char	program_name[256];	/* Name of the program */
char	reco_no[30];		/* record # in the shared memory */
char	host_name[20];		/* Host name as in shared memory */
char	field4[20];		/* field reserve for future  */
char	field5[20];		/* field reserve for future */
char	appl[100];		/* table name as in the shared memory  */
char	field6[25];		/* field reserve for future */
char	Emu_no[20];		/* emulator name as in shared memory  */
char	resource_id[30];	/* resource name in shared memory */
int     stat_str[MAX_PROCS];
char	isdn_flag[10];
char	state[10];
char	shm_index[20];
struct	schedule record;
static	char	mod[] = "launch_applications";
time_t	start_time;
char	disconnect_res[20];

state[0] = isdn_flag[0] = shm_index[0] = '\0';
memset(&record, 0, sizeof(struct schedule));

pvt_ptr = mem_ptr;		/* save the shared memory pointer */
(void)read_arry(tran_tabl_ptr, APPL_STATUS, stat_str);

for(i=0; i<proc_num; i++)
	{
	if(!stat_arry[i])
		{
		stat_arry[i] = 1;
				/* set the process status to 1 */
		if(pid[i])	/* Clear the messages if process gets killed */
			{
			if (kill(pid[i], 0) == -1)/* remove messages only if pid is dead */
				(void) remove_msgs(pid[i]); 
			else
				{
				mem_ptr += SHMEM_REC_LENGTH;	/* goto next record */
				continue;
				}
			}

		(void) time(&num_of_secs);
		sscanf(mem_ptr, "%s%s%s%s%s%s%s%*d%*d%*d\n", host_name, Emu_no,
		 resource_id, field4, field5, field6, appl);
					/* read the shared memory */
		(void) strcpy(appl_name, appl);
		sprintf(reco_no, "%d", i);
				/* create the record # */
		if (strcmp(appl_name, "N/A") == 0 || strcmp(appl_name, RESERVE_PORT) == 0)	/* application off */
			{
			(void)write_fld(tran_tabl_ptr, i, APPL_STATUS, STATUS_OFF);
			mem_ptr += SHMEM_REC_LENGTH;	/* position the ptr */
			continue;
			}
		ret_code = access(appl_name, R_OK | X_OK);
		if(ret_code != 0)
			{
			/* make status off */
			(void)write_fld(tran_tabl_ptr, i, APPL_STATUS, STATUS_OFF);
			sprintf(log_buf,
					"Can't start application %s, errno=%d",appl_name,errno);
    		resp_log(mod, REPORT_NORMAL, 3165, log_buf);
			mem_ptr += SHMEM_REC_LENGTH;	/* goto next record */
			continue;
			}
		resource_found = ISP_NOTFOUND;
		if(stat_str[i] != STATUS_OFF)	/* if status is not stop */
			{
			/* find resource parameters in resource table */
			for (k = 0; k < tot_resources; k++)
				{
				if (strcmp(resource[k].res_no, resource_id) == 0)
					{
					resource_found = ISP_FOUND;
					break;
					}
				} /* find resource */
			if (resource_found == ISP_FOUND)
				{
				if (object == TCP || object == SNA)
					{
					/* load application information */
					/* check_data_token for analog only */
					if (strcmp(resource[k].res_type, "LU2") == 0 || 
						strcmp(resource[k].res_type, "TELNET") == 0 || 
						strcmp(resource[k].res_type, "SSH") == 0 || 
						strcmp(resource[k].res_type, "SSH2") == 0 || 
						strcmp(resource[k].res_type, "RS232") == 0 )
						{
						if (check_data_token(TOKEN1_APPL, resource[k].static_dest, "", &record) != ISP_SUCCESS)
							{
							/* goto next record */
							mem_ptr += SHMEM_REC_LENGTH;
							continue;
							}
						if (application_instance_manager(ISP_CHECK_MAX_INSTANCE, record.program, resource_id) != ISP_SUCCESS)
							{
							/* goto next record */
							mem_ptr += SHMEM_REC_LENGTH;
							continue;
							}
						}
					} /* for Telecom object */
				/* for resource state staic/dynamic */
				(void)set_resource_state_flag(resource[k].res_state, state);
				} /* resource found */
			else
				{
				printf("before exec : Can't find resource (%s) in resource table\n", resource_id);
				mem_ptr += SHMEM_REC_LENGTH;/*position the ptr to next record */
				continue;
				}
			/* shared memory index */
			sprintf(shm_index, "%d", i);
			count[i] += 1;
			time(&start_time);
			t = start_time;
			program_name[0] = '\0';
			dir_name[0] = '\0';
			/* set directory and program name */
			if (strchr(appl_name, '/') == NULL)
				{
				sprintf(program_name, "%s", appl_name);
				sprintf(dir_name, "%s", ".");
				}
			else
				{ /* parse program name and directory name */
				sprintf(dir_name, "%s", appl_name);
				dir_index = strlen(dir_name) -1;
				for(;dir_index>=0; dir_index--)
        				{
        				if (dir_name[dir_index] == '/')
                			{
                			dir_name[dir_index] = '\0';
                			break;
                			}
        				}
				ptr = (char *) strrchr(appl_name, '/');
				if (ptr)
        			ptr++;
				sprintf(program_name, "%s", ptr);
				}
			if (dir_name[0] == '/')
				{ /* full path */
				sprintf(appl_path, "%s/%s", dir_name, program_name);
				}
			else
				{ /* relative path */
				if (strcmp(dir_name, ".") == 0) /*current dir*/
					sprintf(appl_path, "%s/%s", exec_path, program_name);
				else
					sprintf(appl_path, "%s/%s/%s", exec_path, dir_name, program_name);
				}
			sprintf(disconnect_res, "%d", res_attach[atoi(resource_id)]);
			(void)write_fld(tran_tabl_ptr, i, APPL_STATUS, STATUS_INIT);
			if(Turnkey_License == 1)
				write_fld(tran_tabl_ptr, i, APPL_FIELD5, 1);
			
			if((pid[i] = fork()) == 0)
				{ /* load configuration parameter change directory */
				if (chdir(dir_name) != 0)
					{
					sprintf(log_buf, "chdir(%s) system call failed, errno = %d", dir_name, errno);
					resp_log(mod, REPORT_NORMAL, 3026, log_buf);
					}
				switch (object)
				{
				case	TCP:
					ret = execl(appl_path, program_name, 
					"-P", program_name, 
					"-y", record.acct_code,
					"-p", resource_id, 
					/* resource interface name */
					"-t", resource[k].res_type,
					/* host name */
					"-H", resource[k].res_usage,
					"-s", state,
					"-h", heartbeat_interval, 
					"-I", shm_index,
					(char *) 0);
					break;
				case	SNA:
					ret = execl(appl_path, program_name, 
					/* resource PU name */
					"-L", resource[k].res_qualifier,
					"-P", program_name, 
					"-y", record.acct_code,
					"-p", resource_id, 
					"-s", state,
					"-h", heartbeat_interval, 
					"-I", shm_index,
					/* "-z", diag_flag, */
					(char *) 0);
					break;
				default:
					/* will never happen */
					break;
				} /* switch */
				/* check if exec failed */
				if (ret == -1)
					{
					sprintf(log_buf, "execl(path=<%s>, directory=<%s>, program=<%s>) system call failed, errno = %d", appl_name, dir_name, program_name, errno);
					resp_log(mod, REPORT_NORMAL, 3038, log_buf);
					}
				exit (0);
				sprintf(log_buf, "Can't start application %s. errno = %d", appl_name, errno);
				resp_log(mod, REPORT_NORMAL, 3011, log_buf);
				}
			if (pid[i] == -1)
				{
				sprintf(log_buf,"fork() system call failed, errno = %d", errno);
				resp_log(mod, REPORT_NORMAL, 3041, log_buf);
				} /* if fork failed */
			else
				{ /* update application start time and pid */
				(void)write_fld(tran_tabl_ptr, i, APPL_PID, pid[i]); 
				(void)write_fld(tran_tabl_ptr, i, APPL_START_TIME, t); 
				/* increase the application instance for this program */
				(void)application_instance_manager(ISP_ADD_INSTANCE, appl_name, resource_id);
				/* Log the message for application being started */
				sprintf(log_buf, "DEBUG: Application <%s> started pid: %d Resource <%s>", appl, pid[i], resource_id);
				resp_log(mod, REPORT_NORMAL, 3012, log_buf);
				} /* else fork success */
			}
		else
			{
			stat_arry[i] = 0;
			}
		if (object == SNA)
			(void) sleep(2); 
		} /* stat_arry */
	mem_ptr += SHMEM_REC_LENGTH;	/* position the ptr to next record */
	} /* for */
mem_ptr = pvt_ptr;		/* restore the the shared memory pointer */

return (ISP_SUCCESS);
} /* launch_applications */
/*------------------------------------------------------------------------------
This routine creates the shared memory segments and Message Queues 
------------------------------------------------------------------------------*/
int createSharedMem(int obj)
{
static	char	mod[] = "create_msgq_shmem";
key_t	shm_key;

/* set shared memory key and queue */
switch(obj)
	{
	case	SNA:
		shm_key = SHMKEY_SNA;
		break;
	case	TCP:
		shm_key = SHMKEY_TCP;
		break;
	default:
		sprintf(log_buf,  "Invalid server identifier: %d.", obj);	
		resp_log(mod, REPORT_NORMAL, 3014, log_buf);
		return (ISP_FAIL);
	} /* switch */
/* create shared memory segment */
if((tran_shmid = shmget(shm_key,SIZE_SCHD_MEM,PERMS|IPC_CREAT|IPC_EXCL))< 0)
{
	sprintf(log_buf, "Unable to get shared memory segment %ld. errno = %d", 
							shm_key, errno);
	resp_log(mod, REPORT_NORMAL, 3005, log_buf);
	return (-1);
}
sprintf(log_buf, "DEBUG: Shared memory = %ld (hex = %x) created.", 
										(long)shm_key, GV_shm_key);
resp_log(mod, REPORT_DETAIL, 3060, log_buf);

return (ISP_SUCCESS);
} /* createSharedMem */
/*------------------------------------------------------------------------------
 This routine removes unnecessary messages from the message Queue. 
------------------------------------------------------------------------------*/
int remove_msgs(type)
int	type;
{
Mesg	msg1;
long	que_no;
static	char	mod[] = "remove_msgs";
static	int	got_msgqid = 0;

if(start_net_serv == 0)
	return(0);
else
	{
	if(got_msgqid == 0)
		{
		switch(object)
			{
			case	SNA:
				que_no  = SNA_REQS_MQUE;
				break;
			case	TCP:
				que_no  = TCP_REQS_MQUE;
				break;
			default:
				sprintf(log_buf,  "Invalid server identifier: %d.", object);	
				resp_log(mod, REPORT_NORMAL, 3014, log_buf);
				return (ISP_FAIL);
			} /* switch */
		/* get message id for the request/response queue */
		GV_que_no = que_no;
		resp_mqid = msgget(que_no, PERMS);
		got_msgqid = 1;
		}
	}
return (ISP_SUCCESS);
} /* remove_msgs */	
/*------------------------------------------------------------------------------
 This routine reads the message from the message queue 
------------------------------------------------------------------------------*/
int mesg_recv(id, mesqptr)
int	id;
Mesg	*mesqptr;
{
static	char	mod[]="mesg_recv";
int	ret_code;

ret_code = msgrcv(id, mesqptr, MSIZE, mesqptr->mesg_type, 0|IPC_NOWAIT);
if(ret_code <= 0)
	{
	if(errno != ENOMSG)		/* no message of that type */
		{
		sprintf(log_buf, "Unable to receive message from queue %ld. errno = %d", GV_dyn_mgr_que, errno);
		resp_log(mod, REPORT_NORMAL, 3017, log_buf);
		return(-1);
		}
	}
return(ret_code);
} /* mesg_recv */
/*------------------------------------------------------------------------------
 This routine loads the shared memory into the memory 
------------------------------------------------------------------------------*/
int load_shmem_tabl(mem_id)
int	mem_id;
{
static	char	mod[]="load_shmem_tabl";
int	res_found=ISP_NOTFOUND;
int	slot_no=0;
char	*pvt_ptr;		/* pointer to the shared memory */
char	buf[BUFSIZE];
char	*tmp_ptr;
char	resource_no[30];
char	file_name[MAX_PROGRAM];
char	appl_grp[MAX_APPLICATION];

pvt_ptr = shmat(mem_id, 0, 0); 	/* attach the shared memory segment */
tmp_ptr = pvt_ptr;
if(pvt_ptr == (char *) (-1))
	{
	sprintf(log_buf, "Unable to attach shared memory segment %ld. errno = %d", GV_shm_key, errno);
	resp_log(mod, REPORT_NORMAL, 3003, log_buf);
	return (ISP_FAIL);
	}
/* load resource into shadred memory slot */
for (slot_no=0; slot_no<MAX_SLOT && slot_no < tot_resources; slot_no++)
	{ 
	buf[0] = '\0';
	file_name[0] = '\0';
	sprintf(resource_no, "%s", resource[slot_no].res_no);
	/* for ISDN dynamic application fire dynamic manager */
	switch(object)
		{
		case	TCP:
			if (find_application(SCHEDULER, slot_no, resource[slot_no].static_dest, "", file_name, appl_grp) != ISP_SUCCESS)
				{
				sprintf(buf, SHM_FORMAT, "Reserve", "0", resource_no, "field4", "0", "0000000000", "N/A", "6", "0", "0");
				}
			else
				{
				sprintf(buf, SHM_FORMAT, "Reserve", "0", resource_no, "field4", "0", "0000000000", file_name, "0", "0", "0");
				}
			break;

		case	SNA:
			if (find_application(SCHEDULER, slot_no, resource[slot_no].static_dest, "", file_name, appl_grp) != ISP_SUCCESS)
				{
				sprintf(buf, SHM_FORMAT, "Reserve", "0", resource_no, "field4", "0", "0000000000", "N/A", "6", "0", "0");
				}
			else
				{
				sprintf(buf, SHM_FORMAT, "Reserve", "0", resource_no, "field4", "0", "0000000000", file_name, "0", "0", "0");
				}
			break;
		} /* switch */
	/* fill the record into slot */
	(void) strncpy(pvt_ptr, buf, strlen(buf));
	pvt_ptr += strlen(buf);
	} /* for all slot */

ret_code = shmdt(tmp_ptr);		/* detach the shared memory segment */
if(ret_code == -1)
	{
	sprintf(log_buf, "Unable to dettach shared memory segment %ld errno = %d", GV_shm_key, errno);
	resp_log(mod, REPORT_NORMAL, 3013, log_buf);
	return (ISP_FAIL);
	}
return (ISP_SUCCESS);
} /* load_shmem_tabl */
/*------------------------------------------------------------------------------
 this routine removes the shared memory & message queues 
------------------------------------------------------------------------------*/
int removeSharedMem(int obj)
{
static	char	mod[] = "removeSharedMem";
key_t	shm_key;
int		ret_code;

/* set shared memory key and queue */
switch(obj)
	{
	case	SNA:
		shm_key = SHMKEY_SNA;
		break;
	case	TCP:
		shm_key = SHMKEY_TCP;
		break;
	default:
		sprintf(log_buf, "Invalid server identifier: %d.", obj);	
		resp_log(mod, REPORT_NORMAL, 3014, log_buf);
		return (ISP_FAIL);
	} /* switch */

/* remove shared memory  -  get shared memory segments id */
tran_shmid = shmget(shm_key, SIZE_SCHD_MEM, PERMS);
ret_code = shmctl(tran_shmid, IPC_RMID, 0);
if((ret_code < 0) && (errno != EINVAL))
{
	sprintf(log_buf,"Can't remove shared memory. errno=%d.", errno);
	resp_log(mod, REPORT_NORMAL, 3058, log_buf);
	return (ISP_FAIL);
}
return (ISP_SUCCESS);
} /*removeSharedMem */
/*--------------------------------------------------------------------------
Check if responsibility is already running.
---------------------------------------------------------------------------*/
static	char	ps[] = "ps -ef";   

int check_responsibility()
{
static	char	mod[] = "check_responsibility";
FILE	*fin;			/* file pointer for the ps pipe */
int	i;
char	buf[BUFSIZE];

if((fin = popen(ps, "r")) == NULL)
	{		/* open the process table */
	sprintf(log_buf, 
	"Failed to verify that Responsibility is running. errno=%d.", errno);
	resp_log(mod, REPORT_NORMAL, 3052, log_buf);
	exit(0);
	}

(void) fgets(buf, sizeof buf, fin); 	/* strip of the header */
				/* get the responsibility s proc_id */
i = 0;
while (fgets(buf, sizeof buf, fin) != NULL)
	{
	if(strstr(buf, resp_name) != NULL)
		{
		i = i + 1;
		if(i >= 2)
			{
			sprintf(log_buf, "%s", "Responsibility is already running.");
			resp_log(mod, REPORT_NORMAL, 3053, log_buf);
			exit(0);
			}
		}
	}
(void) pclose(fin);

return(ISP_SUCCESS);
} /* check_responsibility */
/*------------------------------------
Check if all configuration file exists.
--------------------------------------*/
int check_configuration_tables(obj)
int	obj;
{
static	char	mod[] = "check_configuration_tables";
register	int	i=0;
int	tot_config_table = 0;
char	file_path[1024];

resp_log(mod, REPORT_DETAIL, 3027,"check_configuration_table(): Checking configuration tables ...");
switch (obj)
	{
	case	SNA:
		tot_config_table = sna_tot_tables;
		break;
	case	TCP:
		tot_config_table = tcp_tot_tables;
		break;
	default :
		sprintf(log_buf, "Invalid server identifier: %d.", obj);
		resp_log(mod, REPORT_NORMAL, 3014, log_buf);
		return (ISP_FAIL);
	} /* switch */

for (i=0; i < tot_config_table; i++)
	{
	if (obj == SNA)
		sprintf(file_path, "%s/%s", table_path, sna_config_file[i]);
	else if (obj == TCP)
		sprintf(file_path, "%s/%s", table_path, tcp_config_file[i]);
	else
		{
		sprintf(log_buf, "Invalid server identifier: %d.", obj);
		resp_log(mod, REPORT_NORMAL, 3014, log_buf);
		return (ISP_FAIL);
		}
	sprintf(log_buf, "DEBUG: Checking existence of file <%s>.", file_path);
	resp_log(mod, REPORT_VERBOSE, 3015, log_buf);
	/* check if file exists */	
	ret_code = access(file_path, R_OK);
	if(ret_code < 0)
		{
		sprintf(log_buf, "Can't access file %s, errno = %d", file_path, errno);
		resp_log(mod, REPORT_NORMAL, 3057, log_buf);
		return (ISP_FAIL);
		}
	} /* for all files */
return (ISP_SUCCESS);
} /* check_configuration_tables */
/*----------------------------------------------------------------------------
int chek_environment()
---------------------------------------------------------------------------*/
int check_environment(int obj)
{
static	char	mod[] = "check_environment";
char 	*speech_file_var;
char	*home;

resp_log(mod, REPORT_DETAIL, 3028, "check_environment(): Checking Environment");
/* set HOME path */
if ((home = getenv("HOME")) == NULL)
	{
	sprintf(log_buf, "Environment variable %s is not found/set.", "HOME");
	resp_log(mod, REPORT_NORMAL, 3001, log_buf);
	fprintf(stderr, "Unable to read variable %s from environment\n", "HOME");
	return (ISP_FAIL);
	}
sprintf(isp_home, "%s", home);

/* set ISPBASE path */
if ((base_path=(char *)getenv("ISPBASE")) == NULL)
	{
	sprintf(log_buf, "Environment variable %s is not found/set.","ISPBASE");
	resp_log(mod, REPORT_NORMAL, 3001, log_buf);
	fprintf(stderr, "Unable to read variable %s from environment\n", "ISPBASE");
	return (ISP_FAIL);
	}
sprintf(isp_base, "%s", base_path);

switch (obj)
	{
	case	SNA:
		break;
	case	TCP:
		/* nothing */
		break;
	default :
		sprintf(log_buf, "Invalid server identifier: %d.", obj);
		resp_log(mod, REPORT_NORMAL, 3014, log_buf);
		return (ISP_FAIL);
	} /* switch */		
return (ISP_SUCCESS);
} /* check_environment */
/*----------------------------------------------------------------------------
int set_object_path()
---------------------------------------------------------------------------*/
int set_object_path(int obj)
{
static	char	mod[] = "set_object_path";
char	server_dir[256];
struct	utsname	sys_info;
struct	rlimit 	limits;

sprintf(mod, "%s", "set_object_path");
/* get machine name on which object is running */
(void)uname(&sys_info);
sprintf(log_buf, "Machine Name = %s", sys_info.nodename);
resp_log(mod, REPORT_VERBOSE, 3029, log_buf);
resp_log(mod,REPORT_VERBOSE,3030,"Setting directory path structure for object");
/* set defailt variables */
sprintf(heartbeat_interval, "%s", "60");	/* one minute */
sprintf(DefaultLang, "%s", "AMENG");
sprintf(NetworkStatus, "%s", "OFF");
sprintf(diag_flag, "%s", "0");

sprintf(object_machine_name, "%s", sys_info.nodename);

switch (obj)
	{
	case	SNA:
		sprintf(server_dir, "%s", SNA_DIR);
		break;
	case	TCP:
		sprintf(server_dir, "%s", TCP_DIR);
		break;
	default :
		sprintf(log_buf, "Invalid server identifier: %d.", obj);
		resp_log(mod, REPORT_NORMAL, 3014, log_buf);
		return (ISP_FAIL);
	} /* switch */
sprintf(table_path, "%s/%s/%s", isp_base, server_dir, TABLE_DIR);
sprintf(exec_path, "%s/%s/%s", isp_base, server_dir, EXEC_DIR);
sprintf(lock_path, "%s/%s/%s", isp_base, server_dir, LOCK_DIR);

/* set configuration table files path */
sprintf(schedule_tabl, "%s/%s", table_path, "schedule");
sprintf(resource_tabl, "%s/%s", table_path, "ResourceDefTab");
sprintf(appref_tabl, "%s/%s", table_path, "appref");
sprintf(pgmref_tabl, "%s/%s", table_path, "pgmreference");

resp_log(mod, REPORT_VERBOSE, 3031, exec_path);
resp_log(mod, REPORT_VERBOSE, 3032, table_path);
resp_log(mod, REPORT_VERBOSE, 3033, lock_path);
/* set resource limit for number of file open */
(void)getrlimit(RLIMIT_NOFILE, &limits);
limits.rlim_cur = limits.rlim_max;
(void)setrlimit(RLIMIT_NOFILE, &limits);
/* set session leader as responsibility */
setsid(); 
return (ISP_SUCCESS);
} /* set_object_path */
/*------------------------------------------------------------------------
This routine initializes the dead_application and dead_tran_stat array.
-------------------------------------------------------------------------*/
void init_array()
{
int i;

for(i = 0; i < MAX_PROCS; i++)
	{
	dead_application[i] = -1;   	/* initialize the array element by -1 */
   	dead_appl_stat[i] = 0;    	/* initialize the status by 0 */
	res_status[i].status = NOT_FREE; /* initialize all resource not free */
	res_status[i].pid = 0; /* initialize all resource pid */
	res_attach[i] = RESOURCE_NOT_ATTACH;
	}
return;
} 
/*-------------------------------------------------------------------------
This routine checks for the dead application and removes the message.
-------------------------------------------------------------------------*/
int clean_msgs_for_dead_appl()
{
int	i;

for(i=0; i< MAX_PROCS; i++)
	{
	if(dead_application[i] != -1)
		{   /* if the dead proc_id exists then remove
                                      it from message response queue */
		(void) remove_msgs(dead_application[i]);
		dead_appl_stat[i] += 1;	/* update status of appli. proc_id */
		}
	if(dead_appl_stat[i] > 2)
		{     /*  message deleted - confirmation - reset flag */
		dead_application[i] = -1;         /* initialize the array */
		dead_appl_stat[i] = 0;          /* initialize the status */
		}
	}
return (ISP_SUCCESS);
} /* clean_msgs_for_dead_appl */
/*-------------------------------------------------------------------
load_tables(): This Routine load all the tables into memory.
--------------------------------------------------------------------*/
int	load_tables(char *schedule_file, char *resource_file, char *appref_file, char *pgmref_file)
{
static	char	mod[]="load_tables"; 
static	int	first_time = 1;

/* Load server specific parameter configuration table */
load_parameter_configuration();
/* resource table can't be reload dynamically */
if (first_time == 1)
	{
	if (load_resource_table(resource_file) != ISP_SUCCESS)
		{
		return (ISP_FAIL);
		}
	first_time = 0;
	}
if (load_appref_table(appref_file) != ISP_SUCCESS)
	{
	return (ISP_FAIL);
	}
if (load_pgmref_table(pgmref_file) != ISP_SUCCESS)
	{
	return (ISP_FAIL);
	}
if (load_schedule_table(schedule_file) != ISP_SUCCESS)
	{
	return (ISP_FAIL);
	}
return (ISP_SUCCESS);
} /* load_tables */
/*-------------------------------------------------------------------
load_resource_table(): This Routine load resource table into memory.
--------------------------------------------------------------------*/
int	load_resource_table(char *resource_file)
{
static	char	mod[]="load_resource_table";
FILE	*fp;
char	record[1024];
char	field[1024];
int	field_no = 0;
int	load_entry = 0;			/* decide whether to load entry */

sprintf(log_buf, "load_resource_table(): Loading %s table", resource_file);
resp_log(mod, REPORT_VERBOSE, 3034, log_buf);

if ((fp=fopen(resource_file, "r")) == NULL)
	{
	sprintf(log_buf,"Unable to access file %s. errno = %d",resource_file,errno);
	resp_log(mod, REPORT_NORMAL, 3016, log_buf);
	return (ISP_FAIL);
	}
while (fgets(record, sizeof(record), fp) != NULL)
	{
	load_entry = 1;			/* load the entry */
	for (field_no = 1; field_no <= MAX_RESOURCE_FIELD; field_no ++)
		{
		field[0] = '\0';
		if (get_field(record, field_no, field) < 0)
			{
			sprintf(log_buf, "Corrupted line in <%s>. Unable to read field no %d, record no. %d. Line is: <%s>", resource_file,field_no,tot_resources,record);
			resp_log(mod, REPORT_NORMAL, 3018, log_buf);
			load_entry = 0;		/* don't load entry */
			/* return (ISP_FAIL); */
			}
		if (load_entry == 0)		/* skip record for errors */
			break;
		switch(field_no)
			{
			case 	RESOURCE_NO:			/* port no */
				sprintf(resource[tot_resources].res_no, "%s", field);
				if ((int)strlen(resource[tot_resources].res_no) > fld_siz[APPL_RESOURCE-1])
					{
					sprintf(log_buf, "Ignoring entry ResourceDefTab <%s>, Resource name %s is > maximum resource length %d", record, field, fld_siz[APPL_RESOURCE-1]);
					resp_log(mod, REPORT_NORMAL, 3035, log_buf);
					continue;
					}
				break;
			case 	RESOURCE_TYPE:
				sprintf(resource[tot_resources].res_type, "%s", field);
				break;
			case 	RESOURCE_STATE:
				sprintf(resource[tot_resources].res_state, "%s", field);
				break;
			case 	RESOURCE_USAGE:
				sprintf(resource[tot_resources].res_usage, "%s", field);
				break;
			case	STATIC_DEST:
				sprintf(resource[tot_resources].static_dest, "%s", field);
				break;
			default :
				sprintf(log_buf, "File = %s, Field %d is invalid field number", resource_file, field_no);
				resp_log(mod, REPORT_NORMAL, 3019, log_buf);
				break;
			} /*switch */
		} /* for */
	if (load_entry == 0)
		continue;		/* skip entry */
	/* for sna object read PU name (resource qualifier) 
	   from resource file */
	if (object == SNA)
		{
		resource[tot_resources].res_qualifier[0] = '\0';

		(void) find_pu_name(resource[tot_resources].res_no, resource[tot_resources].res_qualifier);

		sprintf(log_buf, "find_pu_name returned (resource[%d]res_no(%s), resource[%d].res_qualifier(%s))", 
				tot_resources, resource[tot_resources].res_no,
				tot_resources, resource[tot_resources].res_qualifier);
		resp_log(mod, REPORT_VERBOSE, 3036, log_buf);

		}
#ifdef DEBUG_TABLES
	sprintf(log_buf, "Resource no = %s", resource[tot_resources].res_no);
	resp_log(mod, REPORT_VERBOSE, 3036, log_buf);
	if (object == SNA)
		{
		sprintf(log_buf, "Resource qualifier = %s", resource[tot_resources].res_qualifier);
		resp_log(mod, REPORT_VERBOSE, 3037, log_buf);
		}
	sprintf(log_buf, "Resource type = %s", resource[tot_resources].res_type);
	resp_log(mod, REPORT_VERBOSE, 3023, log_buf);
	sprintf(log_buf, "Resource state = %s", resource[tot_resources].res_state);
	resp_log(mod, REPORT_VERBOSE, 3044, log_buf);
	sprintf(log_buf, "Resource usage = %s", resource[tot_resources].res_usage);
	resp_log(mod, REPORT_VERBOSE, 3045, log_buf);
	sprintf(log_buf, "Resource static destination = %s", resource[tot_resources].static_dest);
	resp_log(mod, REPORT_VERBOSE, 3042, log_buf);
	sprintf(log_buf,"%s", "*******************  next resource ***************");
	resp_log(mod, REPORT_VERBOSE, 3043, log_buf);
#endif
	/* for reserve resource make resource status free */
	if ( strcmp(resource[tot_resources].res_usage, RESERVE_PORT) == 0)
		res_status[atoi(resource[tot_resources].res_no)].status = FREE;
	tot_resources ++;
	} /* while end of file */

(void) fclose(fp);

sprintf(log_buf,"Total entries loaded from resource table = %d.",tot_resources);
resp_log(mod, REPORT_VERBOSE, 3046, log_buf);

return (ISP_SUCCESS);
} /* load_resource_table */
/*-------------------------------------------------------------------
load_schedule_table(): This Routine load scheduling table into memory.
--------------------------------------------------------------------*/
int	load_schedule_table(char *schedule_file)
{
static	char	mod[]="load_schedule_table";
register	int	i,j;
FILE	*fp;
char	record[1024];
char	field[1024];
int	field_no = 0;
char	pgm_name[100];
int	pgm_found = ISP_NOTFOUND, grp_found = ISP_NOTFOUND;
int	load_entry = 0;			/* decide whether to load entry */

tot_schedules = 0;

sprintf(log_buf, "Loading %s table....", schedule_file);
resp_log(mod, REPORT_VERBOSE, 3047, log_buf);
if ((fp=fopen(schedule_file, "r")) == NULL)
	{
	sprintf(log_buf, "Unable to open file %s. errno = %d", schedule_file,errno);
	resp_log(mod, REPORT_NORMAL, 3040, log_buf);
	return (ISP_FAIL);
	}
while (fgets(record, sizeof(record), fp) != NULL)
	{
	load_entry = 1;			/* load the entry */
	for (field_no = 1; field_no <= MAX_SCHEDULE_FIELD; field_no ++)
		{
		field[0] = '\0';
		if (get_field(record, field_no, field) < 0)
			{
			sprintf(log_buf, "Corrupted line in <%s>. Unable to read field no %d, record no. %d. Line is: <%s>", schedule_file, field_no,tot_schedules, record);
			resp_log(mod, REPORT_NORMAL, 3048, log_buf);
			load_entry = 0;		/* don't load entry */
			/* return (ISP_FAIL); */
			}
		if (load_entry == 0)		/* skip record for errors */
			break;
		switch(field_no)
			{
			case 	SERVER_NAME:
				schedule[tot_schedules].srvtype[0] = '\0';
				sprintf(schedule[tot_schedules].srvtype, "%s", field);
				break;
			case 	MACHINE_NAME:
				schedule[tot_schedules].machine[0] = '\0';
				sprintf(schedule[tot_schedules].machine, "%s", field);
				break;
			case 	DESTINAION:
				schedule[tot_schedules].destination[0] = '\0';
				sprintf(schedule[tot_schedules].destination, "%s", field);
				break;
			case 	ORIGINATION:
				schedule[tot_schedules].origination[0] = '\0';
				sprintf(schedule[tot_schedules].origination, "%s", field);
				break;
			case	PRIORITY:
				schedule[tot_schedules].priority = 0;
				schedule[tot_schedules].priority = atoi(field);
				break;
			case 	RULE:
				schedule[tot_schedules].rule = 0;
				schedule[tot_schedules].rule = atoi(field);
				break;
			case 	START_DATE:
				schedule[tot_schedules].start_date[0] = '\0';
				sprintf(schedule[tot_schedules].start_date, "%s", field);
				break;
			case 	STOP_DATE:
				schedule[tot_schedules].stop_date[0] = '\0';
				sprintf(schedule[tot_schedules].stop_date, "%s", field);
				break;
			case	START_TIME:
				schedule[tot_schedules].start_time[0] = '\0';
				sprintf(schedule[tot_schedules].start_time, "%s", field);
				break;
			case 	STOP_TIME:
				schedule[tot_schedules].stop_time[0] = '\0';
				sprintf(schedule[tot_schedules].stop_time, "%s", field);
				break;
			case 	PROGRAM_NAME:	
				schedule[tot_schedules].program[0] = '\0';
				sprintf(schedule[tot_schedules].program, "%s", field);
				if((int)strlen(schedule[tot_schedules].program) > MAX_APPL_NAME)
					{
					sprintf(log_buf, "Ignore scheduling table entry for %s : Length of Application name too long, limit = %d", schedule[tot_schedules].program, MAX_APPL_NAME);
					resp_log(mod, REPORT_NORMAL, 3049, log_buf);
					continue;
					}
				break;
			default :
				sprintf(log_buf, "Field %d is invalid field number", field_no);
				resp_log(mod, REPORT_NORMAL, 3054, log_buf);
				break;
			} /*switch */
		} /* for */
	if (load_entry == 0)
		continue;		/* corrupted entry */
#ifdef DEBUG_TABLES
sprintf(log_buf, "Service = %s", schedule[tot_schedules].srvtype);
resp_log(mod, REPORT_VERBOSE, 3055, log_buf);
sprintf(log_buf, "Machine = %s", schedule[tot_schedules].machine);
resp_log(mod, REPORT_VERBOSE, 3056, log_buf);
sprintf(log_buf, "Destination = %s", schedule[tot_schedules].destination);
resp_log(mod, REPORT_VERBOSE, 3059, log_buf);
sprintf(log_buf, "Origination = %s", schedule[tot_schedules].origination);
resp_log(mod, REPORT_VERBOSE, 3062, log_buf);
sprintf(log_buf, "Priority = %d", schedule[tot_schedules].priority);
resp_log(mod, REPORT_VERBOSE, 3064, log_buf);
sprintf(log_buf, "Rule = %d", schedule[tot_schedules].rule);
resp_log(mod, REPORT_VERBOSE, 3074, log_buf);
sprintf(log_buf, "Start Date = %s", schedule[tot_schedules].start_date);
resp_log(mod, REPORT_VERBOSE, 3076, log_buf);
sprintf(log_buf, "Stop Date = %s", schedule[tot_schedules].stop_date);
resp_log(mod, REPORT_VERBOSE, 3077, log_buf);
sprintf(log_buf, "Start Time = %s", schedule[tot_schedules].start_time);
resp_log(mod, REPORT_VERBOSE, 3078, log_buf);
sprintf(log_buf, "Stop Time = %s", schedule[tot_schedules].stop_time);
resp_log(mod, REPORT_VERBOSE, 3079, log_buf);
sprintf(log_buf, "Program = %s", schedule[tot_schedules].program);
resp_log(mod, REPORT_VERBOSE, 3080, log_buf);
#endif
	/* check if program exists , if not exists don't load the entry */
	if (access(schedule[tot_schedules].program, R_OK | X_OK) != 0)
		{
		sprintf(log_buf, "Application <%s> not found, errno = %d. Scheduling table entry (DNIS, ANI) = (%s, %s) is ignored.", schedule[tot_schedules].program, errno, schedule[tot_schedules].destination, schedule[tot_schedules].origination);
		resp_log(mod, REPORT_NORMAL, 3081, log_buf);
		load_entry = 0;			/* don't load entry */
		}
	/* find and load program name */
	/* get basename of program */
	sprintf(pgm_name, "%s", schedule[tot_schedules].program); 
	pgm_found = ISP_NOTFOUND;
	for (i=0; i< tot_pgmrefs; i++)
		{
		if (strcmp(pgm_name, pgmref[i].program_name) == 0)
			{
			sprintf(schedule[tot_schedules].appl_grp_name, "%s", pgmref[i].appl_grp_name);
			pgm_found = ISP_FOUND;
			break;
			}
		}	
	/* if program found in program ref now check in application reference */
	if (pgm_found == ISP_FOUND)
		{
		grp_found = ISP_NOTFOUND;
		/*  find and load application name and maxium instances */
		for (j=0; j < tot_apprefs; j++)
			{
			if (strcmp(schedule[tot_schedules].appl_grp_name, appref[j].appl_grp_name) == 0)
				{
				sprintf(schedule[tot_schedules].acct_code, "%s", appref[j].acct_code);
				schedule[tot_schedules].max_instance = appref[j].max_instance;
				grp_found = ISP_FOUND;
				break;
				}
			} /* for */	
		} /* if */
	else
		{ /* not found */
		sprintf(log_buf, "WARNING: Program Name = %s is not found in program reference file, program will not be started by %s server. Scheduling table entry for (DNIS, ANI) = (%s, %s) is ignored.", schedule[tot_schedules].program, object_name, schedule[tot_schedules].destination, schedule[tot_schedules].origination);
		resp_log(mod, REPORT_VERBOSE, 3082, log_buf);
		load_entry = 0;			/* don't load entry */
		continue;
		}
	/* if application group not in program reference */
	if (pgm_found == ISP_FOUND && grp_found == ISP_NOTFOUND)
		{
		sprintf(log_buf, "WARNING: Program Name = %s Application Name : %s is not found in application reference file, program will not be started by %s server. Scheduling table entry for (DNIS, ANI) = (%s, %s) is ignored.", schedule[tot_schedules].program, schedule[tot_schedules].appl_grp_name, object_name, schedule[tot_schedules].destination, schedule[tot_schedules].origination);
		resp_log(mod, REPORT_VERBOSE, 3083, log_buf);
		load_entry = 0;			/* don't load entry */
		continue;
		}
#ifdef DEBUG_TABLES
sprintf(log_buf, "Application Group = %s", schedule[tot_schedules].appl_grp_name);
resp_log(mod, REPORT_VERBOSE, 3084, log_buf);
sprintf(log_buf, "Accounting Code = %s", schedule[tot_schedules].acct_code);
resp_log(mod, REPORT_VERBOSE, 3085, log_buf);
sprintf(log_buf, "Max Instances = %d", schedule[tot_schedules].max_instance);
resp_log(mod, REPORT_VERBOSE, 3086, log_buf);
#endif

	if (load_entry == 1)			/* load entry is ok */
		tot_schedules ++;
	} /* while end of file */

(void) fclose(fp);

sprintf(log_buf, "Total entries loaded from scheduling table = %d.", tot_schedules);
resp_log(mod, REPORT_VERBOSE, 3087, log_buf);

return (ISP_SUCCESS);
} /* load_schedule_table */
/*-------------------------------------------------------------------
load_appref_table(): This Routine load appref table into memory.
--------------------------------------------------------------------*/
int	load_appref_table(char *appref_file)
{
static	char	mod[]="load_appref_table";
FILE	*fp;
char	record[1024];
char	field[1024];
int	field_no = 0;
int	load_entry = 0;			/* decide whether to load entry */

if (Turnkey_License)
{
	load_appref_pgmref_from_license();
	return(ISP_SUCCESS);
}

tot_apprefs = 0;

sprintf(log_buf, "load_appref_table(): Loading %s table", appref_file);
resp_log(mod, REPORT_VERBOSE, 3066, log_buf);

if ((fp=fopen(appref_file, "r")) == NULL)
	{
	sprintf(log_buf, "Unable to open file <%s>. errno=%d.", appref_file, errno);
	resp_log(mod, REPORT_NORMAL, 3040, log_buf);
	return (ISP_FAIL);
	}
while (fgets(record, sizeof(record), fp) != NULL)
	{
	load_entry = 1;			/* load the entry */
	for (field_no = 1; field_no <= MAX_APPREF_FIELD; field_no ++)
		{
		field[0] = '\0';
		if (get_field(record, field_no, field) < 0)
			{
			sprintf(log_buf, "Corrupted line in <%s>. "
				"Unable to read field no %d, record no. %d. Line is: <%s>", 
				appref_file, field_no, tot_apprefs, record);
			resp_log(mod, REPORT_NORMAL, 3067, log_buf);
			load_entry = 0;		/* don't load entry */
			/* return (ISP_FAIL); */
			}
		if (load_entry == 0)		/* skip record for errors */
			break;
		switch(field_no)
			{
			case 	APPL_GRP_NAME:
				sprintf(appref[tot_apprefs].appl_grp_name, "%s", field);
				break;
			case 	ACCT_CODE:
				sprintf(appref[tot_apprefs].acct_code, "%s", field);
				break;
			case	MAX_INSTANCES:
				appref[tot_apprefs].max_instance = atoi(field);
				break;
			default :
				sprintf(log_buf, "File = %s, Field %d is invalid field number", appref_file, field_no);
				resp_log(mod, REPORT_NORMAL, 3068, log_buf);
				break;
			} /*switch */
		} /* for */
	if (load_entry == 0)
		continue;		/* corrupted entry */
	sprintf(log_buf, "Application Group Name = %s", appref[tot_apprefs].appl_grp_name);
	resp_log(mod, REPORT_VERBOSE, 3069, log_buf);
	sprintf(log_buf, "Accounting Code = %s", appref[tot_apprefs].acct_code);
	resp_log(mod, REPORT_VERBOSE, 3070, log_buf);
	sprintf(log_buf, "Maxium Instances = %d", appref[tot_apprefs].max_instance);
	resp_log(mod, REPORT_VERBOSE, 3071, log_buf);
	tot_apprefs ++;
	} /* while end of file */

(void) fclose(fp);

#ifdef DEBUG_TABLES
sprintf(log_buf, 
	"Total entries loaded from Application Group Reference table = %d.", 
															tot_apprefs);
resp_log(mod, REPORT_VERBOSE, 3072, log_buf);
#endif

return (ISP_SUCCESS);
} /* load_appref_table */
/*-------------------------------------------------------------------
load_pgmref_table(): This Routine load pgmref table into memory.
--------------------------------------------------------------------*/
int	load_pgmref_table(char *pgmref_file)
{
static	char	mod[]="load_pgmref_table";
FILE	*fp;
char	record[1024];
char	field[1024];
int	field_no = 0;
int	load_entry = 0;			/* decide whether to load entry */

if (Turnkey_License)
{ /* No need to load pgmref, it's loaded with appref */
	return(ISP_SUCCESS);
}

tot_pgmrefs = 0;

sprintf(log_buf, "load_pgmref_table(): Loading %s table", pgmref_file);
resp_log(mod, REPORT_VERBOSE, 3073, log_buf);
if ((fp=fopen(pgmref_file, "r")) == NULL)
	{
	sprintf(log_buf, "Unable to open file %s. errno = %d", pgmref_file, errno);
	resp_log(mod, REPORT_NORMAL, 3040, log_buf);
	return (ISP_FAIL);
	}
while (fgets(record, sizeof(record), fp) != NULL)
	{
	load_entry = 1;			/* load the entry */
	for (field_no = 1; field_no <= MAX_PGMREF_FIELD; field_no ++)
		{
		field[0] = '\0';
		if (get_field(record, field_no, field) < 0)
			{
			sprintf(log_buf,"Corrupted line in <%s>. Unable to read field no %d, record no. %d. Line is: <%s>", pgmref_file, field_no, tot_pgmrefs, record);
			resp_log(mod, REPORT_NORMAL, 3075, log_buf);
			load_entry = 0;		/* don't load entry */
			/* return (ISP_FAIL); */
			}
		if (load_entry == 0)		/* skip record for errors */
			break;
		switch(field_no)
			{
			case 	PROGRAM:	
				sprintf(pgmref[tot_pgmrefs].program_name, "%s", field);
				break;
			case 	APPL_GRP:
				sprintf(pgmref[tot_pgmrefs].appl_grp_name, "%s", field);
				break;
			default :
				sprintf(log_buf, "File = %s, Field %d is invalid field number", pgmref_file, field_no);
				resp_log(mod, REPORT_NORMAL, 3088, log_buf);
				break;
			} /*switch */
		} /* for */
	if (load_entry == 0)
		continue;		/* corrupted entry */
	sprintf(log_buf, "Program Name = %s", pgmref[tot_pgmrefs].program_name);
	resp_log(mod, REPORT_VERBOSE, 3089, log_buf);
	sprintf(log_buf, "Application Group Name = %s", pgmref[tot_pgmrefs].appl_grp_name);
	resp_log(mod, REPORT_VERBOSE, 3090, log_buf);
	tot_pgmrefs ++;
	} /* while end of file */

(void) fclose(fp);

#ifdef DEBUG_TABLES
sprintf(log_buf, "Total entries loaded from Program Reference table = %d.", tot_pgmrefs);
resp_log(mod, REPORT_VERBOSE, 3091, log_buf);
#endif

return (ISP_SUCCESS);
} /* load_pgmreference_table */

/*-------------------------------------------------------------------
This routine gets the value of desired field from data record.
-------------------------------------------------------------------*/
int find_application(requester, resource_index, token1, token2, application, group)
int	requester;		/* requester SCHEDULER/DYNAMIC_MANAGER */
int	resource_index;		/* resource index in resource table */
char	*token1;		/* DNIS */
char	*token2;		/* ANI */
char	*application;
char	*group;
{
static	char	mod[]="find_application";
struct	schedule record;
int	ret;
/* find out program name */
if (check_data_token(TOKEN1_APPL, token1, token2, &record) != ISP_SUCCESS)
	{
	return (ISP_FAIL);
	}

/* after finding application for given token successfully,
	following are the criteria to start application */
/* check server type is same as define in command line object code (argv[1]) */
if (strcmp(record.srvtype, object_code) != 0)
	{
	if (database_hit == 1)
		{
		sprintf(log_buf, "Application %s is not started, (static token = %s) is schedule for %s Server and this server is %s", record.program, token1, record.srvtype, object_name);
		resp_log(mod, REPORT_NORMAL, 3092, log_buf);
		}
	return (ISP_FAIL);
	}
/* 1. check if application schedule is for same machine */
if((strcmp(object_machine_name, record.machine) != 0) && (strcmp("*", record.machine) != 0))
	{
	if (database_hit == 1)
		{
		sprintf(log_buf, "Application %s is not started, (static token = %s) is schedule for machine %s, and %s server name is %s", record.program, token1, record.machine, object_name, object_machine_name);
		resp_log(mod, REPORT_NORMAL, 3093, log_buf);
		}
	return (ISP_FAIL);
	}
/* 3. check date and time rule if STATIC */
if (object == SNA || object == TCP)
	{
	if ((strcmp(resource[resource_index].res_type, "LU2") == 0 ||  /* sna */
	strcmp(resource[resource_index].res_type, "TELNET") == 0 ||  /* tcp */
	strcmp(resource[resource_index].res_type, "SSH") == 0 ||  /* tcp */
	strcmp(resource[resource_index].res_type, "SSH2") == 0 ||  /* tcp */
	strcmp(resource[resource_index].res_type, "RS232") == 0 )/* tcp */
	&& 
	strcmp(resource[resource_index].res_state,"STATIC") == 0) 
		{
		if ((ret = check_date_time_rule(resource[resource_index].res_no, record.program, record.start_date, record.stop_date, record.start_time, record.stop_time, record.rule)) != ISP_SUCCESS)
			{
			if (ret != -1)		/* not normal failure */
				{
				if (database_hit == 1)
					{
					switch (ret)
						{
						case	1: /* rule failed */
							sprintf(log_buf, "Rule criteria are not met, Rule(%d) failed for Program = %s for token %s, program not ready to start. start_date = %d, stop_date = %d, start_time = %d, stop_time = %d", record.rule, record.program, token1, atoi(record.start_date), atoi(record.stop_date), atoi(record.start_time), atoi(record.stop_time));
							break;
						case	2: /* invalid input */
							sprintf(log_buf, "Invalid input parameters, Rule(%d) failed for Program = %s for token %s, program not ready to start. start_date = %d, stop_date = %d, start_time = %d, stop_time = %d", record.rule, record.program, token1, atoi(record.start_date), atoi(record.stop_date), atoi(record.start_time), atoi(record.stop_time));
							break;
						case	3: /* logic error */
							sprintf(log_buf, "Internal logic error, Rule(%d) failed for Program = %s for token %s, program not ready to start. start_date = %d, stop_date = %d, start_time = %d, stop_time = %d", record.rule, record.program, token1, atoi(record.start_date), atoi(record.stop_date), atoi(record.start_time), atoi(record.stop_time));
							break;
						case	4: /* rule not implemented */
							sprintf(log_buf, "Program  Rule(%d) failed for Program = %s for token %s, program not ready to start. Rule not implemented", record.rule, record.program, token1);
							break;
						default :
							sprintf(log_buf, "Unknown return code from rule, return code = %d, Rule(%d) failed for Program = %s for token %s, program not ready to start. start_date = %d, stop_date = %d, start_time = %d, stop_time = %d", ret, record.rule, record.program, token1, atoi(record.start_date), atoi(record.stop_date), atoi(record.start_time), atoi(record.stop_time));
							break;
						} /* switch */		
					if (database_hit == 1)
						{
						resp_log(mod, REPORT_NORMAL, 3094, log_buf);
						}
					} /* if */
				} /* if */
			return (ISP_FAIL);
			}
		else
			{
			sprintf(application, "%s", record.program);
			sprintf(group, "%s", record.acct_code);
			sprintf(log_buf, "Program = %s for token %s, state = ready to run.", record.program, token1);
			resp_log(mod, REPORT_VERBOSE, 3095, log_buf);
			return (ISP_SUCCESS);
			}
		} /* for analog and static */
	else
		{
		if (database_hit == 1)
			{
			sprintf(log_buf, "Resource type %s and state %s is not supported. Static Destination %s, can't find application.", resource[resource_index].res_type,resource[resource_index].res_state, resource[resource_index].static_dest);
			resp_log(mod, REPORT_NORMAL, 3096, log_buf);
			}
		return (ISP_FAIL);
		}
	} /* all Object */
else
	{
	if (database_hit == 1)
		{
		sprintf(log_buf, "object = %d not supported.", object);
		resp_log(mod, REPORT_NORMAL, 3097, log_buf);
		}
	}
return (ISP_FAIL);
} /* find_application */
/*------------------------------------------------------------
This routine check the token information.
and this routine is called for ISDN applications.
--------------------------------------------------------------*/
int	check_data_token(match_rule, token1, token2, record)
int	match_rule;		/* matching rule */
char	*token1;		/* token 1  - DNIS */
char	*token2;		/* token 2 - ANI */
struct	schedule *record;	/* schedule record */
{
static	char	mod[]="check_data_token";
register 	int	i;
char	dnis[20];
char	ani[20];

sprintf(dnis, "%s", token1);
sprintf( ani, "%s", token2);

if (token1 == NULL || (int)strlen(token1) == 0)
	{
	resp_log(mod, REPORT_DETAIL, 3098, "token1 value NULL");
	return (ISP_FAIL); 
	}

if (match_rule == TOKEN_APPL || match_rule == TOKEN1_APPL)
	{
	for (i=0; i < tot_schedules; i++)
		{
		if (strcmp(schedule[i].destination, dnis) == 0 || strcmp(schedule[i].destination, WILD_DNIS) == 0)
			{
			/* token 1 found  or wild token 1 */
			/* find application from token1 only */
			if (match_rule ==  TOKEN1_APPL)
				{
				/* mahesh new change to allow mutiple dnis */
				if (check_date_time_rule("RES", schedule[i].program, schedule[i].start_date, schedule[i].stop_date, schedule[i].start_time, schedule[i].stop_time, schedule[i].rule) != ISP_SUCCESS)
					{
					/* if date and time rule failed find */
					/* next match */
					continue;
					}
				sprintf(record->srvtype, "%s", schedule[i].srvtype);
				sprintf(record->machine, "%s", schedule[i].machine);
				sprintf(record->destination, "%s", schedule[i].destination);
				sprintf(record->origination, "%s", schedule[i].origination);
				record->priority = schedule[i].priority;
				record->rule     = schedule[i].rule;
				sprintf(record->start_date, "%s", schedule[i].start_date);
				sprintf(record->stop_date, "%s", schedule[i].stop_date);
				sprintf(record->start_time, "%s", schedule[i].start_time);
				sprintf(record->stop_time, "%s", schedule[i].stop_time);
				sprintf(record->program, "%s", schedule[i].program);
				sprintf(record->acct_code, "%s", schedule[i].acct_code);
				sprintf(record->appl_grp_name, "%s", schedule[i].appl_grp_name);
				record->max_instance = schedule[i].max_instance;
				return (ISP_SUCCESS);
				}
			/* find application from both token */
			if (match_rule == TOKEN_APPL)
				{
				/* token 2 found */
				if (strcmp(schedule[i].origination, ani) == 0)
					{
					if (check_date_time_rule("RES", schedule[i].program, schedule[i].start_date, schedule[i].stop_date, schedule[i].start_time, schedule[i].stop_time, schedule[i].rule) != ISP_SUCCESS)
						{
						/* if date and time rule */
						/* failed find next match */
						continue;
						}
					sprintf(record->srvtype, "%s", schedule[i].srvtype);
					sprintf(record->machine, "%s", schedule[i].machine);
					sprintf(record->destination, "%s", schedule[i].destination);
					sprintf(record->origination, "%s", schedule[i].origination);
					record->priority = schedule[i].priority;
					record->rule     = schedule[i].rule;
					sprintf(record->start_date, "%s", schedule[i].start_date);
					sprintf(record->stop_date, "%s", schedule[i].stop_date);
					sprintf(record->start_time, "%s", schedule[i].start_time);
					sprintf(record->stop_time, "%s", schedule[i].stop_time);
					sprintf(record->program, "%s", schedule[i].program);
					sprintf(record->acct_code, "%s", schedule[i].acct_code);
					sprintf(record->appl_grp_name, "%s", schedule[i].appl_grp_name);
					record->max_instance = schedule[i].max_instance;
					return (ISP_SUCCESS);
					}
				}
			}
		} /* for rule token1 and token */
	} /* if */

if (match_rule == TOKEN2_APPL)
	{
	for (i=0; i < tot_schedules; i++)
		{
		if (strcmp(schedule[i].origination, ani) == 0)
			{
			if (check_date_time_rule("RES", schedule[i].program, schedule[i].start_date, schedule[i].stop_date, schedule[i].start_time, schedule[i].stop_time, schedule[i].rule) != ISP_SUCCESS)
				{
				/* if date and time rule failed find */
				/* next match */
				continue;
				}
			/* found token 2 */
			sprintf(record->srvtype, "%s", schedule[i].srvtype);
			sprintf(record->machine, "%s", schedule[i].machine);
			sprintf(record->destination, "%s", schedule[i].destination);
			sprintf(record->origination, "%s", schedule[i].origination);
			record->priority = schedule[i].priority;
			record->rule     = schedule[i].rule;
			sprintf(record->start_date, "%s", schedule[i].start_date);
			sprintf(record->stop_date, "%s", schedule[i].stop_date);
			sprintf(record->start_time, "%s", schedule[i].start_time);
			sprintf(record->stop_time, "%s", schedule[i].stop_time);
			sprintf(record->program, "%s", schedule[i].program);
			sprintf(record->acct_code, "%s", schedule[i].acct_code);
			sprintf(record->appl_grp_name, "%s", schedule[i].appl_grp_name);
			record->max_instance = schedule[i].max_instance;
			return (ISP_SUCCESS);
			}
		} /* for */
	} /* if */

if (database_hit == 1)
	{
	switch(object)
		{
		case	TCP:
		case	SNA:
			sprintf(log_buf, "Received static destination <%s>. No such entry in scheduling table.", token1);
			break;
		default:
			sprintf(log_buf, "Received (DNIS,ANI)=(<%s>,<%s>). No such entry in scheduling table.", token1, token2);
			break;
		}
	resp_log(mod, REPORT_VERBOSE, 3099, log_buf);
	}
return (ISP_FAIL);				
} /* check_data_token */
/*---------------------------------------------------------------------------
This routine check the date and time valid to execute the application.
------------------------------------------------------------------------------*/
int 	check_date_time_rule(resource_name, program, start_date_str, stop_date_str, start_time_str, stop_time_str, rule)
char	*resource_name;
char	*program;
char	*start_date_str; 
char	*stop_date_str; 
char	*start_time_str; 
char	*stop_time_str; 
int	rule; 
{
static	char	mod[]="check_date_time_rule";
time_t	now;
struct	tm	*t;				/* time structure */
int	start_date, stop_date, start_time, stop_time, ret;

start_date = atoi(start_date_str);
stop_date = atoi(stop_date_str);
start_time = atoi(start_time_str);
stop_time  = atoi(stop_time_str);

if ((time(&now) == (time_t) -1) || ((t=localtime(&now)) == NULL) )
	{
	sprintf(log_buf, "Unable to get system time. errno = %d", errno);
	resp_log(mod, REPORT_NORMAL, 3022, log_buf);
	return (ISP_FAIL);
	}
/* debug line */
#ifdef DEBUG_RULES
sprintf(log_buf, "Resource Name = %s, start date = %d, stop date = %d, start time = %d, stop time = %d, year = %d, month = %d, week day = %d, month day = %d, hour = %d, minutes = %d, seconds = %d", resource_name, start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
resp_log(mod, REPORT_VERBOSE, 3100, log_buf);
#endif
switch(rule)
	{
	case	0:
		ret = sc_rule0(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	case	1:
		ret = sc_rule1(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	case	2:
		ret =  sc_rule2(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	case	3:
		ret = sc_rule3(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	case	4:
		ret =  sc_rule4(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	case	5:
		ret =  sc_rule5(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	case	6:
		ret = sc_rule6(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	case	7:
		ret = sc_rule7(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	case	8:
		ret = sc_rule8(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	case	9:
		ret =  sc_rule9(start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		break;
	default:
		if (database_hit == 1)
			{
			sprintf(log_buf,
					"Invalid rule number in scheduling table: %d.", rule);
			resp_log(mod, REPORT_NORMAL, 3101, log_buf);
			}
		return (ISP_FAIL);
	} /* rule */
if (ret == 0)
	return (ISP_SUCCESS);
return (ret);
} /* check_date_time_rule */
/*----------------------------------------------------------
Following routine update shared memory slot if necessary.
-----------------------------------------------------------*/
void update_shmem()
{
static	char	mod[]="update_shmem";
int	i, pid_int=0;
int	slot_no=0;
char	field[256];			/* needs to be changed */
int	res_found = ISP_NOTFOUND;
char	file_name[MAX_PROGRAM];
char	application[MAX_PROGRAM];
char	group[MAX_GROUP];
char	pid_str[10];
char	status_str[10];
char	res_name[20];

for(slot_no = 0; slot_no < MAX_SLOT && slot_no < tot_resources; slot_no++)
	{
	/* read application status */
	(void)read_fld(tran_tabl_ptr, slot_no, APPL_STATUS, status_str);     
	/* if application busy skip record */
	if (atoi(status_str) == STATUS_BUSY)
		continue;
	/* read application pid */
	(void)read_fld(tran_tabl_ptr, slot_no, APPL_PID, pid_str);     
	sscanf(pid_str, "%d", &pid_int);
	/* if application already running */
	if (pid_int > 0)
		{
		continue;	
		}
	/* read application name */
	(void)read_fld(tran_tabl_ptr, slot_no, APPL_NAME, field);     
	(void)read_fld(tran_tabl_ptr, slot_no, APPL_RESOURCE, res_name);     
	/* start finding resource in resource table and load only one time */
	res_found = ISP_NOTFOUND;
	for (i=0; i < tot_resources; i++)
		{
		/* find out resource matching in resource definition table */
		if (strcmp(res_name, resource[i].res_no) == 0)
			{
			res_found = ISP_FOUND;
			break;
			}
		} /* for all resource */
	/* ignore the entry for reserve port */
	if(res_found == ISP_FOUND && strcmp(resource[i].res_usage, RESERVE_PORT)==0)
			res_found = ISP_NOTFOUND;
	if (res_found == ISP_FOUND)
		{
		file_name[0] = '\0';
		application[0] = '\0';
		switch(object)
			{
			case	TCP:
				if (find_application(SCHEDULER, i, resource[i].static_dest, "", file_name, group) != ISP_SUCCESS)
					{
					/* delete application if not busy */
					if ((atoi(status_str) == STATUS_IDLE || atoi(status_str) == STATUS_INIT) && (atoi(pid_str) != 0))
						{
						sprintf(log_buf, "Application %s (pid = %d) terminated not qualified for current instance", field, pid_int);
						resp_log(mod, REPORT_NORMAL, 3102, log_buf);
						(void)kill ((pid_t)atoi(pid_str), SIGTERM);
						continue;
						}
					sprintf(application, "%s", "N/A");
					}
				else
					{
					sprintf(application, "%s", file_name);
					}
				(void) update_application(i, slot_no, application);
				break;

			case	SNA:
				if (find_application(SCHEDULER, i, resource[i].static_dest, "", file_name, group) != ISP_SUCCESS)
					{
					/* delete application if not busy */
					if ((atoi(status_str) == STATUS_IDLE || atoi(status_str) == STATUS_INIT) && (atoi(pid_str) != 0))
						{
						sprintf(log_buf, "Application %s (pid = %d) terminated not qualified for current instance", field, pid_int);
						resp_log(mod, REPORT_NORMAL, 3103, log_buf);
						(void)kill ((pid_t)atoi(pid_str), SIGTERM);
						continue;
						}
					sprintf(application, "%s", "N/A");
					}
				else
					{
					sprintf(application, "%s", file_name);
					}
				(void) update_application(i, slot_no, application);
				break;
			default :
				break;
			} /* switch */		
		} /* if resource found */
	} /* for all slot */
return;
} /* update_shmem */
/*--------------------------------------------------------------
This routine gets the value of desired field from data record.
---------------------------------------------------------------*/
int get_field(buffer, fld_num, field)
const   char    buffer[];               /* data record */
int     fld_num;                        /* field # in the buffer */
char    field[];                        /* buffer to fill with the field name */
{
register        int     i;
int     fld_len = 0;                    /* field length */
int     state = OUT;                    /* current state IN or OUT */
int     wc = 0;                         /* word count */

field[0] = '\0';
if( fld_num < 0)
	{
        return (-1);
	}

for(i=0; i < (int)strlen(buffer); i++) 
        {
        if(buffer[i] == FIELD_DELIMITER || buffer[i] == '\n')
                {
                state = OUT;
                if(buffer[i] == FIELD_DELIMITER && buffer[i-1] == FIELD_DELIMITER)
                        ++wc;
                }
        else if (state == OUT)
                {
                state = IN;
                ++wc;
                }
        if (fld_num == wc && state == IN)
		{
                field[fld_len++] = buffer[i];
		}
        if (fld_num == wc && state == OUT)
		{
                break;
		}
        } /* for */

if (fld_len > 0)                                /*  for last field */
	{
	field[fld_len] = '\0';
	while(field[0] == ' ')
		{
		for (i=0; field[i] != '\0'; i++)
			field[i] = field[i+1];
		}
	fld_len = strlen(field);
	return (fld_len);                       /* return field length*/
	}
return (-1);                                    /* return error */
} /* get_field() */
/*--------------------------------------------------------------------------
application_instance_manager(): this routine update instance for application
----------------------------------------------------------------------------*/
int application_instance_manager(command, program_name, res_id)
int	command;			/* ADD, DELETE */
char	*program_name;
char	*res_id;
{
static	char	mod[]="application_instance_manager";
int	i, j, found = ISP_NOTFOUND;
char	appl_grp_name[MAX_GROUP];
char	tmp_pgm_name[MAX_PROGRAM];

for (i=0; i< tot_pgmrefs; i++)
	{
	/* find program(basename) in program reference */
	sprintf(tmp_pgm_name, "%s", program_name);
	if (strcmp(tmp_pgm_name, pgmref[i].program_name) == 0)
		{
		sprintf(appl_grp_name, "%s", pgmref[i].appl_grp_name);
		found = ISP_FOUND;
		break;
		}
	}	
if (found != ISP_FOUND)
	{
	/* instance may is dynamic manager */
	return (-1);
	}
found = ISP_NOTFOUND;
/* find  group in application reference */
for (j=0; j < tot_apprefs; j++)
	{
	if (strcmp(appl_grp_name, appref[j].appl_grp_name) == 0)
		{
		found = ISP_FOUND;
		break;
		}
	} /* for */	

if (found != ISP_FOUND)
	{
	if (database_hit == 1)
		{
		sprintf(log_buf, "Can't find program group <%s> in application reference table", appl_grp_name);
		resp_log(mod, REPORT_NORMAL, 3104, log_buf);
		}
	return (-1);
	}

switch(command)
	{
	case	ISP_ADD_INSTANCE:
		appref[j].curr_instance = appref[j].curr_instance + 1;		
		break;
	case	ISP_DELETE_INSTANCE:
		appref[j].curr_instance = appref[j].curr_instance - 1;		
		break;
	case	ISP_CHECK_MAX_INSTANCE:
		if (appref[j].curr_instance >= appref[j].max_instance)
			{
			sprintf(log_buf, "Command = %d, Maximum Instance reached : Program  >%s<, Resource = <%s>, Acct code = >%s<, Max instance = %d, Current Instance = %d", command, program_name, res_id, appref[j].acct_code, appref[j].max_instance, appref[j].curr_instance);
			resp_log(mod, REPORT_VERBOSE, 3105, log_buf);
			if (database_hit == 1)
				{
				sprintf(log_buf, "Can't start program <%s>, Resource = <%s>, Account code <%s>, Application group = <%s>, Maximum allowable instances(%d) are running", program_name, res_id, appref[j].acct_code, appref[j].appl_grp_name, appref[j].max_instance);
				resp_log(mod, REPORT_NORMAL, 3106, log_buf);
				}
			return (-1);
			}
		break;
	default:
		sprintf(log_buf, "application_instance_manager: Invalid command/parameter: %d", command);
		resp_log(mod, REPORT_NORMAL, 3107, log_buf);
		return (-1);
	} /* switch */
sprintf(log_buf, "Command = %d, Program  %s, Acct code = %s, Max instance = %d, Current Instance = %d <successful>.", command, program_name, appref[j].acct_code, appref[j].max_instance, appref[j].curr_instance);
resp_log(mod, REPORT_VERBOSE, 3108, log_buf);
return (0);
} /* application_instance_manager */
/*--------------------------------------------------------------------
set_resource_state_flag(): This routine set the resource state flag 
			  to fire application.
--------------------------------------------------------------------*/
int set_resource_state_flag(char *resource_state, char *state)
{
if (strcmp(resource_state, "STATIC") == 0)
	{
	sprintf(state, "%d", 1);
	}	
else if (strcmp(resource_state, "DYNAMIC") == 0)
	{
	sprintf(state, "%d", 2);
	}	
else
	{
	sprintf(state, "%d", 0);
	}	
return (0);
} /* set_resource_state_flag */
/**********************************************************************
resp_shutdown():
**********************************************************************/
void resp_shutdown()
{
sprintf(log_buf, "%s", "Responsibility received termination signal ...");
resp_log("resp_shutdown", REPORT_NORMAL, 3109, log_buf);
exit (0);
} /* resp_shutdwon */

/*-----------------------------------------------------------------
update_application(): update the application into shared memory
------------------------------------------------------------------*/
int	update_application(int res_index, int shm_index, char *pgm_name)
{
static	char	mod[] = "update_application";
char	appl_name[MAX_PROGRAM];
char	*ptr, *ptr1;

sprintf(appl_name, "%s", pgm_name);
if (strlen(appl_name) > MAX_APPL_NAME)
	{
	sprintf(log_buf, "Application name >%s<, The length of application name can't be > %d", appl_name, MAX_APPL_NAME);
	resp_log(mod, REPORT_NORMAL, 3110, log_buf);
	return (0);
	}
if (strcmp(appl_name, "N/A") != 0)		/* if application exists */
	{
	if (access(appl_name, R_OK|X_OK) != 0)
		{
		if (database_hit == 1)
			{
			sprintf(log_buf, "Application <%s> not found, errno = %d. Scheduling table entry (DNIS, ANI) = (%s, %s) is ignored.", appl_name, errno, resource[res_index].res_no, "N/A");
			resp_log(mod, REPORT_NORMAL, 3111, log_buf);
			}
		sprintf(appl_name, "%s", "N/A");
		}
	}
ptr = tran_tabl_ptr;
ptr += (shm_index*SHMEM_REC_LENGTH);
/* position the pointer to the field */
ptr += vec[APPL_NAME-1];			/* application start index */
ptr1 = ptr;
(void) memset(ptr1, ' ', MAX_APPL_NAME);
ptr += (MAX_APPL_NAME - strlen(appl_name));
(void) memcpy(ptr, appl_name, strlen(appl_name));
/* if application is in off status, turning it on */
appl_stat_arry[shm_index] = 0;
(void)write_fld(tran_tabl_ptr, shm_index, APPL_STATUS, STATUS_INIT);
return (0);
} /* update_application */
/*-----------------------------------------------------------------
find_pu_name(): find pu name from sna resource.
------------------------------------------------------------------*/
static	int	find_pu_name(res_name, pu_name)
char	*res_name;
char	*pu_name;
{
static	char	mod[] = "find_pu_name";
char	data[512];
char	pu[20];
FILE	*fp;
char	gateway_file[256];

//sprintf(gateway_file, "%s/3270/%s", isp_home, res_name);
sprintf(gateway_file, "%s/tn3270c/cdir/%s", isp_home, res_name);

		sprintf(log_buf, "Gateway file =  (%s)", gateway_file);
		resp_log(mod, REPORT_VERBOSE, 3036, log_buf);

if((fp = fopen(gateway_file, "r+")) == NULL)
	{
	sprintf(log_buf,"Unable to open file %s. errno = %d", gateway_file, errno);
	resp_log(mod, REPORT_NORMAL, 3040, log_buf);
	sprintf(pu_name, "%s", "BAD");
	return;
	}

		sprintf(log_buf, "Succesfully opened Gateway file =  (%s)", gateway_file);
		resp_log(mod, REPORT_VERBOSE, 3036, log_buf);

while(fgets(data, 80, fp) != NULL)
	{
	if(data[0] == '#')
		continue;
	sprintf(pu_name, "%s", data);
	pu_name[strlen(pu_name) - 1] = '\0';
	}

if(strlen(pu_name) == 0)
	sprintf(pu_name, "%s", "BAD");

fclose(fp);
return (ISP_SUCCESS);
} /* find_pu_name */
/*--------------------------------------------------------------------
kill_all_app(): This module kills all application that are running.
----------------------------------------------------------------------*/
static	int	kill_all_app()
{
register	int	i;
(void) fill_vecs(tran_tabl_ptr, appl_pid);
for(i=0; i< tran_proc_num; i++)
	{
	if (appl_pid[i] != 0)
		(void) kill((pid_t)appl_pid[i], SIGTERM);
	}
return (0);
} /* kill_all_app */
/*--------------------------------------------------------------------------
Check for update table request.
---------------------------------------------------------------------------*/
int check_update_table_request()
{
static	char	mod[] = "check_update_table_request";
static	time_t	last_time_modified;
char	reload_path[256];   
struct  stat    file_stat;

sprintf(reload_path, "%s/%s", lock_path, RELOAD_FILENAME);
/* check if reload file exists if not create one */
if (access(reload_path, R_OK) != 0)
	{
	if (creat(reload_path, 0644) != 0)
		{
		sprintf(log_buf, "Failed to create new reload file %s, errno = %d", reload_file, errno);
		resp_log(mod, REPORT_NORMAL, 3061, log_buf);
		return (-1);
		} 
	}
if (stat(reload_path, &file_stat) == 0)
	{
	/* check if last modified time is changed */
	if (file_stat.st_mtime != last_time_modified)
		{
		last_time_modified = file_stat.st_mtime;
		return (0); 	/* last modification time change reload found */
		}
	} /* stat */
return (-1);
} /* check_update_table_request */
/*----------------------------------------------------------------------------
load_parameter_configuration(): This routine load configuration file for server.
-----------------------------------------------------------------------------*/
void load_parameter_configuration()
{
char	buf[100];
char	param[30];
char	cfg_file[256];
FILE	*cfp;
char	*ptr;

switch(object)
	{
	case	TCP:
		sprintf(cfg_file, "%s/%s", table_path, TCP_PARM_CONFIG);
		break;
	case	SNA:
		sprintf(cfg_file, "%s/%s", table_path, SNA_PARM_CONFIG);
		break;
	default :
		return;
	} /* switch */
cfp = fopen(cfg_file, "r");
if (cfp == NULL)
	{
	sprintf(log_buf,"Unable to open <%s>. errno=%d. Setting default parameters.", cfg_file, errno);	
	resp_log("load_parm_config", REPORT_NORMAL, 3040, log_buf);
	return;
	}
while(fgets(buf, sizeof(buf), cfp) != NULL)
	{
	buf[(int)strlen(buf)-1] = '\0';
	/* set heartbeat parameters */
	if (strstr(buf, "HeartBeatInterval") != NULL)
		{
		sscanf(buf, "%[^=]=%s", param, heartbeat_interval);
		}
	/* set network services */
	if (strstr(buf, "NetServices") != NULL)
		{
		sscanf(buf, "%[^=]=%s", param, NetworkStatus);
		}
	} /* while not eof */
fclose(cfp);
return;
}  /* load_parameter_configuration */
/*-----------------------------------------------------------------------------
check_kill_app():This routine check for all application dead and does cleanup.
-----------------------------------------------------------------------------*/
static	void check_kill_app()
{
static	char	mod[] = "check_kill_app";
register	int	appl_no, resno;
char	pid_str[10];
char	res_str[30];
int	int_pid;

for(appl_no=0; appl_no<tran_proc_num; appl_no++)
	{
	/* read pid field */
	(void)read_fld(tran_tabl_ptr, appl_no, APPL_PID, pid_str);     
	int_pid = atoi(pid_str);
	if (kill(int_pid, 0) == -1)	/* check application exists */
		{
		if (errno == ESRCH) /* No process */
			{
			/* application cleanup function */
			(void) cleanup_app(int_pid, appl_no);
			(void)read_fld(tran_tabl_ptr, appl_no, APPL_RESOURCE, res_str);     
			/* reserve resource no more available to application */
			/* make resource free */
			for (resno = 0; resno <= MAX_PROCS; resno ++)
				{
				/* find the resource allocated to this proc */
				if (res_status[resno].pid  == atoi(pid_str) && strcmp(res_str, RESERVE_PORT) == 0)
					{
					res_status[resno].status = FREE; 
					res_status[resno].pid = 0;	
					break;
					} /* if pid same */
				} /* for */
			} /* if process not exists */
		} /* kill */
	} /* for */

/* following code is for network services */
if (start_net_serv == 1)	/* if network service exists */
	{
	if (kill(net_serv_pid, 0) == -1)     /* check application exists */
		{
		if (errno == ESRCH) /* No process */
			{ /* restart network services */
			sprintf(log_buf, "Network services for %s server stopped PID = %d", object_name, net_serv_pid);
			resp_log(mod, REPORT_NORMAL, 3112, log_buf);
			start_netserv();
			}
		}
	} /* if network services */

return;
} /* check_kill_app */
/*-----------------------------------------------------
start_netserv() : start network server for my server.
------------------------------------------------------*/
int	start_netserv()
{
int	ret;
char	net_serv_name[64];
char	net_serv_path[256];
char	mod[]="start_netserv";

if (start_net_serv != 1)			/* netwok not co-resident */
	return (0);

switch(object)
	{
	case	TCP:
		sprintf(net_serv_path, "%s/%s", exec_path, TCP_NET_SERV);
		sprintf(net_serv_name, "%s", TCP_NET_SERV);
		break;
	case	SNA:
		sprintf(net_serv_path, "%s/%s", exec_path, SNA_NET_SERV);
		sprintf(net_serv_name, "%s", SNA_NET_SERV);
		break;
	default:
		sprintf(log_buf, "Invalid server identifier: %d.", object);	
		resp_log(mod, REPORT_NORMAL, 3014, log_buf);
		return (ISP_FAIL);
	} /* switch */
/* fire network services */
if ((net_serv_pid = fork()) == 0)
	{
	ret = execl(net_serv_path, net_serv_name, NULL);
	if (ret == -1)
		{
		sprintf(log_buf, "Unable to start network services for %s server execl(path=<%s>,  program=<%s>) system call failed, errno = %d", object_name, net_serv_path, net_serv_name, errno);
		resp_log(mod, REPORT_NORMAL, 3039, log_buf);
		}
	exit (0);
	}
sprintf(log_buf, "Starting Network services %s server started PID = %d", object_name, net_serv_pid);
resp_log(mod, REPORT_NORMAL, 3113, log_buf);
return (0);
} /* start_netserv */
/*-----------------------------------------------
check_net_config() : check network if server is
		     configuration for network.
-----------------------------------------------*/
void	check_net_config()
{
if (strcmp(NetworkStatus,"ON") == 0) 
	start_net_serv = 1;
else
	start_net_serv = 0;
} /* check_net_config */

/*---------------------------------------------------------------------------- 
This function checks the mac address of the machine, then looks for and
counts the requested feature. If the feature is not found, and turnkey_allowed 
is set to 1, the then "TURN" is appended to the requested feature and a check 
is done to see if that feature is licensed. If so, the turkey_found is set to
1 and the function returns 0 (success).
----------------------------------------------------------------------------*/
static int arcValidLicense( char *feature_requested, int turnkey_allowed, int  *turnkey_found, int  *feature_count, char *msg)
{
	int rc;
	int count, mac_tries;
	char feature[64];
	char err_msg[512];

	/* Initialize return values */
	*turnkey_found = 0;
	*feature_count = 0;
	strcpy(msg,""); 


#ifdef GETTING_MAC_ADDRESS
eat it and die
	/* Get the MAC address; try up to ten times. */
	mac_tries=0;
	while (mac_tries < 10)
	{
		mac_tries++;
		rc=lic_get_mac_address("local_host", feature, err_msg);
		if (rc == 0) break;
		sleep(5);
		fprintf(stderr,
		"Attempt %d: Failed to obtain license information.\n", 
		mac_tries);
		fflush(stderr);
	}
	if (rc != 0) 
	{
		strcpy(msg,err_msg);
		return(-1);
	}
#if 0
	
	/* Treat the MAC address as a feature */
	rc = lic_get_license(feature, err_msg);
	
	if (rc != 0)
	{
		strcpy(msg,err_msg);
		return(-1);
	}
#endif
#endif

#if 0
	strcpy(feature,feature_requested);
	rc = lic_get_license(feature, err_msg);
	switch(rc)
	{
	case 0:
		strcpy(msg,"License granted (%s).");
		break;
	case -1:
		if (!turnkey_allowed)
		{
			strcpy(msg, err_msg);
			return(-1);
		}
		sprintf(feature,"%sTURN",feature_requested);
		rc = lic_get_license(feature, err_msg);
		if (rc == 0)
		{
			sprintf(msg,"Turnkey license granted (%s).", feature);
			*turnkey_found = 1;
		}
		else
		{
			sprintf(msg,"Failed to get license (%s).", 
							feature_requested);
			return(-1);
		}
		break;
	default:
		sprintf(msg,"Unknown license return code %d.", rc);
		return(-1);
	}

	rc = lic_count_licenses(feature, &count, err_msg);
	if (rc == -1)
	{
		strcpy(msg,err_msg);
	 	return(-1);
	}
	*feature_count = count;
#endif
	*feature_count = 100;
	return(0);
}

/*----------------------------------------------------------------------------
This routine loads the appref & pgmref tables from the license file.
----------------------------------------------------------------------------*/
int load_appref_pgmref_from_license()
{
	int rc;
	int fcount=0;
	FILE *fp;
	char	opcode[50];
	char	dummy[128];
	char	feature[128];
	char 	inputbuf[256];
	char 	license_file[256];
	char 	mod[]="load_appref_pgmref";
	int 	dummy2;

	tot_apprefs = 0;
	tot_pgmrefs = 0;

#if 0
	rc = get_license_file_name(license_file, log_buf); 
	if (rc != 0)
	{
		resp_log(mod, REPORT_NORMAL, 3063, log_buf);
		return(ISP_FAIL);
	}

	if ((fp = fopen(license_file, "r")) == NULL)
        {
       	sprintf(log_buf, "Unable to open <%s> for reading. errno=%d.",
							license_file, errno);
		resp_log(mod, REPORT_NORMAL, 3040, log_buf);
		return(ISP_FAIL);
        }

	while(fgets(inputbuf, sizeof(inputbuf), fp))
	{
		opcode[0] = '\0';
		feature[0] = '\0';
      	inputbuf[(int)strlen(inputbuf)-1] = '\0';
		if (strlen(inputbuf) == 0) continue;
		if (inputbuf[0] == '\t' || 
			inputbuf[0] == '#'  ||
			inputbuf[0] == ' ') continue;

		sscanf(inputbuf,"%s %s %s %s %s %d", 
		opcode, feature, dummy, dummy, dummy, &fcount);
		if ( strcmp(opcode, "FEATURE") != 0 ) continue;
		if ( feature[0] == 'T' && feature[1] == 'K')
		{
/* new code */
		rc = lic_get_license(feature,log_buf); 
		if (rc != 0)
		{
			sprintf(log_buf,"Cannot load <%s>. License violation.",
					&feature[2]);
			resp_log(mod, REPORT_NORMAL, 3065, log_buf);
			continue;
		}		
/* end code */
			sprintf(appref[tot_apprefs].appl_grp_name, "%s", 
					&feature[2]);
			sprintf(appref[tot_apprefs].acct_code, "%s", 
					&feature[2]);
			appref[tot_apprefs].max_instance = fcount;
			tot_apprefs++;
			sprintf(pgmref[tot_pgmrefs].program_name, "%s", 
					&feature[2]);
			sprintf(pgmref[tot_pgmrefs].appl_grp_name, "%s", 
					&feature[2]);
			tot_pgmrefs++;
		}
   	} 
	pclose(fp);
#endif
	return(ISP_SUCCESS);
} 


/*----------------------------------------------------------------------------
Get the full pathname of the license file.
Note: This function is a duplicate of an internal function used by the
licensing mechanism. I had to include it since I could not link with the 
original.  gpw 7/26/99
----------------------------------------------------------------------------*/	
static int get_license_file_name( char *license_file, char *err_msg)
{
	static int got_file_already=0;
	static char hold_license_file[256];
	char *ispbase;
	char base_env_var[]="ISPBASE";

	if (got_file_already)
	{
		strcpy(license_file, hold_license_file);
		strcpy(err_msg,"");
		return(0);
	}
	
	if ((ispbase = getenv(base_env_var)) != NULL)
	{
		got_file_already=1;
		sprintf(hold_license_file,"%s/Global/Tables/license.dat", 
					ispbase);
		strcpy(license_file, hold_license_file);
#ifdef DEBUG
		fprintf(stdout,"The file is '%s'\n", license_file); fflush(stdout);
#endif
		strcpy(err_msg,"");
		return(0);
	}
	else
	{
		sprintf(err_msg,"Unable to get evironment variable %s.",
			base_env_var); 
		return(-1);
	}
}
/*---------------------------------------------------------------------------
check to see if the application is valid turnkey application
----------------------------------------------------------------------------*/
checkValidTurnkeyApp()
{
static	char	mod[] = "checkValidTurnkeyApp";
static	int	counter=0;
int	handshakeArray[MAX_PROCS];
int	statusArray[MAX_PROCS];
char	program_name[256];
int	i;

counter++;
if(counter > 3)
	{
	counter = 0;
	(void)read_arry(tran_tabl_ptr, APPL_STATUS, statusArray);
	(void)read_arry(tran_tabl_ptr, APPL_FIELD5, handshakeArray);
	for(i=0; i<tran_proc_num; i++)
		{

/*	fprintf(stdout,"gpwDEBUG: slot=%d status=%d handshake=%d\n", 
		i, statusArray[i], handshakeArray[i]); fflush(stdout);  */
		if((statusArray[i] == STATUS_BUSY) && (handshakeArray[i] == 1))
			{
			if(appl_pid[i] != 0)
				{
				(void)read_fld(tran_tabl_ptr, i, APPL_NAME, program_name);     
				(void) kill((pid_t)appl_pid[i], SIGTERM);
				sprintf(log_buf, "Killing application <%s> (pid %d) for license violation.", 
				program_name, appl_pid[i]);
				resp_log(mod, REPORT_NORMAL, 3114, log_buf);
				}
			}
		}
	}
return(0);
}

resp_log(char *mod, int mode, int msgid, char *msg)
{
	LogARCMsg(mod, mode, "0", "RES", resp_name, msgid, msg);  
}
/*------------------------------- eof() --------------------------------------*/
