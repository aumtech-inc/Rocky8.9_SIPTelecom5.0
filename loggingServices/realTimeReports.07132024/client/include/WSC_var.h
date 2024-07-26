/*----------------------------------------------------------------------------
WSC_var.h : This is header file for work station client library using socket 
	   The file contains the definations and data structure for client 
	   library.

Update:		03/04/97 sja	Changed MAX_RETRY from 3 to 2
Update:		03/27/97 sja	Increased MAXDATA from 2000 to 4096
Update:		04/16/97 sja	Decreased MAXDATA from 4096 to 2048
Update:		06/05/97 sja	removed Que related variables & GV_SysDelimiter
Update:		06/05/97 sja	removed MAXDATA
Update:		06/05/97 sja	Added the recvSize & sendSize func. prototypes
Update:		06/05/97 sja	Added readSocketData & writeSocketData
Update:		04/26/99 apj	No Change.
----------------------------------------------------------------------------*/

#ifndef WSC_var_H
#define WSC_var_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <netdb.h> 
// #include <sys/stropts.h>
#include <sys/stat.h> 
#include <unistd.h>


#define _FILE_OFFSET_BITS   64

#define SERV_TCP_PORT 	6800			/* TCP port */
#define MAXDATA 	2048			/* maxi. data length for TCP */
#define	MAX_RETRY	2			/* maximum retry to server */

#define NET_EOF		'~'

int	GV_ScConnected;				/* connection made - false */
int	GV_SysTimeout;
char	__log_buf[MAXDATA+1];			/* buffer for log messages */
char	__Log_Date[10];				/* time string YYMMDD */	
int  	 __proc_id;				/* current process id */

extern	int errno;				/* system error no */
extern	int t_errno;				/* TCP/IP error no */

/* following data structures are define for TCP/IP */
int 	sockfd;					/* socket file descriptor */
int	flags;				/* used to receive info */
struct 	hostent *hp;				/* holds IP address of server */
struct	sockaddr_in serv_addr;			/* the server's full address */

extern int	errno;

/*-----------------------------------------------------------------
The following routines are used internally by library
-----------------------------------------------------------------*/
int Write_Log(char *mod, int debugMode, char *ptr);
int recvSize(char *module, long *size, int timeout);
int sendSize(char *module, long size);
int readSocketData(char *module, char *whatData, int timeout,
						long dataSize, char *dataBuf);
int writeSocketData(char *module, char *whatData, long dataSize, char *dataBuf);
#endif /* WSC_var_H */
