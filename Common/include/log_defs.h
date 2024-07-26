/*-----------------------------------------------------------------------------
Program:        log_defs.h
Purpose:        To define mnemonic names functions for remote logging 
		routines.
Author:         D. Barto
Date:           02/24/97
Update:         03/13/97 D. Barto added XFER_FILE, XFER_DATA, and modified
				  prototypes for.
Update:		11/05/97 D. Barto removed function declarations
------------------------------------------------------------------------------*/

#define FIFONAME	"log.fifo"
#define	BUFSZ		256
#define	PATHSZ		256
#define	PERMS		0666
#define MAXLOGDIRS      2
#define FORWARD_LOG	1
#define LOCAL_LOG	0
#define EXTENSION	"for"
#define	XFER_FILE	"0"
#define XFER_DATA	"1"

#define	XFER_ENCR		"1"
#define XFER_NOENCR		"2"

#define NUM_ENC_BYTES_TO_PROCESS	4096000

#define LOG_X_FILE      "LOG_x_LOCAL_FILE"
#define LOG_2X_FILE     "LOG_2x_LOCAL_FILE"
#define LOG_PRI_SERVER  "LOG_PRIMARY_SERVER"
#define DEF_BASENAME    "ISP"

#ifdef LOG_SERVER_CLIENT
int GetLogForwardEncryption(int *zLogForwardEncryption);
int     GetLogServer(char *Server,	char *NodeName);
void    GetLogPaths( char *LogPath_1,	char *LogPath_2);
int     WriteToFifo( char *logpath1,	char *message);
int     WriteLocal(  char *LogPath_1,	char *Message, int forward_sw);
//int	WriteLocal_2(char *Message, int forward_sw);
int	DelReadRecords(char *filename,	FILE *fp, char *buf);
#endif
