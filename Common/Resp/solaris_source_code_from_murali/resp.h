/*----------------------------------------------------------------------------
Program:	resp.h
Purpose:	Header file resp.c.
Author : 	Mahesh Joshi.
Date   : 	11/06/96 
Update:		02/28/96 G. Wenzel added NET_STR, NET, NET_DIR, NET_PARM_CONFIG 
Update:		03/02/97 M. Bhide added dynmgr_pid array 
Update:		09/19/98 M. Bhide Changed MAX_PROC from 110 to 125 to 130 
Update:		04/19/99 M. Bhide Changed MAX_PROC from 130 to 200 
Update:		04/24/99 mpb Added DCHAN. 
Update:		06/18/99 gpw Added GLOBAL_DIR, global_exec_path
Update:		08/31/99 gpw removed cfgdef (def file) from tel_config_file[]
Update: 10/28/99 mpb Added MAX_DEAD_PROCS. 
Update: 01/28/02 mpb Added SKIP_PORT. 
Update: 02/09/02 mpb Added STATIC_APPL. 
-----------------------------------------------------------------------------*/
#define DYN_MGR			"dyn_mgr"
#define TELECOM_STR		"TEL"
#define TCP_STR			"TCP"
#define SNA_STR			"SNA"
#define WSS_STR			"WSS"
#define NET_STR			"NET"
#define RELOAD_FILENAME		"reload"
#define	WILD_DNIS		"*"

#define	TEL_NET_SERV		"isp_telserv"
#define	TCP_NET_SERV		"isp_tcpserv"
#define	SNA_NET_SERV		"isp_snaserv"

#define	SNA_ACCESS		"access"
#define	SNA_ACCESS_PATH		"/usr/bin/access"

/* object supported */
#define TEL			1
#define	TCP			2
#define	SNA			3
#define	WSS			4
#define	NET			5

#define	RESOURCE_NOT_ATTACH	1000
/* heartbeat interval defines */
#define ISP_MIN_HBEAT		5
#define ISP_MAX_HBEAT		61
#define ISP_DEFAULT_HBEAT	10
#define MAX_FIRE_LAPS_SEC	300	/* 5 min */
#define MAX_NUM_APPL_FIRE	25	/* maxium number of time application */
					/* got fired in MAX_FIRE_LAPS_SEC */
#define MAX_REQUEST_PROCESS	10	/* maximum request to be processed  */
					/* by dynamic manager one time */

#define DEFAULT_ALARM		5
#define SHUTDOWN_MODULE		"./isp_stopresp"

/* directory path */
#define TELECOM_DIR		"Telecom"
#define GLOBAL_DIR		"Global"
#define SNA_DIR			"SNA"
#define TCP_DIR			"TCP"
#define WSS_DIR			"WSS"
#define NET_DIR			"Network"
#define TABLE_DIR		"Tables"
#define EXEC_DIR		"Exec"
#define UTL_DIR			"Utilities"
#define OPER_DIR		"Operations"
#define LOCK_DIR		"locks"
#define TEL_PARM_CONFIG		".TEL.cfg"
#define TCP_PARM_CONFIG		".TCP.cfg"
#define SNA_PARM_CONFIG		".SNA.cfg"
#define NET_PARM_CONFIG		".NET.cfg"

/* reserve port usage */
#define RESERVE_PORT		"RESERVED"
#define SKIP_PORT		"SKIPPORT"
#define DCHAN			"DCHANNEL"
#define TWO_WAY_PORT		"TWOWAY"
#define	RESERVE			6

#define	FIELD_DELIMITER		'|'
#define IN			0
#define	OUT			1
#define MAX_SIZE		1000		/* maximum table size in rec */
#define MAX_RESOURCE		400		/* maximum table size in rec */
#define MAX_PGMREF		1000		/* maximum table size in rec */
#define MAX_APPREF		1000		/* maximum table size in rec */
#define	MAX_PROCS		200		/* Maxmum # processes */
#define MAX_PROGRAM		256		/* Max program name */
#define MAX_APPLICATION		100		/* maxium application name */
#define MAX_CODE		100		/* max account code */
#define MAX_MACHINE		50		/* max machine name */
#define MAX_GROUP		50		/* max group name */
#define MAX_APPL_NAME		50		/* max application name in */
						/* shared memory */
#define	MAX_DEAD_PROCS		4 * MAX_PROCS	/* Maxmum # dead processes */

/* following are current state of application */
#define STOP_STATE		0	/* application is in stop state */
#define	RUN_STATE		1	/* application currently running */

/* scheduling field number and field array */
#define SERVER_NAME		1
#define MACHINE_NAME		2
#define DESTINAION		3
#define ORIGINATION		4
#define PRIORITY		5
#define RULE			6
#define START_DATE		7
#define STOP_DATE		8
#define START_TIME		9
#define STOP_TIME		10
#define PROGRAM_NAME		11
#define MAX_SCHEDULE_FIELD	11

/* resource field number and field array */
#define RESOURCE_NO		1
#define RESOURCE_TYPE		2
#define RESOURCE_STATE		3
#define RESOURCE_USAGE		4
#define STATIC_DEST		5
#define MAX_RESOURCE_FIELD	5

/* Application group reference field number and field arry */
#define APPL_GRP_NAME	1
#define ACCT_CODE	2
#define	MAX_INSTANCES	3
#define MAX_APPREF_FIELD	3

/* Program reference field number and field arry */
#define	PROGRAM		1
#define APPL_GRP	2
#define MAX_PGMREF_FIELD	2

/* start access in fore / back ground */
#define ISP_ACCESS_FORE	1
#define ISP_ACCESS_BACK	2

/* Return Code */
#define ISP_SUCCESS	0
#define ISP_FAIL	-1

/* define for dynamic manager application database */
#define ISP_PROGRAM_ADD		1
#define ISP_PROGRAM_DELETE	2
#define ISP_PROGRAM_FIND	3

/* define for application/resource found */
#define ISP_FOUND	1
#define ISP_NOTFOUND	0

#define ISP20_SUCCESS	0
#define ISP20_NOAPPL	1
#define ISP20_FAIL	-1
#define INVALID_TOKEN	-2

/* following are command for find command */
#define		PORT_APPL	0	/* application based on port */
#define		TOKEN_APPL	1	/* application based on 2 token */
#define		TOKEN1_APPL	2	/* application based in token 1 only */
#define		TOKEN2_APPL	3	/* application based in token 2 only */
#define		ALL_APPL	4	/* All applications eligible to fire */
#define		STATIC_APPL	5	/* Static applications eligible to fire */

/* Message queue */
#define		MSG_COMMAND	3

/* Requester for finding application */
#define	SCHEDULER		4
#define	DYNAMIC_MANAGER		5

/* command to update application instances */
#define ISP_ADD_INSTANCE	1
#define ISP_DELETE_INSTANCE	2
#define ISP_CHECK_MAX_INSTANCE	3

/* debug mode */
#define ISP_DEBUG_NORMAL	1	/* log errors only */
#define ISP_DEBUG_DETAIL	2	/* log  details */
#define ISP_DEBUG_SHMEM		3	/* log shmem operations */

/* isdn type */
#define NSF_SDN_SERVICE 	1
#define	NSF_MEGA800_SERVICE 	2
#define	NSF_MEGACOM_SERVICE 	3
#define	NSF_ACCUNET_SERVICE 	4
#define	NSF_LD_SERVICE 		5
#define	NSF_INTL800_SERVICE 	6
#define	NSF_MULTIQUEST_SERVICE  7

/* update activce file string */
#define UPDATE_REQUEST_FILE	"isp_rdb."

/* Teravox running lookup string , sfmgr - speech file manager */
#define	TVOX_LOOKUP	"sfmgr"

/* resource status to use resource dynamic */
#define NOT_FREE	0
#define FREE		1


/* schedule field strings */
static char	*schedule_field_str[] = 
	{
	""
	"SERVER_NAME", 
	"MACHINE_NAME", 	
	"DESTINAION", 
	"ORIGINATION", 
	"PRIORITY", 
	"RULE", 
	"START_DATE", 
	"STOP_DATE", 
	"START_TIME", 
	"STOP_TIME", 
	"PROGRAM_NAME"
	};

/* resource field string */
static	char	*resource_field_str[] =
	{
	"", 
	"RESOURCE_NO", 
	"RESOURCE_TYPE", 
	"RESOURCE_STATE", 
	"RESOURCE_USAGE", 
	"STATIC_DEST"
	};
	
/* appref field string */
static	char	*appref_field_str[] =
	{
	"", 
	"Application Group Name", 
	"Accounting Code", 
	"Maximun Instances"
	};

/*  program reference strings */
static	char	*pgmref_field_str[] =
	{
	"", 
	"Program Name", 
	"Application Group Name"
	};

/*------------------
Globles structure
------------------*/
struct	resource
	{
	char	res_no[30];		/* resource name - port/resno/luname */
	char	res_type[20];		/* ANALOG / ISDN */
	char	res_state[20];		/* STATIC / DYNAMIC / DYN_MGR */
	char	res_usage[20];		/* INBOUND / OUTBOUND / NOT_USED  */
	char	static_dest[20];	/* matching token/phone number */
	char	res_qualifier[20];	/* For SNA only - PU NAME */
	};

struct	schedule
	{
	int	index;			/* current index in array */
	int	pid;			/* process id of program if running */
	int	state;			/* mode of application - run, stop */
	char	srvtype[10];		/* 3 character server name TEL, TCP */
	char	machine[MAX_MACHINE];	/* machine name of application */
	char	destination[20];	/* DNIS - token 1 */
	char	origination[20];	/* ANI - token 2 */
	int	priority;		/* order in which record evaluated */
	int	rule;			/* the date rule to be used */
	char	start_date[20];		/* start date of application yymmdd */
	char	stop_date[20];		/* end date of application yymmdd */
	char	start_time[20];		/* start time of application hhmmss */
	char	stop_time[20];		/* end time of application hhmmss */
	char	program[MAX_PROGRAM];	/* applications name */
	char	appl_grp_name[MAX_GROUP]; /* application group name */
	char	acct_code[MAX_CODE]; 	/* application name */
	int	max_instance;		/* maxium executing */
	};

struct	pgmref
	{
	char	program_name[MAX_PROGRAM];	/* name of program */
	char	appl_grp_name[MAX_GROUP];	/* program group */
	};

struct	appref
	{
	char	appl_grp_name[MAX_GROUP];	/* program group */
	char	acct_code[MAX_CODE];		/* accounting code */
	int	curr_instance;			/* current instance */
	int	max_instance;			/* maximum number of instances*/
	};

/* structure to keep track of instances */
struct	instance
	{
	char	acct_code[MAX_CODE];		/* application group */
	int	max_inst;			/* maxium allowable instance */
	int	curr_inst;			/* maximum number of instances*/
	};

/* structure to maintain the list of applications exec by dynamic manager */
struct  dynamic_manager_application 
	{
	int	dyn_mgr_pid;
	char	program[MAX_PROGRAM];
	};

static	struct 	resource 	resource[MAX_RESOURCE];
static	struct	schedule	schedule[MAX_SIZE];
static	struct	appref		appref[MAX_APPREF];
static	struct	pgmref		pgmref[MAX_PGMREF];
static	struct	instance	instance[MAX_SIZE];
static	struct	dynamic_manager_application	dyn_mgr_appl[MAX_PROCS];

struct  msqid_ds        mesg_stat;		/* message queue stats */
extern	int	errno;
/* total entries in resource/schedule/appref/pgmref tables */
static	int	tot_resources=0;
static	int	tot_schedules=0;
static	int	tot_apprefs=0;
static	int	tot_pgmrefs=0;

static	char	path[1024];
static	char	appl_path[1024];	/* full path of application */
static	char	heartbeat_interval[10];
static	char	DefaultLang[20];
static	char	NetworkStatus[20];		/* ON / OFF */
static	char	ResMgrStatus[20];		/* ON / OFF *//*added 062498*/
static	char	AccessMode[20];			/* BACK/FORE */
static	char	diag_flag[20];
/* Global variables */
static	long	GV_shm_key;
static	long	GV_que_no;
static	long	GV_dyn_mgr_que;
/* database hit variable get set to 1 only while loading database
   so message will get logged only after database reload */
static	int	database_hit = 1;
/**************************************************
following are configuration files need for ISP
**************************************************/
/* Telecom Server configuration files */
static 	char	*tel_config_file[] = 
	{	
	"ResourceDefTab",
	"appref",
	"pgmreference",
	"schedule",
	"analogcfg",
	"isdncfg",
	".TEL.cfg"
	};
/* total files */
static int	tel_tot_tables = 
	sizeof(tel_config_file) / sizeof(tel_config_file[0]);

/* SNA Server Configuration files */
static	char	*sna_config_file[] = 
	{
	"ResourceDefTab",
	"appref",
	"pgmreference",
	"schedule",
	".SNA.cfg"
	};

static 	int sna_tot_tables = 
	sizeof(sna_config_file) / sizeof(sna_config_file[0]);

/* TCP/IP Server Configuration files */
static	char	*tcp_config_file[] = 
	{
	"ResourceDefTab",
	"appref",
	"pgmreference",
	"schedule",
	".TCP.cfg"
	};
static 	int	tcp_tot_tables = 
	sizeof(tcp_config_file) / sizeof(tcp_config_file[0]);

/* WSS Server Configuration files */
static	char	*wss_config_file[] = 
	{
	"ResourceDefTab",
	"appref",
	"pgmreference",
	"schedule",
	".WSS.cfg"
	};
static	int	wss_tot_tables = 
	sizeof(wss_config_file) / sizeof(wss_config_file[0]);


static	int	object;
static	char	object_code[10];
static	char	object_name[20];
static	char	object_machine_name[MAX_MACHINE];

static	int	dyn_mqid;
static	long	num_of_secs;		/* no of seconds from time() call */
static	int	ret_code;
static	int	appl_stat_arry[MAX_PROCS]; /* to store Status of application */
static	int	appl_proc_no[MAX_PROCS];	/* application no */
static	int	appl_pid[MAX_PROCS];	/* to store the process ids of appl */
static	int	dynmgr_pid[MAX_PROCS];	/* to store the proc ids of dynamic mgr */
static 	int	tran_proc_num;		/* number of application */
static 	int	pid_dead;		/* proc_id of the process dead */
static 	int	access_pid=0;		/* access pid */

static	int dead_application[MAX_DEAD_PROCS];  /* store the killed appli. pid */
static	int dead_appl_stat[MAX_DEAD_PROCS];  /* store status of killed appli. */

static	int 	count[MAX_PROCS];	/* number of times process exec */
static	char	schedule_tabl[256];
static	char	resource_tabl[256];
static	char	appref_tabl[256];
static	char	pgmref_tabl[256];
static	char	table_path[256];
static	char	global_exec_path[256];
static	char	exec_path[256];
static	char	isp_base[256];
static	char	isp_home[256];
static	char	lock_path[256];
static	char	log_buf[1024];
static	char	resp_name[256];
static	char	reload_file[256];   
static	char	*base_path;

/*-----------------------------------------------------
following are global variables to fire the application
-------------------------------------------------------*/
static	char    GV_DialSequence[20];
static	char    GV_PreBrtr[20];
static	char    GV_PreBrtrSub[20];
static	char    GV_PostBrtrAns[20];
static	char    GV_PostBrtrNotAns[20];
static	char    GV_ISDN_TYPE[20];
static	char    __my_area_code[10];
static	char    __my_call_dial[10];
static	char    __long_dist_dial[10];
static	char    __international_dial[10];
static	char     __flash_length[10];
static	char     rlt_flag[10];		/* release link trunk */
static	int    GV_ISDNType;
static	int    start_net_serv = 0;	/* nework services start - on/off */
static	int    net_serv_pid = 0;	/* nework services pid */
static	int    start_ResMgr = 0; /* ResourceMgr start on/offadded on 062498 */
static	int    resmgr_pid = 0;	/* ResourceMgr pid added on 062498*/

struct	resource_status
		{
		int	status;
		int	pid;
		};
/* resource status free/in use */
static	struct    resource_status	res_status[MAX_PROCS];    
int	res_attach[MAX_PROCS];

/* structure to keep track of alarm handler updates */
struct	alarm_status
	{
	int	startPort;		/* Start Port */
	int	endPort;		/* end port */
	char	status[5];			/* ON/OFF */
	};
static	struct	alarm_status	alarmStatus[20];
/********************************** eof() ********************************/
