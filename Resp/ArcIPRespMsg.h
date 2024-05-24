/*----------------------------------------------------------------------------
Program:	ArcIPRespMsg.h
Purpose:	All message text and message numbering definitions for IP 
		Telecom Services Responsibility. This file was copied over
		from resp_msg.h and modified appropriately for IP Telecom.
	Note:	Convention is:	The message number is defined relative to the
				value of RES_BASE. 
				The message format statement is the same as the
				message number with "_MSG" added to it.
Author:		George Wenzel
Date:		12/04/2000
----------------------------------------------------------------------------*/
#define RES_BASE	1000
#define RES_EENV	RES_BASE+1
#define RES_EKILL	RES_BASE+2
#define RES_ESHMID	RES_BASE+3
#define	RES_ESHM_AT	RES_BASE+4
#define	RES_EMSGQ_RM 	RES_BASE+5
#define	RES_ESHM_RM	RES_BASE+6

#define	RES_EENV_MSG	"Environment variable <%s> is not found/set."
#define	RES_ESIG_MSG	"Application <%s> terminated. pid=%d. resource=%d."
#define	RES_ESHM_AT_MSG	"Unable to attach shared memory segment %ld. errno = %d" 
#define	RES_ESHM_DT_MSG	"Unable to dettach shared memory segment %ld errno = %d" 
#define	RES_ESHMID_MSG	"Unable to get shared memory segment %ld. errno = %d" 
#define	RES_ESHM_RM_MSG	"Unable to remove shared memory %ld. errno = %d"
#define	RES_EMSGQID_MSG	"Unable to get message queue (%ld) identifier. errno = %d"
#define	RES_EMSGQ_RM_MSG "Unable to remove message queue %ld. errno = %d" 
#define	RES_ESTOP_MSG	"Application <%s> (%d) started %d times in %d seconds. Spawning too rapidly. Application will not be started again until next reload." 		
#define	RES_EACTIVITY_MSG	"No heartbeat from application <%s>, Resource = <%s>, pid=%d in %d seconds. It appears to be hung. Application terminated. Last activity = %s"
#define	RES_ESTART_MSG	"Can't start application %s. errno = %d" 
#define	RES_EFILE_MSG	"Application <%s> not found, errno = %d. Scheduling table entry (DNIS,ANI)=<%s,%s> is ignored." 
#define	RES_ETEL_MATCH_MSG	"Received (DNIS,ANI)=(<%s>,<%s>). No such entry in scheduling table."
#define	RES_EMATCH_MSG	"Received static destination <%s>. No such entry in scheduling table."
#define	RES_EOBJ_MSG	"Invalid parameter <%s>. Server is not supported."	
#define	RES_EOBJ_NUM_MSG "Invalid server identifier: %d."	
#define	RES_ERECV_MESG_MSG	"Unable to receive message from queue %ld. errno = %d"
#define	RES_EOPEN_PIPE_MSG	"%s. Unable to open pipe for command %s. errno = %d"
#define	RES_EOPEN_FILE_MSG	"Unable to access file %s. errno = %d"	
#define	RES_ERULE_MSG	"Invalid rule number in scheduling table: %d."
#define	RES_EKILL_MSG	"DEBUG: Can't kill application <%s>. pid: %d errno %d"
#define	RES_EHBEAT_MSG	"DEBUG: Setting kill time to %d." 
#define	RES_APPINFO_MSG	"DEBUG: Application <%s> started pid: %d Resource <%s>" 
#define	RES_SHMINFO_MSG	"DEBUG: Shared memory = %ld (hex = %x) created."
#define	RES_EMSGQINFO_MSG	"DEBUG: Message queue = %ld (hex = %x) created."
#define	RES_CHKFILE_MSG	"DEBUG: Checking existence of file <%s>." 	
#define	RES_MSGDATA_MSG	"DEBUG:	Dynamic Manager Queue Mesg Received is type = %ld data = <%s>" 
#define	RES_ECMD_MSG	"%s : Invalid command/parameter : %d"
#define	RES_EPGMGRP_MSG	"Can't find program group <%s> in application reference table"
#define	RES_EPGM_MSG	"Can't find program name <%s> in program reference table" 
#define	RES_ETERM_MSG	"Application %s (pid = %d) terminated not qualified for current instance"	/* Mahesh needs to verify use of this message */
#define	RES_ETABLEFULL_MSG	"Dynamic manager table full. Unable to add program name <%s>. pid = %d"
#define	RES_EMSGSND_MSG	"Unable to start application <%s>. Failed to send msg to dynamic manager. errno = %d"
#define	RES_ESYSTIME_MSG	"Unable to get system time. errno = %d"		
#define	RES_EFIELD_MSG	"Corrupted line in <%s>. Unable to read field no %d, record no. %d. Line is: <%s>"
#define RES_EEXECL_MSG	"execl(path=<%s>, directory=<%s>, program=<%s>) system call failed, errno = %d"		/* place-holder not used in code */
#define RES_ESTART_NET_MSG	"Unable to start network services for %s server execl(path=<%s>,  program=<%s>) system call failed, errno = %d" /* Place-holder */
#define RES_ESTART_ACCESS_MSG "Unable to start %s services for %s server execl system call failed, errno = %d"		/* Place-holder */
#define RES_EFORK_MSG	"fork() system call failed, errno = %d" /*Place-holder*/
/**************************** eof() ***********************************/
