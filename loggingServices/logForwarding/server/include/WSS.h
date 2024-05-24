/*-----------------------------------------------------------------------------
WSS.h:		Header file for Workstation Services Applications.

Author:		Aumtech Inc.

Update:		06/05/97 sja	Added WSS_FILE_EXISTS
----------------------------------------------------------------------------*/

#define	WSS_SUCCESS	 0			/* server api success */
#define	WSS_FAILURE	-1			/* server api failed */
#define	WSS_TIMEOUT	-2			/* api timeout */
#define	WSS_DISCONNECT	-3			/* api client disconnect */
#define	WSS_FILE_EXISTS	 1			/* Destination exists */

#define RF_APPEND	1
#define RF_OVERWRITE	2
#define RF_PROTECT	3

/*---------------------------------------------------
	WSS API prototypes
---------------------------------------------------*/
int WSS_Exit(void);
int WSS_GetGlobal(char *globalVar, int *value);
int WSS_GetGlobalString(const char *var_name, char *var_value);
int WSS_Init(int argc, char **argv);
int WSS_RecvData(int timeout, long dataSize, char *dataBuf, long *readBytes);
int WSS_RecvFile(char *destination, int fileOption);
int WSS_SendData(long size, char *data);
int WSS_SendFile(char *destination);
int WSS_SetGlobal(char *globalVar, int value);
