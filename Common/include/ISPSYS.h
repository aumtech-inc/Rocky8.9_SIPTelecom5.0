/*-------------------------------------------------------------------------
ISPSYS.h : This file has all system key used by isp servers.

Update:		1/22/97	sja	Added SHMKEY_LOG, SHMKEY_DB and changed WKS
				to WSS.
Update:		04/11/97 sja	Added SIZE_SCHD_MEM_WSS
Update:		07/22/97 sja	Added SHMKEY_ARC
Update:		05/17/99 mpb	Changed SIZE_SCHD_MEM form 20 to 60.
Update:		08/13/02 djb	Added SHMKEY_SNMP
Update:		08/19/02 djb	Changed SHMKEY_SNMP to SHMKEY_SNMP_VXML; added
                            SHMKEY_SNMP_VAR, SHMKEY_SNMP_TCALLS,
							and SHMKEY_SNMP_SR   
Update:     10/28/2010 ddn  Added SHMKEY_SIP_RECYCLE for MR3196
--------------------------------------------------------------------------*/
#ifndef ISPSYS_H
#define	ISPSYS_H

#define	SHMKEY_TEL	((key_t)5001) /* telecom server shared memory segment */
#define	SHMKEY_WSS	((key_t)5002) /* work station shared memory segment */
#define	SHMKEY_TCP	((key_t)5003) /* Tcp server shared memory segment */
#define	SHMKEY_SNA	((key_t)5004) /* Sna server shared memory segment */

#define	SHMKEY_LOG	((key_t)5005) /* LOG shared memory segment */
#define	SHMKEY_DB	((key_t)5006) /* DB shared memory segment */
#define	SHMKEY_ARC	((key_t)5007) /* ARC shared memory segment */
#define SHMKEY_SIP_LICENSE  ((key_t)5008) /* Port status shared memory segment for accurate licensing*/
#define SHMKEY_SIP_RECYCLE  ((key_t)5009) /* Recycle shared memory segment for accurate recycling*/

#define	SHMKEY_SNMP_VXML	((key_t)5020) /* SNMP shrd memory;vxml vars */
#define	SHMKEY_SNMP_VAR		((key_t)5021) /* SNMP shrd memory;generic vars */
#define	SHMKEY_SNMP_TCALLS	((key_t)5022) /* SNMP shrd memory;telecom counters*/
#define	SHMKEY_SNMP_SR		((key_t)5023) /* SNMP shrd memory;speech rec vars */

#define SIZE_SCHD_MEM	130*1024		/* size of the shared memory */
#define SIZE_SCHD_MEM_WSS	40*1024		/* size of the shared memory */

/* Queue key for server */

/* Telecom */
/* #define	TEL_RESP_MQUE	5001L */
#define	TEL_RESP_MQUE	0313L

/* Work station server */
/* #define	WKS_RESP_MQUE	5002L */
#define	WSS_RESP_MQUE	0314L

/* TCP Server */
/* #define	TCP_REQS_MQUE	5003L */
#define	TCP_REQS_MQUE	0323L

/* Sna Server */
/* #define	SNA_REQS_MQUE	5004L */
#define	SNA_REQS_MQUE	1234L

/* Bill queue for server */
#define	TEL_BILL_MQUE	5011L
#define	WSS_BILL_MQUE	5012L
#define	PERMS		0666

#define	TEL_DYN_MGR_MQUE	5020L
#define	WSS_DYN_MGR_MQUE	5021L

#endif



