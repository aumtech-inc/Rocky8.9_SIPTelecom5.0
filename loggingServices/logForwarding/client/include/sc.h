/*--------------------------------------------------------------------
File:		sc.h

Purpose:	Contains header information that is used by the sc APIs

Authors:	Aumtech Inc.
Update:		04/26/99 apj	Added isSocketStillConnected.
Update:		07/26/02 ddn	Added zStartTime and zTimeOut and appPid to sc_NewRecvFile
--------------------------------------------------------------------*/

#define	sc_SUCCESS	0
#define	sc_FAILURE	-1
#define	sc_TIMEOUT	-2
#define	sc_DISCONNECT	-3
#define	sc_DISCONNECT	-3
#define	sc_FILE_EXISTS	1

#define RF_APPEND	1
#define RF_OVERWRITE	2
#define RF_PROTECT	3

/*-----------------------------------------------------------
Following are the function prototype for work station client 
-------------------------------------------------------------*/
int sc_Exit(void);
int sc_GetGlobal(char *globalVar, int *value);
int sc_Init(char *application, char *host);
int sc_RecvData(int timeout, long size, char *buf, long *readBytes);
int sc_NewRecvData(int timeout, long size, char *buf, long *readBytes);
int sc_RecvFile(char *destination, int fileOption);
int sc_NewRecvFile(char *destination, int fileOption, time_t zStartTime, int zTimeOut, char appPid[8]);
int sc_SendData(long size, char *data);
int sc_SendFile(char *destination);
int sc_NewSendFile(char *destination);
int sc_SetGlobal(char *globalVar, int value);

int isSocketStillConnected(char *parmModuleName, int *parmErrNo);
