/*----------------------------------------------------------------------------
Program:	ispinc.h
Purpose:	To hold defines and structure definitions.
Author:		G. Gannon
Date:		03/30/94
Update: 	02/26/96 G. Wenzel got rid of junk from old Resp and SIO
Update:		03/12/97 sja	Changed WKS to WSS
Update:		03/12/97 sja	Added #include <sys/time.h>
Update:		09/05/98 mpb	changed GV_CDR_Key to 50

 * -------------------------------------------------------
 * Mar 30-1994   ggannon      Added subsystem mnemonics
 * Mar 31-1994   ggannon      Added documentation & mnemonics 
 * Apr 27-1994   ecunning     Added UTLLIST information
 * May 15-1994   ggannon	add ISP definitions for UTLLLMGR 
 * June 6-1994   rschmidt	add new Log Manager reporting modes
 * June 8-1994   rschmidt	add GV_CDR_Key and GV_CDR_ServTranName
 * June 22-1994   drstephe	add (4) defines for UTLCLNT
 * Sept 30-1994   drstephe	add (1) define for UTLCLNT - ISP_PKT_SIZE
 * Oct 29-1994   rschmidt	MR689 - add DefaultLanguage to 

Update:		02/27/96 G. Wenzel added NULL_CDR_TIME				        GV_SRV_GlobalConfig_type
 *-----------------------------------------------------
*/
#ifndef	__ISPINC_H__
#define	__ISPINC_H__

#include <sys/param.h>
#include <sys/time.h>
/*Ajay Added following include files from Telecom.h On Aug 7th '98 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/utsname.h>			
#include <termio.h>
#include <ctype.h>
#include <sys/signal.h>
#include <sys/stat.h>  
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/msg.h>
/*--------------------------------------------
 * Definitions for the ISP general stuff  
 *--------------------------------------------
*/
#define ISP_BUF_SIZE	1024
#define ISP_PKT_SIZE	256
#define ISP_MAXDIRSIZE	144	  /* Max Directory size to use */
#define ISP_MAXHOSTNAME	14	  /* Host name entry size */
#define ISP_MAXDOMXSIZE  10	  /* Domain X ref array size */
#define ISP_MAXUSERDATA	50	  /* Max len of the user data token */
#define ISP_MAXPGMNAME	100	  /* Max length of a program name */
#define ISP_MAXLISTMBR  75	  /* Max Number of lists for UTLLLMGR */
#define ISP_MAXPRINTERNAME 25	  /* Max length of the printer name */
#define ISP_HIGHVALUE   0xff      /* Value of FF highest byte rep */    
#define ISP_LOWVALUE    0x01      /* Value of 01 lowest not null  */    

#define RES_TYPE_DYNAM		1
#define RES_TYPE_STATIC		2
#define RES_TYPE_DYNAM_MGR	3
#define RES_TYPE_ISDN		4
#define RES_TYPE_ANALOG 	5
#define RES_TYPE_LU2 		6
#define RES_TYPE_TELNET		7
#define RES_TYPE_RS232		8
#define RES_TYPE_EVENT_SRV	9
#define RES_TYPE_SSH		10
#define RES_TYPE_SSH2		11
#define RES_TYPE_NONE		20

#define APP_STATE_ACTIVE 	1
#define APP_STATE_KILLORDER 	2
#define APP_STATE_DEAD		3
#define APP_STATE_SHUTORDER	4
#define APP_STATE_FREE		5
#define APP_STATE_MARKED	6
#define APP_STATE_MGR	        7

#define ACCESS_MODE_FORE	1
#define ACCESS_MODE_BACK	2

/*--------------------------------------------
Null value for CDR time variables
--------------------------------------------*/
# define NULL_CDR_TIME 	"-1" 	

/*--------------------------------------------
Definitions for the ISP return code values 
--------------------------------------------*/
#define ISP_FAIL	0
#define ISP_SUCCESS	1
#define ISP_WARN	2
#define ISP_SERVOFF	3
/*--------------------------------------------
 * StatusCode information from a request      
 * 	returned by a Public API call and 
 * 	returned in a solicited event.
 *--------------------------------------------
*/
#define ISP_STAT_BADPARM 	1
#define ISP_STAT_BADCOM  	2
#define ISP_STAT_INTERROR 	3
#define ISP_STAT_BADFORMAT 	5

/*--------------------------------------------
 *  Mnemonics used for the Global Attributes.
 *
 *--------------------------------------------
*/
#define DOM             1
#define INTER           2
#define USDATE          3
#define INTERDATE       4
#define ALTDATE         5
#define LONGDATE        6
#define MILTIME         7
#define STDTIME         8
#define US              9
#define GERMAN          10
#define US1             11
#define US2             12
#define DM1             13
#define DM2             14
#define M               15
#define A               16
#define Y               17
#define N               18
#define LANGUS          19
#define LANGER          20
#define STATEON         21
#define STATEOFF        22

/*--------------------------------------------
 *  ISP object types in the domain
 *--------------------------------------------
*/
#define ISP_OBJ_TEL	0 
#define ISP_OBJ_APP	1
#define ISP_OBJ_DIR	2
#define ISP_OBJ_SNA 	3
#define ISP_OBJ_TCP 	4
#define ISP_OBJ_NET 	5
#define ISP_OBJ_MAIL	6
#define ISP_OBJ_WSS	7
#define ISP_OBJ_X25	8
#define ISP_OBJ_USER	9
#define ISP_OBJ_SIO	10 
/*----------------------------------------------------
 * Type defs for all isp stuff
 *-----------------------------------------------------*/

typedef unsigned long int Uint32;
typedef short int Int16;
typedef unsigned short int Uint16;
typedef signed char Int8;
typedef unsigned char Uint8;
typedef char Char;

typedef enum {
	False = 0,
	True = 1
} Bool;
/*------------------------------------------------------------------
 * commented out the typedef of 'Int32' because it HAS
 * to be defined in the isp_rpc.x and isp_rpc.h file for
 * the XDR routines to work.
 *
 * When defined here a warning message is printed
 * during compulation it is multiply defined!
 *
 * typedef long int Int32;
 *--------------------------------------------------
 * Include the isp_rpc.h header file so that
 * the rpc defined OBJ_TramsitParms data structure
 * is available and Int32.
 *---------------------------------------------------*/
#include "isp_rpc.h" 

/*--------------------------------------------------------------------
 * Defines: typedef for the ISP global attributes known to all objects.
 *--------------------------------------------------------------------
*/
typedef struct {
	char	DomainName[ISP_MAXHOSTNAME];
	char	DirectoryHost[ISP_MAXHOSTNAME];
	Int32	MaxMessageLength;
	Int32 	DateFormat;
	Int32	TimeFormat;
	Int32	CurrencyType;
	Int32 	MoneyFormat;
	char	FormatDelimiter;
	char	Interruptable;
	Int32	Language;
	char	ObjectName[4];
        Int32	ObjectType;
	char 	ProcessMode;
} GV_GlobalAttr_type;

extern GV_GlobalAttr_type GV_GlobalAttr;

/*--------------------------------------------------------------------
 * Defines: typedef for the Global Config information.
 *--------------------------------------------------------------------
*/
typedef struct {

	char 	DefaultPrinter[ISP_MAXPRINTERNAME];
	Int32 	MAXUP;
	Int32	MSGMNB;
	Int32	MSGSEG;

} GV_GlobalConfig_type;

extern GV_GlobalConfig_type GV_GlobalConfig;

/*--------------------------------------------------------------------
 * Defines: typedef for the Global Telecom Config information.
 *--------------------------------------------------------------------
*/
typedef struct {

	char 	ServiceType[3];
	char 	Release[3];
	char	LocalAreaCode[5];
	char	OffPremiseAccess[5];
	char	LongLinesAccess[5];
	char	InternatAccessCode[5];
	char	DefaultLanguage[10];
	Int32	NumberPorts;
	Int32	FaxTimeOut;
	Int32	PBXType;
	Int32	PBXCountry;
	Int32	AutoStart;
	Int32	AutoStop;
	Int32	HeartBeatInterval;
	Int32	CheckSum;
	Int32	AppMonitor;
	Int32	NetServices;
	Int32	TelCoupler;

	Int32	AccessMode;


} GV_SRV_GlobalConfig_type;

extern GV_SRV_GlobalConfig_type GV_SRV_GlobalConfig;

/*--------------------------------------------------------------------
 * Defines: typedef for the Base Directory Structure.  This
 *	directory is relative to the $PARMLIB variable.  For details
 *	see Data Areas of Object Detailed Design.
 *--------------------------------------------------------------------
*/
typedef struct {

        char	Base[ISP_MAXDIRSIZE];
        char	Log[ISP_MAXDIRSIZE];
        char	Global[ISP_MAXDIRSIZE];
        char	Exec[ISP_MAXDIRSIZE];
        char	Tables[ISP_MAXDIRSIZE];
        char	Applications[ISP_MAXDIRSIZE];
        char	Utilities[ISP_MAXDIRSIZE];
        char	Operations[ISP_MAXDIRSIZE];
	char	Locks[ISP_MAXDIRSIZE];
	char	Temp[ISP_MAXDIRSIZE];
	char	Parms[ISP_MAXDIRSIZE];

} GV_Directory_type;

extern GV_Directory_type GV_Directory;

/* Definition of Call Detail Recording Key */
extern char GV_CDR_Key[50];
extern char GV_CDR_ServTranName[27];
extern char GV_CDR_CustData1[9];
extern char GV_CDR_CustData2[101];

/* Definition of Services Globals */
extern char GV_ServType[4];
extern int  GV_InitCalled;
extern int  GV_EndOfCall;
extern int  GV_HideData;
extern char GV_IndexDelimit;

/*---------------------------------------------------------------
  The following defines and structure are used by the UTLFILE module.  
  This module will open, read and close attribute files.

  04/29/94 - Ecunning	Created
*---------------------------------------------------------------*/

#define ISP_FILE_OPEN 		1
#define ISP_FILE_READ 		2
#define ISP_FILE_CLOSE		3

#include <stdio.h>
struct File_Parms{

  FILE	*PT_Handle;
  char 	FileName[ISP_MAXDIRSIZE];
  Int32 FILECommand; 

  char  *Buffer;
  Int32 BufSize;
};

/*---------------------------------------------------------------
  The following section is for the UTLSERV module
 *---------------------------------------------------------------
*/
#define ISP_SRV_REG         1
#define ISP_SRV_UNREG       2
#define ISP_SRV_RUN         3

#define ISP_MAX_NETWORK 100

#ifdef  __hp9000s800
#define ulong   u_long
#endif

typedef struct Server_Data_type {

  Int32         SRVCommand;
  ulong         Prognum;
  ulong         Versnum;

  char          *(*PT_ValidateFunction)();

} Server_Data;

/*---------------------------------------------------------------
  The following section is for the UTLCLNT module
 *---------------------------------------------------------------
*/
#define ISP_CLNT_SEND_ASYNC	0
#define ISP_CLNT_SEND_SYNC	1
#define ISP_CLNT_CREATE		2
#define ISP_CLNT_DESTROY	3


/*--------------------------------------------------------------------
 * File: $PARMLIB/source/include/utlllmgr.h
 * Description:  This file contains the needed defines and type defs
 *      to use the UTLLLMGR object.  For a detailed description of
 *      UTLLLMGR see common object detailed design.
 *
 * Date     Who      What
 * 5/12/94  ggannon  Installed the manager.
 *-------------------------------------------------------------------
 */
#define LIST_MAXELEMENT 1000
#define LIST_CMD_REGISTER       9000
#define LIST_CMD_PT_TOP         9001
#define LIST_CMD_PT_BOTTOM      9002
#define LIST_CMD_ADDNEW         9003
#define LIST_CMD_UPDATE         9004
#define LIST_CMD_DELETE         9005
#define LIST_CMD_DUMP           9006
#define LIST_CMD_GETLAST        9007
#define LIST_CMD_GETNEXT        9008
/*---*/
#define LIST_STATUS_NOCNTL      9500
#define LIST_STATUS_TOOLITTLE   9501
#define LIST_STATUS_TOOMUCH     9502
#define LIST_STATUS_INVALIDCMD  9503
#define LIST_STATUS_LISTEMPTY   9504
#define LIST_STATUS_LISTEND     9505
#define LIST_STATUS_ALLOCFAIL   9506
#define LIST_STATUS_BADBUFR     9507
#define LIST_STATUS_NOGET       9508
/*---*/
typedef struct {
        Int32   LIST_Command;
        Int32   LIST_Handle;
        Int32   LIST_Status;
	Int32	LIST_ElementCount;
        void    *PT_LIST_Buffer;
	void	*Reserved01;
	void	*Reserved02;
} ListACB_type;
typedef struct {
        void    *PT_ListTop;
        void    *PT_ListCurrent;
        void    *PT_ListBottom;
        void    *PT_ListBuffer;
        Int32   ListElementCount;
	Bool	ListType;
	Int32	ListElementLength;
        Int32   ListLastCmd;
} ListCntl_type;
        
typedef struct{
        void *PT_Fwd;
        void *PT_Bwd;
} ListPointers_type;

enum UTLMSGS_Commands{

  MSGS_REGISTER,
  MSGS_DEREGISTER,
  MSGS_ADD,
  MSGS_REMOVE,
  MSGS_GETMSGS,
  MSGS_PUTMSGS,
  MSGS_REPORT,
  MSGS_CHECK
  
};

enum UTLMSGS_Status{

  MSGS_STATUS_OK,
  MSGS_STATUS_SSIOREG_ERROR,
  MSGS_STATUS_SOFREG_IN_ERROR,
  MSGS_STATUS_SOFREG_OUT_ERROR,
  MSGS_STATUS_ADD_ERROR,
  MSGS_STATUS_REMOVE_ERROR,
  MSGS_STATUS_GET_ERROR,
  MSGS_STATUS_PUT_ERROR,
  MSGS_STATUS_ALLOC_ERROR,
  MSGS_STATUS_NOTREGISTERED,
  MSGS_STATUS_NOTAVAILABLE
  
};


/*-----------------------------------------------------------------
* 06/12/94
*
* Things needed for APPMAINT
*
*----------------------------------------------------------------*/
#define		ISP_MAXGROUPNAME	26
#define 	ISP_MAXUSERID		15
#define 	ISP_MAXDESCRIPTION	75
#define		ISP_MAXUSERTOKEN	25
#define		ISP_TOTALSCHEDULESLOTS	96
/*-------------------------------------------------------------------
 * Defines: The enumerated values for the Different files.
 * See Application Access OBject Detailed-design 
 ------------------------------------------------------------------*/
 enum MAINT_FILE {
	MAINT_FILE_PGMREF = 1,
	MAINT_FILE_DATATOKEN,
	MAINT_FILE_APPGRP
	};
/*-------------------------------------------------------------------
 * Defines: The enumerated values for the maint commands. 
 * See Application Access OBject Detailed-design 
 ------------------------------------------------------------------*/
enum MAINT_CMD {
	MAINT_CMD_RANGESTART = 1,
        MAINT_CMD_GETNEXT,
        MAINT_CMD_GETLAST,
        MAINT_CMD_POINTTOP,
        MAINT_CMD_POINTREC,
        MAINT_CMD_GETRELATED,
        MAINT_CMD_ADDNEW,
        MAINT_CMD_DELETE,
        MAINT_CMD_UPDATE,
        MAINT_CMD_REGISTER,
        MAINT_CMD_DEREGISTER,
	MAINT_CMD_DEREGQUICK,
        MAINT_CMD_RELOAD,
        MAINT_CMD_RANGEEND
};
/*-------------------------------------------------------------------
 * Defines: The enumerated values for the maint STatus codes.
 * See Application Access OBject Detailed-design 
 ------------------------------------------------------------------*/
enum MAINT_STATUS {
	MAINT_STATUS_RANGESTART = 1,
        MAINT_STATUS_ENDFILE,
        MAINT_STATUS_NOTFOUND,
        MAINT_STATUS_DELFAILED,
        MAINT_STATUS_INVALIDCMD,
        MAINT_STATUS_NOTREGISTERED,
        MAINT_STATUS_REGISTERED,
        MAINT_STATUS_UPDTFAIL,
        MAINT_STATUS_UPDTERR,
	MAINT_STATUS_FILEOPEN,
	MAINT_STATUS_BADFILE,
	MAINT_STATUS_INTERNAL,
	MAINT_STATUS_SORT_MALLOC,
	MAINT_STATUS_SORT_DELETE,
	MAINT_STATUS_SORT_ADD,
        MAINT_STATUS_RANGEEND
};
/*-------------------------------------------------------------------
 * Defines: The program reference record. 
 * See Application Access OBject Detailed-design 
 ------------------------------------------------------------------*/
typedef struct {
        Int32   ProgramNumber;
        char    ApplicationGroup[ISP_MAXGROUPNAME];
        char    ProgramName[ISP_MAXPGMNAME];
        char    StaticOrDynamic;
        Int32   NumberStaticPgm;
        char    Description01[ISP_MAXDESCRIPTION];
        char    CreatorId[ISP_MAXUSERID];
        time_t  TimeStampCreated;
        char    UpdaterId[ISP_MAXUSERID];
        time_t  TimeStampUpdated;
        time_t  TimeStampInvalid;
} PgmRefRec_type;
/*-------------------------------------------------------------------
 * Defines: The application maintenance Application Group record. 
 * See Application Access OBject Detailed-design 
 *------------------------------------------------------------------*/
 typedef struct {
        char    ApplicationGroup[ISP_MAXGROUPNAME];
        char    Description01[75];
        char    AccountingCode[35];
        Int32   ApplicationGroupNumber;
        Int32   MaxNumberExecuting;
        char    CreatorId[ISP_MAXUSERID];
        time_t  TimeStampCreated;
        char    UpdaterId[ISP_MAXUSERID];
        time_t  TimeStampUpdated;
} AppGrpRec_type;
/*-------------------------------------------------------------------
 * Defines: The application maintenance User Data Token   record. 
 * See Application Access OBject Detailed-design 
 *------------------------------------------------------------------*/
 typedef struct {
        char    ServiceName[ISP_MAXUSERTOKEN];
        char    MachineName[ISP_MAXHOSTNAME];
        char    UserDataToken1[ISP_MAXUSERTOKEN];
        char    UserDataToken2[ISP_MAXUSERTOKEN];
        char    DayOfWeek;
        Int32   ProgramNumber[ISP_TOTALSCHEDULESLOTS];
        char    CreatorId[ISP_MAXUSERID];
        time_t  TimeStampCreated;
        char    UpdaterId[ISP_MAXUSERID];
        time_t  TimeStampUpdated;
        char    Description01[75];
        time_t  TimeStampInvalid;
} DataTokenRec_type;
/*-------------------------------------------------------------------
 * Defines: The application maintenance interface access control
 * block. See Application Access OBject Detailed-design 
 *------------------------------------------------------------------*/
typedef struct {
	Int32	MAINT_Command;
	Int32	MAINT_Status;
	Int32	MAINT_File;
	PgmRefRec_type		*PT_PgmRefRec;
	AppGrpRec_type	*PT_AppGrpRec;
	DataTokenRec_type	*PT_DataTokenRec;
	void	*Reserved01;
	void	*Reserved02;
	void	*Reserved03;
} MaintACB_type;

/*-------------------------------------------------------------------
 * Defines: The application maintenance control Structure
 * Valid States are:
 *	bin 0 - unknown
 *	R	- Registered
 *	I	- Initialization (begining of registration)
 *	U	- Unregistered
 *	L	- Loading one or more tables.
 *------------------------------------------------------------------*/
 typedef struct {
	ListACB_type	PgmRefList;
	ListACB_type	AppGroupList;
	ListACB_type	DataTokenList;
	Int32		PgmRefFile;
	Int32		AppGroupFile;
	Int32		DataTokenFile;
	Int32		CTL_LastCmd;
	char		State;
} MaintCTL_type;

/*-----------------------------------------------------------------
* 06/12/94
*
* Things needed for APPFIND
*
*----------------------------------------------------------------*/

/*-------------------------------------------------------------------
 * Defines: The enumerated values for the maint commands. 
 * See Application Access OBject Detailed-design 
 ------------------------------------------------------------------*/
enum FIND_CMD {
	FIND_CMD_RANGESTART = 1,
        FIND_CMD_GET,
        FIND_CMD_REGISTER,
        FIND_CMD_RELOAD_PGM,
        FIND_CMD_RELOAD_TOKEN,
        FIND_CMD_RELOAD_GROUP,
        FIND_CMD_RELOAD_ALL,
        FIND_CMD_RANGEEND
};
/*-------------------------------------------------------------------
 * Defines: The enumerated values for the maint STatus codes.
 * See Application Access OBject Detailed-design 
 ------------------------------------------------------------------*/
enum FIND_STATUS {
	FIND_STATUS_RANGESTART = 1,
        FIND_STATUS_NOMATCH01,
        FIND_STATUS_NOMATCH02,
        FIND_STATUS_NOMATCH03,
        FIND_STATUS_NOMATCH04,
        FIND_STATUS_NOMATCH05,
        FIND_STATUS_NOMATCH06,
        FIND_STATUS_NOMATCH07,
        FIND_STATUS_NOMATCH08,
        FIND_STATUS_INTEGRITY,
        FIND_STATUS_INTEGPGM,
        FIND_STATUS_INTEGAPP,
        FIND_STATUS_INTERNAL, 
        FIND_STATUS_FILEOPEN,
        FIND_STATUS_NOTREGISTERED,
        FIND_STATUS_BADFILE,
	FIND_STATUS_INVALIDCMD,
        FIND_STATUS_RANGEEND
};
/*-------------------------------------------------------------------
 * Defines: The access control block with which a user can 
 *	 request a serach for application program be made.
 ------------------------------------------------------------------*/
 typedef struct {
	Int32	FIND_Command;
	Int32	FIND_Status;
	char	ServiceName[ISP_MAXUSERTOKEN];
	char	MachineName[ISP_MAXHOSTNAME];	
	char	UserDataToken1[ISP_MAXUSERTOKEN];
	char	UserDataToken2[ISP_MAXUSERTOKEN];
	char	ProgramName[ISP_MAXPGMNAME];
	char	ApplicationGroup[ISP_MAXGROUPNAME];
	Int32	ApplicationGroupNumber;
        char    AccountingCode[35];
	Int32	MaxNumberExecuting;
	char	StaticOrDynamic;
	Int32	NumberStaticPgm;
	void 	*Reserved01;
	void	*Reserved02;
	void	*Reserved03;
} FindACB_type;
Int32 APPFIND( Int32, FindACB_type *);
Int32 APPMAINT( Int32, MaintACB_type *);


/*-------------------------------

the following is needed for util_relational()
*------------------------------*/

#define DB_OPEN	1
#define DB_CLOSESAVE 2
#define DB_QUIT 3
#define DB_GET_DATATOKEN 4
#define DB_PUT_DATATOKEN 5
#define DB_GET_APPGRP 6
#define DB_PUT_APPGRP 7
#define DB_GET_PGMREF 8
#define DB_PUT_PGMREF 9


typedef struct {

  DataTokenRec_type	DataTokenRec;
  AppGrpRec_type	AppGrpRec;
  PgmRefRec_type	PgmRefRec;

} db_type;

/*------------------------------------------
the following structures are needed for the util_rdb
facility
*/


enum search_things{

  SEARCH_REGISTER = 100,
  SEARCH_FIND

};


typedef struct {

  ListACB_type Programs;
  ListACB_type AppGrps;
  ListACB_type Tokens;

  Int32 Registered;

} rdbACB_type;

#include "loginc.h" 

#endif /* __ISPINC_H__ */


