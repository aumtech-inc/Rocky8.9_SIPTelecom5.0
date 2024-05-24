/*------------------------------------------------------------------------------
File:		processRequests.c

Purpose:	This routine contains the main client request processing loop.

Author:		Sandeep Agate, Aumtech Inc.

Update:		2002/05/23 sja 	Created the file.
------------------------------------------------------------------------------*/

/*
** Include Files
*/
#include <arcLicenseClient.h>

/*
** Static Function Prototypes
*/
int sSendKeyToServer(char *zKey);
int sGetKeyFromServer(char *zKey);

/*------------------------------------------------------------------------------
processRequests(): Send requests to server for processing.
------------------------------------------------------------------------------*/
int processRequests(void)
{
	static char	yMod[] 			= "main";
	char		yErrorMsg[512];
	int			yRc;
	int			yDone;
	int			yRequestCount;
	char		yKey[100];
	char		yEncryptedKey[512];
	char		yDecryptedKey[100];
	struct sockaddr_in	yServerAddr;

	srand(getpid());

	yRequestCount 	= 1;
	yDone 			= 0;

	while( ! yDone)
	{
		if(!(yRequestCount % 10))
		{
			break;
		}
		
		/*
		** Initialize the socket connection
		*/
		if(saClientInit(gAppEnv.serverName, gAppEnv.serverPort, 
					&gAppEnv.fdSocketClient, &yServerAddr, yErrorMsg) < 0)
		{
			arcLog(NORMAL,
					"Failed to initialize server socket connection (%s).",
					yErrorMsg);
		
			return(-1);
		}
		gAppEnv.fIsSocketConnected = 1;

		/*
		** Send the key to the client
		*/
		sprintf(yKey, "%d%d~", getpid(), rand());
		// sprintf(yKey, "%d~", getpid());
		if(sSendKeyToServer(yKey) < 0)
		{
			return(-1); // error msg. written in sub-routine
		}
		yKey[strlen(yKey)-1] = 0;

		/*
		** Read the key from the server.
		*/
		if(sGetKeyFromServer(yEncryptedKey) < 0)
		{
			return(-1); // error msg. written in sub-routine
		}

		/*
		** Decrypt the key
		*/
		arcDecrypt(yEncryptedKey, yDecryptedKey);

		if(strcmp(yKey, yDecryptedKey) != 0)
		{
			arcLog(NORMAL,
					"Failed to match key (%d:%s) with decrypted key (%d:%s), "
					"encrypted key = (%d:%s)",
					strlen(yKey), yKey, strlen(yDecryptedKey), yDecryptedKey,
					strlen(yEncryptedKey), yEncryptedKey);

			return(-1);
		}
		else
		{
			arcLog(NORMAL,
					"Found match for key (%d:%s) with decrypted key (%d:%s), "
					"encrypted key = (%d:%s)",
					strlen(yKey), yKey, strlen(yDecryptedKey), yDecryptedKey,
					strlen(yEncryptedKey), yEncryptedKey);
		}

		if(saExit(gAppEnv.fdSocketClient, yErrorMsg) < 0)
		{
			arcLog(NORMAL,
					"Failed to shutdown client socket connection, (%s)",
					yErrorMsg);
		
			// don't return here
		}
		gAppEnv.fIsSocketConnected = 0;

		// sleep(1);

		yRequestCount++;

	} // while !yDone

	return(0);

} /* END: processRequests() */

/*------------------------------------------------------------------------------
sSendKeyToServer(): Get the key from the server.
------------------------------------------------------------------------------*/
int sSendKeyToServer(char *zKey)
{
	static char	yMod[] = "sSendKeyToServer";
	int			yRc;
	char		yErrorMsg[256];
	long		yBytesWritten;


	if((yRc = saWriteSocketData(gAppEnv.fdSocketClient, strlen(zKey), zKey, 
								&yBytesWritten, yErrorMsg)) < 0)
	{
		arcLog(NORMAL,
				"Failed to send (%ld) bytes of encrypted key to server, "
				"rc=(%d) - (%s).",
				strlen(zKey), yRc, yErrorMsg);

		return(-1);
	}

	return(0);

} // END: sSendKeyToServer()

/*------------------------------------------------------------------------------
sGetKeyFromServer(): Get the key from the server.
------------------------------------------------------------------------------*/
int sGetKeyFromServer(char *zKey)
{
	static char	yMod[] = "sGetKeyFromServer";
	int			yRc;
	char		yEncryptedKey[512];
	char		yErrorMsg[256];
	long		yBytesRead;
	int			yIndex;
	int			yDone;

	/*
	** Clear all buffers
	*/
	memset(yEncryptedKey, 0, sizeof(yEncryptedKey));

	/*
	** Read the header
	*/
	yIndex 		= 0;
	yDone 		= 0;
	while(!yDone)
	{
		if(yIndex > (sizeof(yEncryptedKey)-1))
		{
			yDone = 1;
			break;
		}

		yRc = saReadSocketData(gAppEnv.fdSocketClient, 5, 0, 1, 1,
								&yBytesRead, &yEncryptedKey[yIndex], yErrorMsg);
		if(yRc != SA_SUCCESS)
		{
			arcLog(NORMAL,
					"Failed to read key from server, rc=(%d) - (%s)."
					"bytes read so far = (%ld)",
					yRc, yErrorMsg, yBytesRead);
	
			return(-1);
		}

		if(yEncryptedKey[yIndex] == '~')
		{
			yEncryptedKey[yIndex] = 0;
			yDone = 1;
			break;
		}

		yIndex++;

	} /* while */

	sprintf(zKey, "%s", yEncryptedKey);

	return(0);

} // END: sGetKeyFromServer()


