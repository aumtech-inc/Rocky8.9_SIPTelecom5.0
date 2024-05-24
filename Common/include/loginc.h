/*-----------------------------------------------------------------------------
Program:	loginc.h
Purpose:	ISP common logger constant definition, this file provides
		the public interface to the ISP common logging utility
Author:		Jenny Huang
Date:		03/24/94
Update:	 	07/22/94 J. Huang increase maximum message size
Update:		07/28/94 J. Huang Log_block_int, Log_block_timeout;
Update:		12/05/95 G. Wenzel moved reporting modes from ispinc.h
Update:		03/20/96 G. Wenzel added comments
Update:		03/20/96 G. Wenzel changed MAX_FILENAME to MAX_CONFIG_FILENAME
Update:		09/05/98 mpb changed  GV_CDR_Key to 50
Update:		09/14/99 gpw updated comments, copyright, removed commented code
Update:		03/05/00 apj added define for REPORT_DETAIL.
Update:		04/16/00 mpb added GV_DateFormat.
Update:		05/20/03 mpb Changed MAX_SRVNAME from 20 to 32.
-------------------------------------------------------------------------------
Copyright (c) 1999, Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech ISP which is protected under
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/

#ifndef _ISP_LOGINC_H		/* avoid multiple include problems */
#define _ISP_LOGINC_H

/*----------------------------------------------------------------------------*/
/*  Structure for communicating with the LOGXXCTL function */

/*  Structure for loading message sets, i.e., the formats for each message */
/* For example, there is a set of messages for TEL, SNA, TCP, LOG, COM, etc.
Message sets are defined in TELmsg.c, TELmsg.h, SNAmsg,c, SNAmsg.h, etc */

typedef struct	MsgSet {
	int	msgbase;
	int	version;
	int	max_defined;
	char	**msglist;
	char	prefix[4];
	char	object[9];
} MsgSet_t;


typedef struct LOG_request {
	Int32		CommandId;
        Int32		StatusCode;
	Char *		PT_Object;
	Char *		PT_UserData;
	Char *		PT_LogOpt;
	MsgSet_t *	PT_MsgSet;
} LOG_ReqParms_t;

/* commands passed to LOGXXCTL in the CommandId field of LOG_ReqParms_t */

#define ISP_LOGCMD_INIT		800 	/* Initialize iVIEW logging */
#define ISP_LOGCMD_SHUTDOWN	801	/* Shutdown iVIEW logging */
#define ISP_LOGCMD_LOADMSG	802	/* Load Log Message sets */
#define ISP_LOGCMD_SETCONF	803	/* Read Config, set GV_LogConfig vals */
#define ISP_LOGCMD_TAGMSG	804	/* Set var used as iVIEW TransactionId*/
#define ISP_LOGCMD_SETMODEON	805	/* Turn a reporting mode on */
#define ISP_LOGCMD_SETMODEOFF	806	/* Turn a reporting mode off */

/* Codes that can be returned in the StatusCode field of LOG_ReqParms_t */

#define	LOG_FAILED		1
#define	LOG_EINVALID_MODE	2
#define	LOG_EINVALID_ID		3
#define LOG_ERELOAD		4
#define LOG_ETOOMANY_MSGSETS	5
#define LOG_EINVALID_LOGCMD	6
#define LOG_EINIT_FAILED	7
#define LOG_ELOAD_CONFIG	8
#define LOG_ESET_OPTION		9

/*  iVIEW Log related definitions  - See iVIEW Programmer's Referece */

#define MAX_MSGSETS	10 	/* max # message set, i.e., TEL, COM,..*/
#define MAXMSGLEN	512	/* Len of user defined part of iVIEW msg Body */
#define MAXMSGBODY	720	/* Length of iVIEW Body field in msg header */
#define LOG_ID_LEN	3	/* Length of iVIEW LogId field in msg header */
#define MAX_APPID_LEN	20	/* Length of iVIEW TransactionId field in hdr */

/* iVIEW message "LogId" values - 1st field in every iVIEW log message header */
#define CALL_DETAIL_LOG_ID	"CDR"
#define CALL_EVENT_LOG_ID	"CEV"
#define DIAGNOSE_LOG_ID		"DIA"


/* Defaults for GV_LogConfig Parameters , which are set in LOGglb.c */
#define DEFAULT_LOG_ID		"ISP"		/* Default LogID */
#define	DEFAULT_ISP_LOG		"/tmp/ISP" 	/* Default Log_x_local_file */ 
#define DEFAULT_RETENTION	7		/* Default Log_Retention */

#define MAX_SRVNAME		32
#define MAX_FILENAME		80

typedef struct {
	Bool	CentralMgr;
	char	LogID[4];
	Int32	ReportingMode;
	Bool	Log_x_remote;
	char	Log_PrimarySrv[MAX_SRVNAME];
	char	Log_SecondarySrv[MAX_SRVNAME];
	Bool	Log_x_stderr;
	Bool	Log_x_stdout;
	Bool	Log_x_local;
	char	Log_x_local_file[MAX_FILENAME];
	int	Log_Retention;
	int	Log_block_int;
	int	Log_block_timeout;
} LogAttr_t;


/*-----------------------------------------------------------------------------
Report modes
-----------------------------------------------------------------------------*/
#ifndef REPORT_NORMAL
#define REPORT_NORMAL  	( 1 << 0 )  	/* 1 */
#endif
#ifndef REPORT_VERBOSE
#define REPORT_VERBOSE  ( 1 << 1 )	/* 2 */	
#endif
#define REPORT_DEBUG    ( 1 << 2 )	/* 4 */
#define REPORT_SPECIAL  ( 1 << 3 )	/* 8 */
#ifndef REPORT_CDR
#define REPORT_CDR      ( 1 << 4 )	/* 16 */
#endif
#ifndef REPORT_CEV
#define REPORT_CEV      ( 1 << 5 )	/* 32 */
#endif
#ifndef REPORT_DIAGNOSE
#define REPORT_DIAGNOSE ( 1 << 6 )	/* 64 */
#endif
#ifndef REPORT_DETAIL
#define REPORT_DETAIL 	( 1 << 7 )	/* 128 */
#endif
#ifndef REPORT_EVENT
#define REPORT_EVENT 	( 1 << 8 )	/* 256 */ // BT-2
#endif
#define REPORT_STDWRITE 0

#define PRINTF_MSG     1  /* MessageId passed to LOGXXPRT for REPORT_STDWRITE */

/* Macros to check for a reporting mode. For example,
if(ReportNormal_ON) will be true if LOG_REPORT_NORMAL=ON, false otherwise. */

#define ReportNormal_ON		(GV_LogConfig.ReportingMode&REPORT_NORMAL)
#define ReportVerbose_ON	(GV_LogConfig.ReportingMode&REPORT_VERBOSE)
#define ReportDebug_ON		(GV_LogConfig.ReportingMode&REPORT_DEBUG)
#define ReportSpecial_ON	(GV_LogConfig.ReportingMode&REPORT_SPECIAL)
#define ReportCDR_ON		(GV_LogConfig.ReportingMode&REPORT_CDR)
#define ReportCEV_ON		(GV_LogConfig.ReportingMode&REPORT_CEV)
#define ReportDiagnose_ON	(GV_LogConfig.ReportingMode&REPORT_DIAGNOSE)

/*---------------------------------------------------------------------------*/

/* Definition of Call Detail Recording Key */
extern char GV_CDR_Key[50];
extern char GV_CDR_ServTranName[27];
extern char GV_CDR_CustData1[9];
extern char GV_CDR_CustData2[101];

/* 'extern' declarations */

extern int      GV_COM_msgnum;
extern char *   GV_COMmsg[];
extern int      GV_LOG_msgnum;
extern char *   GV_LOGmsg[];
extern int      GV_CDR_msgnum;
extern char *   GV_CDRmsg[];
extern int      GV_CEV_msgnum;
extern char *   GV_CEVmsg[];
extern int	GV_TEL_msgnum;
extern char *	GV_TELmsg[];
extern int	GV_APP_msgnum;
extern char *	GV_APPmsg[];

extern LogAttr_t GV_LogConfig;		/* Holds all Log Config info */
extern MsgSet_t	GV_LOG_MsgDB[];
extern char	GV_AppId[];
extern char	GV_Application_Name[];
extern char	GV_Resource_Name[];
extern int	GV_DateFormat;
extern Bool	GV_LogInitialized; 	/* Have we initialized with iVIEW ? */
extern int	GV_CevEventLogging;

void	ISP_ApplicationId( char * );

#endif /* _ISP_LOGINC_H */
