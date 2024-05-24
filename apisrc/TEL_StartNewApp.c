static char mod[]="TEL_StartNewApp";
/*-----------------------------------------------------------------------------
Program:	TEL_StartNewApp.c
Purpose:	Start a new application
Author:		Madhav Bhide, Sandeep Agate
Date:		03/18/97
Update:		03/08/01 gpw updated for IP
Update:		05/09/01 apj Added code to support SCHEDULED_DNIS_AND_APP and
			     SCHEDULED_DNIS_ONLY options.
Update: 08/25/04 djb Changed sys_errlist strerror(errno).
-----------------------------------------------------------------------------
Copyright (c) 2001, Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "COMmsg.h"


#define MAX_APPL_NAME	50	/* from $HOME/isp2.2/Common/include/resp.h */

extern int write_fld(char *ptr, int rec, int ind, int stor_arry);

static int updateShmem(char *oldApp, char *newApp);
static int getAppFromResp(char *dnis,char *newApp,
			char *appToFire,char *appDir, int appType);
static int getAppDir(char *appPath, char *app, char *appDir);
static int changeDir(char *zDirName);
static int turnkey_write_fld(int field, int value);

/*-----------------------------------------------------------------------------
TEL_StartNewApp(): Start a new application on the current port
Note:	The ani parameter is currently ignored and is reserved for future use.
-----------------------------------------------------------------------------*/
int TEL_StartNewApp(int appType, char *newApp, char *dnis, char *ani)
{
int	rc;
char 	apiAndParms[MAX_LENGTH] = "";
char	applicationPath[256];
char	pwd[256];
char	old_app_pwd[256];
char	appToFire[80];
char	appDir[256];
char	ExecDir[100];
char 	tmpAppCallNum[5];
char 	tmpAppPassword[5];
char 	tmpDynMgrId[5];

sprintf(apiAndParms, "TEL_StartNewApp(%s,%s,%s,%s)",
			arc_val_tostr(appType),newApp,dnis, "future_use");
rc = apiInit(mod, TEL_STARTNEWAPP, apiAndParms, 1, 0, 0);
if (rc != 0) HANDLE_RETURN(rc);

if(appType != SCHEDULED_DNIS_AND_APP && appType != STANDALONE_APP && appType != SCHEDULED_DNIS_ONLY)
	{
	telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"Invalid appType parameter: %d. Valid values: %s.", appType,
		"SCHEDULED_DNIS_AND_APP,STANDALONE_APP or SCHEDULED_DNIS_ONLY");
	HANDLE_RETURN(-1);
	}
if(appType != SCHEDULED_DNIS_ONLY)
	{
	if(newApp == (char *)NULL)
		{
		telVarLog(mod, REPORT_NORMAL, TEL_E_NULL_APP, GV_Err,
		"NULL application name passed.");
		HANDLE_RETURN(-1);
		}
	}
memset(appToFire, 0, sizeof(appToFire));
memset(appDir, 0, sizeof(appDir));

if (getcwd(old_app_pwd, sizeof(old_app_pwd)) == NULL)
	{
	telVarLog(mod, REPORT_NORMAL, TEL_E_CUR_DIR, GV_Err,
		TEL_E_CUR_DIR_MSG, errno, strerror(errno));
	HANDLE_RETURN(-1);
	}

	sprintf(ExecDir, "%s/Telecom/Exec", GV_ISPBASE);
	if(chdir(ExecDir) == -1)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_E_CHDIR, GV_Err,
			TEL_E_CHDIR_MSG, ExecDir, errno, strerror(errno));
		HANDLE_RETURN(-1);
	}

	switch(appType)
	{
	case SCHEDULED_DNIS_AND_APP:
	case SCHEDULED_DNIS_ONLY:
		if(dnis == (char *)NULL)
			{
			telVarLog(mod, REPORT_NORMAL, TEL_E_NULL_DNIS, GV_Err,
				TEL_E_NULL_DNIS);
			changeDir(old_app_pwd);
			HANDLE_RETURN(-1);
			}
		if(getAppFromResp(dnis, newApp, appToFire, appDir, appType) < 0)
			{
			changeDir(old_app_pwd);
			HANDLE_RETURN(-1);	/* msg. written in subroutine */
			}
		if(chdir(appDir) == -1)
			{
			telVarLog(mod, REPORT_NORMAL, TEL_E_CUR_DIR, GV_Err,
				TEL_E_CHDIR_MSG,appDir,errno,strerror(errno));
			changeDir(old_app_pwd);
			HANDLE_RETURN(-1);
			}
		if(access(appToFire, F_OK|R_OK|X_OK) == -1)
			{
			telVarLog(mod, REPORT_NORMAL, TEL_E_APP_ACCESS, GV_Err,
				TEL_E_APP_ACCESS, appToFire,
				errno, strerror(errno));
			changeDir(old_app_pwd);
			HANDLE_RETURN(-1);
			}
		if(updateShmem(GV_AppName, appToFire) < 0)
			{
			changeDir(old_app_pwd);
			HANDLE_RETURN(-1);     	/* msg. written in subroutine */
			}
		break;

	case STANDALONE_APP:
		if(access(newApp, F_OK|R_OK|X_OK) == -1)
			{
			telVarLog(mod, REPORT_NORMAL, TEL_E_APP_ACCESS, GV_Err,
				TEL_E_APP_ACCESS, newApp,
				errno, strerror(errno));
			changeDir(old_app_pwd);
			HANDLE_RETURN(-1);
			}
		getAppDir(newApp, appToFire, appDir);
		if(chdir(appDir) == -1)
			{
			telVarLog(mod, REPORT_NORMAL, TEL_E_CHDIR, GV_Err,
				TEL_E_CHDIR_MSG,appDir,errno,strerror(errno));
			changeDir(old_app_pwd);
			HANDLE_RETURN(-1);
			}
		break;

	default:
		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"Invalid appType parameter: %d. Valid values: %s.", appType,
		"SCHEDULED_DNIS_AND_APP,STANDALONE_APP or SCHEDULED_DNIS_ONLY");
		changeDir(old_app_pwd);
		HANDLE_RETURN(-1);
	} /* switch */

	if (getcwd(pwd, sizeof(pwd)) == NULL)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_E_CUR_DIR, GV_Err,
			TEL_E_CUR_DIR_MSG, errno, strerror(errno));
		changeDir(old_app_pwd);
		HANDLE_RETURN(-1);
	}

	sprintf(applicationPath, "%s/%s", pwd,appToFire);

/* TEL_ExitTelecom stuff STARTS here ... */

// Note: dx_close is not needed, but do we need an IP equivalent? gpw 3/8/01
//dx_close(__voice_handle);
	lm_exit();

/* TEL_ExitTelecom stuff ENDS here ... */

	telVarLog(mod, REPORT_NORMAL, TEL_START_NEW_APP, GV_Info, 
				TEL_START_NEW_APP_MSG,appToFire);

/* ------------------------------------------------------------
Turnkey processing: Set status to idle, field 5 to "kill me".
Must set status to idle first so I don't get killed.
 ------------------------------------------------------------*/
	turnkey_write_fld(APPL_STATUS, STATUS_IDLE);
	turnkey_write_fld(APPL_FIELD5, 1);	

	sprintf(tmpAppCallNum, "%d", GV_AppCallNum1);
	sprintf(tmpAppPassword, "%d", GV_AppPassword);
	sprintf(tmpDynMgrId, "%d", GV_DynMgrId);

	if(execl(applicationPath, appToFire,
                                "-p",
                                tmpAppCallNum,
                                "-z",
                                tmpAppPassword,
								"-U", 
								GV_AppToAppInfo,
                                "-D",
                                GV_DNIS,
                                "-A",
                                GV_ANI,
								"-d",
								tmpDynMgrId,
								"-O",
								"1",
                                (char *) 0) != 0)
	{

		/* ------------------------------------------------------------
		Turnkey processing: Restore normal values, we failed to spawn.
		Must set to "don't kill me" first so I don't get killed.
 		------------------------------------------------------------*/
		turnkey_write_fld(APPL_FIELD5, 0);
		turnkey_write_fld(APPL_STATUS, STATUS_BUSY);
		telVarLog(mod, REPORT_NORMAL, TEL_E_EXEC, GV_Err,
			TEL_E_EXEC_MSG, appToFire,errno,strerror(errno));
		changeDir(old_app_pwd);
		HANDLE_RETURN(-1);
	}

	HANDLE_RETURN(0);
} /* END: TEL_StartNewApp() */
/*-----------------------------------------------------------------------------
getAppDir(): Get the app name and its dir.
-----------------------------------------------------------------------------*/
static int getAppDir(char *appPath, char *app, char *appDir)
{
int	dir_index;
char	*ptr;

/* set directory and program name */
if (strchr(appPath, '/') == NULL)
	{
	sprintf(app, "%s", appPath);
	sprintf(appDir, "%s", ".");
	}
else
	{
	/* parse program name and directory name */
	sprintf(appDir, "%s", appPath);
	dir_index = strlen(appDir) -1;
	for(;dir_index>=0; dir_index--)
       		{
       		if (appDir[dir_index] == '/')
       			{
       			appDir[dir_index] = '\0';
       			break;
              		}
       		}
	ptr = (char *) strrchr(appPath, '/');
	if (ptr)
       		ptr++;
	sprintf(app, "%s", ptr);
	}
return(0);
} /* END: getAppDir() */
/*-----------------------------------------------------------------------------
getAppFromResp(): Get the app. from resp. based on DNIS
-----------------------------------------------------------------------------*/
static int getAppFromResp(char *dnis,char *newApp,char *appToFire, char *appDir, int appType)
{
char	appPath[256];
char	app_for_msg[128];
int	flag = 0;
struct MsgToRes lMsg;
int 	toRespFifoFd;
struct MsgToApp response;
int 	lGotResponseFromResp = 0;
int 	rc;

lMsg.opcode 	= RESOP_GET_APP_TO_OVERLAY;
lMsg.appPid 	= GV_AppPid;
lMsg.appPassword= GV_AppPassword;
lMsg.appCallNum = GV_AppCallNum1;
lMsg.appRef 	= GV_AppRef++;
sprintf(lMsg.dnis, "%s", dnis);
sprintf(lMsg.ani, "%s", GV_ANI);
memset(lMsg.data, '\0', sizeof(lMsg.data));

if ((mkfifo(GV_ResFifo, S_IFIFO | 0666) < 0) && (errno != EEXIST))
{
	telVarLog(mod, REPORT_NORMAL, TEL_CANT_CREATE_FIFO, GV_Err,
		TEL_CANT_CREATE_FIFO_MSG,GV_ResFifo, errno, strerror(errno));
       	return(-1);
}

if ((toRespFifoFd = open(GV_ResFifo, O_RDWR)) < 0)
{
	telVarLog(mod, REPORT_NORMAL, TEL_CANT_OPEN_FIFO, GV_Err,
		TEL_CANT_OPEN_FIFO_MSG, GV_ResponseFifo,
		errno, strerror(errno));
	return(-1);
}                 

rc = write(toRespFifoFd, (char *)&lMsg, sizeof(struct MsgToRes));
if(rc == -1)
{
	telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err,
        	"Can't write message to <%s>. errno=%d", GV_ResFifo, errno);
	close(toRespFifoFd);
       	return(-1);
}

telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
        	"Sent %d bytes to <%s>. msg={%d,%d,%d,%d,%d,%s,%s,%s}",
		rc, GV_ResFifo, lMsg.opcode, lMsg.appCallNum, 
		lMsg.appPid, lMsg.appRef, lMsg.appPassword, lMsg.dnis, 
		lMsg.ani,lMsg.data);

close(toRespFifoFd);

while(lGotResponseFromResp == 0)
{
	// The readResponseFromDynMgr name is misleading. Here it reads the
	// the response sent by Responsibility to the App's response fifo.
	if(readResponseFromDynMgr(mod,0,&response,sizeof(response)) == -1)
		return(-1);

	switch(response.opcode)
	{
		case RESOP_GET_APP_TO_OVERLAY:
			lGotResponseFromResp = 1;
			sprintf(appPath, "%s", response.message);
			break;

		default:
			continue;
			break;
	} 
}

if (response.returnCode != 0)
{
	switch(appType)
	{
		case SCHEDULED_DNIS_AND_APP:
			sprintf(app_for_msg,"%s,%s",newApp, dnis);
			break;
		case SCHEDULED_DNIS_ONLY:
			strcpy(app_for_msg,dnis);
			break;
		case STANDALONE_APP:
			strcpy(app_for_msg,newApp);
			break;
	}
	telVarLog(mod, REPORT_NORMAL, TEL_E_APPNOTSCHED, GV_Err,
			TEL_E_APPNOTSCHED_MSG, app_for_msg);
	return(-1);
}

getAppDir(appPath, appToFire, appDir);

if(appType == SCHEDULED_DNIS_AND_APP)
{
	if(strcmp(newApp, appToFire) != 0)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_E_APPNOTSCHED, GV_Err,
			TEL_E_APPNOTSCHED_MSG, newApp);
		return(-1);
	}
}

return(0);
} /* END: getAppFromResp() */
/*-----------------------------------------------------------------------------
updateShmem(): Update shared memory with the new app. name
-----------------------------------------------------------------------------*/
static int updateShmem(char *oldApp, char *newApp)
{
key_t	tran_shmid;
char	*tran_tabl_ptr, *ptr, *ptr1;
int	tran_proc_num, i;
char	tmpProg[128];

tran_shmid = shmget(SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);

if ((tran_tabl_ptr = shmat(tran_shmid, 0, 0)) == (char *) -1)
	{
	telVarLog(mod, REPORT_NORMAL, TEL_E_SHMAT, GV_Err,
			TEL_E_SHMAT_MSG, tran_shmid, errno,strerror(errno));
	return(-1);
	}

tran_proc_num = strlen(tran_tabl_ptr)/SHMEM_REC_LENGTH;


ptr = tran_tabl_ptr;
ptr += (GV_shm_index*SHMEM_REC_LENGTH);

ptr += vec[APPL_NAME-1];	/* application start index */
ptr1 = ptr;
(void) memset(ptr1, ' ', MAX_APPL_NAME);
ptr += (MAX_APPL_NAME - strlen(newApp));
(void) memcpy(ptr, newApp, strlen(newApp));

shmdt(tran_tabl_ptr);		/* detach the shared memory segment */
return(0);
} /* END: updateShmem() */

/*-----------------------------------------------------------------------------
Write values to shared memory fields required for turnkey handshaking
-----------------------------------------------------------------------------*/
static int turnkey_write_fld(int field, int value)
{
	key_t	tran_shmid;
	char	*tran_tabl_ptr;

	tran_shmid = shmget(SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);

	if ((tran_tabl_ptr = shmat(tran_shmid, 0, 0)) == (char *) -1)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_E_SHMAT, GV_Err,
			TEL_E_SHMAT_MSG, tran_shmid, errno,strerror(errno));
		return(-1);
	}

	write_fld(tran_tabl_ptr, GV_shm_index, field, value);

	/* detach the shared memory segment */
	shmdt(tran_tabl_ptr);
	return(0);
} 

static int changeDir(char *directory)
{
	if(chdir(directory) == -1)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_E_CHDIR, GV_Err,
			TEL_E_CHDIR_MSG,directory,errno,strerror(errno));
		return(-1);
	}
	return(0);
}
