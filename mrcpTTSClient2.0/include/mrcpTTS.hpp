/* -----------------------------------------------------------------------------
Program:	mrcpTTS.hpp
Purpose:	Common includes, defines, globals, and prototypes mrcp2.0 apis
Author :	Aumtech Inc.
UpDate : 	06/27/06	djb	Created the file.
------------------------------------------------------------------------------*/
#include <string>
#ifndef MRCP_TTS_HPP
#define MRCP_TTS_HPP

#include "arcSR.h"

using namespace std;

const int		YES_EVENT_RECEIVED	= 1;

const int		READ_EVENT			= 1;
const int		READ_RESPONSE		= 2;

/* -----------------------------------------------------------------------------
Function Prototypes
------------------------------------------------------------------------------*/
int TTSSpeak(int zPort, ARC_TTS_REQUEST_SINGLE_DM *zParams);
int TTSGetParams(int zPort, char *zParameterName, char *zParameterValue);


// Common routines.

int processMRCPRequest(int zPort, string & zSendMsg, string & zRecvMsg,
			string & zServerState, int *zStatusCode, int *zCauseCode,
			string & zEventStr);
int	readMrcpSocket(int zPort, int zReadType, string & zRecvMsg,
            string & zEventName, string & zServerState,
			int *zStatusCode, int *zCauseCode);

int ttsGetResource(const int appPort);
int ttsReleaseResource(const int callNum);
int	cleanUp(int zPort);

int myGetProfileString(char *section,
                    char *key, char *defaultVal, char *value,
                    long bufSize, FILE *fp, char *file);
int sendRequestToDynMgr(int zPort, char *mod, struct MsgToDM *request);


#endif
