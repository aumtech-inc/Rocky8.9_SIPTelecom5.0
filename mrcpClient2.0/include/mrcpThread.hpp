/*------------------------------------------------------------------------------
Program: mrcpClient2Threads.hpp
Purpose: Common includes, defines, globals, and prototypes.
Author:	Aumtech, Inc.
Update:	06/16/2006 djb Created the file.
Update: 01/03/2007 djb Added processSrGetParameter().
------------------------------------------------------------------------------*/

#ifndef MRCPTHREAD_HPP
#define MRCPTHREAD_HPP

#include "mrcp2_HeaderList.hpp"
#include <sys/timeb.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>

#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"
#include "mrcp2_Request.hpp"
#include "mrcp2_Response.hpp"
#include "mrcp2_Event.hpp"

using namespace mrcp2client;

extern "C"
{
    #include "Telecom.h"
}

//static struct MrcpPortInfo      mrcpPortInfo;
static struct Msg_SRRecognize		gLast_SRRecognize[MAX_PORTS];
static struct Msg_SRReleaseResource	gLast_SRReleaseResource[MAX_PORTS];

static void     *mrcpThreadFunc(void*);


static void		processSrInit(MsgToDM *pAppRequest);
static void     processSrLoadGrammar(MsgToDM *pAppRequest);
static void     processSrRecognizeV2(MsgToDM *pAppRequest);
static void     processSrSetParameter(MsgToDM *pAppRequest);
static void     processSrGetParameter(MsgToDM *pAppRequest);
static void		processSrReserveResource(MsgToDM *pAppRequest);
static void     processSrReleaseResource(MsgToDM *pAppRequest);
static void		processSrGetResult(MsgToDM *pAppRequest);
static void 	processSrGetXMLResult(MsgToDM *pAppRequest);
static void     processSrUnloadGrammars(MsgToDM *pAppRequest);
void     processSrUnloadGrammar(MsgToDM *pAppRequest);
static void     processSrExit(MsgToDM *pAppRequest);
static void		processDmopDisconnect(int zPort, MsgToDM *pAppRequest);

static int      build_tcp_connection(const string& serverAddr,
                                    const int serverPort);

void	sendSpeechDetected(int zPort);
int		stopBargeInPhrase(int zPort);
int		stopCollectingSpeech(int zPort);
int		sendRecognizeToDM(int zPort, char *zpServerIP, int zRtpPort,
                int zRtcpPort, char *zUtteranceFile, int zPayloadType,
                int zCodecType);
int		sendReleaseResourceToDM(int zPort);


using namespace std;
using namespace mrcp2client;





//=================
class MrcpThread
//=================
{
public:

	MrcpThread();
	~MrcpThread();

	int start(int zPort);

private:
	pthread_t 		mrcpThreadId;

	MrcpThread(const MrcpThread& src);
	MrcpThread& operator=(const MrcpThread& src);
};

#endif
