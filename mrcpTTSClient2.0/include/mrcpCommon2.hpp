/*------------------------------------------------------------------------------
Program Name:   mrcpCommon2.hpp
Purpose     :   Common definitions for the mrcp 2.0 client
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.
Update: 09/25/07 djb Added multiple ttsClient logic.
Update: 10/26/14 djb MR 4241 - Added cancelCid to SRPort class
------------------------------------------------------------------------------*/
#ifndef MRCP_COMMON2_HPP
#define MRCP_COMMON2_HPP

#include <list>
#include <string>
//#include <vector>
#include <queue>
#include <map>
#include <pthread.h>

#ifdef EXOSIP_4
#include <eXosip2/eX_setup.h>
#endif // EXOSIP_4

#include "mrcp2_HeaderList.hpp"
#include "parseResult.hpp"
//#include "GrammarList.hpp"

extern "C"
{
	#include "AppMessages_mrcpTTS.h"
	#include "ttsStruct.h"
	int LogARCMsg(char *mod, int mode, char *portStr,
				char *obj, char *appName, int msgId,  char *msg);
}

using namespace std;

void reloadMrcpRtpPorts();

void mrcpClient2Log(char *zFile, int zLine, int zCallNum, char *zpMod, int mode,
				 int msgId, int msgType, char *msgFormat, ...);
int writeResponseToApp(int zCallNumber, char *zFile, int zLine, 
			void *pMsg, int zMsgLength);
void threadId2String(pthread_t pt, char *charPt);

// Find the IP address of zHostName and sets zIp to that address.

namespace mrcp2client
{

const int LOCAL_STARTING_MRCP_PORT = 12000;
const int MAX_PORTS = 576;
const int MRCP_2_BASE =	3000;
const int MAX_URIS = 20;

const int MRCP_SR_REQ_FROM_CLIENT		= 1;
const int MRCP_SR_REQ_FROM_SRRECOGNIZE	= 2;
const int MRCP_SR_REQ_STOP_PROCESSING	= 99;

const int REPORT_NORMAL = 1;
const int REPORT_VERBOSE = 2;
const int REPORT_DETAIL = 128;


const int ERR = 0;
const int WARN = 1;
const int INFO = 2;

const int LOAD_GRAMMAR_NOW			=	1;
const int LOAD_GRAMMAR_VAD_LATER	=	2;

const int SR    = 0;
const int TTS   = 1;

const int NO_INCREMENT_AFTER_GET	= 0;
const int INCREMENT_AFTER_GET		= 1;
#if 0
struct GrammarData
{
	int           type;
	bool          isActivated;
	bool          isLoadedOnBackend;
	string        grammarName;
	string        data;
};
#endif


//=============
class SdpMedia
//=============
{
public:
	SdpMedia();
	SdpMedia(const string& xResourceType, 
			 const string& channelID, 
			 const int serverPort);
	SdpMedia(const SdpMedia &xSdpMedia);
	SdpMedia&  operator=(const SdpMedia & xSdpMedia);
	~SdpMedia();

	string		getResourceType();  // speechrecog, dtmfrecog, speechsynth
	string		getChannelID();
	int			getServerPort();

private:
	string		resourceType;  // speechrecog, dtmfrecog, speechsynth
	string		channelID;
	int			serverPort;
};


class RtpMedia
{
	public:
	RtpMedia();
	RtpMedia(int xPayload, int xCodec, int xPort);
	RtpMedia(const RtpMedia& xRtpMedia);
	RtpMedia& operator=(const RtpMedia& xRtpMedia);
	~RtpMedia();

	int 	getCodec();
	int 	getPayLoad();
	int   getServerPort();

	private:
	int		codec;
	int		payload;
	int		port;
};

#if 0

struct  MyXMLInstanceInfo
{
  string    name;
  string    value;
}; 


struct XmlResultInfo
{
  int         confidence;
  int         numInstanceChildren;
  string      resultGrammar;
  string      inputValue;
  string      instance;
  string      inputMode;
  string      literalTimings;
  vector<MyXMLInstanceInfo> instanceChild; // we can check MAX_INSTANCE_CHILDREN in application
	int         NumOfResultsAvailable; // backwards compatibility
  //MyXMLInstanceInfo instanceChild[MAX_INSTANCE_CHILDREN + 1];
};
#endif

struct UriInfo
{
	int		inService;
	time_t	lastFailureTime;
	int		lastPort;
	string	serverIP;
	string	requestURI;
};

struct ServerInfo 		// from arcMRCP2.cfg
{
	int				sipPort;
	string			transportLayer;
	int				numUris;
	int				uriIndex;
	struct UriInfo	uri[MAX_URIS + 1];
	string			via;	
	string			payloadType;
	string			codecType;
};

struct MrcpCfgInfo		// from arcMRCP2.cfg
{
	string	sipVersion;
	string	mrcpVersion;
    string  localIP;
	struct ServerInfo	serverInfo[2];
};

struct TtsInfo
{
	long						id;
	ARC_TTS_REQUEST_SINGLE_DM	req;
	char						*largeString;
	struct TtsInfo 				*next;
};

class MrcpInitialize
{
public:
	MrcpInitialize();
	~MrcpInitialize();
	
	int		readCfg();
	void	printInitInfo();

	string	getSipVersion() const;
	string	getMrcpVersion() const;
	int		getSipPort(int zType) const;
	int		getServerUri(int zPort, int zType, int zStartingIndex,
				string &requestURI, string &serverIP);
    string  getLocalIP() const;


	string	getTransportLayer(int zType) const;
	string	getVia(int zType) const;
	string	getPayloadType(int zType) const;
	string	getCodecType(int zType) const;
	string	getMrcpFifoDir() const;
	string	getDynmgrFifoDir() const;
	string	getApplFifoDir() const;
	int		populateUris(int zType, char *zUriStr);
	int		getNumURIs() const;

	void    setClientId(int zId){clientId = zId;};/*DDN: Added*/
    int     getClientId(){return (clientId);};/*DDN: Added*/
	int		getUriInfo (int xPort, int xIndex, struct UriInfo *xUriInfo);

#ifdef EXOSIP_4
    struct eXosip_t *getExcontext();
#endif // EXOSIP_4 

//	string	getServerIP(int zType) const;
//	string	getRequestURI(int zType) const;

	void    setMrcpFifoDir(string zMrcpFifoDir);
	void    setDynmgrFifoDir(string zDynmgrFifoDir);
	void    setApplFifoDir(string zApplFifoDir);
    void    disableServer(int zLine, int zPort, string xRequestURI);
    void    enableServer(int zLine, int zPort, string xRequestURI);
	void	setUriInfo (int xPort, int xIndex, struct UriInfo *xUriInfo);

#ifdef EXOSIP_4
    void    setEx4Context(struct eXosip_t *zexcontext);
#endif // EXOSIP_4 

private:
	string				mrcpFifoDir;
	string				dynmgrFifoDir;
	string				applFifoDir;

	int					clientId;

	struct MrcpCfgInfo	mrcpCfgInfo;

	mutable pthread_mutex_t	uriInfoLock;
	mutable pthread_mutex_t uriIndexLock;
#ifdef EXOSIP_4
    struct eXosip_t     *excontext;
#endif // EXOSIP_4 

	int	sVerifyConfig(char *zConfigFile);
};// END: MrcpInitialize



class SRPort
{
public:

	SRPort();
	~SRPort();

	void 	initializeElements();
	void 	initializeMutex();

	void 	destroyMutex();

	//--- Setters ---
	void 	setPid(int xPid);
	void	setApplicationPort(int xPort);
	void 	setCid(int xPort, int xCid); // SIP callid
	void 	setDid(int xDid); // SIP dialogid
    void    setCancelCid(int xCid); // SIP dialogid
	void 	setOpCode(int xOpCode);
	void 	setCloseSession(int xCloseSession);

	// SetParameter elements
	void    setGrammarAfterVAD(bool xGrammarAfterVAD); 
	void    setFailOnGrammarLoad(bool xFailOnGrammarLoad);
	void	setSessionRecord(bool xSessionRecord);
	void	setSessionPath(string xSessionPath);
	void    setGrammarName(string xGrammarName); 

	void	setGrammarLoaded(bool xLoaded);
	void	setGrammarsLoaded(int xGrammarsLoaded);
	void	setGrammarsLoaded(int xGrammarsLoaded, int zIncrement);

//	void    setParsedResult(XmlResultInfo xResultInfo);
//	void    setXmlResult(string xInfo);
	void    setRtpMedia(const RtpMedia& rtpMedia);
	void    setSdpMedia(const vector<SdpMedia>& sdpMedia);

	void    setLicenseReserved(bool xLicenseReserved);
	void    setLicenseRequestFailed(bool xLicenseRequestFailed);
	void    setAppResponseFifo(string zFifoName);
    void    setAppResponseFd(int zFd);
    void    setFdToDynMgr(int zFdToDynMgr);
	void    setInviteSent(int xPort, bool xInviteSent);

	void    setTelSRInitCalled(bool xRelSRInitCalled);
	void    setCallActive(bool xCallActive); // false if dynMgr find client not connected 
	void    setArePhrasesPlayingOrScheduled(bool xPhrasesScheduled);
	void    setStopProcessingTTS(bool xStopProcessingTTS);
	void    setDisconnectReceived(bool xDisconnectReceived);

	void    setMrcpThreadId(pthread_t xId);
	void    setSavedMrcpThreadId(pthread_t xId);
	void	setSocketfd(int zPort, int xSocketfd);
	void	setMrcpPort(int xMrcpPort);
	void	setRtpPort(int xRtpPort);
	void	setPayload(string xPayload);
	void	setCodec(string xCodec);
	void	setChannelId(string xChannelId);
	void	setRequestId(string xRequestId);
	void	setServerIP(string xServerIP);
	void	incrementRequestId();

	void	setRequestQueueLockInitialized(bool xRequestQueueLockInitialized);
	int	    openAppResponseFifo(char *zMsg);
    int     openAppResponseFifo_saved(char *zMsg, int zPort);
	int	    closeAppResponseFifo(int xPort);

	void	eraseCallMapByPort(int xPort);

	//	ttsResult
	void	setTTSFileName(char *zFileName);
	void	setTTSPort(char *zPort);
	void	setTTSPid(char *zPid);
	void	setTTSResultFifo(char *zResultFifo);
	void	setTTSResultCode(char *zResultCode);
	void	setTTSError(char *zError);
	void	setTTSSpeechFile(char *zSpeechFile);

	// --- Isxxx ---
	bool    IsFailOnGrammarLoad() const;
	bool    IsGrammarAfterVAD() const;
	bool	IsSessionRecord() const;

	bool	IsGrammarLoaded() const;
	bool    IsLicenseReserved();
	bool    IsLicenseRequestFailed() const;
	bool    IsTelSRInitCalled() const;
    bool    IsCallActive() const;
    bool    IsPhrasesPlayingOrScheduled() const;
    bool    IsStopProcessingTTS() const;
    bool    IsFdToDynmgrOpen() const;
    bool    IsDisconnectReceived() const;
    bool    IsInviteSent() const;

	// --- Getters ---
	int		getPid() const;
	int		getApplicationPort() const;
//	int  	getNumGrammarsLoaded() const;

	string  getAppResponseFifo() const;
    string  getAppResponseFifo_saved() const;
	int  	getAppResponseFd() const;
    int     getFdToDynMgr() const;
	int	    getCid() const;
	int	    getDid() const;
	int     getCancelCid() const;
	int	    getOpCode() const;
	int		getCloseSession() const;
	string  getGrammarName() const;
	string	getSessionPath() const;

	pthread_t getMrcpThreadId(char *zFile, int zLine);
	pthread_t getSavedMrcpThreadId(char *zFile, int zLine);
	int		getSocketfd() const;
	int		getMrcpPort() const;
	int		getRtpPort() const;
	string	getPayload() const;
	string	getCodec() const;
	string	getChannelId() const;
	string	getRequestId() const;
	string	getServerIP() const;
    long    getLastDisconnect() const;

	int		addTTSRequest(long zId, 
					ARC_TTS_REQUEST_SINGLE_DM *zTtsRequest);
	ARC_TTS_REQUEST_SINGLE_DM *retrieveTTSRequest(long zId, string & zStr);
	int		removeTTSRequest(long zId);
	bool	isTTSRequestOnQueue(long zId);
	int		clearTTSRequests();
	int		printTTSRequests();

	bool	getRequestQueueLockInitialized() const;
#if 0
	ARC_TTS_REQUEST_SINGLE_DM	*getTTSInfoById(long zId);
	ARC_TTS_REQUEST_SINGLE_DM	*getNextTTSInfoById();
#endif

	ARC_TTS_RESULT		*getTTSResult();
	RtpMedia			getRtpMedia() const;
	vector<SdpMedia>	getSdpMedia() const;

//	XmlResultInfo*	  	getParsedResult();
//	string				getXmlResult() const;

	bool	isInProgressReceived() const;
	void  	setInProgressReceived(bool xInProgressReceived);

private:
	int			pid;
	int			appPort;
	int			appResponseFd;
	int			fdToDynmgr;	//
	int			opCode;
	int			closeSession;
	int			numGrammarsLoaded;

	bool		isGrammarLoaded;
	bool		licenseReserved;
	bool		licenseRequestFailed;
	bool		telSRInitCalled;
    bool        callActive;
    bool        arePhrasesPlayingOrScheduled;
    bool        stopProcessingTTS;
    bool        inviteSent;

	// Internal Set/Get Parameters
	bool		grammarAfterVAD;
	bool		failOnGrammarLoad;
	bool		sessionRecord;
	string		sessionPath;
	string		grammarName;

	int			socketfd;
	int			mrcpPort;
	int			rtpPort;
	string		serverIP;

	pthread_t			pMrcpThreadId;
	pthread_t			pSavedMrcpThreadId;
	pthread_mutex_t		licenseLock;

	bool		requestQueueLockInitialized;

	string		payload;
	string		codec;
	string		channelId;
	string		requestId;

	int    	cid;	// sip callid from INVITE response
	int    	did;	// sip dialog id
    int     cancelCid;

	string				appResponseFifo;
    string              appResponseFifo_saved;

	vector<SdpMedia>		vecSdpMedia;  //SdpMedia  sdpMedia[5];
	RtpMedia		    	rtpMedia;
	struct TtsInfo			*firstTtsRequest;
	struct TtsInfo			*lastTtsRequest;
	ARC_TTS_RESULT			ttsResult;

	bool			isInProgressRecv;
	bool			disconnectReceived;

    long            disconnectTimeInMilliseconds;

// char               gResultInputMode[32];

};// END: SRPort

/*
//=================
class MrcpMessage
//=================
{
public:
	MrcpMessage();
	MrcpMessage(const string& version, const string& requestId);

	string getVersion();
	virtual string getRequestId();
	virtual string buildMsg();

	virtual ~MrcpMessage();

private:
	string version;
	unsigned int msgLen;
	string requestId;
	string msgContent;
};

//===================================
class MrcpRequest:public MrcpMessage
//===================================
{
public:
	MrcpRequest();
	~MrcpRequest();

private:
	string methodName;
}
*/

//=================
class ClientRequest
//=================
{
public:
  ClientRequest(const string& xVersion,
                const string& xMethod,
                const string& xRequestId,
                MrcpHeaderList& xHeaders,
                const int contentLength);

  string buildMrcpMsg();

private:
	string version;
	string method;
	int msgLength; // mrcp message length
	string requestId;
	string headers; // rows of name:value pairs
	int _numOfDigits(int i);
};


/*
 * This class is to generate a monotonically increasing 
 * reqeustId for MrcpClient and sent with a MrcpRequest.
 */
#if 0
//==============
class Singleton
//==============
{
public:
	static Singleton& getInstance();
	int getCounter();
	
protected:
	Singleton(){};
	//~Singleton(){};

private:
	Singleton(const Singleton& rhs){};
	Singleton& operator=(const Singleton& rhs){};
	static int counter;
};
#endif


//=========================
#ifdef MRCP2_COMMON_GLOBALS
	char					gAppName[16] = "mrcpClient2";

	SRPort								gSrPort[MAX_PORTS];
	queue<ARC_TTS_REQUEST_SINGLE_DM>	requestQueue[MAX_PORTS]; 
//	GrammarList				grammarList[MAX_PORTS];
	map<const int, int>		callMap; 		//map (callid, appPort#)
	MrcpInitialize			gMrcpInit;

	pthread_mutex_t			requestQueueLock[MAX_PORTS]; // requstQueue lock
	pthread_mutex_t			callMapLock;	// callMap lock 
#else
	extern	MrcpInitialize		gMrcpInit;
	extern	char	   			gAppName[];

	extern	SRPort      		gSrPort[];
	extern  queue<ARC_TTS_REQUEST_SINGLE_DM>	requestQueue[];
//	extern	GrammarList			grammarList[MAX_PORTS];
	extern  map<const int, int>	callMap;

	extern  pthread_mutex_t		requestQueueLock[]; // requstQueue lock
	extern  pthread_mutex_t		callMapLock; 
#endif

}

#endif
