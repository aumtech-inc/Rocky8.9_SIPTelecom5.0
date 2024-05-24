/*


Update: 	03/27/97 sja	Added this header
Update: 	04/02/97 sja	Added ISP_Parm[]
Update: 	05/30/97 sja	Added GV_machinename & GV_clientpid
Update: 	06/02/97 mpb	Added GV_IndexDelimit 
Update: 	06/03/97 sja	Removed GV_IndexDelimit: It's in the IndxAry API
Update: 	06/05/97 sja	Removed GV_SysDelimiter
Update:	06/11/98 mpb	Changed GV_Connected to GV_SsConnected.

*/

#ifndef WSS_Globals_H
#define WSS_Globals_H

/* GetGlobal Variables */
int	GV_SysTimeout;

/* GetGlobalString Variables. */
int	GV_tfd;
char	GV_CDR_Time[15];
char	GV_WSS_ANI[16];
char	GV_WSS_DNIS[16];
int	GV_StaticDynamic ;
char	GV_WSS_PortName[20];

int     GV_shm_index ;
int     GV_DiAgOn_ ;
int	GV_Disconnect ;	
char	GV_InitTime[15];	/* Time when ISP Init routine is called. */
char	GV_ConnectTime[15];	/* Time when call is answered. */
char	GV_DisconnectTime[15];	/* Time when call disconnect is detected. */
char	GV_ExitTime[15];	/* Time when ISP exit routine is called. */

long	GV_InitTimeSec;		/* Time when ISP Init routine is called. */
long	GV_ConnectTimeSec;	/* Time when call is answered. */
long	GV_DisconnectTimeSec;	/* Time when call disconnect is detected. */
long	GV_ExitTimeSec;		/* Time when ISP exit routine is called. */

short	GV_ShutdownDone;

/* Heatbeat Interval */
int	GV_HeartBeatInterval ;
int	GV_HeartBeatDefault ;

char	GV_AccountCode[51];

char	GV_ClientHost[80];
int	GV_ClientPid;

/* Call Detail Recording Globals */
char	GV_CDR_AppName[51];
char	GV_CDR_Originator[16];
char	GV_CDR_Destination[16];
char	LAST_API[64];

/* Miscellaneous Vars */
int	GV_WSS_PortNumber;		/* Application access (read only). */
char	__port_name[20];		/* Index in shared memory */
short	GV_SsConnected;			/* Are we connected ? */

char 	*ISP_Parm[MAX_ISP_PARM] = 
	{
		"Unknown",		/* 0 */
		"RF_APPEND",
		"RF_OVERWRITE",
		"RF_PROTECT",
		""
	};
#endif /* WSS_Globals_H */
