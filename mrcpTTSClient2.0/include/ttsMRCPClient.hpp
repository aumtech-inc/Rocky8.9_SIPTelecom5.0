/*------------------------------------------------------------------------------ File:		ttsMRCPClient.hpp
Purpose:	Contains all header information needed to integrate MRCP TTS.

			This was originally ttsClient.h

Author:		Aumtech, Inc

Update: 111/28/2006	djb Created the file
------------------------------------------------------------------------------*/
#ifndef ttsMRCPClient_HPP__		/* Just preventing multiple includes */
#define ttsMRCPClient_HPP__

//#define PIPE_READ_TIMEOUT 			120
#define PIPE_READ_TIMEOUT 			2
#define TOTAL_NUMBER_OF_TRIES 		5
#define INITIAL_TIMEOUT_VALUE 		10
#define TIMEOUT_VALUE 				10
#define CONNECT_FAIL_SLEEP_TIME		60

#ifdef NEED_GLOBALS
#else
#endif

#if 0

/* Extern Variables */
extern int	errno;

/* Function Prototypes */
int connect2TtsServer(int *connected);
int disconnect(int *connected);
int exitApp(int fd, int exitCode);
int getNextRequest(int *connected, int fd, 
		ARC_TTS_REQUEST_SINGLE_DM *ttsRequestSingleDM);
int getResult(int *connected, ARC_TTS_RESULT *ttsResult, char *tmpFile, int ttsTimeOut, time_t zStartTime, char * appPid);
int getTtsHost(char *ttsHost);
int initialize(int *fd);
int sendRequest(int *connected, const ARC_TTS_REQUEST *ttsRequest);
int sendResult(ARC_TTS_RESULT *ttsResult);
#endif

#endif /* ttsClient_H__ */
