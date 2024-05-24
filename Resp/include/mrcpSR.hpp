/* -----------------------------------------------------------------------------
Program:	mrcpSR.hpp
Purpose:	Common includes, defines, globals, and prototypes mrcp2.0 apis
Author :	Aumtech Inc.
UpDate : 	06/27/06	djb	Created the file.
------------------------------------------------------------------------------*/
#include <string>
#ifndef MRCP_SR_HPP
#define MRCP_SR_HPP

#include "arcSR.h"
#include "mrcp2_Event.hpp"

using namespace std;

const int		YES_EVENT_RECEIVED	= 1;

const int		READ_EVENT			= 1;
const int		READ_RESPONSE		= 2;

/* -----------------------------------------------------------------------------
Function Prototypes
------------------------------------------------------------------------------*/
int SRInit(int zPort);
int SRSetParameter(int zPort, char *zParameterName, char *zParameterValue);

int SRLoadGrammar(int zPort, int zGrammarType, const string &zGrammar, float weight);
int SRUnloadGrammars(int zPort);

int SRRecognizeV2(int zPort, int zAppPid, SRRecognizeParams *zParams, char *);
int SRReleaseResource(const int zPort);
int SRReserveResource(int zPort);
int SRGetResult(int zPort, int zAlternativeNumber, int zFutureUse,
					char *zDelimiter, char *zResult, int *zOverallConfidence);
int SRExit(int zPort);
int SRGetParameter(int zPort, char *zParameterName, char *zParameterValue);
int SRGetXMLResult(int zPort, char *zXMLResultFile);
int SRReleaseResource(int zPort, int *zVendorCode);
int SRUnloadGrammars(int zPort, int *zVendorCode);
int SRUnloadGrammar(int zPort, char *zGrammarName); // VAD only


// Common routines.

int processMRCPRequest(int zPort, string & zSendMsg, string & zRecvMsg,
			string & zServerState, int *zStatusCode, int *zCauseCode);
int	readMrcpSocket(int zPort, int zReadType, string & zRecvMsg,
            string & zEventName, string & zServerState, Mrcp2_Event  & mrcpEvent,
			int *zStatusCode, int *zCauseCode);
int srUnLoadGrammar(int zPort, const char* zGrammarName);
int srUnLoadGrammars(int zPort);
int srLoadGrammar(int zPort, int zGrammarType,  const char *zGrammarName,
            const char *zGrammar, int zLoadNow, float zWeight);
int srGetResource(const int appPort);
int srReleaseResource(const int callNum);
int	cleanUp(int zPort);

int myGetProfileString(char *section,
                    char *key, char *defaultVal, char *value,
                    long bufSize, FILE *fp, char *file);
int sendRequestToDynMgr(int zPort, char *mod, struct MsgToDM *request);


#endif
