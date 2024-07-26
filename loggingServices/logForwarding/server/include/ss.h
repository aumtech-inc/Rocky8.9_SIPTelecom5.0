/*-----------------------------------------------------------------------------
ss.h:		Header file for ss Lite Applications.

Author:		Aumtech Inc.
Date:		01/24/96
Update:		04/08/97 sja	Added ss_FILE_EXISTS
----------------------------------------------------------------------------*/
#include <stdio.h>
#define	ss_SUCCESS	 0			/* server api success */
#define	ss_FAILURE	-1			/* server api failed */
#define	ss_TIMEOUT	-2			/* api timeout */
#define	ss_DISCONNECT	-3			/* api client disconnect */
#define	ss_FILE_EXISTS	 1			/* Destination exists */

#define RF_APPEND	1
#define RF_OVERWRITE	2
#define RF_PROTECT	3

#define MAX_ISP_PARM	5	/* Must be > last item above */

/*DDN: 02032009 */
#ifndef GUINT32
#define GUINT32
typedef unsigned int guint32;
#endif

/*---------------------------------------------------
	ss API prototypes
---------------------------------------------------*/
int ss_Exit(void);
int ss_GetGlobal(char *globalVar, int *value);
int ss_GetGlobalString(const char *var_name, char *var_value);
int ss_Init(int argc, char **argv);
int ss_RecvData(int timeout, size_t dataSize, char *dataBuf, long *readBytes);
int ss_RecvFile(char *destination, int fileOption);
int ss_NewRecvFile(char *destination, int fileOption);
int ss_SendData(guint32 size, char *data);
int ss_SendFile(char *destination);
int ss_NewSendFile(char *destination);
int ss_SetGlobal(char *globalVar, int value);
