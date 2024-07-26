/*-----------------------------------------------------------------------------
ssHeaders.h:	Header information used by WSS Lite APIs

Update:		01/24/97 sja	Segregrated this information from WSS.h
Update:		03/27/97 sja	Increased MAXDATA from 2000 to 4096
Update:		04/16/97 sja	Decreased MAXDATA from 4096 to 2048
----------------------------------------------------------------------------*/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <netdb.h> 
// #include <sys/stropts.h>
#include <time.h>
#include <sys/time.h>

#include "ss.h"

int	GV_InitCalled;

char	__log_buf[512];

extern int	errno;

/*-----------------------------------------------------------------
The following routines are used internally by the API lib
-----------------------------------------------------------------*/
int shutdownSocket(char *ModuleName);
int sendSize(char *module, guint32 size);
int recvSize(char *module, size_t *size, int timeout);
int readSocketData(char *module, char *whatData, int timeout,
						size_t dataSize, char *dataBuf);
int writeSocketData(char *module, char *whatData, guint32 dataSize, char *dataBuf);
int getField(char delim, char *buffer, int fld_num, char *field);
