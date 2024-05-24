/*------------------------------------------------------------------------------
File:		saUtils.h

Purpose:	Contains all header information for the socket accessory routines.

Author:		Sandeep Agate

Update:		2001/06/04 sja	Created the file
------------------------------------------------------------------------------*/
#ifndef saUtils_H__
#define saUtils_H__

/*
 * Include Files
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


/*
 * Return Code Definitions
 */
#define SA_SUCCESS			0
#define SA_FAILURE			-1
#define SA_TIMEOUT			-2
#define SA_DISCONNECTED		-3

/*
 * Function Prototypes
 */

/*
 * Generic functions for client and server
 */
int saExit(int iSocketFd, char *oErrorMsg);
int saReadSocketData(int iSocketFd,
					long iTimeoutSeconds,long iTimeoutMicroSeconds,
					long iMinDataSizeToRead, long iMaxDataSizeToRead, 
					long *oBytesRead, char *oDataRead, char *oErrorMsg);
int saWriteSocketData(int iSocketFd, long iDataSize, void *iData, 
					long *oBytesWritten, char *oErrorMsg);
int saWriteSocketString(int iSocketFd, long iDataSize, char *iData, 
					long *oBytesWritten, char *oErrorMsg);
int saIsSocketConnected(int iSocketFd, int *oConnectFlag, char *oErrorMsg);
int saGetHostInfo(int zFdSocket, struct sockaddr_in *zClientAddr, 
					char *zClientIP, char *zClientHostName, char *zErrorMsg);

/*
 * Client specific functions
 */
int saClientInit(char *iServerHost, int iServerPort, int *oFdSocket,
					struct sockaddr_in *zServerAddr, char *oErrorMsg);

/*
 * Server specific functions
 */
int saServerInit(int iServerPort, int *oFdListen, char *oErrorMsg);
int saServerAccept(int iFdListen, int *oFdAccept, 
					struct sockaddr_in *zClientAddr, char *oErrorMsg);


#endif /* saUtils_H__ */
