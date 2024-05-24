/*------------------------------------------------------------------------------
Program Name:   mrcpCommon2.hpp
Purpose     :   Common definitions for the mrcp 2.0 client
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.
Update: 12/06/2006 djb Added mrcpConnectionIP to SRPort class.
Update: 04/09/2007 djb Added getLanguage prototype to SRPort class.
Update: 06/07/2007 djb Added transportLayer and sipPort.
Update: 10/23/2009 djb MR 2120 - Added mrcpCfgFile to MrcpCfgInfo.
Update :   08/05/10 djb  MR 3052 - Added pid initialization logic
Update: 08/28/14 djb MR 4241 - Added cancelCid to SRPort class
------------------------------------------------------------------------------*/
#ifndef MRCP_COMMON2_HPP
#define MRCP_COMMON2_HPP

#include <list>
#include <string>
//#include <vector>
#include <queue>
#include <map>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

#ifdef EXOSIP_4
#include <eXosip2/eX_setup.h>
#endif // EXOSIP_4

#include "mrcp2_Event.hpp"
#include "mrcp2_HeaderList.hpp"
#include "parseResult.hpp"
#include "GrammarList.hpp"

extern "C"
{
//	#include "memwatch.h"
	#include "AppMessages.h"
	#include "TEL_LogMessages.h"
	int LogARCMsg(char *mod, int mode, char *portStr,
				char *obj, char *appName, int msgId,  char *msg);
}

using namespace std;

void mrcpClient2Log(char *zFile, int zLine, int zCallNum, char *zpMod, int mode,
				 int msgId, int msgType, char *msgFormat, ...);

int writeResponseToApp(int zCallNumber, char *zFile, int zLine, 
			void *pMsg, int zMsgLength);
void threadId2String(pthread_t pt, char *charPt);


// Find the IP address of zHostName and sets zIp to that address.
string getIpAddress(const string& zHostName);
int setVXMLVersion();


namespace mrcp2client
{

const int LOCAL_STARTING_MRCP_PORT = 10500;
const int MAX_PORTS = 576;
const int MAX_LICENSE_FAILURES = 96;
const int MRCP_2_BASE =	3000;
const int MAX_URIS = 20;

const int MRCP_SR_REQ_FROM_CLIENT		= 1;
const int MRCP_SR_REQ_FROM_SRRECOGNIZE	= 2;
const int MRCP_SR_REQ_STOP_PROCESSING	= 99;

#if 0
const int REPORT_NORMAL = 1;
const int REPORT_VERBOSE = 2;
const int REPORT_DETAIL = 128;
const int REPORT_DIAGNOSE = 64;

const int ERR = 0;
const int WARN = 1;
const int INFO = 2;
#endif

const int LOAD_GRAMMAR_NOW			=	1;
const int LOAD_GRAMMAR_VAD_LATER	=	2;

const int SR    = 0;
const int TTS   = 1;

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
    int     inService;
    time_t  lastFailureTime;
    string  serverIP;
    string  requestURI;
};

struct ServerInfo 		// from arcMRCP2.cfg
{
	int     sipPort;
    string  transportLayer;
    int             numUris;
    int             uriIndex;
    struct UriInfo  uri[MAX_URIS + 1];
	string	via;	
	string	payloadType;
	string	codecType;
    int     waveformURI;
};

struct MrcpCfgInfo		// from arcMRCP2.cfg
{
    string  cfgFile;
	string	sipVersion;
	string	mrcpVersion;
	string	localIP;
	struct ServerInfo	serverInfo[2];
};


class MrcpInitialize
{
public:
	MrcpInitialize();
	~MrcpInitialize();
	
	int		readCfg();
	void	printInitInfo();
	string	getCfgFile() const;
	string	getSipVersion() const;
	string	getMrcpVersion() const;
	string	getLocalIP() const;
	int		getSipPort(int zType) const;
    int     getServerUri(int zPort, int zType, int zStartingIndex,
                string &requestURI, string &serverIP);
    string  getTransportLayer(int zType) const;
    int     getwaveformURI() const;
//	string	getRequestURI(int zType) const;
	string	getVia(int zType) const;
	string	getPayloadType(int zType) const;
	string	getCodecType(int zType) const;
	string	getMrcpFifoDir() const;
	string	getDynmgrFifoDir() const;
	string	getApplFifoDir() const;
	string	getMrcpFifo() const;
    int     populateUris(int zType, char *zUriStr);
    int     getNumURIs() const;

	void    setClientId(int zId){clientId = zId;};/*DDN: Added*/
	int     getClientId(){return (clientId);};/*DDN: Added*/
	int		getUriInfo (int xPort, int xIndex, struct UriInfo *xUriInfo);

#ifdef EXOSIP_4
    struct eXosip_t *getExcontext();
#endif // EXOSIP_4

	void    setMrcpFifoDir(string zMrcpFifoDir);
	void    setDynmgrFifoDir(string zDynmgrFifoDir);
	void    setApplFifoDir(string zApplFifoDir);
    void    disableServer(int zLine, int zPort, string xRequestURI);
    void    enableServer(int zLine, int zPort, string xRequestURI);
	void	setUriInfo (int xPort, int xIndex, struct UriInfo *xUriInfo);

#ifdef EXOSIP_4
    void    setEx4Context(struct eXosip_t *zexcontext);
#endif // EXOSIP_4

	void	setTooManyFailures_shutErDown(bool xTooManyFailures_shutErDown);
    bool    IsTooManyFailures_shutErDown() const;

private:
	string				mrcpFifoDir;
	string				dynmgrFifoDir;
	string				applFifoDir;
	string				mrcpFifo;
	int					mrcpFifoFd;
	int					clientId;
	struct MrcpCfgInfo	mrcpCfgInfo;
	bool				tooManyFailures_shutErDown;
	mutable pthread_mutex_t	uriInfoLock;
	mutable pthread_mutex_t	uriIndexLock;
#ifdef EXOSIP_4
    struct eXosip_t     *excontext;
#endif // EXOSIP_4

	int	sVerifyConfig(char *zConfigFile);

};// END: MrcpInitialize

typedef struct
{
    int     statusCode;
    int     completionCause;
    char	eventStr[128];
	char	serverState[128];
    char	content[4096];
	Mrcp2_Event         mrcpV2Event;
} OutOfSyncEvent;

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
	void 	setCid(int xCid); // SIP callid
	void 	setDid(int xDid); // SIP dialogid
	void 	setCancelCid(int xCid); // SIP dialogid
	void 	setOpCode(int xOpCode);

	// SetParameter elements
	void	setIsGrammarGoogle(bool xIsGrammarGoogle);
	void    setGrammarAfterVAD(bool xGrammarAfterVAD); 
	void    setFailOnGrammarLoad(bool xFailOnGrammarLoad);
	void	setSessionRecord(bool xSessionRecord);
	void	setSessionPath(string xSessionPath);
	void    setGrammarName(string xGrammarName); 
	void    setLanguage(string xLanguage); 
	void    setSensitivity(string xSensitivity); 
	void    setRecognitionMode(string xRecognitionMode); 
	void    setInterdigitTimeout(string xInterdigitTimeout); 
	void    setIncompleteTimeout(string xIncompleteTimeout); 
	void    setTermTimeout(string xTermTimeout); 
	void    setTermChar(string xTermTimeout); 
	void    setSpeedAccuracy(string xSpeedAccuracy); 
	void    setMaxNBest(string xMaxNBest); 
	void    setConfidenceThreshold(string xConfidenceThreshold); 
    void    setHideDTMF(bool xHideDTMF);

	void	setRecognitionARecord(bool xRecognitionIsARecord); 		// MR 4949
	void	setGrammarLoaded(bool xLoaded);
	void	setGrammarsLoaded(int xGrammarsLoaded);
	void	setGrammarsLoaded(int xGrammarsLoaded, int zIncrement);

//	void    setParsedResult(XmlResultInfo xResultInfo);
//	void    setXmlResult(string xInfo);
	void    setRtpMedia(const RtpMedia& rtpMedia);
	void    setSdpMedia(const vector<SdpMedia>& sdpMedia);

	void    setLicenseReserved(bool xLicenseReserved);
	void    setReserveLicQuickFail(bool xReserveLicQuickFail);		// BT-195
	void    setAppResponseFifo(string zFifoName);
    void    setAppResponseFd(int zFd);
    void    setFdToDynMgr(int zFdToDynMgr);
    void    setInviteSent(int xPort, bool xInviteSent);

	void    setTelSRInitCalled(bool xRelSRInitCalled);
	void    setCallActive(bool xCallActive); // false if dynMgr find client not connected 
	void    setCleanUpCalled(bool xCleanUpCalled);
	void    setArePhrasesPlayingOrScheduled(bool xPhrasesScheduled);
	void    setStopProcessingRecognition(bool xStopProcessingRecognition);
	void    setDisconnectReceived(bool xDisconnectReceived);

	void    setMrcpThreadId(pthread_t xId);
	void	setMrcpConnectionIP(string xMrcpConnectionIP);
	void	setSocketfd(int xPort, int xSocketfd);
	void	setMrcpPort(int xMrcpPort);
	void	setServerRtpPort(int xServerRtpPort);
	void	setPayload(string xPayload);
	void	setCodec(string xCodec);
	void	setChannelId(string xChannelId);
	void	setRequestId(string xRequestId);
    void    setServerIP(string xServerIP);

	void	setOutOfSyncEvent(OutOfSyncEvent *zOutOfSyncEvent);
	void	clearOutOfSyncEvent();

	void 	setOutOfSyncEventReceived(int zOutOfSyncEventReceived); 
	void	incrementRequestId();
	void	setRecognizeResponse(struct MsgToApp *zMsgToApp);
	void	clearRecognizeResponse();
    void    setExitSentToMrcpThread(int zPort, bool xExit);

	int	    openAppResponseFifo(char *zMsg);
    int     openAppResponseFifo(char *zMsg, int zPort);
	int	    openAppResponseFifo_saved(char *zMsg, int zPort);
	int	    closeAppResponseFifo(char *zFile, int zLine, int xPort);
	int	    closeSocketFd(int xPort);

	void	eraseCallMapByPort(int xPort);

	// --- Isxxx ---
	bool    IsFailOnGrammarLoad() const;
	bool	IsGrammarGoogle() const;
	bool    IsGrammarAfterVAD() const;
	bool	IsSessionRecord() const;

	bool	IsRecognitionARecord() const; 		// MR 4949
	bool	IsGrammarLoaded() const;
	bool    IsLicenseReserved();
	bool    DidReserveLicQuickFail();		// BT-195
	bool    IsTelSRInitCalled() const;
    bool    IsCallActive() const;
	bool    IsCleanUpCalled() const;
    bool    IsPhrasesPlayingOrScheduled() const;
    bool    IsStopProcessingRecognition() const;
    bool    IsDisconnectReceived() const;
    bool    IsFdToDynmgrOpen() const;
    bool	IsExitSentToMrcpThread(int zPort) const;
	bool    IsOutOfSyncEventReceived() const;
    bool    IsInviteSent() const;

	// --- Getters ---
	int		getPid() const;
	int		getApplicationPort() const;
//	int  	getNumGrammarsLoaded() const;

	struct MsgToApp *getRecognizeResponse();
	void	getAppResponseFifo(string &zAppResponseFifo);
	string  getAppResponseFifo_saved() const;
	int  	getAppResponseFd() const;
    int     getFdToDynMgr() const;
	int	    getCid() const;
	int	    getDid() const;
	int	    getCancelCid() const;
	int	    getOpCode() const;
	string  getGrammarName() const;
	string	getSessionPath() const;
	string	getLanguage() const; 
	string	getSensitivity() const; 
	string	getRecognitionMode() const; 
	string	getConfidenceThreshold() const; 
	string	getSpeedAccuracy() const; 
	string	getMaxNBest() const; 
	string	getInterdigitTimeout() const; 
	string	getIncompleteTimeout() const; 
	string	getTermTimeout() const; 
	string	getTermChar() const; 
    bool    getHideDTMF();

	pthread_t getMrcpThreadId();
	string	getMrcpConnectionIP() const;
	int		getSocketfd() const;
	int		getMrcpPort() const;
	int		getServerRtpPort() const;
	string	getServerIP() const;
	string	getPayload() const;
	string	getCodec() const;
	string	getChannelId() const;
	string	getRequestId() const;

	OutOfSyncEvent		*getOutOfSyncEvent();
	void				copyOutOfSyncEvent(OutOfSyncEvent *zOutOfSyncEvent);
	int					getOutOfSyncEventReceived() const;

	RtpMedia			getRtpMedia() const;
	vector<SdpMedia>	getSdpMedia() const;

//	XmlResultInfo*	  	getParsedResult();
//	string				getXmlResult() const;


public:
	pthread_mutex_t		fifoLock;
    time_t  			lastInitTime;
private:
	int			pid;
	int			appPort;
	int			appResponseFd;
	int			fdToDynmgr;	//
	int			opCode;
	int			numGrammarsLoaded;

	bool		isRecognitionARecord; 		// MR 4949
	bool		isGrammarLoaded;
	bool		licenseReserved;
	bool		licenseQuickFail;		// BT-195
	bool		telSRInitCalled;
    bool        callActive;
    bool        arePhrasesPlayingOrScheduled;
    bool        stopProcessingRecognition;
    bool        disconnectReceived;
    bool        cleanUpCalled;
    bool        exitSentToMrcpThread;
    bool        inviteSent;

	// Internal Set/Get Parameters
	bool		isGrammarGoogle;
	bool		grammarAfterVAD;
	bool		failOnGrammarLoad;
	bool		sessionRecord;
	string		sessionPath;
	string		grammarName;
	string		language;
	string		sensitivity;
	string		recognitionMode;
	string		confidenceThreshold;
	string		speedAccuracy;
	string		maxNBest;
	string		interdigitTimeout;
	string		termTimeout;
	string		termChar;
	string		incompleteTimeout;
    bool        hideDTMF;

	string		mrcpConnectionIP;
	int			socketfd;
	int			mrcpPort;
	int			serverRtpPort;
    string      serverIP;

	pthread_t			pMrcpThreadId;
	pthread_mutex_t		licenseLock;
	pthread_mutex_t		exitSentLock;	// BT-217
	pthread_mutex_t		appResponseFifoLock;	// BT-259

	string		payload;
	string		codec;
	string		channelId;
	string		requestId;
	struct MsgToApp	recognizeResponse;

	int    	cid;	// sip callid from INVITE response
	int    	did;	// sip dialog id
	int		cancelCid;

	string				appResponseFifo;
	string				appResponseFifo_saved;

	vector<SdpMedia>	vecSdpMedia;  //SdpMedia  sdpMedia[5];
	RtpMedia			    rtpMedia;

	OutOfSyncEvent		outOfSyncEvent;
	int					outOfSyncEventReceived;
//	Mrcp2_Event			outOfSyncMrcpV2Event;

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

	SRPort					gSrPort[MAX_PORTS];
	queue<MsgToDM>			requestQueue[MAX_PORTS]; 
	GrammarList				grammarList[MAX_PORTS];
	//list<GrammarData>		grammarList[MAX_PORTS];	
	map<const int, int>		callMap; 		//map (callid, appPort#)
	MrcpInitialize			gMrcpInit;

	pthread_mutex_t			requestQueueLock[MAX_PORTS]; // requstQueue lock
	pthread_mutex_t			callMapLock;	// callMap lock 
#else
	extern	MrcpInitialize		gMrcpInit;
	extern	char	   			gAppName[];

	extern	SRPort      		gSrPort[];
	extern  queue<MsgToDM>		requestQueue[];
//	extern  list<GrammarData>	grammarList[];	
	extern	GrammarList			grammarList[MAX_PORTS];
	extern  map<const int, int>	callMap;

	extern  pthread_mutex_t		requestQueueLock[]; // requstQueue lock
	extern  pthread_mutex_t		callMapLock; 
#endif

}

#endif