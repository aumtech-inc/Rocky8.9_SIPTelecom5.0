/*------------------------------------------------------------------------------
Program: mrcpClient2Threads.hpp
Purpose: Common includes, defines, globals, and prototypes.
Author:	Aumtech, Inc.
Update:	06/16/2006  djb Created the file.
------------------------------------------------------------------------------*/

#ifndef MRCPTHREAD_HPP
#define MRCPTHREAD_HPP

#include "mrcp2_HeaderList.hpp"
#include <sys/timeb.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>

#include "mrcpCommon2.hpp"
#include "mrcpTTS.hpp"
#include "mrcp2_Request.hpp"
#include "mrcp2_Response.hpp"
#include "mrcp2_Event.hpp"

using namespace mrcp2client;

extern "C"
{
    #include "Telecom.h"
	#include "AppMessages_mrcpTTS.h"
}

//static struct MrcpPortInfo      mrcpPortInfo;
static struct Msg_SRRecognize   gLast_SRRecognize[MAX_PORTS];

static void     *mrcpThreadFunc(void*);

static int		processTtsReserveResource(ARC_TTS_REQUEST_SINGLE_DM
							*pAppRequest);
static int		processTtsReleaseResource(int zPort, ARC_TTS_REQUEST_SINGLE_DM
							*pAppRequest);
static int		processSpeakTTS(ARC_TTS_REQUEST_SINGLE_DM *pAppRequest);
static int      build_tcp_connection(const string& serverAddr,
                                    const int serverPort);

void	sendSpeechDetected(int zPort);
int		stopBargeInPhrase(int zPort);

//int		sendSpeakMRCPToDM(int zPort, char *zpServerIP, int zRtpPort,
//				int zRtcpPort, char *zFile, int zPayloadType,
//				int zCodecType);


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
