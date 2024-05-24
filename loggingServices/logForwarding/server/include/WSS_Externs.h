/*


Update: 	04/02/97 sja	Created this file
Update: 	05/30/97 sja	Added GV_machinename & GV_clientpid
Update: 	06/02/97 mpb	Added GV_IndexDelimit 
Update: 	06/03/97 sja	Removed GV_IndexDelimit: It's in the IndxAry API
Update: 	06/05/97 sja	Removed GV_SysDelimiter
Update:	06/11/98 mpb	Changed GV_Connected to GV_SsConnected.

*/

#ifndef WSS_Externs_H
#define WSS_Externs_H

/* GetGlobal Variables */
extern int	GV_SysTimeout;

/* GetGlobalString Variables. */
extern int	GV_tfd;
extern char	GV_CDR_Time[];
extern char	GV_WSS_ANI[];
extern char	GV_WSS_DNIS[];
extern int	GV_StaticDynamic ;
extern char	GV_WSS_PortName[];

extern int     GV_shm_index ;
extern int     GV_DiAgOn_ ;
extern int	GV_Disconnect ;	
extern char	GV_InitTime[];
extern char	GV_ConnectTime[];
extern char	GV_DisconnectTime[];
extern char	GV_ExitTime[];
extern long	GV_InitTimeSec;	
extern long	GV_ConnectTimeSec;
extern long	GV_DisconnectTimeSec;
extern long	GV_ExitTimeSec;
extern short	GV_ShutdownDone;

/* Heatbeat Interval */
extern int	GV_HeartBeatInterval ;
extern int	GV_HeartBeatDefault ;

extern char	GV_AccountCode[];

extern char	GV_ClientHost[];
extern int	GV_ClientPid;

/* Call Detail Recording Globals */
extern char	GV_CDR_AppName[];
extern char	GV_CDR_Originator[];
extern char	GV_CDR_Destination[];
extern char	LAST_API[];

/* Miscellaneous Vars */
extern int	GV_WSS_PortNumber;
extern char	__port_name[];
extern short	GV_SsConnected;

extern char 	*ISP_Parm[];

#endif /* WSS_Externs_H */
