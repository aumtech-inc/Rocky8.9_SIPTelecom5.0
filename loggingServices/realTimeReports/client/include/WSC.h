/*--------------------------------------------------------------------
File:		WSC.h

Purpose:	Contains header information that is used by the WSC APIs

Authors:	Aumtech Inc.

Update:		06/05/97
--------------------------------------------------------------------*/

#define	WSC_SUCCESS		0
#define	WSC_FAILURE		-1
#define	WSC_TIMEOUT		-2
#define	WSC_DISCONNECT		-3
#define	WSC_READ_ZERO_BYTES	-4

#define	WSC_FILE_EXISTS		1

#define RF_APPEND		1
#define RF_OVERWRITE		2
#define RF_PROTECT		3

/*-----------------------------------------------------------
Following are the function prototype for work station client 
-------------------------------------------------------------*/
int WSC_Exit(void);
int WSC_GetGlobal(char *globalVar, int *value);
int WSC_Init(char *application, char *host);
int WSC_RecvData(int timeout, long size, char *buf, long *readBytes);
int WSC_RecvFile(char *destination, int fileOption);
int WSC_SendData(long size, char *data);
int WSC_SendFile(char *destination);
int WSC_SetGlobal(char *globalVar, int value);
