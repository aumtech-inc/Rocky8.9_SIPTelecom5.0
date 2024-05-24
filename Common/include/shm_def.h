#ifndef  SHM_DEF_DOT_H
#define SHM_DEF_DOT_H


/*---------------------------------------------------------------------------
File:		shm_def.h 
Purpose:	This file defines shared memory field names and numbers, and
	    	defines application status information.
Date:		Unknown
Update:		03/20/97 gpw fixed this header
Update:		03/20/97 mb added STATUS_CHNOFF
Update:		06/21/99 gpw  removed commented lines
Update:	02/18/03 mpb Added STATUS_SUSPEND.
Update:	04/15/03 mpb Added STATUS_COT.
----------------------------------------------------------------------------*/

/*------------------------------------------------------------
shared memory field positions and field sizes.
-------------------------------------------------------------*/

static  int     vec[10]     = {0, 9, 18, 36, 49, 55, 66, 117, 120, 127};
static  int     fld_siz[10] = {8, 8, 17, 12,  5, 10, 50,  2,  7,  2};

/* Following is the shared memory format to update shared memory */
#define	SHM_FORMAT	"%8s %8s %17s %12s %5s %10s %50s %2s %6s %2s\n" 

/*--------------------------------------------------
shared memory field names and field numbers.
--------------------------------------------------*/
#define SHMEM_REC_LENGTH 130	/* shared memory record length */
#define APPL_FIELD1	1
#define APPL_API	2	/* application current API no */
#define APPL_RESOURCE	3	/* application resource */
#define APPL_START_TIME	4	/* start time of application in seconds */
#define APPL_FIELD5	5	
#define APPL_FIELD6	6	/* DNIS */
#define APPL_NAME	7	/* application name */
#define APPL_STATUS	8	/* application status */
#define APPL_PID	9	/* application pid */
#define APPL_SIGNAL	10	/* application signal */

/*----------------------------------------------
Shared memory status field.
----------------------------------------------*/
#define	STATUS_INIT 		0 	/* application init state */
#define	STATUS_IDLE 		1 	/* application idle state */
#define	STATUS_BUSY 		2 	/* application busy state */
#define	STATUS_KILL 		3 	/* application killed */
#define	STATUS_MONT 		4 	/* application under monitor */
#define	STATUS_UMON 		5 	/* application unmonitor */
#define	STATUS_OFF 		6 	/* application turned off */
#define	STATUS_CHNOFF 		7 	/* channel off */
#define	STATUS_SUSPEND 		8 	/* Application is suspended */
#define	STATUS_COT 		9 	/* Application is performing COT test */
#define	STATUS_ROFF 	10 	/* Application is turned off bt resact */

/****************************** eof() ************************************/

#endif


