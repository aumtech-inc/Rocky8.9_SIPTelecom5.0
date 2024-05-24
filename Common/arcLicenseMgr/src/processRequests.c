/*------------------------------------------------------------------------------
File:		processRequests.c

Purpose:	This routine contains the main client request processing loop.

Author:		Sandeep Agate, Aumtech Inc.

Usage:		arcLicenseMgr

Update:		2002/05/23 sja 	Created the file.
------------------------------------------------------------------------------*/

/*
** Include Files
*/
#include <arcLicenseMgr.h>

/*
** Static Function Prototypes
*/
int sSendKeyToClient(char *zKey);
int sGetKeyFromClient(char *zKey);

/*------------------------------------------------------------------------------
processRequests(): Process client requests for license validation
------------------------------------------------------------------------------*/
int processRequests(void)
{
	static char	yMod[] 			= "main";
	static int	yNapTime		= 300;
	char		yVersion[10]	= "1.0.0";
	char		yBuild[10]		= "1";
	char		yStartFeature[] = "ARC_LIC_START";
	char		yFeature[]		= "BME";
	int			yTurnkeyAllowed = 0;
	int			yTurnkeyFound;
	int			yFeatureCount;
	char		yErrorMsg[512];
	int			yRc;
	int			yDone;
	struct sockaddr_in	yClientAddr;
	char		yClientIP[20];
	char		yClientHostName[50];
	int			yRequestCount;
	char		yKey[100];
	char		yEncryptedKey[512];
	int			optval, status;
	struct
	{
		int		l_onoff;
		int		l_linger;
	} linger;

	yRequestCount 	= 0;
	yDone 			= 0;

	while( ! yDone)
	{
		/*
		** Validate the license after every 100 requests.
		*/
		/*
		if((yRequestCount % 100) == 0)
		{
			if((yRc = arcValidLicense(yStartFeature, yFeature, yTurnkeyAllowed, 
									&yTurnkeyFound, &yFeatureCount, 
									yErrorMsg)) < 0)
			{
				arcLog(NORMAL, "%s", yErrorMsg); 

				sleep(yNapTime);

				continue;
			}

			// arcLog(VERBOSE, "Validated license.");
		}
		*/

		/*
		** Wait on incoming client requests. This is a blocking call.
		*/
		if(saServerAccept(gAppEnv.fdSocketListen, &gAppEnv.fdSocketServer,
							&yClientAddr, yErrorMsg) < 0)
		{
			arcLog(NORMAL, "%s", yErrorMsg); 

			return(-1);
		}
		gAppEnv.fIsServerConnected = 1;

		/*
		** Get client host information
		*/
		if(saGetHostInfo(gAppEnv.fdSocketServer, &yClientAddr, 
						yClientIP, yClientHostName, yErrorMsg) < 0)
		{
			arcLog(NORMAL, "%s", yErrorMsg); 

			return(-1);
		}

		/*
		arcLog(VERBOSE,
				"Received license request from client, IP (%s), Host (%s).",
				yClientIP, yClientHostName);
		*/


		/*
	 	* Set the SO_KEEPALIVE socket option
	 	*/
		optval = 1;
		status 	= setsockopt(gAppEnv.fdSocketServer, SOL_SOCKET, SO_KEEPALIVE, 
							(char *) &optval, sizeof(optval));
		if(status != 0)
		{
			arcLog(NORMAL,
					"setsockopt() failed for SO_KEEPALIVE, [%d: %s]",
					errno, strerror(errno));
	
			return(-1);
		}
	
		/*
	 	* Set the SO_LINGER socket option
	 	*/
		linger.l_onoff = 1;
		linger.l_linger = 0;
	
		if (setsockopt(gAppEnv.fdSocketServer, SOL_SOCKET, SO_LINGER,
						(void *)&linger, sizeof(linger)) < 0)
		{
			arcLog(NORMAL,
					"setsockopt() failed for SO_LINGER, [%d: %s]",
					errno, strerror(errno));
	
			return(-1);
		}
	
		/*
	 	* Set the SO_REUSEADDR socket option
	 	*/
   		if(setsockopt(gAppEnv.fdSocketServer, SOL_SOCKET, SO_REUSEADDR,
					(char*)&optval, sizeof(optval)) < 0) 
		{
			arcLog(NORMAL,
					"setsockopt() failed for SO_REUSEADDR, [%d: %s]",
					errno, strerror(errno));
	
			return(-1);
   		}

		/*
		** Read the key from the client.
		*/
		if(sGetKeyFromClient(yKey) < 0)
		{
			return(-1); // error msg. written in sub-routine
		}

		/*
		** Encrypt the key
		*/
		arcEncrypt(yKey, yEncryptedKey);
		strcat(yEncryptedKey, "~");

		/*
		** Send the encrypted key back to the client
		*/
		if(sSendKeyToClient(yEncryptedKey) < 0)
		{
			return(-1); // error msg. written in sub-routine
		}

		/*
		** Close the server connection.
		*/
		if(saExit(gAppEnv.fdSocketServer, yErrorMsg) < 0)
		{
			arcLog(NORMAL, yErrorMsg);

			return(-1);
		}

		yRequestCount++;

	} // while !yDone

	return(0);

} /* END: processRequests() */

/*------------------------------------------------------------------------------
sSendKeyToClient(): Get the key from the client.
------------------------------------------------------------------------------*/
int sSendKeyToClient(char *zKey)
{
	static char	yMod[] = "sSendKeyToClient";
	int			yRc;
	char		yErrorMsg[256];
	long		yBytesWritten;


	if((yRc = saWriteSocketData(gAppEnv.fdSocketServer, strlen(zKey), zKey, 
								&yBytesWritten, yErrorMsg)) < 0)
	{
		arcLog(NORMAL,
				"Failed to send (%ld) bytes of encrypted key to client, "
				"rc=(%d) - (%s).",
				strlen(zKey), yRc, yErrorMsg);

		return(-1);
	}

	return(0);

} // END: sSendKeyToClient()

/*------------------------------------------------------------------------------
sGetKeyFromClient(): Get the key from the client.
------------------------------------------------------------------------------*/
int sGetKeyFromClient(char *zKey)
{
	static char	yMod[] = "sGetKeyFromClient";
	int			yRc;
	char		yKey[100];
	char		yErrorMsg[256];
	long		yBytesRead;
	int			yIndex;
	int			yDone;

	/*
	** Clear all buffers
	*/
	memset(yKey, 0, sizeof(yKey));

	/*
	** Read the header
	*/
	yIndex 		= 0;
	yDone 		= 0;
	while(!yDone)
	{
		if(yIndex > (sizeof(yKey)-1))
		{
			yDone = 1;
			break;
		}

		yRc = saReadSocketData(gAppEnv.fdSocketServer, 5, 0, 1, 1,
								&yBytesRead, &yKey[yIndex], yErrorMsg);
		if(yRc != SA_SUCCESS)
		{
			arcLog(NORMAL,
					"Failed to read key from client, rc=(%d) - (%s)."
					"bytes read so far = (%ld)",
					yRc, yErrorMsg, yBytesRead);
	
			return(-1);
		}

		if(yKey[yIndex] == '~')
		{
			yKey[yIndex] = 0;
			yDone = 1;
			break;
		}

		yIndex++;

	} /* while */

	sprintf(zKey, "%s", yKey);

	return(0);

} // END: sGetKeyFromClient()


