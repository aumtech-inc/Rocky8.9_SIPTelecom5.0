/*-----------------------------------------------------------------------------
Program:	util_send_to_monitor.c
Purpose:	Contains send_to_monitor, shes_alive, & other routines
		that update shared memory.
		1. This function updates the keep alive flag in 
			shared memory for the application running.
		2. Write the number of the currently running api in 
			memory shared
		3. Writes a monitor record into the monitor file if the
		     application is being monitored.
Author: 	Mahesh Joshi.
Date:   	12/11/95
Update: 	08/27/97 mpb added call to see if file exists & append to it
Update:		11/23/98 gpw changed "APPENDING TO EXISTING FILE" to Warning; 
Update:		06/21/99 gpw rewrote. removed hardcodings, etc.
Update:		08/26/99 gpw removed commented code. No functional changes.
Update:		08/01/00 mpb Added STATUS_CHNOFF to check_diagnose routine.
Update:	2000/10/07 sja 4 Linux: Added function prototypes
Update:	2009/08/18 ddn Changed all strtok to strtok_r
-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <time.h>
#include <libgen.h>		/* mkdirp */
#include <unistd.h>		/* access() */
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "ISPSYS.h"
#include "shm_def.h"
#include "monitor.h"
#include "ispinc.h"
#include "loginc.h"
#include "COMmsg.h"
#include "TEL_Mnemonics.h"
#include "TCP_Mnemonics.h"
#include "SNA_Mnemonics.h"
#include "WSS_Mnemonics.h"
#include "COM_Mnemonics.h"

extern	LOGXXPRT();
extern	int	GV_shm_index;
extern	int	GV_HeartBeatInterval;
extern	int	GV_DiAgOn_;
extern	int GV_AppCallNum1;

#define HBEAT_SUCCESS	1
#define HBEAT_FAIL	0
#define ISP_DIAG_OFF	0
#define ISP_DIAG_ON	1

/*-----------------------------------------------
external variables 
------------------------------------------------*/
extern	int	errno;
extern	write_fld();
extern	read_fld();

static	char	ModuleName[] = "send_to_monitor";
static	int	process_id;
static	int	tran_shmid;
static	char	*tran_table_ptr;
static	int	tran_rec_num;
static	int	monitor_file_is_open;
static	FILE	*fp_monitor;
static	char	Msg[1024];
static	time_t	last_update = 0L;

/*
 * Static Function Prototypes
 */
static int get_shared_memory_key(char *mod, int api, key_t *shm_key);
static int open_monitor_file(int *file_exists);

static int createDirIfNotPresent(char *zDir)
{
        int rc;
        mode_t          mode;
        struct stat     statBuf;

        rc = access(zDir, F_OK);
        if(rc == 0)
        {
                /* Is it really directory */
                rc = stat(zDir, &statBuf);
                if (rc != 0)
                        return(-1);

                mode = statBuf.st_mode & S_IFMT;
                if ( mode != S_IFDIR)
                        return(-1);

                return(0);
        }

        rc = mkdir(zDir, 0755);
        return(rc);
}

static int mkdirp(char *zDir, int perms)
{
        char *pNextLevel;
        int rc;
        char lBuildDir[512];
		char	*yStrTok = NULL;

        pNextLevel = (char *)strtok_r(zDir, "/", &yStrTok);
        if(pNextLevel == NULL)
                return(-1);

        memset(lBuildDir, 0, sizeof(lBuildDir));
        if(zDir[0] == '/')
                sprintf(lBuildDir, "%s", "/");

        strcat(lBuildDir, pNextLevel);
        for(;;)
        {
                rc = createDirIfNotPresent(lBuildDir);
                if(rc != 0)
                        return(-1);

                if ((pNextLevel=strtok_r(NULL, "/", &yStrTok)) == NULL)
                {
                        break;
                }
                strcat(lBuildDir, "/");
                strcat(lBuildDir, pNextLevel);
        }

        return(0);
}




/*------------------------------------------------------------------------
int send_to_monitor():
--------------------------------------------------------------------------*/
int send_to_monitor(monitor_code, api, api_and_parms)
int	monitor_code;		
int	api;			/* api number */
char	*api_and_parms;		
{
	static	int	first_time = 1, busy_flag=0;
	static	key_t	shm_key;
	int 	rc;
	time_t	current_time;
	int 	new_status;
	
	if (first_time == 1)
	{
		rc=get_shared_memory_key(ModuleName, api, &shm_key);
		if ( rc == -1) return(HBEAT_FAIL);

		tran_rec_num = GV_shm_index;
		/* get shared memory queue identification */
		tran_shmid = shmget(shm_key, SIZE_SCHD_MEM, PERMS);
		if(tran_shmid < 0)
		{	
			sprintf(Msg, 
			"Failed to get shared memory segment id. errno=%d.", 
			errno);
			LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
			return(HBEAT_FAIL);
		}		

		tran_table_ptr = shmat(tran_shmid, 0, 0);

		/* attach to the schd_tran_tabl shared memory */
		if(tran_table_ptr == (char *) (-1))
		{
			sprintf(Msg, "shmat() failed. errno=%d", errno);
			LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
			return(HBEAT_FAIL);
		}

		process_id = getpid();
		tran_rec_num = GV_shm_index;
		first_time = 0;

		/* Prevent shared mem access for standalone apps. */
		if (tran_rec_num < 0) return(HBEAT_SUCCESS);

		if (GV_DiAgOn_ > 0)
		{
			/* if GV_DiAgOn_ we automatically monitor the app. */ 
			write_fld(tran_table_ptr, tran_rec_num, 
						APPL_STATUS, STATUS_MONT);
		}		
	} /* first time */

	/* Prevent shared mem access for standalone apps. */
	if (tran_rec_num < 0) return(HBEAT_SUCCESS);

	/* Update keep alive byte in shared memory */
	time(&current_time);
	if(current_time > (last_update + KEEP_ALIVE_UPDATE ) )
	{
		write_fld(tran_table_ptr, tran_rec_num, APPL_SIGNAL, 1);
		last_update = current_time;
	}

	if (monitor_code == MON_STATUS)		
	{
		new_status=api;
		/* Update monitor status */
		if (	new_status == STATUS_INIT || 
			new_status == STATUS_IDLE || 
			new_status == STATUS_MONT || 
			new_status == STATUS_BUSY || 
			new_status == STATUS_UMON)
		{
			/* Only update non-monitored apps. */
			if (check_diagnose() != 1) 
				write_fld(tran_table_ptr,
					tran_rec_num, APPL_STATUS, new_status);
		}
		return (0);
	}

	if (monitor_code == MON_API || monitor_code == MON_CUSTOM )	
	{
		write_fld(tran_table_ptr, tran_rec_num, APPL_API, api);
	}

	/* If the app is being monitored, write the monitor record. */
	if (check_diagnose() == 1)
	{
		write_monitor_record(monitor_code, api, api_and_parms);
		return (HBEAT_SUCCESS);
	}

	/* At this point we know the app is not being monitored */
	if (monitor_code == MON_API)
	{
		/* Set status to INIT for some api's */
		if (	api == TEL_INITTELECOM || 
			api == TCP_INIT || 
			api == SNA_INIT || 
			api == WSS_INIT)
			write_fld(tran_table_ptr, tran_rec_num, 
						APPL_STATUS, STATUS_INIT);
		
		/* Set status to INIT for TEL_AnswerCall */
		if (api == TEL_ANSWERCALL)
			write_fld(tran_table_ptr,tran_rec_num,
						APPL_STATUS, STATUS_IDLE);
		
		/* if busy flag is not set, set it if it should be */
		if (busy_flag == 0)
		{
			/* Set busy flag for some api's */
			if ( (GV_DiAgOn_ == 0) && 
			(	api == TCP_GETDATA || 
		 		api == SNA_GETDATA || 
				api == TEL_INITIATECALL) )
				write_fld(tran_table_ptr, tran_rec_num, 
				APPL_STATUS, STATUS_BUSY);
			busy_flag = 1;
		}
	}
	return (HBEAT_SUCCESS);
} /* send_to_monitor */

/*--------------------------------------------------------------------------
This routine checks if the application is in diagnose mode. 
----------------------------------------------------------------------------*/
int check_diagnose()
{
	static	int log_diag_mode=0;		/* diagnose mode off */
	char	tran_pid[8], tran_stat[3];
	char	status[10];

	tran_pid[0] = '\0';
	tran_stat[0] = '\0';

	/* get the status of the application */
	read_fld(tran_table_ptr, tran_rec_num, APPL_STATUS, tran_stat);

	if((atoi(tran_stat) == STATUS_UMON) && (monitor_file_is_open))	
	{	
		/* No longer in diagnose mode */
		fclose(fp_monitor);
		monitor_file_is_open = 0;
		read_fld(tran_table_ptr, tran_rec_num, APPL_STATUS, status);
		if (atoi(status) != STATUS_MONT)
			write_fld(tran_table_ptr, tran_rec_num, 
				APPL_STATUS, STATUS_BUSY);
		return(0);
	}

	if(atoi(tran_stat) == STATUS_MONT)	
	{
		/* Get the application's pid */
		read_fld(tran_table_ptr, tran_rec_num, APPL_PID, tran_pid);
		if (log_diag_mode != 1) set_diagnose(ISP_DIAG_ON);
 		if(atoi(tran_pid) == process_id)		
 			return (1);		/* ON */
 		else
 			return (0);		/* OFF */
 	}				
	if(atoi(tran_stat) == STATUS_CHNOFF)	
	{
		return(1);
	}
	else
	{
		/* if diagnose mode is on set diagnose mode off */
		if (log_diag_mode != 0) set_diagnose(ISP_DIAG_OFF);
		return(0);
	}
} /* check_diagnose() */

/*--------------------------------------------------------------------------
This routine writes the message header and message line to monitor file.
--------------------------------------------------------------------------*/
int write_monitor_record(monitor_code, api_no, data)
int	monitor_code;
int	api_no;
char	*data;
{
	char	header[64];
	int	rc;
	int	monitor_file_already_exists=0;
	static char append_warning[]=
	"Warning: Appending information to a previously existing monitor file.";

	if ( ! monitor_file_is_open )
	{
		rc=open_monitor_file(&monitor_file_already_exists);
		if (rc == -1 ) return(-1);
		if (monitor_file_already_exists)
		{
			fprintf(fp_monitor, "\n%s\n\n", append_warning);
			fflush(fp_monitor);
		}
	}

	header[0] = '\0';
	switch(monitor_code)
	{
	case	MON_API:
		sprintf(header, "%s", "");
		break;
	case	MON_DATA:
		sprintf(header, "%s", "\t\t");/* indent for data */
		break;
	case	MON_ERROR:
		sprintf(header, "%s :", "ERROR");	/* error */
		break;
	case	MON_CUSTOM:
		/* custom event */
		sprintf(header, "%s : %d:", "CUSTOM", api_no);	
		break;
	default :
		sprintf(header, "%s", "");		/* don't know */
		break;
	} /* switch */

	fprintf(fp_monitor, "%s%s\n", header, data); 
	fflush(fp_monitor);
	return(0);
} /* write_monitor_record */

/*----------------------------------------------------------------------------
This routine writes a keep alive byte based on the port number passed. It is 
only used by Telecom Services & thus has SHMKEY_TEL hard coded.;
----------------------------------------------------------------------------*/
int	she_is_alive(port)
int	port;
{
	char	*ptr1;
	int	shmem_id, rc;
	char	ModuleName[] = "she_is_alive";

	/* get shared memory queue identification */
	shmem_id = shmget(SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);
	if( shmem_id < 0)
	{	
		sprintf(Msg, 
		"Failed to get shared memory segment id. errno=%d.", errno);
		LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
		return(HBEAT_FAIL);
	}		

	/* attach to the shared memory */
	ptr1 = shmat(shmem_id, 0, 0);
	if (ptr1 == (char *) (-1))
	{
		sprintf(Msg, 
		"shmat() failed while attaching to shmid=%d. errno=%d", 
		errno,shmem_id);
		LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
		return(HBEAT_FAIL);
	}

	/* Write the keep alive byte */
	write_fld(ptr1, port, APPL_SIGNAL, 1);

	/* detach from shared memory */
	rc = shmdt(ptr1);
	if (rc != 0)
	{
		sprintf(Msg, 
		"shmat() failed while detaching. error=%d.",errno);
		LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
		return(HBEAT_FAIL);
	}
	return(HBEAT_SUCCESS);
}

/*----------------------------------------------------------------------------
This routine determines the shared memory key based on the set of apis to 
which the passed api belongs. 
----------------------------------------------------------------------------*/
static int get_shared_memory_key(char *mod, int api, key_t *shm_key)
{
	int 	base;

	/* determine the base of the api, e.g. if api=1050, base=1000 */
	base = ((api/1000) * 1000);
/*
	fprintf(stdout,"Value of api is %d, value of base is %d\n", api,base);
	fprintf(stdout,"Value of TEL_API_BASE is %d\n", TEL_API_BASE);
	fflush(stdout);
*/
	switch(base)
	{
	case	TEL_API_BASE:
		*shm_key = SHMKEY_TEL;
		break;
	case	TCP_API_BASE:
		*shm_key = SHMKEY_TCP;
		break;
	case	SNA_API_BASE:	
		*shm_key = SHMKEY_SNA;
		break;
	case	WSS_API_BASE:	
		*shm_key = SHMKEY_WSS;
		break;
	default :
		sprintf(Msg,
		"Failed to determine shared memory key for api=%d.", api);
		LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
		return(HBEAT_FAIL);
		break;
	} 
	return(0);
}

/*----------------------------------------------------------------------------
set_diagnose();
-----------------------------------------------------------------------------*/
int	set_diagnose(mode)
int	mode;
{
	switch (mode)
	{
	case	ISP_DIAG_ON:
		break;
	case	ISP_DIAG_OFF:
		break;
	default:
		return (0);
	} /* switch */
}

/*----------------------------------------------------------------------------
This routine opens the monitor file, creating the directory if necessary and
reports whether the file existed previously.
----------------------------------------------------------------------------*/
static int open_monitor_file(int *file_exists)
{
	char	file_name[512];
	char	monitor_dir[512];
	char	*ptr;

	if ((ptr = (char *)getenv("ISPBASE")) == NULL)
	{
		sprintf(Msg,"Environment variable ISPBASE is not set. errno=%d",
			errno);
		LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
		return (-1);
	}

	sprintf(monitor_dir, "%s/MONITOR", ptr);
	/* if the monitor directory does not exist create it */
	if (access(monitor_dir, W_OK) != 0) 
	{
		if (mkdirp(monitor_dir, 0755) == -1)
		{
			sprintf(Msg, "Can't create directory <%s>. errno=%d", 
				monitor_dir, errno);
			LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
			return (-1);
		}
	}

	/* Create the monitor file name */
	printf("DJB|%ld|%s|%d|GV_AppCallNum1=%d\n",
			getpid(), __FILE__, __LINE__, GV_AppCallNum1);
	fflush(stdout);

	sprintf(file_name,"%s/%s.%-d.%d",monitor_dir,"monitor",process_id,
				GV_AppCallNum1);

	if(access(file_name, F_OK) == 0)
	{
		if((fp_monitor = fopen(file_name, "a+")) == NULL)
		{
 			sprintf(Msg, "Unable to open <%s> for append. errno=%d",
				file_name, errno);
			LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
			return(-1);
 		}
		*file_exists;
	}
	else
	{
		if((fp_monitor = fopen(file_name, "w")) == NULL)
		{
 			sprintf(Msg,"Unable to open <%s> for write. errno=%d.", 
				file_name, errno);
			LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, Msg);
 			return(-1);
		}
	}
	monitor_file_is_open = 1;
}

