/*-----------------------------------------------------------------------------
Program:	ArcIPResp.c
Purpose: 	Responsibility for IP-IVR product.
Author:		Madhav Bhide.
Date:		10/26/00.
Update:		11/03/00 gpw commented out looking for OBJDB not needed
Update:		12/04/00 moved defines of REPORT_NORMAL, etc. to ArcIPResp.h
			 just so they are defined in fewer places.
Update:		03/05/01 mpb Added code for handling static ports.
Update:	03/27/01 mpb In cleanupApp routine changed STATUS_KILL to INIT.
Update:	04/05/01 apj For OCServer pass cdf file name in -U option.
Update:	05/30/01 mpb B4 starting New DynMgr, kill all apps with SIGKILL.
Update:	07/12/01 apj Added ringEventTime handling.
Update:	08/07/01 mpb Notify DM about max number of ports.
Update:	08/16/01 apj In writeToDynMgr, create the fifo if we don't have it.
Update:	08/20/01 mpb Changed couple of messages from normal to verbose.
Update:	01/15/02 apj Changed '-p'->'-z' and '-c'->'-p'.
Update:	01/15/02 apj Added '-P' and -y.
Update:	09/05/03 mpb Added STATIC changes in cleanupApp.
Update:	10/06/03 apj Removed debug message.
Update:	12/08/03 mpb Replaced ISDN with IP.
Update:	01/12/04 mpb Added respPidFile.
Update: 03/08/2004 mpb Added code to start VXML2 Browser.
Update: 03/31/2004 mpb Added code to read msgs from VXML2 Browser.
Update: 08/25/04 djb Changed sys_errlist strerror(errno).
Update: 12/09/04 mpb  Added printfs for ADD & DELETE instances.
Update: 12/27/04 mpb  Added code to update shared memory with dynmgr name &
pid, in StratUpDynMgr routine.
Update: 12/27/04 mpb  Commented write_fld for STATUS_APPL.
Update: 07/18/05 mpb  Changed OCSMgr from static to dynamic.
Update: 10/06/05 mpb  Copied StartUpDynMgr, shouldIStartDM, checkDynMgr
writeToDynMgr from bumpy.
Update: 10/06/05 mpb  Added to code to start and check ArcSipRedirector process.
Update: 11/02/05 mpb  For checkAndStartOCSApp routine, making sure that correct
dynamic manager gets the message.
Update: 11/06/05 djb  Added new floating licensing logic.
Update: 11/16/05 mpb  In checkHeartBeat routine killing the app even if state
is IDLE, if pid is not dynamic manager pid..
Update: 11/16/05 mpb  In cleanupApp routine unlink responseFifo..pid.
Update: 11/23/05 mpb Added ability to start 4 instances of arcVXML is 192 ports.
Update: 11/23/05 ddn Send DMOP_REREGISTER to all SDMs if Redirector restarts
Update: 02/16/06 mpb In StartUpDynMgr changed sleep(2) to util_sleep(0, 250).
Update: 02/16/06 mpb If Call manger goes away, make all the slots free, after
the applications for that dynamic manager are killed.
Update: 05/22/06 ddn Re-initialize GV_CurrentDMPid in StartUpDynMgr (tmon issue)
Update: 06/21/06 mpb pid and port were switched in log message in cleanupApp routine.
Update: 09/18/06 ddn Check for errno == 11 for starting ocs app.
Don't delete cdf file if writeToDynMgr fails with errno 11.
Update: 10/31/06 ddnmpb Do fclose in getAppToAppInfoFromCDF
Update: 11/01/2006 djb MR#1666: modified licensing logic in sRequestLicense.
Update: 02/28/2007 mpb MR#1814: modified launchApp routine to adjust dynmgr number.
Update: 05/02/2007 mpb MR#1813: Divide the available licenses by half if multiple
responsibilities are running.
Update: 05/29/2007 mpb MR#1813: Multiple responsiblity logic corected.
Update: 06/05/2007 djb Added handleLicenseFailure() routine to
perform a graceful shutdown.
Update: 10/12/2007 ddn If hbeat_interval is -1, dont check heart bit
Update: 01/03/2008 ddn MR#2078: Added global struct gInstanceStatus
Update: 02/29/2008 jms changed SIP_redirection to SipRedirection
Update: 04/02/2008 djb MR #2182. Moved fclose in updateTryCount()
updatelastAttempt() routines.
Update: 04/18/2008 ddn Added VXML1, VXML2, VXI in startOCSApp
Update: 04/23/2008 djb Added licensing.
Update: 08/13/2009 ddn MR#2697  processRESOP_VXML_PID Don't add app instance again if OCS
Update: 10/06/2009 ddn Send DMOP_REREGISTER to all SDMs based on ID.
Update: 12/09/2009 djb MR #2765; No longer cores on a license failure.
Update: 09/17/2010 ddn Return -3 and take no action if OCS resource is busy for getFreePort
Update: 09/29/2010 djb Clean up log messages.
Update: 08/14/2013 djb Added GV_FromDMFifo to launchApp parameters
Update: 09/10/2013 djb Added network transfer code
Update: 06/19/2014 djb MR 4325 added logic to CANTFIREAPP - licenses - 503
Update: 08/07/2014 djb MR 4337 returned -1 from launchApp when app does not exist
Update: 08/07/2014 djb MR 4325 uncommented tot_resources/Licensed_Resources
Update: 03/10/2015 jms Modified Resp to run both the sip proxy and the Redirector if told to do so. 
Update: 06/29/2015 djb MR 4451 - Removed VIVINT_BYPASS ifdefs.
Update: 10/28/2015 djb MRs 4509 and 4510
Update: 05/20/2015 djb MRs 4535, 4591 - start up of SR and TTS
Update: 03/15/2017 djb MR 3707  
-----------------------------------------------------------------------------*/
#include "ISPSYS.h"								  /* Shared memory/Message Queue id */
#include "BNS.h"								  /* shared memory variables */
#include "ArcIPResp.h"							  /* global variables definations */
#include "shm_def.h"							  /* shared memory field information */
#include <license.h>
#include "lm_code.h"
#include <ocmHeaders.h>
#include <ocmNTHeaders.h>
#include "AppMessages.h"						  /* Common msgs between Resp,DM,APIs. */
#include "TEL_LogMessages.h"                          /* Common msgs between Resp,DM,APIs. */
#include <regex.h>
#include <sys/poll.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>

int gRespId = -1;
int gArcVXMLRequestFifoFd;
int gArcVXML2RequestFifoFd[MAX_PROCS/48];
int gIsVXIPresent = 0;
static char gFifoDir[128] = "";

#define MAX_SLOT1 MAX_PROCS
#define MAX_VXML_APPS 20
#define NO_CDF_FILES -99			// BT-128

time_t defunctCheckTime = 0;

/*DDN: Added for MR2078*/
typedef struct INSTANCE_STATUS
{
	int port;
	int status;
	int pid;
	char program_name[256];
} InstanceStatus;

typedef struct STRUCT_GV_CDFINFO
{
	int toFireOcsApp;
	char cdfFileName[256];
	char appToAppInfo[256];
} GV_CDFInfo;

GV_CDFInfo gGV_CDFInfo[MAX_PROCS];
InstanceStatus gInstanceStatus[MAX_PROCS];
struct MsgToApp     gGV_MsgToApp[MAX_PROCS];

LM_HANDLE *GV_lmHandle;

typedef struct STRUCT_GV_FAXINFO
{
	int		toFireFaxApp;
	char	faxFileName[256];
	char appToAppInfo[256];
}GV_FAXInfo;

GV_FAXInfo gGV_FAXInfo[MAX_PROCS];

typedef struct STRUCT_GV_DMINFO
{
	int fd;
	int pid;
	int startPort;
	int endPort;
	long startTime;
	long stopTime;
	char dynMgrName[15];
	char fifoName[256];
} GV_DMInfo;

GV_DMInfo gGV_DMInfo[MAX_PROCS/48];

int GV_NumberOfDMRunning;
int GV_CurrentDMPid;

char GV_FromVxmlFifo[256];
int GV_FromVxmlFifoFd;

char GV_FromDMFifo[256];						  /* temp declaration */
int GV_FromDMFifoFd;							  /* temp declaration */
char GV_ToDMFifo[128];							  /* temp declaration */
int GV_ToDMFifoFd;								  /* temp declaration */
char gResultLine[1098];

char respPidFile[32];
int hbeat_interval = 0;
int tran_sig[MAX_PROCS];						  /* (field 10) to store signal status */
int appType[MAX_PROCS];							  /* STATIC=1, OCS=2 etc */
int appStatus[MAX_PROCS];						  /* running=1, killed=2 etc */
char gCDFFileName[128];
int gOcsConfigTimeout = 30;
int gOcsMaxCDFToProcess = -1;
int GV_arcGoogleSRPid;
int GV_arcChatGPTPid;
int GV_arcVXML1Pid[MAX_VXML_APPS];
int GV_arcVXML2Pid[MAX_VXML_APPS];
int GV_lastOCSPortUsed = 0;

// Network Transfer
int         GV_removeCdfFiles = 1;
static char ntAppCfg[512]="";


#define HUNT_GROUP_CFG "trunkGroups.cfg"
#define MAX_NUM_SECTIONS 20
#define DEFAULT_RULE "FIRSTAVAILABLE"

#define RULE_FIRST_AVAILABLE 1
#define RULE_ASCENDING 2
#define RULE_ASCENDING_EVEN 3
#define RULE_ASCENDING_ODD 4
#define RULE_DESCENDING 5
#define RULE_DESCENDING_EVEN 6
#define RULE_DESCENDING_ODD 7

struct GivePortRule
{
	char section[256];
	int rule;
	int lastPortGiven;
} gGivePortRule[MAX_NUM_SECTIONS];

int gMaxSections;
int RedirectorPid = 0;
int siproxdPid = 0;
int ttsMgrPid = 0;
int srMgrPid = 0;
int rtrClientPid = 0;

int beginPort = -1;
int lastPort, dynMgrStartNum;

struct MsgToRes gMsgReceived;
struct Msg_CantFireApp msg;
struct MsgToDM gMsgToDM;
struct Msg_ReservePort gMsg_ReservePort;
struct Msg_StartOcsApp gMsg_StartOcsApp;
struct Msg_StartFaxApp gMsg_StartFaxApp;

static int Licensed_Resources, Turnkey_License, numOfAppsAllowed = 0;
static int fifo_is_open = 0;
static int StartRedirector;
static int StartSiproxd;
static int StartTTSMgr;
static int StartSRMgr;			// BT-42
static int StartVXML2;			// BT-42
static int StartRTRClient = 0;
static int StartGoogleSR = 0;	
static int StartChatGPT = 0;	

static int checkResponsibility (char *);
static int check_environment ();
static int check_configuration_tables ();
static int removeSharedMem ();
static int createSharedMem (int);
static int check_update_table_request ();
static int load_tables (char *, char *, char *, char *);
static void resp_shutdown ();
static void resp_log (char *mod, int port, int mode, int msgid, char *msg);
static void load_parameter_configuration ();
static void cleanupApp (int, int, int);
static void check_kill_app ();
static int set_object_path ();
static int get_field ();
static int update_DNIS (int, char *);
static void convertStarToUnixPatternMatch (char *, char *);
static int StartUpResourceMgr ();
static int StartUpRedirector (int);
static int StartUpTTSMgr ();
static int StartUpSRMgr ();
static int StartUpRTRClient ();
static int StartUpDynMgr (int);
//static int StartUpDynMgrAfterDefunct (int);
static int startVXML2Process (int zVxmlId);
static int startGoogleSRProcess (int zGoogleSRId);
static int startChatGPTProcess(int zChatGPTId);
static int checkAndKillChatGPT (char *procName);
static void see_if_googleSR_chatgpt_should_run ();
static int StartarcVXML ();
static int StartUpSipProxy (int);
static void see_if_ResourceMgr_should_run ();
static void see_if_redirector_should_run ();
static void see_if_tts_sr_should_run ();
static void see_if_vxml_should_run ();
static void see_if_googleSR_should_run ();
static int get_license_file_name (char *, char *);
static int launchApp (int, char *, char *, char *, int, char *, char *, char *);
static int readGlobalCfg (char *fifo_dir, int *startRTRClient);
static int sReportCallStatus (char *zCallStatusMsg, char *CDFFileName, int errCode);
static int sRequestLicenses (int numResourcesToRequest);
//static int sReleaseLicenses ();
static int getNumResources (char *resource_file, int *numResources);

// Network Transfer
static int writeToNTApp (int zLine, struct MsgToApp *response);
static int startNTOCSApp (char *zCdfFile, struct MsgToApp *zResponse);
//static int isNTAppEligible (char *ocsFileName, char *trunkName);
static int readTrunkName (char *ocsFileName, char *trunkName, char *app, char *failureReason);
static int NT_dispose_on_failure (char *header_file, char *Result_line, char *zFailMsg);
static int NT_dispose_on_success (char *header_file, char *Result_line);
static int sNTReportCallStatus (char *zCallStatusMsg, char *CDFFileName, int errCode);
static void loadNTAppCfg();

int handleLicenseFailure ();
int readRulesFromCfg();
int	start_netserv();
int load_shmem_tabl (int mem_id, int startPort, int endPort);
int checkIfVXIPresent ();
void startStaticApp (int port);
extern int util_get_name_value(char *, char *, char *, char *, int , char *, char *);
int shouldIstartDM ();
int processDMrequest ();
extern int read_fld(char *ptr, int rec, int ind, char *stor_arry);
int checkAndStartOCSApp (int port);
int checkAndStartFaxServer(int port);
int checkHeartBeat (time_t curr_time);
int load_resource_table (char *resource_file);
int load_appref_table (char *appref_file);
int load_pgmref_table (char *pgmref_file);
int load_schedule_table(char *);
int find_application ( int , int , char *, char *, char *, char *);
int readFromDynMgr (int zLine, int timeout, void *pMsg, int zMsgLength);
int processRESOP_FIREAPP (struct MsgToRes *zpMsgToRes);
int processRESOP_FIREAPP_for_reservePort (struct Msg_ReservePort *zpMsgToRes);
int processRESOP_STATIC_APP_GONE (struct MsgToRes *zpMsgToRes);
int processRESOP_STATIC_APP_GONE_for_reservePort (struct Msg_ReservePort *zpMsgToRes);
int processRESOP_START_OCS_APP (struct MsgToRes *zpMsgToRes);
int processRESOP_START_FAX_APP(struct MsgToRes *zpMsgToRes);
int processRESOP_VXML_PID (int zLine, struct MsgToRes *zpMsgToRes);
int processRESOP_START_NTOCS_APP (struct MsgToRes *zpMsgToRes);
int processRESOP_START_NTOCS_APP_DYN (struct MsgToRes *zpMsgToRes);
extern int write_fld(char *ptr, int rec, int ind, int);
void get_answer_time ( char *current_time);
int application_instance_manager (int command, char *program_name, int res_id);
int updateAppName (int port, char *appl_name);
int createRequestFifo (int ReleaseNo, char *mod);
extern int LogARCMsg(char *, int , char *, char *, char *, int ,  char *);
extern int write_arry(char *ptr, int ind, int stor_arry[]);
int checkAndKillRunVXID (char *procName);
int checkAndKillGoogleSRJar (char *procName);
int initDynMgrStruct ();
//static int util_sleep(int Seconds, int Milliseconds);
int sendStaticPortReserveReqToDM (int ZNumber);
int checkDynMgr (char *dynMgrName);
int checkDataToken ( int match_rule, char *token1, char *token2, struct schedule *record);
int check_date_time_rule ( char *, char *, char *, char *, char *, char *, int );
int lookForDnisMatch (int match_rule, int type, char *dnis, char *ani, struct schedule * record);
int sc_rule0 (int , int , int , int , int , int , int , int , int , int , int );
int sc_rule1 (int , int , int , int , int , int , int , int , int , int , int );
int sc_rule2 (int , int , int , int , int , int , int , int , int , int , int );
int sc_rule3 (int , int , int , int , int , int , int , int , int , int , int );
int sc_rule4 (int , int , int , int , int , int , int , int , int , int , int );
int sc_rule5 (int , int , int , int , int , int , int , int , int , int , int );
int sc_rule6 (int , int , int , int , int , int , int , int , int , int , int );
int sc_rule7 (int , int , int , int , int , int , int , int , int , int , int );
int sc_rule8 (int , int , int , int , int , int , int , int , int , int , int );
int sc_rule9 (int , int , int , int , int , int , int , int , int , int , int );
int lookForDnisMatch (int , int , char *, char *, struct schedule * );
int stringMatch (char *iPattern, char *iString);
int util_get_cfg_filename( char *, char *, char *);
int read_arry(char *ptr, int ind, int stor_arry[]);
int compress8To1 (char *zpDestArray, char *zpSrcArray, int zArrayLength);
int processRESOP_PORT_RESPONSE_PERM (struct Msg_ReservePort *, int , char *);
void getFreePort_onSameSpan ( int, int , int *, int *, char *);
int getAppToAppInfoFromCDF (char *, char *);
int processStaticPortResponseFromDM (char *, char *, int , int );
int ocmMoveCDF(const OCM_ASYNC_CDS *, char *, char*, char * , char *);
int dispose_on_success (int , char *, char *);
int dispose_on_failure (char *, char *);
int isAppEligible (char *, char *, char *, char *, char *, char *);
void getFreePort ( int , int *, int *, char *);
int updateTryCount (int , char *, char *);
int updatelastAttempt (int , char *, char *);
int readCallTime (char *, char *, char *, char *, char *, char *, char *, char *, char *);
int getTicsFromHMS (int , char *);
int gaChangeRecord(char , char *, int , char *, int , char *);
int checkRuleAndGetPort (int , int , int , int , int , int *, char *);
int dispose_on_fax_failure( char *, char *);
void get_available_port ( int , int *, char *);
int checkRuleAndGetPort_onSameSpan (int , int , int , int , int , int , int *, char *);
int get_available_port_onSameSpan ( int , int , int *, char *);
int checkIfFree (int );

extern int util_sleep(int Seconds, int Milliseconds);

//extern char log_buf[1024];

int
main (int argc, char *argv[])
{
	static char mod[] = "main";
	int i, j, rc = 0;
	struct sigaction sig_act, sig_oact;
	time_t curr_time;							  /* to store the current time */
	time_t ocs_time;							  /* to store the ocs time */
	time_t  fax_time;           /* to store the Fax time *//*DDN: 03/04/2010 */
	int ocsStatus = 0;
	char pid_str[10];
	char status_str[10];
	int int_pid;
	FILE *fp;
	int	currentOCSPort;
	int ocsCounter;			// bt-308

	if (strcmp (argv[1], "-v") == 0)
	{
		fprintf (stdout, "%s - %s - %s\n", argv[0], __DATE__, __TIME__);
		exit (0);
	}

	sprintf (log_buf, "Starting up Aumtech Telecom Responsibility Subsystem. pid=%d", getpid());
	resp_log (mod, -1, REPORT_DETAIL, 3000, log_buf);

/* set heartbeat seconds and set object name */
	switch (argc)
	{
		case 6:
			gRespId = atoi (argv[5]);

		case 5:

			beginPort = atoi (argv[3]);
			sprintf (log_buf, "startPort is = %d.", beginPort);
			resp_log (mod, -1, REPORT_DETAIL, 3001, log_buf);

			lastPort = atoi (argv[4]);
			sprintf (log_buf, "endPort is = %d.", lastPort);
			resp_log (mod, -1, REPORT_DETAIL, 3001, log_buf);

		case 3:
			if ((atoi (argv[2]) > ISP_MIN_HBEAT) && (atoi (argv[2]) < ISP_MAX_HBEAT))
			{
												  /* secs */
				hbeat_interval = (atoi (argv[2])) * 60;
			}
			else if (atoi (argv[2]) == -1)
			{
				hbeat_interval = -1;
			}
			else
			{
												  /* default */
				hbeat_interval = ISP_DEFAULT_HBEAT * 60;
				sprintf (log_buf, "WARNING : kill time should be  %d < kill time < %d, Setting kill time to default = %d Min.", ISP_MIN_HBEAT, ISP_MAX_HBEAT,
					ISP_DEFAULT_HBEAT);
				resp_log (mod, -1, REPORT_DETAIL, 3001, log_buf);
			}
			break;
		default:
			fprintf (stderr, "Usage: %s <Object Name> <kill time interval>\n", argv[0]);
			sprintf (log_buf, "Usage: %s <Object Name> <kill time interval>", argv[0]);
			resp_log (mod, -1, REPORT_NORMAL, 3003, log_buf);
			exit (1);
	}											  /* switch */

	time (&(defunctCheckTime));

#if 0
	rc = arcValidLicense ("IPTEL", 1, &Turnkey_License, &Licensed_Resources, log_buf);
	if (rc != 0)
	{
		fprintf (stderr, "%s: LICENSE FAILURE. %s\n", argv[1], log_buf);
		resp_log (mod, -1, REPORT_NORMAL, 3009, "License failure. See nohup.out.");
		exit (0);
	}
#endif

	if (beginPort == -1)
	{
		beginPort = 0;
	}

	if (beginPort == 0)
	{
												  /* check if object is running */
		if (checkResponsibility (argv[0]) != ISP_SUCCESS)
		{
			exit (1);
		}
	}

	sprintf (respPidFile, "%s.pid", argv[0]);
	if (beginPort == 0)
	{
		if ((fp = fopen (respPidFile, "w+")) == NULL)
			;
		else
		{
			fprintf (fp, "%d\n", getpid ());
			fclose (fp);
		}
	}
	else
	{
		if ((fp = fopen (respPidFile, "a+")) == NULL);
		else
		{
			fprintf (fp, "%d\n", getpid ());
			fclose (fp);
		}
	}

	sprintf (log_buf, "Heartbeat interval is %d", hbeat_interval);
	resp_log (mod, -1, REPORT_VERBOSE, 3004, log_buf);

	sprintf (object_code, "%s", argv[1]);
	if (check_environment () != ISP_SUCCESS)
	{
		exit (1);
	}

/* set directory structure */
	if (set_object_path () != ISP_SUCCESS)
	{
		exit (1);
	}

	if (gRespId == -1)
	{
		if ((rc = sRequestLicenses (-1)) == -1)
		{
			exit (1);
		}
	}
	else
	{
		if ((rc = sRequestLicenses (lastPort - beginPort + 1)) == -1)
		{
			exit (1);
		}
	}

	switch (beginPort)
	{
		case 0:
		case 96:
		case 144:
		case 192:
		case 240:
		case 288:
		case 336:
		case 384:
		case 432:
		case 480:
		case 528:
		case 576:
		case 624:
		case 672:
		case 720:
		case 768:
		case 816:
		case 864:
		case 912:
			dynMgrStartNum = beginPort/48;
			break;

		default:
			sprintf (log_buf,
				"Invalid value of start port %d, passed to responsibility, "
				"valid values are 0, 96, 144, 192, 240, 288, 336 or 384. Please corrct in Start_Telecom script. Exiting..", beginPort);
			resp_log (mod, -1, REPORT_NORMAL, 3004, log_buf);
			exit (0);

	}											  /*END: switch */

    memset((char *)gFifoDir, '\0', sizeof(gFifoDir));
    readGlobalCfg (gFifoDir, &StartRTRClient);
    sprintf (log_buf, "readGlobalCfg() returned (%s), startRTRClient=%d", gFifoDir, StartRTRClient);
	resp_log (mod, -1, REPORT_VERBOSE, 3054, log_buf);

/* check if all configuration files exist */
	if (check_configuration_tables () != ISP_SUCCESS)
	{
		exit (1);
	}

/* Remove any existing shared memory segment. */
	if (beginPort == 0)
	{
		if (removeSharedMem () != ISP_SUCCESS)
			exit (1);
	}

/* Create required shared memory segment.  */
	if (createSharedMem (beginPort) != ISP_SUCCESS)
	{
		exit (1);
	}

/* set shutdown function */
	(void) signal (SIGTERM, resp_shutdown);

/* set death of child function */
	sig_act.sa_handler = NULL;
	sig_act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
	if (sigaction (SIGCHLD, &sig_act, &sig_oact) != 0)
	{
		sprintf (log_buf, "sigaction(SIGCHLD, SA_NOCLDWAIT): system call failed. [%d, %s].",
				errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3005, log_buf);
		exit (-1);
	}
	//if (signal (SIGCHLD, SIG_IGN) == SIG_ERR || sigset (SIGCLD, SIG_IGN) == -1)
	if (signal (SIGCHLD, SIG_IGN) == SIG_ERR )
	{
		sprintf (log_buf, "signal(SIGCHLD, SIG_IGN): system call failed. [%d, %s].", errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3006, log_buf);
		exit (-1);
	}
/* attach shared memory */
												  /* attach the shared memory segment */
	if ((tran_tabl_ptr = shmat (tran_shmid, 0, 0)) == (char *) -1)
	{
		sprintf (log_buf, "Can't attach shared memory segment. [%ld, %d, %s]", GV_shm_key, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3007, log_buf);
		exit (-1);
	}

/* this will store the time file last modified */
	(void) check_update_table_request ();

/* Load all configuration tables into memory */
	if (load_tables (schedule_tabl, resource_tabl, appref_tabl, pgmref_tabl) != ISP_SUCCESS)
	{
		exit (1);
	}

    readRulesFromCfg();

//  NT config load
    loadNTAppCfg();

	if (beginPort == 0)
	{
/* start network services if configured */
		if (strcmp (NetworkStatus, "ON") == 0)
		{
			StartNetworkServices = 1;
			start_netserv ();
		}
		else
			StartNetworkServices = 0;

		if (StartResourceMgr == 1)
		{
			StartUpResourceMgr ();
		}

		if (StartSiproxd == 1)
		{
			StartUpSipProxy (0);
		}

		if (StartRedirector == 1)
		{
			StartUpRedirector (0);
		}

		if (StartTTSMgr == 1)
		{
			StartUpTTSMgr ();
		}

		if (StartSRMgr == 1)
		{
			StartUpSRMgr ();
		}

		if ( StartRTRClient == 1 )
		{
			StartUpRTRClient ();
		}

		if ( StartGoogleSR == 1 )
		{
			startGoogleSRProcess (0);
		}

		if ( StartChatGPT == 1 )
		{
			startChatGPTProcess (0);
		}
	}

	if (gRespId == -1)
	{
		tran_proc_num = tot_resources;
		lastPort = tot_resources - 1;
	}
	else
	{
		tran_proc_num = lastPort - beginPort + 1;
	}

/* Load the resource and scheduling tables into the shared memory segments */
	if (load_shmem_tabl (tran_shmid, beginPort, lastPort) != ISP_SUCCESS)
	{
		exit (1);
	}

	sprintf (log_buf, "Successfully Started up Aumtech Telecom Responsibility Subsystem. pid=%d",
				getpid());
	resp_log (mod, -1, REPORT_NORMAL, 3004, log_buf);
/*DDN: Added for MR2087*/
	for (i = 0; i < MAX_PROCS; i++)
	{
		gInstanceStatus[i].status = ISP_DELETE_INSTANCE;
		gInstanceStatus[i].port = i;
		gInstanceStatus[i].program_name[0] = '\0';
		memset((struct MsgToApp *) &(gGV_MsgToApp[i]), '\0', sizeof(struct MsgToApp ));
	}

	gIsVXIPresent = checkIfVXIPresent ();

	for (i = dynMgrStartNum; i < GV_NumberOfDMRunning + dynMgrStartNum; i++)
	{
		StartUpDynMgr (i);
		sleep (1);								  // This sleep gives some time for DM to create it's Request fifo.
	}

	for (i = beginPort; i < tran_proc_num + beginPort; i++)
	{
		appl_pid[i] = 0;
		appCount[i] = 0;
		appStatus[i] = 0;
	}
	memset (gResultLine, 0, sizeof (gResultLine));

	for (i = beginPort; i < tran_proc_num + beginPort; i++)
	{
		if (appType[i] == 1)					  /* Static App */
		{
			startStaticApp (i);
		}
	}

	if (StartVXML2 == 1)			// BT-42
	{
		for (i = 0; i < MAX_VXML_APPS; i++)
		{
			GV_arcVXML1Pid[i] = 0;
			GV_arcVXML2Pid[i] = 0;
		}
		StartarcVXML ();
	}

	{											  // Read ocs config timeout value.
		char lFileName[512];
		char val[10];
		char lLogMsg[256];

		val[0] = '\0';
		sprintf (lFileName, "%s/%s", table_path, "OCServer.cfg");
		util_get_name_value ("", "TIMEOUT", "30", val, sizeof (val) - 1, lFileName, lLogMsg);
		if (rc == 0)
		{
			gOcsConfigTimeout = atoi (val);
		}

		val[0] = '\0';
		util_get_name_value ("", "MAX_CDF_TO_PROCESS", "-1", val, sizeof (val) - 1, lFileName, lLogMsg);        // bt-308
		if (rc == 0)
		{
			gOcsMaxCDFToProcess = atoi (val);
			if ( gOcsMaxCDFToProcess > 0 )
			{
				sprintf (log_buf, "[%d] cdf: Enabled max number of CDF files to process per iteration to %d.", __LINE__, gOcsMaxCDFToProcess);
				resp_log (mod, -1, REPORT_VERBOSE, 3560, log_buf);
			}
		}
	}

	time (&ocs_time);
	time(&fax_time);	/*DDN: 03/04/2010 */
	for (;;)									  /* the watchdog */
	{
/* following routine check for all dead application and update info */
		(void) check_kill_app ();

/* check to see if dynamic manager needs to be re-started */
		(void) shouldIstartDM ();

/* check for dynamic manager request */
		(void) processDMrequest ();

		time (&curr_time);

		if (beginPort == 0)
		{
			if ((curr_time - ocs_time) > gOcsConfigTimeout)
			{
				ocsCounter = 0;			// bt-308
				//sprintf (log_buf, "Last OCS port used %d.", GV_lastOCSPortUsed);
                //resp_log (mod, -1, REPORT_VERBOSE, 3560, log_buf);

				sprintf (log_buf, "[%d] Last OCS port used %d.  tran_proc_num=%d", __LINE__, GV_lastOCSPortUsed, tran_proc_num);
                resp_log (mod, -1, REPORT_VERBOSE, 3560, log_buf);
                currentOCSPort = GV_lastOCSPortUsed;

				for (i = currentOCSPort; i < tran_proc_num; i++)
				{

					if ((appType[i] == 2) && (gGV_CDFInfo[i].toFireOcsApp == 0))
					{		/* OCServer App */
						char yStrTmpProgramName[64];
						read_fld (tran_tabl_ptr, i, APPL_STATUS, status_str);
						read_fld(tran_tabl_ptr, i, APPL_NAME, yStrTmpProgramName);     /*DDN:20101021 Jeff's issue*/

						if (atoi (status_str) == STATUS_IDLE &&  strcmp(yStrTmpProgramName, "arcVXML2"))
						{
							if ( gOcsMaxCDFToProcess > 0 )      // bt-308
							{
								if ( ocsCounter >= gOcsMaxCDFToProcess )
								{
									sprintf (log_buf, "[%d] Number of CDF files processed (%d) exceeded maximum (%d).",
														__LINE__, ocsCounter, gOcsMaxCDFToProcess);
									resp_log (mod, -1, REPORT_VERBOSE, 3560, log_buf);
									break;
								}
							}

							if ( (rc = checkAndStartOCSApp (i)) == NO_CDF_FILES)			// BT-128
							{
								break;
							}
							else
							{
								sprintf (log_buf, "[%d] Current OCS Port - appType[%d]=%d gGV_CDFInfo[%d].toFireOcsApp=%d "
									"atoi (%s)=%d [STATUS_IDLE=%d] yStrTmpProgramName=(%s) ", __LINE__,
									i, appType[i], i, gGV_CDFInfo[i].toFireOcsApp,
									status_str, atoi (status_str), STATUS_IDLE, yStrTmpProgramName);
								resp_log (mod, i, REPORT_VERBOSE, 3560, log_buf);
							}
						}
						else		// BT-128
						{
                        	if ( i >= ( tran_proc_num - 1 ) )
							{
								GV_lastOCSPortUsed = 0;
								sprintf (log_buf, "[%d] Port %d is busy. Resetting GV_lastOCSPortUsed to %d.", __LINE__, i, GV_lastOCSPortUsed);
				                resp_log (mod, -1, REPORT_VERBOSE, 3560, log_buf);
							}
						}
					}
				}

				for (i = 0; i < currentOCSPort; i++)
                {
                    if ((appType[i] == 2) && (gGV_CDFInfo[i].toFireOcsApp == 0))
                    {       /* OCServer App */
                        char yStrTmpProgramName[64];
                        read_fld (tran_tabl_ptr, i, APPL_STATUS, status_str);
                        read_fld(tran_tabl_ptr, i, APPL_NAME, yStrTmpProgramName);     /*DDN:20101021 Jeff's issue*/

						if (StartVXML2 == 1)			// BT-42
						{
							if (atoi (status_str) == STATUS_IDLE &&  strcmp(yStrTmpProgramName, "arcVXML2"))
							{
								if ( gOcsMaxCDFToProcess > 0 )      // bt-308
								{
									if ( ocsCounter >= gOcsMaxCDFToProcess )
									{
										sprintf (log_buf, "[%d] Number of CDF files processed (%d) exceeded maximum (%d).",
															__LINE__, ocsCounter, gOcsMaxCDFToProcess);
										resp_log (mod, -1, REPORT_VERBOSE, 3560, log_buf);
										break;
									}
								}

								if ( (rc = checkAndStartOCSApp (i)) == NO_CDF_FILES)			// BT-128
								{
									break;
								}
								else
								{
									sprintf (log_buf, "[%d] Current OCS Port - appType[%d]=%d gGV_CDFInfo[%d].toFireOcsApp=%d "
										"atoi (%s)=%d [STATUS_IDLE=%d] yStrTmpProgramName=(%s) ", __LINE__,
										i, appType[i], i, gGV_CDFInfo[i].toFireOcsApp,
										status_str, atoi (status_str), STATUS_IDLE, yStrTmpProgramName);
									resp_log (mod, i, REPORT_VERBOSE, 3560, log_buf);
								}
							}
							else		// BT-128
							{
                        		if ( i >= ( tran_proc_num - 1 ) )
								{
									GV_lastOCSPortUsed = 0;
									sprintf (log_buf, "[%d] Port %d is busy. Resetting GV_lastOCSPortUsed to %d.", __LINE__, i, GV_lastOCSPortUsed);
				           		     resp_log (mod, -1, REPORT_VERBOSE, 3560, log_buf);
								}
							}
						}
                    }
				}

				ocs_time = curr_time;
			}

			if((curr_time - fax_time) > 60)
			{
				for(i=0; i < tran_proc_num; i++)
				{
					if((appType[i] == 3) &&(gGV_FAXInfo[i].toFireFaxApp == 0))
					{	/* FaxServer App */
						char yStrTmpProgramName[64];
						read_fld(tran_tabl_ptr, i, APPL_STATUS, status_str);
						read_fld(tran_tabl_ptr, i, APPL_NAME, yStrTmpProgramName);     /*DDN:20101021 Jeff's issue*/
						if(atoi(status_str) == STATUS_IDLE &&  strcmp(yStrTmpProgramName, "arcVXML2"))
						{
							checkAndStartFaxServer(i);
						}
					}
				}

				fax_time = curr_time;
			}
	
		}

/* after every hbeat_interval seconds, check application health */
		if (hbeat_interval != -1 && !gIsVXIPresent)
		{
			checkHeartBeat (curr_time);
		}

/* check for update configuration table request */
		database_hit = 0;
		if (check_update_table_request () == 0)	  /* request for table updation */
		{
			resp_log (mod, -1, REPORT_NORMAL, 3008, "Reloading system tables.");
			if (load_tables (schedule_tabl, resource_tabl, appref_tabl, pgmref_tabl) != ISP_SUCCESS)
			{
				exit (0);						  /* decision making exit */
			}
			resp_log (mod, -1, REPORT_NORMAL, 3009, "Done reloading system tables");
			for (i = beginPort; i < tran_proc_num + beginPort; i++)
			{
				if (appl_pid[i] != 0)
				{
												  /* check if application exists */
					if (kill (appl_pid[i], 0) == -1)
					{
						if (errno == ESRCH)		  /* No process */
						{
							if (appType[i] == 1)  /* Static App */
							{
								startStaticApp (i);
							}
						}
					}
				}
				else
				{
					if (appType[i] == 1)		  /* Static App */
					{
						startStaticApp (i);
					}
				}
			}
			database_hit = 1;
		}										  /* check_update_table_request */
	}											  /* watchdog */
}												  /* main */


/*--------------------------------------------------------------------------
Check if responsibility is already running.
---------------------------------------------------------------------------*/
static char ps[] = "ps -ef | grep -v grep";

int
checkResponsibility (char *respName)
{
	static char mod[] = "checkResponsibility";
	FILE *fin;									  /* file pointer for the ps pipe */
	int i;
	char buf[BUFSIZE];

	if ((fin = popen (ps, "r")) == NULL)		  /* open the process table */
	{
		sprintf (log_buf, "Failed to verify that Responsibility is running. [%d, %s]", errno, strerror(errno));
		resp_log (mod, -1, REPORT_DETAIL, 3052, log_buf);
		exit (0);
	}

	(void) fgets (buf, sizeof buf, fin);		  /* strip off the header */
/* get the responsibility s proc_id */
	i = 0;
	while (fgets (buf, sizeof buf, fin) != NULL)
	{
		if (strstr (buf, respName) != NULL)
		{
			i = i + 1;
			if (i >= 2)
			{
				sprintf (log_buf, "%s", "Responsibility is already running.");
				resp_log (mod, -1, REPORT_DETAIL, 3053, log_buf);
				(void) pclose (fin);
				exit (0);
			}
		}
	}
	(void) pclose (fin);

	return (ISP_SUCCESS);
}												  /* check_responsibility */


/*----------------------------------------------------------------------------
int chek_environment()
---------------------------------------------------------------------------*/
int
check_environment ()
{
	static char mod[] = "check_environment";
	char *home;

/* set HOME path */
	if ((home = getenv ("HOME")) == NULL)
	{
		sprintf (log_buf, "Environment variable %s is not found/set.", "HOME");
		resp_log (mod, -1, REPORT_VERBOSE, 3054, log_buf);
		return (ISP_FAIL);
	}
	sprintf (isp_home, "%s", home);

/* set ISPBASE path */
	if ((base_path = (char *) getenv ("ISPBASE")) == NULL)
	{
		sprintf (log_buf, "Environment variable %s is not found/set.", "ISPBASE");
		resp_log (mod, -1, REPORT_VERBOSE, 3055, log_buf);
		return (ISP_FAIL);
	}
	sprintf (isp_base, "%s", base_path);

	return (ISP_SUCCESS);
}												  /* check_environment */


/*-----------------------------------------------------------------------------
Check if all configuration files exist.
-----------------------------------------------------------------------------*/
int
check_configuration_tables ()
{
	static char mod[] = "check_configuration_tables";
	register int i = 0;
	int tot_config_table = 0;
	char file_path[512];

	tot_config_table = tel_tot_tables;

	for (i = 0; i < tot_config_table; i++)
	{
		sprintf (file_path, "%s/%s", table_path, tel_config_file[i]);
/* check if file exists */
		ret_code = access (file_path, R_OK);
		if (ret_code < 0)
		{
			sprintf (log_buf, "Can't access file %s. [%d, %s]", file_path, errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3057, log_buf);
			return (ISP_FAIL);
		}
	}											  /* for all files */
	return (ISP_SUCCESS);
}												  /* check_configuration_tables */


/*------------------------------------------------------------------------------
 this routine removes the shared memory & message queues
------------------------------------------------------------------------------*/
int
removeSharedMem ()
{
	static char mod[] = "removeSharedMem";
	int ret_code;

/* set shared memory key and queue */
	GV_shm_key = SHMKEY_TEL;

/* remove shared memory  -  get shared memory segments id */
	tran_shmid = shmget (GV_shm_key, SIZE_SCHD_MEM, PERMS);
	ret_code = shmctl (tran_shmid, IPC_RMID, 0);
	if ((ret_code < 0) && (errno != EINVAL))
	{
		sprintf (log_buf, "Can't remove shared memory. [%ld, %d, %s]", GV_shm_key, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3058, log_buf);
		return (ISP_FAIL);
	}
	return (ISP_SUCCESS);
}												  /*removeSharedMem */


/*------------------------------------------------------------------------------
This routine creates the shared memory segments and Message Queues
------------------------------------------------------------------------------*/
int
createSharedMem (int startPort)
{
	static char mod[] = "createSharedMem";

	GV_shm_key = SHMKEY_TEL;
/* create shared memory segment */

	if (startPort == 0)
	{
		if ((tran_shmid = shmget (GV_shm_key, SIZE_SCHD_MEM, PERMS | IPC_CREAT | IPC_EXCL)) < 0)
		{
			sprintf (log_buf, "Unable to get shared memory segment %ld. [%d, %s]",
						GV_shm_key, errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3059, log_buf);
			return (-1);
		}
		sprintf (log_buf, "DEBUG: Shared memory = %ld (hex = %lx) created.", (long) GV_shm_key, GV_shm_key);
		resp_log (mod, -1, REPORT_VERBOSE, 3060, log_buf);
	}
	else
	{
		if ((tran_shmid = shmget (GV_shm_key, SIZE_SCHD_MEM, PERMS)) < 0)
		{
			sprintf (log_buf, "Unable to get shared memory segment %ld. [%d, %s]",
						GV_shm_key, errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3059, log_buf);
			return (-1);
		}
	}

	sprintf (log_buf, "DEBUG: tran_shmid = %d.", tran_shmid);
	resp_log (mod, -1, REPORT_VERBOSE, 3060, log_buf);

	return (ISP_SUCCESS);
}												  /* createSharedMem */


/*--------------------------------------------------------------------------
Check for update table request.
---------------------------------------------------------------------------*/
int
check_update_table_request ()
{
	static char mod[] = "check_update_table_request";
	static time_t last_time_modified;
	char reload_path[1024];
	struct stat file_stat;

	sprintf (reload_path, "%s/%s", lock_path, RELOAD_FILENAME);
/* check if reload file exists if not create one */
	if (access (reload_path, R_OK) != 0)
	{
		if (creat (reload_path, 0644) != 0)
		{
			sprintf (log_buf, "Failed to create new reload file %s. [%d, %s]", reload_file,
					errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3061, log_buf);
			return (-1);
		}
	}
	if (stat (reload_path, &file_stat) == 0)	  /* check if last modified time is changed */
	{
		if (file_stat.st_mtime != last_time_modified)
		{
			last_time_modified = file_stat.st_mtime;
			return (0);							  /* last modification time change reload found */
		}
	}											  /* stat */
	return (-1);
}												  /* check_update_table_request */


/*-------------------------------------------------------------------
load_tables(): This Routine load all the tables into memory.
--------------------------------------------------------------------*/
int
load_tables (char *schedule_file, char *resource_file, char *appref_file, char *pgmref_file)
{
	static int first_time = 1;

/* Load server specific parameter configuration table */
	load_parameter_configuration ();
/* resource table can't be reload dynamically */
	if (first_time == 1)
	{
		if (load_resource_table (resource_file) != ISP_SUCCESS)
			return (ISP_FAIL);
		first_time = 0;
	}
	if (load_appref_table (appref_file) != ISP_SUCCESS)
		return (ISP_FAIL);
	if (load_pgmref_table (pgmref_file) != ISP_SUCCESS)
		return (ISP_FAIL);
	if (load_schedule_table (schedule_file) != ISP_SUCCESS)
		return (ISP_FAIL);

	return (ISP_SUCCESS);
}												  /* load_tables */


/*----------------------------------------------------------------------------
load_parameter_configuration(): This routine load configuration file for server.
-----------------------------------------------------------------------------*/
void
load_parameter_configuration ()
{
	char buf[100];
	char param[30];
	char cfg_file[256];
	FILE *cfp;
	char *ptr;

	if (beginPort == 0)
	{
		see_if_ResourceMgr_should_run ();
		see_if_redirector_should_run ();
		see_if_tts_sr_should_run ();
		see_if_googleSR_should_run ();
		see_if_googleSR_chatgpt_should_run ();
	}
	see_if_vxml_should_run();
	sprintf (cfg_file, "%s/%s", table_path, TEL_PARM_CONFIG);

	cfp = fopen (cfg_file, "r");
	if (cfp == NULL)
	{
		sprintf (log_buf, "Unable to open (%s). [%d, %s] Setting default parameters.",
				cfg_file, errno, strerror(errno));
		resp_log ("load_parm_config", -1, REPORT_NORMAL, 3062, log_buf);
		return;
	}
	while (fgets (buf, sizeof (buf), cfp) != NULL)
	{
		buf[(int) strlen (buf) - 1] = '\0';

/* set network services */
		if (strstr (buf, "NetServices") != NULL)
			sscanf (buf, "%[^=]=%s", param, NetworkStatus);
/* set language */
		if (strstr (buf, "DefaultLanguage") != NULL)
			sscanf (buf, "%[^=]=%s", param, DefaultLang);
	}											  /* while not eof */
	fclose (cfp);
	return;
}												  /* load_parameter_configuration */

/*----------------------------------------------------------------------------
loadNTAppCfg(): 
-----------------------------------------------------------------------------*/
static void loadNTAppCfg()
{
	char buf[100];
	char param[64];
	char value[64];

	char cfg_file[256];
	FILE *cfp;
	char *ptr;

	cfp = fopen (ntAppCfg, "r");
	if (cfp == NULL)
	{
		sprintf (log_buf, "Unable to open (%s). [%d, %s] Setting default parameters.",
				ntAppCfg, errno, strerror(errno));
		resp_log ("loadNTAppCfg", -1, REPORT_DETAIL, 3062, log_buf);
		return;
	}
	memset((char *)param, '\0', sizeof(param));
	memset((char *)value, '\0', sizeof(value));

	while (fgets (buf, sizeof (buf), cfp) != NULL)
	{
		buf[(int) strlen (buf) - 1] = '\0';

		if (strstr (buf, "RemoveCdfFiles") != NULL)
		{
			sscanf (buf, "%[^=]=%s", param, value);
			if ( ( value[0] == 'N' ) || ( value[0] == 'n' ) )
            {
				GV_removeCdfFiles = 0;
            }
            else
            {
				GV_removeCdfFiles = 1;
            }
			break;
		}
	}											  /* while not eof */
	fclose (cfp);
	sprintf (log_buf, "removeCdrFiles set to %d (from %s)",
			GV_removeCdfFiles, ntAppCfg);
	resp_log ("loadNTAppCfg", -1, REPORT_VERBOSE, 3066, log_buf);
	return;
} // END - loadNTAppCfg(): 


/*----------------------------------------------------------------------------
This routine loads the appref & pgmref tables from the license file.
----------------------------------------------------------------------------*/
int
load_appref_pgmref_from_license ()
{
	int rc;
	int fcount = 0;
	FILE *fp;
	char opcode[50];
	char dummy[128];
	char feature[128];
	char inputbuf[256];
	char license_file[256];
	char mod[] = "load_appref_pgmref";
	int dummy2;

	tot_apprefs = 0;
	tot_pgmrefs = 0;

	rc = get_license_file_name (license_file, log_buf);
	if (rc != 0)
	{
		resp_log (mod, -1, REPORT_NORMAL, 3063, log_buf);
		return (ISP_FAIL);
	}

	if ((fp = fopen (license_file, "r")) == NULL)
	{
		sprintf (log_buf, "Unable to open (%s) for reading. [%d, %s]", license_file, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3064, log_buf);
		return (ISP_FAIL);
	}

	while (fgets (inputbuf, sizeof (inputbuf), fp))
	{
		opcode[0] = '\0';
		feature[0] = '\0';
		inputbuf[(int) strlen (inputbuf) - 1] = '\0';
		if (strlen (inputbuf) == 0)
			continue;
		if (inputbuf[0] == '\t' || inputbuf[0] == '#' || inputbuf[0] == ' ')
			continue;

		sscanf (inputbuf, "%s %s %s %s %s %d", opcode, feature, dummy, dummy, dummy, &fcount);
		if (strcmp (opcode, "FEATURE") != 0)
			continue;
#if 0
		if (feature[0] == 'T' && feature[1] == 'K')
		{
			rc = lic_get_license (feature, log_buf);
			if (rc != 0)
			{
				sprintf (log_buf, "Cannot load (%s). License violation.", &feature[2]);
				resp_log (mod, -1, REPORT_NORMAL, 3065, log_buf);
				continue;
			}

			sprintf (appref[tot_apprefs].appl_grp_name, "%s", &feature[2]);
			sprintf (appref[tot_apprefs].acct_code, "%s", &feature[2]);
			appref[tot_apprefs].max_instance = fcount;
			tot_apprefs++;
			sprintf (pgmref[tot_pgmrefs].program_name, "%s", &feature[2]);
			sprintf (pgmref[tot_pgmrefs].appl_grp_name, "%s", &feature[2]);
			tot_pgmrefs++;
		}
#endif
	}
	fclose (fp);
	return (ISP_SUCCESS);
}


/*-------------------------------------------------------------------
load_appref_table(): This Routine load appref table into memory.
--------------------------------------------------------------------*/
int
load_appref_table (char *appref_file)
{
	static char mod[] = "load_appref_table";
	FILE *fp;
	char record[256];
	char field[MAX_CODE - 1];
	int field_no = 0;
	int load_entry = 0;							  /* decide whether to load entry */

#if 0										  // djb - why is this here?
#ifdef COMMENTED_FOR_LINUX
	if (Turnkey_License)
	{
		load_appref_pgmref_from_license ();
		return (ISP_SUCCESS);
	}
#endif										  /* COMMENTED_FOR_LINUX */
#endif

	tot_apprefs = 0;

	sprintf (log_buf, "load_appref_table(): Loading %s table", appref_file);
	resp_log (mod, -1, REPORT_VERBOSE, 3066, log_buf);

	if ((fp = fopen (appref_file, "r")) == NULL)
	{
		sprintf (log_buf, "Unable to open file (%s). [%d, %s]", appref_file, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3163, log_buf);
		return (ISP_FAIL);
	}
	while (fgets (record, sizeof (record), fp) != NULL)
	{
		load_entry = 1;							  /* load the entry */
		for (field_no = 1; field_no <= MAX_APPREF_FIELD; field_no++)
		{
			field[0] = '\0';
			if (get_field (record, field_no, field) < 0)
			{
				sprintf (log_buf, "Corrupted line in (%s). " "Unable to read field no %d, record no. %d. " "Line is: (%s)", appref_file, field_no, tot_apprefs,
					record);
				resp_log (mod, -1, REPORT_NORMAL, 3067, log_buf);
				load_entry = 0;					  /* don't load entry */
			}
			if (load_entry == 0)				  /* skip record for errors */
				break;
			switch (field_no)
			{
				case APPL_GRP_NAME:
					sprintf (appref[tot_apprefs].appl_grp_name, "%.*s", 49, field);
					break;
				case ACCT_CODE:
					sprintf (appref[tot_apprefs].acct_code, "%s", field);
					break;
				case MAX_INSTANCES:
					appref[tot_apprefs].max_instance = atoi (field);
					break;
				default:
					sprintf (log_buf, "File = %s, Field %d is invalid field number", appref_file, field_no);
					resp_log (mod, -1, REPORT_NORMAL, 3068, log_buf);
					break;
			}									  /*switch */
		}										  /* for */
		if (load_entry == 0)
			continue;							  /* corrupted entry */
		sprintf (log_buf, "Application Group Name = %s", appref[tot_apprefs].appl_grp_name);
		resp_log (mod, -1, REPORT_VERBOSE, 3069, log_buf);
		sprintf (log_buf, "Accounting Code = %s", appref[tot_apprefs].acct_code);
		resp_log (mod, -1, REPORT_VERBOSE, 3070, log_buf);
		sprintf (log_buf, "Maxium Instances = %d", appref[tot_apprefs].max_instance);
		resp_log (mod, -1, REPORT_VERBOSE, 3071, log_buf);
		tot_apprefs++;
	}											  /* while end of file */

	(void) fclose (fp);

	sprintf (log_buf, "Total entries loaded from Application Group Reference table = %d.", tot_apprefs);
	resp_log (mod, -1, REPORT_VERBOSE, 3072, log_buf);

	return (ISP_SUCCESS);
}												  /* load_appref_table */


/*-------------------------------------------------------------------
load_pgmref_table(): This Routine load pgmref table into memory.
--------------------------------------------------------------------*/
int
load_pgmref_table (char *pgmref_file)
{
	static char mod[] = "load_pgmref_table";
	FILE *fp;
	char record[256];
	char field[256];
	int field_no = 0;
	int load_entry = 0;							  /* decide whether to load entry */

	if (Turnkey_License)
	{
/* No need to load pgmref, it's loaded with appref */
		return (ISP_SUCCESS);
	}
	tot_pgmrefs = 0;

	sprintf (log_buf, "load_pgmref_table(): Loading %s table", pgmref_file);
	resp_log (mod, -1, REPORT_VERBOSE, 3073, log_buf);

	if ((fp = fopen (pgmref_file, "r")) == NULL)
	{
		sprintf (log_buf, "Unable to open file %s. [%d, %s]", pgmref_file, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3074, log_buf);
		return (ISP_FAIL);
	}
	while (fgets (record, sizeof (record), fp) != NULL)
	{
		load_entry = 1;							  /* load the entry */
		for (field_no = 1; field_no <= MAX_PGMREF_FIELD; field_no++)
		{
			field[0] = '\0';
			if (get_field (record, field_no, field) < 0)
			{
				sprintf (log_buf, "Corrupted line in (%s). Unable to read field no %d, record no. %d. Line is: (%s)", pgmref_file, field_no, tot_pgmrefs, record);
				resp_log (mod, -1, REPORT_NORMAL, 3075, log_buf);
				load_entry = 0;					  /* don't load entry */
/* return (ISP_FAIL); */
			}
			if (load_entry == 0)				  /* skip record for errors */
				break;
			switch (field_no)
			{
				case PROGRAM:
					sprintf (pgmref[tot_pgmrefs].program_name, "%s", field);
					break;
				case APPL_GRP:
					sprintf (pgmref[tot_pgmrefs].appl_grp_name, "%.*s", 49, field);
					break;
				default:
					sprintf (log_buf, "File = %s, Field %d is invalid field number", pgmref_file, field_no);
					resp_log (mod, -1, REPORT_NORMAL, 3076, log_buf);
					break;
			}									  /*switch */
		}										  /* for */
		if (load_entry == 0)
			continue;							  /* corrupted entry */
		sprintf (log_buf, "Program Name = %s", pgmref[tot_pgmrefs].program_name);
		resp_log (mod, -1, REPORT_VERBOSE, 3077, log_buf);
		sprintf (log_buf, "Application Group Name = %s", pgmref[tot_pgmrefs].appl_grp_name);
		resp_log (mod, -1, REPORT_VERBOSE, 3078, log_buf);
		tot_pgmrefs++;
	}											  /* while end of file */

	(void) fclose (fp);

	sprintf (log_buf, "Total entries loaded from Program Reference table = %d.", tot_pgmrefs);
	resp_log (mod, -1, REPORT_VERBOSE, 3079, log_buf);

	return (ISP_SUCCESS);
}												  /* load_pgmreference_table */


/*-------------------------------------------------------------------
load_schedule_table(): This Routine load scheduling table into memory.
--------------------------------------------------------------------*/
int
load_schedule_table (char *schedule_file)
{
	static char mod[] = "load_schedule_table";
	register int i, j;
	FILE *fp;
	char record[256];
	char field[256];
	int field_no = 0;
	char pgm_name[512];
	int pgm_found = ISP_NOTFOUND, grp_found = ISP_NOTFOUND;
	int load_entry = 0;							  /* decide whether to load entry */

	tot_schedules = 0;

	sprintf (log_buf, "Loading %s table....", schedule_file);
	resp_log (mod, -1, REPORT_VERBOSE, 3080, log_buf);

	if ((fp = fopen (schedule_file, "r")) == NULL)
	{
		sprintf (log_buf, "Unable to open file (%s). [%d, %s]  "
				"Loading schedule table failed.", 
				schedule_file, errno, strerror(errno));

		resp_log (mod, -1, REPORT_NORMAL, 3081, log_buf);
		return (ISP_FAIL);
	}
	while (fgets (record, sizeof (record), fp) != NULL)
	{
		load_entry = 1;							  /* load the entry */
		for (field_no = 1; field_no <= MAX_SCHEDULE_FIELD; field_no++)
		{
			field[0] = '\0';
			if (get_field (record, field_no, field) < 0)
			{
				sprintf (log_buf, "Corrupted line in (%s). Unable to read field no %d, record no. %d. Line is: (%s)", schedule_file, field_no, tot_schedules,
					record);
				resp_log (mod, -1, REPORT_NORMAL, 3082, log_buf);
				load_entry = 0;					  /* don't load entry */
			}
			if (load_entry == 0)				  /* skip record for errors */
				break;
			switch (field_no)
			{
				case SERVER_NAME:
					schedule[tot_schedules].srvtype[0] = '\0';
					sprintf (schedule[tot_schedules].srvtype, "%.*s", 9, field);
					break;
				case MACHINE_NAME:
					schedule[tot_schedules].machine[0] = '\0';
					sprintf (schedule[tot_schedules].machine, "%.*s", MAX_MACHINE - 1,  field);
					break;
				case DESTINAION:
					schedule[tot_schedules].destination[0] = '\0';
					sprintf (schedule[tot_schedules].destination, "%.*s", 19, field);
					break;
				case ORIGINATION:
					schedule[tot_schedules].origination[0] = '\0';
					sprintf (schedule[tot_schedules].origination, "%.*s", 19, field);
					break;
				case PRIORITY:
					schedule[tot_schedules].priority = 0;
					schedule[tot_schedules].priority = atoi (field);
					break;
				case RULE:
					schedule[tot_schedules].rule = 0;
					schedule[tot_schedules].rule = atoi (field);
					break;
				case START_DATE:
					schedule[tot_schedules].start_date[0] = '\0';
					sprintf (schedule[tot_schedules].start_date, "%.*s", 19, field);
					break;
				case STOP_DATE:
					schedule[tot_schedules].stop_date[0] = '\0';
					sprintf (schedule[tot_schedules].stop_date, "%.*s", 19, field);
					break;
				case START_TIME:
					schedule[tot_schedules].start_time[0] = '\0';
					sprintf (schedule[tot_schedules].start_time, "%.*s", 19, field);
					break;
				case STOP_TIME:
					schedule[tot_schedules].stop_time[0] = '\0';
					sprintf (schedule[tot_schedules].stop_time, "%.*s", 19, field);
					break;
				case PROGRAM_NAME:
					schedule[tot_schedules].program[0] = '\0';
					sprintf (schedule[tot_schedules].program, "%s", field);
					if ((int) strlen (schedule[tot_schedules].program) > MAX_APPL_NAME)
					{
						sprintf (log_buf, "Ignore scheduling table entry for %s : Length of Application name too long, limit = %d", schedule[tot_schedules].program,
							MAX_APPL_NAME);
						resp_log (mod, -1, REPORT_NORMAL, 3083, log_buf);
						continue;
					}
					break;
				default:
					sprintf (log_buf, "Field %d is invalid field number", field_no);
					resp_log (mod, -1, REPORT_NORMAL, 3084, log_buf);
					break;
			}									  /*switch */
		}										  /* for */
		if (load_entry == 0)
			continue;							  /* corrupted entry */
#ifdef DEBUG_TABLES
		sprintf (log_buf, "Service = %s", schedule[tot_schedules].srvtype);
		resp_log (mod, -1, REPORT_VERBOSE, 3085, log_buf);
		sprintf (log_buf, "Machine = %s", schedule[tot_schedules].machine);
		resp_log (mod, -1, REPORT_VERBOSE, 3086, log_buf);
		sprintf (log_buf, "Destination = %s", schedule[tot_schedules].destination);
		resp_log (mod, -1, REPORT_VERBOSE, 3087, log_buf);
		sprintf (log_buf, "Origination = %s", schedule[tot_schedules].origination);
		resp_log (mod, -1, REPORT_VERBOSE, 3088, log_buf);
		sprintf (log_buf, "Priority = %d", schedule[tot_schedules].priority);
		resp_log (mod, -1, REPORT_VERBOSE, 3089, log_buf);
		sprintf (log_buf, "Rule = %d", schedule[tot_schedules].rule);
		resp_log (mod, -1, REPORT_VERBOSE, 3090, log_buf);
		sprintf (log_buf, "Start Date = %s", schedule[tot_schedules].start_date);
		resp_log (mod, -1, REPORT_VERBOSE, 3091, log_buf);
		sprintf (log_buf, "Stop Date = %s", schedule[tot_schedules].stop_date);
		resp_log (mod, -1, REPORT_VERBOSE, 3092, log_buf);
		sprintf (log_buf, "Start Time = %s", schedule[tot_schedules].start_time);
		resp_log (mod, -1, REPORT_VERBOSE, 3093, log_buf);
		sprintf (log_buf, "Stop Time = %s", schedule[tot_schedules].stop_time);
		resp_log (mod, -1, REPORT_VERBOSE, 3094, log_buf);
		sprintf (log_buf, "Program = %s", schedule[tot_schedules].program);
		resp_log (mod, -1, REPORT_VERBOSE, 3095, log_buf);
#endif
/* check if program exists , if not exists don't load the entry */
		if (access (schedule[tot_schedules].program, R_OK | X_OK) != 0)
		{
			sprintf (log_buf, "Application (%s) not found, [%d, %s]  "
				"Scheduling table entry (DNIS, ANI) = (%s, %s) is ignored.",
				schedule[tot_schedules].program, errno, strerror(errno),
				schedule[tot_schedules].destination, schedule[tot_schedules].origination);
			resp_log (mod, -1, REPORT_DETAIL, 3096, log_buf);
			load_entry = 0;						  /* don't load entry */
		}
/* find and load program name */
/* get basename of program */
		sprintf (pgm_name, "%s", schedule[tot_schedules].program);
		pgm_found = ISP_NOTFOUND;
		for (i = 0; i < tot_pgmrefs; i++)
		{
			if (strcmp (pgm_name, pgmref[i].program_name) == 0)
			{
				sprintf (schedule[tot_schedules].appl_grp_name, "%s", pgmref[i].appl_grp_name);
				pgm_found = ISP_FOUND;
				break;
			}
		}
/* if program found in program ref now check in application reference */
		if (pgm_found == ISP_FOUND)
		{
			grp_found = ISP_NOTFOUND;
/*  find and load application name and maxium instances */
			for (j = 0; j < tot_apprefs; j++)
			{
				if (strcmp (schedule[tot_schedules].appl_grp_name, appref[j].appl_grp_name) == 0)
				{
					sprintf (schedule[tot_schedules].acct_code, "%s", appref[j].acct_code);
					schedule[tot_schedules].max_instance = appref[j].max_instance;
					grp_found = ISP_FOUND;
					break;
				}
			}									  /* for */
		}										  /* if */
		else									  /* not found */
		{
			sprintf (log_buf,
				"WARNING: Program = %s is not found in program reference file, program will not be started by %s server. Scheduling entry for (DNIS, ANI) = (%s, %s) is ignored.", schedule[tot_schedules].program, object_name, schedule[tot_schedules].destination, schedule[tot_schedules].origination);
			resp_log (mod, -1, REPORT_DETAIL, 3097, log_buf);
			load_entry = 0;						  /* don't load entry */
			continue;
		}
/* if application group not in program reference */
		if (pgm_found == ISP_FOUND && grp_found == ISP_NOTFOUND)
		{
			sprintf (log_buf,
				"WARNING: Program = %s Application : %s is not found in application reference file, program will not be started by %s server. Scheduling entry for (DNIS, ANI) = (%s, %s) is ignored.",
				schedule[tot_schedules].program, schedule[tot_schedules].appl_grp_name, object_name, schedule[tot_schedules].destination,
				schedule[tot_schedules].origination);
			resp_log (mod, -1, REPORT_DETAIL, 3098, log_buf);
			load_entry = 0;						  /* don't load entry */
			continue;
		}
#ifdef DEBUG_TABLES
		sprintf (log_buf, "Application Group = %s", schedule[tot_schedules].appl_grp_name);
		resp_log (mod, -1, REPORT_VERBOSE, 3099, log_buf);
		sprintf (log_buf, "Accounting Code = %s", schedule[tot_schedules].acct_code);
		resp_log (mod, -1, REPORT_VERBOSE, 3100, log_buf);
		sprintf (log_buf, "Max Instances = %d", schedule[tot_schedules].max_instance);
		resp_log (mod, -1, REPORT_VERBOSE, 3101, log_buf);
#endif

		if (load_entry == 1)					  /* load entry is ok */
			tot_schedules++;
	}											  /* while end of file */

	(void) fclose (fp);

	sprintf (log_buf, "Total entries loaded from scheduling table = %d.", tot_schedules);
	resp_log (mod, -1, REPORT_VERBOSE, 3102, log_buf);

	return (ISP_SUCCESS);
}												  /* load_schedule_table */


/*------------------------------------------------------------------------------
 This routine loads the shared memory into the memory
------------------------------------------------------------------------------*/
int
load_shmem_tabl (int mem_id, int startPort, int endPort)
{
	static char mod[] = "load_shmem_tabl";
	int res_found = ISP_NOTFOUND;
	int slot_no = 0;
	char *pvt_ptr;								  /* pointer to the shared memory */
	char buf[512];
	char *tmp_ptr;
	char resource_no[30];
	char file_name[MAX_PROGRAM];
	char appl_grp[MAX_APPLICATION];

	pvt_ptr = shmat (mem_id, 0, 0);				  /* attach the shared memory segment */
	tmp_ptr = pvt_ptr;
	if (pvt_ptr == (char *) (-1))
	{
		sprintf (log_buf, "Unable to attach shared memory segment %ld. [%d, %s]", GV_shm_key, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3103, log_buf);
		return (ISP_FAIL);
	}
	if (startPort != 0)
	{
		pvt_ptr += (startPort * SHMEM_REC_LENGTH);
	}

	sprintf (log_buf, "mem_id = %d, startPort =%d endPort = %d, tot_resources = %d", mem_id, startPort, endPort, tot_resources);
	resp_log (mod, -1, REPORT_VERBOSE, 3104, log_buf);

/* load resource into shadred memory slot */
	for (slot_no = startPort; slot_no <= endPort && slot_no < tot_resources + startPort; slot_no++)
	{
#if 0
		printf ("RespId(%d), Ptr(%p) Slot(%d)\n", gRespId, pvt_ptr, slot_no);
		fflush (stdout);
#endif
		buf[0] = '\0';
		file_name[0] = '\0';
		sprintf (resource_no, "%s", resource[slot_no].res_no);
/* for IP dynamic application fire dynamic manager */
		if (strcmp (resource[slot_no].res_usage, RESERVE_PORT) == 0)
		{
			sprintf (file_name, "%s", RESERVE_PORT);
			sprintf (buf, SHM_FORMAT, "Reserve", "0", resource_no, "field4", "0", "0000000000", file_name, "0", "0", "0");
		}										  /* if IP & DYN_MGR */

		if (strcmp (resource[slot_no].res_type, "IP") == 0 && strcmp (resource[slot_no].res_state, "STATIC") != 0)
		{
			sprintf (buf, SHM_FORMAT, "Reserve", "0", resource_no, "field4", "0", "0000000000", "N/A", "0", "0", "0");
		}
		else
		{
			if (find_application (SCHEDULER, slot_no, resource[slot_no].static_dest, "", file_name, appl_grp) != ISP_SUCCESS)
			{
				sprintf (buf, SHM_FORMAT, "Reserve", "0", resource_no, "field4", "0", "0000000000", "N/A", "6", "0", "0");
			}
			else
			{
/*      sprintf(file_name, "%s", resource[slot_no].res_usage);
   sprintf(buf, SHM_FORMAT, "Reserve", "0", resource_no, "field4", "0", "0000000000", file_name, "0", "0", "0"); */
				sprintf (buf, SHM_FORMAT, "Reserve", "0", resource_no, "field4", "0", "0000000000", "N/A", "0", "0", "0");
			}
		}
/* fill the record into slot */
		(void) strncpy (pvt_ptr, buf, strlen (buf));
		pvt_ptr += strlen (buf);
	}											  /* for all slot */

	ret_code = shmdt (tmp_ptr);					  /* detach the shared memory segment */
	if (ret_code == -1)
	{
		sprintf (log_buf, "Unable to detach shared memory segment %ld. [%d, %s]", GV_shm_key, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3104, log_buf);
		return (ISP_FAIL);
	}
	return (ISP_SUCCESS);
}												  /* load_shmem_tabl */


/*---------------------------------------------------------------------------
check the request (max request = MAX_REQUEST_PROCESS) on dynamic manager
request queue and process the request(s) if any.
----------------------------------------------------------------------------*/
int
processDMrequest ()
{
	static char mod[] = "processDMrequest";
	register int i;

	for (i = 0; i <= MAX_REQUEST_PROCESS; i++)
	{
		ret_code = readFromDynMgr (__LINE__, 100, &gMsgReceived, sizeof (gMsgReceived));
		if (ret_code != 0)
			return (0);
		switch (gMsgReceived.opcode)
		{
			case RESOP_FIREAPP:
				processRESOP_FIREAPP (&gMsgReceived);
				break;
			case RESOP_STATIC_APP_GONE:
				processRESOP_STATIC_APP_GONE (&gMsgReceived);
				break;
			case RESOP_START_OCS_APP:
				processRESOP_START_OCS_APP (&gMsgReceived);
				break;
			case RESOP_START_FAX_APP:
				processRESOP_START_FAX_APP (&gMsgReceived);
				break;
			case RESOP_VXML_PID:
				if (StartVXML2 == 1)			// BT-42
				{
					processRESOP_VXML_PID (__LINE__, &gMsgReceived);
				}
				break;
            case RESOP_START_NTOCS_APP:
                processRESOP_START_NTOCS_APP (&gMsgReceived);
                break;
            case RESOP_START_NTOCS_APP_DYN:
                processRESOP_START_NTOCS_APP_DYN (&gMsgReceived);
                break;
			default:
				sprintf (log_buf, "[%d] Invalid request %d, received",
							__LINE__, gMsgReceived.opcode);
				resp_log (mod, -1, REPORT_NORMAL, 3107, log_buf);
		}										  /* switch */
	}
	return (ISP_SUCCESS);
}												  /* processDMrequest */


/*----------------------------------------------------------------------------
Function to get the results of a request back from the dynamic manager.
----------------------------------------------------------------------------*/
int
readFromDynMgr (int zLine, int timeout, void *pMsg, int zMsgLength)
{
	static char mod[] = "readFromDynMgr";
	int rc ;
	char fifoDir[128];
	struct pollfd pollset[2];
	struct MsgToRes *m = (struct MsgToRes *) pMsg;
	static int lFifoNr = 0;
	struct MsgVxmlToResp lMsgVxmlToResp;
	int i;

	memset (pMsg, 0, zMsgLength);

	if (!fifo_is_open)
	{
	//	readGlobalCfg (fifoDir);

		sprintf (GV_FromDMFifo, "%s/%s.%d", gFifoDir, "ResFifo", gRespId);

		if (gRespId == -1)
		{
			sprintf (GV_FromDMFifo, "%s/%s", gFifoDir, "ResFifo");
		}

		if ((mknod (GV_FromDMFifo, S_IFIFO | 0666, 0) < 0) && (errno != EEXIST))
		{
/*      sprintf(log_buf,
   "Can't create fifo (%s). errno=%d. [%s]",
   GV_FromDMFifo, errno, strerror(errno)); */
			resp_log (mod, -1, REPORT_NORMAL, 3109, log_buf);
			return (-1);
		}
		if ((GV_FromDMFifoFd = open (GV_FromDMFifo, O_RDWR)) < 0)
		{
			sprintf (log_buf, "[from %d] Can't open fifo (%s). [%d, %s]", zLine, GV_FromDMFifo, errno, strerror (errno));
			resp_log (mod, -1, REPORT_NORMAL, 3110, log_buf);
			return (-1);
		}

		if (StartVXML2 == 1)			// BT-42
		{
			sprintf (GV_FromVxmlFifo, "%s/%s.%d", gFifoDir, "ResFromVxmlFifo", gRespId);
	
			if (gRespId == -1)
			{
				sprintf (GV_FromVxmlFifo, "%s/%s", gFifoDir, "ResFromVxmlFifo");
			}
	
			if ((mknod (GV_FromVxmlFifo, S_IFIFO | 0666, 0) < 0) && (errno != EEXIST))
			{
				sprintf (log_buf, "[from %d] Can't create fifo (%s). [%d, %s]", zLine, GV_FromVxmlFifo, errno, strerror (errno));
				resp_log (mod, -1, REPORT_NORMAL, 3466, log_buf);
				return (-1);
			}
	
			if ((GV_FromVxmlFifoFd = open (GV_FromVxmlFifo, O_RDWR)) < 0)
			{
				sprintf (log_buf, "[from %d] Can't open fifo (%s). [%d, %s]", zLine, GV_FromVxmlFifo, errno, strerror (errno));
				resp_log (mod, -1, REPORT_NORMAL, 3467, log_buf);
				return (-1);
			}
		}

		fifo_is_open = 1;
	}

	pollset[0].fd = GV_FromDMFifoFd;
	pollset[0].events = POLLIN;

	if (StartVXML2 == 1)			// BT-42
	{
		pollset[1].fd = GV_FromVxmlFifoFd;
		pollset[1].events = POLLIN;
	}

	if (StartVXML2 == 1)			// BT-42
	{
		rc = poll (pollset, 2, timeout);
	}
	else
	{
		rc = poll (pollset, 1, timeout);
	}
	if (rc == 0)
	{
		return (-1);
	}
	if (rc < 0)
	{
		if (errno != 4)
		{
			sprintf (log_buf, "[from %d] poll failed. [%d, %s]", zLine, errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3111, log_buf);
			return (-1);
		}
		return (-1);
	}

	if ( StartVXML2 == 1 )		 // BT-42
	{
		for (i = 0; i < 2; i++)
		{
			if (pollset[i].revents == 0)
			{
				continue;
			}
			if ( i == 1 )			
			{
				memset (&lMsgVxmlToResp, 0, sizeof (struct MsgVxmlToResp));
				rc = read (GV_FromVxmlFifoFd, (char *) &lMsgVxmlToResp, sizeof (struct MsgVxmlToResp));
				if (rc == -1)
				{
					sprintf (log_buf, "[from %d] Can't read from fifo (%s). [%d, %s]", zLine, GV_FromVxmlFifo, errno, strerror (errno));
					resp_log (mod, -1, REPORT_NORMAL, 3469, log_buf);
					return (-1);
				}
				m->appCallNum = lMsgVxmlToResp.appCallNum;
				m->appPid = lMsgVxmlToResp.appPid;
				m->opcode = RESOP_VXML_PID;
			}
			else
			{
				rc = read (GV_FromDMFifoFd, (char *) pMsg, zMsgLength);
				if (rc == -1)
				{
					sprintf (log_buf, "[from %d] Can't read from fifo (%s). [%d, %s]", zLine, GV_FromDMFifo, errno, strerror (errno));
					resp_log (mod, -1, REPORT_NORMAL, 3469, log_buf);
					return (-1);
				}
				GV_CurrentDMPid = ((struct MsgToRes *) pMsg)->dynMgrPid;
			}
			break;
		}
	}
	else	// BT-42
	{
		if (pollset[0].revents > 0)
		{
			rc = read (GV_FromDMFifoFd, (char *) pMsg, zMsgLength);
			if (rc == -1)
			{
				sprintf (log_buf, "[from %d] Can't read from fifo (%s). [%d, %s]", zLine, GV_FromDMFifo, errno, strerror (errno));
				resp_log (mod, -1, REPORT_NORMAL, 3469, log_buf);
				return (-1);
			}
			GV_CurrentDMPid = ((struct MsgToRes *) pMsg)->dynMgrPid;
		}
	}

	m = (struct MsgToRes *) pMsg;
/* Here we have a good message read */
	sprintf (log_buf, "[from %d] Received %d bytes from DM. "
		"msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,dnis:%s,ani:%s,data:%s}", zLine,
		rc, m->opcode, m->appCallNum, m->appPid, m->appRef, m->appPassword, m->dnis, m->ani, m->data);
	resp_log (mod, m->appCallNum, REPORT_VERBOSE, 3106, log_buf);
	return (0);
}


/*Send REREGISTER to All dynamic managers*/
int writeToAllDynMgrs (char *mod, struct MsgToDM *request)
{

	int dynMgrCount = 12;
	int i = 0;
	char fifoName[256];
	char lMsg[256];
	int fd = -1;
	int rc = -1;

#if 0
#ifdef NO_HARDCODE_TMP
		memset ((char *) lMsg, '\0', sizeof (lMsg));
		rc = UTL_GetArcFifoDir (DYNMGR_FIFO_INDEX, fifoDir, sizeof (fifoDir), lMsg, sizeof (lMsg));
#endif
#endif

//	readGlobalCfg (fifoDir);

	for(i = 0; i < dynMgrCount; i++)
	{
		sprintf (fifoName, "%s/%s.%d", gFifoDir, "RequestFifo", i);

#ifdef NO_HARDCODE_TMP
		sprintf (fifoName, "%s/%s.%d", gFifoDir, DYNMGR_REQUEST_FIFO, i);
#endif

		fd = open (fifoName, O_RDWR | O_NONBLOCK);

		if(fd < 0)
		{
			break;
		}

		rc = write (fd, (char *) request, sizeof (struct MsgToDM));
		if (rc == -1)
		{
			sprintf (log_buf, "Can't write to (%s). [%d, %s]", fifoName, errno, strerror(errno));

			resp_log (mod, request->appCallNum, REPORT_NORMAL, 3473, log_buf);

			break;
		}

		sprintf (log_buf,
			"Sent %d bytes to (%s). msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,",
				rc, fifoName,
				request->opcode,
				request->appCallNum,
				request->appPid,
				request->appRef,
				request->appPassword);

	}

	return(i);

}/*END: int writeToAllDynMgrs*/

/*-------------------------------------------------------------------------
This function is the only function used by all the APIs to send a message
to the dynamic manager, (except REREGISTER message).
-------------------------------------------------------------------------*/
int
writeToDynMgr (int zLine, char *mod, struct MsgToDM *request, int zDMPid)
{
	int rc;
	int zDMNumber = -1;
	int i;
	char fifoDir[256];
	char str[256];
	struct Msg_CantFireApp *cantFireAppRequest;
	struct Msg_AppDied *appDiedRequest;
	struct Msg_PortRequest *appPortRequest;
	struct Msg_StartOcsApp *startOcsApp;
	struct Msg_StartFaxApp *startFaxApp;

#ifdef NO_HARDCODE_TMP
	char lMsg[256];
#endif

//	sprintf (log_buf, "[%d, from %d] DJB: zDMPid=%d", __LINE__, zLine, zDMPid);
//	resp_log (mod, -1, REPORT_VERBOSE, 3117, log_buf);

	for (i = 0; i < MAX_NUM_OF_DM; i++)
	{
		if (gGV_DMInfo[i].pid == zDMPid)
		{
			zDMNumber = i;

//			sprintf (log_buf, "[%d, from %d] DJB: zDMNumber set to %d", __LINE__, zLine, zDMNumber);
//			resp_log (mod, -1, REPORT_VERBOSE, 3117, log_buf);

			break;
		}
	}

	if (!gGV_DMInfo[zDMNumber].fd)
	{
#if 0
#ifdef NO_HARDCODE_TMP
		memset ((char *) lMsg, '\0', sizeof (lMsg));
		rc = UTL_GetArcFifoDir (DYNMGR_FIFO_INDEX, fifoDir, sizeof (fifoDir), lMsg, sizeof (lMsg));
#endif
		readGlobalCfg (fifoDir);
#endif
		sprintf (gGV_DMInfo[zDMNumber].fifoName, "%s/%s.%d", gFifoDir, "RequestFifo", zDMNumber);
#ifdef NO_HARDCODE_TMP
		sprintf (gGV_DMInfo[zDMNumber].fifoName, "%s/%s.%d", gFifoDir, DYNMGR_REQUEST_FIFO, zDMNumber);
#endif

		if ((mknod (gGV_DMInfo[zDMNumber].fifoName, S_IFIFO | 0777, 0) < 0)
			&& (errno != EEXIST))
		{
			sprintf (log_buf, "[%d, from %d] Can't create fifo %s. [%d, %s]", 
					__LINE__, zLine, gGV_DMInfo[zDMNumber].fifoName, errno, strerror(errno));
			resp_log (mod, request->appCallNum, REPORT_NORMAL, 3471, log_buf);
			return (-1);
		}

		if ((gGV_DMInfo[zDMNumber].fd = open (gGV_DMInfo[zDMNumber].fifoName, O_RDWR | O_NONBLOCK)) < 0)
		{
			sprintf (log_buf, "[%d, from %d] Can't open fifo %s, [%d, %s]",  __LINE__, zLine, gGV_DMInfo[zDMNumber].fifoName, errno, strerror(errno));
			resp_log (mod, request->appCallNum, REPORT_NORMAL, 3472, log_buf);
			return (-1);
		}
	}
	if (kill (zDMPid, 0) == -1)					  /* check if dynamic manager exists */
	{
		if (errno == ESRCH)						  /* No process */
		{
			sprintf (log_buf,
				"[%d, from %d] DynMgr %d doesn't exist, can't send msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,",
				__LINE__, zLine, zDMPid, request->opcode, request->appCallNum, request->appPid, request->appRef, request->appPassword);
			resp_log (mod, request->appCallNum, REPORT_NORMAL, 3500, log_buf);
			return (-1);
		}
	}

	rc = write (gGV_DMInfo[zDMNumber].fd, (char *) request, sizeof (struct MsgToDM));
	if (rc == -1)
	{
		sprintf (log_buf, "[%d, from %d] Can't write to (%s). [%d, %s]", __LINE__, zLine, gGV_DMInfo[zDMNumber].fifoName, errno, strerror(errno));

		if (errno == 11 && request->opcode == DMOP_STARTOCSAPP)
		{
			resp_log (mod, request->appCallNum, REPORT_DETAIL, 3473, log_buf);
		}
		else if (errno == 11 && request->opcode == DMOP_STARTFAXAPP)
		{
			resp_log (mod, request->appCallNum, REPORT_DETAIL, 3473, log_buf);
		}
		else
		{
			resp_log (mod, request->appCallNum, REPORT_NORMAL, 3473, log_buf);
		}

		return (-1);
	}

	sprintf (log_buf,
		"[%d, from %d] Sent %d bytes to (%s). msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,", __LINE__, zLine,
			rc, gGV_DMInfo[zDMNumber].fifoName,
			request->opcode,
			request->appCallNum,
			request->appPid,
			request->appRef,
			request->appPassword);

	switch (request->opcode)
	{
		case DMOP_APPDIED:
			appDiedRequest = (struct Msg_AppDied *) request;
			sprintf (str, ",rc:%d}", appDiedRequest->returnCode);
			strcat (log_buf, str);
			break;

		case DMOP_CANTFIREAPP:
			cantFireAppRequest = (struct Msg_CantFireApp *) request;
			sprintf (str, ",dnis:%s", cantFireAppRequest->dnis);
			strcat (log_buf, str);
			sprintf (str, ",ani:%s", cantFireAppRequest->ani);
			strcat (log_buf, str);
			sprintf (str, ",data:%s", cantFireAppRequest->data);
			strcat (log_buf, str);
			sprintf (str, ",rc:%d}", cantFireAppRequest->returnCode);
			strcat (log_buf, str);
			break;
		case DMOP_PORTREQUEST:
			appPortRequest = (struct Msg_PortRequest *) request;
			sprintf (str, ",portNumber:%d", appPortRequest->portNumber);
			strcat (log_buf, str);
			sprintf (str, ",returnCode:%d}", appPortRequest->returnCode);
			strcat (log_buf, str);
			break;
		case DMOP_STARTFAXAPP:
			startFaxApp = (struct Msg_StartFaxApp *) request;
			sprintf (str, ",dnis:%s", startFaxApp->dnis);
			strcat (log_buf, str);
			sprintf (str, ",ani:%s", startFaxApp->ani);
			strcat (log_buf, str);
			sprintf (str, ",data:%s", startFaxApp->data);
			strcat (log_buf, str);
			break;
		case DMOP_STARTOCSAPP:
			startOcsApp = (struct Msg_StartOcsApp *) request;
			sprintf (str, ",dnis:%s", startOcsApp->dnis);
			strcat (log_buf, str);
			sprintf (str, ",ani:%s", startOcsApp->ani);
			strcat (log_buf, str);
			sprintf (str, ",data:%s", startOcsApp->data);
			strcat (log_buf, str);
		default:
			strcat (log_buf, "}");
			break;
	}
	resp_log (mod, request->appCallNum, REPORT_VERBOSE, 3474, log_buf);
	return (0);
}

/*-------------------------------------------------------------------------
-------------------------------------------------------------------------*/
static int writeToNTApp (int zLine, struct MsgToApp *response)
{
	static	char mod[]="writeToNTApp";
	int		rc;
	int		i;
	char	str[256];
	static int	fd = -1;
	char	responseFifo[256];
	char	lMsg[256];

    sprintf(responseFifo, "%s/ResponseFifo.%d", gFifoDir, response->appPid);

	if ((mknod (responseFifo, S_IFIFO | 0777, 0) < 0) && (errno != EEXIST))
	{
		sprintf (log_buf, "[From %d] Can't create fifo %s. [%d, %s]", 
				zLine, responseFifo, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, TEL_IPC_ERROR, log_buf);
		return (-1);
	}

	if (kill (response->appPid, 0) == -1)	/* check if app exists */
	{
		if (errno == ESRCH)					  /* No process */
		{
			sprintf (log_buf, 
				"Application pid %d doesn't exist, can't send msg={op:%d,c#:%d,pid:%d,rc:%d:msg:(%s)",
				response->appPid, response->opcode, response->appCallNum,
				response->appPid, response->returnCode, response->message);
			resp_log (mod, -1, REPORT_NORMAL, TEL_PROCESS_NOT_FOUND, log_buf);
			return (-1);
		}
	}

	if ((fd = open (responseFifo, O_RDWR | O_NONBLOCK)) < 0)
	{
		sprintf (log_buf, "Can't open fifo %s, [%d, %s]", responseFifo, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, TEL_IPC_ERROR, log_buf);
		return (-1);
	}

	rc = write (fd, (char *) response, sizeof (struct MsgToApp));
	close(fd);

	if (rc == -1)
	{
		sprintf (log_buf, "Can't write to (%s). [%d, %s]", responseFifo, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, TEL_IPC_ERROR, log_buf);
		return (-1);
	}

	sprintf (log_buf,
		"[from %d[ Sent %d bytes to (%s).  msg={op:%d,c#:%d,pid:%d,rc:%d:msg:(%s)",
		zLine, rc, responseFifo, response->opcode, response->appCallNum,
		response->appPid, response->returnCode, response->message);
	resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);

	return (0);
} // END: writeToNTApp ()

/*------------------------------------------------------------------------------
 This routine launch the application based on the information in shared memory
 and scheduling table. if the value for the corresponding element is 0
------------------------------------------------------------------------------*/
int
launchApp (int port, char *appl_name, char *dnis, char *ani, int password, char *ringEventTime, char *acctName, char *appToAppInfoFile)
{
	static char mod[] = "launchApp";
	char appPassWord[10], appCallNum[10];
	char dir_name[256];							  /* Name of the directory */
	char program_name[256];						  /* Name of the program */
	char *ptr;
	int dir_index = 0;
	int pid, i;
	int rc;
	char tmpFifoName[128];
	char answerTime[20];
	char lpDynMgrId[5];

	ret_code = access (appl_name, R_OK | X_OK);
	if (ret_code != 0)							  /* make status off */
	{
		write_fld (tran_tabl_ptr, port, APPL_STATUS, STATUS_OFF);
		sprintf (log_buf, "[%d] Can't start application %s, [%d, %s]",
			__LINE__, appl_name, errno, strerror(errno));
		resp_log (mod, port, REPORT_NORMAL, 3165, log_buf);
		if (gGV_CDFInfo[port].toFireOcsApp == 1)
		{
			sprintf (gResultLine, "%s", log_buf);
			sReportCallStatus (gResultLine, gGV_CDFInfo[port].cdfFileName, 20003);
			gGV_CDFInfo[port].toFireOcsApp = 0;
		}
		return(-1);
	}
	memset (program_name, 0, sizeof (program_name));
	memset (dir_name, 0, sizeof (dir_name));
/* set directory and program name */
	if (strchr (appl_name, '/') == NULL)
	{
		sprintf (program_name, "%s", appl_name);
		sprintf (dir_name, "%s", ".");
	}
	else										  /* parse program name and directory name */
	{
		sprintf (dir_name, "%s", appl_name);
		dir_index = strlen (dir_name) - 1;
		for (; dir_index >= 0; dir_index--)
		{
			if (dir_name[dir_index] == '/')
			{
				dir_name[dir_index] = '\0';
				break;
			}
		}
		ptr = (char *) strrchr (appl_name, '/');
		if (ptr)
			ptr++;
		sprintf (program_name, "%s", ptr);
	}
	if (dir_name[0] == '/')						  /* full path */
	{
		sprintf (appl_path, "%s/%s", dir_name, program_name);
	}
	else										  /* relative path */
	{
		if (strcmp (dir_name, ".") == 0)		  /*current dir */
			sprintf (appl_path, "%s/%s", exec_path, program_name);
		else
			sprintf (appl_path, "%s/%s/%s", exec_path, dir_name, program_name);
	}

	sprintf (appPassWord, "%d", password);
	sprintf (appCallNum, "%d", port);

	i = 0;
	if ( gGV_MsgToApp[port].opcode == RESOP_START_NTOCS_APP )
	{
		i = port / 48;
		
		if ((port >= gGV_DMInfo[i].startPort) && (port <= gGV_DMInfo[i].endPort))
		{
			GV_CurrentDMPid = gGV_DMInfo[i].pid;
		}
		else
		{
			; // what to do here?
		}
	}
	else
	{
		for (i = dynMgrStartNum; i < GV_NumberOfDMRunning + dynMgrStartNum; i++)
		{
			if ((port >= gGV_DMInfo[i].startPort) && (port <= gGV_DMInfo[i].endPort))
			{
				GV_CurrentDMPid = gGV_DMInfo[i].pid;
				break;
			}
		}
	}

	sprintf (lpDynMgrId, "%d", i);

	if ((pid = fork ()) == 0)					  /* load configuration parameter change directory */
	{
		if (chdir (dir_name) != 0)
		{
			sprintf (log_buf, "Failed to execute chdir system call to (%s). [%d, %s]", dir_name, errno, strerror(errno));
			resp_log (mod, port, REPORT_NORMAL, 3166, log_buf);
			if (gGV_CDFInfo[port].toFireOcsApp == 1)
			{
				sprintf (gResultLine, "%s", log_buf);
				sReportCallStatus (gResultLine, gGV_CDFInfo[port].cdfFileName, 20004);
				gGV_CDFInfo[port].toFireOcsApp = 0;
			}
			else if (gGV_FAXInfo[port].toFireFaxApp == 1)
			{
				sprintf (gResultLine, "%s", log_buf);
				sReportCallStatus (gResultLine, gGV_FAXInfo[port].faxFileName, 20004);
				gGV_FAXInfo[port].toFireFaxApp = 0;
			}
		}

		if (gGV_CDFInfo[port].toFireOcsApp == 1 || gGV_FAXInfo[port].toFireFaxApp == 1)
		{
			char lModAppToAppInfo[1024];

			if(gGV_FAXInfo[port].toFireFaxApp == 1)
			{
				sprintf (lModAppToAppInfo, "%s|%s", gGV_FAXInfo[port].faxFileName, gGV_FAXInfo[port].appToAppInfo);
			}
			else
			{
				sprintf (lModAppToAppInfo, "%s|%s", gGV_CDFInfo[port].cdfFileName, gGV_CDFInfo[port].appToAppInfo);
			}

			get_answer_time (answerTime);

			rc = execl (appl_path, program_name,
				"-p",
				appCallNum,
				"-y",
				acctName,
				"-P",
				program_name,
				"-z",
				appPassWord, "-D", dnis, "-A", ani, "-U", lModAppToAppInfo, "-r", answerTime, "-d", lpDynMgrId, "-t", resource[port].res_usage, "-F", GV_FromDMFifo,
				(char *) 0);
		}
		else
		{
			rc = execl (appl_path, program_name,
				"-p",
				appCallNum, "-y", acctName, "-P", program_name, "-z", appPassWord, "-D", dnis, "-A", ani, "-r", ringEventTime, "-d", lpDynMgrId, "-F", GV_FromDMFifo,
				(char *) 0);
		}
		if (rc != 0)
		{
			sprintf (log_buf, "[%d] Channel=%d. execl(program=%s, password %d). "
					"Failed to start application. [%d, %s]",
					__LINE__, port, appl_name, password, errno, strerror(errno));
			resp_log (mod, port, REPORT_NORMAL, 3116, log_buf);
		}

        if ( gGV_MsgToApp[port].opcode == RESOP_START_NTOCS_APP )
        {
            gGV_MsgToApp[port].returnCode = -1;
            sprintf(gGV_MsgToApp[port].message, "%.*s", 223, log_buf);
            (void)writeToNTApp (__LINE__, &(gGV_MsgToApp[port]));
            memset((struct MsgToApp *) &(gGV_MsgToApp[port]), '\0', sizeof(struct MsgToApp ));
        }

		exit (0);
	}											  /* fork */

	if (pid == -1)
	{
		sprintf (log_buf, "Failed to execute fork. [%d, %s]", errno, strerror(errno));
		resp_log (mod, port, REPORT_NORMAL, 3117, log_buf);
		if (gGV_CDFInfo[port].toFireOcsApp == 1)
		{
			sprintf (gResultLine, "%s", log_buf);
			sReportCallStatus (gResultLine, gGV_CDFInfo[port].cdfFileName, 20004);
			gGV_CDFInfo[port].toFireOcsApp = 0;
		}

		if (gGV_FAXInfo[port].toFireFaxApp == 1)
		{
			sprintf (gResultLine, "%s", log_buf);
			sReportCallStatus (gResultLine, gGV_FAXInfo[port].faxFileName, 20004);
			gGV_FAXInfo[port].toFireFaxApp = 0;
		}

        if ( gGV_MsgToApp[port].opcode == RESOP_START_NTOCS_APP )
        {
            gGV_MsgToApp[port].returnCode = -1;
            sprintf(gGV_MsgToApp[port].message, "%.*s", 223, log_buf);
            (void)writeToNTApp (__LINE__, &(gGV_MsgToApp[port]));
            memset((struct MsgToApp *) &(gGV_MsgToApp[port]), '\0', sizeof(struct MsgToApp ));
        }

		return (-1);
	}
	sprintf (log_buf, "Started application (%s), callnum=%d, password=%d, pid=%d dynmgr(#%d:%d)", appl_name, port, password, pid, i, GV_CurrentDMPid);
	resp_log (mod, port, REPORT_VERBOSE, 3117, log_buf);

    if ( gGV_MsgToApp[port].opcode == RESOP_START_NTOCS_APP )
    {
        gGV_MsgToApp[port].returnCode = 0;
        sprintf(gGV_MsgToApp[port].message, "%.*s", 223, log_buf);
        (void)writeToNTApp (__LINE__, &(gGV_MsgToApp[port]));
        memset((struct MsgToApp *) &(gGV_MsgToApp[port]), '\0', sizeof(struct MsgToApp ));
    }

	if (gGV_CDFInfo[port].toFireOcsApp == 1)
	{
		sprintf (gResultLine, "%s", log_buf);
		gGV_CDFInfo[port].toFireOcsApp = 0;
	}

	if(gGV_FAXInfo[port].toFireFaxApp == 1)
	{
		gGV_FAXInfo[port].toFireFaxApp = 0;
	}

	if (appl_pid[port] != 0)
	{
		char program_name[64];

		sprintf (tmpFifoName, "%s.%d", "/tmp/ResponseFifo", appl_pid[port]);
		if (access (tmpFifoName, F_OK) == 0)
		{
			unlink (tmpFifoName);
		}

		if (strcmp (resource[port].res_type, "IP") == 0 && strcmp (resource[port].res_state, "DYN_MGR") == 0
			&& strcmp (resource[port].res_usage, RESERVE_PORT) != 0)
		{
			read_fld (tran_tabl_ptr, port, APPL_NAME, program_name);
			application_instance_manager (ISP_DELETE_INSTANCE, program_name, port);
		}
	}

	sprintf (log_buf, "pid=%d port=%d", pid, port);
	resp_log (mod, port, REPORT_VERBOSE, 3117, log_buf);

	write_fld (tran_tabl_ptr, port, APPL_PID, pid);
	
	sprintf (log_buf, "%s", "setting APPL_STATUS, STATUS_BUSY");
	resp_log (mod, port, REPORT_VERBOSE, 3480, log_buf);
	
	write_fld (tran_tabl_ptr, port, APPL_STATUS, STATUS_BUSY);
	updateAppName (port, appl_name);
	appl_pid[port] = pid;
	appCount[port] += 1;

	return (ISP_SUCCESS);
}												  /* launchApp */


/*----------------------------------------------------------------------------
Send a message to the master VXML Browser process. The process will fork
and exec the child and send the pid back
----------------------------------------------------------------------------*/
int
launchBrowserApp (int port, char *appl_name, char *dnis, char *ani, int password, char *ringEventTime, char *acctName, char *appToAppInfoFile)
{
	int pid = 0;
	int buf_size = 0;			// bt-307
	char buf[750];
	char lpDynMgrId[5];
	char lAppToAppInfo[512] = "";
	int rc, i;
	char mod[] = { "launchBrowserApp" };
	char tmpFifoName[128];

	if (StartVXML2 == 0)			// BT-42
	{
		return(0);
	}

	if (gIsVXIPresent)
	{
		resp_log (mod, port, REPORT_VERBOSE, 3473, "VXI is present.");
	}

	if (strcmp (appl_name, "arcVXML") == 0)
	{
		rc = createRequestFifo (1, mod);
		if (rc != 0)
		{
			return (-1);
		}
	}
												  /*No VXI */
	else if (strcmp (appl_name, "arcVXML2") == 0 && !gIsVXIPresent)
	{
		rc = createRequestFifo (2, mod);
		if (rc != 0)
		{
			return (-1);
		}
	}
	else if (strcmp (appl_name, "arcVXML2") == 0 && gIsVXIPresent)
	{
		/*VXI*/ rc = createRequestFifo (3, mod);
		if (rc != 0)
		{
			return (-1);
		}
	}
	else
	{
		sprintf (log_buf, "Invalid application name (%s).", appl_name);
		resp_log (mod, port, REPORT_NORMAL, 3473, log_buf);
		return (-1);
	}

//sprintf (lAppToAppInfo, "%s", "|");
	if(gGV_CDFInfo[port].toFireOcsApp == 1)
	{
		sprintf (lAppToAppInfo, "%s|%s", gGV_CDFInfo[port].cdfFileName, gGV_CDFInfo[port].appToAppInfo);
	}
	else if(gGV_FAXInfo[port].toFireFaxApp == 1)
	{
		sprintf (lAppToAppInfo, "%s|%s", gGV_FAXInfo[port].faxFileName, gGV_FAXInfo[port].appToAppInfo);
	}

	for (i = dynMgrStartNum; i < GV_NumberOfDMRunning + dynMgrStartNum; i++)
	{
		if ((port >= gGV_DMInfo[i].startPort) && (port <= gGV_DMInfo[i].endPort))
		{
			GV_CurrentDMPid = gGV_DMInfo[i].pid;
			break;
		}
	}
	sprintf (lpDynMgrId, "%d", i);

	// bt-307
	memset((char *)buf, '\0', sizeof(buf));
	buf_size = sprintf (buf,
		"%d^%s^%s^%d^%s^%s^%s^%s^%s^%d^%s^%s^%s^%s^%s^%s^%s^%s^%s^%s^%s^%s^",
		port, appl_name, "-p", port, "-y", acctName,
		"-P", appl_name, "-z", password, "-D", dnis, "-A", ani,
		"-U", lAppToAppInfo, "-r", ringEventTime, "-d", lpDynMgrId,
		"-t", resource[port].res_usage);

	if (strcmp (appl_name, "arcVXML") == 0)
	{
		//rc = write (gArcVXMLRequestFifoFd, (char *) buf, strlen(buf)+1 );	// bt-307
		//rc = write (gArcVXMLRequestFifoFd, (char *) buf, buf_size);	// bt-307
		rc = write (gArcVXMLRequestFifoFd, (char *) buf, 256);	// bt-307 Write the whole structure size.  256 comes from ARCclient.h:#define RESP_TO_VXML2_MSG_SIZE 256
		if (rc == -1)
		{
			sprintf (log_buf, "Can't write to arcVXML app. [%d, %s]", errno, strerror(errno));
			resp_log (mod, port, REPORT_NORMAL, 3473, log_buf);
			return (-1);
		}
	}

	if (strcmp (appl_name, "arcVXML2") == 0)
	{
		if(gGV_CDFInfo[port].toFireOcsApp == 1)
		{
			gGV_CDFInfo[port].toFireOcsApp = 0;
		}

		if(gGV_FAXInfo[port].toFireFaxApp == 1)
		{
			gGV_FAXInfo[port].toFireFaxApp = 0;
		}

		if (gIsVXIPresent)
		{
			//rc = write (gArcVXML2RequestFifoFd[port / 48], (char *) buf, buf_size);	// bt-307
			/*VXI*/ rc = write (gArcVXML2RequestFifoFd[port / 48], (char *) buf, 256);	// bt-307 Write the whole structure size. 256 comes from ARCclient.h:#define RESP_TO_VXML2_MSG_SIZE 256
			if (rc >= 0)
			{
				sprintf (log_buf, "Wrote to arcVXML2 app. fd[%d]=%d. bufsize(%d)", port / 48, gArcVXML2RequestFifoFd[port / 48], buf_size);
				resp_log (mod, port, REPORT_VERBOSE, 3473, log_buf);
			}
		}
		else
		{
/*Non VXI VXML2 */
			//rc = write (gArcVXML2RequestFifoFd[0], (char *) buf, buf_size);	// bt-307
			rc = write (gArcVXML2RequestFifoFd[0], (char *) buf, 256);	// bt-307 Write the whole structure size. 256 comes from ARCclient.h:#define RESP_TO_VXML2_MSG_SIZE 256
		}

		if (rc == -1)
		{
			sprintf (log_buf, "Can't write to arcVXML2 app. [%d, %s]", errno, strerror(errno));
			resp_log (mod, port, REPORT_NORMAL, 3473, log_buf);
			return (-1);
		}
	}

	if (appl_pid[port] != 0)
	{
		char program_name[64];

		sprintf (tmpFifoName, "%s.%d", "/tmp/ResponseFifo", appl_pid[port]);
		if (access (tmpFifoName, F_OK) == 0)
		{
			unlink (tmpFifoName);
		}

		if (strcmp (resource[port].res_type, "IP") == 0 && strcmp (resource[port].res_state, "DYN_MGR") == 0
			&& strcmp (resource[port].res_usage, RESERVE_PORT) != 0)
		{
			read_fld (tran_tabl_ptr, port, APPL_NAME, program_name);
			application_instance_manager (ISP_DELETE_INSTANCE, program_name, port);
		}
		else if (strcmp (resource[port].res_type, "ANALOG") == 0 &&
			strcmp (resource[port].res_state, "DYN_MGR") == 0 && strcmp (resource[port].res_usage, RESERVE_PORT) != 0)
		{
			read_fld (tran_tabl_ptr, port, APPL_NAME, program_name);
			application_instance_manager (ISP_DELETE_INSTANCE, program_name, port);
		}
	}

	sprintf (gResultLine, "Started application (%s) ", appl_name);

	updateAppName (port, appl_name);

	return (ISP_SUCCESS);
}


void resp_log (char *mod, int port, int mode, int msgid, char *msg)
{
	char portNo[10];

	if (port == -1)
		sprintf (portNo, "%s", "-1");
	else
		sprintf (portNo, "%d", port);
	LogARCMsg (mod, mode, portNo, "RES", "ArcIPResp", msgid, msg);
}


/*-----------------------------------------------------------------------------
resp_shutdown():
-----------------------------------------------------------------------------*/
void
resp_shutdown ()
{
	static char mod[] = "resp_shutdown";
	int i, ret;

	for (i = 0; i < tran_proc_num; i++)
		tran_sig[i] = 2;
	write_arry (tran_tabl_ptr, APPL_SIGNAL, tran_sig);

	// BT-86 - changed the order.  Killing the arcVXML2 script before RunVXID
	if (StartVXML2 == 1)			// BT-42
	{
		for (i = 0; i < MAX_VXML_APPS; i++)
		{
			if (GV_arcVXML1Pid[i] > 0)
			{
				(void) kill (GV_arcVXML1Pid[i], SIGKILL);
				sprintf (log_buf, "%s Pid=%d", "Killing arcVXML2,", GV_arcVXML1Pid[i]);
				resp_log (mod, -1, REPORT_DETAIL, 3480, log_buf);
			}
		}
	
		for (i = 0; i < MAX_VXML_APPS; i++)
		{
			if (GV_arcVXML2Pid[i] > 0)
			{
				(void) kill (GV_arcVXML2Pid[i], SIGKILL);
				sprintf (log_buf, "%s Pid=%d", "Killing arcVXML2,", GV_arcVXML2Pid[i]);
					resp_log (mod, -1, REPORT_DETAIL, 3480, log_buf);
			}
		}
	}

	if(beginPort == 0)
	{
		if (StartVXML2 == 1)			// BT-42
		{
			for (i = 0; i < MAX_NUM_OF_DM; i++)
			{
				sprintf (log_buf, "%s", "calling checkAndKillRunVXID");
				resp_log (mod, -1, REPORT_VERBOSE, 3480, log_buf);
				ret = checkAndKillRunVXID ("./RunVXID");
				//sleep (1);
				if (ret == 0)
					break;
			}
		
			for (i = 0; i < 20; i++)
			{
				sprintf (log_buf, "%s", "calling checkAndKillRunVXI");
				resp_log (mod, -1, REPORT_VERBOSE, 3480, log_buf);
				ret = checkAndKillRunVXID ("./RunVXI");
				//sleep (1);
				if (ret == 0)
					break;
			}
		}

	}/*if(beginPort == 0)	*/

	unlink (respPidFile);


	if (beginPort == 0)
	{
		if (RedirectorPid > 0)
		{
			(void) kill (RedirectorPid, SIGKILL);
			sprintf (log_buf, "%s Pid=%d", "Killing ArcSipRedirector,", RedirectorPid);
			resp_log (mod, -1, REPORT_DETAIL, 3480, log_buf);
		}

		if (siproxdPid > 0)
		{
			(void) kill (siproxdPid, SIGKILL);
			sprintf (log_buf, "%s Pid=%d", "Killing arcsiproxd,", siproxdPid);
			resp_log (mod, -1, REPORT_DETAIL, 3480, log_buf);
		}

		if (ttsMgrPid > 0)
		{
			(void) kill (ttsMgrPid, SIGTERM);
			sprintf (log_buf, "%s Pid=%d", "Killing mrcpTTSClient2Mgr,", ttsMgrPid);
			resp_log (mod, -1, REPORT_DETAIL, 3480, log_buf);
		}

		if (srMgrPid > 0)
		{
			(void) kill (srMgrPid, SIGTERM);
			sprintf (log_buf, "%s Pid=%d", "Killing mrcpClient2Mgr,", srMgrPid);
			resp_log (mod, -1, REPORT_DETAIL, 3480, log_buf);
		}

		if (rtrClientPid > 0)
		{
			(void) kill (rtrClientPid, SIGTERM);
			sprintf (log_buf, "%s Pid=%d", "Killing arcRTRClient,", srMgrPid);
			resp_log (mod, -1, REPORT_DETAIL, 3480, log_buf);
		}
	}

	if (StartGoogleSR == 1)
	{
		if (GV_arcGoogleSRPid > 0)
		{
			(void) kill (GV_arcGoogleSRPid, SIGKILL);
			sprintf (log_buf, "%s Pid=%d", "Killing GoogleSpeechRec.jar", GV_arcGoogleSRPid);
			resp_log (mod, -1, REPORT_DETAIL, 3480, log_buf);

			ret = checkAndKillGoogleSRJar ("GoogleSpeechRec.jar");
		}
	}

	if (StartChatGPT == 1)
	{
		if (GV_arcChatGPTPid > 0)
		{
			(void) kill (GV_arcChatGPTPid, SIGKILL);
			sprintf (log_buf, "%s Pid=%d", "Killing chatGPT client manager ( arcChatGPTClient.sh ).", GV_arcChatGPTPid);
			resp_log (mod, -1, REPORT_DETAIL, 3480, log_buf);

			ret = checkAndKillChatGPT ("arcChatGPTClient");
		}
	}

	sprintf (log_buf, "%s", "Responsibility received termination signal... Shutting down.");
	resp_log (mod, -1, REPORT_NORMAL, 3119, log_buf);

	exit (0);
}												  /* resp_shutdwon */


/*-----------------------------------------------------------------------------
checkAppForGivenPort():This routine check for all application dead and does cleanup.
-----------------------------------------------------------------------------*/
int checkAppForGivenPort (int portNo)
{
	static char mod[] = "checkAppForGivenPort";
	char pid_str[10];
	int int_pid;
	int i, j;
	char tmpFifoName[128];
	char program_name[MAX_PROGRAM];
	int dynMgrDead = -1;

	read_fld (tran_tabl_ptr, portNo, APPL_PID, pid_str);
	int_pid = atoi (pid_str);

	if (int_pid == 0)
	{
/*DDN 12/02/2007 : Added for MR2078. */
		if (gInstanceStatus[portNo].status == ISP_ADD_INSTANCE)
		{
			sprintf (program_name, "%s", gInstanceStatus[portNo].program_name);
			sprintf (log_buf, "MR2078: Deleting app instance for port %d since pid is 0.", portNo);
			resp_log (mod, portNo, REPORT_DETAIL, 3439, log_buf);

			application_instance_manager (ISP_DELETE_INSTANCE, gInstanceStatus[portNo].program_name, portNo);
		}
		return (1);
	}

	if (int_pid > 0)
	{
		for (j = dynMgrStartNum; j < GV_NumberOfDMRunning + dynMgrStartNum; j++)
		{
			if (gGV_DMInfo[j].pid == int_pid)
			{
				return (1);
			}
		}
		sprintf(log_buf, "%s%d", "killing int_pid=", int_pid);
		resp_log(mod, -1, REPORT_VERBOSE, 3480, log_buf);
		if (kill (int_pid, 0) == -1)			  /* check application exists */
		{
			if (errno == ESRCH)					  /* No process */
			{
				for (j = dynMgrStartNum; j < GV_NumberOfDMRunning + dynMgrStartNum; j++)
				{
					if (gGV_DMInfo[j].pid == int_pid)
					{
						dynMgrDead = j;
						break;
					}
				}
				if (dynMgrDead == -1)
				{
					(void) cleanupApp (__LINE__, int_pid, portNo);
					return (1);
				}
			}
		}										  /* kill */
	}
	return (0);
}


int
checkIfVXIPresent ()
{
	static char mod[] = "checkIfVXIPresent";
	char yStrCheckCommand[256];
	FILE *fin = NULL;
	char buf[BUFSIZE];

	if (StartVXML2 == 0)			// BT-42
	{
		return(0);
	}

	sprintf (yStrCheckCommand, "%s", "./arcVXML2 -v");

	if (access ("arcVXML2", F_OK) != 0)
	{
		return 0;
	}

	if ((fin = popen (yStrCheckCommand, "r")) == NULL)
	{
		return 0;
	}

	fgets (buf, sizeof buf, fin);

	(void) pclose (fin);
	fin = NULL;

	if (buf[0] && strstr (buf, "VXI") != NULL)
	{
		return (1);
	}
	else
	{
		return (0);
	}
}


/*-----------------------------------------------------------------------------
check_kill_app():This routine check for all application dead and does cleanup.
-----------------------------------------------------------------------------*/
void
check_kill_app ()
{
	static char mod[] = "check_kill_app";
	register int appl_no;
	char pid_str[10];
	int int_pid;
	int i, j;
	int dynMgrDead = -1;
	char tmpFifoName[128];
	char program_name[MAX_PROGRAM];
	char tmpProcName[256];
	time_t tmpTime;
	char yStrDefunctStr[256];

	FILE *fin;									  /* file pointer for the ps pipe */
	char buf[BUFSIZE];
	char buf1[128];
	int defunctDynMgr = -1;
	int defunctMediaMgr = -1;
    int defunctRunVXI_1 = -1;
    int defunctRunVXI_2 = -1;
	int startPort;

	time (&(tmpTime));

	if ((tmpTime - 300) > defunctCheckTime)
	{
		time (&(defunctCheckTime));

		for (j = dynMgrStartNum; j < GV_NumberOfDMRunning + dynMgrStartNum; j++)
		{
			memset (buf, 0, BUFSIZE);
			memset (buf1, 0, 128);

			sprintf (yStrDefunctStr, "ps -fp %d | grep ArcIPDynMgr | grep \"\\-s %d\" | grep defunct | grep -v grep", gGV_DMInfo[j].pid, j * 48);

			if ((fin = popen (yStrDefunctStr, "r")) != NULL)
			{
				fgets (buf, sizeof buf, fin);
				sscanf (buf, "%s %d", buf1, &defunctDynMgr);
				(void) pclose (fin);
				fin = NULL;

				if (defunctDynMgr > 0 && defunctDynMgr == gGV_DMInfo[j].pid)
				{
					sprintf (log_buf, "DynMgr found in defunct state. pid=%d. Restarting.", gGV_DMInfo[j].pid);
					resp_log (mod, -1, REPORT_NORMAL, 3483, log_buf);

/*Kill corresponding media mgr too */
					memset (buf, 0, BUFSIZE);
					memset (buf1, 0, 128);

					if ((fin = popen (yStrDefunctStr, "r")) != NULL)
					{
						sprintf (yStrDefunctStr, "ps -ef | grep ArcSipMediaMgr | grep \"\\-s %d\" | grep %d | grep -v grep", j * 48, gGV_DMInfo[j].pid);
						fgets (buf, sizeof buf, fin);
						sscanf (buf, "%s %d", buf1, &defunctMediaMgr);
						(void) pclose (fin);
						fin = NULL;
						if (defunctMediaMgr > 0)
						{
							sprintf (log_buf, "Stopping Media Manager pid=%d.", defunctMediaMgr);
							resp_log (mod, -1, REPORT_NORMAL, 3483, log_buf);

							kill (defunctMediaMgr, SIGTERM);
						}
					}
/*END: Kill corresponding media mgr too */

					for (i = gGV_DMInfo[j].startPort; i <= gGV_DMInfo[j].endPort; i++)
					{
						read_fld (tran_tabl_ptr, i, APPL_PID, pid_str);
						int_pid = atoi (pid_str);
						resource[i].app_password = NULL_PASSWORD;
						if ((int_pid > 0) && (int_pid != gGV_DMInfo[j].pid))
						{
					sprintf (log_buf, "Sending SIGKILL pid=%d.", int_pid);
					resp_log (mod, -1, REPORT_DETAIL, 3483, log_buf);


							(void) kill (int_pid, SIGKILL);

							read_fld (tran_tabl_ptr, i, APPL_NAME, program_name);
							if (strcmp (resource[i].res_type, "IP") == 0 && strcmp (resource[i].res_state, "DYN_MGR") == 0
								&& strcmp (resource[i].res_usage, RESERVE_PORT) != 0)
							{
								application_instance_manager (ISP_DELETE_INSTANCE, program_name, i);
							}
							res_status[i].status = FREE;
						}
					}

					sprintf (log_buf, "DynMgr has stopped. pid=%d. Restarting.", gGV_DMInfo[j].pid);
					resp_log (mod, -1, REPORT_NORMAL, 3483, log_buf);

					if(gGV_DMInfo[j].fd  > 0)
					{
						close(gGV_DMInfo[j].fd);
					}

					gGV_DMInfo[j].fd = 0;

					sprintf (tmpFifoName, "/tmp/RequestFifo.%d", gGV_DMInfo[j].pid);
					unlink (tmpFifoName);

					load_shmem_tabl (tran_shmid, gGV_DMInfo[j].startPort, gGV_DMInfo[j].endPort);
					gGV_DMInfo[j].startTime = 0;
					gGV_DMInfo[j].stopTime = time (0);

					// StartUpDynMgr(j);

				}								  /*END: if defunctDynMgr > 0 */

			}									  /*END: if fin */

		}										  /*END: for */

	}											  /*END: if tmpTime */

	j = 0;

												  /* read pid field */
	for (appl_no = beginPort; appl_no < tran_proc_num + beginPort; appl_no++)
	{
		read_fld (tran_tabl_ptr, appl_no, APPL_PID, pid_str);
		int_pid = atoi (pid_str);
		if (int_pid > 0)
		{
			sprintf (tmpProcName, "/proc/%d", int_pid);

												  /* check application exists */
			if (kill (int_pid, 0) == -1 || access (tmpProcName, F_OK) != 0)
			{
												  /* No process */
				if (errno == ESRCH || access (tmpProcName, F_OK) != 0)
				{
					for (j = dynMgrStartNum; j < GV_NumberOfDMRunning + dynMgrStartNum; j++)
					{
						if (gGV_DMInfo[j].pid == int_pid)
						{
							sprintf (log_buf, "[%d] dynamic manager %d, pid = %d", __LINE__, j, gGV_DMInfo[j].pid);
							resp_log (mod, -1, REPORT_VERBOSE, 3118, log_buf);
							dynMgrDead = j;
							break;
						}
					}
					if (dynMgrDead == -1)
					{
						(void) cleanupApp (__LINE__, int_pid, appl_no);
//break;
					}
				}
			}									  /* kill */
		}
	}											  /* for */

	if (beginPort == 0)
	{
/* following code is for network services */
		if (StartNetworkServices == 1)			  /* if network service exists */
		{
												  /* check application exists */
			if (kill (NetworkServicesPid, 0) == -1)
			{
				if (errno == ESRCH)				  /* No process *//* restart network services */
				{
					sprintf (log_buf, "Network services for %s server stopped. pid=%d.", object_name, NetworkServicesPid);
					resp_log (mod, -1, REPORT_VERBOSE, 3118, log_buf);
					start_netserv ();
				}
			}
		}										  /* if network services */

		if (StartResourceMgr == 1)
		{
			if (kill (ResourceMgrPid, 0) == -1)
			{
				if (errno == ESRCH)				  /* No process */
				{
					sprintf (log_buf, "ResourceMgr has stopped. pid=%d. Restarting.", ResourceMgrPid);
					resp_log (mod, -1, REPORT_NORMAL, 3119, log_buf);
					StartUpResourceMgr ();
				}
			}
		}

		if (StartSiproxd == 1)
		{
			if (kill (siproxdPid, 0) == -1)
			{
				if (errno == ESRCH)				  /* No process */
				{
					sprintf (log_buf, "arcsiproxd has stopped. pid=%d. Restarting.", siproxdPid);
					resp_log (mod, -1, REPORT_NORMAL, 3119, log_buf);
					StartUpSipProxy (1);
				}
			}
		}

		if (StartRedirector == 1)
		{
			if (kill (RedirectorPid, 0) == -1)
			{
				if (errno == ESRCH)				  /* No process */
				{
					sprintf (log_buf, "ArcSipRedirector has stopped. pid=%d. Restarting.", RedirectorPid);
					resp_log (mod, -1, REPORT_NORMAL, 3119, log_buf);
					StartUpRedirector (1);
				}
			}
		}

		if (StartTTSMgr == 1)
		{
			if (kill (ttsMgrPid, 0) == -1)
			{
				if (errno == ESRCH)				  /* No process */
				{
					sprintf (log_buf, "mrcpTTSClient2Mgr has stopped. pid=%d. Restarting.", ttsMgrPid);
					resp_log (mod, -1, REPORT_NORMAL, 3119, log_buf);
					StartUpTTSMgr ();
				}
			}
		}

		if (StartSRMgr == 1)			// BT-42
		{
			if (kill (srMgrPid, 0) == -1)
			{
				if (errno == ESRCH)				  /* No process */
				{
					sprintf (log_buf, "mrcpClient2Mgr has stopped. pid=%d. Restarting.", srMgrPid);
					resp_log (mod, -1, REPORT_NORMAL, 3119, log_buf);
					StartUpSRMgr ();
				}
			}
		}

		if (StartRTRClient == 1)
		{
			if (kill (rtrClientPid, 0) == -1)
			{
				if (errno == ESRCH)               /* No process */
				{
					sprintf (log_buf, "arcRTRClient has stopped. pid=%d. Restarting.", rtrClientPid);
					resp_log (mod, -1, REPORT_NORMAL, 3119, log_buf);
					StartUpRTRClient ();
				}
			}
		} 

		if ( StartGoogleSR == 1 )
		{
			if (kill (GV_arcGoogleSRPid, 0) == -1)
			{
				if (errno == ESRCH)				  /* No process */
				{
					sprintf (log_buf, "Google speech client has stopped. pid=%d. Restarting.", GV_arcGoogleSRPid);
					resp_log (mod, -1, REPORT_NORMAL, 3119, log_buf);
					startGoogleSRProcess (0);
				}
			}
		}

		if ( StartChatGPT == 1 )
		{
			if (kill (GV_arcChatGPTPid, 0) == -1)
			{
				if (errno == ESRCH)				  /* No process */
				{
					sprintf (log_buf, "ChatGPT AI client has stopped. pid=%d. Restarting.", GV_arcChatGPTPid);
					resp_log (mod, -1, REPORT_NORMAL, 3119, log_buf);
					startChatGPTProcess (0);
				}
			}
		}
	}

	if (StartVXML2 == 1)			// BT-42
	{
		for (i = 0; i < MAX_VXML_APPS; i++)
		{
			if (GV_arcVXML1Pid[i] != 0)
			{
				if (kill (GV_arcVXML1Pid[i], 0) == -1)
				{
					if (errno == ESRCH)				  /* No process */
					{
						GV_arcVXML1Pid[i] = 0;
					}
				}
			}
		}
	
		for (i = 0; i < MAX_VXML_APPS; i++)
		{
			if (GV_arcVXML2Pid[i] != 0)
			{
				if (kill (GV_arcVXML2Pid[i], 0) == -1)
				{
					if (errno == ESRCH)				  /* No process */
					{
						startVXML2Process(i);
					}
				}
			}
		}
	}

	if (StartDynMgr == 1)
	{
		if (dynMgrDead != -1)
		{
			j = dynMgrDead;
			if (gGV_DMInfo[j].startTime == 0)
			{
				return;
			}


			if ( (!gIsVXIPresent) || (StartVXML2 == 0) )
			{
				for (i = gGV_DMInfo[j].startPort; i <= gGV_DMInfo[j].endPort; i++)
				{
					read_fld (tran_tabl_ptr, i, APPL_PID, pid_str);
					int_pid = atoi (pid_str);
					resource[i].app_password = NULL_PASSWORD;
	
					if ((int_pid > 0) && (int_pid != gGV_DMInfo[j].pid))
					{
						if (!gIsVXIPresent)
						{
						sprintf (log_buf, "Sending SIGKILL pid=%d.", int_pid);
						resp_log (mod, -1, REPORT_NORMAL, 3483, log_buf);
						(void) kill (int_pid, SIGKILL);
						}
					}
					read_fld (tran_tabl_ptr, i, APPL_NAME, program_name);
					if (strcmp (resource[i].res_type, "IP") == 0 && strcmp (resource[i].res_state, "DYN_MGR") == 0
						&& strcmp (resource[i].res_usage, RESERVE_PORT) != 0)
					{
						application_instance_manager (ISP_DELETE_INSTANCE, program_name, i);
					}
					res_status[i].status = FREE;
				}
			}
			else
			{
				i = gGV_DMInfo[j].startPort / 48;
				if(GV_arcVXML2Pid[i] > 0)
				{
					// First kill the RunVXI processes
#if 0
					sprintf (yStrDefunctStr, "ps -ef | grep RunVXI | grep \"\\-id %d\"", i);
					if ((fin = popen (yStrDefunctStr, "r")) != NULL)
					{
						fgets (buf, sizeof(buf), fin);
						sscanf (buf, "%s %d", buf1, &defunctRunVXI_1);

						fgets (buf, sizeof(buf), fin);
						sscanf (buf, "%s %d", buf1, &defunctRunVXI_2);
						(void) pclose (fin);
						if (defunctRunVXI_1 > 0)
						{
							sprintf (log_buf, "Stopping RunVXI pid=%d.", defunctRunVXI_1);
							resp_log (mod, -1, REPORT_NORMAL, 3483, log_buf);
							kill (defunctRunVXI_1, SIGKILL);
						}
						if (defunctRunVXI_2 > 0)
						{
							sprintf (log_buf, "Stopping RunVXI pid=%d.", defunctRunVXI_2);
							resp_log (mod, -1, REPORT_NORMAL, 3483, log_buf);
							kill (defunctRunVXI_2, SIGKILL);
						}
					}
					sprintf (log_buf, "Stopping arcVXML2 pid=%d.", GV_arcVXML2Pid[i]);
					resp_log (mod, -1, REPORT_NORMAL, 3483, log_buf);
					(void) kill (GV_arcVXML2Pid[i], SIGKILL);
#endif

					for (i = gGV_DMInfo[j].startPort; i <= gGV_DMInfo[j].endPort; i++)
					{
						read_fld (tran_tabl_ptr, i, APPL_NAME, program_name);
						if ( (strcmp (program_name, "arcVXML2") == 0) && 
						     (strcmp (resource[i].res_usage, RESERVE_PORT) != 0) )
						{
							application_instance_manager (ISP_DELETE_INSTANCE, program_name, i);
						}
						res_status[i].status = FREE;
					}
				}
			}
			sprintf (log_buf, "DynMgr has stopped. pid=%d. Restarting.", gGV_DMInfo[j].pid);
			resp_log (mod, -1, REPORT_NORMAL, 3483, log_buf);

			if(gGV_DMInfo[j].fd  > 0)
			{
				close(gGV_DMInfo[j].fd);
			}

			gGV_DMInfo[j].fd = 0;

			sprintf (tmpFifoName, "/tmp/RequestFifo.%d", gGV_DMInfo[j].pid);
			unlink (tmpFifoName);

			load_shmem_tabl (tran_shmid, gGV_DMInfo[j].startPort, gGV_DMInfo[j].endPort);
			gGV_DMInfo[j].startTime = 0;
			gGV_DMInfo[j].stopTime = time (0);
		}
	}
	return;
}												  /* check_kill_app */


/*------------------------------------------------------------------------------
This routine is called when the application gets killed/terminated. This module updates the variable indicating the death of the application.
------------------------------------------------------------------------------*/
void
cleanupApp (int zLine, int app_pid, int appl_no)
{
	char program_name[MAX_PROGRAM];
	static char mod[] = "cleanupApp";
	char res_id[10];
	char status_str[10];
	char appSignal[10];
	int i;
	struct Msg_AppDied lMsg_AppDied;
	char tmpFifoName[128];

/* read program name and resource */
	read_fld (tran_tabl_ptr, appl_no, APPL_NAME, program_name);

	sprintf (log_buf, "[%d, from %d] Entering cleanupApp. app_pid=%d,appl_no=%d. program_name=%s",
					__LINE__, zLine, app_pid, appl_no, program_name);
	resp_log (mod, appl_no, REPORT_DETAIL, 3120, log_buf);

    //added joes debug 
#if 0
    if(kill(app_pid, 0) == 0){
      fprintf(stderr, " %s() app is still alive though I am told to clean it up! pid=%d\n", __func__, app_pid);
    } else {
      ;; // fprintf(stderr, " %s() app is really dead %d\n", __func__, app_pid);
    }
#endif


	for (i = dynMgrStartNum; i < GV_NumberOfDMRunning + dynMgrStartNum; i++)
	{
		if ((appl_no >= gGV_DMInfo[i].startPort) && (appl_no <= gGV_DMInfo[i].endPort))
		{
			GV_CurrentDMPid = gGV_DMInfo[i].pid;
//			sprintf (log_buf, "[%d, from %d] DJB: Set GV_CurrentDMPid to %d from gGV_DMInfo[%d].pid(%d).",
//						__LINE__, zLine, GV_CurrentDMPid, i, gGV_DMInfo[i].pid);
//			resp_log (mod, -1, REPORT_VERBOSE, 3117, log_buf);
			break;
		}
	}

	read_fld (tran_tabl_ptr, appl_no, APPL_STATUS, status_str);
//if((atoi(status_str)!=STATUS_OFF)&&(atoi(status_str)!=STATUS_CHNOFF))
//write_fld(tran_tabl_ptr, appl_no, APPL_STATUS, STATUS_INIT);

	sprintf (log_buf, "[%d, from %d] pid=%d port=%d", __LINE__, zLine, 0, appl_no);
	resp_log (mod, appl_no, REPORT_DETAIL, 3117, log_buf);

	write_fld (tran_tabl_ptr, appl_no, APPL_API, 0);
	write_fld (tran_tabl_ptr, appl_no, APPL_PID, 0);
	read_fld (tran_tabl_ptr, appl_no, APPL_SIGNAL, appSignal);
	if (atoi (appSignal) != 2)
	{
		memset (&lMsg_AppDied, 0, sizeof (struct Msg_AppDied));
		lMsg_AppDied.opcode = DMOP_APPDIED;
		lMsg_AppDied.appCallNum = appl_no;
		lMsg_AppDied.appPid = app_pid;
		lMsg_AppDied.appRef = 0;
		lMsg_AppDied.appPassword = 0;
		lMsg_AppDied.returnCode = 0;
		memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
		memcpy (&gMsgToDM, &lMsg_AppDied, sizeof (struct Msg_AppDied));

		writeToDynMgr (__LINE__, mod, &gMsgToDM, GV_CurrentDMPid);
	}

	sprintf (tmpFifoName, "%s.%d", "/tmp/ResponseFifo", app_pid);
	if (access (tmpFifoName, F_OK) == 0)
	{
		unlink (tmpFifoName);
	}

	write_fld (tran_tabl_ptr, appl_no, APPL_SIGNAL, 1);
	update_DNIS (appl_no, "0000000000");
	if (appl_pid[appl_no] == app_pid)
		appl_pid[appl_no] = 0;

	if ((strcmp (resource[appl_no].res_state, "STATIC") == 0) && (strcmp (resource[appl_no].res_type, "IP") == 0))
	{
		if (appStatus[appl_no] == 3)
		{
			sprintf (log_buf, "[%d, from %d] pid=%d port=%d", __LINE__, zLine, 0, appl_no);
			resp_log (mod, appl_no, REPORT_DETAIL, 3117, log_buf);
			write_fld (tran_tabl_ptr, appl_no, APPL_PID, 0);
			appStatus[appl_no] = 0;
			write_fld (tran_tabl_ptr, appl_no, APPL_API, 0);
			updateAppName (appl_no, "N/A");
			startStaticApp (appl_no);
		}
		else
		{
			updateAppName (appl_no, "N/A");
		}
		return;
	}
	else
	{
		sprintf (log_buf, "[%d, from %d] pid=%d port=%d", __LINE__, zLine, GV_CurrentDMPid, appl_no);
		resp_log (mod, appl_no, REPORT_DETAIL, 3117, log_buf);
		write_fld (tran_tabl_ptr, appl_no, APPL_PID, GV_CurrentDMPid);
		updateAppName (appl_no, resource[appl_no].static_dest);

		application_instance_manager (ISP_DELETE_INSTANCE, program_name, appl_no);
		res_status[appl_no].status = FREE;
	}

	return;
}												  /* cleanupApp */


/*----------------------------------------------------------------------------
int set_object_path()
---------------------------------------------------------------------------*/
int
set_object_path ()
{
	static char mod[] = "set_object_path";
	char server_dir[12];
	struct utsname sys_info;
	struct rlimit limits;

	sprintf (mod, "%s", "set_object_path");
/* get machine name on which object is running */
	(void) uname (&sys_info);
	sprintf (log_buf, "Machine Name = %s", sys_info.nodename);
	resp_log (mod, -1, REPORT_VERBOSE, 3122, log_buf);
	resp_log (mod, -1, REPORT_VERBOSE, 3123, "Setting directory path structure");
/* set default variables */
	sprintf (DefaultLang, "%s", "AMENG");
	sprintf (NetworkStatus, "%s", "OFF");
	sprintf (diag_flag, "%s", "0");

	sprintf (object_machine_name, "%s", sys_info.nodename);

	sprintf (server_dir, "%s", TELECOM_DIR);

	sprintf (table_path, "%s/%s/%s", isp_base, server_dir, TABLE_DIR);
	sprintf (exec_path, "%s/%s/%s", isp_base, server_dir, EXEC_DIR);
	sprintf (global_exec_path, "%s/%s/%s", isp_base, GLOBAL_DIR, EXEC_DIR);
	sprintf (lock_path, "%s/%s/%s", isp_base, server_dir, LOCK_DIR);

/* set configuration table files path */
	sprintf (schedule_tabl, "%s/%s", table_path, "schedule");
	sprintf (resource_tabl, "%s/%s", table_path, "ResourceDefTab");
	sprintf (appref_tabl, "%s/%s", table_path, "appref");
	sprintf (pgmref_tabl, "%s/%s", table_path, "pgmreference");

    sprintf (ntAppCfg, "%s/%s", exec_path, "NTApp.cfg");

	resp_log (mod, -1, REPORT_VERBOSE, 3124, exec_path);
	resp_log (mod, -1, REPORT_VERBOSE, 3125, table_path);
	resp_log (mod, -1, REPORT_VERBOSE, 3126, lock_path);
/* set resource limit for number of file open */
	(void) getrlimit (RLIMIT_NOFILE, &limits);
	limits.rlim_cur = limits.rlim_max;
	(void) setrlimit (RLIMIT_NOFILE, &limits);
/* set session leader as responsibility */
	setsid ();
	return (ISP_SUCCESS);
}												  /* set_object_path */


/*-------------------------------------------------------------------
load_resource_table(): This Routine load resource table into memory.
--------------------------------------------------------------------*/
int
load_resource_table (char *resource_file)
{
	static char mod[] = "load_resource_table";
	FILE *fp;
	char record[256];
	char field[256];
	int field_no = 0;
	int load_entry = 0;							  /* decide whether to load entry */
	int currentPort = -1;

	sprintf (log_buf, "load_resource_table(): Loading %s table", resource_file);
	resp_log (mod, -1, REPORT_VERBOSE, 3127, log_buf);

	if ((fp = fopen (resource_file, "r")) == NULL)
	{
		sprintf (log_buf, "Can't open file %s, [%d, %s]", resource_file, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3128, log_buf);
		return (ISP_FAIL);
	}

	while (fgets (record, sizeof (record), fp) != NULL)
	{
		currentPort = atoi (record);

		if (currentPort < beginPort)
		{
			continue;
		}

		if (gRespId != -1)
		{
			if (currentPort > lastPort)
			{
				break;
			}
		}

		resource[tot_resources + beginPort].app_password = NULL_PASSWORD;

		load_entry = 1;							  /* load the entry */

		for (field_no = 1; field_no <= MAX_RESOURCE_FIELD; field_no++)
		{
			field[0] = '\0';

			if (get_field (record, field_no, field) < 0)
			{
				sprintf (log_buf, "Corrupted line in (%s). " "Unable to read field no %d, record no. %d. Line is: (%s)", resource_file, field_no, tot_resources,
					record);
				resp_log (mod, -1, REPORT_NORMAL, 3129, log_buf);

				load_entry = 0;					  /* don't load entry */
			}

			if (load_entry == 0)				  /* skip record for errors */
				break;
			switch (field_no)
			{
				case RESOURCE_NO:				  /* port no */
					sprintf (resource[tot_resources + beginPort].res_no, "%.*s", 29, field);
					if ((int) strlen (resource[tot_resources + beginPort].res_no) > fld_siz[APPL_RESOURCE - 1])
					{
						sprintf (log_buf, "Ignoring entry ResourceDefTab (%s), Resource name %s is > maximum resource length %d", record, field,
							fld_siz[APPL_RESOURCE - 1]);
						resp_log (mod, -1, REPORT_NORMAL, 3130, log_buf);
						continue;
					}
					break;
				case RESOURCE_TYPE:
					sprintf (resource[tot_resources + beginPort].res_type, "%.*s", 19, field);
					break;
				case RESOURCE_STATE:
					sprintf (resource[tot_resources + beginPort].res_state, "%.*s", 19, field);
					break;
				case RESOURCE_USAGE:
					sprintf (resource[tot_resources + beginPort].res_usage, "%.*s", 19, field);
					break;
				case STATIC_DEST:
					sprintf (resource[tot_resources + beginPort].static_dest, "%.*s", 19, field);
					break;
				default:
					sprintf (log_buf, "File = %s, Field %d is invalid field number", resource_file, field_no);
					resp_log (mod, -1, REPORT_NORMAL, 3131, log_buf);
					break;
			}									  /*switch */
		}										  /* for */
		if (load_entry == 0)
			continue;							  /* skip entry */
#ifdef DEBUG_TABLES
		sprintf (log_buf, "Resource no = %s", resource[tot_resources + beginPort].res_no);
		resp_log (mod, -1, REPORT_VERBOSE, 3132, log_buf);
		sprintf (log_buf, "Resource type = %s", resource[tot_resources + beginPort].res_type);
		resp_log (mod, -1, REPORT_VERBOSE, 3133, log_buf);
		sprintf (log_buf, "Resource state = %s", resource[tot_resources + beginPort].res_state);
		resp_log (mod, -1, REPORT_VERBOSE, 3134, log_buf);
		sprintf (log_buf, "Resource usage = %s", resource[tot_resources + beginPort].res_usage);
		resp_log (mod, -1, REPORT_VERBOSE, 3135, log_buf);
		sprintf (log_buf, "Resource static destination = %s", resource[tot_resources + beginPort].static_dest);
		resp_log (mod, -1, REPORT_VERBOSE, 3136, log_buf);
		sprintf (log_buf, "%s", "***************  next resource **************");
		resp_log (mod, -1, REPORT_VERBOSE, 3137, log_buf);
#endif
/* for reserve resource make resource status free */
		if (strcmp (resource[tot_resources + beginPort].res_usage, RESERVE_PORT) == 0)
			res_status[atoi (resource[tot_resources + beginPort].res_no)].status = FREE;
		tot_resources++;
	}											  /* while end of file */

	(void) fclose (fp);

	sprintf (log_buf, "Total entries loaded from resource table = %d.", tot_resources);
	resp_log (mod, -1, REPORT_VERBOSE, 3138, log_buf);

/* Adjust ports for number licensed */
	if (tot_resources > Licensed_Resources)
	{
		sprintf (log_buf, "Only %d resources of %d defined are licensed.", Licensed_Resources, tot_resources);
        // with this enabled the total number of ports started == the number of ports licenced.
        // this line was commented out before at some point. Joe S. Tue Mar 31 08:08:20 EDT 2015
		// tot_resources = Licensed_Resources;
		// numOfAppsAllowed = Licensed_Resources;
		resp_log (mod, -1, REPORT_NORMAL, 3139, log_buf);
	}

	initDynMgrStruct ();

	return (ISP_SUCCESS);
}												  /* load_resource_table */


/*--------------------------------------------------------------
This routine gets the value of desired field from data record.
---------------------------------------------------------------*/
int
get_field (buffer, fld_num, field)
const char buffer[];							  /* data record */
int fld_num;									  /* field # in the buffer */
char field[];									  /* buffer to fill with the field name */
{
	register int i;
	int fld_len = 0;							  /* field length */
	int state = OUT;							  /* current state IN or OUT */
	int wc = 0;									  /* word count */

	field[0] = '\0';
	if (fld_num < 0)
	{
		return (-1);
	}

	for (i = 0; i < (int) strlen (buffer); i++)
	{
		if (buffer[i] == FIELD_DELIMITER || buffer[i] == '\n')
		{
			state = OUT;
			if (buffer[i] == FIELD_DELIMITER && buffer[i - 1] == FIELD_DELIMITER)
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
	}											  /* for */

	if (fld_len > 0)							  /*  for last field */
	{
		field[fld_len] = '\0';
		while (field[0] == ' ')
		{
			for (i = 0; field[i] != '\0'; i++)
				field[i] = field[i + 1];
		}
		fld_len = strlen (field);
		return (fld_len);						  /* return field length */
	}
	return (-1);								  /* return error */
}												  /* get_field() */


/*-----------------------------------------------------------------
update_DNIS(): update the application into shared memory
------------------------------------------------------------------*/
int
update_DNIS (int shm_index, char *did)
{
	static char mod[] = "update_DNIS";
	char dnis[MAX_PROGRAM];
	char *ptr, *ptr1;

												  /* don't update did */
	if ((int) strlen (did) > fld_siz[APPL_FIELD6 - 1])
	{
		return (0);
	}
	sprintf (dnis, "%s", did);
	ptr = tran_tabl_ptr;
	ptr += (shm_index * SHMEM_REC_LENGTH);
/* position the pointer to the field */
	ptr += vec[APPL_FIELD6 - 1];				  /* application start index */
	ptr1 = ptr;
	(void) memset (ptr1, ' ', fld_siz[APPL_FIELD6 - 1]);
	ptr += (fld_siz[APPL_FIELD6 - 1] - strlen (dnis));
	(void) memcpy (ptr, dnis, strlen (dnis));
	return (0);
}												  /* update_DNIS */


/*--------------------------------------------------------------------------
application_instance_manager(): this routine update instance for application
----------------------------------------------------------------------------*/
int
application_instance_manager (int command, char *program_name, int res_id)
{
	static char mod[] = "application_instance_manager";
	int i, j, found = ISP_NOTFOUND;
	char appl_grp_name[MAX_GROUP];
	char tmp_pgm_name[MAX_PROGRAM];

	for (i = 0; i < tot_pgmrefs; i++)			  /* find program(basename) in program reference */
	{
		sprintf (tmp_pgm_name, "%s", program_name);
		if (strcmp (tmp_pgm_name, pgmref[i].program_name) == 0)
		{
			sprintf (appl_grp_name, "%s", pgmref[i].appl_grp_name);
			found = ISP_FOUND;
			break;
		}
	}
	if (found != ISP_FOUND)						  /* instance may is dynamic manager */
	{
		return (-1);
	}
	found = ISP_NOTFOUND;
/* find  group in application reference */
	for (j = 0; j < tot_apprefs; j++)
	{
		if (strcmp (appl_grp_name, appref[j].appl_grp_name) == 0)
		{
			found = ISP_FOUND;
			break;
		}
	}											  /* for */

	if (found != ISP_FOUND)
	{
		if (database_hit == 1)
		{
			sprintf (log_buf, "Can't find program group (%s) in application reference table", appl_grp_name);
			resp_log (mod, res_id, REPORT_NORMAL, 3140, log_buf);
		}
		return (-1);
	}

	switch (command)
	{
		case ISP_ADD_INSTANCE:
			appref[j].curr_instance = appref[j].curr_instance + 1;
			sprintf (log_buf, "Add instance for program (%s), resource <%d>,. Max instance=%d. Current instance=%d, %d.",
				program_name, res_id, appref[j].max_instance, appref[j].curr_instance, numOfAppsAllowed);
			resp_log (mod, res_id, REPORT_DETAIL, 3504, log_buf);
			numOfAppsAllowed = numOfAppsAllowed + 1;

/*DDN: Added for MR 2078 */
			if (command == gInstanceStatus[res_id].status)
			{
				sprintf (log_buf, "MR2078: Conflict in adding instance for port %d.", res_id);
				resp_log (mod, res_id, REPORT_NORMAL, 3439, log_buf);
			}

												  /*DDN: Added for MR2078 */
			gInstanceStatus[res_id].status = command;
			sprintf (gInstanceStatus[res_id].program_name, "%s", program_name);

			break;
		case ISP_DELETE_INSTANCE:
			appref[j].curr_instance = appref[j].curr_instance - 1;
			sprintf (log_buf, "Delete instance for program (%s), resource <%d>,. Max instance=%d. Current instance=%d, %d.",
				program_name, res_id, appref[j].max_instance, appref[j].curr_instance, numOfAppsAllowed);
			resp_log (mod, res_id, REPORT_DETAIL, 3504, log_buf);
			numOfAppsAllowed = numOfAppsAllowed - 1;

/*DDN: Added for MR 2078 */
			if (command == gInstanceStatus[res_id].status)
			{
				sprintf (log_buf, "MR2078: Conflict in deleting instance for port %d.", res_id);
				resp_log (mod, res_id, REPORT_NORMAL, 3439, log_buf);
			}

		   /*DDN: Added for MR2078 */
			gInstanceStatus[res_id].status = command;
			sprintf (gInstanceStatus[res_id].program_name, "%s", "");

			break;
		case ISP_CHECK_MAX_INSTANCE:
			if (appref[j].curr_instance >= appref[j].max_instance)
			{
				//added by Murali on 210525
				//Max instance reached.
				sprintf (log_buf, 
					"ERR: Could not fire application (%s). "
					"appref max instances (%d) for (%s) exceeded. Verify appref.work table.",
					program_name,
					appref[j].max_instance,
					appref[j].appl_grp_name);
				resp_log (mod, res_id, REPORT_NORMAL, 3141, log_buf);
				
				if (database_hit == 1)
				{
					sprintf (log_buf,
						"Can't start program (%s), resource <%d>, Account code (%s), Application group (%s). Maximum allowable instances (%d) are already running.",
						program_name, res_id, appref[j].acct_code, appref[j].appl_grp_name, appref[j].max_instance);
					resp_log (mod, res_id, REPORT_DETAIL, 3142, log_buf);
				}
				return (-2);
			}
			if (numOfAppsAllowed > Licensed_Resources + 4)
			{
				sprintf (log_buf,
					"Can't start program (%s), resource <%d>, Account code (%s), "
					"Application group (%s). Maximum licensed instances (%d) are already running.",
					program_name, res_id, appref[j].acct_code, appref[j].appl_grp_name, numOfAppsAllowed);
				resp_log (mod, res_id, REPORT_NORMAL, 3142, log_buf);

				//added by Murali on 210526
				//Maximum licensed instances exceeds
				return (-3);
			}
			break;
		default:
			sprintf (log_buf, "%s : Invalid command/parameter : %d", mod, command);
			resp_log (mod, res_id, REPORT_NORMAL, 3143, log_buf);
			return (-1);
	}											  /* switch */
	return (0);
}												  /* application_instance_manager */


/*-----------------------------------------------------
start_netserv() : start network server for my server.
------------------------------------------------------*/
int
start_netserv ()
{
	int ret;
	char net_serv_name[64];
	char net_serv_path[256];
	char mod[] = "start_netserv";

	if (StartNetworkServices != 1)				  /* netwok not co-resident */
		return (0);

	sprintf (net_serv_path, "%s/%s", exec_path, TEL_NET_SERV);
	sprintf (net_serv_name, "%s", TEL_NET_SERV);

/* check if network services process exists */
	ret = access (net_serv_path, R_OK | X_OK);
	if (ret != 0)
	{
		sprintf (log_buf, "Failed to start Network Services for Telecom. [%d, %s] Please fix the problem & re-start Telecom services.", errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3144, log_buf);
		StartNetworkServices = 0;
		return (ISP_FAIL);
	}
/* fire network services */
	if ((NetworkServicesPid = fork ()) == 0)
	{
		ret = execl (net_serv_path, net_serv_name, NULL);
		if (ret == -1)
		{
			sprintf (log_buf, "Failed to start Network Services for Telecom Server. [%d, %s]", errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3145, log_buf);
		}
		exit (0);
	}
	sprintf (log_buf, "Starting Network Services for Telecom Server.  pid=%d.", NetworkServicesPid);
	resp_log (mod, -1, REPORT_NORMAL, 3146, log_buf);
	return (0);
}												  /* start_netserv */


/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
static int
StartarcVXML ()
{
	int ret;
	char browser_name[64];
	char browser_path[512];
	char mod[] = "StartarcVXML";
	char respId[10];
	int yIntVxiId = -1;
	char yStrVxiId[10];

	yStrVxiId[0] = 0;
	yIntVxiId = beginPort / 48;

	sprintf (respId, "%d", gRespId);
	sprintf (yStrVxiId, "%d", -1);

	sprintf (browser_path, "%s/%s", exec_path, "arcVXML");
	sprintf (browser_name, "%s", "arcVXML");

	ret = access (browser_name, R_OK | X_OK);
	if (ret == 0)
	{
		if ((GV_arcVXML1Pid[0] = fork ()) == 0)
		{
			ret = execl (browser_path, browser_name, respId, NULL);
			if (ret == -1)
			{
				sprintf (log_buf, "Failed to start arcVXML Browser (%s). [%d, %s].",
								browser_path, errno, strerror(errno));
				resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
			}
			exit (0);
		}
		sprintf (log_buf, "arcVXML Browser started. pid=%d.", GV_arcVXML1Pid[0]);
		resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);

#if 0
		if ((GV_arcVXML1Pid[1] = fork ()) == 0)
		{
			ret = execl (browser_path, browser_name, respId, NULL);
			if (ret == -1)
			{
				sprintf (log_buf, "Failed to start arcVXML Browser (%s). errno=%d.", browser_path, errno);
				resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
			}
			exit (0);
		}
		sprintf (log_buf, "arcVXML Browser started. pid=%d.", GV_arcVXML1Pid[1]);
		resp_log (mod, -1, REPORT_NORMAL, 3511, log_buf);
#endif
		if (tot_resources > 96)
		{
			if ((GV_arcVXML1Pid[2] = fork ()) == 0)
			{
				ret = execl (browser_path, browser_name, respId, NULL);
				if (ret == -1)
				{
					sprintf (log_buf, "Failed to start arcVXML Browser (%s). [%d, %s]",
								browser_path, errno, strerror(errno));
					resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
				}
				exit (0);
			}
			sprintf (log_buf, "arcVXML Browser started. pid=%d.", GV_arcVXML1Pid[2]);
			resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);
		}
		if (tot_resources > 144)
		{
			if ((GV_arcVXML1Pid[3] = fork ()) == 0)
			{
				ret = execl (browser_path, browser_name, respId, NULL);
				if (ret == -1)
				{
					sprintf (log_buf, "Failed to start arcVXML Browser (%s). [%d, %s]",
								browser_path, errno, strerror(errno));
					resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
				}
				exit (0);
			}
			sprintf (log_buf, "arcVXML Browser started. pid=%d.", GV_arcVXML1Pid[3]);
			resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);
		}
	}

	sprintf (browser_path, "%s/%s", exec_path, "arcVXML2");
	sprintf (browser_name, "%s", "arcVXML2");

	ret = access (browser_name, R_OK | X_OK);
	if (ret == 0)
	{
		if (gIsVXIPresent)
		{
			sprintf (yStrVxiId, "%d", yIntVxiId + 0);
		}

		if ((GV_arcVXML2Pid[0] = fork ()) == 0)
		{
			if (gIsVXIPresent)
			{
				ret = execl (browser_path, browser_name, respId, yStrVxiId, NULL);
			}
			else
			{
				ret = execl (browser_path, browser_name, respId, NULL);
			}
			if (ret == -1)
			{
				sprintf (log_buf, "Failed to start arcVXML2 Browser (%s). [%d, %s]",
								browser_path, errno, strerror(errno));
				resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
			}
			exit (0);
		}

		sprintf (log_buf, "arcVXML2 Browser #1 started. pid=%d. vxid(%d) tot_resources(%d)", GV_arcVXML2Pid[0], yIntVxiId, tot_resources);
		resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);

		if (tot_resources > 48)
		{
			if ((GV_arcVXML2Pid[1] = fork ()) == 0)
			{
				if (gIsVXIPresent)
				{
					sprintf (yStrVxiId, "%d", yIntVxiId + 1);
				}

				if (gIsVXIPresent)
				{
					ret = execl (browser_path, browser_name, respId, yStrVxiId, NULL);
				}
				else
				{
					ret = execl (browser_path, browser_name, respId, NULL);
				}
				if (ret == -1)
				{
					sprintf (log_buf, "Failed to start arcVXML2 Browser (%s). [%d, %s]",
						browser_path, errno, strerror(errno));
					resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
				}
				exit (0);
			}
		}

		sprintf (log_buf, "arcVXML2 Browser #2 started. pid=%d. vxid(%d) tot_resources(%d)",
					GV_arcVXML2Pid[1], yIntVxiId, tot_resources);
		resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);

		if (tot_resources > 96)
		{
			if ((GV_arcVXML2Pid[2] = fork ()) == 0)
			{
				if (gIsVXIPresent)
				{
					sprintf (yStrVxiId, "%d", yIntVxiId + 2);
				}

				if (gIsVXIPresent)
				{
					ret = execl (browser_path, browser_name, respId, yStrVxiId, NULL);
				}
				else
				{
					ret = execl (browser_path, browser_name, respId, NULL);
				}

				if (ret == -1)
				{
					sprintf (log_buf, "Failed to start arcVXML2 Browser (%s). [%d, %s]",
							browser_path, errno, strerror(errno));
					resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
				}
				exit (0);
			}
			sprintf (log_buf, "arcVXML2 Browser #3 started. pid=%d. vxid(%d) tot_resources(%d)",
						GV_arcVXML2Pid[2], yIntVxiId, tot_resources);
			resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);
		}

		if (tot_resources > 144)
		{
			if ((GV_arcVXML2Pid[3] = fork ()) == 0)
			{
				if (gIsVXIPresent)
				{
					sprintf (yStrVxiId, "%d", yIntVxiId + 3);
				}

				if (gIsVXIPresent)
				{
					ret = execl (browser_path, browser_name, respId, yStrVxiId, NULL);
				}
				else
				{
					ret = execl (browser_path, browser_name, respId, NULL);
				}

				if (ret == -1)
				{
					sprintf (log_buf, "Failed to start arcVXML2 Browser (%s). [%d, %s]",
							browser_path, errno, strerror(errno));
					resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
				}
				exit (0);
			}
			sprintf (log_buf, "arcVXML2 Browser #4 started. pid=%d. vxid(%d) tot_resources(%d)",
					GV_arcVXML2Pid[3], yIntVxiId, tot_resources);
			resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);
		}
	}
	return (0);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static void see_if_googleSR_should_run ()
{
	static char mod[] = "see_if_googleSR_should_run";
	int rc;
	char filename[256];
	char section[] = "Settings";
	char name[32];
	char default_value[32];
	char value[32];

	StartGoogleSR = 0;

	sprintf (filename, "%s/%s", table_path, ".TEL.cfg");

	sprintf(name, "%s", "GOOGLE_SR");
	sprintf(default_value, "%s", "OFF");
	memset((char *)value, '\0', sizeof(value));
	rc = util_get_name_value (section, name, default_value, value, sizeof(value),
								filename, log_buf);
	if (rc == 0)
	{
		if (strcasecmp (value, "ON") == 0)
		{
			StartGoogleSR = 1;
			sprintf (log_buf, "Value of (%s) in (%s) is (%s).  Goggle SR processes will be started.",
					name, filename, value);
			resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
		}
		else
		{
			sprintf (log_buf, "Value of (%s) in (%s) is (%s).  Goggle SR processes will NOT be started.",
					name, filename, value);
			resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
			StartGoogleSR = 0;
	    }
	}

	return;
} // END: see_if_googleSR_should_run ()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static void see_if_googleSR_chatgpt_should_run ()
{
	static char mod[] = "see_if_googleSR_chatgpt_should_run";
	int rc;
	char filename[256];
	char section[] = "Settings";
	char name[32];
	char default_value[32];
	char value[32];

	StartGoogleSR = 0;
	StartChatGPT = 0;

	sprintf (filename, "%s/%s", table_path, ".TEL.cfg");

	sprintf(name, "%s", "GOOGLE_SR");
	sprintf(default_value, "%s", "OFF");
	memset((char *)value, '\0', sizeof(value));
	rc = util_get_name_value (section, name, default_value, value, sizeof(value),
								filename, log_buf);
	if (rc == 0)
	{
		if (strcasecmp (value, "ON") == 0)
		{
			StartGoogleSR = 1;
			sprintf (log_buf, "Value of (%s) in (%s) is (%s).  Goggle SR processes will be started.",
					name, filename, value);
			resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
		}
		else
		{
			sprintf (log_buf, "Value of (%s) in (%s) is (%s).  Goggle SR processes will NOT be started.",
					name, filename, value);
			resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
			StartGoogleSR = 0;
	    }
	}

	sprintf(section, "%s", "AI");
	sprintf(name, "%s", "ChatGPT");
	sprintf(default_value, "%s", "OFF");
	memset((char *)value, '\0', sizeof(value));
	rc = util_get_name_value (section, name, default_value, value, sizeof(value),
								filename, log_buf);
	if (rc == 0)
	{
		if (strcasecmp (value, "ON") == 0)
		{
			StartChatGPT = 1;
			sprintf (log_buf, "Value of (%s) in (%s) is (%s).  ChatGPT AI processes will be started.",
					name, filename, value);
			resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
		}
		else
		{
			sprintf (log_buf, "Value of (%s) in (%s) is (%s).  ChatGPT AI processes will NOT be started.",
					name, filename, value);
			resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
			StartChatGPT = 0;
	    }
	}
	return;
} // END: see_if_googleSR_chatgpt_should_run ()


/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int startVXML2Process (int zVxmlId)
{
	int ret;
	char browser_name[64];
	char browser_path[256];
	char mod[] = "startVXML2Process";
	char respId[10];
	int yIntVxiId = -1;
	char yStrVxiId[10];

	sprintf (respId, "%d", gRespId);
	sprintf (yStrVxiId, "%d", zVxmlId);

	sprintf (browser_path, "%s/%s", exec_path, "arcVXML2");
	sprintf (browser_name, "%s", "arcVXML2");
	ret = access (browser_name, R_OK | X_OK);
	if (ret == 0)
	{
		if ((GV_arcVXML2Pid[zVxmlId] = fork ()) == 0)
		{
			ret = execl (browser_path, browser_name, respId, yStrVxiId, NULL);
	
			if (ret == -1)
			{
				sprintf (log_buf, "Failed to start arcVXML2 Browser #%d (%s). [%d, %s]",
								zVxmlId, browser_path, errno, strerror(errno));
				resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
			}
			exit (0);
		}
	}

	sprintf (log_buf, "arcVXML2 Browser #%d started. pid=%d. vxid(%d) tot_resources(%d)",
				zVxmlId, GV_arcVXML2Pid[zVxmlId], yIntVxiId, tot_resources);
	resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);

	return (0);
} // END: startVXML2Process()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int startGoogleSRProcess (int zGoogleSRId)
{
	int ret;
	char googleSR_name[64];
	char googleSR_path[256];
	char mod[] = "startGoogleSRProcess";
	char respId[10];
	int yIntVxiId = -1;
	char ySrtGoogleSRId[10];

	sprintf (respId, "%d", gRespId);
	sprintf (ySrtGoogleSRId, "%d", zGoogleSRId);

	sprintf (googleSR_path, "%s/%s", exec_path, "arcGoogleSR.sh");
	sprintf (googleSR_name, "%s", "arcGoogleSR.sh");
	ret = access (googleSR_name, R_OK | X_OK);
	if (ret == 0)
	{
		if ((GV_arcGoogleSRPid = fork ()) == 0)
		{
			ret = execl (googleSR_path, googleSR_name, respId, ySrtGoogleSRId, NULL);
	
			if (ret == -1)
			{
				sprintf (log_buf, "Failed to start Google Speech Rec (%s). [%d, %s]",
								googleSR_path, errno, strerror(errno));
				resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
			}
			exit (0);
		}
	}

	sprintf (log_buf, "Google Speech Rec started. pid=%d.", GV_arcGoogleSRPid);
	resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);

	return (0);
} // END: startGoogleSRProcess()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int startChatGPTProcess (int zChatGPTId)
{
	int ret;
	char arcChatGPT_name[64];
	char arcChatGPT_path[256];
	char mod[] = "startChatGPTProcess";
	char respId[10];
	int yIntVxiId = -1;
	char yStrChatGPTId[10];

	sprintf (respId, "%d", gRespId);
	sprintf (yStrChatGPTId, "%d", zChatGPTId);

	sprintf (arcChatGPT_path, "%s/%s", exec_path, "arcChatGPTClientMgr.sh");
	sprintf (arcChatGPT_name, "%s", "arcChatGPTClientMgr.sh");
	ret = access (arcChatGPT_name, R_OK | X_OK);
	if (ret == 0)
	{
		if ((GV_arcChatGPTPid = fork ()) == 0)
		{
			ret = execl (arcChatGPT_path, arcChatGPT_name, respId, yStrChatGPTId, NULL);
	
			if (ret == -1)
			{
				sprintf (log_buf, "Failed to start chatGPT AI Client Manager (%s). [%d, %s]",
								arcChatGPT_path, errno, strerror(errno));
				resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
			}
			exit (0);
		}
	}

	sprintf (log_buf, "chatGPT AI Client Manager started. pid=%d.", GV_arcChatGPTPid);
	resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);

	return (0);
} // END: startChatGPTProcess()

#if 0
static int
StartUpDynMgrAfterDefunct (int zDMNumber)
{
	int ret, i;
	char DynMgr_name[64];
	char DynMgr_path[512];
	char mod[] = "StartUpDynMgr";
	char startPort[10], endPort[10], respId[10];
	char dynMgrId[5];
	char dynMgrName[128];

	sprintf (startPort, "%d", gGV_DMInfo[zDMNumber].startPort);
	sprintf (endPort, "%d", gGV_DMInfo[zDMNumber].endPort);
	sprintf (DynMgr_path, "%s/%s", exec_path, gGV_DMInfo[zDMNumber].dynMgrName);
	sprintf (DynMgr_name, "%s", gGV_DMInfo[zDMNumber].dynMgrName);
	sprintf (dynMgrId, "%d", zDMNumber);
	sprintf (respId, "%d", gRespId);

	sprintf (dynMgrName, "%s %s %s %s %s", DynMgr_name, "-s", startPort, "-e", endPort);

	if ((DynMgrPid = fork ()) == 0)
	{

		if (gRespId <= -1)
		{
			ret = execl (DynMgr_path, DynMgr_name, "-s", startPort, "-e", endPort, "-d", dynMgrId, NULL);
		}
		else
		{
			ret = execl (DynMgr_path, DynMgr_name, "-s", startPort, "-e", endPort, "-d", dynMgrId, "-r", respId, NULL);
		}

		if (ret == -1)
		{
			sprintf (log_buf, "Failed to start Dynamic Manager (%s). [%d, %s]",
					DynMgr_path, errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
		}
		exit (0);
	}

	gGV_DMInfo[zDMNumber].pid = DynMgrPid;
	gGV_DMInfo[zDMNumber].startTime = time (0);
	gGV_DMInfo[zDMNumber].stopTime = 0;
	GV_CurrentDMPid = DynMgrPid;
	sprintf (log_buf, "Call Manager (%s) started. pid=%d.", DynMgr_name, DynMgrPid);
	resp_log (mod, -1, REPORT_VERBOSE, 3511, log_buf);

	util_sleep (0, 250);
	sendStaticPortReserveReqToDM (zDMNumber);

	i = gGV_DMInfo[zDMNumber].startPort;
	GV_CurrentDMPid = DynMgrPid;

	while ((i >= gGV_DMInfo[zDMNumber].startPort) && (i <= gGV_DMInfo[zDMNumber].endPort))
	{
		if (appType[i] == 1)					  /* Reserved or static Port */
		{
			if (strcmp (resource[i].res_usage, RESERVE_PORT) == 0)
			{
				updateAppName (i, "RESERVED");
			}
			else
			{
				updateAppName (i, "N/A");
			}

#if 0
			sprintf (log_buf, "pid=%d port=%d", i, 0);
			resp_log (mod, -1, REPORT_NORMAL, 3117, log_buf);
#endif
			write_fld (tran_tabl_ptr, i, APPL_PID, 0);
		}
		else
		{
			updateAppName (i, resource[i].static_dest);
#if 0
			sprintf (log_buf, "port=%d pid=%d", i, GV_CurrentDMPid);
			resp_log (mod, -1, REPORT_NORMAL, 3117, log_buf);
#endif
			write_fld (tran_tabl_ptr, i, APPL_PID, GV_CurrentDMPid);
		}

		i++;
	}
	return (0);
}
#endif


/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
static int
StartUpDynMgr (int zDMNumber)
{
	int ret, i;
	char DynMgr_name[64];
	char DynMgr_path[256];
	char mod[] = "StartUpDynMgr";
	char startPort[10], endPort[10], respId[10];
	char dynMgrId[5];
	char dynMgrName[128];

	sprintf (startPort, "%d", gGV_DMInfo[zDMNumber].startPort);
	sprintf (endPort, "%d", gGV_DMInfo[zDMNumber].endPort);
	sprintf (DynMgr_path, "%s/%s", exec_path, gGV_DMInfo[zDMNumber].dynMgrName);
	sprintf (DynMgr_name, "%s", gGV_DMInfo[zDMNumber].dynMgrName);
	sprintf (dynMgrId, "%d", zDMNumber);
	sprintf (respId, "%d", gRespId);

	sprintf (dynMgrName, "%s %s %s %s %s", DynMgr_name, "-s", startPort, "-e", endPort);

	ret = checkDynMgr (dynMgrName);
	if (ret == 1)
		return (0);

	if ((DynMgrPid = fork ()) == 0)
	{
		if (gRespId <= -1)
		{
			ret = execl (DynMgr_path, DynMgr_name, "-s", startPort, "-e", endPort, "-d", dynMgrId, NULL);
		}
		else
		{
			ret = execl (DynMgr_path, DynMgr_name, "-s", startPort, "-e", endPort, "-d", dynMgrId, "-r", respId, NULL);
		}

		if (ret == -1)
		{
			sprintf (log_buf, "Failed to start Dynamic Manager # %d: (%s). [%d, %s]",
				zDMNumber, DynMgr_path, errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3510, log_buf);
		}
		exit (0);
	}

	gGV_DMInfo[zDMNumber].pid = DynMgrPid;
//	sprintf (log_buf, "[%d] DJB: gGV_DMInfo[%d].pid is set to %d", __LINE__, zDMNumber, gGV_DMInfo[zDMNumber].pid);
//	resp_log (mod, -1, REPORT_VERBOSE, 3117, log_buf);

	gGV_DMInfo[zDMNumber].startTime = time (0);
	gGV_DMInfo[zDMNumber].stopTime = 0;
	GV_CurrentDMPid = DynMgrPid;
	sprintf (log_buf, "Dynamic Manager (%s) started.  pid=%d.", DynMgr_name, DynMgrPid);
	resp_log (mod, -1, REPORT_DETAIL, 3511, log_buf);

	util_sleep (0, 250);
	sendStaticPortReserveReqToDM (zDMNumber);

	i = gGV_DMInfo[zDMNumber].startPort;
	GV_CurrentDMPid = DynMgrPid;

	while ((i >= gGV_DMInfo[zDMNumber].startPort) && (i <= gGV_DMInfo[zDMNumber].endPort))
	{
		if (appType[i] == 1)					  /* Reserved or static Port */
		{
			if (strcmp (resource[i].res_usage, RESERVE_PORT) == 0)
			{
				updateAppName (i, "RESERVED");
			}
			else
			{
				updateAppName (i, "N/A");
			}

#if 0
			sprintf (log_buf, "pid=%d port=%d", i, 0);
			resp_log (mod, -1, REPORT_NORMAL, 3117, log_buf);
#endif
			write_fld (tran_tabl_ptr, i, APPL_PID, 0);
		}
		else
		{
			updateAppName (i, resource[i].static_dest);
#if 0
			sprintf (log_buf, "port=%d pid=%d", i, GV_CurrentDMPid);
			resp_log (mod, -1, REPORT_NORMAL, 3117, log_buf);
#endif
			write_fld (tran_tabl_ptr, i, APPL_PID, GV_CurrentDMPid);
		}

		i++;
	}
	return (0);
}


/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
static int
StartUpResourceMgr ()
{
	int ret;
	char ResourceMgr_name[64];
	char ResourceMgr_path[512];
	char mod[] = "StartUpResourceMgr";

	sprintf (ResourceMgr_path, "%s/%s", global_exec_path, "ResourceMgr");
	sprintf (ResourceMgr_name, "%s", "ResourceMgr");

	if ((ResourceMgrPid = fork ()) == 0)
	{
		ret = execl (ResourceMgr_path, ResourceMgr_name, NULL);
		if (ret == -1)
		{
			sprintf (log_buf, "Failed to start ResourceMgr (%s). [%d, %s]", ResourceMgr_path, errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3147, log_buf);
		}
		exit (0);
	}

	sprintf (log_buf, "ResourceMgr started. pid=%d.", ResourceMgrPid);
	resp_log (mod, -1, REPORT_DETAIL, 3148, log_buf);
	return (0);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int StartUpSRMgr ()
{
	int ret;
	char srMgr_name[64];
	char srMgr_path[512];
	char mod[] = "StartUpSRMgr";

	sprintf (srMgr_path, "%s/%s", exec_path, "mrcpClient2Mgr");
	sprintf (srMgr_name, "%s", "mrcpClient2Mgr");

	if ((srMgrPid = fork ()) == 0)
	{
		ret = execl (srMgr_path, srMgr_name, NULL);
		if (ret == -1)
		{
			sprintf (log_buf, "Failed to start mrcpClient2Mgr (%s). [%d, %s]", srMgr_path, errno,  strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3147, log_buf);
		}
		exit (0);
	}

	sprintf (log_buf, "mrcpClient2Mgr started. pid=%d.", srMgrPid);
	resp_log (mod, -1, REPORT_DETAIL, 3148, log_buf);

	return (0);
} // END: StartUpSRMgr ()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int StartUpTTSMgr ()
{
	int ret;
	char ttsMgr_name[64];
	char ttsMgr_path[512];
	char mod[] = "StartUpTTSMgr";

	sprintf (ttsMgr_path, "%s/%s", exec_path, "mrcpTTSClient2Mgr");
	sprintf (ttsMgr_name, "%s", "mrcpTTSClient2Mgr");

	if ((ttsMgrPid = fork ()) == 0)
	{
		ret = execl (ttsMgr_path, ttsMgr_name, NULL);
		if (ret == -1)
		{
			sprintf (log_buf, "Failed to start mrcpTTSClient2Mgr (%s). [%d, %s]", ttsMgr_path, errno,  strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3147, log_buf);
		}
		exit (0);
	}

	sprintf (log_buf, "mrcpTTSClient2Mgr started. pid=%d.", ttsMgrPid);
	resp_log (mod, -1, REPORT_DETAIL, 3148, log_buf);

	return (0);
} // END: StartUpTTSMgr ()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int
StartUpRedirector (int notifySDM)
{
	int ret;
	char Redirector_name[64];
	char Redirector_path[512];
	char mod[] = "StartUpRedirector";

	sprintf (Redirector_path, "%s/%s", exec_path, "ArcSipRedirector");
	sprintf (Redirector_name, "%s", "ArcSipRedirector");

	if ((RedirectorPid = fork ()) == 0)
	{
		ret = execl (Redirector_path, Redirector_name, NULL);
		if (ret == -1)
		{
			sprintf (log_buf, "Failed to start ArcSipRedirector (%s). [%d, %s]", Redirector_path, errno,  strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3147, log_buf);
		}
		exit (0);
	}

	sprintf (log_buf, "ArcSipRedirector started. pid=%d.", RedirectorPid);
	resp_log (mod, -1, REPORT_DETAIL, 3148, log_buf);

	if (notifySDM)
	{
		int i = 0;
		struct Msg_AppDied lMsg_AppDied;

//for(i = 0; i < GV_NumberOfDMRunning; i++)
//for (i = dynMgrStartNum; i < GV_NumberOfDMRunning + dynMgrStartNum; i++)
		{
			memset (&lMsg_AppDied, 0, sizeof (struct Msg_AppDied));
			lMsg_AppDied.opcode = DMOP_REREGISTER;
			lMsg_AppDied.appCallNum = 0;
			lMsg_AppDied.appPid = 0;
			lMsg_AppDied.appRef = 0;
			lMsg_AppDied.appPassword = 0;
			lMsg_AppDied.returnCode = 0;
			memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
			memcpy (&gMsgToDM, &lMsg_AppDied, sizeof (struct Msg_AppDied));

			//writeToDynMgr (mod, &gMsgToDM, gGV_DMInfo[i].pid);

			writeToAllDynMgrs (mod, &gMsgToDM);
		}
	}
	return (0);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int
StartUpSipProxy (int notifySDM)
{
	int ret;
	char siproxd_name[64];
	char siproxd_path[512];
	char mod[] = "StartUpSipProxy";
	char filename[256];

	sprintf (siproxd_path, "%s/%s", exec_path, "arcsiproxd");
	sprintf (siproxd_name, "%s", "arcsiproxd");

	sprintf (filename, "%s/%s", table_path, "arcsiproxy.conf");
	if ((siproxdPid = fork ()) == 0)
	{
		ret = execl (siproxd_path, siproxd_name, "-c", filename, NULL);
		if (ret == -1)
		{
			sprintf (log_buf, "Failed to start arcsiproxd (%s). [%d, %s]", siproxd_path, errno,  strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3147, log_buf);
		}
		exit (0);
	}

	sprintf (log_buf, "arcsiproxd started. pid=%d.", siproxdPid);
	resp_log (mod, -1, REPORT_DETAIL, 3148, log_buf);

	if (notifySDM)
	{
		int i = 0;
		struct Msg_AppDied lMsg_AppDied;

		{
			memset (&lMsg_AppDied, 0, sizeof (struct Msg_AppDied));
			lMsg_AppDied.opcode = DMOP_REREGISTER;
			lMsg_AppDied.appCallNum = 0;
			lMsg_AppDied.appPid = 0;
			lMsg_AppDied.appRef = 0;
			lMsg_AppDied.appPassword = 0;
			lMsg_AppDied.returnCode = 0;
			memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
			memcpy (&gMsgToDM, &lMsg_AppDied, sizeof (struct Msg_AppDied));

			//writeToDynMgr (__LINE__, mod, &gMsgToDM, gGV_DMInfo[i].pid);

			writeToAllDynMgrs (mod, &gMsgToDM);
		}
	}
	return (0);
} // END: StartUpSipProxy()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int StartUpRTRClient ()
{
	int ret;
	char rtrClient_name[64];
	char rtrClient_path[512];
	char mod[] = "StartUpRTRClient";

	sprintf (rtrClient_path, "%s/%s", global_exec_path, "arcRTRClient");
	sprintf (rtrClient_name, "%s", "arcRTRClient");

	if ((rtrClientPid = fork ()) == 0)
	{
		ret = execl (rtrClient_path, rtrClient_name, NULL);
		if (ret == -1)
		{
			sprintf (log_buf, "Failed to start arcRTRClient (%s). [%d, %s]", rtrClient_path, errno,  strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3147, log_buf);
		}
		exit (0);
	}

	sprintf (log_buf, "arcRTRClient started. pid=%d.", rtrClientPid);
	resp_log (mod, -1, REPORT_DETAIL, 3148, log_buf);

	return (0);

} // END: StartUpRTRClient ()

/*-------------------------------------------------------------------
This routine gets the value of desired field from data record.
-------------------------------------------------------------------*/
int
find_application (requester, resource_index, token1, token2, application, group)
int requester;									  /* requester SCHEDULER/DYNAMIC_MANAGER */
int resource_index;								  /* resource index in resource table */
char *token1;									  /* DNIS */
char *token2;									  /* ANI */
char *application;
char *group;
{
	static char mod[] = "find_application";
	struct schedule record;
	int ret;

/* find out program name */
	if ((strcmp (resource[resource_index].res_state, "STATIC") == 0) && (strcmp (resource[resource_index].res_type, "IP") == 0))
	{
		if (checkDataToken (STATIC_APPL, token1, token2, &record) != ISP_SUCCESS)
		{
            sprintf (log_buf, "[%d] checkDataToken (%d, %s, %s)", __LINE__, STATIC_APPL, token1, token2);
            resp_log (mod, resource_index, REPORT_NORMAL, 3149, log_buf);
			return (ISP_FAIL);
		}
	}
	else
	{
		if (checkDataToken (TOKEN1_APPL, token1, token2, &record) != ISP_SUCCESS)
		{
            sprintf (log_buf, "[%d] checkDataToken (%d, %s, %s)", __LINE__, TOKEN1_APPL, token1, token2);
            resp_log (mod, resource_index, REPORT_NORMAL, 3149, log_buf);
			return (ISP_FAIL);
		}
	}

/* after finding application for given token successfully,
	following are the criteria to start application */
/* check server type is same as define in command line object code (argv[1]) */
	if (strcmp (record.srvtype, object_code) != 0)
	{
		if (database_hit == 1)
		{
			sprintf (log_buf, "Application %s is not started, (static token = %s) is schedule for %s Server and this server is %s", record.program, token1,
				record.srvtype, object_name);
			resp_log (mod, resource_index, REPORT_NORMAL, 3149, log_buf);
		}
		return (ISP_FAIL);
	}
/* 1. check if application schedule is for same machine */
	if ((strcmp (object_machine_name, record.machine) != 0) && (strcmp ("*", record.machine) != 0))
	{
		if (database_hit == 1)
		{
			sprintf (log_buf, "Application %s is not started, (static token = %s) is schedule for machine %s, and %s server name is %s", record.program, token1,
				record.machine, object_name, object_machine_name);
			resp_log (mod, resource_index, REPORT_NORMAL, 3150, log_buf);
		}
		return (ISP_FAIL);
	}
/* 3. check date and time rule if ANALOG | IP & STATIC */
												  /* isdn and static */
	if (((strcmp (resource[resource_index].res_type, "ANALOG") == 0 || strcmp (resource[resource_index].res_type, "IP") == 0)
		&&strcmp (resource[resource_index].res_state, "STATIC") == 0)
		|| requester == DYNAMIC_MANAGER)
	{
		if ((ret =
			check_date_time_rule (resource[resource_index].res_no, record.program, record.start_date, record.stop_date, record.start_time, record.stop_time,
			record.rule)) != ISP_SUCCESS)
		{
			if (ret != -1)						  /* not normal failure */
			{
				if (database_hit == 1)
				{
					switch (ret)
					{
						case 1:					  /* rule failed */
							sprintf (log_buf,
								"[%d] Rule criteria are not met, Rule(%d) failed for Program = %s for token %s, program not ready to start. start_date = %d, stop_date = %d, start_time = %d, stop_time = %d", __LINE__,
								record.rule, record.program, token1, atoi (record.start_date), atoi (record.stop_date), atoi (record.start_time),
								atoi (record.stop_time));
							break;
						case 2:					  /* invalid input */
							sprintf (log_buf,
								"[%d] Invalid input parameters, Rule(%d) failed for Program = %s for token %s, program not ready to start. start_date = %d, stop_date = %d, start_time = %d, stop_time = %d", __LINE__, 
								record.rule, record.program, token1, atoi (record.start_date), atoi (record.stop_date), atoi (record.start_time),
								atoi (record.stop_time));
							break;
						case 3:					  /* logic error */
							sprintf (log_buf,
								"[%d] Internal logic error, Rule(%d) failed for Program = %s for token %s, program not ready to start. start_date = %d, stop_date = %d, start_time = %d, stop_time = %d", __LINE__, 
								record.rule, record.program, token1, atoi (record.start_date), atoi (record.stop_date), atoi (record.start_time),
								atoi (record.stop_time));
							break;
						case 4:					  /* rule not implemented */
							sprintf (log_buf, "[%d] Program  Rule(%d) failed for Program = %s for token %s, program not ready to start. Rule not implemented", __LINE__, record.rule,
								record.program, token1);
							break;
						default:
							sprintf (log_buf,
								"[%d] Unknown return code from rule, return code = %d, Rule(%d) failed for Program = %s for token %s, program not ready to start. start_date = %d, stop_date = %d, start_time = %d, stop_time = %d", __LINE__,
								ret, record.rule, record.program, token1, atoi (record.start_date), atoi (record.stop_date), atoi (record.start_time),
								atoi (record.stop_time));
							break;
					}							  /* switch */
					if (database_hit == 1)
					{
						resp_log (mod, resource_index, REPORT_NORMAL, 3151, log_buf);
					}
				}								  /* if */
			}									  /* if */
			return (ISP_FAIL);
		}
		else
		{
			if (strcmp (resource[resource_index].res_type, "IP") == 0 && strcmp (resource[resource_index].res_state, "STATIC") == 0)
			{
				if (requester == DYNAMIC_MANAGER)
					sprintf (application, "%s", record.program);
				else
					sprintf (application, "%s", resource[resource_index].res_usage);
			}
			else
				sprintf (application, "%s", record.program);
			sprintf (group, "%s", record.acct_code);
			sprintf (log_buf, "[%d] Program = %s(%s) for token %s, state = ready to run.", __LINE__, application, record.program, token1);
			resp_log (mod, resource_index, REPORT_VERBOSE, 3152, log_buf);
			return (ISP_SUCCESS);
		}
	}											  /* for analog and static */
	else if (strcmp (resource[resource_index].res_type, "IP") == 0 && strcmp (resource[resource_index].res_state, "DYN_MGR") == 0)
	{
/* get dynamic manager name from resource table */
		sprintf (application, "%s", resource[resource_index].static_dest);
		sprintf (group, "%s", record.acct_code);
		sprintf (log_buf, "[%d] Program = %s for token %s, state = ready to run.", __LINE__, resource[resource_index].static_dest, token1);
		resp_log (mod, resource_index, REPORT_VERBOSE, 3153, log_buf);
		return (ISP_SUCCESS);
	}
	else if (strcmp (resource[resource_index].res_type, "IP") == 0 && strcmp (resource[resource_index].res_state, "STATIC") == 0)
	{
/* get dynamic manager name from resource table */
		sprintf (application, "%s", resource[resource_index].res_usage);
		sprintf (group, "%s", record.acct_code);
		sprintf (log_buf, "[%d] Program = %s for token %s, state = ready to run.", __LINE__, resource[resource_index].static_dest, token1);
		resp_log (mod, resource_index, REPORT_VERBOSE, 3154, log_buf);
		return (ISP_SUCCESS);
	}
	else
	{
		if (database_hit == 1)
		{
			sprintf (log_buf, "[%d] Resource type %s and state %s is not supported. Static Destination %s, can't find application.", __LINE__, resource[resource_index].res_type,
				resource[resource_index].res_state, resource[resource_index].static_dest);
			resp_log (mod, resource_index, REPORT_NORMAL, 3155, log_buf);
		}
		return (ISP_FAIL);

	}
	return (ISP_FAIL);
}												  /* find_application */


/*------------------------------------------------------------
This routine check the token information.
and this routine is called for IP applications.
--------------------------------------------------------------*/
int
checkDataToken (match_rule, token1, token2, record)
int match_rule;									  /* matching rule */
char *token1;									  /* token 1  - DNIS */
char *token2;									  /* token 2 - ANI */
struct schedule *record;						  /* schedule record */
{
	static char mod[] = "checkDataToken";
	register int i;
	char dnis[32];
	char ani[32];
	char pattern[40];
	int ret;

	sprintf (dnis, "%s", token1);
	sprintf (ani, "%s", token2);

    sprintf (log_buf, "Attempting to check tokens (%s,%s) for match_rule %d.",
                        token1, token2, match_rule);
    resp_log (mod, -1, REPORT_VERBOSE, 3522, log_buf);

	if (token1 == NULL || (int) strlen (token1) == 0)
	{
		resp_log (mod, -1, REPORT_VERBOSE, 3522, "token1 value NULL");
		return (ISP_FAIL);
	}
	if (match_rule == TOKEN_APPL || match_rule == TOKEN1_APPL)
	{
		ret = lookForDnisMatch (match_rule, 1, dnis, ani, record);

		if (ret == ISP_SUCCESS)
			return (ISP_SUCCESS);
		else
		{
			ret = lookForDnisMatch (match_rule, 2, dnis, ani, record);
			if (ret == ISP_SUCCESS)
				return (ISP_SUCCESS);
		}
	}											  /* if */
	if (match_rule == STATIC_APPL)
	{
		ret = lookForDnisMatch (match_rule, 1, dnis, ani, record);
		if (ret == ISP_SUCCESS)
			return (ISP_SUCCESS);
	}

	if (match_rule == TOKEN2_APPL)
	{
		for (i = 0; i < tot_schedules; i++)
		{
			if (strcmp (schedule[i].origination, ani) == 0)
			{
												  /* if date and time rule failed find next match */
				if (check_date_time_rule ("RES", schedule[i].program, schedule[i].start_date, schedule[i].stop_date, schedule[i].start_time, schedule[i].stop_time, schedule[i].rule) != ISP_SUCCESS)
				{
					continue;
				}
/* found token 2 */
				sprintf (record->srvtype, "%s", schedule[i].srvtype);
				sprintf (record->machine, "%s", schedule[i].machine);
				sprintf (record->destination, "%s", schedule[i].destination);
				sprintf (record->origination, "%s", schedule[i].origination);
				record->priority = schedule[i].priority;
				record->rule = schedule[i].rule;
				sprintf (record->start_date, "%s", schedule[i].start_date);
				sprintf (record->stop_date, "%s", schedule[i].stop_date);
				sprintf (record->start_time, "%s", schedule[i].start_time);
				sprintf (record->stop_time, "%s", schedule[i].stop_time);
				sprintf (record->program, "%s", schedule[i].program);
				sprintf (record->acct_code, "%s", schedule[i].acct_code);
				sprintf (record->appl_grp_name, "%s", schedule[i].appl_grp_name);
				record->max_instance = schedule[i].max_instance;
				return (ISP_SUCCESS);
			}
		}										  /* for */
	}											  /* if */

//	if (database_hit == 1)
//	{
		sprintf (log_buf, "Received (DNIS,ANI)=((%s),(%s)). No such entry in scheduling table.", token1, token2);
		resp_log (mod, -1, REPORT_VERBOSE, 3523, log_buf);
//	}
	return (ISP_FAIL);
}												  /* checkDataToken */


/*---------------------------------------------------------------------------
This routine check the date and time valid to execute the application.
------------------------------------------------------------------------------*/
int
check_date_time_rule (resource_name, program, start_date_str, stop_date_str, start_time_str, stop_time_str, rule)
char *resource_name;
char *program;
char *start_date_str;
char *stop_date_str;
char *start_time_str;
char *stop_time_str;
int rule;
{
	static char mod[] = "check_date_time_rule";
	time_t now;
	struct tm *t;								  /* time structure */
	int start_date, stop_date, start_time, stop_time, ret;

	start_date = atoi (start_date_str);
	stop_date = atoi (stop_date_str);
	start_time = atoi (start_time_str);
	stop_time = atoi (stop_time_str);

	if ((time (&now) == (time_t) - 1) || ((t = localtime (&now)) == NULL))
	{
		sprintf (log_buf, "Unable to get system time. [%d, %s]", errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3158, log_buf);
		return (ISP_FAIL);
	}
/* debug line */
#ifdef DEBUG_RULES
	sprintf (log_buf,
		"Resource Name = %s, start date = %d, stop date = %d, start time = %d, stop time = %d, year = %d, month = %d, week day = %d, month day = %d, hour = %d, minutes = %d, seconds = %d",
		resource_name, start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	resp_log (mod, -1, REPORT_VERBOSE, 3159, log_buf);
#endif
	switch (rule)
	{
		case 0:
			ret = sc_rule0 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		case 1:
			ret = sc_rule1 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		case 2:
			ret = sc_rule2 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		case 3:
			ret = sc_rule3 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		case 4:
			ret = sc_rule4 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		case 5:
			ret = sc_rule5 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		case 6:
			ret = sc_rule6 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		case 7:
			ret = sc_rule7 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		case 8:
			ret = sc_rule8 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		case 9:
			ret = sc_rule9 (start_date, stop_date, start_time, stop_time, t->tm_year, t->tm_mon, t->tm_wday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			break;
		default:
			if (database_hit == 1)
			{
				sprintf (log_buf, "Invalid rule number in scheduling table:%d.", rule);
				resp_log (mod, -1, REPORT_NORMAL, 3160, log_buf);
			}
			return (ISP_FAIL);
	}											  /* rule */
	if (ret == 0)
		return (ISP_SUCCESS);
	return (ret);
}												  /* check_date_time_rule */


/*---------------------------------------------------------------------------
Look for DNIS match in the scheduling table.
----------------------------------------------------------------------------*/
int lookForDnisMatch (int match_rule, int type, char *dnis, char *ani, struct schedule * record)
{
	int i;
	char pattern[50];
	int ret;
	static char mod[] = "lookForDnisMatch";

	for (i = 0; i < tot_schedules; i++)
	{
		if (type == 1)
		{
			sprintf (pattern, "%s", schedule[i].destination);
			if (strcmp (pattern, dnis) == 0)
				ret = 1;
			else
				ret = 0;
		}
		else
		{
			memset (pattern, 0, sizeof (pattern));
			convertStarToUnixPatternMatch (schedule[i].destination, pattern);
			ret = stringMatch (pattern, dnis);
		}
		if (ret == 1)
		{
			ret = 0;
/* token 1 found  or wild token 1 */
/* find application from token1 only */
			if ((match_rule == TOKEN1_APPL) || (match_rule == STATIC_APPL))
			{
				if (check_date_time_rule
					("RES", schedule[i].program, schedule[i].start_date, schedule[i].stop_date, schedule[i].start_time, schedule[i].stop_time,
					schedule[i].rule) != ISP_SUCCESS)
				{
/* if date and time rule failed find */
/* next match */
					continue;
				}
				sprintf (record->srvtype, "%s", schedule[i].srvtype);
				sprintf (record->machine, "%s", schedule[i].machine);
				sprintf (record->destination, "%s", schedule[i].destination);
				sprintf (record->origination, "%s", schedule[i].origination);
				record->priority = schedule[i].priority;
				record->rule = schedule[i].rule;
				sprintf (record->start_date, "%s", schedule[i].start_date);
				sprintf (record->stop_date, "%s", schedule[i].stop_date);
				sprintf (record->start_time, "%s", schedule[i].start_time);
				sprintf (record->stop_time, "%s", schedule[i].stop_time);
				sprintf (record->program, "%s", schedule[i].program);
				sprintf (record->acct_code, "%s", schedule[i].acct_code);
				sprintf (record->appl_grp_name, "%s", schedule[i].appl_grp_name);
				record->max_instance = schedule[i].max_instance;
				return (ISP_SUCCESS);
			}
/* find application from both token */
			if (match_rule == TOKEN_APPL)
			{
/* token 2 found */
				if (strcmp (schedule[i].origination, ani) == 0)
				{
					if (check_date_time_rule
						("RES", schedule[i].program, schedule[i].start_date, schedule[i].stop_date, schedule[i].start_time, schedule[i].stop_time,
						schedule[i].rule) != ISP_SUCCESS)
					{
/* if date and time rule failed find next match */
						continue;
					}
					sprintf (record->srvtype, "%s", schedule[i].srvtype);
					sprintf (record->machine, "%s", schedule[i].machine);
					sprintf (record->destination, "%s", schedule[i].destination);
					sprintf (record->origination, "%s", schedule[i].origination);
					record->priority = schedule[i].priority;
					record->rule = schedule[i].rule;
					sprintf (record->start_date, "%s", schedule[i].start_date);
					sprintf (record->stop_date, "%s", schedule[i].stop_date);
					sprintf (record->start_time, "%s", schedule[i].start_time);
					sprintf (record->stop_time, "%s", schedule[i].stop_time);
					sprintf (record->program, "%s", schedule[i].program);
					sprintf (record->acct_code, "%s", schedule[i].acct_code);
					sprintf (record->appl_grp_name, "%s", schedule[i].appl_grp_name);
					record->max_instance = schedule[i].max_instance;
					return (ISP_SUCCESS);
				}
			}
		}
	}											  /* for rule token1 and token */
	return (ISP_FAIL);
}


void
convertStarToUnixPatternMatch (char *parmStarString, char *parmResultString)
{
	int i;
	char buf[10];

	for (i = 0; i < strlen (parmStarString); i++)
	{
		if (parmStarString[i] == '*')
			sprintf (buf, "%s", "[0-9]*");
		else
			sprintf (buf, "%c", parmStarString[i]);

		strcat (parmResultString, buf);
	}
	sprintf (buf, "%s", "$");
	strcat (parmResultString, buf);

	return;
}


/*------------------------------------------------------------------------------
stringMatch(): Matches the given string based on the pattern.
Input:
		iPattern       The pattern that needs to be matched
		iString        The string containing the pattern
Output:
		None
Return Codes:
		0              String does not match pattern
		1              String matches pattern.
Notes:
1. If either the pattern or the string are empty, a mismatch is returned.
------------------------------------------------------------------------------*/
int
stringMatch (char *iPattern, char *iString)
{
	static char mod[] = "stringMatch";
	int rc;
	regex_t preg;

	if (iPattern[0] == '\0' || iString[0] == '\0')
		return (0);

	if (regcomp (&preg, iPattern, 0) != 0)
		return (0);

	if (regexec (&preg, iString, 0, 0, 0) != 0)
		rc = 0;
	else
		rc = 1;

	regfree (&preg);

	return (rc);
}												  /* END stringMatch() */

/*----------------------------------------------------------------------------
Go to the appropriate file (determined in this module) and see if the
TTS needs to be started. If so, set global var. StartTTSMgr.
----------------------------------------------------------------------------*/
static void see_if_tts_sr_should_run ()
{
	static char mod[] = "see_if_tts_sr_should_run";
	int rc;
	char filename[512];
	char redirector_info_file[256];
	char section[] = "Settings";
	char name[32];
	char default_value[32];
	char value[32];

/* If all else fails, make sure we don't start ArcSipRedirector */
	StartTTSMgr = 0;
	StartSRMgr = 1;			// BT-42
	StartVXML2 = 1;			// BT-42

	sprintf (filename, "%s/%s", table_path, ".TEL.cfg");

	sprintf(name, "%s", "TTS_MRCP");
	sprintf(default_value, "%s", "OFF");
	memset((char *)value, '\0', sizeof(value));
	rc = util_get_name_value (section, name, default_value, value, sizeof(value),
								filename, log_buf);
	if (rc == 0)
	{
		if (strcasecmp (value, "ON") == 0)
		{
			StartTTSMgr = 1;
		}
		else
		{
			StartTTSMgr = 0;
			sprintf (log_buf, "Value of (%s) in (%s) is (%s).  TTS processes will NOT be started.  "
					"It must be set to ON to enable TTS. ",	
					name, filename, value);
			resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
	    }
	}

	
	sprintf(name, "%s", "SR_MRCP");
	sprintf(default_value, "%s", "OFF");
	memset((char *)value, '\0', sizeof(value));
	rc = util_get_name_value (section, name, default_value, value, sizeof(value),
								filename, log_buf);
	if (rc == 0)
	{
		if (strcasecmp (value, "ON") == 0)
		{
			StartSRMgr = 1;
		}
		else
		{
			StartSRMgr = 0;
			sprintf (log_buf, "Value of (%s) in (%s) is (%s).  SR processes will NOT be started.  "
					"It must be set to ON to enable speech recognition. ",	
					name, filename, value);
			resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
	    }
	}

	return;
}

static void see_if_vxml_should_run ()
{
	static char mod[] = "see_if_vxml_should_run";
	int rc;
	char filename[256];
	char redirector_info_file[256];
	char section[] = "Settings";
	char name[32];
	char default_value[32];
	char value[32];

	StartVXML2 = 1;			// BT-42

	sprintf (filename, "%s/%s", table_path, ".TEL.cfg");


	sprintf(name, "%s", "VXML2");
	sprintf(default_value, "%s", "ON");
	memset((char *)value, '\0', sizeof(value));
	rc = util_get_name_value (section, name, default_value, value, sizeof(value),
								filename, log_buf);
	if (rc == 0)
	{
		if (strcasecmp (value, "OFF") == 0)
		{
			StartVXML2 = 0;
			sprintf (log_buf, "Value of (%s) in (%s) is (%s).  VXML2 processes will NOT be started.  "
					"It must be set to ON to run VoiceXML applications. ",	
					name, filename, value);
			resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
		}
		else
		{
			StartVXML2 = 1;
	    }
	}

	return;
}

/*----------------------------------------------------------------------------
Go to the appropriate file (determined in this module) and see if the
ArcSipRedirector needs to be started. If so, set global var. StartRedirector.
----------------------------------------------------------------------------*/
static void
see_if_redirector_should_run ()
{
	static char mod[] = "see_if_redirector_should_run";
	int rc;
	char filename[256];
	char redirector_info_file[256];
	char section[] = "SIP";
	char redirectorName[] = "SipRedirection";
	char siproxdName[] = "SipProxy";
	char default_value[] = "ON";
	char redirectorValue[64];
	char siproxdValue[64];

/* If all else fails, make sure we don't start ArcSipRedirector */
	StartRedirector = 0;
	StartSiproxd = 0;

/* Get the name of the config file where Resource Mgr info is stored. */
	sprintf (filename, "%s/%s", table_path, ".TEL.cfg");
/*
	rc=util_get_cfg_filename(redirector_info_file, filename, log_buf);
	if (rc != 0)
	{
		resp_log(mod, -1, REPORT_NORMAL, 3161, log_buf);
		return;
	}
*/
	rc = util_get_name_value (section, siproxdName, default_value, siproxdValue, sizeof(siproxdValue),
								filename, log_buf);
	if (rc != 0)
	{
		resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
//		return;
	}

#if 0 // added to support both proxy and redirector 

	if (strcasecmp (siproxdValue, "ON") == 0)
	{
		StartSiproxd = 1;
		sprintf (log_buf, "Value of (%s) in (%s) is set to ON.  (%s) is ignored.",	
				siproxdName, filename, redirectorName);
		resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);
		return;
	}

#endif 

	if (strcasecmp (siproxdValue, "ON") == 0)
	{
		StartSiproxd = 1;
	} else {
		StartSiproxd = 0;
    }

	rc = util_get_name_value (section, redirectorName, default_value, redirectorValue,
						sizeof(redirectorValue), filename, log_buf);
	if (rc != 0)
	{
		resp_log (mod, -1, REPORT_NORMAL, 3162, log_buf);
		return;
	}


	if (strcasecmp (redirectorValue, "ON") == 0)
		StartRedirector = 1;
	else
		StartRedirector = 0;

	return;
}


/*----------------------------------------------------------------------------
Go to the appropriate file (determined in this module) and see if the
Resource Manager needs to be started. If so, set global var. StartResourceMgr.
----------------------------------------------------------------------------*/
static void
see_if_ResourceMgr_should_run ()
{
	static char mod[] = "see_if_ResourceMgr_should_run";
	int rc;
	char filename[256];

	char res_mgr_info_file[] = "GLOBAL_CONFIG";
	char section[] = "";
	char name[] = "ResourceMgr";
	char default_value[] = "ON";
	char value[64];
	int max_buffer_size = 64;

/* If all else fails, make sure we start Resource Manager */
	StartResourceMgr = 1;

/* Get the name of the config file where Resource Mgr info is stored. */
	rc = util_get_cfg_filename (res_mgr_info_file, filename, log_buf);
	if (rc != 0)
	{
		resp_log (mod, -1, REPORT_NORMAL, 3161, log_buf);
		return;
	}

	rc = util_get_name_value (section, name, default_value, value, max_buffer_size, filename, log_buf);
	if (rc != 0)
	{
		resp_log (mod, -1, REPORT_NORMAL, 3162, log_buf);
		return;
	}

	if (strcmp (value, "ON") == 0)
		StartResourceMgr = 1;
	else
		StartResourceMgr = 0;

	return;
}


/*----------------------------------------------------------------------------
Get the full pathname of the license file.
Note: This function is a duplicate of an internal function used by the
licensing mechanism. I had to include it since I could not link with the
original.  gpw 7/26/99
----------------------------------------------------------------------------*/
int
get_license_file_name (char *license_file, char *err_msg)
{
	static int got_file_already = 0;
	static char hold_license_file[256];
	char *ispbase;
	char base_env_var[] = "ISPBASE";

	if (got_file_already)
	{
		strcpy (license_file, hold_license_file);
		strcpy (err_msg, "");
		return (0);
	}

	if ((ispbase = getenv (base_env_var)) != NULL)
	{
		got_file_already = 1;
		sprintf (hold_license_file, "%s/Global/Tables/license.dat", ispbase);
		strcpy (license_file, hold_license_file);
		strcpy (err_msg, "");
		return (0);
	}
	else
	{
		sprintf (err_msg, "Unable to get evironment variable %s.", base_env_var);
		return (-1);
	}
}


/*----------------------------------------------------------------------------
Put the name of Application in application name field of the shared memory.
-----------------------------------------------------------------------------*/
int updateAppName (int port, char *appl_name)
{
	char *ptr, *ptr1;

	ptr = tran_tabl_ptr;
	ptr += (port * SHMEM_REC_LENGTH);
/* position the pointer to the field */
	ptr += vec[APPL_NAME - 1];					  /* application start index */
	ptr1 = ptr;
	(void) memset (ptr1, ' ', MAX_APPL_NAME);
	ptr += (MAX_APPL_NAME - strlen (appl_name));
	(void) memcpy (ptr, appl_name, strlen (appl_name));
	return (0);
}


#define FIFO_DIR "FIFO_DIR"
/*-----------------------------------------------------------------------------
This routine will read the fifo directory from .Global.cfg file. if no
directory is found it will assume /tmp
-----------------------------------------------------------------------------*/
static int readGlobalCfg (char *fifo_dir, int *startRTRClient)
{
	int  counter;
	char *ispbase, *ptr;
	char cfgFile[128], line[128];
	char logBuf[256];
	char lhs[50];
	FILE *fp;
	static char mod[] = "readGlobalCfg";
	int		logCdrFile = 0;
	int		realTimeReport = 0;

	fifo_dir[0]		= '\0';
	*startRTRClient = 0;

	if ((ispbase = getenv ("ISPBASE")) == NULL)
	{
		sprintf (log_buf, "%s", "Env var ISPBASE not set,setting fifo dir to /tmp");
		resp_log (mod, -1, REPORT_NORMAL, 3167, log_buf);
		sprintf (fifo_dir, "%s", "/tmp");
		return (-1);
	}

	memset (cfgFile, 0, sizeof (cfgFile));
	sprintf (cfgFile, "%s/Global/.Global.cfg", ispbase);

	if ((fp = fopen (cfgFile, "r")) == NULL)
	{
		sprintf (log_buf, "Failed to open global config file (%s) for reading, [%d, %s] Setting fifo dir to /tmp",
				cfgFile, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3168, log_buf);
		sprintf (fifo_dir, "%s", "/tmp");
		return (-1);
	}

	counter = 0;
	while (fgets (line, sizeof (line) - 1, fp))
	{
		if ( counter == 3 )
		{
			break;
		}

		if (line[0] == '#')
		{
			continue;
		}
		if ((char *) strchr (line, '=') == NULL)
		{
			continue;							  /* line should be : lhs=rhs     */
		}

		ptr = line;

		memset (lhs, 0, sizeof (lhs));

		sprintf (lhs, "%s", (char *) strtok (ptr, "=\n"));
		if (lhs[0] == '\0')
		{
			continue;							  /* No left hand side!           */
		}

		if (strcmp (lhs, FIFO_DIR) == 0)
		{
			counter++;
			ptr = (char *) strtok (NULL, "\n");
			if (ptr != NULL)
			{
				sprintf (fifo_dir, "%s", ptr);
			}
			continue;
		}

		if (strcmp (lhs, "LOG_CDR_FILE") == 0)
		{
			counter++;
			ptr = (char *) strtok (NULL, "\n");
			logCdrFile = 0;
			if (ptr != NULL)
			{
				if ( strncmp(ptr, "YES", 3) == 0 )
				{
					logCdrFile = 1;
				}
			}
			continue;
		}
		if (strcmp (lhs, "REALTIME_REPORTS") == 0)
		{
			counter++;
			ptr = (char *) strtok (NULL, "\n");
			realTimeReport = 0;
			if (ptr != NULL)
			{
				if ( strncmp(ptr, "YES", 3) == 0 )
				{
					realTimeReport = 1;
				}
			}
			continue;
		}
	}											  /* while */
	fclose (fp);

	if ( fifo_dir[0] == '\0' )
	{
		sprintf (log_buf, "No value specified for %s in %s. Assuming /tmp", FIFO_DIR, cfgFile);
		resp_log (mod, -1, REPORT_NORMAL, 3169, log_buf);
		sprintf (fifo_dir, "%s", "/tmp");
	}

	if (access (fifo_dir, F_OK) != 0)
	{
		sprintf (log_buf, "fifo_dir (%s) doesn't exist, assuming /tmp", fifo_dir);
		resp_log (mod, -1, REPORT_NORMAL, 3170, log_buf);
		sprintf (fifo_dir, "%s", "/tmp");
	}

	sprintf (log_buf, "Fifo directory set to (%s)", fifo_dir);
	resp_log (mod, -1, REPORT_VERBOSE, 3171, log_buf);

	if ( logCdrFile && realTimeReport )
	{
		*startRTRClient = 1;
		sprintf (log_buf, "Enabling RTR Client.");
		resp_log (mod, -1, REPORT_VERBOSE, 3169, log_buf);
	}
	else
	{
		*startRTRClient = 0;
		sprintf (log_buf, "RTR Client is disabled." );
		resp_log (mod, -1, REPORT_VERBOSE, 3169, log_buf);
	}

	return (0);
}


int checkHeartBeat (time_t curr_time)
{
	static char mod[] = "checkHearBeat";
	int i, j;
	char off_appl_name[512];
	char off_res_name[30];
	char last_api_name[30];
	static time_t hbeat_time;
	static time_t appl_off_time;
	static int firstTime = 0;
	int tran_status[MAX_PROCS];					  /*  the status of application */
	int dynmgrPidFound;

	if (firstTime == 0)
	{
		time (&hbeat_time);
		appl_off_time = hbeat_time;
		firstTime = 1;
	}
/* after every hbeat_interval seconds, check application health */
												  /* Initialize the signal vector */
	if ((int) (curr_time - hbeat_time) >= hbeat_interval)
	{
		read_arry (tran_tabl_ptr, APPL_SIGNAL, tran_sig);
		read_arry (tran_tabl_ptr, APPL_STATUS, tran_status);
		for (i = beginPort; i < tran_proc_num + beginPort; i++)
		{
			if (tran_sig[i] == 0)
			{
				if ((tran_status[i] != STATUS_IDLE) && (tran_status[i] != STATUS_OFF) && (tran_status[i] != STATUS_CHNOFF))
				{
					if (appl_pid[i] != 0)
					{

sprintf(log_buf, "%s%d", "killing int_pid=", appl_pid[i]);
resp_log(mod, -1, REPORT_DETAIL, 3480, log_buf);
						(void) kill ((pid_t) appl_pid[i], SIGKILL);
						read_fld (tran_tabl_ptr, i, APPL_RESOURCE, off_res_name);
						read_fld (tran_tabl_ptr, i, APPL_NAME, off_appl_name);
						last_api_name[0] = '\0';
						read_fld (tran_tabl_ptr, i, APPL_API, last_api_name);
						sprintf (log_buf,
							"No heartbeat from application (%s), Resource = (%s), pid=%d in %d seconds. It appears to be hung. Application terminated. Last activity = %s",
							off_appl_name, off_res_name, appl_pid[i], hbeat_interval, last_api_name);
						resp_log (mod, i, REPORT_DETAIL, 3172, log_buf);
/* no activity for hang test */
					}							  /* pid != 0 */
				}								  /* status not idle and off */
				if (tran_status[i] == STATUS_IDLE)
				{
					dynmgrPidFound = 0;
					for (j = dynMgrStartNum; j < GV_NumberOfDMRunning + dynMgrStartNum; j++)
					{
						if (gGV_DMInfo[j].pid == appl_pid[i])
						{
							dynmgrPidFound = 1;
							break;
						}
					}
					if (dynmgrPidFound == 0)
					{
						if (appl_pid[i] != 0)
						{
sprintf(log_buf, "%s%d", "killing int_pid=", appl_pid[i]);
resp_log(mod, -1, REPORT_DETAIL, 3480, log_buf);
							(void) kill ((pid_t) appl_pid[i], SIGKILL);
							read_fld (tran_tabl_ptr, i, APPL_RESOURCE, off_res_name);
							read_fld (tran_tabl_ptr, i, APPL_NAME, off_appl_name);
							last_api_name[0] = '\0';
							read_fld (tran_tabl_ptr, i, APPL_API, last_api_name);
							sprintf (log_buf,
								"No heartbeat from application (%s), Resource = (%s), pid=%d in %d seconds. It appears to be hung. Application terminated even if port staus idle. Last activity = %s",
								off_appl_name, off_res_name, appl_pid[i], hbeat_interval, last_api_name);
							resp_log (mod, i, REPORT_DETAIL, 3172, log_buf);
/* no activity for hang test */
						}						  /* pid != 0 */
					}
				}
			}
		}										  /* for all applications */
/* reset sig */
		for (i = beginPort; i < tran_proc_num + beginPort; i++)
			tran_sig[i] = 0;
		hbeat_time = curr_time;
/* update the signal vector into the shared memory */
		write_arry (tran_tabl_ptr, APPL_SIGNAL, tran_sig);
	}											  /* hang_test */
	if ((curr_time - appl_off_time) >= MAX_FIRE_LAPS_SEC)
	{
		for (j = beginPort; j < tran_proc_num + beginPort; j++)
		{
			if (appCount[j] > MAX_NUM_APPL_FIRE)
			{
				write_fld (tran_tabl_ptr, j, APPL_STATUS, STATUS_OFF);
				read_fld (tran_tabl_ptr, j, APPL_NAME, off_appl_name);
				sprintf (log_buf,
					"Application (%s) (%d) started %d times in %d seconds. Spawning too rapidly. Application will not be started again until next reload.",
					off_appl_name, j, MAX_NUM_APPL_FIRE, MAX_FIRE_LAPS_SEC);
				resp_log (mod, j, REPORT_NORMAL, 3173, log_buf);
			}									  /* Application stopped */
			appCount[j] = 0;
		}
		appl_off_time = curr_time;
	}
	return (0);
}


void startStaticApp (int port)
{
	char application[128];
	char acct_name[128];
	static char mod[] = "startStaticApp";


	if ((strcmp (resource[port].res_state, "STATIC") == 0) && (strcmp (resource[port].res_type, "IP") == 0))
	{
		if (find_application (DYNAMIC_MANAGER, port, resource[port].static_dest, "", application, acct_name) == ISP_SUCCESS)
		{
//util_sleep(0, 450);
			if(strcmp(application, "arcVXML") == 0)
			{
				launchBrowserApp (port,
					application, resource[port].static_dest, "",
					resource[port].app_password, "N/A", acct_name, NULL);

			}
			else if(strcmp(application, "arcVXML2") == 0)
			{
				launchBrowserApp (port,
					application, resource[port].static_dest, "",
					resource[port].app_password, "N/A", acct_name, NULL);
			}
			else
			{
				launchApp (port, application, resource[port].static_dest, "", resource[port].app_password, "N/A", acct_name, NULL);
			}

		}
	}
	return;
}


int
sendStaticPortReserveReqToDM (int ZNumber)
{
	static char mod[] = "sendStaticPortReserveReqToDM";
	char staticResources[MAX_SLOT1];
	int slot_no;
	char bufToDM[256];
	char bufFromDM[256];
	int ret_code;
	int largestStaticPortNumber = -1;
	int lAppPassword = -1;
	int waitForStaticPortResponse = 0;
	int startNum, endNum;

	startNum = gGV_DMInfo[ZNumber].startPort;
	endNum = gGV_DMInfo[ZNumber].endPort;

	memset (staticResources, 0, sizeof (staticResources));

	for (slot_no = startNum; slot_no <= endNum; slot_no++)
	{
		if ((strcmp (resource[slot_no].res_state, "STATIC") == 0) &&
			(strcmp (resource[slot_no].res_type, "IP") == 0) && (resource[slot_no].app_password == NULL_PASSWORD))
		{
			largestStaticPortNumber = slot_no;
			staticResources[slot_no] = 1;
			appType[slot_no] = 1;				  /* Static App */
		}
		else if ((strcmp (resource[slot_no].static_dest, "ArcIPOCSMgr") == 0) &&
			(strcmp (resource[slot_no].res_type, "IP") == 0) && (resource[slot_no].app_password == NULL_PASSWORD))
		{
			largestStaticPortNumber = slot_no;
//staticResources[slot_no] = 1;
			staticResources[slot_no] = 2;
			appType[slot_no] = 2;				  /* OCserver App */
			res_status[slot_no].status = FREE;
			gGV_CDFInfo[slot_no].toFireOcsApp = 0;
			memset (gGV_CDFInfo[slot_no].cdfFileName, 0, sizeof (gGV_CDFInfo[slot_no].cdfFileName));
			memset (gGV_CDFInfo[slot_no].appToAppInfo, 0, sizeof (gGV_CDFInfo[slot_no].appToAppInfo));
		}
		else if ((strcmp (resource[slot_no].static_dest, "ArcIPFaxMgr") == 0) &&
			(strcmp (resource[slot_no].res_type, "IP") == 0) && (resource[slot_no].app_password == NULL_PASSWORD))
		{
			largestStaticPortNumber = slot_no;
//staticResources[slot_no] = 1;
			staticResources[slot_no] = 3;
			appType[slot_no] = 3;				  /* OCserver App */
			res_status[slot_no].status = FREE;
			gGV_FAXInfo[slot_no].toFireFaxApp = 0;
			memset (gGV_FAXInfo[slot_no].faxFileName, 0, sizeof (gGV_FAXInfo[slot_no].faxFileName));
			memset (gGV_FAXInfo[slot_no].appToAppInfo, 0, sizeof (gGV_FAXInfo[slot_no].appToAppInfo));
		}
		else
		{
			staticResources[slot_no] = 0;
			appType[slot_no] = 0;				  /* Regular App */
			res_status[slot_no].status = FREE;
		}
	}
#if 0
	if (largestStaticPortNumber == -1)
	{
/* There is no STATIC port */
		return (0);
	}
#endif

	memset (bufToDM, 0, sizeof (bufToDM));

/*Write Static Calls to the file */
	{
		FILE *yFpCallInfo = NULL;

		char yStrFileCallInfo[100];

		sprintf (yStrFileCallInfo, ".staticCallInfo.%d", ZNumber);

		unlink (yStrFileCallInfo);

		yFpCallInfo = fopen (yStrFileCallInfo, "w+");

		if (yFpCallInfo == NULL)
		{
			;
		}
		else
		{
			fwrite (staticResources, sizeof (staticResources), 1, yFpCallInfo);
			fclose (yFpCallInfo);
		}
	}

	if (largestStaticPortNumber != -1)
	{
		compress8To1 (bufToDM, staticResources, largestStaticPortNumber);
	}

/* Send permanent port reservation request to DM. */
	memset (&gMsg_ReservePort, 0, sizeof (struct Msg_ReservePort));
	gMsg_ReservePort.opcode = DMOP_PORT_REQUEST_PERM;
	gMsg_ReservePort.appCallNum = NULL_APP_CALL_NUM;
	gMsg_ReservePort.appPid = -1;
	gMsg_ReservePort.appRef = -1;
	gMsg_ReservePort.appPassword = -1;
	gMsg_ReservePort.maxCallNum = largestStaticPortNumber;

	if (gRespId == -1)
		gMsg_ReservePort.totalResources = tot_resources;
	else
		gMsg_ReservePort.totalResources = tot_resources * (gRespId + 1);

	memcpy (gMsg_ReservePort.portRequests, bufToDM, sizeof (gMsg_ReservePort.portRequests));

	memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
	memcpy (&gMsgToDM, &gMsg_ReservePort, sizeof (struct Msg_ReservePort));

	waitForStaticPortResponse = 1;
	ret_code = writeToDynMgr (__LINE__, mod, &gMsgToDM, GV_CurrentDMPid);
	if (ret_code != 0)
		return (-1);

	while (waitForStaticPortResponse == 1)
	{
		memset (&gMsg_ReservePort, 0, sizeof (struct Msg_ReservePort));

/* Wait for response from DM */
		ret_code = readFromDynMgr (__LINE__, 10000, &gMsg_ReservePort, sizeof (gMsg_ReservePort));
		if (ret_code < 0)
			return (0);

		switch (gMsg_ReservePort.opcode)
		{
			case RESOP_PORT_RESPONSE_PERM:
				waitForStaticPortResponse = 0;
				if (largestStaticPortNumber != -1)
				{
					processRESOP_PORT_RESPONSE_PERM (&gMsg_ReservePort, largestStaticPortNumber, bufToDM);
				}
				break;

			case RESOP_FIREAPP:
				processRESOP_FIREAPP_for_reservePort (&gMsg_ReservePort);
				break;

			case RESOP_STATIC_APP_GONE:
				processRESOP_STATIC_APP_GONE_for_reservePort (&gMsg_ReservePort);
				break;

			case RESOP_VXML_PID:
				if (StartVXML2 == 1)			// BT-42
				{
					processRESOP_VXML_PID (__LINE__, (struct MsgToRes*)&gMsg_ReservePort);
				}
				break;

			default:
				sprintf (log_buf, "Invalid response %d, " "received", gMsg_ReservePort.opcode);
				resp_log (mod, -1, REPORT_NORMAL, 3107, log_buf);
		}										  /* switch */
	}

	return (0);
}


int
processRESOP_VXML_PID (int zLine, struct MsgToRes *zpMsgToRes)
{
	static char mod[] = "processRESOP_VXML_PID";
	char program_name[60];

	if (zpMsgToRes->appPid == -1)
	{
		res_status[zpMsgToRes->appCallNum].status = FREE;
		return (0);
	}

	if (zpMsgToRes->appPid == -99)
	{
		sprintf (log_buf, "[%d, from %d] Negative appPid received.",  __LINE__, zLine);

		resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3107, log_buf);
		(void) cleanupApp (__LINE__, appl_pid[zpMsgToRes->appCallNum], zpMsgToRes->appCallNum);
		return (0);
	}
	read_fld (tran_tabl_ptr, zpMsgToRes->appCallNum, APPL_NAME, program_name);


//	if (appType[zpMsgToRes->appCallNum] != 2 && appType[zpMsgToRes->appCallNum] != 3)	/* Don't add for OCServer or FaxServer App. Its already done. */
	{
		application_instance_manager (ISP_ADD_INSTANCE, program_name, zpMsgToRes->appCallNum);
	}
#if 0
	else
	{
		resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3107, "Application Instance should already be added.");
	}
#endif

	sprintf (log_buf, "{%d, from %d] port=%d pid=%d",
			__LINE__, zLine, zpMsgToRes->appCallNum, zpMsgToRes->appPid);
	resp_log (mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3117, log_buf);

	write_fld (tran_tabl_ptr, zpMsgToRes->appCallNum, APPL_PID, zpMsgToRes->appPid);

	sprintf (log_buf, "{%d, from %d] %s", __LINE__, zLine, "setting APPL_STATUS, STATUS_BUSY");
	resp_log (mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3480, log_buf);
	write_fld (tran_tabl_ptr, zpMsgToRes->appCallNum, APPL_STATUS, STATUS_BUSY);
	appl_pid[zpMsgToRes->appCallNum] = zpMsgToRes->appPid;
	appCount[zpMsgToRes->appCallNum] += 1;

	return (0);
}

int processRESOP_START_NTOCS_APP (struct MsgToRes *zpMsgToRes)
{
	static char		mod[] = "processRESOP_START_NTOCS_APP";
	int				rc;
	char			ntCdfFile[192] = "";
	struct MsgToApp response;
	char			*ispbase;
	char			IspBase[64];
	char			*p;


	memset((struct MsgToApp *) &response, '\0', sizeof(response));

	response.opcode = zpMsgToRes->opcode;
	response.appCallNum = zpMsgToRes->appCallNum;
	response.appPid = zpMsgToRes->appPid;
	response.appRef = 0;
	response.returnCode = -1;

	if ( strlen(zpMsgToRes->data) == 0 )
	{
		sprintf(response.message, "%s", "Empty data received for RESOP_START_NTOCS_APP.  Expecting <cdf file>."
                "No application will be started.");

		sprintf (log_buf, "Empty data received for RESOP_START_NTOCS_APP.  Expecting <cdf file>."
				"No application will be started.");
		resp_log (mod, -1, REPORT_NORMAL, TEL_DATA_MISMATCH, log_buf);
		(void)writeToNTApp (__LINE__, &response);
		return(0);
	}

	sprintf (log_buf, "cdf file received=(%s)", zpMsgToRes->data);
	resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);

	if ((ispbase = getenv ("ISPBASE")) == NULL)
	{
		sprintf(log_buf, "%s", "Env. Variable ISPBASE not set; assuming /home/arc/.ISP");
		resp_log (mod, -1, REPORT_NORMAL, TEL_DATA_NOT_FOUND, log_buf);
		sprintf (IspBase, "%s", "/home/arc/.ISP");
		ispbase = IspBase;
	}
	sprintf(ntCdfFile, "%s/%s/%s", ispbase, NT_OCS_WORK_DIR, zpMsgToRes->data);

	ret_code = access (ntCdfFile, R_OK);
	if (ret_code < 0)
	{
		p = rindex(ntCdfFile, '/');
		sprintf(response.message,
				"Can't access received cdf file (%s), [%d, %s] No application will be started.",
				p, errno, strerror(errno));
		
		resp_log (mod, -1, REPORT_NORMAL, TEL_FILE_IO_ERROR, response.message);
		(void)writeToNTApp (__LINE__, &response);
		return(0);
	}
	rc = startNTOCSApp (ntCdfFile, &response);

	return(0);

} // END: processRESOP_START_NTOCS_APP()

/*---------------------------------------------------------------------------
If there is a file in work directory start OCServer Application.
---------------------------------------------------------------------------*/
static int startNTOCSApp (char *zCdfFile, struct MsgToApp *zResponse)
{
	char application[128];
	char Ocs_work_dir[512];
	static char mod[] = "startNTOCSApp";
	char y0FilePath[128];
	char yFilePath[128];
	char newAppName[128];
	char trunkName[32];
	char ocsFileName[64];
	int i, tempVar1;
	int free_port, port_type;
	char	msg[256];
	int		rc;

	memset ((char *)newAppName, '\0', sizeof(newAppName));
	memset ((char *)msg, '\0', sizeof(msg));

	memset ((char *)trunkName, '\0', sizeof(trunkName));
	if (( rc = readTrunkName (zCdfFile, trunkName, newAppName, msg)) != 0)
	{	
		sprintf(zResponse->message, "%.*s", 223, msg);
		resp_log (mod, -1, REPORT_NORMAL, TEL_DATA_MISMATCH, msg);
	}
	else
	{
		sprintf(log_buf, 
			"[%d] Read trunk (%s) from cdf file (%s).", __LINE__, trunkName, zCdfFile);
		resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);
	}

	if (strlen (trunkName) != 0)
	{
		free_port = -1;
		//getFreePort (-99, &port_type, &free_port, trunkName);
		getFreePort_onSameSpan(zResponse->appCallNum, -99, &port_type, &free_port, trunkName);
	}
	sprintf(log_buf, 
		"[%d] Retrieved free port %d.", __LINE__, free_port);
	resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);

	if (free_port < 0 )
	{
		sNTReportCallStatus (log_buf, zCdfFile, 20005);
		NT_dispose_on_failure (zCdfFile, log_buf, msg);
		sprintf(zResponse->message, "[%d] Unable to get free port.", __LINE__);
		resp_log (mod, -1, REPORT_NORMAL, TEL_DATA_MISMATCH, zResponse->message);
		(void)writeToNTApp (__LINE__, zResponse); 
		return(-1);
	}
		
	(void)write_fld (tran_tabl_ptr, free_port, APPL_STATUS, STATUS_BUSY);
	sprintf(log_buf, "[%d] SHM: set port %d to STATUS_BUSY", __LINE__, free_port);
	resp_log (mod, free_port, REPORT_VERBOSE, 3580, log_buf);

	sprintf (gGV_CDFInfo[free_port].cdfFileName, "%s", zCdfFile);
	getAppToAppInfoFromCDF (zCdfFile, gGV_CDFInfo[free_port].appToAppInfo);

	memset (&gMsg_StartOcsApp, 0, sizeof (struct Msg_StartOcsApp));
	gMsg_StartOcsApp.opcode = DMOP_STARTNTOCSAPP;
	gMsg_StartOcsApp.appCallNum = free_port;
	gMsg_StartOcsApp.appPid = 0;
	gMsg_StartOcsApp.appRef = 0;
	gMsg_StartOcsApp.appPassword = 0;
	sprintf (gMsg_StartOcsApp.dnis, "%s", "");
	sprintf (gMsg_StartOcsApp.ani, "%s", "");
	sprintf (gMsg_StartOcsApp.data, "%s", newAppName);
	gMsg_StartOcsApp.returnCode = -1;
	memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
	memcpy (&gMsgToDM, &gMsg_StartOcsApp, sizeof (struct Msg_StartOcsApp));

	for (i = dynMgrStartNum; i < GV_NumberOfDMRunning + dynMgrStartNum; i++)
	{
		if ((free_port >= gGV_DMInfo[i].startPort) && (free_port <= gGV_DMInfo[i].endPort))
		{
			GV_CurrentDMPid = gGV_DMInfo[i].pid;
			break;
		}
	}

	ret_code = writeToDynMgr (__LINE__, mod, &gMsgToDM, GV_CurrentDMPid);
	if (ret_code < 0 && errno == 11)
	{
		sprintf(zResponse->message, "%s", "Unable to write msg to dynamic manager. "
					"Failed to start Network Transfer OCS app.");
		resp_log (mod, -1, REPORT_NORMAL, TEL_DATA_MISMATCH, zResponse->message);

		(void)writeToNTApp (__LINE__, zResponse); 
		write_fld (tran_tabl_ptr, free_port, APPL_STATUS, STATUS_IDLE);
		sprintf(log_buf, "SHM: set port %d to STATUS_IDLE", free_port);
		resp_log (mod, free_port, REPORT_VERBOSE, 3580, log_buf);
		return(-1);
	}
	
	application_instance_manager (ISP_ADD_INSTANCE, newAppName, free_port);
	res_status[free_port].status = NOT_FREE;
	
	zResponse->returnCode = 0;
	sprintf(zResponse->message, "Successfully started %s.", newAppName);
	memcpy((struct MsgToApp *)&(gGV_MsgToApp[free_port]), zResponse, sizeof(struct MsgToApp));

//	(void)writeToNTApp (__LINE__, zResponse); 
	return (0);
} // END: startNTOCSApp ()

int
processRESOP_STATIC_APP_GONE (struct MsgToRes *zpMsgToRes)
{
	static char mod[] = "processRESOP_STATIC_APP_GONE";

	if (appType[zpMsgToRes->appCallNum] == 1)	  /* Static App */
	{
		if (appl_pid[zpMsgToRes->appCallNum] == 0)
		{
			appStatus[zpMsgToRes->appCallNum] = 0;
			startStaticApp (zpMsgToRes->appCallNum);
		}
		else
			appStatus[zpMsgToRes->appCallNum] = 3;
	}
	if (appType[zpMsgToRes->appCallNum] == 2)	  /* OCServer App */
	{
		appStatus[zpMsgToRes->appCallNum] = 0;
	}

	return (0);
}

int processRESOP_STATIC_APP_GONE_for_reservePort (struct Msg_ReservePort *zpMsgToRes)
{
	static char mod[] = "processRESOP_STATIC_APP_GONE_for_reservePort";

	if (appType[zpMsgToRes->appCallNum] == 1)	  /* Static App */
	{
		if (appl_pid[zpMsgToRes->appCallNum] == 0)
		{
			appStatus[zpMsgToRes->appCallNum] = 0;
			startStaticApp (zpMsgToRes->appCallNum);
		}
		else
			appStatus[zpMsgToRes->appCallNum] = 3;
	}
	if (appType[zpMsgToRes->appCallNum] == 2)	  /* OCServer App */
	{
		appStatus[zpMsgToRes->appCallNum] = 0;
	}

	return (0);
}




int
processRESOP_FIREAPP (struct MsgToRes *zpMsgToRes)
{
	static char mod[] = "processRESOP_FIREAPP";
	register int i;
	int retCode, ret_code;
	int	ret = 0;

// MR #4325 
	int		cantFireAppRetcode = -1;
// END: MR #4325
	char application[MAX_PROGRAM];
	char acct_name[MAX_APPLICATION];

	memset (application, 0, sizeof (application));
	memset (acct_name, 0, sizeof (acct_name));

	if (res_status[zpMsgToRes->appCallNum].status != FREE)
	{
		retCode = checkAppForGivenPort (zpMsgToRes->appCallNum);
		if (retCode == 1);
		else
		{
			char pid_str[10];
			read_fld (tran_tabl_ptr, zpMsgToRes->appCallNum, APPL_PID, pid_str);
			sprintf (msg.data, "Could not fire App, port %d already busy with pid %s.", zpMsgToRes->appCallNum, pid_str);
			resp_log (mod, zpMsgToRes->appCallNum, REPORT_NORMAL, 3579, msg.data);

			memset (&msg, 0, sizeof (struct Msg_CantFireApp));
			msg.opcode = DMOP_CANTFIREAPP;
			msg.appCallNum = zpMsgToRes->appCallNum;
			msg.appPid = zpMsgToRes->appPid;
			msg.appRef = zpMsgToRes->appRef;
			msg.appPassword = zpMsgToRes->appPassword;
			sprintf (msg.dnis, "%s", zpMsgToRes->dnis);
			sprintf (msg.ani, "%s", zpMsgToRes->ani);
			msg.returnCode = -1;
			memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
			memcpy (&gMsgToDM, &msg, sizeof (struct Msg_CantFireApp));

			ret_code = writeToDynMgr (__LINE__, mod, &gMsgToDM, GV_CurrentDMPid);
			return (0);
		}
	}

/* use the token 1 and token2 to find application */
	if (find_application (DYNAMIC_MANAGER, zpMsgToRes->appCallNum, zpMsgToRes->dnis, zpMsgToRes->ani, application, acct_name) == ISP_SUCCESS)
	{
		//if (application_instance_manager (ISP_CHECK_MAX_INSTANCE, application, zpMsgToRes->appCallNum) != ISP_SUCCESS)
		ret = application_instance_manager (ISP_CHECK_MAX_INSTANCE, application, zpMsgToRes->appCallNum);
		if( ret == -2 )                 //Maximum instance reached
		{
			//changed by Murali on 210521
			cantFireAppRetcode = -8;   // Max instance reached.
			sprintf (log_buf, "Application instance manager failed, "
					"Max instance reached, setting cantFireAppRetcode = -8 for %s, on port %d",
					application, zpMsgToRes->appCallNum);
			resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3108, log_buf);
		}
		else if( ret == -3 )                 //Maximum licensed instances exceeds
		{
			sprintf (log_buf, "Application instance manager failed, no more licenses, "
					"setting cantFireAppRetcode = -3 for %s, on port %d", application, zpMsgToRes->appCallNum);
			resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3108, log_buf);
// MR #4325
            cantFireAppRetcode = -3;   // no more licenses
// END: MR #4325
        }
		else if ( ret != ISP_SUCCESS )  // Some error
		{
			sprintf (log_buf, "Application instance manager failed, "
						"setting cantFireAppRetcode = -1 for %s, on port %d", application, zpMsgToRes->appCallNum);
			resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3108, log_buf);
			cantFireAppRetcode = -1;
		}
		else
		{
												  /* Need to fire VXML Browser App */
			if (strcmp (application, "arcVXML") == 0)
			{
				if (GV_arcVXML1Pid[0] != 0)		  /* VXML Browser parent process has been started */
				{
					ret_code = launchBrowserApp (zpMsgToRes->appCallNum,
						application, zpMsgToRes->dnis, zpMsgToRes->ani,
						zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, zpMsgToRes->data);
					if (ret_code == 0)
					{
						res_status[zpMsgToRes->appCallNum].status = NOT_FREE;
						return (0);
					}
				}
			}
												  /* Need to fire VXML Browser App */
			else if (strcmp (application, "arcVXML2") == 0)
			{
				sprintf (log_buf, "GV_arcVXML2Pid is %d", GV_arcVXML2Pid[0]);
				resp_log (mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3580, log_buf);
				if (GV_arcVXML2Pid[0] != 0)		  /* VXML Browser parent process has been started */
				{
					ret_code = launchBrowserApp (zpMsgToRes->appCallNum,
						application, zpMsgToRes->dnis, zpMsgToRes->ani,
						zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, zpMsgToRes->data);
					sprintf (log_buf, "ret_code after launchBrowserApp is %d", ret_code);
					resp_log (mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3580, log_buf);
					if (ret_code == 0)
					{
						res_status[zpMsgToRes->appCallNum].status = NOT_FREE;
						return (0);
					}
				}
			}
			else
			{
				ret_code = launchApp (zpMsgToRes->appCallNum, application,
					zpMsgToRes->dnis, zpMsgToRes->ani, zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, zpMsgToRes->data);
				if (ret_code == 0)
				{
					application_instance_manager (ISP_ADD_INSTANCE, application, zpMsgToRes->appCallNum);
					res_status[zpMsgToRes->appCallNum].status = NOT_FREE;
					return (0);
				}
				else
				{
					sprintf (msg.data, "%s", "Could not fire App, launchApp failed.");
					/*
					* added by Murali on 20210517 for the Nagios alert (CANTFIREAPP - ArcIPResp could not fire the app. Rejecting the call.)
					* Madhav wants to log the error with detail message
					* cantFireAppRetcode == -1 //find_application failed -  No such entry in scheduling table.
					* cantFireAppRetcode == -3 // no more licenses
					* cantFireAppRetcode == -7 //launchApp failed - Can't start application xxxx, [2, No such file or directory] - scheduling table is good but application could not find in Te dir.
					* cantFireAppRetcode == -8 // max instance reached
					*/
					
					resp_log (mod, zpMsgToRes->appCallNum, REPORT_NORMAL, 3580, msg.data);
					cantFireAppRetcode = TEL_SOURCE_NONEXISTENT;   // -7 launchApp failed - Application not found
				}
			}
		}
	}
	else
	{
		sprintf (msg.data, "%s", "Could not fire App, find_application failed.");
		// resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3580, msg.data);
		resp_log (mod, zpMsgToRes->appCallNum, REPORT_NORMAL, 3580, msg.data); // BT-69
// MR #4325 
		cantFireAppRetcode = -1;   // find_app failed
// END: MR #4325
	}

	memset (&msg, 0, sizeof (struct Msg_CantFireApp));
	msg.opcode = DMOP_CANTFIREAPP;
	msg.appCallNum = zpMsgToRes->appCallNum;
	msg.appPid = zpMsgToRes->appPid;
	msg.appRef = zpMsgToRes->appRef;
	msg.appPassword = zpMsgToRes->appPassword;
	sprintf (msg.dnis, "%s", zpMsgToRes->dnis);
	sprintf (msg.ani, "%s", zpMsgToRes->ani);
	sprintf (msg.data, "%.*s", MAX_RES_DATA - 1, application );

// MR #4325 
//	msg.returnCode = -1;
	msg.returnCode = cantFireAppRetcode;   // find_app failed
// END: MR #4325
	memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
	memcpy (&gMsgToDM, &msg, sizeof (struct Msg_CantFireApp));
	sprintf (log_buf, " Dynamic manager pid is %d; set DMOP_CANTFIREAPP reason to %d.",
				GV_CurrentDMPid, msg.returnCode);
	resp_log (mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3580, log_buf);

	ret_code = writeToDynMgr (__LINE__, mod, &gMsgToDM, GV_CurrentDMPid);

	return (0);
}


int
processRESOP_FIREAPP_for_reservePort (struct Msg_ReservePort *zpMsgToRes)
{
	static char mod[] = "processRESOP_FIREAPP_for_reservePort";
	register int i;
	int retCode, ret_code;
	int	ret = 0;

// MR #4325 
	int		cantFireAppRetcode = -1;
// END: MR #4325
	char application[MAX_PROGRAM];
	char acct_name[MAX_APPLICATION];

	memset (application, 0, sizeof (application));
	memset (acct_name, 0, sizeof (acct_name));

	if (res_status[zpMsgToRes->appCallNum].status != FREE)
	{
		retCode = checkAppForGivenPort (zpMsgToRes->appCallNum);
		if (retCode == 1);
		else
		{
			char pid_str[10];
			read_fld (tran_tabl_ptr, zpMsgToRes->appCallNum, APPL_PID, pid_str);
			sprintf (msg.data, "Could not fire App, port %d already busy with pid %s.", zpMsgToRes->appCallNum, pid_str);
			resp_log (mod, zpMsgToRes->appCallNum, REPORT_NORMAL, 3579, msg.data);

			memset (&msg, 0, sizeof (struct Msg_CantFireApp));
			msg.opcode = DMOP_CANTFIREAPP;
			msg.appCallNum = zpMsgToRes->appCallNum;
			msg.appPid = zpMsgToRes->appPid;
			msg.appRef = zpMsgToRes->appRef;
			msg.appPassword = zpMsgToRes->appPassword;
//			sprintf (msg.dnis, "%s", zpMsgToRes->dnis);
//			sprintf (msg.ani, "%s", zpMsgToRes->ani);
			msg.returnCode = -1;
			memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
			memcpy (&gMsgToDM, &msg, sizeof (struct Msg_CantFireApp));

			ret_code = writeToDynMgr (__LINE__, mod, &gMsgToDM, GV_CurrentDMPid);
			return (0);
		}
	}

#if 0 // can't do this....
/* use the token 1 and token2 to find application */
	if (find_application (DYNAMIC_MANAGER, zpMsgToRes->appCallNum, zpMsgToRes->dnis, zpMsgToRes->ani, application, acct_name) == ISP_SUCCESS)
	{
		//if (application_instance_manager (ISP_CHECK_MAX_INSTANCE, application, zpMsgToRes->appCallNum) != ISP_SUCCESS)
		ret = application_instance_manager (ISP_CHECK_MAX_INSTANCE, application, zpMsgToRes->appCallNum);
		if( ret == -2 )                 //Maximum instance reached
		{
			//changed by Murali on 210521
			cantFireAppRetcode = -8;   // Max instance reached.
			sprintf (log_buf, "Application instance manager failed, "
					"Max instance reached, setting cantFireAppRetcode = -8 for %s, on port %d",
					application, zpMsgToRes->appCallNum);
			resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3108, log_buf);
		}
		else if( ret == -3 )                 //Maximum licensed instances exceeds
		{
			sprintf (log_buf, "Application instance manager failed, no more licenses, "
					"setting cantFireAppRetcode = -3 for %s, on port %d", application, zpMsgToRes->appCallNum);
			resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3108, log_buf);
// MR #4325
            cantFireAppRetcode = -3;   // no more licenses
// END: MR #4325
        }
		else if ( ret != ISP_SUCCESS )  // Some error
		{
			sprintf (log_buf, "Application instance manager failed, "
						"setting cantFireAppRetcode = -1 for %s, on port %d", application, zpMsgToRes->appCallNum);
			resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3108, log_buf);
			cantFireAppRetcode = -1;
		}
		else
		{
												  /* Need to fire VXML Browser App */
			if (strcmp (application, "arcVXML") == 0)
			{
				if (GV_arcVXML1Pid[0] != 0)		  /* VXML Browser parent process has been started */
				{
					ret_code = launchBrowserApp (zpMsgToRes->appCallNum,
						application, zpMsgToRes->dnis, zpMsgToRes->ani,
						zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, zpMsgToRes->data);
					if (ret_code == 0)
					{
						res_status[zpMsgToRes->appCallNum].status = NOT_FREE;
						return (0);
					}
				}
			}
												  /* Need to fire VXML Browser App */
			else if (strcmp (application, "arcVXML2") == 0)
			{
				sprintf (log_buf, "GV_arcVXML2Pid is %d", GV_arcVXML2Pid[0]);
				resp_log (mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3580, log_buf);
				if (GV_arcVXML2Pid[0] != 0)		  /* VXML Browser parent process has been started */
				{
					ret_code = launchBrowserApp (zpMsgToRes->appCallNum,
						application, zpMsgToRes->dnis, zpMsgToRes->ani,
						zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, zpMsgToRes->data);
					sprintf (log_buf, "ret_code after launchBrowserApp is %d", ret_code);
					resp_log (mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3580, log_buf);
					if (ret_code == 0)
					{
						res_status[zpMsgToRes->appCallNum].status = NOT_FREE;
						return (0);
					}
				}
			}
			else
			{
				ret_code = launchApp (zpMsgToRes->appCallNum, application,
					zpMsgToRes->dnis, zpMsgToRes->ani, zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, zpMsgToRes->data);
				if (ret_code == 0)
				{
					application_instance_manager (ISP_ADD_INSTANCE, application, zpMsgToRes->appCallNum);
					res_status[zpMsgToRes->appCallNum].status = NOT_FREE;
					return (0);
				}
				else
				{
					sprintf (msg.data, "%s", "Could not fire App, launchApp failed.");
					/*
					* added by Murali on 20210517 for the Nagios alert (CANTFIREAPP - ArcIPResp could not fire the app. Rejecting the call.)
					* Madhav wants to log the error with detail message
					* cantFireAppRetcode == -1 //find_application failed -  No such entry in scheduling table.
					* cantFireAppRetcode == -3 // no more licenses
					* cantFireAppRetcode == -7 //launchApp failed - Can't start application xxxx, [2, No such file or directory] - scheduling table is good but application could not find in Te dir.
					* cantFireAppRetcode == -8 // max instance reached
					*/
					
					resp_log (mod, zpMsgToRes->appCallNum, REPORT_NORMAL, 3580, msg.data);
					cantFireAppRetcode = TEL_SOURCE_NONEXISTENT;   // -7 launchApp failed - Application not found
				}
			}
		}
	}
	else
	{
		sprintf (msg.data, "%s", "Could not fire App, find_application failed.");
		// resp_log (mod, zpMsgToRes->appCallNum, REPORT_DETAIL, 3580, msg.data);
		resp_log (mod, zpMsgToRes->appCallNum, REPORT_NORMAL, 3580, msg.data); // BT-69
// MR #4325 
		cantFireAppRetcode = -1;   // find_app failed
// END: MR #4325
	}
#endif // can't do this....

	memset (&msg, 0, sizeof (struct Msg_CantFireApp));
	msg.opcode = DMOP_CANTFIREAPP;
	msg.appCallNum = zpMsgToRes->appCallNum;
	msg.appPid = zpMsgToRes->appPid;
	msg.appRef = zpMsgToRes->appRef;
	msg.appPassword = zpMsgToRes->appPassword;
//	sprintf (msg.dnis, "%s", zpMsgToRes->dnis);
//	sprintf (msg.ani, "%s", zpMsgToRes->ani);
	sprintf (msg.data, "%.*s", MAX_RES_DATA - 1, application );

// MR #4325 
//	msg.returnCode = -1;
	msg.returnCode = cantFireAppRetcode;   // find_app failed
// END: MR #4325
	memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
	memcpy (&gMsgToDM, &msg, sizeof (struct Msg_CantFireApp));
	sprintf (log_buf, " Dynamic manager pid is %d; set DMOP_CANTFIREAPP reason to %d.",
				GV_CurrentDMPid, msg.returnCode);
	resp_log (mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3580, log_buf);

	ret_code = writeToDynMgr (__LINE__, mod, &gMsgToDM, GV_CurrentDMPid);

	return (0);
}





int
processRESOP_PORT_RESPONSE_PERM (struct Msg_ReservePort *zpMsg_ReservePort, int zLargestStaticPortNumber, char *zpBufToDM)
{
	static char mod[] = "processRESOP_PORT_RESPONSE_PERM";
	char bufFromDM[256];

	memset (bufFromDM, 0, sizeof (bufFromDM));
	memcpy (bufFromDM, zpMsg_ReservePort->portRequests, sizeof (zpMsg_ReservePort->portRequests));

	processStaticPortResponseFromDM (zpBufToDM, bufFromDM, zLargestStaticPortNumber, zpMsg_ReservePort->appPassword);

	return (0);
}

int
processRESOP_START_NTOCS_APP_DYN (struct MsgToRes *zpMsgToRes)
{
	static char mod[] = "processRESOP_START_NTOCS_APP_DYN";
	char acct_name[MAX_APPLICATION];
	int ret_code;
	char		msg[256];

	memset (acct_name, 0, sizeof (acct_name));

	sprintf(log_buf, "callnum %d, data %s, dnis %s, ani %s, password %d.",
		zpMsgToRes->appCallNum, zpMsgToRes->data, zpMsgToRes->dnis, zpMsgToRes->ani, zpMsgToRes->appPassword);
	resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);

	gGV_CDFInfo[zpMsgToRes->appCallNum].toFireOcsApp = 1;

	NT_dispose_on_success(gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName, gResultLine);

	ret_code = launchApp (zpMsgToRes->appCallNum,
			zpMsgToRes->data, zpMsgToRes->dnis, zpMsgToRes->ani,
			zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, NULL);

	if (ret_code == 0)
	{
        //ddn 20111021: moved before launchApp
        //dispose_on_success(gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName, gResultLine);
	}
	else
	{
        //Move back
        OCM_ASYNC_CDS cds;
        sprintf(cds.cdf, "%s", gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName);
        ocmMoveCDF(&cds, NT_OCS_CALLED_DIR, NT_OCS_ERR_DIR, "", log_buf);

		memset (msg, 0, sizeof (msg));
		NT_dispose_on_failure (gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName, gResultLine, msg);
	}

	memset (gResultLine, 0, sizeof (gResultLine));
	memset (gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName, 0, sizeof (gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName));
	memset (gGV_CDFInfo[zpMsgToRes->appCallNum].appToAppInfo, 0, sizeof (gGV_CDFInfo[zpMsgToRes->appCallNum].appToAppInfo));

	return(0);
} // END: processRESOP_START_NTOCS_APP_DYN()

int
processRESOP_START_OCS_APP (struct MsgToRes *zpMsgToRes)
{
	static char mod[] = "processRESOP_START_OCS_APP";
	char acct_name[MAX_APPLICATION];
	int ret_code;
	char	zeroFile[514];
	char	Ocs_work_dir[256]="";
	FILE	*fp = NULL;

	memset (acct_name, 0, sizeof (acct_name));

	sprintf (log_buf, "callnum %d, data %s, dnis %s, ani %s, password %d.",
		zpMsgToRes->appCallNum, zpMsgToRes->data, zpMsgToRes->dnis, zpMsgToRes->ani, zpMsgToRes->appPassword);
	resp_log (mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3559, log_buf);

	if(zpMsgToRes->appRef == -1)
    {
		sprintf (Ocs_work_dir, "%s/%s", isp_base, OCS_WORK_DIR);
		sprintf(zeroFile, "%s/0.%s", Ocs_work_dir, gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName);
		if((fp = fopen(zeroFile, "w")) == NULL)
		{
	        sprintf (log_buf, "[%d] ERR: OCS request rejected for port %d. "
				"Failed to re-create CDF zero file (%s).  %s must be re-submitted.", __LINE__,
				zpMsgToRes->appCallNum, zeroFile, gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName);
		}
		else
		{
	        sprintf (log_buf, "[%d] INFO: OCS request rejected for port %d. "
				"Successfully re-created CDF zero file (%s).", __LINE__,
				zpMsgToRes->appCallNum, zeroFile);
		}
		fclose(fp);

        resp_log (mod, -1, REPORT_NORMAL, 3557, log_buf);
        memset (gResultLine, 0, sizeof (gResultLine));
        memset (gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName, 0, sizeof (gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName));
        memset (gGV_CDFInfo[zpMsgToRes->appCallNum].appToAppInfo, 0, sizeof (gGV_CDFInfo[zpMsgToRes->appCallNum].appToAppInfo));
        return(-1);
    }

	gGV_CDFInfo[zpMsgToRes->appCallNum].toFireOcsApp = 1;

	if(strcmp(zpMsgToRes->data, "arcVXML") == 0)
	{
		ret_code = launchBrowserApp (zpMsgToRes->appCallNum,
			zpMsgToRes->data, zpMsgToRes->dnis, zpMsgToRes->ani,
			zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, NULL);

	}
	else if(strcmp(zpMsgToRes->data, "arcVXML2") == 0)
	{
		ret_code = launchBrowserApp (zpMsgToRes->appCallNum,
			zpMsgToRes->data, zpMsgToRes->dnis, zpMsgToRes->ani,
			zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, NULL);
	}
	else
	{
		ret_code = launchApp (zpMsgToRes->appCallNum,
			zpMsgToRes->data, zpMsgToRes->dnis, zpMsgToRes->ani,
			zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, NULL);
	}

	if (ret_code == 0)
	{
		dispose_on_success (zpMsgToRes->appCallNum, gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName, gResultLine);
	}
	else
	{
		dispose_on_failure (gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName, gResultLine);
	}

	memset (gResultLine, 0, sizeof (gResultLine));
	memset (gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName, 0, sizeof (gGV_CDFInfo[zpMsgToRes->appCallNum].cdfFileName));
	memset (gGV_CDFInfo[zpMsgToRes->appCallNum].appToAppInfo, 0, sizeof (gGV_CDFInfo[zpMsgToRes->appCallNum].appToAppInfo));

	return(0);
}

int processRESOP_START_FAX_APP(struct MsgToRes *zpMsgToRes)
{
	static char mod[]= "processRESOP_START_FAX_APP";
	char    acct_name[MAX_APPLICATION];
	int ret_code;

	memset(acct_name, 0, sizeof(acct_name));

	sprintf(log_buf, "callnum %d, data %s, dnis %s, ani %s, password %d.",
        zpMsgToRes->appCallNum, zpMsgToRes->data,
         zpMsgToRes->dnis, zpMsgToRes->ani, zpMsgToRes->appPassword);

    resp_log(mod, zpMsgToRes->appCallNum, REPORT_VERBOSE, 3572, log_buf);

	gGV_FAXInfo[zpMsgToRes->appCallNum].toFireFaxApp = 1;

	if(strcmp(zpMsgToRes->data, "arcVXML") == 0)
	{
		ret_code = launchBrowserApp (zpMsgToRes->appCallNum,
			zpMsgToRes->data, zpMsgToRes->dnis, zpMsgToRes->ani,
			zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, NULL);

	}
	else if(strcmp(zpMsgToRes->data, "arcVXML2") == 0)
	{
		ret_code = launchBrowserApp (zpMsgToRes->appCallNum,
			zpMsgToRes->data, zpMsgToRes->dnis, zpMsgToRes->ani,
			zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, NULL);
	}
	else
	{
		ret_code = launchApp (zpMsgToRes->appCallNum,
			zpMsgToRes->data, zpMsgToRes->dnis, zpMsgToRes->ani,
			zpMsgToRes->appPassword, zpMsgToRes->ringEventTime, acct_name, NULL);
	}

	if(ret_code != 0)
	{
		dispose_on_fax_failure(gGV_FAXInfo[zpMsgToRes->appCallNum].faxFileName,
			gResultLine);
	}
	else
	{
		application_instance_manager(ISP_ADD_INSTANCE,
				"FaxServer", zpMsgToRes->appCallNum);
	}

	memset(gResultLine, 0, sizeof(gResultLine));
	memset(gGV_FAXInfo[zpMsgToRes->appCallNum].faxFileName, 0,
			sizeof(gGV_FAXInfo[zpMsgToRes->appCallNum].faxFileName));

	return(0);

}/*END: int processRESOP_START_FAX_APP*/


int
compress8To1 (char *zpDestArray, char *zpSrcArray, int zArrayLength)
{
	int i, k;
	int j = -1;

	for (i = 0; i <= zArrayLength; i++)
	{
		if ((i % 8) == 0)
		{
			j++;
			*(zpDestArray + j) = 0;
		}

/* Shift *(zpDestArray + j) to left by 1 digit,
   add one 0 digit from right. */
		*(zpDestArray + j) = *(zpDestArray + j) << 1;

/* If binary bit in the zpSrcArray is 1 then make the
   newly added right most digit in *(zpDestArray + j) equals 1 */
		if (*(zpSrcArray + i) == 1)
		{
			*(zpDestArray + j) = *(zpDestArray + j) | 1;
		}
	}
	k = zArrayLength % 8;
	*(zpDestArray + j) = *(zpDestArray + j) << (7 - k);

	return (0);
}


int
processStaticPortResponseFromDM (char *zpOriginalCompressedArray, char *zpRespFromDM, int zHigestStaticPort, int zStartingAppPassword)
{
	static char mod[] = "processStaticPortResponseFromDM";
	int i, j;
	int compressedArrayLengthOfInterest;
	unsigned int mask;

	compressedArrayLengthOfInterest = zHigestStaticPort / 8;

	for (i = 0; i <= compressedArrayLengthOfInterest; i++)
	{
		mask = 128;								  /* i.e. 10000000 */
		for (j = 0; j < 8; j++)
		{
			if (*(zpRespFromDM + i) & mask)
			{
//8*i+jth port reserve request is granted by DM.
				resource[8 * i + j].app_password = zStartingAppPassword++;
/* Turn on that port until next reload. */
/*
				write_fld(tran_tabl_ptr, 8*i+j, APPL_STATUS,
						STATUS_INIT);
*/
			}
			else if (*(zpOriginalCompressedArray + i) & mask)
			{
//8*i+jth port reserve request is rejected by DM
				sprintf (log_buf, "Failed to reserve static " "port %d.", 8 * i + j);
				resp_log (mod, -1, REPORT_NORMAL, -999, log_buf);

/* Turn off that port until next reload. */
				write_fld (tran_tabl_ptr, 8 * i + j, APPL_STATUS, STATUS_CHNOFF);
			}

/* Right shift contents of mask by 1 */
			mask = mask >> 1;
		}
	}

	return (0);
}


#define OCS_WORK_DIR    "Telecom/OCS/work"
#define OCS_ERR_DIR     "Telecom/OCS/errors"
#define OCS_CALLED_DIR  "Telecom/OCS/called"
/*---------------------------------------------------------------------------
If there is a file in work directory start OCServer Application.
---------------------------------------------------------------------------*/
int checkAndStartOCSApp (int port)
{
	char application[128];
	char Ocs_work_dir[64];
	char yZeroFile[256];
	DIR *yDirPtr;
	struct dirent *yDirEntry;
	static char mod[] = "checkAndStartOCSApp";
	char y0FilePath[320];
	char yFilePath[320];
	char newAppName[128];
	char trunkName[32];
	char tryCount[20];
	char ocsFileName[256];
	char lastAttempt[32];
	int i, tempVar1;
	int free_port, port_type;
	int	j, k = 0;
	static int vxmlNotRunningRetCode = 0;
	int	cdfFileFound;

	memset (trunkName, 0, sizeof (trunkName));

	sprintf (Ocs_work_dir, "%s/%s", isp_base, OCS_WORK_DIR);

/* Open the work dir to check for headers.  */
	if ((yDirPtr = opendir (Ocs_work_dir)) == NULL)
	{
/* If the directory does not exist, don't write an error msg.
 * The directory gets created when a call is actually scheduled
 * by another application.  */
		if (errno != ENOENT)
		{
			sprintf (log_buf, "Failed to open dir (%s). [%d, %s]", Ocs_work_dir, errno, strerror (errno));
			resp_log (mod, port, REPORT_NORMAL, 3545, log_buf);
		}
		return (1);
	}

	cdfFileFound = 0;
	while (1)
	{
		if ((yDirEntry = readdir (yDirPtr)) == NULL)
		{
			closedir (yDirPtr);
			if ( cdfFileFound == 1 ) 
			{
				return (1);
			}
			else
			{
				return (NO_CDF_FILES);			// BT-128
			}
		}
/* Save the name of the file that we read from the directory. */
		sprintf (yZeroFile, "%s", yDirEntry->d_name);

		if (strcmp (yZeroFile, ".") != 0 && strcmp (yZeroFile, "..") != 0)
		{
			sprintf (log_buf, "[p%d] file found in OCS/work directory is (%s).", port, yZeroFile);
			resp_log (mod, -1, REPORT_VERBOSE, 3559, log_buf);
		}

/* See if this is a file that we recognize. */
/* It's name should start with "0." */

		if (strncmp (yZeroFile, "0.", 2) != 0)
		{
			continue;
		}
		else
		{
			cdfFileFound = 1;
			sprintf (y0FilePath, "%s/%s", Ocs_work_dir, yZeroFile);
			sprintf (yFilePath, "%s/%s", Ocs_work_dir, &yZeroFile[2]);

			ret_code = access (yFilePath, R_OK);
			if (ret_code < 0)
			{
				if ( vxmlNotRunningRetCode != -2 )	// BT-42 - so other resp ports don't log this error
				{
					sprintf (log_buf, "Can't access file %s, [%d, %s] Removing (%s).",
						yFilePath, errno, strerror(errno), y0FilePath);
					resp_log (mod, port, REPORT_NORMAL, 3546, log_buf);
				}
				if ( access(y0FilePath, R_OK) == 0 )
				{
					unlink (y0FilePath);
				}
				continue;
			}
			else
			{
				sprintf (ocsFileName, "%s", &yZeroFile[2]);

				ret_code = isAppEligible (yFilePath, ocsFileName, newAppName, trunkName, tryCount, lastAttempt);
				vxmlNotRunningRetCode = ret_code;			// BT-42

				if (ret_code == 0)
				{
					if (strlen (trunkName) != 0)
					{
						free_port = -1;
						getFreePort (-99, &port_type, &free_port, trunkName);

					}
					else
					{
						free_port = port;
					}

					if(free_port == -3)
					{
						sprintf (log_buf, "[%d] Unable to get free port (%d).  Cannot start app for cdf (%s).",
							__LINE__, free_port, yFilePath);
						resp_log (mod, port, REPORT_NORMAL, 3546, log_buf);

						sprintf (log_buf, "Couldn't find trunk Group (%s) for [%s]", trunkName, yFilePath);
						sReportCallStatus (log_buf, ocsFileName, 20005);
						dispose_on_failure (ocsFileName, log_buf);
						continue;
					}
					else
					if (free_port == -2)
					{
						sprintf (log_buf, "Couldn't find trunk Group (%s) for [%s]", trunkName, yFilePath);
						resp_log (mod, port, REPORT_NORMAL, 3546, log_buf);

						sReportCallStatus (log_buf, ocsFileName, 20005);
						dispose_on_failure (ocsFileName, log_buf);
						continue;
					}
					else 
					if (free_port >= 0)
					{
						memset (&gMsg_StartOcsApp, 0, sizeof (struct Msg_StartOcsApp));
						gMsg_StartOcsApp.opcode = DMOP_STARTOCSAPP;
						gMsg_StartOcsApp.appCallNum = free_port;
						gMsg_StartOcsApp.appPid = 0;
						gMsg_StartOcsApp.appRef = 0;
						gMsg_StartOcsApp.appPassword = 0;
						sprintf (gMsg_StartOcsApp.dnis, "%s", "");
						sprintf (gMsg_StartOcsApp.ani, "%s", "");
						sprintf (gMsg_StartOcsApp.data, "%s", newAppName);
						gMsg_StartOcsApp.returnCode = -1;
						memset (&gMsgToDM, 0, sizeof (struct MsgToDM));
						memcpy (&gMsgToDM, &gMsg_StartOcsApp, sizeof (struct Msg_StartOcsApp));

//for(i = 0; i < GV_NumberOfDMRunning; i++)
						for (i = dynMgrStartNum; i < GV_NumberOfDMRunning + dynMgrStartNum; i++)
						{
							if ((free_port >= gGV_DMInfo[i].startPort) && (free_port <= gGV_DMInfo[i].endPort))
							{
								GV_CurrentDMPid = gGV_DMInfo[i].pid;
								break;
							}
						}

						if ( strcmp(newAppName, "obCallback") == 0 ) 
						{
							GV_lastOCSPortUsed+=3;
						}
						else
						{
							GV_lastOCSPortUsed++;
						}

                        if(GV_lastOCSPortUsed >= tran_proc_num)	// BT-128   
						{
                            GV_lastOCSPortUsed = 0;
						}

						ret_code = writeToDynMgr (__LINE__, mod, &gMsgToDM, GV_CurrentDMPid);
						if (ret_code < 0 && errno == 11)
						{
							break;
						}

						sprintf (gGV_CDFInfo[free_port].cdfFileName, "%s", &yZeroFile[2]);
						getAppToAppInfoFromCDF (yFilePath, gGV_CDFInfo[free_port].appToAppInfo);
						unlink (y0FilePath);
		
						if (strcmp (newAppName, "arcVXML2") == 0 || strcmp (newAppName, "arcVXML") == 0)
						{
							//RGDEBUG Do not add instance if its VXML App.
							sprintf (log_buf, "Its VXML app, instance should be added later newAppName=%s", newAppName);
							resp_log (mod, free_port, REPORT_DETAIL, 3107, log_buf);
						}
						else
						{
							application_instance_manager (ISP_ADD_INSTANCE, newAppName, free_port);
						}
						res_status[free_port].status = NOT_FREE;
						break;
					}
					else
					{
						sprintf (log_buf, "[%d] Unable to get free port (%d).  Cannot start app for cdf (%s).",
							__LINE__, free_port, yFilePath);
						resp_log (mod, port, REPORT_NORMAL, 3546, log_buf);

						if (atoi (tryCount) == 0)
						{
							tempVar1 = 1;
							sprintf (tryCount, "%d", tempVar1);
							sprintf (log_buf, "tryCount=%s", tryCount);
							updateTryCount (1, yFilePath, log_buf);

/* Update with current tics */
							sprintf (log_buf, "lastAttempt=%ld", time (0));
							updatelastAttempt (1, yFilePath, log_buf);
						}
						else
						{
							tempVar1 = atoi (tryCount) + 1;
							sprintf (tryCount, "%d", tempVar1);
							sprintf (log_buf, "%s", tryCount);
							updateTryCount (2, yFilePath, log_buf);

/* Update with current tics */
							sprintf (log_buf, "%ld", time (0));
							updatelastAttempt (2, yFilePath, log_buf);
						}
					}
				}
				else
				{
					continue;
				}
			}
		}
	}
	sprintf (log_buf, "inside checkAndStartOCSApp after while.");
	resp_log (mod, -1, REPORT_VERBOSE, 3559, log_buf);
	closedir (yDirPtr);

	return (0);
}


/*-----------------------------------------------------------------------------
isAppEligible()	Read the header file, ignoring the commented lines. See if
the Application needs to be fired.
-----------------------------------------------------------------------------*/
int
isAppEligible (char *header_file, char *ocsFileName, char *newAppName, char *trunkName, char *tryCount, char *lastAttempt)
{
	char	*p;
	char	tmpHeaderFile[256];
	FILE *fp1;
	char current_yyyymmddhhmmss[20];
	char current_yymmddhhmmss[20];
	char failBuf[256];
	char callTime[20];
	char retryInterval[10];
	int lRetryInterval, lTryCount, lLastAttempt;
	time_t sec;
	time_t ltics;
	struct tm *yTimePtr;
	short badTimeFormat = 0;
	char zero_header_file[512] = "";
	static char mod[] = "isAppEligible";

	sprintf (log_buf, "Looking for header file (%s)", header_file);
	resp_log (mod, -1, REPORT_VERBOSE, 3547, log_buf);

	memset (failBuf, 0, sizeof (failBuf));

	if (readCallTime (header_file, ocsFileName, callTime, newAppName, trunkName, retryInterval, tryCount, lastAttempt, failBuf) < 0)
	{
		sprintf (log_buf, "%s", failBuf);
		resp_log (mod, -1, REPORT_VERBOSE, 3548, log_buf);
		return (-1);
	}

	sprintf (log_buf, "after readCallTime callTime(%s), newAppName (%s), trunkName (%s), retryInterval(%s), tryCount(%s)",
					callTime, newAppName, trunkName, retryInterval, tryCount);
	resp_log (mod, -1, REPORT_VERBOSE, 3547, log_buf);

	if ( (StartVXML2 == 0) && ( strcmp(newAppName, "arcVXML2") == 0 ) )			// BT-42 
	{
		sprintf (log_buf, "WARNING: VXML2 is turned OFF in the .TEL.cfg config file and the application specified in the CDF file to be "
							"started is arcVXML2.  Unable to start arcVXML2 application.");
		resp_log (mod, -1, REPORT_DETAIL, 3001, log_buf);

		sprintf(tmpHeaderFile, "%s", header_file);
	    if ((p = strrchr(tmpHeaderFile, '/')) == (char *)NULL)
	    {
			sprintf(zero_header_file, "0.%s", tmpHeaderFile);
	    }
		else
		{
		    *p = '\0';
		    p++;
			sprintf(zero_header_file, "%s/0.%s", tmpHeaderFile, p);
		}
		unlink(zero_header_file);
		unlink(header_file);

		return(-2);
	}

/* See if it's time to Call. */

	badTimeFormat = 0;
	switch (strlen (callTime))
	{
		case 1:									  /* 0 => call immediately        */
			if (callTime[0] == '0')
			{
				if (atoi (tryCount) > 0)
				{
					time (&sec);
					lRetryInterval = atoi (retryInterval);
					lTryCount = atoi (tryCount);
					sscanf (lastAttempt, "%d", &lLastAttempt);
					if (sec > (lLastAttempt + (lRetryInterval * lTryCount)))
						return (0);
					else
						break;
				}
				else
					return (0);
			}
			badTimeFormat = 1;
			break;
		case 6:									  /* HHMMSS               */
		case 12:								  /* YYMMDDHHMMSS                 */
		case 14:								  /* YYYYMMDDHHMMSS               */
			ltics = getTicsFromHMS (strlen (callTime), callTime);
			if (ltics != -1)
			{
				time (&sec);
				lRetryInterval = atoi (retryInterval);
				lTryCount = atoi (tryCount);
				if (sec > (ltics + (lRetryInterval * lTryCount)))
					return (0);
				else
					break;
			}
			else
				return (-1);
			break;
		default:
			badTimeFormat = 1;
	}											  /* switch */

	if (badTimeFormat)
	{
		sprintf (log_buf, "Invalid format of call time (%s) in CDF (%s). " "Valid formats: YYYYMMDDHHMMSS, YYMMDDHHMMSS, HHMMSS or '0'.", callTime, header_file);
		sprintf (gResultLine, "%s", log_buf);
		dispose_on_failure (ocsFileName, gResultLine);
		resp_log (mod, -1, REPORT_DETAIL, 3549, log_buf);
		return (-1);
	}
	return (1);
} /* END: isAppEligible() */


/*------------------------------------------------------------------------------
readCallTime():	Read the call Time information  from the file.
Return Values:
0 	Read all information in the file successfully.
-1	Failed to read information to the CDF. failureReason contains a string
	indicating the reason why the read failed.
------------------------------------------------------------------------------*/
int
readCallTime (char *header_file, char *ocsFileName, char *callTime,
char *newAppName, char *trunkName, char *retryInterval, char *tryCount, char *lastAttempt, char *failureReason)
{
	char line[256], lhs[64], rhs[256], *ptr;
	FILE *fp;
	int tries;
	int tryCountFound = 0;
	int numberOfTries;

	tryCount[0] = '\0';

	if ((fp = fopen (header_file, "r")) == NULL)
	{
		sprintf (failureReason, "Failed to open CDF file (%s) for reading, [%d, %s]",
					header_file, errno, strerror (errno));
		sReportCallStatus (failureReason, ocsFileName, 20001);
		return (-1);
	}

	while (fgets (line, sizeof (line) - 1, fp))
	{

/* Ignore lines that don't have a '=' in them */
		if (!strchr (line, '='))
		{
			continue;
		}

		memset (lhs, 0, sizeof (lhs));
		memset (rhs, 0, sizeof (rhs));

		ptr = line;

		sscanf(line, "%[^=]=%s", lhs, rhs);

/* If LHS is NULL, don't even bother looking at RHS */
		if (lhs[0] == '\0')
		{
			continue;
		}

		if (strcmp (lhs, "cdv_callTime") == 0)
		{
			sprintf (callTime, "%s", rhs);
		}
		else if (strcmp (lhs, "cdv_newApp") == 0)
		{
			sprintf (newAppName, "%s", rhs);
		}
		else if (strcmp (lhs, "cdv_trunkGroup") == 0)
		{
			if (strcmp (rhs, "(null)") == 0)
				trunkName[0] = '\0';
			else
				sprintf (trunkName, "%s", rhs);
		}
		else if (strcmp (lhs, "cdv_retryInterval") == 0)
		{
			if (strcmp (rhs, "(null)") == 0)
				retryInterval[0] = '\0';
			else
				sprintf (retryInterval, "%s", rhs);
		}
		else if (strcmp (lhs, "tryCount") == 0)
		{
			if (strcmp (rhs, "(null)") == 0)
				tryCount[0] = '\0';
			else
				sprintf (tryCount, "%s", rhs);

			tryCountFound = 1;
		}
		else if (strcmp (lhs, "cdv_tries") == 0)
		{
			if (strcmp (rhs, "(null)") == 0)
				tries = 0;
			else
				sscanf (rhs, "%d", &tries);
		}
		else if (strcmp (lhs, "lastAttempt") == 0)
		{
			if (strcmp (rhs, "(null)") == 0)
				lastAttempt[0] = '\0';
			else
				sprintf (lastAttempt, "%s", rhs);
		}
	}											  /* while */

	fclose (fp);

	if (atoi (tryCount) > tries)
	{
		sprintf (failureReason, "Number of attempts %s, exceded cdv_tries %d.", tryCount, tries);
		sReportCallStatus (failureReason, ocsFileName, 20002);
		dispose_on_failure (ocsFileName, failureReason);
		return (-1);
	}
	if (tryCountFound == 0)
	{
		tryCount[0] = '\0';
		lastAttempt[0] = '\0';
	}
	return (0);
}												  /* END: readCallTime() */

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int readTrunkName (char *ocsFileName, char *trunkName, char *app, char *failureReason)
{
	char line[256], lhs[64], rhs[256], *ptr;
	FILE *fp;

	if ((fp = fopen (ocsFileName, "r")) == NULL)
	{
		sprintf (failureReason, "Failed to open CDF file (%s) for reading, [%d, %s]",
					ocsFileName, errno, strerror (errno));
		sReportCallStatus (failureReason, ocsFileName, 20001);
		return (-1);
	}

	while (fgets (line, sizeof (line) - 1, fp))
	{

/* Ignore lines that don't have a '=' in them */
		if (!strchr (line, '='))
		{
			continue;
		}

		memset (lhs, 0, sizeof (lhs));
		memset (rhs, 0, sizeof (rhs));

		ptr = line;

		sscanf(line, "%[^=]=%s", lhs, rhs);

/* If LHS is NULL, don't even bother looking at RHS */
		if (lhs[0] == '\0')
		{
			continue;
		}

		if (strcmp (lhs, "cdv_trunkGroup") == 0)
		{
			if (strcmp (rhs, "(null)") == 0)
				trunkName[0] = '\0';
			else
				sprintf (trunkName, "%s", rhs);
		}
		else if (strcmp (lhs, "cdv_newApp") == 0)
		{
			sprintf (app, "%s", rhs);
		}

		if ( (app[0] != '\0') && trunkName[0] != '\0' )
		{
			break;
		}

	}											  /* while */

	fclose (fp);

	return (0);
}	// END: readTrunkName()

#define FAX_WORK_DIR    "Telecom/Fax/work"
#define FAX_ERR_DIR     "Telecom/Fax/error"
#define FAX_CALLED_DIR  "Telecom/Fax/called"
#define HDR_DIR         "header"

/*---------------------------------------------------------------------------
If there is a file in work directory start OCServer Application.
---------------------------------------------------------------------------*/
int checkAndStartFaxServer(int port)
{
char	application[128];
char	Fax_header_dir[128];
char	yFile[256];
char	yZeroFile[256];
DIR	*yDirPtr;
struct dirent	*yDirEntry;
static char mod[]="checkAndStartFaxServer";
char 	yFilePath[512];
char 	y0FilePath[512];
char 	newAppName[128];
char 	trunkName[32];
char 	tryCount[20];
char 	faxFileName[512];
char 	lastAttempt[32];
int		i, tempVar1;
int		free_port, port_type;
FILE	*fp;
char	str[256];
long 	sec;
char	current_yyyymmddhhmmss[30];
char	fileType[64];
char	fileToFax[128];
char	faxPhone[64];
char	date_and_time[64];
int	tries;
char 	deleteFlag[64];
int	failOnBusy;
int	retrySeconds;
struct tm	*PT_time;

memset(trunkName, 0, sizeof(trunkName));

sprintf(Fax_header_dir,	"%s/%s/%s",	isp_base, FAX_WORK_DIR, HDR_DIR);

	/* Open the work dir to check for headers.  */
if((yDirPtr = opendir(Fax_header_dir)) == NULL)
{
	/* If the directory does not exist, don't write an error msg.
	 * The directory gets created when a call is actually scheduled
	 * by another application.  */
	if(errno != ENOENT)
	{
		sprintf(log_buf, "Failed to open dir (%s).  [%d, %s]", 
				Fax_header_dir, errno, strerror(errno));
		resp_log(mod, -1, REPORT_NORMAL, 3568, log_buf);
	}
	return(1);
}

while(1)
{
	if((yDirEntry = readdir(yDirPtr)) == NULL)
	{
		closedir(yDirPtr);
		return(1);
	}
	/* Save the name of the file that we read from the directory. */
	sprintf(yZeroFile, "%s", yDirEntry->d_name);

	/* See if this is a file that we recognize. */ 
	/* It's name should start with "0." */

	if(strncmp(yZeroFile, "0.", 2) != 0)
	{
		continue;
	}
	else
	{
		sprintf(log_buf, "file found in FAX/work directory is (%s).",yZeroFile);
		resp_log(mod, -1, REPORT_VERBOSE, 3569, log_buf);

		sprintf(yFile, "%s", &yZeroFile[2]);

		sprintf(y0FilePath, "%s/%s", Fax_header_dir, yZeroFile);
		sprintf(yFilePath, "%s/%s", Fax_header_dir, &yZeroFile[2]);

		ret_code = access(yFilePath, R_OK);
		if(ret_code < 0)
		{
			sprintf(log_buf, "Can't access file %s, [%d, %s]" ,
								yFilePath, errno, strerror(errno));
			resp_log(mod, -1, REPORT_NORMAL, 3570, log_buf);
			continue;
		}
		else
		{
			sprintf(faxFileName, "%s", yFilePath);
			if((fp=fopen(faxFileName, "r")) == NULL)
			{
				sprintf(log_buf,"Failed to open (%s). [%d, %s]",
								faxFileName,errno, strerror(errno));
				resp_log(mod, -1, REPORT_NORMAL, 3571, log_buf);
				continue;
			}

			while(fgets(str, 256, fp) != NULL)
			{
				if(str[0] == '#') 
					continue;
				/* read the parameter line */
				sscanf(str, "%s %s %s %s %d %s %d %d", 
					fileType, fileToFax, faxPhone, date_and_time, 
					&tries, deleteFlag, &failOnBusy, &retrySeconds);
			}
			fclose(fp);

			/* See if it's time to send the fax yet. */
			if(time(&sec) == -1) 
				continue;
			PT_time=localtime(&sec);
			strftime(current_yyyymmddhhmmss, sizeof(current_yyyymmddhhmmss),
						"%Y%m%d%H%M%S", PT_time);
	
			ret_code = strcmp(date_and_time, current_yyyymmddhhmmss);
			if(ret_code > 0) 
				continue;
	
			free_port = port;

			memset(&gMsg_StartOcsApp,0,sizeof(struct Msg_StartOcsApp));
			gMsg_StartOcsApp.opcode = DMOP_STARTFAXAPP;
			gMsg_StartOcsApp.appCallNum = free_port;
			gMsg_StartOcsApp.appPid = 0;
			gMsg_StartOcsApp.appRef = 0;
			gMsg_StartOcsApp.appPassword = 0;
			sprintf(gMsg_StartOcsApp.dnis, "%s", "");
			sprintf(gMsg_StartOcsApp.ani, "%s", "");
			sprintf(gMsg_StartOcsApp.data, "%s", "FaxServer");
			gMsg_StartOcsApp.returnCode = -1;
			memset(&gMsgToDM, 0, sizeof(struct MsgToDM));
			memcpy(&gMsgToDM, &gMsg_StartOcsApp, 
									sizeof(struct Msg_StartOcsApp));
			
			for(i = 0; i < GV_NumberOfDMRunning; i++)
			{
				if((free_port >= gGV_DMInfo[i].startPort) && 
									(free_port <= gGV_DMInfo[i].endPort))
				{
					GV_CurrentDMPid = gGV_DMInfo[i].pid;
					break;
				}
			}
			ret_code = writeToDynMgr(__LINE__, mod, &gMsgToDM, GV_CurrentDMPid);
			sprintf(gGV_FAXInfo[free_port].faxFileName, "%s", yFile);
			unlink(y0FilePath);
			res_status[free_port].status = NOT_FREE;
			break;
		}
	}
}
closedir(yDirPtr);

return(0);
}
int
createRequestFifo (int ReleaseNo, char *mod)
{
	int rc;
	static int sFirstTime = 1;

	if (sFirstTime == 1)
	{
		if (ReleaseNo == 1)
		{
			if ((mknod (RESP_TO_VXML_FIFO, S_IFIFO | 0666, 0) < 0)
				&& (errno != EEXIST))
			{
				sprintf (log_buf, "arcVXML: mknod for (%s) failed. [%d, %s]", RESP_TO_VXML_FIFO, errno, strerror(errno));
				resp_log (mod, -1, REPORT_NORMAL, 3576, log_buf);
				return (-1);
			}

			if (gRespId == -1)
			{
				gArcVXMLRequestFifoFd = open (RESP_TO_VXML_FIFO, O_RDWR | O_NONBLOCK);
			}
			else
			{
				char yStrArcVXMLRequestFifo[256];
				sprintf (yStrArcVXMLRequestFifo, "%s.%d", RESP_TO_VXML_FIFO, gRespId);
				gArcVXMLRequestFifoFd = open (yStrArcVXMLRequestFifo, O_RDWR | O_NONBLOCK);
			}

			if (gArcVXMLRequestFifoFd < 0)
			{
				sprintf (log_buf, "arcVXML: open for (%s) failed. [%d, %s]", RESP_TO_VXML_FIFO, errno, strerror(errno));
				resp_log (mod, -1, REPORT_NORMAL, 3577, log_buf);

/* SHOULD WE PUT SOMETHING INTO THE STRUCTURE ?? */
				return (-1);
			}
		}
		if (ReleaseNo == 2)
		{
			if ((mknod (RESP_TO_VXML2_FIFO, S_IFIFO | 0666, 0) < 0)
				&& (errno != EEXIST))
			{
				sprintf (log_buf, "arcVXML2: mknod for (%s) failed. [%d, %s]", RESP_TO_VXML2_FIFO, errno, strerror(errno));
				resp_log (mod, -1, REPORT_NORMAL, 3576, log_buf);
				return (-1);
			}

			if (gRespId == -1)
			{
				gArcVXML2RequestFifoFd[0] = open (RESP_TO_VXML2_FIFO, O_RDWR | O_NONBLOCK);

				if (gArcVXML2RequestFifoFd[0] < 0)
				{
					sprintf (log_buf, "arcVXML2: open for (%s) failed. [%d, %s]", RESP_TO_VXML2_FIFO, errno, strerror(errno));
					resp_log (mod, -1, REPORT_NORMAL, 3577, log_buf);

					return (-1);
				}
			}
			else
			{
				char yStrArcVXMLRequestFifo[256];
				sprintf (yStrArcVXMLRequestFifo, "%s.%d", RESP_TO_VXML2_FIFO, gRespId);
				gArcVXML2RequestFifoFd[0] = open (yStrArcVXMLRequestFifo, O_RDWR | O_NONBLOCK);
			}
		}

		if (ReleaseNo == 3)
		{
			int yIntVxmlFifoCounter;
			char yStrActualVxml2Fifo[256];

			for (yIntVxmlFifoCounter = (beginPort / 48); yIntVxmlFifoCounter <= (lastPort / 48); yIntVxmlFifoCounter++)
			{
				sprintf (yStrActualVxml2Fifo, "%s.%d", RESP_TO_VXML2_FIFO, yIntVxmlFifoCounter);
				if ((mknod (yStrActualVxml2Fifo, S_IFIFO | 0666, 0) < 0)
					&& (errno != EEXIST))
				{
					sprintf (log_buf, "arcVXML2: mknod for (%s) failed. [%d, %s]", yStrActualVxml2Fifo, errno, strerror(errno));
					resp_log (mod, -1, REPORT_NORMAL, 3576, log_buf);
					return (-1);
				}
			}

#if 0
			if (gRespId == -1)
#endif
			{
				for (yIntVxmlFifoCounter = (beginPort / 48); yIntVxmlFifoCounter <= (lastPort / 48); yIntVxmlFifoCounter++)
				{
					sprintf (yStrActualVxml2Fifo, "%s.%d", RESP_TO_VXML2_FIFO, yIntVxmlFifoCounter);
					gArcVXML2RequestFifoFd[yIntVxmlFifoCounter] = open (yStrActualVxml2Fifo, O_RDWR | O_NONBLOCK);

					if (gArcVXML2RequestFifoFd[yIntVxmlFifoCounter] < 0)
					{
						sprintf (log_buf, "arcVXML2: open for (%s) failed. [%d, %s]", yStrActualVxml2Fifo, errno, strerror(errno));
						resp_log (mod, -1, REPORT_NORMAL, 3577, log_buf);

						return (-1);
					}
				}
			}
#if 0
			else
			{
				char yStrArcVXMLRequestFifo[256];
				sprintf (yStrArcVXMLRequestFifo, "%s.%d", RESP_TO_VXML2_FIFO, gRespId);
				gArcVXML2RequestFifoFd = open (yStrArcVXMLRequestFifo, O_RDWR | O_NONBLOCK);
			}
#endif

		}

		sFirstTime = 0;
	}

	return (0);
}


int getAppToAppInfoFromCDF (char *zpCdfFile, char *appToAppInfo)
{
	char mod[] = { "getAppToAppInfoFromCDF" };
	FILE *fp;
	char line[512];
	char lhs[256];
	char rhs[256];
	char *ptr;
	char *ptr2;

	*appToAppInfo = '\0';

	if ((fp = fopen (zpCdfFile, "r")) == NULL)
	{
		sprintf (log_buf, "Failed to open cdf file (%s) for reading. AppToApp"
			"Info from this file will not be passed to the Application. [%d, %s]", zpCdfFile, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, TEL_FILE_IO_ERROR, log_buf);
		return (-1);
	}

	while (fgets (line, sizeof (line) - 1, fp))
	{
/* Ignore lines that don't have a '=' in them */
		if (!strchr (line, '='))
			continue;

        ptr2 = line + strlen(line) - 1;
        while ( isspace(*ptr2) )
        {
            *ptr2 = '\0';
            ptr2--;
        }

		memset (lhs, 0, sizeof (lhs));
		memset (rhs, 0, sizeof (rhs));
		ptr = line;

/* Even if strtok returns NULL, we don't care.
 * All it will do is give us a NULL lhs.  */

		sscanf(line, "%[^=]=%s", lhs, rhs);//DDN: 05262010 MR 3005: BUG. RHS will never have anything after spaces.

/* If LHS is NULL, don't even bother looking at RHS */
		if (lhs[0] == '\0')
		{
			continue;
		}

		if (strcmp (lhs, "cdv_appToAppInfo") == 0 && rhs[0])
		{

			/*DDN: Addedd to fix MR 3005*/
			char *realRhs = strchr(line, '=');
			realRhs+=1;

			sprintf (appToAppInfo, "%s", realRhs);

			//sprintf (appToAppInfo, "%s", rhs);

			break;
		}
	}											  /* while */

	fclose (fp);

	return (0);
}


/*------------------------------------------------------------------------------
sReportCallStatus():
------------------------------------------------------------------------------*/
static int
sReportCallStatus (char *zCallStatusMsg, char *CDFFileName, int errCode)
{
	static char yMod[] = "sReportCallStatus";
	OCM_CALL_STATUS yCallStatus;
	OCM_ASYNC_CDS yCDS;
	char yErrorMsg[256];

/*  Setup the CDS structure */
	memset (&yCDS, 0, sizeof (OCM_ASYNC_CDS));

	sprintf (yCDS.cdf, "%s", CDFFileName);

/*  Load the cdf info based on the cdf name */
	if (ocmLoadCallRequest (&yCDS, OCS_WORK_DIR, yErrorMsg) < 0)
	{
		return (-1);
	}

/*  Copy the appropriate info into the call status struct */
	memset (&yCallStatus, 0, sizeof (OCM_CALL_STATUS));

	yCallStatus.statusCode = errCode;
	sprintf (yCallStatus.callId, "%s", yCDS.callId);
	sprintf (yCallStatus.statusUrl, "%s", yCDS.statusUrl);
	sprintf (yCallStatus.statusMsg, "%s", zCallStatusMsg);

/* Report the call status */
	if (ocmReportStatus (&yCallStatus, yErrorMsg) < 0)
	{
		return (-1);
	}

	return (0);

}												  // END: static int sReportCallStatus(char *zCallStatusMsg)

/*------------------------------------------------------------------------------
sNTReportCallStatus():
------------------------------------------------------------------------------*/
static int sNTReportCallStatus (char *zCallStatusMsg, char *CDFFileName, int errCode)
{
	static char yMod[] = "sNTReportCallStatus";
	OCM_CALL_STATUS yCallStatus;
	OCM_ASYNC_CDS yCDS;
	char yErrorMsg[256];

/*  Setup the CDS structure */
	memset (&yCDS, 0, sizeof (OCM_ASYNC_CDS));

	sprintf (yCDS.cdf, "%s", CDFFileName);

/*  Load the cdf info based on the cdf name */
	if (ocmLoadCallRequest (&yCDS, NT_OCS_WORK_DIR, yErrorMsg) < 0)
	{
		return (-1);
	}

/*  Copy the appropriate info into the call status struct */
	memset (&yCallStatus, 0, sizeof (OCM_CALL_STATUS));

	yCallStatus.statusCode = errCode;
	sprintf (yCallStatus.callId, "%s", yCDS.callId);
	sprintf (yCallStatus.statusUrl, "%s", yCDS.statusUrl);
	sprintf (yCallStatus.statusMsg, "%s", zCallStatusMsg);

/* Report the call status */
	if (ocmReportStatus (&yCallStatus, yErrorMsg) < 0)
	{
		return (-1);
	}

	return (0);

} // END: sNTReportCallStatus()

/*-----------------------------------------------------------------------------
dispose_on_fax_failure:	Append the Result_line to the header and move it to
			the header error directory.
			Move the file we tried to fax (containing a text file
			or an unloaded phrase file) to the fax error 
			directory regardless of the value of the delete flag 
			in the header.
-----------------------------------------------------------------------------*/
int dispose_on_fax_failure(header_file, Result_line)
char *header_file;
char *Result_line;
{	
static char mod[]="dispose_on_fax_failure";
char 	faxHdrFile[256];
char 	faxFile[256];
char 	faxErrHdrFile[256];
char 	faxErrHdrDir[256];
char 	faxErrFileDir[256];
char 	pathname[258];
char 	command[256];
int 	err_dir_OK = 1;
FILE	*ptr;

sprintf(faxFile, "%s/%s/%s/%s", isp_base, FAX_WORK_DIR, "faxfile", header_file);
sprintf(faxHdrFile, "%s/%s/%s/%s", isp_base,FAX_WORK_DIR,HDR_DIR,header_file);
	
ptr= fopen(faxHdrFile, "a");
if(ptr == NULL) 
{
	sprintf(log_buf,
	"Failed to open (%s) while attempting to append result: (%s) [%d, %s]",
	faxHdrFile, Result_line, errno, strerror(errno));
	resp_log(mod, -1, REPORT_NORMAL, 3573, log_buf);
	return(-1);
}	

fprintf(ptr, "# %s\n", Result_line);
fclose(ptr);

sprintf(faxErrHdrDir, "%s/%s/%s", isp_base,FAX_ERR_DIR,HDR_DIR);
/* make the header directory if necessary */
if(access(faxErrHdrDir, F_OK) != 0)
{
	if(mkdir(faxErrHdrDir, 0755)== -1)
	{
		sprintf(log_buf,"Failed to make directory (%s). [%d, %s]",
				faxErrHdrDir, errno, strerror(errno));
		resp_log(mod, -1, REPORT_NORMAL, 3574, log_buf);
		/* indicate a problem, but don't return */ 
	}
}
/* move the  header file to the header error directory */
sprintf(pathname, "%s/%s", faxErrHdrDir, header_file);
if(rename(faxHdrFile, pathname) != 0)
{
	sprintf(log_buf,"Failed to move (%s) to (%s). [%d, %s]",
		 faxHdrFile, pathname,errno, strerror(errno));
	resp_log(mod, -1, REPORT_NORMAL, 3575, log_buf);
	return(-1);
}

sprintf(faxErrFileDir, "%s/%s/%s", isp_base,FAX_ERR_DIR,"faxfile");
if (access(faxErrFileDir, F_OK) != 0)
{
	if(mkdir(faxErrFileDir, 0755)== -1)
	{
		sprintf(log_buf,"Failed to make directory (%s). [%d, %s]",
				faxErrFileDir, errno, strerror(errno));
		resp_log(mod, -1, REPORT_NORMAL, 3676, log_buf);
		return(-1);
	}
}

/* move the fax file to the fax error directory */
sprintf(pathname, "%s/%s", faxErrFileDir, header_file);
if(rename(faxFile, pathname) != 0)
{
	sprintf(log_buf,"Failed to move (%s) to (%s). [%d, %s]", 
			faxFile, pathname,errno, strerror(errno));
	resp_log(mod, -1, REPORT_NORMAL, 3577, log_buf);
	return(-1);
}
return(0);
}

/*-----------------------------------------------------------------------------
NT_dispose_on_failure:	Network_Transfer
    Append the Result_line to the header and move it to
	the header error directory. Move the file we tried to fax (containing
	a text file or an unloaded phrase file) to the fax error
	directory regardless of the value of the delete flag in the header.
-----------------------------------------------------------------------------*/
static int NT_dispose_on_failure (char *header_file, char *Result_line, char *zFailMsg)
{
	char mod[] = { "dispose_on_failure" };
	char ocs_pathname[512];
	char pathname[512];
	char current_time_date[64];
	time_t sec;
	struct tm *yTimePtr;
	FILE *ptr;
	char	*p;
	char	fnameOnly[256];

	sprintf(fnameOnly, "%s", header_file);
    if ((p = strrchr(header_file, '/')) == (char *)NULL)
    {
		sprintf(zFailMsg, "Invalid header file received (%s); it must be a full path.", header_file);
		resp_log (mod, -1, REPORT_NORMAL, TEL_FILE_IO_ERROR, zFailMsg);
		return(-1);
    }
    *p = '\0';
    p++;
	sprintf(fnameOnly, "%s", p);

	sprintf (ocs_pathname, "%s/%s/%s", isp_base, NT_OCS_WORK_DIR, fnameOnly);

	ptr = fopen (ocs_pathname, "a");
	if (ptr == NULL)
	{
		sprintf(zFailMsg, "Failed to open (%s) while attempting to append result:%s",
				ocs_pathname, Result_line);
		resp_log (mod, -1, REPORT_NORMAL, TEL_FILE_IO_ERROR, zFailMsg);
		return (-1);
	}
	time (&sec);
	yTimePtr = localtime (&sec);
	strftime (current_time_date, sizeof (current_time_date) - 1, "%Y/%m/%d %H:%M:%S", yTimePtr);

	fprintf (ptr, "# %s %s\n", current_time_date, Result_line);
	fclose (ptr);

/* make the header directory if necessary */
	sprintf (pathname, "%s/%s", isp_base, NT_OCS_ERR_DIR);
	if (access (pathname, F_OK) != 0)
	{
		if (mkdir (pathname, 0755) == -1)
		{
			sprintf(zFailMsg, "Failed to make directory %s", pathname);
			resp_log (mod, -1, REPORT_NORMAL, TEL_FILE_IO_ERROR, zFailMsg);
			/* indicate a problem, but don't return */
		}
	}
	/* move the  header file to the error directory */
	sprintf (pathname, "%s/%s/%s", isp_base, NT_OCS_ERR_DIR, fnameOnly);
	sprintf (ocs_pathname, "%s/%s/%s", isp_base, NT_OCS_WORK_DIR, fnameOnly);

	if ( GV_removeCdfFiles )
	{
		unlink(ocs_pathname);
	}
	else
	{
		if (rename (ocs_pathname, pathname) != 0)
		{
			sprintf(zFailMsg, "Failed to move %s to %s", ocs_pathname, pathname);
			resp_log (mod, -1, REPORT_NORMAL, TEL_SYSTEM_CALL_ERROR, zFailMsg);
			return (-1);
		}
	}
	return (0);
}  /* END: NT_dispose on failure */

/*-----------------------------------------------------------------------------
dispose_on_failure:	Append the Result_line to the header and move it to
	the header error directory. Move the file we tried to fax (containing
	a text file or an unloaded phrase file) to the fax error
	directory regardless of the value of the delete flag in the header.
-----------------------------------------------------------------------------*/
int
dispose_on_failure (char *header_file, char *Result_line)
{
	char mod[] = { "dispose_on_failure" };
	char ocs_pathname[256];
	char pathname[256];
	char current_time_date[64];
	time_t sec;
	struct tm *yTimePtr;
	FILE *ptr;

	sprintf (ocs_pathname, "%s/%s/%s", isp_base, OCS_WORK_DIR, header_file);

	ptr = fopen (ocs_pathname, "a");
	if (ptr == NULL)
	{
		sprintf (log_buf, "Failed to open (%s) while attempting to " "append result:%s", ocs_pathname, Result_line);
		resp_log (mod, -1, REPORT_NORMAL, 3561, log_buf);
		return (-1);
	}
	time (&sec);
	yTimePtr = localtime (&sec);
	strftime (current_time_date, sizeof (current_time_date) - 1, "%Y/%m/%d %H:%M:%S", yTimePtr);

	fprintf (ptr, "# %s %s\n", current_time_date, Result_line);
	fclose (ptr);

/* make the header directory if necessary */
	sprintf (pathname, "%s/%s", isp_base, OCS_ERR_DIR);
	if (access (pathname, F_OK) != 0)
	{
		if (mkdir (pathname, 0755) == -1)
		{
			sprintf (log_buf, "Failed to make directory %s", pathname);
			resp_log (mod, -1, REPORT_NORMAL, 3562, log_buf);
/* indicate a problem, but don't return */
		}
	}
/* move the  header file to the error directory */
	sprintf (pathname, "%s/%s/%s", isp_base, OCS_ERR_DIR, header_file);

	if (rename (ocs_pathname, pathname) != 0)
	{
		sprintf (log_buf, "Failed to move %s to %s", ocs_pathname, pathname);
		resp_log (mod, -1, REPORT_NORMAL, 3563, log_buf);
		return (-1);
	}
	return (0);
}												  /* END: dispose on failure */


/*-----------------------------------------------------------------------------
dispose_on_success:	Append the Result_line to the header and move it to
			the header sent directory.

	Phrase or File:	If the delete flag is set then delete the file
	containing either a text file or an unloaded fax phrase. If the delete
	flag is not set, move the file to the sent directory.
-----------------------------------------------------------------------------*/
int
dispose_on_success (int zCall, char *header_file, char *Result_line)
{
	char ocs_pathname[256];
	char pathname[256];
	char current_time_date[64];
	time_t sec;
	struct tm *yTimePtr;
	FILE *ptr;
	char mod[] = { "dispose_on_success" };

	sprintf (ocs_pathname, "%s/%s/%s", isp_base, OCS_WORK_DIR, header_file);

/* Write the result into the header file */
	ptr = fopen (ocs_pathname, "a");
	if (ptr == NULL)
	{
		sprintf (log_buf, "Warning: Failed to open (%s) attempting to " "append result: %s. [%d, %s]", ocs_pathname, Result_line, errno, strerror(errno));
		resp_log (mod, zCall, REPORT_NORMAL, 3564, log_buf);
	}
	else
	{
		time (&sec);
		yTimePtr = localtime (&sec);
		strftime (current_time_date, sizeof (current_time_date) - 1, "%Y/%m/%d %H:%M:%S", yTimePtr);
		fprintf (ptr, "# %s %s\n", current_time_date, Result_line);
		fclose (ptr);
	}

/* Make header called dir, if necessary */
	sprintf (pathname, "%s/%s", isp_base, OCS_CALLED_DIR);
	if (access (pathname, F_OK) != 0)
	{
		if (mkdir (pathname, 0755) == -1)
		{
			sprintf (log_buf, "Warning:Failed to make directory %s", pathname);
			resp_log (mod, zCall, REPORT_DETAIL, 3565, log_buf);
/* indicate problem, but don't return */
		}
	}
/* move the  header file to the header sent directory */
	sprintf (pathname, "%s/%s/%s", isp_base, OCS_CALLED_DIR, header_file);
	if (rename (ocs_pathname, pathname) != 0)
	{
		sprintf (log_buf, "Warning: Failed to move %s to %s", ocs_pathname, pathname);
		resp_log (mod, zCall, REPORT_DETAIL, 3566, log_buf);
	}
	return (0);
}												  /* END: dispose on success */

/*-----------------------------------------------------------------------------
NT_dispose_on_success:	Append the Result_line to the header and move it to
			the header sent directory.

	Phrase or File:	If the delete flag is set then delete the file
	containing either a text file or an unloaded fax phrase. If the delete
	flag is not set, move the file to the sent directory.
-----------------------------------------------------------------------------*/
static int NT_dispose_on_success (char *header_file, char *Result_line)
{
	char ocs_pathname[256];
	char pathname[256];
	char current_time_date[64];
	time_t sec;
	struct tm *yTimePtr;
	FILE *ptr;
	char mod[] = "NT_dispose_on_success";
	char *headerOnly;

	if ( GV_removeCdfFiles )
	{
//		sprintf (log_buf, "GV_removeCdfFiles is on. Not moving cdf (%s).",
//					header_file);
//		resp_log (mod, 0, REPORT_VERBOSE, 3550, log_buf);
		return(0);
	}

	sprintf (ocs_pathname, "%s", header_file);

/* Write the result into the header file */
	ptr = fopen (ocs_pathname, "a");
	if (ptr == NULL)
	{
		sprintf (log_buf, 
				"Warning: Failed to open (%s) attempting to append result: %s. [%d, %s]",
				ocs_pathname, Result_line, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, TEL_FILE_IO_ERROR, log_buf);
	}
	else
	{
		time (&sec);
		yTimePtr = localtime (&sec);
		strftime (current_time_date, sizeof (current_time_date) - 1, "%Y/%m/%d %H:%M:%S", yTimePtr);
		fprintf (ptr, "# %s %s\n", current_time_date, Result_line);
		fclose (ptr);
	}

/* Make header called dir, if necessary */
	sprintf (pathname, "%s/%s", isp_base, NT_OCS_CALLED_DIR);
	if (access (pathname, F_OK) != 0)
	{
		if (mkdir (pathname, 0755) == -1)
		{
			sprintf (log_buf, "Warning:Failed to make directory %s", pathname);
			resp_log (mod, -1, REPORT_NORMAL, TEL_FILE_IO_ERROR, log_buf);
			/* indicate problem, but don't return */
		}
	}

/* move the  header file to the header sent directory */
    if ( (headerOnly = strrchr(header_file, '/')) == (char *)NULL)
    {
    	sprintf(pathname, "%s", header_file) ;
    }
    else
    {
		sprintf (pathname, "%s/%s/%s", isp_base, NT_OCS_CALLED_DIR, ++headerOnly);
    }

	if (rename (ocs_pathname, pathname) != 0)
	{
		sprintf (log_buf, "Warning: Failed to move %s to %s. [%d, %s]",
				ocs_pathname, pathname, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, TEL_SYSTEM_CALL_ERROR, log_buf);
	}
		sprintf (log_buf, "[%s, %d] Moved %s to %s.",
				__FILE__, __LINE__, ocs_pathname, pathname);
		resp_log (mod, -1, REPORT_NORMAL, TEL_SYSTEM_CALL_ERROR, log_buf);

	return (0);
} /* END: NT_dispose_on_success */


/*-----------------------------------------------------------------------------
updateTryCount:	Append the Result_line to the header.
-----------------------------------------------------------------------------*/
int
updateTryCount (int type, char *header_file, char *Result_line)
{
	FILE *ptr;
	char mod[] = { "updateTryCount" };

	if (type == 2)
		gaChangeRecord ('=', header_file, 1, "tryCount", 2, Result_line);
	else
	{
/* Write the result into the header file */
		ptr = fopen (header_file, "a");
		if (ptr == NULL)
		{
			sprintf (log_buf, "Warning: Failed to open (%s) attempting to " "append result: %s. [%d, %s]", header_file, Result_line, errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3564, log_buf);
		}
		else
		{
			fprintf (ptr, "%s\n", Result_line);
			fclose (ptr);
		}
	}
	return (0);
}												  /* END: updateTryCount */


/*-----------------------------------------------------------------------------
updatelastAttempt:	Append the Result_line to the header.
-----------------------------------------------------------------------------*/
int
updatelastAttempt (int type, char *header_file, char *Result_line)
{
	FILE *ptr;
	char mod[] = { "updatelastAttempt" };

	if (type == 2)
		gaChangeRecord ('=', header_file, 1, "lastAttempt", 2, Result_line);
	else
	{
/* Write the result into the header file */
		ptr = fopen (header_file, "a");
		if (ptr == NULL)
		{
			sprintf (log_buf, "Warning: Failed to open (%s) attempting to " "append result: %s. [%d, %s]", header_file, Result_line, errno, strerror(errno));
			resp_log (mod, -1, REPORT_NORMAL, 3564, log_buf);
		}
		else
		{
			fprintf (ptr, "%s\n", Result_line);
			fclose (ptr);
		}
	}
	return (0);
}												  /* END: updatelastAttempt */


/*----------------------------------------------------------------------
This function sets the answer time in msec and converts it to  a string.
----------------------------------------------------------------------*/
void get_answer_time (current_time)
char *current_time;
{
	struct tm *tm;
	struct timeval tp;
	struct timezone tzp;
	int ret_code;

/* gettimeofday() is a UNIX system call. */
	if ((ret_code = gettimeofday (&tp, &tzp)) == -1)
	{
		current_time = NULL;
		return;
	}

/* localtime() is a UNIX system call. */
	tm = localtime (&tp.tv_sec);

	sprintf (current_time, "%02d%02d%04d%02d%02d%02d%02ld",
		tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec, tp.tv_usec / 10000);
	return;
}												  /* get answer time */


int
getTicsFromHMS (int type, char *str)
{
	time_t lCurTics;
	struct tm lTm;
	char lBuf[20];

	time (&lCurTics);
	lTm = *(struct tm *) localtime (&lCurTics);

	memset (lBuf, 0, sizeof (lBuf));

	switch (type)
	{
		case 14:								  /* YYYYMMDDHHMMSS */
			lBuf[0] = '\0';
			strncpy (lBuf, str, 4);
			lTm.tm_year = atoi (lBuf) - 1900;

			lBuf[0] = '\0';
			lBuf[2] = '\0';
			strncpy (lBuf, str + 4, 2);
			lTm.tm_mon = atoi (lBuf) - 1;

			lBuf[0] = '\0';
			strncpy (lBuf, str + 6, 2);
			lTm.tm_mday = atoi (lBuf);

			lBuf[0] = '\0';
			strncpy (lBuf, str + 8, 2);
			lTm.tm_hour = atoi (lBuf);

			lBuf[0] = '\0';
			strncpy (lBuf, str + 10, 2);
			lTm.tm_min = atoi (lBuf);

			lBuf[0] = '\0';
			strncpy (lBuf, str + 12, 2);
			lTm.tm_sec = atoi (lBuf);

			return (mktime (&lTm));
			break;

		case 12:								  /* YYMMDDHHMMSS */
			lBuf[0] = '\0';
			strncpy (lBuf, str, 2);
			lTm.tm_year = atoi (lBuf) + 100;

			lBuf[0] = '\0';
			strncpy (lBuf, str + 2, 2);
			lTm.tm_mon = atoi (lBuf) - 1;

			lBuf[0] = '\0';
			strncpy (lBuf, str + 4, 2);
			lTm.tm_mday = atoi (lBuf);

			lBuf[0] = '\0';
			strncpy (lBuf, str + 6, 2);
			lTm.tm_hour = atoi (lBuf);

			lBuf[0] = '\0';
			strncpy (lBuf, str + 8, 2);
			lTm.tm_min = atoi (lBuf);

			lBuf[0] = '\0';
			strncpy (lBuf, str + 10, 2);
			lTm.tm_sec = atoi (lBuf);

			return (mktime (&lTm));
			break;

		case 6:									  /* HHMMSS */
			lBuf[0] = '\0';
			strncpy (lBuf, str, 2);
			lTm.tm_hour = atoi (lBuf);

			lBuf[0] = '\0';
			strncpy (lBuf, str + 2, 2);
			lTm.tm_min = atoi (lBuf);

			lBuf[0] = '\0';
			strncpy (lBuf, str + 4, 2);
			lTm.tm_sec = atoi (lBuf);

			return (mktime (&lTm));
			break;

		default:
			break;
	}

	return (-1);
}


/*-----------------------------------------------------------------------------
getFreePort() : This function gets the first free port.
-----------------------------------------------------------------------------*/
void getFreePort (requester, port_type, free_port, token1)
int requester;
int *port_type;
int *free_port;
char *token1;
{
	register int j, k;
	char res_id[10];
	char res_pid[10];
	char status_str[10];
	int available_port, field_num;
	char usage_type[20];
	static char mod[] = "getFreePort";
	int trunkGroupFound = 0;

	int descending = 0;
	int even = 0;
	int odd = 0;

	*free_port = -1;
	*port_type = 0;
	if (strlen (token1) != 0)					  /* particular resource type is requested */
	{
		for (field_num = 1; field_num < 10; field_num++)
		{
			get_field (token1, field_num, usage_type);
			if (strcmp (usage_type, "NODEFAULT") == 0)
			{
				return;							  /* NODEFAULT specified by the application */
			}

			if (strlen (usage_type) == 0)
			{
				break;
			}

			for (j = 0; j < gMaxSections; j++)
			{
				descending = 0;
				even = 0;
				odd = 0;
				if (strcmp (gGivePortRule[j].section, usage_type) == 0)
				{
					switch (gGivePortRule[j].rule)
					{
						case RULE_ASCENDING:
							break;

						case RULE_ASCENDING_EVEN:
							even = 1;
							break;

						case RULE_ASCENDING_ODD:
							odd = 1;
							break;

						case RULE_DESCENDING:
							descending = 1;
							break;

						case RULE_DESCENDING_EVEN:
							even = 1;
							descending = 1;
							break;

						case RULE_DESCENDING_ODD:
							odd = 1;
							descending = 1;
							break;

						default:
							continue;
							break;
					}
					break;
				}
			}

			if (j < gMaxSections)
			{
				if (checkRuleAndGetPort (requester, gGivePortRule[j].lastPortGiven, descending, even, odd, &available_port, usage_type) == 0)
				{
					*free_port = available_port;
					gGivePortRule[j].lastPortGiven = available_port;
					return;
				}
			}
		}

		for (field_num = 1; field_num < 10; field_num++)
		{
			get_field (token1, field_num, usage_type);

			if (strcmp (usage_type, "NODEFAULT") == 0)
			{
				return;							  /* NODEFAULT specified by the application */
			}

			if (strlen (usage_type) == 0)
			{
				break;
			}

/* find first free port for particular usage */
			get_available_port (requester, &available_port, usage_type);
			if (available_port == -1)			  /* no port of given type is available, continue */
			{
				;
			}
			else
			{
				*free_port = available_port;
				return;
			}
		}
	}

	if (requester == -99)						  /* Don't look for RESERVED resource */
	{
		trunkGroupFound = 0;
		for (field_num = 1; field_num < 10; field_num++)
		{
			get_field (token1, field_num, usage_type);
			if (strlen (usage_type) == 0)
			{
				break;
			}
			for (j = 0; j < tot_resources; j++)
			{
				if (strcmp (resource[j].res_usage, usage_type) == 0)
				{
					trunkGroupFound = 1;
					break;
				}
			}
		}
		if (trunkGroupFound == 0)
		{
			*free_port = -2;
		}
		else if(strlen(token1) > 0)
		{
			*free_port = -3;
		}

		return;

	}

/* find first free port of RESERV_PORT USAGE if available */
	for (j = 0; j < tot_resources; j++)
	{
/* if port is requetser */
		if (atoi (resource[j].res_no) == requester)
		{
			continue;
		}
		if (strcmp (resource[j].res_usage, RESERVE_PORT) == 0)
		{
			sprintf (log_buf, "Found Reserve Resource %s Status = %d", resource[j].res_no, res_status[atoi (resource[j].res_no)].status);
			resp_log (mod, j, REPORT_VERBOSE, 3550, log_buf);
			if (res_status[atoi (resource[j].res_no)].status == FREE)
			{
				read_fld (tran_tabl_ptr, j, APPL_STATUS, status_str);
				if (atoi (status_str) == STATUS_CHNOFF)
				{
					sprintf (log_buf, "Found FREE Reserve Resource %s, but port is OFF", resource[j].res_no);
					resp_log (mod, j, REPORT_VERBOSE, 3551, log_buf);
					continue;
				}
				else
				{
					*free_port = atoi (resource[j].res_no);
					*port_type = RESERVE;
					return;
				}
			}
		}										  /* strcmp */
	}											  /* for */

/* find first free port of any usage */
	for (j = 0; j < tot_resources; j++)
	{
/* if available port is requetser - skip */
		if (atoi (resource[j].res_no) == requester)
		{
			continue;
		}

		if ((strcmp (resource[j].res_usage, TWO_WAY_PORT) == 0) || (strncmp (resource[j].res_usage, "ArcA", 4) == 0))
		{
			sprintf (log_buf, "status of port %d is %d", j, res_status[atoi (resource[j].res_no)].status);
			resp_log (mod, j, REPORT_VERBOSE, 3552, log_buf);
			if (res_status[atoi (resource[j].res_no)].status == FREE)
			{
				(void) read_fld (tran_tabl_ptr, j, APPL_STATUS, status_str);
				if (atoi (status_str) == STATUS_IDLE)
				{
					*free_port = atoi (resource[j].res_no);
					return;
				}
				else
				{
					sprintf (log_buf, "Port %d Free, but no Dynamic Manager", j);
					resp_log (mod, j, REPORT_VERBOSE, 3552, log_buf);
					continue;
				}
			}
		}										  /* strcmp */
	}											  /* for */
/* no resource found */

	return;
}												  /* getFreePort */

/*-----------------------------------------------------------------------------
getFreePort_onSameSpan() : This function gets the first free port.
-----------------------------------------------------------------------------*/
void getFreePort_onSameSpan (inPort, requester, port_type, free_port, token1)
int inPort;
int requester;
int *port_type;
int *free_port;
char *token1;
{
	register int j, k;
	char res_id[10];
	char res_pid[10];
	char status_str[10];
	int available_port, field_num;
	char usage_type[20];
	static char mod[] = "getFreePort_onSameSpan";
	int trunkGroupFound = 0;

	int descending = 0;
	int even = 0;
	int odd = 0;

	*free_port = -1;
	*port_type = 0;

	if (strlen (token1) != 0)					  /* particular resource type is requested */
	{
		for (field_num = 1; field_num < 10; field_num++)
		{
			get_field (token1, field_num, usage_type);

			if (strcmp (usage_type, "NODEFAULT") == 0)
			{
				return;		/* NODEFAULT specified by the application */
			}

			if (strlen (usage_type) == 0)
			{
				break;
			}

			for (j = 0; j < gMaxSections; j++)
			{
				descending = 0;
				even = 0;
				odd = 0;

				if (strcmp (gGivePortRule[j].section, usage_type) == 0)
				{
					switch (gGivePortRule[j].rule)
					{
						case RULE_ASCENDING:
							break;

						case RULE_ASCENDING_EVEN:
							even = 1;
							break;

						case RULE_ASCENDING_ODD:
							odd = 1;
							break;

						case RULE_DESCENDING:
							descending = 1;
							break;

						case RULE_DESCENDING_EVEN:
							even = 1;
							descending = 1;
							break;

						case RULE_DESCENDING_ODD:
							odd = 1;
							descending = 1;
							break;

						default:
							continue;
							break;
					}
					break;
				}
			}

			if (j < gMaxSections)
			{
				if (checkRuleAndGetPort_onSameSpan (inPort, requester,
								gGivePortRule[j].lastPortGiven, descending, 
								even, odd, &available_port, usage_type) == 0)
				{
					*free_port = available_port;
					gGivePortRule[j].lastPortGiven = available_port;

					return;
				}
			}
		}

		for (field_num = 1; field_num < 10; field_num++)
		{
			get_field (token1, field_num, usage_type);

			if (strcmp (usage_type, "NODEFAULT") == 0)
			{
				return;							  /* NODEFAULT specified by the application */
			}

			if (strlen (usage_type) == 0)
			{
				break;
			}

/* find first free port for particular usage */
			get_available_port_onSameSpan (inPort, requester, &available_port, usage_type);
			if (available_port == -1)			  /* no port of given type is available, continue */
			{
				;
			}
			else
			{
				*free_port = available_port;
				return;
			}
		}
	}

	if (requester == -99)						  /* Don't look for RESERVED resource */
	{
		trunkGroupFound = 0;
		for (field_num = 1; field_num < 10; field_num++)
		{
			get_field (token1, field_num, usage_type);
			if (strlen (usage_type) == 0)
			{
				break;
			}
			for (j = 0; j < tot_resources; j++)
			{
				if (strcmp (resource[j].res_usage, usage_type) == 0)
				{
					trunkGroupFound = 1;
					break;
				}
			}
		}
		if (trunkGroupFound == 0)
		{
			*free_port = -2;
		}
		else if(strlen(token1) > 0)
		{
			*free_port = -3;
		}

		return;

	}

/* find first free port of RESERV_PORT USAGE if available */
	for (j = 0; j < tot_resources; j++)
	{
/* if port is requetser */
		if (atoi (resource[j].res_no) == requester)
		{
			continue;
		}
		if (strcmp (resource[j].res_usage, RESERVE_PORT) == 0)
		{
			sprintf (log_buf, "Found Reserve Resource %s Status = %d",
					resource[j].res_no, res_status[atoi (resource[j].res_no)].status);
			resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);
			if (res_status[atoi (resource[j].res_no)].status == FREE)
			{
				read_fld (tran_tabl_ptr, j, APPL_STATUS, status_str);
				if (atoi (status_str) == STATUS_CHNOFF)
				{
					sprintf (log_buf, 
							"Found FREE Reserve Resource %s, but port is OFF", resource[j].res_no);
					resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);
					continue;
				}
				else
				{
					*free_port = atoi (resource[j].res_no);
					*port_type = RESERVE;
					return;
				}
			}
		}										  /* strcmp */
	}											  /* for */

/* find first free port of any usage */
	for (j = 0; j < tot_resources; j++)
	{
/* if available port is requetser - skip */
		if (atoi (resource[j].res_no) == requester)
		{
			continue;
		}

		if ((strcmp (resource[j].res_usage, TWO_WAY_PORT) == 0) || (strncmp (resource[j].res_usage, "ArcA", 4) == 0))
		{
			sprintf (log_buf, 
					"status of port %d is %d", j, res_status[atoi (resource[j].res_no)].status);
			resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);
			if (res_status[atoi (resource[j].res_no)].status == FREE)
			{
				(void) read_fld (tran_tabl_ptr, j, APPL_STATUS, status_str);
				if (atoi (status_str) == STATUS_IDLE)
				{
					*free_port = atoi (resource[j].res_no);
					return;
				}
				else
				{
					sprintf (log_buf, "Port %d Free, but no Dynamic Manager", j);
					resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);
					continue;
				}
			}
		}										  /* strcmp */
	}											  /* for */
/* no resource found */

	return;

} // END: getFreePort_onSameSpan()

int
checkRuleAndGetPort (int requester, int lastGiven, int descending, int even, int odd, int *port, char *usage_type)
{
	int start, end, step, i;

	start = 0;
	end = tot_resources - 1;
	step = 1;
	*port = -1;

	if (even == 1)
	{
		step = 2;
		if (end % 2 != 0)
			end--;

		if (lastGiven == -1)
		{
			if (descending == 1)
				lastGiven = 0;
			else
				lastGiven = -2;
		}
	}
	else if (odd == 1)
	{
		start = 1;
		step = 2;
		if (end % 2 == 0)
			end--;

		if (lastGiven == -1)
		{
			if (descending == 1)
				lastGiven = 1;
		}
	}

	if (descending == 1)
	{
		for (i = lastGiven - step; i >= start; i = i - step)
		{
			if (strcmp (resource[i].res_usage, usage_type) == 0)
			{
				if (checkIfFree (i) == 0)
				{
					if (requester == -99)
					{
						if ((appType[i] == 2) || (appType[i] == 3))
						{
							*port = i;
							return (0);
						}
						else
							continue;
					}
					else
					{
						*port = i;
						return (0);
					}
				}
			}
		}
		for (i = end; i >= lastGiven; i = i - step)
		{
			if (strcmp (resource[i].res_usage, usage_type) == 0)
			{
				if (checkIfFree (i) == 0)
				{
					if (requester == -99)
					{
						if ((appType[i] == 2) || (appType[i] == 3))
						{
							*port = i;
							return (0);
						}
						else
							continue;
					}
					else
					{
						*port = i;
						return (0);
					}
				}
			}
		}
		return (-1);
	}
	else
	{
		for (i = lastGiven + step; i <= end; i = i + step)
		{
			if (strcmp (resource[i].res_usage, usage_type) == 0)
			{
				if (checkIfFree (i) == 0)
				{
					if (requester == -99)
					{
						if ((appType[i] == 2) || (appType[i] == 3))
						{
							*port = i;
							return (0);
						}
						else
							continue;
					}
					else
					{
						*port = i;
						return (0);
					}
				}
			}
		}
		for (i = start; i <= lastGiven; i = i + step)
		{
			if (strcmp (resource[i].res_usage, usage_type) == 0)
			{
				if (checkIfFree (i) == 0)
				{
					if (requester == -99)
					{
						if ((appType[i] == 2) || (appType[i] == 3))
						{
							*port = i;
							return (0);
						}
						else
							continue;
					}
					else
					{
						*port = i;
						return (0);
					}
				}
			}
		}
		return (-1);
	}
	return (-1);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int checkRuleAndGetPort_onSameSpan (int inPort, int requester, int lastGiven,
			int descending, int even, int odd, int *port, char *usage_type)
{
    static char mod[]="checkRuleAndGetPort_onSameSpan";
	int start, end, step, i;
	int		multiplier;
	int		startPort;
	int		endPort;

	multiplier = inPort / 48;
	startPort  = multiplier * 48;
	endPort    = startPort + 48 - 1;

//	start = 0;
//	end = tot_resources - 1;
	start = startPort;
	end   = endPort;
	step = 1;
	*port = -1;

	if (even == 1)
	{
		step = 2;
		if (end % 2 != 0)
			end--;

		if (lastGiven == -1)
		{
			if (descending == 1)
				lastGiven = 0;
			else
				lastGiven = -2;
		}
	}
	else if (odd == 1)
	{
		start = 1;
		step = 2;
		if (end % 2 == 0)
			end--;

		if (lastGiven == -1)
		{
			if (descending == 1)
				lastGiven = 1;
		}
	}

	if (descending == 1)
	{
		for (i = lastGiven - step; i >= start; i = i - step)
		{
			if (strcmp (resource[i].res_usage, usage_type) == 0)
			{
				if (checkIfFree (i) == 0)
				{
					if (requester == -99)
					{
						if ((appType[i] == 2) || (appType[i] == 3))
						{
							*port = i;
							sprintf (log_buf, "SHM [%d] returning port %d, requester=%d, appType[%d]=%d",
											__LINE__, *port, requester, i, appType[i]);
							resp_log (mod, -1, REPORT_VERBOSE, 3553, log_buf);
							return (0);
						}
						else
							continue;
					}
					else
					{
						*port = i;
						sprintf (log_buf, "SHM [%d] returning port %d, requester=%d,",
											__LINE__, *port, requester);
						resp_log (mod, -1, REPORT_VERBOSE, 3553, log_buf);
						return (0);
					}
				}
			}
		}
		for (i = end; i >= lastGiven; i = i - step)
		{
			if (strcmp (resource[i].res_usage, usage_type) == 0)
			{
				if (checkIfFree (i) == 0)
				{
					if (requester == -99)
					{
						if ((appType[i] == 2) || (appType[i] == 3))
						{
							*port = i;
							sprintf (log_buf, "SHM [%d] returning port %d, requester=%d, appType[%d]=%d",
											__LINE__, *port, requester, i, appType[i]);
							resp_log (mod, -1, REPORT_VERBOSE, 3553, log_buf);
							return (0);
						}
						else
							continue;
					}
					else
					{
						*port = i;
						sprintf (log_buf, "SHM [%d] returning port %d, requester=%d,",
											__LINE__, *port, requester);
						resp_log (mod, -1, REPORT_VERBOSE, 3553, log_buf);
						return (0);
					}
				}
			}
		}
		return (-1);
	}
	else
	{
		for (i = lastGiven + step; i <= end; i = i + step)
		{
			if (strcmp (resource[i].res_usage, usage_type) == 0)
			{
				if (checkIfFree (i) == 0)
				{
					if (requester == -99)
					{
						if ((appType[i] == 2) || (appType[i] == 3))
						{
							*port = i;
							sprintf (log_buf, "SHM [%d] returning port %d, requester=%d, appType[%d]=%d",
											__LINE__, *port, requester, i, appType[i]);
							resp_log (mod, -1, REPORT_VERBOSE, 3553, log_buf);
							return (0);
						}
						else
						{
							continue;
						}
					}
					else
					{
						*port = i;
						sprintf (log_buf, "SHM [%d] returning port %d, requester=%d,",
											__LINE__, *port, requester);
						resp_log (mod, -1, REPORT_VERBOSE, 3553, log_buf);
						return (0);
					}
				}
			}
		}
		for (i = start; i <= lastGiven; i = i + step)
		{
			if (strcmp (resource[i].res_usage, usage_type) == 0)
			{
				if (checkIfFree (i) == 0)
				{
					if (requester == -99)
					{
						if ((appType[i] == 2) || (appType[i] == 3))
						{
							*port = i;
							sprintf (log_buf, "SHM [%d] returning port %d, requester=%d, appType[%d]=%d",
											__LINE__, *port, requester, i, appType[i]);
							resp_log (mod, -1, REPORT_VERBOSE, 3553, log_buf);
							return (0);
						}
						else
						{
							continue;
						}
					}
					else
					{
						*port = i;
						sprintf (log_buf, "SHM [%d] returning port %d, requester=%d,",
											__LINE__, *port, requester);
						resp_log (mod, -1, REPORT_VERBOSE, 3553, log_buf);
						return (0);
					}
				}
			}
		}
		return(-1);
	}
	return (-1);
} // END: checkRuleAndGetPort_onSameSpan()

int
readRulesFromCfg ()
{
	char mod[] = "readRulesFromCfg";
	FILE *fp;
	char buf[256];
	char parm[32];
	char tmpRuleBuf[64];
	int rc;
	char *pChar;
	int sectionStarted = 0;
	int lenBufRead;
	int i;
	char huntGroupCfgFileName[256];
	int defaultSectionRule = RULE_FIRST_AVAILABLE;

	sprintf (huntGroupCfgFileName, "%s/%s", table_path, HUNT_GROUP_CFG);
	gMaxSections = 0;
	memset (gGivePortRule, 0, sizeof (struct GivePortRule) * MAX_NUM_SECTIONS);

	fp = fopen (huntGroupCfgFileName, "r");
	if (fp == NULL)
	{
		sprintf (log_buf, "Failed to open file %s. [%d, %s]", huntGroupCfgFileName, errno, strerror(errno));
		resp_log (mod, -1, REPORT_VERBOSE, 3553, log_buf);
		return (-1);
	}

	while (fgets (buf, sizeof (buf), fp) != NULL)
	{
		if (buf[0] == '[')
		{
			gMaxSections++;

			gGivePortRule[gMaxSections - 1].rule = -1;
			pChar = (char *) strchr (buf, ']');
			*pChar = '\0';
			sprintf (gGivePortRule[gMaxSections - 1].section, "%s", &buf[1]);
			gGivePortRule[gMaxSections - 1].lastPortGiven = -1;

			sectionStarted = 1;
		}
		else if (sectionStarted == 1)
		{
			lenBufRead = strlen (buf);
			if (buf[lenBufRead - 1] == '\n')
			{
				buf[lenBufRead - 1] = '\0';
				lenBufRead--;
			}
			if (lenBufRead > 0)
			{
				sectionStarted = 0;

				for (i = 0; i < lenBufRead; i++)
					buf[i] = toupper (buf[i]);

				if (strstr (buf, "RULE") != NULL)
					sscanf (buf, "%[^=]=%s", parm, tmpRuleBuf);

				if (strcmp (tmpRuleBuf, "FIRSTAVAILABLE") == 0)
				{
					gGivePortRule[gMaxSections - 1].rule = RULE_FIRST_AVAILABLE;
				}
				else if (strcmp (tmpRuleBuf, "ASCENDING") == 0)
				{
					gGivePortRule[gMaxSections - 1].rule = RULE_ASCENDING;
				}
				else if (strcmp (tmpRuleBuf, "ASCENDINGEVEN") == 0)
				{
					gGivePortRule[gMaxSections - 1].rule = RULE_ASCENDING_EVEN;
				}
				else if (strcmp (tmpRuleBuf, "ASCENDINGODD") == 0)
				{
					gGivePortRule[gMaxSections - 1].rule = RULE_ASCENDING_ODD;
				}
				else if (strcmp (tmpRuleBuf, "DESCENDING") == 0)
				{
					gGivePortRule[gMaxSections - 1].rule = RULE_DESCENDING;
				}
				else if (strcmp (tmpRuleBuf, "DESCENDINGEVEN") == 0)
				{
					gGivePortRule[gMaxSections - 1].rule = RULE_DESCENDING_EVEN;
				}
				else if (strcmp (tmpRuleBuf, "DESCENDINGODD") == 0)
				{
					gGivePortRule[gMaxSections - 1].rule = RULE_DESCENDING_ODD;
				}
				else
				{
					sprintf (log_buf, "Invalid rule (%s) for section (%s). "
						"Possible values are FirstAvailable, Ascending, "
						"Descending, AscendingEven, DescendingEven, " "AscendingOdd, DescendingOdd. Ignoring.", buf, gGivePortRule[gMaxSections - 1].section);
					resp_log (mod, -1, REPORT_NORMAL, 3554, log_buf);
					sectionStarted = 1;
				}

				if (strcmp (gGivePortRule[gMaxSections - 1].section, "Default") == 0)
				{
					defaultSectionRule = gGivePortRule[gMaxSections - 1].rule;
				}
			}
		}
	}

	fclose (fp);

	for (i = 0; i < gMaxSections; i++)
	{
		if (gGivePortRule[i].rule == -1)
		{
			gGivePortRule[i].rule = defaultSectionRule;
		}
	}
	return (0);
}


int
checkIfFree (int zPort)
{
	char mod[] = { "checkIfFree" };
	int k;
	char status_str[10];
	char res_id[10];
	char res_pid[10];

	if (res_status[atoi (resource[zPort].res_no)].status == FREE)
	{
		read_fld (tran_tabl_ptr, zPort, APPL_STATUS, status_str);
		if ((atoi (status_str) == STATUS_IDLE) || (atoi (status_str) == STATUS_OFF))
		{
//			sprintf (log_buf, "Port %d Free, status=(%s)", zPort, status_str);
//			resp_log (mod, zPort, REPORT_VERBOSE, 3555, log_buf);
			return (0);
		}
		else
		{
			sprintf (log_buf, "Port %d Free, but no Dynamic Manager", zPort);
			resp_log (mod, zPort, REPORT_VERBOSE, 3555, log_buf);
			return (-1);
		}
	}
	return (-1);
}


/*---------------------------------------------------------------------------
This routine will check if the resource of desired type is available.
Return the port is available or -1 if not.
-------------------------------------------------------------------------*/
void get_available_port (requester, available_port, token1)
int requester;
int *available_port;
char *token1;
{
	register int j, k;
	int app_pid;
	char res_id[10];
	char res_pid[10];
	char status_str[10];
	char program_name[50];
	char mod[] = { "get_available_port" };

	*available_port = -1;

/* find first free port of any usage */
	for (j = 0; j < tot_resources; j++)
	{
/* if available port is requetser - skip */
		if (atoi (resource[j].res_no) == requester)
			continue;
		if (strcmp (resource[j].res_usage, token1) == 0)
		{
			if (res_status[atoi (resource[j].res_no)].status == FREE)
			{
				read_fld (tran_tabl_ptr, j, APPL_STATUS, status_str);
												  /* dynamic manager as well as reserved */
				if ((atoi (status_str) == STATUS_IDLE) || (atoi (status_str) == STATUS_OFF))
				{
					if (requester == -99)
					{
						if ((appType[j] == 2) || (appType[j] == 3))
							*available_port = atoi (resource[j].res_no);
						else
							continue;
					}
					else
						*available_port = atoi (resource[j].res_no);
					for (k = 0; k < tot_resources; k++)
					{
/* search resource in shared memory */
						(void) read_fld (tran_tabl_ptr, k, APPL_RESOURCE, res_id);
						if (atoi (res_id) == atoi (resource[j].res_no))
						{
							return;
						}
					}							  /*find resource id from shared memory */
				}
				else
				{
					sprintf (log_buf, "Port %d Free, but no Dynamic Manager", j);
					resp_log (mod, j, REPORT_VERBOSE, 3556, log_buf);
					continue;
				}
			}
		}										  /* strcmp */
	}											  /* for */
/* no resource found */
	return;
}

/*---------------------------------------------------------------------------
This routine will check if the resource of desired type is available.
Return the port is available or -1 if not.
-------------------------------------------------------------------------*/
int get_available_port_onSameSpan (inPort, requester, available_port, token1)
int inPort;
int requester;
int *available_port;
char *token1;
{
	register int j, k;
	int app_pid;
	char res_id[10];
	char res_pid[10];
	char status_str[10];
	char program_name[50];
	char mod[] = "get_available_port_onSameSpan";

	int		multiplier;
	int		startPort;
	int		endPort;

	multiplier = inPort / 48;
	startPort  = multiplier * 48;
	endPort    = startPort + 48 - 1;

	*available_port = -1;

	sprintf (log_buf, "[%s, %d] Getting available port between %d and %d.",
			__FILE__, __LINE__, startPort, endPort);
	resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);

/* find first free port of any usage */
	for (j = startPort; j <= endPort; j++)
	{
/* if available port is requetser - skip */
		if (atoi (resource[j].res_no) == requester)
			continue;

		if (strcmp (resource[j].res_usage, token1) == 0)
		{
			if (res_status[atoi (resource[j].res_no)].status == FREE)
			{
				read_fld (tran_tabl_ptr, j, APPL_STATUS, status_str);
												  /* dynamic manager as well as reserved */
				if ((atoi (status_str) == STATUS_IDLE) || (atoi (status_str) == STATUS_OFF))
				{
					if (requester == -99)
					{
						if ((appType[j] == 2) || (appType[j] == 3))
						{
							*available_port = atoi (resource[j].res_no);

							sprintf (log_buf, "SHM [%d] status_str=(%s) requester=%d "
								"appType[%d]=%d  available_port=%d",
								__LINE__, status_str, requester, j, appType[j], *available_port);
							resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);
						}
						else
							continue;
					}
					else
					{
						*available_port = atoi (resource[j].res_no);
						sprintf (log_buf, "SHM [%d] status_str=(%s) requester=%d"
								"appType[%d]=%d  available_port=%d",
								__LINE__, status_str, requester, j, appType[j], *available_port);
						resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);
					}
					for (k = 0; k < tot_resources; k++)
					{
/* search resource in shared memory */
						(void) read_fld (tran_tabl_ptr, k, APPL_RESOURCE, res_id);
						if (atoi (res_id) == atoi (resource[j].res_no))
						{
							return(0);
						}
					}							  /*find resource id from shared memory */
				}
				else
				{
					sprintf (log_buf, "Port %d Free, but no Dynamic Manager", j);
					resp_log (mod, -1, REPORT_VERBOSE, RESP_BASE, log_buf);
					continue;
				}
			}
		}										  /* strcmp */
	}											  /* for */
/* no resource found */
	return(0);
} // END: get_available_port_onSameSpan()

int initDynMgrStruct ()
{
	int newDynMgr = 0;
	int i, counter;
	char mod[] = { "initDynMgrStruct" };

	GV_NumberOfDMRunning = 0;

	if (beginPort == 0)
	{
		counter = 0;
	}
	else
	{
		counter = dynMgrStartNum * 48;
		newDynMgr = dynMgrStartNum * 48;
	}

	for (i = counter; i < tot_resources + counter; i++)
	{
		if (i == newDynMgr)
		{
			GV_NumberOfDMRunning++;
//newDynMgr = newDynMgr + tot_resources/2;
			newDynMgr = newDynMgr + 48;
			gGV_DMInfo[GV_NumberOfDMRunning + dynMgrStartNum - 1].startPort = i;
			if (newDynMgr < tot_resources + counter)
			{
//gGV_DMInfo[GV_NumberOfDMRunning - 1].endPort = i + tot_resources/2 - 1;
				gGV_DMInfo[GV_NumberOfDMRunning + dynMgrStartNum - 1].endPort = i + 47;
			}
			else if (newDynMgr == tot_resources + counter)
			{
				gGV_DMInfo[GV_NumberOfDMRunning + dynMgrStartNum - 1].endPort = tot_resources + counter - 1;
			}
			else
				gGV_DMInfo[GV_NumberOfDMRunning + dynMgrStartNum - 1].endPort = tot_resources + counter - 1;

			sprintf (gGV_DMInfo[GV_NumberOfDMRunning + dynMgrStartNum - 1].dynMgrName, "%s", "ArcIPDynMgr");

			sprintf (log_buf, "Starting up call manager. newDynMgr=%d, start port=%d, end port = %d.",
				newDynMgr,
				gGV_DMInfo[GV_NumberOfDMRunning + dynMgrStartNum - 1].startPort,
				gGV_DMInfo[GV_NumberOfDMRunning + dynMgrStartNum - 1].endPort);
			resp_log (mod, -1, REPORT_DETAIL, 3556, log_buf);
		}
	}
	return (0);
}


/*--------------------------------------------------------------------------
Check if Dynamic manger is already running.
---------------------------------------------------------------------------*/
int
checkDynMgr (char *dynMgrName)
{
	static char mod[] = "checkDynMgr";
	FILE *fin;									  /* file pointer for the ps pipe */
	int i;
	char buf[BUFSIZE];
	char buf1[128];
	int dynMgrPid;

	if ((fin = popen (ps, "r")) == NULL)		  /* open the process table */
	{
		sprintf (log_buf, "Failed to verify that Dynamic Manager is running. [%d, %s]", errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3411, log_buf);
		return (-1);
	}

	(void) fgets (buf, sizeof buf, fin);		  /* strip off the header */
/* get the responsibility s proc_id */
	i = 0;
	while (fgets (buf, sizeof buf, fin) != NULL)
	{
		if (strstr (buf, dynMgrName) != NULL)
		{
			sscanf (buf, "%s %d", buf1, &dynMgrPid);
			sprintf (log_buf, "Dynamic manager (%s) is already running, about to kill pid %d.", dynMgrName, dynMgrPid);
			resp_log (mod, -1, REPORT_DETAIL, 3412, log_buf);
			if (dynMgrPid != 0)
				(void) kill (dynMgrPid, SIGKILL);
			(void) pclose (fin);
			return (1);
		}
	}
	(void) pclose (fin);

	return (ISP_SUCCESS);
}												  /* checkDynMgr */


/*--------------------------------------------------------------------------
Check if RunVXID is already running.
---------------------------------------------------------------------------*/
int
checkAndKillRunVXID (char *procName)
{
	static char mod[] = "checkAndKillVXID";
	FILE *fin;									  /* file pointer for the ps pipe */
	int i;
	char buf[BUFSIZE];
	char buf1[128];
	int RunVXIDPid;

	if ((fin = popen (ps, "r")) == NULL)		  /* open the process table */
	{
		sprintf (log_buf, "Failed to verify that RunVXID is running. [%d, %s]", errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3052, log_buf);
		exit (0);
	}

	(void) fgets (buf, sizeof buf, fin);		  /* strip off the header */
/* get the responsibility s proc_id */
	i = 0;
	while (fgets (buf, sizeof buf, fin) != NULL)
	{
		if (strstr (buf, procName) != NULL)
		{
			sscanf (buf, "%s %d", buf1, &RunVXIDPid);
			sprintf (log_buf, "RunVXID process is running, about to kill pid %d.", RunVXIDPid);
			resp_log (mod, -1, REPORT_DETAIL, 3412, log_buf);
			if (RunVXIDPid != 0)
				(void) kill (RunVXIDPid, SIGKILL);
			
			//return (1);

			sleep(1);
		}
	}
	(void) pclose (fin);

	return (0);
}

/*--------------------------------------------------------------------------
Check if GoogleSpeechRec.jar is already running.
---------------------------------------------------------------------------*/
int
checkAndKillGoogleSRJar (char *procName)
{
	static char mod[] = "checkAndKillGoogleSRJar";
	FILE *fin;									  /* file pointer for the ps pipe */
	int i;
	char buf[BUFSIZE];
	char buf1[128];
	int googleSRJarPID;

	if ((fin = popen (ps, "r")) == NULL)		  /* open the process table */
	{
		sprintf (log_buf, "Failed to verify that %s is running. [%d, %s]", procName, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3052, log_buf);
		exit (0);
	}

	(void) fgets (buf, sizeof buf, fin);		  /* strip off the header */
/* get the responsibility s proc_id */
	i = 0;
	while (fgets (buf, sizeof buf, fin) != NULL)
	{

		if (strstr (buf, procName) != NULL)
		{
			sscanf (buf, "%s %d", buf1, &googleSRJarPID);
			sprintf (log_buf, "%s process is running, killing pid %d.", procName, googleSRJarPID);
			resp_log (mod, -1, REPORT_DETAIL, 3412, log_buf);
			if (googleSRJarPID != 0)
			{
				(void) kill (googleSRJarPID, SIGKILL);
				sprintf (log_buf, "%s process pid %d killed", procName, googleSRJarPID);
				resp_log (mod, -1, REPORT_DETAIL, 3412, log_buf);
			}
			
			//return (1);

			sleep(1);
		}
	}
	(void) pclose (fin);

	return (0);
} // END: GoogleSpeechRec.jar()

/*--------------------------------------------------------------------------
Check if chatGPT/ arcChatGPTClientMgr.sh is running.
---------------------------------------------------------------------------*/
static int checkAndKillChatGPT (char *procName)
{
	static char mod[] = "checkAndKillChatGPT";
	FILE *fin;									  /* file pointer for the ps pipe */
	int i;
	char buf[BUFSIZE];
	char buf1[128];
	int chatGPTPid;

	if ((fin = popen (ps, "r")) == NULL)		  /* open the process table */
	{
		sprintf (log_buf, "Failed to verify that %s is running. [%d, %s]", procName, errno, strerror(errno));
		resp_log (mod, -1, REPORT_NORMAL, 3052, log_buf);
		exit (0);
	}

	(void) fgets (buf, sizeof buf, fin);		  /* strip off the header */
/* get the responsibility s proc_id */
	i = 0;
	while (fgets (buf, sizeof buf, fin) != NULL)
	{

		if (strstr (buf, procName) != NULL)
		{
			sscanf (buf, "%s %d", buf1, &chatGPTPid);
			sprintf (log_buf, "%s process is running, killing pid %d.", procName, chatGPTPid);
			resp_log (mod, -1, REPORT_DETAIL, 3412, log_buf);
			if (chatGPTPid != 0)
			{
				(void) kill (chatGPTPid, SIGKILL);
				sprintf (log_buf, "%s process pid %d killed", procName, chatGPTPid);
				resp_log (mod, -1, REPORT_DETAIL, 3412, log_buf);
			}
			
			//return (1);

			sleep(1);
		}
	}
	(void) pclose (fin);

	return (0);
} // END: checkAndKillChatGPT()

int
shouldIstartDM ()
{
	int i, j;
	long currentTime;

//for(j = 0; j < GV_NumberOfDMRunning; j++)
	for (j = dynMgrStartNum; j < GV_NumberOfDMRunning + dynMgrStartNum; j++)
	{
		if (gGV_DMInfo[j].startTime == 0)
		{
			time (&currentTime);
			if (currentTime - gGV_DMInfo[j].stopTime > 3)
			{
				StartUpDynMgr (j);

				for (i = gGV_DMInfo[j].startPort; i <= gGV_DMInfo[j].endPort; i++)
				{
					if (appType[i] == 1)		  /* Static App */
					{
						startStaticApp (i);
					}
				}
				break;
			}
		}
	}
	return (0);
}


#if 0
/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
sReleaseLicenses ()
{
	static char mod[] = "sReleaseLicenses";

	(void) flexLMCheckIn (&GV_lmHandle, "IPTEL", log_buf);
	sprintf (log_buf, "Feature \"IPTEL\" licenses are released.");
	resp_log (mod, -1, REPORT_VERBOSE, 3439, log_buf);

	return (0);
}												  // END: sReleaseLicenses()
#endif

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
sRequestLicenses (int numResourcesToRequest)
{
	int rc;
	int tot_lic_in_use = 0;
	int num_lics = 400;
	int resourceFlag = 0;
	char failureReason[256];
	static char mod[] = "sRequestLicenses";
	char product[12] = "IPTEL";
	char version[12] = "2.0";
	char tmpBuf[512];
    time_t  secs;
    struct tm   *PT_time;
    char    charMon[16];
    char    charYear[16];
    int     intMon;
    int     intYear;

	if (numResourcesToRequest == -1)
	{
		if ((rc = getNumResources (resource_tabl, &numResourcesToRequest)) == -1)
		{
			return (-1);
		}
	}
#if 0
	else
	{
		resourceFlag = 1;
	}
#endif

	// rc = flexLMGetNumLicenses (product, version, &tot_lic_in_use, &num_lics, tmpBuf);
	numResourcesToRequest = 96;
	rc = 0;
	tot_lic_in_use = 0;
	num_lics = 96;
	if (rc == 0)
	{
		if ((num_lics - tot_lic_in_use) < numResourcesToRequest)
		{
			numResourcesToRequest = num_lics - tot_lic_in_use;
		}
	}
	else
	{
		sprintf (log_buf, "flexLMGetNumLicenses() failed.  [%d, %s]", rc, tmpBuf);
		resp_log (mod, -1, REPORT_NORMAL, 3439, log_buf);
		sprintf (log_buf, "License failure. Unable to start Telecom Services.");
		resp_log (mod, -1, REPORT_NORMAL, 3439, log_buf);
		numResourcesToRequest = 96;
		return(-1);
	}

#if 0
	if (resourceFlag == 1)
	{
		numResourcesToRequest = num_lics / 2;
	}
#endif

#if 0
	sprintf (log_buf, "For %sV%s, total number of license available=%d; "
			"licenses in use=%d; licenses to request=%d.",
			product, version, num_lics, tot_lic_in_use, numResourcesToRequest);
	resp_log (mod, -1, REPORT_VERBOSE, 3439, log_buf);
	if ((rc = flexLMCheckOut (product, version, numResourcesToRequest,
						&GV_lmHandle, failureReason)) != 0)
	{
		sprintf (log_buf, "License failure.  %s.", failureReason);
		resp_log (mod, -1, REPORT_NORMAL, 3404, log_buf);
		sprintf (log_buf, "Unable to start Telecom Services.");
		resp_log (mod, -1, REPORT_NORMAL, 3439, log_buf);
		printf ("%s\n", log_buf);
		return (-1);
	}
#endif
	Licensed_Resources = numResourcesToRequest;
	sprintf (log_buf, "Successfully checked out %d licenses.",	
				numResourcesToRequest);
	resp_log (mod, -1, REPORT_VERBOSE, 3439, log_buf);

	return (0);
}												  // END: sRequestLicenses()

/*-------------------------------------------------------------------
getNumResources(): This Routine load resource table into memory.
--------------------------------------------------------------------*/
static int
getNumResources (char *resource_file, int *numResources)
{
	static char mod[] = "getNumResources";
	char record[256];
	FILE *fp;
	int i;

	*numResources = 0;
	i = 0;
	if ((fp = fopen (resource_file, "r")) == NULL)
	{
		sprintf (log_buf, "Can't open file %s. [%d, %s]", resource_file, errno, strerror (errno));
		resp_log (mod, -1, REPORT_NORMAL, 3491, log_buf);
		return (-1);
	}

	while (fgets (record, sizeof (record), fp) != NULL)
	{
		if (strcmp (record, "DCHANNEL") == 0)
		{
			continue;
		}
		i++;
	}

	(void) fclose (fp);
	*numResources = i;

	return (0);

}												  // END: getNumResources()


int
handleLicenseFailure ()
{
	static char mod[] = "handleLicenseFailure";
	char log_buf[256];
	char stopBuf[256];
	char *ispbase;
	char base_env_var[] = "ISPBASE";

	if ((ispbase = getenv ("ISPBASE")) != NULL)
	{
		sprintf (log_buf, "%s", "Env. Variable ISPBASE not set; assuming " "/home/arc/.ISP");
		resp_log (mod, -1, REPORT_VERBOSE, 3577, log_buf);
		sprintf (stopBuf, "%s", "nohup /home/arc/.ISP/Telecom/bin/stoptel &");
	}
	else
	{
		sprintf (stopBuf, "nohup %s/Telecom/bin/stoptel &", ispbase);
	}

	sprintf (log_buf, "%s", "License expired or reconnection error.  " "Shutting down.");
	resp_log (mod, -1, REPORT_VERBOSE, 3577, log_buf);
	system (stopBuf);
	return (0);

}												  // END: handleLicenseFailure()