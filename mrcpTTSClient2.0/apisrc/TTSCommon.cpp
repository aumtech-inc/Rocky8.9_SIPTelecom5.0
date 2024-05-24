/*-----------------------------------------------------------------------------
Program:TTSCommon.c

Author:	Aumtech, Inc.
Update:	08/01/2006 djb Ported to mrcpV2.
Update:	10/18/2006 djb Increased the recvBuf from 1024 to 2048 to avoid a core
Update:	10/24/2014 djb MR 4241: Added same logic as SR code
------------------------------------------------------------------------------*/

#include <fstream>

#ifdef EXOSIP_4
#include "osipparser2/osip_message.h"
#include <eXosip2/eXosip.h>
#else
#include <osip_message.h>
#include <eXosip2/eXosip.h>
#endif // EXOSIP_4 


#include "mrcpCommon2.hpp"
#include "mrcp2_HeaderList.hpp"
#include "mrcp2_Request.hpp"
#include "mrcp2_Response.hpp"
#include "mrcp2_Event.hpp"
#include "mrcpTTS.hpp"

extern "C"
{
	#include <stdlib.h>
	#include <string.h>
	#include <sys/time.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
    #include <fcntl.h>
	#include <netdb.h>
	#include <errno.h>
	#include "Telecom.h"
	#include "arcSR.h"
#ifdef EXOSIP_4
    #include "eXosip2/eX_setup.h"
#endif // EXOSIP_4 
}
using namespace std;
using namespace mrcp2client;

extern int		gTtsMrcpPort[];

// static string getHostIpAddress (const string& zHostName);
string createSdpOffer (int audioPort, int voiceCodec, int telephonyPayload,
				char *zCodecStr, char *zPayloadStr);
int send_sip_invite(int zPort, const string& fromUrl,
                        const string& toUrl,
                        const string& route,
                        const string& subject,
                        const string& sdpOffer);
//static int sIsGrammarAlreadyLoaded(int zPort, string &zGrammarName);
int sLoadGrammar(int zPort, int zGrammarType, string & zGrammarName,
					string & zGrammarContent);
int writeData(int zPort, int sockfd, string& data);
//static void srPrintGrammarList(int zPort);
void myTrimWS(char *zStr);
void getRtpInfo(int zPort, char *zPayloadStr, char *zCodecStr,
                int *zPayloadType, int *zCodecType);
int sWaitForLicenceReserved(int zPort, bool zTrueOrFalse);
int build_tcp_connection(int zPort, const string& serverName, 
						const int serverPort);
int sIsDataOnMrcpSocket(int zPort, int zMrcpPort, int zMilliSeconds);
int doControlledRead(int zPort, int zMrcpPort, char *zRecvBuf,
								int zMaxSize);
int sGetHeaderContent(string &serverMsg, string &headerMsg, 
					string &contentMsg);
int resetConnection(int zPort);

//=========================================
int ttsGetResource(const int zPort)
//=========================================
{
	char 	mod[] ="ttsGetResource";
    char    destUrl[256];
    char    fromUrl[256];
    int		rc=-1;
    int		localSdpPort;
    string	sdpOffer;
    string	sipRoute="";
    string	sipSubject="";

	string	requestURI;
	string	serverIP;
	int		cid, did;

    int		voiceCodec;
    int		telephonePayload;
	char	payLoadStr[128];
	char	codecStr[128];
	int		socketFd;

	int		uriIndex = -1;
	int     nextUriIndex = -1;
	int		numURIs;
	int		i, j;
	int		myServerPort;

    if ( gSrPort[zPort].IsLicenseReserved() )
    {                           
        return(0);      // Already have a license.  Just return success.
    }   

	numURIs = gMrcpInit.getNumURIs();
	for (i=0; i < numURIs; i++)
	{
        if ( (uriIndex = gMrcpInit.getServerUri(zPort, TTS, nextUriIndex, requestURI, serverIP)) < 0 )
        {
            return(-1);
        }
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "Retrieved URI=(%d,%d,%s,%s)",
            uriIndex, nextUriIndex, requestURI.c_str(), serverIP.c_str());

        if ( (uriIndex + 1) >= numURIs )
        {
            nextUriIndex = 0;
        }
        else
        {
            nextUriIndex = uriIndex + 1;
        }

		gSrPort[zPort].setServerIP(serverIP);

		serverIP = gSrPort[zPort].getServerIP();

	    // build and send INVITE to server
	    // --------------------------------
		sprintf (fromUrl, "<sip:%d@%s>",
				zPort, gMrcpInit.getLocalIP().c_str() );
	    sprintf (destUrl, "%s",	requestURI.c_str());
	
	    sipRoute = "";
	    sipSubject="send INVITE";

	    // construct SDP message body
//		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
//			MRCP_2_BASE, ERR, "DJB: gTtsMrcpPort[%d] = %d", zPort, gTtsMrcpPort[zPort]);

		if ( gTtsMrcpPort[zPort] == -1 )
		{
	    	localSdpPort = LOCAL_STARTING_MRCP_PORT + (zPort * 2);
		}
		else
		{
	    	localSdpPort = gTtsMrcpPort[zPort];
		}
	
		gSrPort[zPort].setRtpPort(localSdpPort);
	
//		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
//			MRCP_2_BASE, ERR, "DJB: fromUrl:(%s); destUrl:(%s); rptPort:(%d).",
//			fromUrl, destUrl, localSdpPort);
	
		getRtpInfo(zPort, payLoadStr, codecStr, &telephonePayload, &voiceCodec);
	
	    sdpOffer = createSdpOffer (localSdpPort, voiceCodec, telephonePayload,
					codecStr, payLoadStr);
	
//	   mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
//			MRCP_2_BASE, ERR, "DJB: telephonePayload=%d; voiceCoded=%d; "
//			"sdpOffer=(%s)", telephonePayload, voiceCodec, sdpOffer.c_str());
	
		gSrPort[zPort].setLicenseRequestFailed(false);

		pthread_mutex_lock( &callMapLock);

	    rc = send_sip_invite(zPort, fromUrl, destUrl, sipRoute, sipSubject,
					sdpOffer);

		if(rc >= 0)
		{
			callMap[rc] = zPort;
			gSrPort[zPort].setCid(zPort, rc);
			gSrPort[zPort].setCancelCid(0);

			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, "Successfully sent sip INVITE to server. "
					"Now wait for the response.");
	
		}
		pthread_mutex_unlock( &callMapLock);
	
	    // set INVITE callId
	    if (rc < 0)
	    {
	        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, "send INVITE from %s to %s failed.",
				fromUrl, destUrl);
			gMrcpInit.disableServer(__LINE__, (int)zPort, requestURI);

			continue;
	    }

	
		if ( (rc=sWaitForLicenceReserved(zPort, true)) == -1 )
		{
			gSrPort[zPort].setCancelCid(1);
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, "Failed to reserve a license to (%s).",
				requestURI.c_str());

			// cid and did are set by sipThread when get INVITE response.
            cid = gSrPort[zPort].getCid();
            did = gSrPort[zPort].getDid();

#ifdef EXOSIP_4
		    struct eXosip_t     *eXcontext;

		    eXcontext=gMrcpInit.getExcontext();
            eXosip_lock (eXcontext);
            rc = eXosip_call_terminate(eXcontext, cid, did); // send SIP BYE/CANCEL to server
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN, "Sent BYE / CANCEL to backend.");
            eXosip_unlock (eXcontext);
#else
            rc = eXosip_call_terminate(cid, did); // send SIP BYE/CANCEL to server
#endif // EXOSIP_4 

			gMrcpInit.disableServer(__LINE__, (int)zPort, requestURI);
			continue;
		}
        myServerPort = -1;
        for (j=0; j<3; j++)
        {
            myServerPort = gSrPort[zPort].getMrcpPort();
            if ( myServerPort != -1 )
            {
                mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
                    MRCP_2_BASE, INFO, "Retrieved Server port (%d)", myServerPort);
            }
            if ( myServerPort > 0 )
            {
                break;
            }
            usleep(1000);
        }

        if ( myServerPort <= 0 )
        {
            gSrPort[zPort].setCancelCid(1);
			gMrcpInit.disableServer(__LINE__, (int)zPort, requestURI);
            continue;
        }

		socketFd = build_tcp_connection(zPort, serverIP, 
					gSrPort[zPort].getMrcpPort());
		if ( socketFd < 0)
		{
			gSrPort[zPort].setCancelCid(1);
			gMrcpInit.disableServer(__LINE__, (int)zPort, requestURI);
			continue;
	    }

		gMrcpInit.enableServer(__LINE__, (int)zPort, requestURI);
		gSrPort[zPort].setSocketfd(zPort, socketFd);
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "Successfully reserved a license (mrcp port=%d, socketfd=%d)).",
            gSrPort[zPort].getMrcpPort(), socketFd);

		return(0);
	}
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
		MRCP_2_BASE, ERR, "Attempts to all Servers have failed. "
		"Unable to reserve a resource.");

	return(-1);

} // END:ttsGetResource()

//========================================
string createSdpOffer (int audioPort, 
					int voiceCodec, 
					int telephonyPayload,
					char *zCodecStr, 
					char *zPayloadStr)
//========================================
// This function creates the sdp body for outgoing SIP INVITEs.
/**
This function only needs to know what port SIPIVR will be
receiving audio on (zStrAudioPort), what DTMF payload type will be used 
(zTelephonyPayload), and what voice codec this call will be using (zVoiceCodec).

SDP m-line:

  must be one m-line for each MRCPv2 resource
  the format field must be empty

  c->s: must have "application TCP/MRCPv2 9"
  c->s: resource attribute: specify resource type identifier
  c->s: transport type: TCP
  c->s:  a=setup attribute MUST be "active"

  s->c: must have "application TCP/MRCPv2 listenPort"
  s->c: channel attribute: channel identifier(hexiString@resourceTypeIdentifier)           
  s->c:  a=setup attribute MUST be "passive"

  - recognizer
    need receive audio stream

*/

{
	char mod[] = "createSdpOffer"; 
	string sdp_body ="";
	char tmp[4086];
	char localIp[128];
	char yStrTelephonyPayload[100];
	
	string loginNameOnLocalHost = string("arc");	
	int sessionId = 123456789;	
	int intVersionNum = 2006;
	int cmid = 1;    // control media id
	int mid = cmid;  // media id

#ifdef EXOSIP_4
    struct eXosip_t     *eXcontext;
	string				pLocalIP;

    eXcontext=gMrcpInit.getExcontext();
	//eXosip_guess_localip (eXcontext, AF_INET, localIp, 128);
	pLocalIP = gMrcpInit.getLocalIP();
	sprintf(localIp, "%s", pLocalIP.c_str());
#else
	eXosip_guess_localip (AF_INET, localIp, 128);
#endif // EXOSIP_4 

	sprintf (tmp,
		 	"v=0\r\n"
			"o=%s %d %d IN IP4 %s\r\n"
			"s=SIP Call\r\n"
			"c=IN IP4 %s\r\n"
		 	"t=0 0\r\n"
			"m=application 9 TCP/MRCPv2\r\n"
			"a=setup:active\r\n"
			"a=connection:new\r\n"
			"a=resource:speechsynth\r\n"
			"a=cmid:%d\r\n"
			"m=audio %d RTP/AVP %d %d\r\n"
		 	"a=rtpmap:%d %s\r\n"
		 	"a=rtpmap:%d %s\r\n"
		 	"a=fmtp:96 0-15\r\n" 
		 	"a=recvonly\r\n" 
			"a=mid:%d\r\n",
			loginNameOnLocalHost.c_str(), sessionId, intVersionNum, 
			gMrcpInit.getLocalIP().c_str(),
			gMrcpInit.getLocalIP().c_str(),
			cmid,
		 	audioPort, voiceCodec, telephonyPayload,
			voiceCodec, zCodecStr, 
		 	telephonyPayload, zPayloadStr,
		 	mid);
//			loginNameOnLocalHost.c_str(), sessionId, intVersionNum, localIp,
//			localIp,

	sdp_body = tmp;
	return sdp_body;

}// END: int createSdpOffer

/*
 Send INVITE to SipServer. 
 callId = return int if  return Int >= 0, else return negative error code. 
*/
// ========================================
int send_sip_invite(int zPort, const string& fromUrl, 
					const string& toUrl,  
					const string& route, 
					const string& subject,
					const string& sdpOffer)
// ========================================
{
	char 	mod[] = "send_sip_invite";
	int		rc = -1;
	osip_message_t *invite = NULL;

#ifdef EXOSIP_4
    struct eXosip_t *eXcontext;
#endif // EXOSIP_4 

	// build initial INVITE 
	// ---------------------

#ifdef EXOSIP_4
    eXcontext=gMrcpInit.getExcontext();

    eXosip_lock (eXcontext);
    rc = eXosip_call_build_initial_invite ( eXcontext, &invite, toUrl.c_str(), fromUrl.c_str(), route.c_str(), subject.c_str());
    eXosip_unlock (eXcontext);
#else
	eXosip_lock ();
	rc = eXosip_call_build_initial_invite (
		&invite,
		toUrl.c_str(), fromUrl.c_str(), route.c_str(), 
		subject.c_str());
	eXosip_unlock ();
#endif // EXOSIP_4 

	if ( rc < 0 )
	{
#ifdef EXOSIP_4
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
            MRCP_2_BASE, ERR, "Unable to build SIP invite for eXcontext=%u, toUrl=(%s), "
            "fromUrl=(%s).  rc=%d.", eXcontext, toUrl.c_str(), fromUrl.c_str(), rc);
#else
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, "Unable to build SIP invite for toUrl=(%s), "
			"fromUrl=(%s).  rc=%d.", toUrl.c_str(), fromUrl.c_str(), rc);
#endif // EXOSIP_4 
		return rc;
	}

	// construct SDP message body 
	// --------------------------
	osip_message_set_body (invite, sdpOffer.c_str(), sdpOffer.length() );
	osip_content_type_free(invite->content_type);
	osip_message_set_content_type (invite, "application/sdp");

	// put invite content in Log before sending
	char* tmpBuf = NULL;
	char** pb = &tmpBuf;
	size_t bufLen;
	rc = osip_message_to_str(invite, pb, &bufLen);
	if ( rc == 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Sending INVITE string to to MRCP Server: (%s)", tmpBuf);
	}
	free(tmpBuf);


	// send initial Inivite 
	// --------------------
#ifdef EXOSIP_4
    eXosip_lock(eXcontext);
    rc = eXosip_call_send_initial_invite (eXcontext, invite);
    eXosip_unlock(eXcontext);
#else
	eXosip_lock();
	rc = eXosip_call_send_initial_invite (invite);
	eXosip_unlock();
#endif // EXOSIP_4 

	if ( rc < 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, "Failed to send SIP invite.  rc= %d.", rc);

	    pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return(-1) ;
	}
	gSrPort[zPort].setInviteSent(zPort, true);
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "Successfully sent SIP INVITE.  CallId=%d", rc);
	return rc; // rc is callId
} // END: send_sip_invite()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int processMRCPRequest(int zPort, string & zSendMsg, string & zRecvMsg,
		string & zServerState, int *zStatusCode, int *zCompletionCause,
		string & zEventStr)
{
	static char		mod[] = "processMRCPRequest";
	int				rc;
	string			yEventName = "";
	string			savedServerState;
	int				savedStatusCode;
	int				savedCompletionCause;
	string			savedRecvMsg;
	string			savedEventName;

	zRecvMsg			= "";
	zServerState		= "";
	*zStatusCode		= -1;
	*zCompletionCause	= -1;


	if ((rc = writeData(zPort, gSrPort[zPort].getSocketfd(), zSendMsg)) == -1)
	{
		return(-1);
	}

	rc = readMrcpSocket(zPort, READ_RESPONSE, zRecvMsg, yEventName,
			zServerState, zStatusCode, zCompletionCause);

	if ( rc != YES_EVENT_RECEIVED )
	{
		gSrPort[zPort].setInProgressReceived(true);
		return(rc);
	}


	if ( yEventName == "SPEAK-COMPLETE" )
	{
		string		yTmpServerState;
		int			yTmpStatusCode;
		int			yTmpCompletionCause;

		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Received event (%s) to complete the TTS request.  ",
			yEventName.c_str());
		while( (rc == 1) || (rc == -999) )
		{
			// throw this away
			yTmpServerState		= "";
			yTmpStatusCode		= -1;
			yTmpCompletionCause	= -1;;
		
			rc = readMrcpSocket(zPort, READ_RESPONSE, zRecvMsg, yEventName,
					yTmpServerState, &yTmpStatusCode, &yTmpCompletionCause);
			if ( rc > 0 )
			{
				mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
					REPORT_VERBOSE, MRCP_2_BASE, INFO,
					"Read and discarding response (%s:%d:%d)",
					zServerState.c_str(), yTmpStatusCode, yTmpCompletionCause);
			}
		}
		return(YES_EVENT_RECEIVED);
	}
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Received non-response event (%s) to request.  "
			"Continuing to read for response.", yEventName.c_str());

	savedServerState		= zServerState;
	savedStatusCode			= *zStatusCode;
	savedCompletionCause	= *zCompletionCause;
	savedEventName			= yEventName;

	rc = -999;
	
	while(rc == -999)
	{
		rc = readMrcpSocket(zPort, READ_RESPONSE, zRecvMsg, yEventName,
				zServerState, zStatusCode, zCompletionCause);
	}

	zServerState		= savedServerState;
	*zStatusCode		= savedStatusCode;
	*zCompletionCause	= savedCompletionCause;
	zEventStr			= savedEventName;
	
	return(YES_EVENT_RECEIVED);
} // END: processMRCPRequest()

/*------------------------------------------------------------------------------
readMrcpSocket():
------------------------------------------------------------------------------*/
int readMrcpSocket(int zPort, int zReadType,
				string & zRecvMsg, 
				string & zEventName,
				string & zServerState, 
				int *zStatusCode, 
				int *zCompletionCause)
{
	static char		mod[] = "readMrcpSocket";
	int				rc;
	int				mrcpPort	= gSrPort[zPort].getSocketfd();
	string			channelId	= gSrPort[zPort].getChannelId();
	string			requestId	= gSrPort[zPort].getRequestId();
	int				socketId	= gSrPort[zPort].getSocketfd();
	int				returnType;
	int				milliSeconds;

	zRecvMsg			= "";
	zServerState		= "";
	*zStatusCode		= 0;
	*zCompletionCause	= 0;

	if ( zReadType == READ_EVENT )
	{
		milliSeconds = 200;
	}
	else
	{
		milliSeconds = 3000000;	// Give a response 3 secs (may have to change)
	}
	
	rc = sIsDataOnMrcpSocket(zPort, mrcpPort, milliSeconds);
//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
//			REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"%d = sIsDataOnMrcpSocket(%d, %d, %d)", rc, zPort, mrcpPort, milliSeconds);

	switch (rc)
	{
		case -1: 			// message logged in routine
		case 0:
			return(rc);
			break;
	}

	// parse response/event
	char	recvBuf[2048]; // or smart pointer
	int		receivedLen = -1;

	memset((char *)recvBuf, '\0', sizeof(recvBuf));
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Calling doControlledRead()...");
	if ( (receivedLen = doControlledRead(zPort, mrcpPort, recvBuf,
						sizeof(recvBuf))) < 0 )
	{
		mrcpPort = gSrPort[zPort].getSocketfd();
		// message is logged in routine
		return(-1);
	}
	mrcpPort = gSrPort[zPort].getSocketfd();

	string serverMsg = string(recvBuf); // response or event

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Read %d bytes:(%.*s)", receivedLen, 500, serverMsg.c_str());

	// judge received messagege type (response or event) 
	// from 3rd field of start line  
	int pos1, pos2;
	pos1 = serverMsg.find(" ", 0); // 1st space
	pos2 = serverMsg.find(" ", pos1 + 1); // 2nd space
	pos1 = pos2 + 1;
	pos2 = serverMsg.find(" ", pos1); // 3rd space
	pos2 = pos2 - 1;
	int xFieldLen = pos2 - pos1 + 1;
	string xString = serverMsg.substr(pos1, xFieldLen);
	char xField = xString[0];

	Mrcp2_Response		mrcpResponse;
	Mrcp2_Event			mrcpEvent;
	MrcpHeaderList 		headerList; 
	long				serverMsgLen;
	string  			receivedRequestId;

	returnType = 0;

	string headerMsg	= "";
	string contentMsg	= "";
	if ((rc = sGetHeaderContent(serverMsg, headerMsg, contentMsg)) == -1)
	{
		return(-1);	// message is logged in routine.
	}
	if ( contentMsg.size() > 0 )
	{
		zRecvMsg = contentMsg;
	}
	
	if(isdigit((int)xField))  // response message
	{  
		mrcpResponse.addMessageBody(headerMsg);
		headerList = mrcpResponse.getHeaderList(); // headerList 
	    serverMsgLen = mrcpResponse.getMessageLength(); // message length
		// requestId
		char tmpRequestId[10];
		unsigned int rid = mrcpResponse.getRequestId();
		sprintf(tmpRequestId, "%d", rid);
		receivedRequestId = string(tmpRequestId);

		int yIntRequestId = atoi(requestId.c_str());
		int yIntReceivedRequestId = atoi(receivedRequestId.c_str());

		// verify requestId
		//if(yIntReceivedRequestId == (yIntRequestId - 1))
		if(yIntReceivedRequestId < yIntRequestId)
		{
			return(-999);
		}
		else if(receivedRequestId != requestId)
		{
	  		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"requestId (%s) received does not match "
				"with C->S requestId (%s).",
				receivedRequestId.c_str(), requestId.c_str());
			return(-1);
		}

		*zStatusCode = atoi(mrcpResponse.getStatusCode().c_str());
		zServerState = mrcpResponse.getRequestState();
		
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Response parsed: requestId(%s) statusCode:(%d) RequestState:(%s)",
				receivedRequestId.data(), *zStatusCode,
				zServerState.c_str());

	}
	else  //event message
	{  
		returnType = YES_EVENT_RECEIVED;

		mrcpEvent.addMessageBody(headerMsg);

		headerList = mrcpEvent.getHeaderList(); // headerList 
	    serverMsgLen = mrcpEvent.getMessageLength(); // message length

		char tmpRequestId[10];    // requestId
		unsigned int rid = mrcpEvent.getRequestId();
		sprintf(tmpRequestId, "%d", rid);
		receivedRequestId = string(tmpRequestId);

		zEventName = mrcpEvent.getEventName(); // eventname
		zServerState = mrcpEvent.getRequestState(); // request state

		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"received event(%s).", zEventName.c_str());

	}

	// verify msgLen 
	if (receivedLen < serverMsgLen)
	{
		// read more into buffer and insert into serverMsg
		int needLen = serverMsgLen - receivedLen; 
		int newLen = -1;
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Need more for message.  Attempting to read %d bytes", needLen);
		newLen = read(socketId, (void *)&recvBuf[receivedLen], needLen);
		if(newLen < needLen)
		{
  			mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Unable to read complete message.  Returning.");
			return(-1);
		}	
	}
	else if(serverMsgLen < receivedLen)
	{
		// todo:Can't remove extra bytes; must save them and append them later;
		//      however, not sure if this would never happen.

		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Received message length (%d) longer than expected (%d).",
			receivedLen, serverMsgLen);
		
		int msgSize = serverMsg.length();
		serverMsg.erase(serverMsgLen, msgSize - serverMsgLen);
	}

	while(!headerList.empty())	
	{
		MrcpHeader		header;
		header = headerList.front();

		string name = header.getName();
		string value = header.getValue();
		if(name == "Completion-Cause")
		{
			*zCompletionCause = atoi(value.c_str());
		}

		headerList.pop_front();

	} // END: for(int i = ...)  

//	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
//		REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"DJB: returnType=%d; YES_EVENT_RECEIVED=%d",
//		returnType, YES_EVENT_RECEIVED);

	if ( returnType == YES_EVENT_RECEIVED )
	{
		return(YES_EVENT_RECEIVED);
	}

	if ( (*zStatusCode < 200) || (*zStatusCode >= 300) )
	{
		return(-1);
	}

	return(0);
} // END: readMrcpSocket()

/*------------------------------------------------------------------------------
doControlledRead():
------------------------------------------------------------------------------*/
int	doControlledRead(int zPort, int zMrcpPort, char *zRecvBuf,
								int zMaxSize)
{
	static char	mod[] = "doControlledRead";
	int			receivedLen;
	int			rc;
	//static char tmpBuf[64];
	//static char lBuf[1024];
	//static char bytesToReadBuf[64];
	char tmpBuf[64];
	char lBuf[2048];
	char bytesToReadBuf[64];
	const int	numHeaderBytes = 20;
	int			bytesToRead;
	int			i;
	char		*p;

	memset((char *)zRecvBuf, '\0', zMaxSize);
	memset((char *)tmpBuf, '\0', sizeof(tmpBuf));

	// MRCP/2.0 xxxx - max message size

	// read numHeaderBytes just to get the total number of bytes to read.
//	for (i=0; i<2; i++)
//	{
		if ( (receivedLen = read(zMrcpPort, (void *)tmpBuf, numHeaderBytes)) <= 0 )
		{
	        if ( gSrPort[zPort].IsStopProcessingTTS() )
	        {
	            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
	                REPORT_VERBOSE, MRCP_2_BASE, INFO,
	                "Failed to read from the MRCP socket (%d). [%d, %s]",
	                zMrcpPort, errno, strerror(errno));
	        }
	        else
	        {
	            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
	                REPORT_NORMAL, MRCP_2_BASE, ERR,
	                "Failed to read from the MRCP socket (%d). [%d, %s]",
	                zMrcpPort, errno, strerror(errno));
	        }
			if ( (rc = resetConnection(zPort)) == -1 )
			{
				return(-1);
			}
			zMrcpPort = gSrPort[zPort].getSocketfd();
		}
//	}
	
	/*Not all of 20 chars received??*/
	if(receivedLen < numHeaderBytes)
	{
		int yIntTmpNumHeaderBytes = numHeaderBytes - receivedLen;
		usleep(1000);
		memset((char *)lBuf, '\0', sizeof(lBuf));
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Not all %d header size received (%d) receivedLen.  Must read more.", 
				numHeaderBytes, receivedLen);
		receivedLen = read(zMrcpPort, lBuf, yIntTmpNumHeaderBytes);

		if(receivedLen < yIntTmpNumHeaderBytes)
		{
  			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Failed to read 20 bytes of header line from the MRCP "
				"socket in two attempts. port=%d", zMrcpPort);
			return(-1);
		}

		strcat(tmpBuf, lBuf);
	}

	if ( strncmp("MRCP/2.0", tmpBuf, 8) != 0 )
	{
  		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"Received non-MRCP2.0 message (%s) from mrcp port %d.  "
			"Expecting MRCP/2.0 ...", tmpBuf, zMrcpPort);
		return(-1);
	}

	p = tmpBuf + strlen("MRCP/2.0 ");	// p now points to the msg size.
	memset((char *)bytesToReadBuf, '\0', sizeof(bytesToReadBuf));
	i = 0;
	while ( isdigit(*p) )
	{
		bytesToReadBuf[i++] = *p;
		p++;
	}

	bytesToRead = atoi(bytesToReadBuf);

	if ( bytesToRead < numHeaderBytes )
	{
  		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"Received corrupt MRCP2.0 message from mrcp port %d.  "
			"Expecting size > %d.  Received size=%d.",
			 numHeaderBytes, bytesToRead);
		return(-1);
	}
	sprintf(zRecvBuf, "%s", tmpBuf);

	// Now read the the exact number of bytes.
	int	byteCount;
	byteCount = numHeaderBytes;
	receivedLen = 0 ;
	memset((char *)lBuf, '\0', sizeof(lBuf));
	while ( byteCount < bytesToRead )
	{
		i = bytesToRead - byteCount;
		if ( (receivedLen = read(zMrcpPort, (void *)lBuf, i)) < 0 )
		{
            if ( gSrPort[zPort].IsStopProcessingTTS() )
            {
                mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                    REPORT_VERBOSE, MRCP_2_BASE, INFO,
                    "Failed to read from the MRCP socket (%d). [%d, %s]",
                    zMrcpPort, errno, strerror(errno));
            }
            else
            {
                mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                    REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "Failed to read from the MRCP socket (%d). [%d, %s]",
                    zMrcpPort, errno, strerror(errno));
            }
			return(-1);
		}
		strcat(zRecvBuf, lBuf);
	
		byteCount += receivedLen;
	}
	
	if ( byteCount != bytesToRead )
	{
  		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_NORMAL, MRCP_2_BASE, ERR,
			"Failed to obtain valid MRCP message.  Read %d bytes; "
			"expecting %d.", byteCount, bytesToRead);
		return(-1);
	}
	
	return(bytesToRead);

} // END: doControlledRead()

/*------------------------------------------------------------------------------
sGetHeaderContent():
------------------------------------------------------------------------------*/
int sGetHeaderContent(string &serverMsg, string &headerMsg, 
					string &contentMsg)
{
	static char	mod[] = "sGetHeaderContent";
	static char	contentLenStr[32] = "Content-Length:";
	int		rc;
	
	rc = serverMsg.find(contentLenStr);
	if ( rc == -1 )
	{
		headerMsg = serverMsg;
		contentMsg = "";
		return(0);
	}

	char	*pServerMsg;
	char 	*p1;
	char 	*p2;
	if ( (pServerMsg = (char *) calloc( serverMsg.size() + 1,
												sizeof(char) )) == NULL )
    {
  		mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
                "Error: unable to allocate memory. [%d, %s].  "
                "Cannot parse message.", errno, strerror(errno));
        return(-1);
    }

	sprintf(pServerMsg, "%s", serverMsg.c_str());

    p1 = strstr(pServerMsg, contentLenStr);
	p2 = strstr(p1, "\n");
	p2 += 1;
	*p2 = '\0';
	p2++;

	while ( isspace(*p2) )
	{
		p2++;
	}
	
	headerMsg = pServerMsg;
	contentMsg = p2;
		
	free(pServerMsg);
	return(0);
} // END: sGetHeaderContent()

//============================================
int writeData(int zPort, int sockfd, string& data)
//============================================
{
	char mod[] ="writeData";
	int nCurrent = 0; 
	int nSent = 0;
	time_t			t1;
	time_t			t2;

	int nbytes = data.size(); 
//	int nbytes = data.size()  + 1; 
	char *buff = new char[nbytes + 1];
	memset(buff, 0, nbytes);


	//sprintf(buff, "%s", data.data());
	sprintf(buff, "%s", data.c_str());

	time(&t1);
	while (1)
	{

		nCurrent = write(sockfd, &buff[nSent], nbytes-nSent);
		if ( nCurrent < 0 && errno != EAGAIN )
		{
            if ( gSrPort[zPort].IsStopProcessingTTS() )
            {
                mrcpClient2Log(__FILE__, __LINE__, zPort,
                    mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
                    "Failed to write %d bytes (%s) to socket fd %d.  [%d, %s]",
                    nbytes-nSent, &buff[nSent], sockfd, errno, strerror(errno));
            }
            else
            {
                mrcpClient2Log(__FILE__, __LINE__, zPort,
                    mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "Failed to write %d bytes (%s) to socket fd %d.  [%d, %s]",
                    nbytes-nSent, &buff[nSent], sockfd, errno, strerror(errno));
            }

			delete [] buff;
			return(-1);
		}
        else if(nCurrent < 0 && errno == EAGAIN)	// MR 5008
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort,
                mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "EAGAIN: write() failed to write %d bytes; rc=%d", nbytes-nSent, nCurrent);
			time(&t2);
            if ( t2 - t1 > 3 )  // if can't write in 3 seconds, call it quits
            {
                mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                    REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "Returning failure after 3 seconds of attempting to write() to socket.");
                delete [] buff;
                return(-1);
            }
            usleep(200);
            if ( nCurrent >= 0 )
            {
                nSent += nCurrent;
                if ( 0 == (nbytes - nSent))
                {
                    break;
                }
            }
            continue;
        }
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort,
				mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Wrote (%.*s) to socket fd %d: %d bytes.",
				600, &buff[nSent], sockfd, nbytes-nSent);
		}

		nSent += nCurrent;
		if (0 == (nbytes - nSent))
		{
			break;
		}
	}

	delete [] buff;

	if (nSent < nbytes)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort,
			mod, REPORT_NORMAL, MRCP_2_BASE, INFO,
			"Write (socketfd=%d) failed.  Wrote %d of %d bytes.  [%d:%s]",
			 sockfd, nSent, nbytes, errno, strerror(errno));
		return(-1);
	}

    return(0);

} //END: writeData()

/*------------------------------------------------------------------------------
cleanUp():
	* clear grammarList
	* release a resource (if reserved)
	* destroy licenseLock mutex
------------------------------------------------------------------------------*/
int cleanUp(int zPort)
{
	static char	mod[]="cleanUp";
    int     	rc;
	int			dynMgrFd;
	int			tmpFd;
	string		tmpFifo;

	rc = ttsReleaseResource(zPort);

	dynMgrFd = gSrPort[zPort].getFdToDynMgr();
	if ( dynMgrFd  > 0 )
	{
		close(dynMgrFd);
	}

		// We have to save the application response fifo information.  
	tmpFd	= gSrPort[zPort].getAppResponseFd();
	tmpFifo	= gSrPort[zPort].getAppResponseFifo();

	if ( tmpFd != -1 )
	{
		gSrPort[zPort].setAppResponseFifo(tmpFifo);
		gSrPort[zPort].setAppResponseFd(tmpFd);
	}
	gSrPort[zPort].destroyMutex();
	gSrPort[zPort].eraseCallMapByPort(zPort);

	return(0);
} // END: cleanUp()

/*------------------------------------------------------------------------------
sendRequestToDynMgr():
------------------------------------------------------------------------------*/
int sendRequestToDynMgr(int zPort, char *mod, struct MsgToDM *request)
{
    int     rc;
	static int yIntSendCount = 0;

	yIntSendCount++;

	switch(request->opcode)
	{
		case DMOP_TTS_COMPLETE_SUCCESS:
		case DMOP_TTS_COMPLETE_FAILURE:
		case DMOP_TTS_COMPLETE_TIMEOUT:
			sprintf(request->data, "Sender:%d, count(%d)",
				getpid(), yIntSendCount);
			break;
	}

    rc = write(gSrPort[zPort].getFdToDynMgr(),
                (char *)request, sizeof(struct MsgToDM));
    if (rc == -1)
    {
        if ( gSrPort[zPort].IsStopProcessingTTS() )
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort,
                 mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
                    "Unable write message to dynmgr fifo (fd=%d). [%d, %s].",
                    gSrPort[zPort].getFdToDynMgr(), errno, strerror(errno));
        }
        else
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_NORMAL, MRCP_2_BASE, ERR,
                "Unable write message to dynmgr fifo (fd=%d). [%d, %s].",
                gSrPort[zPort].getFdToDynMgr(), errno, strerror(errno));
        }
        return(-1);
    }

    mrcpClient2Log(__FILE__, __LINE__, zPort,
        	mod, REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Sent %d bytes to dynmgr fifo. "
			"msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d,data:%s}",
            rc,
            request->opcode,
            request->appCallNum,
            request->appPid,
            request->appRef,
            request->appPassword,
            request->data);

    return(rc);
} // END: sendRequestToDynMgr()

/*------------------------------------------------------------------------------
sWaitForLicenceReserved():
	Check if IsLicenseReserved return zTrueOrFalse.  The sipThread updates
	the licenseReserved member; have to wait for it.
------------------------------------------------------------------------------*/
int sWaitForLicenceReserved(int zPort, bool zTrueOrFalse)
{ 
	static char mod[]="sWaitForLicenceReserved";
	time_t	startTime;
	time_t	stopTime;
	time_t	currentTime = 0;

	time(&startTime);
	stopTime = startTime + 5;		 // give it 3 seconds
	while ( currentTime < stopTime )
	{
		if ( gSrPort[zPort].IsLicenseReserved() == zTrueOrFalse )
		{
			return(0);			// Got the license
		}

		if ( gSrPort[zPort].IsLicenseRequestFailed() )
		{
			return(-1);			// Some type of failure
		}

		usleep(1000);
		time(&currentTime);

	}
	return(-1);			// Timed out. 

} // END: sWaitForLicenceReserved()

/*------------------------------------------------------------------------------
myGetProfileString():
------------------------------------------------------------------------------*/
int myGetProfileString(char *section,
					char *key, char *defaultVal, char *value,
					long bufSize, FILE *fp, char *file)
{
	static char	yMod[] = "sGetProfileString";
	char		line[1024];
	char		currentSection[80],lhs[512], rhs[512];
	short		found, inSection;
	char 		*ptr;
	int		findSection;
	int			rc;

	sprintf(value, "%s", defaultVal);

	findSection = 0;
	if (strlen(section) > 0)
	{
		findSection = 1;
	}

	if(key[0] == '\0')
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Error: No key received.  retruning failure.");
		return(-1);
	}
	if(bufSize <= 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Error: Return bufSize (%ld) must be > 0", bufSize);
		return(-1);
	}

	rewind(fp);
	memset(line, 0, sizeof(line));
	inSection = 0;
	found = 0;
	while(fgets(line, sizeof(line)-1, fp) != NULL)
	{
		/*
		 * Strip \n from the line if it exists
		 */
		if(line[(int)strlen(line)-1] == '\n')
		{
			line[(int)strlen(line)-1] = '\0';
		}

		myTrimWS(line);

		if (findSection)
		{
			if(!inSection)
			{
				if(line[0] != '[')
				{
					memset(line, 0, sizeof(line));
					continue;
				}
	
				memset(currentSection, 0,
						sizeof(currentSection));
				sscanf(&line[1], "%[^]]", currentSection);
	
				if(strcmp(section, currentSection) != 0)
				{
					memset(line, 0, sizeof(line));
					continue;
				}
	
				inSection = 1;
				memset(line, 0, sizeof(line));
				continue;
			} /* if ! inSection */
			else
			{
				/*
				 * If we are already in a section and have
				 * encountered another section, get out of
				 * the loop.
				 */
				if(line[0] == '[')
				{
					memset(line, 0, sizeof(line));
					break;
				}
			} /* if inSection */
		} /* if findSection */
	
		memset(lhs, 0, sizeof(lhs));
		memset(rhs, 0, sizeof(rhs));

		if ( strncmp(line, "payloadType", strlen("payloadType")) == 0 )
		{
			rc = sscanf(line, "%[^=]=%[^=]", lhs, rhs);
		}
		else
		{
			rc = sscanf(line, "%[^=]=%s", lhs, rhs);
		}

		if (rc < 2)
		{
			if ((ptr = (char *)strchr(line, '=')) == (char *)NULL)
			{
				memset(line, 0, sizeof(line));
				continue;
			}
		}

		myTrimWS(lhs);
		myTrimWS(rhs);

		if(strcmp(lhs, key) != 0)
		{
			memset(line, 0, sizeof(line));
			continue;
		}

		found = 1;

		sprintf(value, "%.*s", bufSize, rhs);

		break;
	} /* while != NULL */

	if(!found)
	{
//		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
//				REPORT_NORMAL, MRCP_2_BASE, ERR,
//				"Error: Failed to find value of key(%s) under "
//				"section(%s) in file (%s).", key, section, file);
		return(-1);
	}

	return(0);
} /* END myGetProfileString() */

/*------------------------------------------------------------------------------
myTrimWS():
------------------------------------------------------------------------------*/
void myTrimWS(char *zStr)
{
	char 		*p1;
	char 		*p2;

	p1 = (char *)calloc(strlen(zStr) + 1, sizeof(char));

	sprintf(p1, "%s", zStr);

	p2 = p1;

	if ( isspace(*p2) )
	{
		while(isspace(*p2))
		{
			p2++;
		}
	}

	sprintf(p1, "%s", p2);
	p2 = &(p1[strlen(p1)-1]);
	while(isspace(*p2))
	{
		*p2 = '\0';
		p2--;
	}

	sprintf(zStr, "%s", p1);
	free(p1);
} // END: myTrimSW()

/*------------------------------------------------------------------------------
getRtpInfo()
------------------------------------------------------------------------------*/
void getRtpInfo(int zPort, char *zPayloadStr, char *zCodecStr,
                int *zPayloadType, int *zCodecType)
{
	static		char mod[]="getRtpInfo";
    FILE        *pFp;
	int			tmpInt;
	int			i;
	char		crlf[8];
	char		*ispbase;
	char		rtpFile[256];
	char		tmpStr[64];

	typedef struct
	{
		int		index;
		int		codecType;
		char	codecDesc[64];
	} codecInfoStruct;

	codecInfoStruct 	codecs[30] = 
	{
		{ 0, 0, "pcmu/8000" },
		{ 1, 1, "1016/8000" },
		{ 2, 2, "g721/8000" },
		{ 3, 3, "gsm/8000" },
		{ 4, 4, "g723/8000" },
		{ 5, 5, "dvi4/8000" },
		{ 6, 6, "dvi4/16000" },
		{ 7, 7, "lpc/8000" },
		{ 8, 8, "pcma/8000" },
		{ 9, 9, "g722/8000" },
		{ 10, 10, "l16/44100" },
		{ 11, 11, "l16/44100" },
		{ 12, 12, "qcelp/8000" },
		{ 13, 13, "cn/8000" },
		{ 14, 14, "mpa/90000" },
		{ 15, 15, "g728/8000" },
		{ 16, 16, "dvi4/11025" },
		{ 17, 17, "dvi4/22050" },
		{ 18, 18, "g729/8000" },
		{ 19, 19, "reserved" },
		{ 20, 25, "cellb/90000" },
		{ 21, 26, "jpeg/90000" },
		{ 22, 28, "nv/90000" },
		{ 23, 31, "h261/90000" },
		{ 24, 32, "mpv/90000" },
		{ 25, 33, "mp2t/90000" },
		{ 26, 34, "h263/90000" },
		{ 27, 96, "telephone-event/8000" },
		{ 28, 101, "telephone-event/8000" },
		{ 29, -1, "" }
	};

	// Set defaults
			// default payload Type
	int	payloadType = atoi(gMrcpInit.getPayloadType(TTS).c_str());
	zPayloadStr[0] = '\0';

			// default codec Type
	*zCodecType = atoi(gMrcpInit.getCodecType(TTS).c_str());;
	memset((char *)tmpStr, '\0', sizeof(tmpStr));
	for (i = 0; codecs[i].codecType != -1; i++)
	{
		if (*zCodecType == codecs[i].codecType )
		{
			sprintf(tmpStr, "%s", codecs[i].codecDesc);
			break;
		}
	}

	if ( tmpStr[0] == '\0' )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_VERBOSE, MRCP_2_BASE, WARN,
				"Invalid default codec type (%d) specified in the arcMRCP2.cfg "
				"file.  Using default of 0.", *zCodecType);
		*zCodecType	= 0;
		sprintf(zCodecStr, "%s", "pcmu/8000");
	}
	else
	{
		sprintf(zCodecStr, "%s", tmpStr);
	}

	// now defaults are set.
    if((ispbase = (char *)getenv("ISPBASE")) == NULL)
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN,
			"Env. var. ISPBASE is not set; using default codec (%d)",
			zCodecType);
    }

    sprintf(rtpFile, "%s/Telecom/Exec/.mrcpRtpDetails.%d",
                    ispbase, zPort);

    if ( access (rtpFile, F_OK) != 0 )
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Failed to access rtp info file (%s) for reading; "
                " using default codec (%d).", rtpFile, *zCodecType);
        tmpInt = *zCodecType;
    }
    else
    {
        if ((pFp = fopen(rtpFile,"r")) == NULL )
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                    REPORT_VERBOSE, MRCP_2_BASE, INFO,
                    "Failed to open rtp info file (%s) for reading; "
                    " using default codec (%d).", rtpFile, *zCodecType);
            tmpInt = *zCodecType;
        }
        else
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Successfully opened config file (%s) for reading.", rtpFile);

            (void) myGetProfileString("", "codecType", "0",
             tmpStr, sizeof(tmpStr) - 1, pFp, rtpFile);
            fclose(pFp);
            tmpInt = atoi(tmpStr);
        }
    }

	memset((char *)tmpStr, '\0', sizeof(tmpStr));
	for (i = 0; codecs[i].codecType != -1; i++)
	{
		if (tmpInt == codecs[i].codecType )
		{
			sprintf(tmpStr, "%s", codecs[i].codecDesc);
			break;
		}
	}

	if ( tmpStr[0] == '\0' )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Error: unable to find description for codec type (%d).  "
                "Using (%s).", *zCodecType, zCodecStr);
	}
	else
	{
		*zCodecType = tmpInt;
		sprintf(zCodecStr, "%s", tmpStr);
	}

	tmpInt = payloadType;
	memset((char *)tmpStr, '\0', sizeof(tmpStr));
	for (i = 0; codecs[i].codecType != -1; i++)
	{
		if (tmpInt == codecs[i].codecType )
		{
			sprintf(tmpStr, "%s", codecs[i].codecDesc);
			break;
		}
	}

	if ( tmpStr[0] == '\0' )
	{
		*zPayloadType = 96;
		sprintf(zPayloadStr, "%s", "telephone-event/8000");
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Error: unable to find description for payload type (%d).  "
                "Using (%s).", *zPayloadType, zPayloadStr);
	}
	else
	{
		*zPayloadType = tmpInt;
		sprintf(zPayloadStr, "%s", tmpStr);
	}
	return;
} // END: getRtpInfo()

/*------------------------------------------------------------------------------
ttsReleaseResource():
------------------------------------------------------------------------------*/
int ttsReleaseResource(const int zPort)
{
	char	mod[] ="ttsReleaseResource";
	int		cid, did;
	int		rc = -1;
	int		socketfd;
#ifdef EXOSIP_4
    struct eXosip_t *eXcontext;
#endif // EXOSIP_4 

	if ( ! gSrPort[zPort].IsLicenseReserved() )
	{
		return(0);		// no need to send BYE, no license is reserved.
	}

    mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
         MRCP_2_BASE, INFO, "Attempting to release a resource.");

    // cid and did are set by sipThread when get INVITE response.
    cid = gSrPort[zPort].getCid();
    did = gSrPort[zPort].getDid();

#ifdef EXOSIP_4
    eXcontext=gMrcpInit.getExcontext();
#endif // EXOSIP_4 

	// send sip BYE
    if ( (cid <= 0) || (did <= 0) )
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
            MRCP_2_BASE, ERR, "Invalid cid(%d) or did(%d). Unable to send BYE.",
			cid, did);
		return(-1);
    }
   
   	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO,
				"Sending BYE for callId (%d) and did (%d).", cid, did);
#ifdef EXOSIP_4
    eXosip_lock (eXcontext);
    rc = eXosip_call_terminate(eXcontext, cid, did); // send SIP BYE to server
    eXosip_unlock (eXcontext);
#else
	eXosip_lock ();
	rc = eXosip_call_terminate(cid, did); // send SIP BYE to server
	eXosip_unlock ();
#endif // EXOSIP_4 

	socketfd = gSrPort[zPort].getSocketfd();
	if ( socketfd >= 0 )
	{
//		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
//			REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"Closing MRCP socket fd (%d).", socketfd);
		
		rc = close(socketfd);
//		if( rc == 0)
//		{
//			mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
//				REPORT_VERBOSE, MRCP_2_BASE, INFO,
//				"socket(%d) closed.", socketfd);
//		}
	}
	gSrPort[zPort].setLicenseReserved(false);
	gSrPort[zPort].setSocketfd(zPort, -1);

    if (rc < 0)
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
                MRCP_2_BASE, ERR, "Failed to send BYE. "
        "%d = eXosip_terminate_call(%d, %d);", rc, cid, did);
		return(-1);
    }

	return(0);

} // END: ttsReleaseResource()

//===============================
int build_tcp_connection(int zPort, const string& serverName, 
						const int serverPort)
//===============================
// on success: return socket number
// on failure: return -1
{
	char mod[] = "build_tcp_connection"; 
	int rc = -1; 

	int sockfd, numbytes;
	struct hostent *he;
	struct sockaddr_in serverAddr; // connector's address info 

    struct timeval      timeout;
    fd_set              set;

	if ((he = gethostbyname(serverName.c_str())) == NULL) //get host info 
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR,
			"Error: gethostbyname failed for server(%s). Unable to connect to "
			"MRCP Server", serverName.c_str());
		return rc;
	}

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"hostname = %s, sreverPort=%d", serverName.c_str(), serverPort);

	if ((sockfd = socket(PF_INET, SOCK_STREAM, 6)) == -1) 
	{
  		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, "socket() failed. Unable to connect to "
            "MRCP Server");
		return rc;
	}

	serverAddr.sin_family = PF_INET;         // host byte order 
	//serverAddr.sin_family = AF_INET;         // host byte order 
	serverAddr.sin_port = htons(serverPort);       // short, network byte order 
	serverAddr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(serverAddr.sin_zero), 8);        // zero the rest of the struct

    if ((rc = fcntl(sockfd, F_SETFL, O_NONBLOCK)) == -1)
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, WARN,
                "fcntl() failed to set socket fd to nonblocking for connect().  Continuing.");
    }

	rc = connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr));
	if ( rc == -1 )
    {
        if ( errno != EINPROGRESS )
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_NORMAL, MRCP_2_BASE, ERR,
                "connect() failed.  Unable to establish connection to the MRCP Server. [%d, %d]",
                rc, errno);
            close(sockfd);
            return rc;
        }
    }

    mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
        REPORT_VERBOSE, MRCP_2_BASE, INFO,
        "Nonblocking connect() to MRCP Server succeeded. [%d, %d]", rc, errno);

    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    timeout.tv_sec  = 3;
    timeout.tv_usec = 0;
    rc = select( sockfd + 1, NULL, &set, NULL, &timeout );
    if (rc == -1)
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_NORMAL, MRCP_2_BASE, ERR,
            "select() failed.  Unable to establish connection to the MRCP Server. rc = %d.  [%d, %s]",
            rc, errno, strerror(errno));
        if ( sockfd )
        {
            close(sockfd);
        }
        return(rc);
    }
    else if (rc)
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Successfully built connection to MRCP Server.");
        return(sockfd);
    }
    else
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_NORMAL, MRCP_2_BASE, ERR,
            "select() timed out.  Unable to connect() to MRCP Server within 3 seconds. "
            "Returning failure.");
        if ( sockfd )
        {
            close(sockfd);
        }
        return(-1);
    }

} // END:  build_tcp_connection()

/*------------------------------------------------------------------------------
int resetConnection():
------------------------------------------------------------------------------*/
int resetConnection(int zPort)
{
	char mod[] = "resetConnection"; 
	int rc = -1; 

	int sockfd; 
	int	serverPort;
	struct hostent *he;
	struct sockaddr_in serverAddr; // connector's address info 
	string	serverIP;

    struct timeval      timeout;
    fd_set              set;

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_DETAIL, MRCP_2_BASE, INFO,
			"Attempting to reset network connection.");

	sockfd = gSrPort[zPort].getSocketfd();
	if ( sockfd >= 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Closing MRCP socket fd (%d).", sockfd);
		
		rc = close(sockfd);
		if( rc == 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"socket(%d) closed.", sockfd);
		}
	}
	else
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, INFO,
			"Socket fd is not valid (%d).  Unable to close socket and reset connection.", sockfd);
		return(-1);
	}

	serverIP = gSrPort[zPort].getServerIP();

	if ((he = gethostbyname(serverIP.c_str())) == NULL) //get host info 
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR,
			"Error: gethostbyname failed for server(%s). Unable to connect to "
			"MRCP Server", serverIP.c_str());
		return rc;
	}

	if ((sockfd = socket(PF_INET, SOCK_STREAM, 6)) == -1) 
	{
  		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, "socket() failed. Unable to connect to MRCP Server");
		return(rc);
	}

	serverPort = gSrPort[zPort].getMrcpPort();
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
		REPORT_VERBOSE, MRCP_2_BASE, INFO, "Successfully opened socket; fd=%d; serverPort=%d", sockfd, serverPort);
	gSrPort[zPort].setSocketfd(zPort, sockfd);

	serverAddr.sin_family = PF_INET;         // host byte order 
	//serverAddr.sin_family = AF_INET;         // host byte order 
	serverAddr.sin_port = htons(serverPort);       // short, network byte order 
	serverAddr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(serverAddr.sin_zero), 8);        // zero the rest of the struct

    if ((rc = fcntl(sockfd, F_SETFL, O_NONBLOCK)) == -1)
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, WARN,
                "fcntl() failed to set socket fd to nonblocking for connect().  Continuing.");
    }

	rc = connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr));
	if ( rc == -1 )
    {
        if ( errno != EINPROGRESS )
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_NORMAL, MRCP_2_BASE, ERR,
                "connect() failed.  Unable to establish connection to the MRCP Server. [%d, %d]",
                rc, errno);
            close(sockfd);
            return rc;
        }
    }

    mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
        REPORT_VERBOSE, MRCP_2_BASE, INFO,
        "Nonblocking connect() to MRCP Server succeeded. [%d, %d]", rc, errno);

    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    timeout.tv_sec  = 3;
    timeout.tv_usec = 0;
    rc = select( sockfd + 1, NULL, &set, NULL, &timeout );
    if (rc == -1)
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_NORMAL, MRCP_2_BASE, ERR,
            "select() failed.  Unable to establish connection to the MRCP Server. rc = %d.  [%d, %s]",
            rc, errno, strerror(errno));
        if ( sockfd )
        {
            close(sockfd);
        }
        return(rc);
    }
    else if (rc)
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Successfully built connection to MRCP Server.");
        return(sockfd);
    }
    else
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_NORMAL, MRCP_2_BASE, ERR,
            "select() timed out.  Unable to connect() to MRCP Server within 3 seconds. "
            "Returning failure.");
        if ( sockfd )
        {
            close(sockfd);
        }
        return(-1);
    }

	return(0);

} // END:  resetConnection()

/*------------------------------------------------------------------------------
sIsDataOnMrcpSocket():
	Return values:
		-1  - failure
		1	- there is data on the socket; proceed with reading
		0	- there is NO data on the socket.
------------------------------------------------------------------------------*/
int sIsDataOnMrcpSocket(int zPort, int zMrcpPort, int zMilliSeconds)
{
	static char		mod[]="sIsDataOnMrcpSocket";
	fd_set          readFdSet;
	struct timeval	tval;
	int				nEvents;

	if ( zMrcpPort < 0 )
	{
        if ( gSrPort[zPort].IsStopProcessingTTS() )
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort,
                 mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Invalid MRCP Port received (%d).  Unable to attempt "
                "to check for MRCP data on socket.", zMrcpPort);
        }
        else
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_NORMAL, MRCP_2_BASE, ERR,
                "Invalid MRCP Port received (%d).  Unable to attempt "
                "to check for MRCP data on socket.", zMrcpPort);
        }
		return(-1);
	}

	FD_ZERO (&readFdSet);
	FD_SET(zMrcpPort, &readFdSet);
	tval.tv_sec     = 0;
	tval.tv_usec    = zMilliSeconds;

	nEvents =  select (zMrcpPort + 1, &readFdSet, NULL, NULL, &tval);
		
	if ( nEvents < 0 )
	{
        if ( gSrPort[zPort].IsStopProcessingTTS() )
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Select failed.  Will be unable to read from MRCP socket.  "
                "[%d, %s]", errno, strerror(errno));
        }
        else
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_NORMAL, MRCP_2_BASE, ERR,
                "Select failed.  Will be unable to read from MRCP socket.  "
                "[%d, %s]", errno, strerror(errno));
        }

		return(-1);
	}
		
	if ( nEvents == 0 )
	{
		return(0);
	}
	
	return(1);
} // END: sIsDataOnMrcpSocket()