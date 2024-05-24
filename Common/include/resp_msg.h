/*----------------------------------------------------------------------------
Program:	resp_msg.h
Purpose:	Error message definitions for responsibility subsystem.
Author:		M. Joshi
Date:		Unknown
Update:		03/01/96 M.Joshi updated messages.
Update:		08/12/97 M. Bhide added "place-holder" msgs to facilitate 
Update:			 for future upgrade of message processing system for 
Update:			 Resp. to make it log messages like other systems.

NOTE:		03/01/96 Mahesh needs to verify use of ISP_ETERM msg.

NOTE:		08/12/97 DO NOT CHANGE THE ORDER OF THESE MESSAGES IN THE
			 THIS FILE. THEY SHOULD CORRESPOND TO THE #defines
			 IN isp_baseresp.c, so that we can eventually convert
			 the log message processing in Resp to the way it
			 is done in all other systems.
----------------------------------------------------------------------------*/
#define	ISP_EMSG	"%s"	
#define	ISP_EENV	"Environment variable %s is not found/set."
#define	ISP_ESIG	"Application %s terminated process id %d resource = %d."
#define	ISP_ESHM_AT	"Unable to attach shared memory segment %ld. errno = %d" 
#define	ISP_ESHM_DT	"Unable to dettach shared memory segment %ld errno = %d" 
#define	ISP_ESHMID	"Unable to get shared memory segment %ld. errno = %d" 
#define	ISP_ESHM_RM	"Unable to remove shared memory %ld. errno = %d"
#define	ISP_EMSGQID 	"Unable to get message queue (%ld) identifier. errno = %d"
#define	ISP_EMSGQ_RM	"Unable to remove message queue %ld. errno = %d" 
#define	ISP_ESTOP	"Application <%s> (%d) started %d times in %d seconds. Spawning too rapidly. Application will not be started again until next reload." 		
#define	ISP_EACTIVITY	"No heartbeat from application <%s>, Resource = <%s>, pid=%d in %d seconds. It appears to be hung. Application terminated. Last activity = %s"
#define	ISP_ESTART	"Can't start application %s. errno = %d" 
#define	ISP_EFILE	"Application <%s> not found, errno = %d. Scheduling table entry (DNIS, ANI) = (%s, %s) is ignored." 
#define	ISP_ETEL_MATCH	"Received (DNIS,ANI)=(<%s>,<%s>). No such entry in scheduling table."
#define	ISP_EMATCH	"Received static destination <%s>. No such entry in scheduling table."
#define	ISP_EOBJ	"Invalid parameter <%s>. Server is not supported."	
#define	ISP_EOBJ_NUM	"Invalid server identifier: %d."	
#define	ISP_ERECV_MESG	"Unable to receive message from queue %ld. errno = %d"
#define	ISP_EOPEN_PIPE	"%s. Unable to open pipe for command %s. errno = %d"
#define	ISP_EOPEN_FILE	"Unable to access file %s. errno = %d"	
#define	ISP_EVOICE	"Voice software is not running"
#define	ISP_ERULE	"Invalid rule number in scheduling table: %d."
#define	ISP_EKILL	"DEBUG: Can't kill application <%s>. pid: %d errno %d"
#define	ISP_EHBEAT	"DEBUG: Setting kill time to %d." 
#define	ISP_EUSAGE	"Usage : %s <object name> <kill time>"	
#define	ISP_APPINFO	"DEBUG: Application <%s> started pid: %d Resource <%s>" 
#define	ISP_SHMINFO	"DEBUG: Shared memory = %ld (hex = %x) created."
#define	ISP_EMSGQINFO	"DEBUG: Message queue = %ld (hex = %x) created."
#define	ISP_CHKFILE	"DEBUG: Checking existence of file <%s>." 	
#define	ISP_MSGDATA	"DEBUG:	Dynamic Manager Queue Mesg Received is type = %ld data = <%s>" 
#define	ISP_ECMD	"%s : Invalid command/parameter : %d"
#define	ISP_EPGMGRP	"Can't find program group <%s> in application reference table"
#define	ISP_EPGM	"Can't find program name <%s> in program reference table" 
#define	ISP_ETERM	"Application %s (pid = %d) terminated not qualified for current instance"	/* Mahesh needs to verify use of this message */
#define	ISP_ETABLEFULL	"Dynamic manager table full. Unable to add program name <%s>. pid = %d"
#define	ISP_EMSGSND	"Unable to start application <%s>. Failed to send msg to dynamic manager. errno = %d"
#define	ISP_ESYSTIME	"Unable to get system time. errno = %d"		
#define	ISP_EFIELD	"Corrupted line in <%s>. Unable to read field no %d, record no. %d. Line is: <%s>"
#define ISP_EEXECL	"execl(path=<%s>, directory=<%s>, program=<%s>) system call failed, errno = %d"		/* place-holder not used in code */
#define ISP_ESTART_NET	"Unable to start network services for %s server execl(path=<%s>,  program=<%s>) system call failed, errno = %d" /* Place-holder */
#define ISP_ESTART_ACCESS "Unable to start %s services for %s server execl system call failed, errno = %d"		/* Place-holder */
#define ISP_EFORK	"fork() system call failed, errno = %d" /*Place-holder*/
/**************************** eof() ***********************************/
