/*



Update:		03/28/97 sja	Added this header
Update:		03/31/97 sja	Changed available to Available in  SHMEM_REC
Update:		04/16/97 sja	Decreased MAXDATA from 4096 to 2048
Update:		05/23/97 sja	Defined SOCKET_ERROR_NAP
Update:		06/02/97 sja	Added exitNap
Update:		2000/11/30 sja	Added include <time.h>

*/

/*------------------------------
	INCLUDES
------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
// #include <stropts.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

/*------------------------------
	CONSTANTS & MACROS
------------------------------*/
#define	NET_EOF		'~' 

#define	MAX_PROCESS	300
#define	MAXDATA		2048

#define	INIT		"0"
#define	KILL		"3"

#define SOCKET_ERROR_NAP	10

/*------------------------------
	SHMEM DEFINES
------------------------------*/
#define SHM_FORMAT      "%8s %8s %17s %12s %5s %10s %50s %2s %6s %2s\n"
#define SHMEM_REC_LENGTH 130    /* shared memory record length */
#define	SHMEM_REC "     xxx      xxx               xxx          xxx    xx        xxx                                          Available  x     xx  0\n"
#define APPL_FIELD1     1
#define APPL_API        2       /* application current API no */
#define APPL_RESOURCE   3       /* application resource */
#define APPL_START_TIME 4       /* start time of application in seconds */
#define APPL_FIELD5     5
#define APPL_FIELD6     6       /* DNIS */
#define APPL_NAME       7       /* application name */
#define APPL_STATUS     8       /* application status */
#define APPL_PID        9       /* application pid */
#define APPL_SIGNAL     10      /* application signal */

/*------------------------------
	GLOBAL VARS
------------------------------*/
int 	listen_fd;
int	recfd;
int	tran_shmid;
char	*__tran_tabl_ptr;
char	__log_buf[256];
char	servType[25];		/* Server Type: ISP_WSS, ISP_DB, ISPLOG */
int	serverPort;
key_t	shmKey;
int	exitNap;

struct process_tabl
	{
	int 	pid;			/* process id of application */
	char	*ptr;			/* pointer to record */
	} ;				/* in shared memory having same pid */

struct process_tabl mem_map[MAX_PROCESS];

/*------------------------------
	EXTERNS
------------------------------*/
extern 	int	errno;

/*------------------------------
	FUNCTION PROTOTYPES
------------------------------*/
int Write_Log(char *ptr);
int recvApplicationName(int fd, char *app);
int updateShmem(char *app, int shmem_idx);
int createIpc(key_t);
int deleteIpc(key_t);
int loadShmem(void);
int getShmemSlot(void);


