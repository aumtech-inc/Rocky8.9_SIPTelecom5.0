/***********************************************************
* Author      :   Aumtech, Inc.
 * Update: 06/26/06 yyq Created the file.
 * Update: 10/26/2014 djb MR 4241; added cancelCid logic
 ************************************************************/

#include <iostream>
#include <pthread.h>
#include <mrcpCommon2.hpp>
#include <mrcpTTS.hpp>

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <sys/time.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <errno.h>
	#include "ttsStruct.h"
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
	requestQueueLockInitialized = false;

	initializeElements();
}

SRPort::~SRPort() { }

void SRPort::initializeElements()
{
	struct timeval  tp;
	struct timezone tzp;
	struct tm   *tm;
	char		buf[128];

	gettimeofday(&tp, &tzp);
    tm = localtime(&tp.tv_sec);
	sprintf(buf, "%02d:%02d:%04d %02d:%02d:%02d:%03d",
            tm->tm_mon + 1, tm->tm_mday, tm->tm_year+1900, tm->tm_hour,
            tm->tm_min, tm->tm_sec, tp.tv_usec / 1000);

	pid				= -1;
	appPort			= -1;;
	opCode			= -1;
	fdToDynmgr      = -1;
	closeSession	= 0;
	appResponseFd	= -1;
	appResponseFifo	= "";
	telSRInitCalled	= false;

	cid				= -99;
	did				= -1;
	cancelCid		= 0;
	requestId		= "0";

	socketfd		= -1;
	mrcpPort		= -1;
	rtpPort			= -1;

	payload			= "96";
	codec			= "0";

	failOnGrammarLoad	= false;
	grammarAfterVAD		= false;
	sessionRecord		= false;
	sessionPath			= "";
	grammarName 		= "";

	isInProgressRecv = true;

	licenseReserved			= false;
	licenseRequestFailed	= false;
	disconnectReceived		= false;
	arePhrasesPlayingOrScheduled	= true;
	stopProcessingTTS		= false;
	inviteSent				= false;

	disconnectTimeInMilliseconds = -1;

	memset((ARC_TTS_RESULT *)&ttsResult, '\0', sizeof(ttsResult));
	clearTTSRequests();

	vecSdpMedia.clear() ;
}

void SRPort::initializeMutex()
{
	pthread_mutex_init(&licenseLock, NULL);
}

void SRPort::destroyMutex()
{
	pthread_mutex_destroy(&licenseLock);
}

//--- Setters ---
void SRPort::setPid(int xPid) {
	pid  = xPid;
}

void SRPort::setApplicationPort(int xPort) {
	appPort  = xPort;
}

void SRPort::setCid(int xPort, int xCid) {
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
    appResponseFifo = zFifoName;
    appResponseFifo_saved = zFifoName;
}

void SRPort::setCloseSession(int xCloseSession) {
	closeSession  = xCloseSession;
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
	
		if ((appResponseFd = open(lAppResponseFifo, O_RDWR)) < 0)
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

//  mrcpClient2Log(__FILE__, __LINE__, zCall, "openAppResponseFifo_saved",
//          REPORT_VERBOSE, MRCP_2_BASE, INFO,
//          "inside openAppResponseFifo with port number");

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

int SRPort::closeAppResponseFifo(int xPort)
{
	int			rc;

	if( appResponseFd > 0)
    {
		rc = close(appResponseFd);
		appResponseFd = -1;
        mrcpClient2Log(__FILE__, __LINE__, xPort, "closeAppResponseFifo",
            REPORT_VERBOSE, MRCP_2_BASE, INFO,
            "Closed appResponseFifo fd(%d).", appResponseFd);
    }
//	appResponseFifo = "";
	return(0);

} // END: SRPort::closeAppResponseFifo()

void SRPort::eraseCallMapByPort(int xPort)
{
	map<const int, int>::iterator   pos;

	pthread_mutex_lock(&callMapLock);
	for (pos = callMap.begin() ; pos != callMap.end(); ++pos)
    {
		if ( pos->second == xPort )
		{
			callMap.erase(pos);
			break;
		}
    }
	pthread_mutex_unlock(&callMapLock);
	
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

void  SRPort::setGrammarAfterVAD(bool xGrammarAfterVAD){
	grammarAfterVAD = xGrammarAfterVAD;
}

void  SRPort::setSessionRecord(bool xSessionRecord){
    sessionRecord = xSessionRecord;
}

void  SRPort::setSessionPath(string xSessionPath){
    sessionPath = xSessionPath;
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
	struct timeval  tp;
	struct timezone tzp;
	struct tm   *tm;
	char		buf[128];

	gettimeofday(&tp, &tzp);
    tm = localtime(&tp.tv_sec);
	sprintf(buf, "%02d:%02d:%04d %02d:%02d:%02d:%03d",
            tm->tm_mon + 1, tm->tm_mday, tm->tm_year+1900, tm->tm_hour,
            tm->tm_min, tm->tm_sec, tp.tv_usec / 1000);

	pthread_mutex_lock(&licenseLock);
	licenseReserved = xLicenseReserved;
	mrcpClient2Log(__FILE__, __LINE__, appPort, "SRPort::setLicenseReserved",
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"licenseReserved is set to %d", licenseReserved);
	pthread_mutex_unlock(&licenseLock);
}

void  SRPort::setLicenseRequestFailed(bool xLicenseRequestFailed)
{
	licenseRequestFailed = xLicenseRequestFailed;
}


void  SRPort::setTelSRInitCalled(bool xRelSRInitCalled){
	telSRInitCalled =xRelSRInitCalled;
}

void  SRPort::setCallActive(bool xCallActive){
    callActive =xCallActive;
}

void  SRPort::setArePhrasesPlayingOrScheduled(bool xPhrasesScheduled){
    arePhrasesPlayingOrScheduled = xPhrasesScheduled;
}

void  SRPort::setStopProcessingTTS(bool xStopProcessingTTS){
    stopProcessingTTS = xStopProcessingTTS;

	mrcpClient2Log(__FILE__, __LINE__, appPort, "SRPort::setStopProcessingTTS",
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Set stopProcessingTTS to %d.", stopProcessingTTS);
}

void  SRPort::setDisconnectReceived(bool xDisconnectReceived){
    struct timeval te;

    disconnectReceived = xDisconnectReceived;

    if ( disconnectReceived )
    {
        gettimeofday(&te, NULL); // get current time
        disconnectTimeInMilliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // caculate milliseconds

        mrcpClient2Log(__FILE__, __LINE__, appPort, "SRPort::setDisconnectReceived", REPORT_VERBOSE, MRCP_2_BASE, INFO,
         "Set disconnectReceived to %d; time in milliseconds=%ld", disconnectReceived, disconnectTimeInMilliseconds);
    }
	else
	{
		mrcpClient2Log(__FILE__, __LINE__, appPort, "SRPort::setDisconnectReceived", REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Set disconnectReceived to %d", disconnectReceived);
	}
}


void SRPort::setGrammarName(string xGrammarName)
{
	grammarName = xGrammarName;
}

void SRPort::setMrcpThreadId(pthread_t xId)
{
	pMrcpThreadId = xId;
}

void SRPort::setSavedMrcpThreadId(pthread_t xId)
{
	pSavedMrcpThreadId = xId;
}

void SRPort::setSocketfd(int xPort, int xSocketfd)
{
	socketfd = xSocketfd;
}

void SRPort::setMrcpPort(int xMrcpPort)
{
	mrcpPort = xMrcpPort;
}

void SRPort::setRtpPort(int xRtpPort)
{
	rtpPort = xRtpPort;
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
}

void SRPort::setServerIP(string xServerIP)
{
	serverIP = xServerIP;
}

void SRPort::setTTSFileName(char *zFileName)
{
	sprintf(ttsResult.fileName, "%s", zFileName);
}

void SRPort::setTTSPort(char *zPort)
{
	sprintf(ttsResult.port, "%s", zPort);
}

void SRPort::setTTSPid(char *zPid)
{
	sprintf(ttsResult.pid, "%s", zPid);
}

void SRPort::setTTSResultFifo(char *zResultFifo)
{
	sprintf(ttsResult.resultFifo, "%s", zResultFifo);
}

void SRPort::setTTSResultCode(char *zResultCode)
{
	sprintf(ttsResult.resultCode, "%s", zResultCode);
}

void SRPort::setTTSError(char *zError)
{
	sprintf(ttsResult.error, "%s", zError);
}

void SRPort::setTTSSpeechFile(char *zSpeechFile)
{
	sprintf(ttsResult.speechFile, "%s", zSpeechFile);
}

void SRPort::setRequestQueueLockInitialized(bool xRequestQueueLockInitialized){
	requestQueueLockInitialized = xRequestQueueLockInitialized;
}

int SRPort::addTTSRequest(long zId, 
				ARC_TTS_REQUEST_SINGLE_DM *zTtsRequest)
{
    struct TtsInfo  *pTmpTtsRequestQ;
	static char mod[] = "addTTSRequest";

    if ((pTmpTtsRequestQ = (struct TtsInfo *)
                        calloc(1, sizeof(struct TtsInfo))) == NULL)
	{
		mrcpClient2Log(__FILE__, __LINE__, appPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
			"Unable to allocate memory, failed to app TTS request to "
			"queue.  [%d, %s]", errno, strerror(errno));

		return(-1);
	}

	pTmpTtsRequestQ->id = zId;

	memcpy(( ARC_TTS_REQUEST_SINGLE_DM *) &pTmpTtsRequestQ->req, zTtsRequest,
                               sizeof(ARC_TTS_REQUEST_SINGLE_DM));
	pTmpTtsRequestQ->next = NULL;

	if ( strcmp(zTtsRequest->type, "FILE") == 0 )
	{
		struct stat		yStat;
		string			yString;
		int				fd;
		int				rc;
	
	    if(stat(zTtsRequest->string, &yStat) < 0)
	    {
			mrcpClient2Log(__FILE__, __LINE__, appPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, 
	            "Failed to stat file(%s). [%d: %s] Unable to read TTS string.",
	            zTtsRequest->string, errno,strerror(errno));
	        return(-1);
	    }

		pTmpTtsRequestQ->largeString = (char *)malloc(yStat.st_size + 10);
		if ( pTmpTtsRequestQ->largeString == NULL )
		{
			mrcpClient2Log(__FILE__, __LINE__, appPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, 
	            "Failed to allocate memory. [%d:%s] Unable to process "
				"TTS string.", errno, strerror(errno));
	        return(-1);
		}
		memset(pTmpTtsRequestQ->largeString, '\0', yStat.st_size+10);
		if ( (fd = open(zTtsRequest->string, O_RDONLY)) == -1)
		{
			mrcpClient2Log(__FILE__, __LINE__, appPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, 
	            "Failed to open TTS string file(%s). [%d: %s] "
				"Unable to read TTS string.",
	            zTtsRequest->string, errno, strerror(errno));
			free(pTmpTtsRequestQ->largeString);
	        return(-1);
		}
	
		if ( (rc = read(fd, pTmpTtsRequestQ->largeString,
						yStat.st_size+1)) < yStat.st_size )
		{
			mrcpClient2Log(__FILE__, __LINE__, appPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, 
	            "Failed to read TTS string from file(%s). [%d: %s] ",
	            zTtsRequest->string, errno, strerror(errno));
			free(pTmpTtsRequestQ->largeString);
			close(fd);
	        return(-1);
		}
			
		close(fd);
	}
	else
	{
		pTmpTtsRequestQ->largeString = NULL;

	}

    if ( firstTtsRequest == NULL )
    {
        firstTtsRequest = pTmpTtsRequestQ;
        lastTtsRequest = pTmpTtsRequestQ;
    }
    else
    {
        lastTtsRequest->next = pTmpTtsRequestQ;
        lastTtsRequest = pTmpTtsRequestQ;
    }

    return(0);

} // END: SRPort::addTTSRequest()

ARC_TTS_REQUEST_SINGLE_DM *SRPort::retrieveTTSRequest(long zId, string & zStr)
{
    struct TtsInfo  *pTmpTtsRequestQ;

    pTmpTtsRequestQ = firstTtsRequest;

    while ( pTmpTtsRequestQ != NULL )
    {
		if ( zId == -1 )
		{
			if ( pTmpTtsRequestQ->largeString == NULL )
			{
				zStr = "";
			}
			else
			{
				zStr = pTmpTtsRequestQ->largeString;
			}

			return(&(pTmpTtsRequestQ->req));
		}

        if ( zId == pTmpTtsRequestQ->id )
        {
			if ( pTmpTtsRequestQ->largeString == NULL )
			{
				zStr = "";
			}
			else
			{
				zStr = pTmpTtsRequestQ->largeString;
			}

			return(&(pTmpTtsRequestQ->req));
        }
        pTmpTtsRequestQ = pTmpTtsRequestQ->next;
    }

	return((ARC_TTS_REQUEST_SINGLE_DM *) NULL);

} // END: SRPort::retrieveTTSRequest()

int SRPort::removeTTSRequest(long zId)
{
	char mod[] = "removeTTSRequest";

    struct TtsInfo  *pTmpTtsRequestQ;
    struct TtsInfo  *prevTtsRequestQ;

    pTmpTtsRequestQ = firstTtsRequest;
    prevTtsRequestQ = pTmpTtsRequestQ;

    while ( pTmpTtsRequestQ != NULL )
    {
        if ( zId == pTmpTtsRequestQ->id )
        {
            if ( prevTtsRequestQ == pTmpTtsRequestQ )
            {   // Removing first element

                firstTtsRequest = pTmpTtsRequestQ->next;
	            if ( pTmpTtsRequestQ->largeString != NULL )
	            {
					free(pTmpTtsRequestQ->largeString);
	            }

                free( pTmpTtsRequestQ );
                return(0);
            }
            else
            {
                prevTtsRequestQ->next = pTmpTtsRequestQ->next;

	            if ( pTmpTtsRequestQ->largeString != NULL )
	            {
					free(pTmpTtsRequestQ->largeString);
	            }

				if(pTmpTtsRequestQ == lastTtsRequest)
				{
					lastTtsRequest = prevTtsRequestQ;
				}

                free (pTmpTtsRequestQ );
                return(0);
            }
        }

        prevTtsRequestQ = pTmpTtsRequestQ;
        pTmpTtsRequestQ = pTmpTtsRequestQ->next;
    }

	return 0;
} // END: SRPort::removeTTSRequest()

bool SRPort::isTTSRequestOnQueue(long zId)
{
    struct TtsInfo  *pTmpTtsRequestQ;

    pTmpTtsRequestQ = firstTtsRequest;

    while ( pTmpTtsRequestQ != NULL )
	{
        if ( zId == pTmpTtsRequestQ->id )
        {
			mrcpClient2Log(__FILE__, __LINE__, appPort, "clearTTSRequests",
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Found request (%d) in TTS queue.", pTmpTtsRequestQ->id);

			return(true);
        }

        pTmpTtsRequestQ = pTmpTtsRequestQ->next;
    }

	printTTSRequests();

	return(false);

} // END: SRPort::isTTSRequestOnQueue()

bool SRPort::getRequestQueueLockInitialized() const {
	return requestQueueLockInitialized;
}

int SRPort::clearTTSRequests()
{
	char mod[] = "clearTTSRequests";
    struct TtsInfo  *tmpRequestQ;
    int             i = 0;

    tmpRequestQ = firstTtsRequest;

    while ( tmpRequestQ != NULL )
    {
        firstTtsRequest = tmpRequestQ->next;

		if ( tmpRequestQ->largeString != NULL )
		{
			free(tmpRequestQ->largeString);
		}

		mrcpClient2Log(__FILE__, __LINE__, appPort, "clearTTSRequests",
					REPORT_VERBOSE, MRCP_2_BASE, INFO,
					"Freeing requestId (%d)", tmpRequestQ->id);

        free ( tmpRequestQ );

        tmpRequestQ = firstTtsRequest;
    }

	firstTtsRequest	= NULL;
	lastTtsRequest	= NULL;

	return(0);

} // END: SRPort::clearTTSRequest()

int SRPort::printTTSRequests()
{
	struct TtsInfo  *tmpRequest;
	int             i = 0;
	char			*p;

	tmpRequest = firstTtsRequest;

	while ( tmpRequest != NULL )
	{
		mrcpClient2Log(__FILE__, __LINE__, appPort, "printTTSRequests",
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"%d:ttsRequest->id=%d,port=%s,pid=%s,sync=%d,(%s),"
				"largeString=(%.*s)",
				i++, tmpRequest->id,
				tmpRequest->req.port,
				tmpRequest->req.pid,
				tmpRequest->req.speakMsgToDM.synchronous,
				tmpRequest->req.string, 300, tmpRequest->largeString);

		tmpRequest = tmpRequest->next;
	}

	return(0);

} // END: SRPort::printTTSRequests()

void SRPort::incrementRequestId()
{
	int		tmpInt;
	char	tmpChar[16];
	
	tmpInt = atoi(requestId.c_str());

	sprintf(tmpChar, "%d", ++tmpInt);

	requestId = string(tmpChar);
}

//  --- Isxxx ---
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
	struct timeval  tp;
	struct timezone tzp;
	struct tm   *tm;
	char	buf[256];
	bool	tmpLicenseReserved;

	gettimeofday(&tp, &tzp);
    tm = localtime(&tp.tv_sec);
	sprintf(buf, "%02d:%02d:%04d %02d:%02d:%02d:%03d",
            tm->tm_mon + 1, tm->tm_mday, tm->tm_year+1900, tm->tm_hour,
            tm->tm_min, tm->tm_sec, tp.tv_usec / 1000);

	pthread_mutex_lock(&licenseLock);
	tmpLicenseReserved = licenseReserved;
//	mrcpClient2Log(__FILE__, __LINE__, appPort, "SRPort::IsLicenseReserved",
//		REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"DJB: licenseReserved is currently %d", licenseReserved);
	pthread_mutex_unlock(&licenseLock);

	return tmpLicenseReserved;
}

bool SRPort::IsLicenseRequestFailed() const {
	return licenseRequestFailed;
}

bool SRPort::IsTelSRInitCalled() const {
	return telSRInitCalled; 
}

bool SRPort::IsCallActive() const {
    return callActive;
}

bool SRPort::IsPhrasesPlayingOrScheduled() const {
    return arePhrasesPlayingOrScheduled;
}

bool SRPort::IsInviteSent() const {
    return inviteSent;
}


bool SRPort::IsStopProcessingTTS() const {
//	mrcpClient2Log(__FILE__, __LINE__, appPort, "SRPort::IsStopProcessingTTS",
//		REPORT_NORMAL, MRCP_2_BASE, ERR,
//		"stopProcessingTTS is %d.", stopProcessingTTS);
    return stopProcessingTTS;
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

bool SRPort::IsDisconnectReceived() const {
    if ( disconnectReceived > 0 )
    {
        return(true);
    }
    else
    {
        return(false);
    }
}


//----- Getters -----
int SRPort::getPid() const {
	return pid;
}

pthread_t SRPort::getMrcpThreadId(char *zFile, int zLine) {
	return pMrcpThreadId;
}

pthread_t SRPort::getSavedMrcpThreadId(char *zFile, int zLine) {
	return pSavedMrcpThreadId;
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

int SRPort::getCloseSession() const {
	return closeSession;
}

string  SRPort::getAppResponseFifo() const
{
    return appResponseFifo;
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

int  SRPort::getSocketfd() const {
	return socketfd;
}

int  SRPort::getMrcpPort() const {
	return mrcpPort;
}

int  SRPort::getRtpPort() const {
	return rtpPort;
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
	return requestId;
}

string  SRPort::getServerIP() const
{
	return serverIP;
}

long  SRPort::getLastDisconnect() const
{
    return disconnectTimeInMilliseconds;
}

ARC_TTS_RESULT *SRPort::getTTSResult() {
	return &ttsResult;
}

void  SRPort::setInProgressReceived(bool xInProgressReceived){
    isInProgressRecv = xInProgressReceived;
}

bool SRPort::isInProgressReceived() const {
    return isInProgressRecv;
}