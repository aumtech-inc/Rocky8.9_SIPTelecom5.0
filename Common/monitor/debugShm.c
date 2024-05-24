/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
#include "BNS.h"
#include "resp.h"
#include "ISPSYS.h"
#include "shm_def.h"
#include "monitor.h"
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <ResStat.h>

#include <sys/time.h>
#include <sys/timeb.h>

#ifdef ISP_LOG
#include <ISPCommon.h>
#endif 

#define CLEAR		1
#define NO_CLEAR	0

#define GET_STATUS	1	/* Used with monitor_status routine */
#define SET_STATUS	2	/* Used with monitor_status routine */

#define MORE		"---More---"

#define COL_PER_PAGE	78
#define LINES_PER_PAGE	18		/* Number of lines on screen */
#define MAX_LEN         256		/* Maximum length of line in file */

#define UNMONITOR	1	
#define	MONITOR		2

#define NOT_RUNNING_MSG_FORMAT  "%s Services not running."

extern 	int write_fld();
extern 	int read_fld();
extern	int Status_Resources(char *fileName, char *logMsg);
extern 	int  errno;

int 	GV_AppPid = 0;

static	void display_results();	
static	void get_shmem();

static FILE		*fp = NULL;
static void myLog(char *messageFormat, ...);

static  key_t   Shm_key;

static char	gMsg[512];

static	int	offset;
static	char	*Status[] = {"Init", /* 0 */
			 "Idle", /* 1 */
			 "Busy", /* 2 */
			 "Kill", /* 3 */
                         "Mont", /* 4 */
                         "Umon", /* 5 */
			 "Off ", /* 6 */
			 "POff", /* 7 */
			 "Susp", /* 8 */
			 "Cot", /* 9 */
			 "ROff"}; /* 10 */

static	long	Que_no;
static	int	cur_pg_no = 1;	/* Page that is currently displayed */
static	int	max_pg = 1;	/* Total Pages to be displayed */
static	int	no_reco;	/* # of records in the shared memory table */
static	int     cur_file_pg = 1; /* pg is currently displayed from log file */
static	int     Mem_ok;         /* Successful access to shared memory ?*/
static	int	tot_file_lines=0;/* total number of lines in monitor file */
static	int     tot_file_pg=1;	/* total pages in the monitor tran_proc file */
static	int	port_no;	/* port number assign to application */
static	int	max_len_line=80; /* maxium length of line on current screen */
static	int	cur_col_pos=0;  /* current column position for display page */
static	char	Monitor_dir[512];
static	char	menu_screen_title[80];
static  char column_headings[128];
static  char Host_name[128];
static  char ArcServiceName[64];
static  char ModuleName[]="isp_monitor";
static	int	MsgLine;	/* Line # on screen for messages */

static	char			outputFile[64]="";

extern 	int load_arc_custom_api(char *err_msg);
extern 	int	read_apiName(int api_id, char *apiName);


int main(int argc, char *argv[])
{
	int 	rc;
	char 	subtitle[80];
	char 	err_msg[256];
	char 	*ptr;
	struct stat 	yStat;
	char	triggerFile[128] = "/tmp/debugShm.trigger";

    struct timeval  tm;
    struct tm       tempTM;
	char			curTime[64];
	int				i;

    gettimeofday(&tm, NULL);
    (void *)localtime_r(&tm.tv_sec, &tempTM);
	(void)strftime(curTime, 32, "%m%d%y%H%M", &tempTM);
	sprintf(outputFile, "/tmp/debugShm.%s.out", curTime);

	if ( stat(triggerFile, &yStat) == 0 )
	{
		unlink(outputFile);
	}

	myLog("[%d] Starting.\n", __LINE__);
	i=0;
	while ( (rc = stat(triggerFile, &yStat)) < 0 )
	{
		usleep(500000);
		i++;
		if ( i % 600 == 0 ) 
		{
			myLog("[%d] Still checking for trigger file (%s)...\n",
				__LINE__, triggerFile);
			i=0;
		}
	}

	unlink(triggerFile);
	
	myLog("[%d] DING DING DING!!!  Got it.  Dumping shared memory...\n", __LINE__);
	display_results();

	if ( fp != NULL )
	{
		fclose(fp);
	}
	
}

/*-----------------------------------------------------------------------------
This routine displays the status of the processes
-----------------------------------------------------------------------------*/
void display_results()	
{
register int i, j;
int	no_lines, api_enum;		/* # LINES displayed on the screen */
char	Host_name[20];
char	Emu_num[20];
char	Env[20], api_name[50];
char	Log_tbl[51];
char	Tabl_name[100], pgm_name[100], *pgm_ptr;
char	Proc_no[20];		/* process number as in the schd_tabl */
char	Proc_stat[20];		/* status as in shared memory */
char	Proc_id[20];		/* process id as in shared memory */
char	Tran_type[20];		/* transaction type as in schd_tabl */
char	Sig[3];
void	*ptr;
char	apiName[30];

		Shm_key = SHMKEY_TEL;

	ptr = 0;
	get_shmem();

	if(! Mem_ok)
	{
		myLog("[%d] failed to get shared memory\n", __LINE__);
		return;
	}
	ptr = tran_tabl_ptr;
	fprintf(fp, "%s", ptr);
	no_reco = (int)strlen(tran_tabl_ptr)/SHMEM_REC_LENGTH;

	myLog("[%d] strlen(ptr)=%d,  record len=%d, number of records=%d\n",
		__LINE__, strlen(ptr),  SHMEM_REC_LENGTH, no_reco);
		
	no_lines = no_reco;

	ptr += (SHMEM_REC_LENGTH*offset);	
	/* adjust the pointer to the offset */

	for(i=offset+1, j=5; i<= no_lines; i++, j++)
	{
		myLog("[%d] DJB: len=%d (%.*s)\n", __LINE__, strlen(ptr), SHMEM_REC_LENGTH, ptr);
		ptr += SHMEM_REC_LENGTH;    /* move the pointer to record end */
	} /* for */

	ptr = tran_tabl_ptr;
	ptr += (SHMEM_REC_LENGTH*offset);	
	for(i=offset+1, j=5; i<= no_lines; i++, j++)
	{
		(void)sscanf(ptr, "%s%s%s%s%s%s%s%s%s%s\n", 
			Host_name, Emu_num, Proc_no, Env, Tran_type, Log_tbl, 
			Tabl_name, Proc_stat, Proc_id, Sig);
	
		myLog("[%d] Host_name(%s) Emu_num(%s) Proc_no(%s) Env(%s) Tran_type(%s) "
			"Log_tbl(%s) Tabl_name(%s) Proc_stat(%s) Proc_id(%s) Sig(%s)\n",  __LINE__,
			Host_name, Emu_num, Proc_no, Env, Tran_type, Log_tbl, 
			Tabl_name, Proc_stat, Proc_id, Sig);

		ptr += SHMEM_REC_LENGTH;    /* move the pointer to record end */
	} /* for */


	ptr = tran_tabl_ptr;
	ptr += (SHMEM_REC_LENGTH*offset);	
	for(i=offset+1, j=5; i<= no_lines; i++, j++)
	{
		memset (api_name, 0, sizeof(api_name));

		(void)sscanf(ptr, "%s%s%s%s%s%s%s%s%s%s\n", 
			Host_name, Emu_num, Proc_no, Env, Tran_type, Log_tbl, 
			Tabl_name, Proc_stat, Proc_id, Sig);
	
		switch(api_enum)
		{
		case 0:
			sprintf(api_name, "%s", "N/A                   ");
			break;
		case CUSTOM_API:
			sprintf(api_name, "%s", "CUSTOM");
			break;
		default:
			memset(apiName, 0, sizeof(apiName));
			read_apiName(api_enum, apiName);
			sprintf(api_name, "%s        ", apiName);
			break;
		}
	
		/* Get name of program to display */	
		memset (pgm_name, 0, sizeof(pgm_name));
		if (strcmp(Tabl_name, "N/A") != 0)
		{
			if ((pgm_ptr = strrchr(Tabl_name, '/')) != NULL)
			{
				pgm_ptr ++;
				sprintf(pgm_name, "%s", pgm_ptr);
			}
			else
				sprintf(pgm_name, "%s", Tabl_name);
			}
		else
			sprintf(pgm_name, "%s", Tabl_name);
	
		myLog("%-6d  ", i);
		myLog("%.19s  ", pgm_name);
		myLog("%.10s  ", Proc_no);
		myLog("%.8s  ", Status[atoi(Proc_stat)]);  // uncomment this
		myLog("%.7s  ", Proc_id);
		myLog("%.25s\n  ", api_name);
	
		ptr += SHMEM_REC_LENGTH;    /* move the pointer to record end */
	} /* for */

	if(Mem_ok)
	{
		(void)shmdt(tran_tabl_ptr);
	}
	return;
} /* of display_results() */

/*---------------------------------------------------------------------------
This routine returs the shared memory pointer
----------------------------------------------------------------------------*/
void get_shmem()
{

      	Mem_ok = 0;
	if((tran_shmid = shmget(Shm_key, SIZE_SCHD_MEM, 0))<0)
	{
		myLog("[%d] shmget failed. rc=%d [%d, %s]\n", __LINE__,
			tran_shmid, errno, strerror(errno));
				
 		return;
 	}
	tran_tabl_ptr = shmat(tran_shmid, 0, 0);


	if (tran_tabl_ptr == (void *)-1)
	{
		myLog("[%d] shmat failed.\n", __LINE__);
 		return;
 	}
	Mem_ok = 1; 
	myLog("[%d] tran_shmid=%d   Mem_ok=%d\n", __LINE__, tran_shmid, Mem_ok);
	return;
} 

static void myLog(char *messageFormat, ...)
{
  va_list     ap;
    char        message[2048];
    struct timeval  tm;
    struct tm       tempTM;
	char			str[64];
	char			curTime[64];
	static char		firstTime = 1;

    gettimeofday(&tm, NULL);
    (void *)localtime_r(&tm.tv_sec, &tempTM);
    (void)strftime(str, 32, "%m-%d %H:%M:%S", &tempTM);
    sprintf(curTime, "%s.%d", str, tm.tv_usec/1000);

    va_start(ap, messageFormat);
    vsprintf(message, messageFormat, ap);
    va_end(ap);


	
	if ( fp  == NULL )
	{
		if((fp = fopen(outputFile, "w") ) == NULL)
		{
			printf("%-12s|%-5d|%s",
				curTime, (int)getpid(), message);
		    fflush(stdout);
			return;
		}
		
	}
    fprintf(fp, "%s| %s", curTime, message);
	fflush(fp);
 
	return;

}

