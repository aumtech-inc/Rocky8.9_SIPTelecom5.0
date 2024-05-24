/***********************************************************
 * Program Name:   SRPort.cpp
 * Purpose     :   SRPort definition 
 * Author      :   Aumtech, Inc.
 * Update: 06/26/06 yyq Created the file.
Update: 12/06/2006 djb Added mrcpConnectionIP logic.
Update: 04/09/2007 djb Added getLanguage and initialized language element.
Update: 08/28/2014 djb MR 4241 Added cancelCid routines
 ************************************************************/

#include <iostream>
#include <pthread.h>
#include <mrcpCommon2.hpp>
#include <string>

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <errno.h>
}

using namespace std;
using namespace mrcp2client;


//=============================
// class SdpMedia
//=============================

// --- constructor --- 
SdpMedia::SdpMedia()
{
	resourceType = "";
	channelID = "";
	serverPort = -1;
}

SdpMedia:: SdpMedia(const string& xResourceType, 
                    const string& xChannelID, 
                    const int xServerPort)
{
	resourceType = xResourceType;
	channelID = xChannelID;
	serverPort = xServerPort;
}


SdpMedia::SdpMedia(const SdpMedia& source)
{
	resourceType = source.resourceType;
	channelID = source.channelID;
	serverPort = source.serverPort;
}


SdpMedia& SdpMedia::operator=(const SdpMedia& source)
{
	resourceType = source.resourceType;
	channelID = source.channelID;
	serverPort = source.serverPort;
	return *this;
}


SdpMedia::~SdpMedia() { }


string SdpMedia::getResourceType() {
	return resourceType;
}

string SdpMedia::getChannelID() {
	return channelID;
}

int SdpMedia:: getServerPort() {
	return serverPort;
}


//=============================
// class RtpMedia
//=============================
// --- constructor --- 
RtpMedia::RtpMedia()
{
	port = -1;
	codec = -1;
	payload = -1;
}

RtpMedia::RtpMedia(int xPayload, int xCodec, int xPort)
{
	port = xPort;
	codec = xCodec;
	payload = xPort;
}

RtpMedia::RtpMedia(const RtpMedia &source)
{
	port = source.port;
	codec = source.codec;
	payload = source.payload;
}

RtpMedia& RtpMedia::operator=(const RtpMedia &source)
{
  port = source.port;
  codec = source.codec;
  payload = source.payload;

	return *this;
}


RtpMedia::~RtpMedia() { }


int RtpMedia::getPayLoad()
{
	return payload;
}

int RtpMedia::getCodec()
{
	return codec;
}

int RtpMedia::getServerPort()
{
	return port;
}


//=============================
// class SRPort
//=============================
SRPort::SRPort() {
	pid				= -1;

	initializeElements();

	initializeMutex();
}

SRPort::~SRPort()
{
//	mrcpClient2Log(__FILE__, __LINE__, appPort, "SRPortDestructor",
//		REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"DJB: SRPort Destructor: pid=%d, appPort=%d, requestId=(%s), "
//		"cid=%d, did=%d, channelId=(%s), opCode=%d",
//		pid, appPort, requestId.c_str(), cid, did, channelId.c_str(),
//		opCode);
//

	destroyMutex();
}

void SRPort::initializeElements()
{

	telSRInitCalled	= false;
#if 0
	mrcpClient2Log(__FILE__, __LINE__, appPort, "initializeElements",
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"telSRInitCalled=[%u:%d]", &telSRInitCalled, telSRInitCalled);
#endif

	pid				= -1;
	appPort			= -1;;
	opCode			= -1;
	fdToDynmgr      = -1;
	appResponseFd	= -1;
	appResponseFifo	= "";
//	telSRInitCalled	= false;

	pMrcpThreadId   = 0;
	cid				= -1;
	did				= -1;
	cancelCid		= 0;
	requestId		= "0";

	mrcpConnectionIP	= "";
	socketfd		= -1;
	mrcpPort		= -1;
	serverRtpPort	= -1;

	payload			= "96";
	codec			= "0";

	isRecognitionARecord = false;			// MR 4949
	failOnGrammarLoad	= false;
	grammarAfterVAD		= false;
	sessionRecord		= false;
	sessionPath			= "";
	grammarName 		= "";
	language	 		= "";
	channelId	 		= "";

	//sensitivity			="0.5";
	sensitivity				=	"";
	recognitionMode				=	"";
	confidenceThreshold		=	"0.5";
	speedAccuracy			=	"";
	maxNBest				=	"";
	interdigitTimeout		=	"";
	termTimeout				=	"";
	termChar				=	"";
	incompleteTimeout		=	"";

	licenseReserved		= false;
	licenseQuickFail	= false;	// BT-195
	arePhrasesPlayingOrScheduled	= true;
	stopProcessingRecognition		= false;
//	disconnectReceived				= false;
	exitSentToMrcpThread			= false;
	lastInitTime					= 0;
    inviteSent              = false;

	vecSdpMedia.clear() ;
}

void SRPort::initializeMutex()
{
	pthread_mutex_init(&licenseLock, NULL);
	pthread_mutex_init(&fifoLock, NULL);
	pthread_mutex_init(&exitSentLock, NULL);	// BT-217
	pthread_mutex_init(&appResponseFifoLock, NULL);	// BT-259
}

void SRPort::destroyMutex()
{
	pthread_mutex_destroy(&licenseLock);
	pthread_mutex_destroy(&fifoLock);
	pthread_mutex_destroy(&exitSentLock);	// BT-217
	pthread_mutex_destroy(&appResponseFifoLock);	// BT-259
}

//--- Setters ---
void SRPort::setPid(int xPid) {
	pid  = xPid;
}

void SRPort::setApplicationPort(int xPort) {
	appPort  = xPort;
}

void SRPort::setCid(int xCid) {
	cid = xCid;
}

void SRPort::setDid(int xDid) {
	did = xDid;
}

void SRPort::setCancelCid(int xCancelCid) {
	cancelCid = xCancelCid;
}

void SRPort::setOpCode(int xOpCode) {
	opCode = xOpCode;
}

void SRPort::setAppResponseFifo(string zFifoName)
{
	pthread_mutex_lock(&appResponseFifoLock);
    appResponseFifo = zFifoName;
    appResponseFifo_saved = zFifoName;
	pthread_mutex_unlock(&appResponseFifoLock);
}

void SRPort::setAppResponseFd(int zFd)
{
    appResponseFd = zFd;
}

void SRPort::setFdToDynMgr(int zFdToDynMgr)
{
    fdToDynmgr = zFdToDynMgr;
}

void SRPort::setInviteSent(int xPort, bool xInviteSent)
{
    inviteSent = xInviteSent;
    mrcpClient2Log(__FILE__, __LINE__, xPort, "setInviteSent",
            REPORT_VERBOSE, MRCP_2_BASE, INFO,
            "inviteSent set to %d.", inviteSent);
}

int SRPort::openAppResponseFifo(char *zMsg)
{
	char		lAppResponseFifo[256];

	zMsg[0] = '\0';

	mrcpClient2Log(__FILE__, __LINE__, appPort, "openAppResponseFifo",
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"inside openAppResponseFifo without port number");
	
	if ( appResponseFifo.empty() ) 
	{
		sprintf(zMsg, "%s", "Application Response Fifo is not set.  "
				"Unable to open.  Set it and retry.");
		return(-1);
	}
	
	sprintf(lAppResponseFifo, "%s", appResponseFifo.c_str());
	if(appResponseFd <= 0)
	{
		if ( (mknod(lAppResponseFifo, S_IFIFO | 0666, 0) < 0) &&
		     (errno != EEXIST) )
		{
			sprintf(zMsg, "Creating fifo (%s) failed.  [%d:%s]",
				lAppResponseFifo, errno, strerror(errno));
			return(-1);
		}
	
		if ((appResponseFd = open(lAppResponseFifo, O_RDWR | O_NONBLOCK)) < 0)
		{
			sprintf(zMsg, "Unable to open fifo <%s>.  [%d:%s]",
				lAppResponseFifo, errno, strerror(errno));
			return(-1);
		}
		sprintf(zMsg, "Successfully opened fifo <%s>.  fd=%d.",
				lAppResponseFifo, appResponseFd);
	}
	else
	{
		sprintf(zMsg, "Fifo (%s) was already opened.  fd=%d.", 
				lAppResponseFifo, appResponseFd);
	}

	return(appResponseFd);
} // END: SRPort::openAppResponseFifo

int SRPort::openAppResponseFifo(char *zMsg, int zCall)
{
    char        lAppResponseFifo[256];

    zMsg[0] = '\0';
		
//	mrcpClient2Log(__FILE__, __LINE__, zCall, "openAppResponseFifo",
//			REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"inside openAppResponseFifo with port number");

    if ( appResponseFifo.empty() )
    {
        sprintf(zMsg, "%s", "Application Response Fifo is not set.  "
                "Unable to open.  Set it and retry.");
        return(-1);
    }

    sprintf(lAppResponseFifo, "%s", appResponseFifo.c_str());
    if(appResponseFd <= 0)
    {
        if ( (mknod(lAppResponseFifo, S_IFIFO | 0666, 0) < 0) &&
             (errno != EEXIST) )
        {
            sprintf(zMsg, "Creating fifo (%s) failed.  [%d:%s]",
                lAppResponseFifo, errno, strerror(errno));
            return(-1);
        }

        if ((appResponseFd = open(lAppResponseFifo, O_RDWR | O_NONBLOCK)) < 0)
        {
            sprintf(zMsg, "Unable to open fifo <%s>.  [%d:%s]",
                lAppResponseFifo, errno, strerror);
            return(-1);
        }
        sprintf(zMsg, "Successfully opened fifo <%s>.  fd=%d.",
                lAppResponseFifo, appResponseFd);
    }
    else
    {
        sprintf(zMsg, "Fifo (%s) was already opened.  fd=%d.",
                lAppResponseFifo, appResponseFd);
    }
		
    return(appResponseFd);

} // END: SRPort::openAppResponseFifo

int SRPort::openAppResponseFifo_saved(char *zMsg, int zCall)
{
    char        lAppResponseFifo[256];

    zMsg[0] = '\0';
		
//	mrcpClient2Log(__FILE__, __LINE__, zCall, "openAppResponseFifo_saved",
//			REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"inside openAppResponseFifo with port number");

    if ( appResponseFifo_saved.empty() )
    {
        sprintf(zMsg, "%s", "Application saved Response Fifo is not set.  "
                "Unable to open.  Set it and retry.");
        return(-1);
    }

    sprintf(lAppResponseFifo, "%s", appResponseFifo_saved.c_str());
    if(appResponseFd <= 0)
    {
        if ( (mknod(lAppResponseFifo, S_IFIFO | 0666, 0) < 0) &&
             (errno != EEXIST) )
        {
            sprintf(zMsg, "Creating fifo (%s) failed.  [%d:%s]",
                lAppResponseFifo, errno, strerror(errno));
            return(-1);
        }

        if ((appResponseFd = open(lAppResponseFifo, O_RDWR | O_NONBLOCK)) < 0)
        {
            sprintf(zMsg, "Unable to open fifo <%s>.  [%d:%s]",
                lAppResponseFifo, errno, strerror);
            return(-1);
        }
        sprintf(zMsg, "Successfully opened fifo <%s>.  fd=%d.",
                lAppResponseFifo, appResponseFd);
    }
    else
    {
        sprintf(zMsg, "Fifo (%s) was already opened.  fd=%d.",
                lAppResponseFifo, appResponseFd);
    }
		
    return(appResponseFd);

} // END: SRPort::openAppResponseFifo_saved

int SRPort::closeAppResponseFifo(char *zMod, int zLine, int xPort)
{
	int			rc;

	pthread_mutex_lock( &fifoLock);

	if( appResponseFd > 0)
    {
		rc = close(appResponseFd);
		if ( rc != 0 )
		{
			mrcpClient2Log(__FILE__, __LINE__, xPort, "closeAppResponseFifo",
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Failed to closed appResponseFifo fd(%d) from [%s, %d]. rc=%d [%d, %s]",
				appResponseFd, zMod, zLine, rc, errno, strerror(errno));
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, xPort, "closeAppResponseFifo",
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Closed appResponseFifo fd(%d) from [%s, %d].", appResponseFd, zMod, zLine);
		}

		appResponseFd = -1;
    }
	appResponseFifo = "";

	pthread_mutex_unlock( &fifoLock);

	return(0);

} // END: SRPort::closeAppResponseFifo()

int SRPort::closeSocketFd(int xPort)
{
	int			rc;

	if( socketfd > 0)
    {
		rc = close(socketfd);
		mrcpClient2Log(__FILE__, __LINE__, xPort, "closeSocketFd",
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Closed socket fd(%d).", socketfd);

		socketfd = -1;
    }
	return(0);

} // END: SRPort::closeSocketFd()

void SRPort::eraseCallMapByPort(int xPort)
{
	map<const int, int>::iterator   pos;

	pthread_mutex_lock( &callMapLock);

	for (pos = callMap.begin() ; pos != callMap.end(); ++pos)
    {
		if ( pos->second == xPort )
		{
			callMap.erase(pos);
			break;
		}
    }

	pthread_mutex_unlock( &callMapLock);
	
} // END:  SRPort::eraseCallMapByPort()

void SRPort::setRtpMedia(const RtpMedia& xRtpMedia){
	rtpMedia = xRtpMedia;	
}

void SRPort::setSdpMedia(const vector<SdpMedia>& xVecSdpMedia)
{
	unsigned int indx;
	for (indx = 0; indx < xVecSdpMedia.size(); indx++) {
	  vecSdpMedia.push_back(xVecSdpMedia[indx]); 
	} 
}

void SRPort::setFailOnGrammarLoad(bool xFailOnGrammarLoad){
	failOnGrammarLoad = xFailOnGrammarLoad;
}

void SRPort::setIsGrammarGoogle(bool xIsGrammarGoogle) {
	isGrammarGoogle = xIsGrammarGoogle;
}

void  SRPort::setGrammarAfterVAD(bool xGrammarAfterVAD){
	grammarAfterVAD = xGrammarAfterVAD;
}

void  SRPort::setSessionRecord(bool xSessionRecord){
    sessionRecord = xSessionRecord;
}

void  SRPort::setHideDTMF(bool xHideDTMF){
    hideDTMF = xHideDTMF;
}

void  SRPort::setSessionPath(string xSessionPath){
    sessionPath = xSessionPath;
}

void  SRPort::setRecognitionARecord(bool xRecognitionIsARecord) {	// MR 4949
	isRecognitionARecord = xRecognitionIsARecord;
	mrcpClient2Log(__FILE__, __LINE__, appPort, "setTelSRInitCalled",
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Set isRecognitionARecord to %d [true=%d, false=%d].", isRecognitionARecord, true, false);
}

void  SRPort::setGrammarLoaded(bool xIsLoaded){
	isGrammarLoaded = xIsLoaded;
}

void  SRPort::setGrammarsLoaded(int xGrammarsLoaded){
	numGrammarsLoaded = xGrammarsLoaded;
}

void  SRPort::setGrammarsLoaded(int zGrammarsLoaded, int zIncrement)
{
    numGrammarsLoaded += zIncrement;
}

void  SRPort::setLicenseReserved(bool xLicenseReserved)
{
	pthread_mutex_lock(&licenseLock);
	licenseReserved = xLicenseReserved;
	pthread_mutex_unlock(&licenseLock);
}

void  SRPort::setReserveLicQuickFail(bool xReserveLicQuickFail)	// BT-195
{
	pthread_mutex_lock(&licenseLock);
	licenseQuickFail = xReserveLicQuickFail;
	pthread_mutex_unlock(&licenseLock);
}

void  SRPort::setTelSRInitCalled(bool xRelSRInitCalled)
{
	telSRInitCalled =xRelSRInitCalled;
//	mrcpClient2Log(__FILE__, __LINE__, appPort, "setTelSRInitCalled",
//		REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"telSRInitCalled=[%u:%d]", &telSRInitCalled, telSRInitCalled);
}

void  SRPort::setCallActive(bool xCallActive){
    callActive =xCallActive;
}

void  SRPort::setCleanUpCalled(bool xCleanUpCalled){
    cleanUpCalled =xCleanUpCalled;
}

void  SRPort::setArePhrasesPlayingOrScheduled(bool xPhrasesScheduled){
    arePhrasesPlayingOrScheduled = xPhrasesScheduled;
}

void  SRPort::setStopProcessingRecognition(bool xStopProcessingRecognition){
    stopProcessingRecognition = xStopProcessingRecognition;
}

void  SRPort::setDisconnectReceived(bool xDisconnectReceived){
    disconnectReceived = xDisconnectReceived;
}

void SRPort::setGrammarName(string xGrammarName)
{
	grammarName = xGrammarName;
}

void SRPort::setLanguage(string xLanguage)
{
	language = xLanguage;
}

void SRPort::setSensitivity(string xLevel)
{
	sensitivity = xLevel;
}

void SRPort::setRecognitionMode(string xLevel)
{
	recognitionMode = xLevel;
}

void SRPort::setInterdigitTimeout(string xInterdigitTimeout)
{
	interdigitTimeout = xInterdigitTimeout;
}

void SRPort::setIncompleteTimeout(string xIncompleteTimeout)
{
	incompleteTimeout = xIncompleteTimeout;
}
void SRPort::setTermTimeout(string xTermTimeout)
{
	termTimeout = xTermTimeout;
}
void SRPort::setTermChar(string xTermChar)
{
	termChar = xTermChar;
}

void SRPort::setConfidenceThreshold(string xConfidenceThreshold)
{
	confidenceThreshold = xConfidenceThreshold;
}

void SRPort::setSpeedAccuracy(string xSpeedAccuracy)
{
	speedAccuracy = xSpeedAccuracy;
}

void SRPort::setMaxNBest(string xMaxNBest)
{
	maxNBest = xMaxNBest;
}

void SRPort::setMrcpThreadId(pthread_t xId)
{
	pMrcpThreadId = xId;
}

void SRPort::setMrcpConnectionIP(string xMrcpConnectionIP)
{
	mrcpConnectionIP = xMrcpConnectionIP;
}

void SRPort::setSocketfd(int xPort, int xSocketfd)
{
	socketfd = xSocketfd;
//	mrcpClient2Log(__FILE__, __LINE__, xPort, "closeSocketFd",
//			REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"Set socket fd to %d.", socketfd);
}

void SRPort::setMrcpPort(int xMrcpPort)
{
	mrcpPort = xMrcpPort;
}

void SRPort::setServerRtpPort(int xServerRtpPort)
{
	serverRtpPort = xServerRtpPort;
}

void SRPort::setPayload(string xPayload)
{
	payload = xPayload;
}

void SRPort::setCodec(string xCodec)
{
	codec = xCodec;
}

void SRPort::setChannelId(string xChannelId)
{
	channelId = xChannelId;
}

void SRPort::setRequestId(string xRequestId)
{
	requestId = xRequestId;

//	mrcpClient2Log(__FILE__, __LINE__, appPort, "setRequestId",
//		REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"DJB: Setting requestId (%s) from (%s)",
//		requestId.c_str(), xRequestId.c_str());
}

void SRPort::setServerIP(string xServerIP)
{
    serverIP = xServerIP;
}

void SRPort::setExitSentToMrcpThread(int zPort, bool xExit)
{
	pthread_mutex_lock(&exitSentLock);		// BT-217
	exitSentToMrcpThread = xExit;
	pthread_mutex_unlock(&exitSentLock);
}

void SRPort::incrementRequestId()
{
	int		tmpInt;
	char	tmpChar[16];
	
	tmpInt = atoi(requestId.c_str());

	sprintf(tmpChar, "%d", ++tmpInt);

	requestId = string(tmpChar);
}

void SRPort::setRecognizeResponse(struct MsgToApp *zMsgToApp)
{
	memcpy(&recognizeResponse, zMsgToApp, sizeof(recognizeResponse));
}

void SRPort::clearRecognizeResponse()
{
	memset((struct MsgToApp *)&recognizeResponse, '\0',
				sizeof(recognizeResponse));
}

void SRPort::copyOutOfSyncEvent(OutOfSyncEvent *zOutOfSyncEvent)
{
	zOutOfSyncEvent->statusCode = outOfSyncEvent.statusCode;
	zOutOfSyncEvent->completionCause = outOfSyncEvent.completionCause;
	sprintf(zOutOfSyncEvent->eventStr,		"%s", outOfSyncEvent.eventStr);
	sprintf(zOutOfSyncEvent->serverState,		"%s", outOfSyncEvent.serverState);
	sprintf(zOutOfSyncEvent->content,			"%s", outOfSyncEvent.content);

	/* = operator is overloaded */
	zOutOfSyncEvent->mrcpV2Event = outOfSyncEvent.mrcpV2Event;
}

void SRPort::setOutOfSyncEvent(OutOfSyncEvent *zOutOfSyncEvent)
{
	outOfSyncEvent.statusCode			= zOutOfSyncEvent->statusCode;
	outOfSyncEvent.completionCause		= zOutOfSyncEvent->completionCause;
	sprintf(outOfSyncEvent.eventStr,		"%s", zOutOfSyncEvent->eventStr);
	sprintf(outOfSyncEvent.serverState,		"%s", zOutOfSyncEvent->serverState);
	sprintf(outOfSyncEvent.content,			"%s", zOutOfSyncEvent->content);

	/* = operator is overloaded */
	outOfSyncEvent.mrcpV2Event = zOutOfSyncEvent->mrcpV2Event;
}

void SRPort::clearOutOfSyncEvent()
{

#if 0
	outOfSyncEvent.statusCode			= zOutOfSyncEvent->statusCode;
	outOfSyncEvent.completionCause		= zOutOfSyncEvent->completionCause;
	sprintf(outOfSyncEvent.eventStr,		"%s", zOutOfSyncEvent->eventStr);
	sprintf(outOfSyncEvent.serverState,		"%s", zOutOfSyncEvent->serverState);
	sprintf(outOfSyncEvent.content,			"%s", zOutOfSyncEvent->content);

	outOfSyncEvent.mrcpV2Event = zOutOfSyncEvent->mrcpV2Event;
#endif

	outOfSyncEvent.statusCode			= 0;
	outOfSyncEvent.completionCause		= 0;
	sprintf(outOfSyncEvent.eventStr,		"%s", "");
	sprintf(outOfSyncEvent.serverState,		"%s", "");
	sprintf(outOfSyncEvent.content,			"%s", "");
}

void SRPort::setOutOfSyncEventReceived(int zOutOfSyncEventReceived)
{
	outOfSyncEventReceived = zOutOfSyncEventReceived;
}

//  --- Isxxx ---

bool SRPort::IsRecognitionARecord() const {		// MR 4949
	return isRecognitionARecord;
}

bool SRPort::IsOutOfSyncEventReceived() const {
	return outOfSyncEventReceived;
}

bool SRPort::IsGrammarGoogle() const {
	return isGrammarGoogle;
};

bool SRPort::IsFailOnGrammarLoad() const {
	return failOnGrammarLoad;
};

bool SRPort::IsGrammarAfterVAD() const {
	return grammarAfterVAD;
}

bool SRPort::IsSessionRecord() const {
	return sessionRecord;
}

bool SRPort::IsLicenseReserved()
{
	bool	tmpLicenseReserved;

	pthread_mutex_lock(&licenseLock);
	tmpLicenseReserved = licenseReserved;
	pthread_mutex_unlock(&licenseLock);

	return tmpLicenseReserved;
}

bool SRPort::DidReserveLicQuickFail()	// BT-195
{
	bool	tmpDidReserveLicQuickFail;

	pthread_mutex_lock(&licenseLock);
	tmpDidReserveLicQuickFail = licenseQuickFail;
	pthread_mutex_unlock(&licenseLock);

	return tmpDidReserveLicQuickFail;
}

bool SRPort::IsTelSRInitCalled() const {
//	mrcpClient2Log(__FILE__, __LINE__, appPort, "IsTelSRInitCalled",
//		REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"returning telSRInitCalled=[%u:%d]", &telSRInitCalled, telSRInitCalled);
	return telSRInitCalled; 
}

bool SRPort::IsCallActive() const {
    return callActive;
}

bool SRPort::IsCleanUpCalled() const {
    return cleanUpCalled;
}

bool SRPort::IsPhrasesPlayingOrScheduled() const {
    return arePhrasesPlayingOrScheduled;
}

bool SRPort::IsInviteSent() const {
    return inviteSent;
}

bool SRPort::IsStopProcessingRecognition() const {
    return stopProcessingRecognition;
}

bool SRPort::IsDisconnectReceived() const {
    return disconnectReceived;
}

bool SRPort::IsFdToDynmgrOpen() const {
    if ( fdToDynmgr > 0 )
    {
        return(true);
    }
    else
    {
        return(false);
    }
}

bool SRPort::IsExitSentToMrcpThread(int zPort) const
{
	pthread_mutex_lock(( pthread_mutex_t *) &exitSentLock);			// BT-217
    if ( exitSentToMrcpThread > 0 )
    {
		pthread_mutex_unlock(( pthread_mutex_t *) &exitSentLock);
        return(true);
    }

	pthread_mutex_unlock(( pthread_mutex_t *) &exitSentLock);
	return(false);
}

//----- Getters -----
int SRPort::getPid() const {
	return pid;
}

pthread_t SRPort::getMrcpThreadId() {
	return pMrcpThreadId;
}

int SRPort::getApplicationPort() const {
	return appPort;
}

int  SRPort::getCid() const {
	return cid;
}

int  SRPort::getDid() const {
	return did;
}

int  SRPort::getCancelCid() const {
	return cancelCid;
}
int  SRPort::getOpCode() const {
	return opCode;
}

struct MsgToApp *SRPort::getRecognizeResponse() {
	return &recognizeResponse;
}

OutOfSyncEvent *SRPort::getOutOfSyncEvent()
{
	return &outOfSyncEvent;
}

int SRPort::getOutOfSyncEventReceived() const {
	return outOfSyncEventReceived;
}

void SRPort::getAppResponseFifo(string &zAppResponseFifo) 
{
	pthread_mutex_lock(&appResponseFifoLock);
    zAppResponseFifo = appResponseFifo;
	pthread_mutex_unlock(&appResponseFifoLock);
}

string  SRPort::getAppResponseFifo_saved() const
{
    return appResponseFifo_saved;
}

int  SRPort::getAppResponseFd() const
{
	return appResponseFd;
}

int  SRPort::getFdToDynMgr() const
{
    return fdToDynmgr;
}

string  SRPort::getGrammarName() const
{
    return grammarName;
}

string  SRPort::getSessionPath() const
{
    return sessionPath;
}

bool  SRPort::getHideDTMF(){
    return hideDTMF;
}

string	SRPort::getLanguage() const
{
	return language;
}

string	SRPort::getSensitivity() const
{
	return sensitivity;
}

string	SRPort::getRecognitionMode() const
{
	return recognitionMode;
}

string	SRPort::getSpeedAccuracy() const
{
	return speedAccuracy;
}

string	SRPort::getMaxNBest() const
{
	return maxNBest;
}

string	SRPort::getInterdigitTimeout() const
{
	return interdigitTimeout;
}

string	SRPort::getIncompleteTimeout() const
{
	return incompleteTimeout;
}

string	SRPort::getTermTimeout() const
{
	return termTimeout;
}
string	SRPort::getTermChar() const
{
	return termChar;
}
string	SRPort::getConfidenceThreshold() const
{
	return confidenceThreshold;
}

string	SRPort::getMrcpConnectionIP() const
{
    return mrcpConnectionIP;
}

int  SRPort::getSocketfd() const {
	return socketfd;
}

int  SRPort::getMrcpPort() const {
	return mrcpPort;
}

int  SRPort::getServerRtpPort() const {
	return serverRtpPort;
}

string SRPort::getPayload() const {
	return payload;
}

string SRPort::getCodec() const {
	return codec;
}

string SRPort::getChannelId() const {
	return channelId;
}

string  SRPort::getRequestId() const {
//	mrcpClient2Log(__FILE__, __LINE__, appPort, "getRequestId",
//		REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"DJB: Returning requestId (%s)",requestId.c_str());

	return requestId;
}

string  SRPort::getServerIP() const
{
    return serverIP;
}



