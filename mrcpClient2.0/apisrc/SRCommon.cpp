/*-----------------------------------------------------------------------------
Program:SRCommon.c

Author:	Aumtech, Inc.
Update:	08/01/2006 djb Ported to mrcpV2.
Update:	10/18/2006 djb Increased the recvBuf from 1024 to 2048 to avoid a core
Update:	12/06/2006 djb Added mrcpServerIP connection logic in srGetResource()
Update:	08/30/2010 djb MR 3099. Modified readMrcpSocket() to validate the
       	               the response/event.  See MR 3011 for details.
Update:	08/20/2014 djb MR 4241. Reverted back to old code prior to change
Update:	08/20/2014 djb MR 4241/4366. Changed 5 seconds to 3 seconds wait for 
                       response.
Update: 08/22/2014 djb MR 4241. changed connect() to unblocking followed by select.
Update: 08/22/2014 djb MR 4241. changed invite timeout from 5 to 3 seconds
------------------------------------------------------------------------------*/

#include <fstream>
#include <string>

#ifdef EXOSIP_4
#include <osipparser2/osip_message.h>
#include <eXosip2/eXosip.h>
#else
#include <osip_message.h>
#include <eXosip2/eXosip.h>
#include <eXosip2.h>
#endif // EXOSIP_4

#include "mrcpCommon2.hpp"
#include "mrcp2_HeaderList.hpp"
#include "mrcp2_Request.hpp"
#include "mrcp2_Response.hpp"
#include "mrcpSR.hpp"
#include "mrcp2_Event.hpp"

extern "C"
{
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include <sys/time.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
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

extern int gCanProcessReaderThread;
extern int gCanProcessSipThread;
extern int gCanProcessMainThread;

// extern int					outofsyncEventReceived;
// extern EventInfoChar		outofsyncEventInfo;
// extern Mrcp2_Event			outofsyncMrcpv2Event;


// static string getHostIpAddress (const string& zHostName);
string createSdpOffer (int audioPort, int voiceCodec, int telephonyPayload,
				char *zCodecStr, char *zPayloadStr);
int send_sip_invite(int zPort, const string& fromUrl,
                        const string& toUrl,
                        const string& route,
                        const string& subject,
                        const string& sdpOffer);
int sLoadGrammar(int zPort, int zGrammarType, string & zGrammarName,
					string & zGrammarContent, float yWeight);
int writeData(int zPort, int sockfd, string& data);
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


//=========================================
int srGetResource(const int zPort)
//=========================================
{
	char 	mod[] ="srGetResource";
    char    destUrl[256];
    char    fromUrl[256];
    int		rc=-1;
    int		localSdpPort;
    string	sdpOffer;
    string	sipRoute="";
    string	sipSubject="";
    string	mrcpIPAddress="";

    int		voiceCodec = 0;
    int		telephonePayload = 101;
	char	payLoadStr[128];
	char	codecStr[128];
	int		socketFd;
	int		cid, did;

    string  requestURI;
    string  serverIP;

	int		uriIndex = -1;
	int		nextUriIndex = -1;
	int		startingUriIndex=0;
    int     numURIs;
    int     i, j;
	int		myServerPort;

	if ( gSrPort[zPort].IsLicenseReserved() )
	{
		return(0);		// Already have a license.  Just return success.
	}

	numURIs = gMrcpInit.getNumURIs();
    for (i=0; i < numURIs; i++)
	{
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "numURIs=%d;  i=%d", numURIs, i);

        if ( (uriIndex = gMrcpInit.getServerUri(zPort, SR, nextUriIndex, requestURI, serverIP)) < 0 )
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

	    // build and send INVITE to server
	    // --------------------------------
		sprintf (fromUrl, "<sip:%d@%s>", zPort,
								gMrcpInit.getLocalIP().c_str() );
	    //sprintf (destUrl, "%d@%s",		zPort, gMrcpInit.getRequestURI(SR).c_str());
	    sprintf (destUrl, "%s",	requestURI.c_str());
	
	    sipRoute = "";
	    sipSubject="send INVITE";
	
	    // construct SDP message body
	    localSdpPort = LOCAL_STARTING_MRCP_PORT + zPort;
        
		getRtpInfo(zPort, payLoadStr, codecStr, &telephonePayload, &voiceCodec);
	
	    sdpOffer = createSdpOffer (localSdpPort, voiceCodec, telephonePayload,
					codecStr, payLoadStr);
		
		pthread_mutex_lock( &callMapLock);
	    rc = send_sip_invite(zPort, fromUrl, destUrl, sipRoute, sipSubject,
					sdpOffer);
	    // set INVITE callId
	    if (rc < 0)
	    {
	        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, "Attempt to send INVITE from %s to %s failed.",
				fromUrl, destUrl);
			gMrcpInit.disableServer(__LINE__, (int)zPort, requestURI);

			pthread_mutex_unlock( &callMapLock);
			continue;
	    }
		gSrPort[zPort].setCid(rc);
		gSrPort[zPort].setCancelCid(0);
	
	   	callMap[rc] = zPort;
		pthread_mutex_unlock( &callMapLock);
	
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, "Successfully sent sip INVITE to server.  Now "
			"wait for the response.");
	
		if ( (rc=sWaitForLicenceReserved(zPort, true)) == -1 )
		{
		    cid = gSrPort[zPort].getCid();
			gSrPort[zPort].setCancelCid(cid);
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, "Failed to reserve a license to (%s).  Set cancelCid to %d.",
                requestURI.c_str(), gSrPort[zPort].getCancelCid());
	
#if 0
		    // cid and did are set by sipThread when get INVITE response.
		    cid = gSrPort[zPort].getCid();
		    // did = gSrPort[zPort].getDid();
			eXosip_lock ();
            rc = eXosip_call_terminate(cid, -1); // send SIP BYE to server
            eXosip_unlock ();
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
						MRCP_2_BASE, INFO, "Called eXosip_call_terminate(%d, -1)", cid);
#endif
		
			gMrcpInit.disableServer(__LINE__, (int)zPort, requestURI);
			continue;
		}
		
#if 0
		if ( gSrPort[zPort].getMrcpConnectionIP().empty() )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, "INVITE connection IP address is empty.  "
				"Using IP (%s) from config file.", 
				gSrPort[zPort].getServerIP(SR).c_str());
	    	mrcpIPAddress = gMrcpInit.getServerIP(SR);
		}
		else
		{
			mrcpIPAddress = gSrPort[zPort].getMrcpConnectionIP();
		}
#endif
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

		socketFd = build_tcp_connection(zPort, serverIP, myServerPort);
		if ( socketFd < 0)
		{
			gSrPort[zPort].setCancelCid(1);
			gMrcpInit.disableServer(__LINE__, (int)zPort, requestURI);
//			return(-1);
			continue;
	    }
		gSrPort[zPort].setSocketfd(zPort, socketFd);
		gMrcpInit.enableServer(__LINE__, (int)zPort, requestURI);
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			 MRCP_2_BASE, INFO, "Successfully reserved a license (mrcp port=%d, socketfd=%d).",
					myServerPort, gSrPort[zPort].getSocketfd());

		return(0);
	}

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR, "Attempts to all Servers have failed. "
			"Unable to reserve a resource.");
	return(-1);

} // END:srGetResource()

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
	char tmp[4096];
	char localIp[128];
	char yStrTelephonyPayload[100];
	
	string loginNameOnLocalHost = string("arc");	
	int sessionId = 123456789;	
	int intVersionNum = 2006;
	int cmid = 1;    // control media id
	int mid = cmid;  // media id
#ifdef EXOSIP_4
    struct eXosip_t     *eXcontext;
//    string              pLocalIP;

    eXcontext=gMrcpInit.getExcontext();
//	pLocalIP = gMrcpInit.getLocalIP();
//	sprintf(localIp, "%s", pLocalIP.c_str());
    eXosip_guess_localip (eXcontext, AF_INET, localIp, 128);
#else
	eXosip_guess_localip (AF_INET, localIp, 128);
#endif // EXOSIP_4

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "localIp:(%s) voiceCodec=%d, zCodecStr=%s", localIp, voiceCodec, zCodecStr);
		
#if 0
	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "before sprintf loginNameOnLocalHost=%s", loginNameOnLocalHost.c_str());
	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "before sprintf sessionId=%d intVersionNum=%d", sessionId, intVersionNum);
	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "before sprintf localIp=%s", localIp);
	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "cmid=%d, audioPort=%d, voiceCodec=%d, telephonyPayload=%d", cmid, audioPort, voiceCodec, telephonyPayload);
	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "localIp:(%s) voiceCodec=%d, zCodecStr=%s", localIp, voiceCodec, zCodecStr);
	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "telephonyPayload=%d, zPayloadStr=%s", telephonyPayload, zPayloadStr);
#endif
	sprintf (tmp,
		 	"v=0\r\n"
			"o=%s %d %d IN IP4 %s\r\n"
			"s=SIP Call\r\n"
			"c=IN IP4 %s\r\n"
		 	"t=0 0\r\n"
			"m=application 9 TCP/MRCPv2\r\n"
			"a=setup:active\r\n"
			"a=connection:new\r\n"
			"a=resource:speechrecog\r\n"
			"a=cmid:%d\r\n"
			"m=audio %d RTP/AVP %d %d\r\n"
		 	"a=rtpmap:%d %s\r\n"
		 	"a=rtpmap:%d %s\r\n"
		 	"a=fmtp:96 0-15\r\n" 
		 	"a=sendonly\r\n" 
			"a=mid:%d\r\n",
			loginNameOnLocalHost.c_str(), sessionId, intVersionNum, localIp,
			localIp,
			cmid,
		 	audioPort, voiceCodec, telephonyPayload,
			voiceCodec, zCodecStr, 
		 	telephonyPayload, zPayloadStr,
		 	mid);

			//"m=audio %s RTP/AVP 18 0 8 3 110 111 101 96\r\n"
			//"a=rtpmap:18 G729/8000\r\n"
			//"a=rtpmap:4 G723/8000\r\n"
			//"a=rtpmap:8 PCMA/8000\r\n"
		 	//"a=rtpmap:3 GSM/8000\r\n" 
		 	//"a=rtpmap:96 telephone-event/8000\r\n"
		 	//"a=rtpmap:101 telephone-event/8000\r\n",

	sdp_body = tmp;
		
	return sdp_body;

}// END: int createSdpOffer


#define SOCKET_TIMEOUT 100
static int is_socket_connected(int sock)
{
    int res;
    struct timeval tv;
    fd_set wrset;
    int valopt;
    socklen_t sock_len;
    tv.tv_sec = SOCKET_TIMEOUT / 1000;
    tv.tv_usec = (SOCKET_TIMEOUT % 1000) * 1000;

    FD_ZERO(&wrset);
    FD_SET(sock, &wrset);

    res = select(sock + 1, NULL, &wrset, NULL, &tv);
    if (res > 0) {
        sock_len = sizeof(int);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *) (&valopt), &sock_len)
            == 0) {
            if (valopt) {
                OSIP_TRACE(osip_trace
                           (__FILE__, __LINE__, OSIP_INFO2, NULL,
                            "Cannot connect socket node / %s[%d]\n",
                            strerror(ex_errno), ex_errno));
                return -1;
            } else {
                return 0;
            }
        } else {
            OSIP_TRACE(osip_trace
                       (__FILE__, __LINE__, OSIP_INFO2, NULL,
                        "Cannot connect socket node / error in getsockopt %s[%d]\n",
                        strerror(ex_errno), ex_errno));
            return -1;
        }
    } else if (res < 0) {
        OSIP_TRACE(osip_trace
                   (__FILE__, __LINE__, OSIP_INFO2, NULL,
                    "Cannot connect socket node / error in select %s[%d]\n",
                    strerror(ex_errno), ex_errno));
        return -1;
    } else {
        OSIP_TRACE(osip_trace
                   (__FILE__, __LINE__, OSIP_INFO2, NULL,
                    "Cannot connect socket node / select timeout (%d ms)\n",
                    SOCKET_TIMEOUT));
        return -1;
    }
}


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

	char	yStrTmpServer[256];
	char	yStrTmpServerIP[256];
	char	yStrTmpServerPort[32];
	int		yIntTmpServerPort;
#ifdef EXOSIP_4
    struct eXosip_t *eXcontext;
#endif // EXOSIP_4

	osip_message_t *invite = NULL;

	sprintf(yStrTmpServer, "%s", toUrl.c_str());

	if(strchr(yStrTmpServer, ':'))
	{
		rc = sscanf(yStrTmpServer, "%[^:]:%s", yStrTmpServerIP, yStrTmpServerPort);
		yIntTmpServerPort = atoi(yStrTmpServerPort);
	}
	else
	{
		sprintf(yStrTmpServerIP, "%s", yStrTmpServer);
		yIntTmpServerPort = 5070;
	}

	// build initial INVITE 
	// ---------------------
#ifdef EXOSIP_4
    eXcontext=gMrcpInit.getExcontext();
    eXosip_lock (eXcontext);

    rc = eXosip_call_build_initial_invite ( eXcontext,
        &invite,
        toUrl.c_str(), fromUrl.c_str(), route.c_str(),
        subject.c_str());

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
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, "Unable to build SIP invite for toUrl=(%s); "
			"fromUrl=(%s).  rc=%d.", toUrl.c_str(), fromUrl.c_str(), rc);
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

	/*It will malloc 4K from inside osip_message_to_str*/
	rc = osip_message_to_str(invite, pb, &bufLen);
	if ( ( rc != 0 ) || ( bufLen <= 0 ) )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR, "osip_message_to_str() failed.  rc=%d. "
			"Unable to send SIP INVITE.", rc);
		return(-1);

	}
	else
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, 
			"Sending INVITE string to to MRCP Server: (%s)", tmpBuf);
	}

	/*Free*/
	free(tmpBuf);

#if 0
	char *transport = _eXosip_transport_protocol(invite);

	if(transport != NULL && strcasecmp(transport, "TCP") == 0)
	{
		int out_socket = _eXosip_tcp_find_socket (yStrTmpServerIP, yIntTmpServerPort);

		if(out_socket <= 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL, MRCP_2_BASE, INFO, 
				"Socket for (%s:%d) not present in SIP pool. Creating for the first time.", 
				yStrTmpServerIP, yIntTmpServerPort);
	
			out_socket = _eXosip_tcp_connect_socket(yStrTmpServerIP, yIntTmpServerPort);
		}

		if(out_socket <= 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL, MRCP_2_BASE, INFO, 
				"Failed _eXosip_tcp_connect_socket for %s:%d.",
				yStrTmpServerIP, yIntTmpServerPort);

			return (-1);
		}

		int rc = is_socket_connected(out_socket);
		if(rc < 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL, MRCP_2_BASE, INFO, 
				"Failed is_socket_connected for (%s:%d) socket fd(%d).",
				yStrTmpServerIP, yIntTmpServerPort, out_socket);
		}
	}
#endif


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

int  srUnLoadGrammars(int zPort)
{
	grammarList[zPort].deactivateAllGrammars();
	return 0;
}
int srUnLoadGrammar(int zPort, const char* zGrammarName)
{
	char mod[] = "srUnLoadGrammar";
	string              yGrammarName;
	yGrammarName = string(zGrammarName);
	//grammarList[zPort].deactivateGrammar(yGrammarName);
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
		MRCP_2_BASE, INFO, "calling removeGrammarFromList yGrammarName=%s", yGrammarName.c_str());
	int yRc = grammarList[zPort].removeGrammarFromList(yGrammarName);
	if(yRc == 1)
	{
		gSrPort[zPort].setIsGrammarGoogle(false);
	}
	return(0);
}

/*------------------------------------------------------------------------
 *-----------------------------------------------------------------------*/
int srLoadGrammar(int zPort, 
				int zGrammarType, 
				const char *zGrammarName, 
				const char *zGrammar, 
				int zLoadNow,
				float zWeight)
{
	static char			mod[]="srLoadGrammar";
	int					rc;
//	GrammarData			yGrammarData;
	string				yGrammarName;
	string				yGrammar;
	float				yWeight = zWeight;

	yGrammarName = string(zGrammarName);
	yGrammar	 = string(zGrammar);

	if(zGrammarType == SR_GRAM_ID)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, 
			"Activating Grammar (type=%d:%s:%.*s) loaded in memory.", 
			zGrammarType, yGrammarName.c_str(), 600,
			yGrammar.c_str());

		rc = grammarList[zPort].activateGrammar(yGrammarName);

		if(rc == 1)
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, 
				"ARCGS: Activating Google Grammar");

			gSrPort[zPort].setIsGrammarGoogle(true);
		}
		else
		{
			gSrPort[zPort].setIsGrammarGoogle(false);
		}

		return 0;
	}
	if ( ! grammarList[zPort].isEmpty() )
	{
		if ( grammarList[zPort].isGrammarAlreadyLoaded(yGrammarName) )
		{
//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
//			MRCP_2_BASE, INFO, 
//			"DJB: zLoadNow=%d; LOAD_GRAMMAR_VAD_LATER=%d", 
//			zLoadNow, LOAD_GRAMMAR_VAD_LATER);
			if ( zLoadNow == LOAD_GRAMMAR_VAD_LATER )
			{
				return(TEL_SUCCESS);
			}
		
			rc = sLoadGrammar(zPort, zGrammarType, yGrammarName, yGrammar, yWeight);
			return(rc);
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
					MRCP_2_BASE, INFO, 
				"yGrammarName (%s) is already loaded", yGrammarName.c_str());
		}
	}

//	yGrammarData.type				= zGrammarType;
//	yGrammarData.isActivated		= 0;
//	yGrammarData.isLoadedOnBackend	= 0;
//	yGrammarData.grammarName		= yGrammarName;
//	yGrammarData.data				= zGrammar;
	
	grammarList[zPort].addGrammarToList(zGrammarType, false, true,
			yGrammarName, yGrammar);

//	grammarList[zPort].push_back(yGrammarData);

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, 
			"Grammar (type=%d:%s:%.*s) loaded in memory.", 
			zGrammarType, yGrammarName.c_str(), 600,
			yGrammar.c_str());

//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
//			MRCP_2_BASE, INFO, 
//			"DJB: zLoadNow=%d; LOAD_GRAMMAR_VAD_LATER=%d", 
//			zLoadNow, LOAD_GRAMMAR_VAD_LATER);
	if ( zLoadNow == LOAD_GRAMMAR_VAD_LATER )
	{
		return(TEL_SUCCESS);
	}

	rc = sLoadGrammar(zPort, zGrammarType, yGrammarName, yGrammar, yWeight);

	return(rc);

} // End srLoadGrammar() 

//==============================================
int sLoadGrammar(int zPort, 
						int zGrammarType, 
						string &zGrammarName,
						string &zGrammarContent,
						float zWeight)
//==============================================
{
	int		rc;
	char	strContentLength[12];
	char	strWeight[12];

	static char			mod[]="sLoadGrammar";

#if 0

	//DDN: Google: Check if it is builtin google
	if (zGrammarName.find("builtin:grammar/google") != std::string::npos)
	{
		//This is Google grammar.
		gSrPort[zPort].setIsGrammarGoogle(true);

		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, "ARCGSR: Found name builtin:grammar/google.");

	}
	else if (zGrammarContent.find("builtin:grammar/google") != std::string::npos)
	{
		//This is Google grammar.
		gSrPort[zPort].setIsGrammarGoogle(true);

		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, "ARCGSR: Found content builtin:grammar/google.");

	}

#endif

	if(zGrammarType == SR_GRAM_ID)
	{
		rc = grammarList[zPort].activateGrammar(zGrammarName);

		if(rc == 1)
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, "ARCGSR: Found id for builtin:grammar/google.");
			gSrPort[zPort].setIsGrammarGoogle(true);
		}

		return 0;
	}


	// build DEFINE-GRAMMAR
	// --------------------
	int		contentLength = zGrammarContent.length();
	sprintf(strContentLength, "%d", contentLength);
	sprintf(strWeight, "%f", zWeight);

	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, 
			"Creating DEFINE-GRAMMAR message.");

	MrcpHeader header;
	MrcpHeaderList headerList;
	header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
	headerList.push_back(header);
	if ( zGrammarType == SR_URI )
	{
		header.setNameValue("Content-Type", "text/uri-list");
		headerList.push_back(header);
	}
	else
	{
		header.setNameValue("Content-Type", "application/srgs+xml");
		headerList.push_back(header);
	}

	if ( gSrPort[zPort].getLanguage().length() > 0 )
	{
		header.setNameValue("speech-language", gSrPort[zPort].getLanguage().c_str());
		headerList.push_back(header);
	}
	else
	{
		header.setNameValue("speech-language", "en-US");
		headerList.push_back(header);
	}

	header.setNameValue("Content-Id", zGrammarName);
	headerList.push_back(header);

	header.setNameValue("Content-Length", strContentLength);
	headerList.push_back(header);

	header.setNameValue("Weight", strWeight);
	headerList.push_back(header);

//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
//			MRCP_2_BASE, INFO, 
//			"DJB: Calling gSrPort[%d].getRequestId()", zPort);
//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
//			MRCP_2_BASE, INFO, 
//			"DJB: gSrPort[%d].getRequestId()=(%s)", zPort,
//			gSrPort[zPort].getRequestId().c_str());
	ClientRequest loadGrammarRequest(
			gMrcpInit.getMrcpVersion(),
			"DEFINE-GRAMMAR", 
			gSrPort[zPort].getRequestId(), 
			headerList,
			contentLength); 

	string mrcpMsg = loadGrammarRequest.buildMrcpMsg();
	string grammarMsg = mrcpMsg + zGrammarContent;

	string	recvMsg;
	int		statusCode	= -1;
	int		causeCode	= -1;

	string serverState = "";
	rc = processMRCPRequest(zPort, grammarMsg, recvMsg,
			serverState, &statusCode, &causeCode);
	if ( statusCode >= 300 )
	{
		// return(-1);
		return(causeCode);
	}

	if ( serverState != "COMPLETE" )
	{
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
            MRCP_2_BASE, ERR,
            "Load grammar failed; no COMPLETE response received from MRCP Server. [serverState is (%s)]",
			serverState.c_str());
		return(-1);
	}

	return(rc);
} // END: sLoadGrammar()


/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int processMRCPRequest(int zPort, string & zSendMsg, string & zRecvMsg,
			string & zServerState, int *zStatusCode, int *zCompletionCause)
{
	static char		mod[] = "processMRCPRequest";
	int				rc;
	string			yEventName = "";
	int				counter;
	OutOfSyncEvent	yOutOfSyncEvent;
	Mrcp2_Event     mrcpEvent;

	zRecvMsg			= "";
	zServerState		= "";
	*zStatusCode		= -1;
	*zCompletionCause	= -1;

	if ((rc = writeData(zPort, gSrPort[zPort].getSocketfd(), zSendMsg)) == -1)
	{
		return(-1);
	}

	rc = -999;

	counter=0;

	while(rc == -999)
	{
		if(!gCanProcessSipThread)
		{
			return -1;
		}

		rc = readMrcpSocket(zPort, READ_RESPONSE, zRecvMsg, yEventName,
				zServerState, mrcpEvent, zStatusCode, zCompletionCause);

		if(! yEventName.empty())
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Received out-of-sequence event (%s) for request (%s). ",
				yEventName.c_str(), zSendMsg.c_str());

				gSrPort[zPort].clearOutOfSyncEvent();

				yOutOfSyncEvent.statusCode      = *zStatusCode;
				yOutOfSyncEvent.completionCause = *zCompletionCause;
				sprintf(yOutOfSyncEvent.eventStr,		"%s", yEventName.c_str());
				sprintf(yOutOfSyncEvent.serverState,	"%s", zServerState.c_str());
				sprintf(yOutOfSyncEvent.content,		"%s", zRecvMsg.c_str());
				yOutOfSyncEvent.mrcpV2Event = mrcpEvent;

				gSrPort[zPort].setOutOfSyncEvent(&yOutOfSyncEvent);
	
				gSrPort[zPort].setOutOfSyncEventReceived(1);

				if ( strcmp(yEventName.c_str(), "START-OF-INPUT") == 0 )
				{ 
					mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
						REPORT_VERBOSE, MRCP_2_BASE, INFO,
						"Setting event and continuing.");
					rc = -999;
				}
				else if ( strcmp(yEventName.c_str(), "RECOGNITION-COMPLETE") == 0 || 
					strcmp(yEventName.c_str(), "INTERPRETATION-COMPLETE") == 0 ||
						strcmp(yEventName.c_str(), "RECORD-COMPLETE") == 0)
				{ 
					rc = 0;
					mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
						REPORT_VERBOSE, MRCP_2_BASE, INFO,
						"Setting event and halting.");
				}

				if ( counter >=3 )
				{
					mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
						REPORT_VERBOSE, MRCP_2_BASE, INFO,
						"After 3 tries, upable to make sense of the response.  Returning failure.");
					return(-1);
				}
				counter++;
		}
	}

	return(rc);
} // END: processMRCPRequest()

/*------------------------------------------------------------------------------
readMrcpSocket():
------------------------------------------------------------------------------*/
int readMrcpSocket(int zPort, 
				int zReadType,
				string & zRecvMsg, 
				string & zEventName,
				string & zServerState,
				Mrcp2_Event	&		mrcpEvent,
				int *zStatusCode, 
				int *zCompletionCause)
{
	static char		mod[] = "readMrcpSocket";
	int				rc;
	int				mrcpPort	= gSrPort[zPort].getSocketfd();
	//string			channelId	= gSrPort[zPort].getChannelId();
//	string			requestId	= gSrPort[zPort].getRequestId();
	string			requestId;
	int				socketId	= gSrPort[zPort].getSocketfd();
	int				returnType;
	int				milliSeconds;

//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
//			MRCP_2_BASE, INFO, 
//			"DJB: Calling gSrPort[%d].getRequestId()", zPort);
//
	requestId	= gSrPort[zPort].getRequestId();

//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
//			MRCP_2_BASE, INFO, 
//			"DJB: gSrPort[%d].getRequestId()=(%s)", zPort,
//			gSrPort[zPort].getRequestId().c_str());

	zRecvMsg			= "";
	zServerState		= "";
	zEventName			= "";
	*zStatusCode		= -1;
	*zCompletionCause	= 0;

	if ( zReadType == READ_EVENT )
	{
		milliSeconds = 200;
	}
	else
	{
		// milliSeconds = 6000 * 1000;	// Give a response 6 secs (may have to change)
		milliSeconds = 8000 * 1000;	// Give a response 8 secs (may have to change)
	}

	rc = sIsDataOnMrcpSocket(zPort, mrcpPort, milliSeconds);
//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
//			MRCP_2_BASE, INFO, 
//			"sIsDataOnMrcpSocket rc=%d", rc);
	switch (rc)
	{
		case -1: 			// message logged in routine
		case 0:
			return(rc);
			break;
	}

	// parse response/event
	char	recvBuf[8192]; // or smart pointer
	int		receivedLen = -1;

	memset((char *)recvBuf, '\0', sizeof(recvBuf));
	if ( (receivedLen = doControlledRead(zPort, mrcpPort, recvBuf,
						sizeof(recvBuf))) < 0 )
	{
		// message is logged in routine
		return(-1);
	}

	string serverMsg = string(recvBuf); // response or event

	if ( ! gSrPort[zPort].getHideDTMF() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Read %d bytes:(%.*s)", receivedLen, 500, serverMsg.c_str());
	}

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

/*****************************************************************************
From the ietf mrcpV2 draft 
    ( http://datatracker.ietf.org/doc/draft-ietf-speechsc-mrcpv2/ )
    The format of response message is:
        response-line  =    mrcp-version SP message-length SP request-id
                                    SP status-code SP request-state CRLF
    An example is of a response message:
        "MRCP/2.0 101 2 200 COMPLETE"
        
    The format of an event message is:
        event-line       =  mrcp-version SP message-length SP event-name
                                    SP request-id SP request-state CRLF

    An example is of an event message:
        "MRCP/2.0 132 START-OF-INPUT 1 IN-PROGRESS Channel-Identifier..."

    An example of a bad message:
        "MRCP/2.0 101 99 2 200 COMPLETE Channel-Identifier:..."

This routine is to verify it is one of the other. 
*****************************************************************************/
    pos1 = pos2 + 1 ;
    pos2 = serverMsg.find(" ", pos1 + 1); // 4th space
    pos1 += 1;
    pos2 = pos2 - 1;

    pos1 = pos2 + 1 ;
    pos2 = serverMsg.find(" ", pos1 + 1); // 5th space
    pos1 += 1;
    pos2 = pos2 - 1;

        // parsing 5th field to validate msg.
    int yFieldLen = pos2 - pos1 + 1;
    string yString = serverMsg.substr(pos1, yFieldLen);
    char yField = yString[0];

	Mrcp2_Response		mrcpResponse;
	//Mrcp2_Event			mrcpEvent;
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
		zRecvMsg.assign(contentMsg.c_str());
	}
	
	if(isdigit((int)xField))  // response message
	{  
        if(isdigit((int)yField))  // invalid msg - field 5 should be request-state,
		                          //               not a valid integer
        {
  			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Message received is invalid; it's neither a valid "
				"MRCPV2 event nor response message.  Returning "
				"failure.");
            return(-1);          // invalid msg
        }

		mrcpResponse.addMessageBody(headerMsg);
		headerList = mrcpResponse.getHeaderList(); // headerList 
	    serverMsgLen = mrcpResponse.getMessageLength(); // message length
		// requestId
		char tmpRequestId[10];
		unsigned int rid = mrcpResponse.getRequestId();
		sprintf(tmpRequestId, "%d", rid);
		receivedRequestId = string(tmpRequestId);

		int yIntReceivedRequestId = atoi(receivedRequestId.c_str());
		int yIntRequestId = atoi(requestId.c_str());

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
				"RequestId (%s) received does not match "
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
			REPORT_DIAGNOSE, MRCP_2_BASE, INFO, 
			"received event(%s).", zEventName.c_str());

	}

	// verify msgLen 
	if (receivedLen < serverMsgLen)
	{
		// read more into buffer and insert into serverMsg
		int needLen = serverMsgLen - receivedLen; 
		int newLen = -1;
		newLen = read(socketId, (void *)&recvBuf[receivedLen], needLen);
		if(newLen < needLen)
		{
  			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Unable to read complete message.  Returning.");
			return(-1);
		}	
	}
	else if(serverMsgLen < receivedLen)
	{
		// todo:Can't remove extra bytes; must save them and append them later;
		//      however, not sure if this would never happen.
		
		int msgSize = serverMsg.length();
		serverMsg.erase(serverMsgLen, msgSize - serverMsgLen);
	}

#if 0
	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"ready to parse received headList.");
#endif

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

	} // END: headerList.empty()

	if ( returnType == YES_EVENT_RECEIVED )
	{
		return(YES_EVENT_RECEIVED);
	}

	if ( (*zStatusCode < 200) || (*zStatusCode >= 300) )
	{
		return(-1);
	}

	if ( contentMsg.size() <= 0 )
	{
		zRecvMsg = headerMsg;
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
	//static char tmpBuf[64];
	//static char lBuf[1024];
	//static char bytesToReadBuf[64];
	char tmpBuf[1024];
	char lBuf[8192];
	char bytesToReadBuf[64];
	const int	numHeaderBytes = 20;
	int			bytesToRead;
	int			i, j;
	char		*p;
	time_t		t1;
	time_t		t2;

	memset((char *)zRecvBuf, '\0', zMaxSize);
	memset((char *)tmpBuf, '\0', sizeof(tmpBuf));
	memset((char *)bytesToReadBuf, '\0', sizeof(bytesToReadBuf));

	// MRCP/2.0 xxxx - max message size

	time(&t1);
	while (1)
	{
		// read numHeaderBytes just to get the total number of bytes to read.
		if ( (receivedLen = read(zMrcpPort, (void *)tmpBuf, numHeaderBytes)) < 0 )
		{
			if ( errno != EAGAIN ) 
			{
		        if ( gSrPort[zPort].IsStopProcessingRecognition() )
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
			else		// errno == EAGAIN
			{
				mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
								REPORT_VERBOSE, MRCP_2_BASE, INFO,
								"Failed to read from the MRCP socket (%d). [%d, %s]",
								zMrcpPort, errno, strerror(errno));
				time(&t2);

				// usleep(200);
				usleep(500000);		// BT-228
				if ( t2 - t1 > 3 )  // MR 4620 - if can't write in 3 seconds, call it quits
				{
					mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
								REPORT_NORMAL, MRCP_2_BASE, ERR,
								"Returning failure after 3 seconds of attempting to read() from socket.");
					return(-1);
				}
				else
				{
					continue;
				}
			}
		}
		else
		{
			break;
		}
	}

	/*Not all of 20 chars received??*/
	if(receivedLen < numHeaderBytes)
	{
		int yIntTmpNumHeaderBytes = numHeaderBytes - receivedLen;
		usleep(1000);
		receivedLen = read(zMrcpPort, tmpBuf + receivedLen, yIntTmpNumHeaderBytes);

		time(&t1);
		while (1)
		{
			// read numHeaderBytes just to get the total number of bytes to read.
			if ( (receivedLen = read(zMrcpPort, tmpBuf + receivedLen, yIntTmpNumHeaderBytes)) < 0 )
			{
				if ( errno != EAGAIN ) 
				{
			        if ( gSrPort[zPort].IsStopProcessingRecognition() )
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
				else		// errno == EAGAIN
				{
					mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
									REPORT_VERBOSE, MRCP_2_BASE, INFO,
									"Failed to read from the MRCP socket (%d). [%d, %s]",
									zMrcpPort, errno, strerror(errno));
					time(&t2);
					usleep(200);
					if ( t2 - t1 > 3 )  // MR 4620 - if can't write in 3 seconds, call it quits
					{
						mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
									REPORT_NORMAL, MRCP_2_BASE, ERR,
									"Returning failure after 3 seconds of attempting to read() frokm socket.");
						return(-1);
					}
					else
					{
						continue;
					}
				}
			}
			else
			{
				if(receivedLen < yIntTmpNumHeaderBytes)
				{
		  			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
						REPORT_NORMAL, MRCP_2_BASE, ERR,
						"Failed to read 20 bytes of header line from the MRCP socket in two attempts. port=%d", 
						zMrcpPort);
					return(-1);
				}
				break;
			}
		}
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
	j = 0;

	while ( isdigit(p[j]) )
	{
		bytesToReadBuf[i++] = p[j++];
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
		memset((char *)lBuf, '\0', sizeof(lBuf));

		time(&t1);
		while (1)
		{
			// read numHeaderBytes just to get the total number of bytes to read.
			if ( (receivedLen = read(zMrcpPort, (void *)lBuf, i)) < 0 )
			{
				if ( errno != EAGAIN ) 
				{
			        if ( gSrPort[zPort].IsStopProcessingRecognition() )
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
				else		// errno == EAGAIN
				{
					mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
									REPORT_VERBOSE, MRCP_2_BASE, INFO,
									"Failed to read from the MRCP socket (%d). [%d, %s]",
									zMrcpPort, errno, strerror(errno));
					time(&t2);
					usleep(200);
					if ( t2 - t1 > 3 )  // MR 4620 - if can't write in 3 seconds, call it quits
					{
						mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
									REPORT_NORMAL, MRCP_2_BASE, ERR,
									"Returning failure after 3 seconds of attempting to read() frokm socket.");
						return(-1);
					}
					else
					{
						continue;
					}
				}
			}
			else
			{
				break;
			}
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
	const char	contentLenStr[32] = "Content-Length:";
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
	p2 = strstr(p1, "<?xml");					// MR 4948

//	p2 += 1;
//	*p2 = '\0';
//	p2++;

	headerMsg = pServerMsg;			// MR 4973
	if ( p2 != NULL )
	{
		while ( isspace(*p2) )
		{
			p2++;
		}
		contentMsg = p2;
	}
	else
	{
		contentMsg = "";
	}
	
		
	free(pServerMsg);
	return(0);
} // END: sGetHeaderContent()

//============================================
int writeData(int zPort, int sockfd, string& data)
//============================================
{
	char mod[] ="writeData";
	//int	maxBytesToSend = 4096;
	int	maxBytesToSend = 8192;
	int nCurrent = 0; 
	int nSent = 0;
	int	nBytesToSend = 0;
	time_t		t1;
	time_t		t2;

	char 		*p;
	char 		*q;
	int			printit = 0;

	int nbytes = data.size(); 
//	int nbytes = data.size()  + 1; 
	char *buff = new char[nbytes + 1];


	//sprintf(buff, "%s", data.data());
	sprintf(buff, "%s", data.c_str());

	// while( (nCurrent = write(sockfd, &buff[nSent], nbytes-nSent)) > 0 ) 
	//while((nCurrent = send(sockfd, &buff[nSent], nbytes-nSent, MSG_NOSIGNAL)) >= 0 ) 
//	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"DJB: Sending %d maximum bytes. Total to send is %d.", maxBytesToSend, nbytes);

	time(&t1);
	while (1)
	{
		if ( nbytes - nSent > maxBytesToSend )
		{
			nBytesToSend = maxBytesToSend;
		}
		else
		{
			nBytesToSend = nbytes - nSent;
		}
	
		nCurrent = send(sockfd, &buff[nSent], nBytesToSend, MSG_NOSIGNAL);
//		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"DJB: %d = send(%d, %d); nSent=%d ",  nCurrent, sockfd, nBytesToSend, nSent);

		if ( nCurrent < 0 && errno != EAGAIN)
		{
            if ( gSrPort[zPort].IsStopProcessingRecognition() )
            {
                mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                    REPORT_VERBOSE, MRCP_2_BASE, INFO,
                    "Failed to write %d bytes to socket fd %d.  [%d, %s] rc = %d",
                    nbytes-nSent, sockfd, errno, strerror(errno), nCurrent);
            }
            else
            {
                mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                    REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "Failed to write %d bytes to socket fd %d.  [%d, %s] rc = %d",
                    nbytes-nSent, sockfd, errno, strerror(errno), nCurrent);
            }

			delete [] buff;
			return(-1);
		}
		else if ( nCurrent < 0 && errno == EAGAIN)
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort,
				mod, REPORT_DETAIL, MRCP_2_BASE, WARN,
				"EAGAIN: write() failed to write %d bytes; rc=%d.", nBytesToSend, nCurrent);

			time(&t2);
			if ( t2 - t1 > 3 )	// MR 4620 - if can't write in 3 seconds, call it quits
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
				if ( nSent == nbytes )
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
				600, &buff[nSent], sockfd, nBytesToSend);
		}

		nSent += nCurrent;
		if ( nSent == nbytes )
		{
			break;
		}

		time(&t2);
		if ( t2 - t1 > 3 )	// MR 4620 - if can't write in 3 seconds, call it quits
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                   REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "Returning failure after 3 seconds of attempting to write() to socket.");
			delete [] buff;
			return(-1);
		}
	}

	delete [] buff;

	if (nSent < nbytes)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort,
			mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
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

	gSrPort[zPort].setTelSRInitCalled(false);

	if ( gSrPort[zPort].IsCleanUpCalled() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort,
			mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"CleanUp has already been called.  Returning with no action.");
		return(0);
	}

	gSrPort[zPort].setCleanUpCalled(true);
	rc = srReleaseResource(zPort);
	rc = gSrPort[zPort].closeSocketFd(zPort);

	if ( ! grammarList[zPort].isEmpty() )
	{
		grammarList[zPort].removeAll();
	}

	dynMgrFd = gSrPort[zPort].getFdToDynMgr();
	if ( dynMgrFd  > 0 )
	{
		close(dynMgrFd);
	}

		// We have to save the application response fifo information.  
	tmpFd	= gSrPort[zPort].getAppResponseFd();
	gSrPort[zPort].getAppResponseFifo(tmpFifo);		// BT-259


	if(gCanProcessSipThread)/*No cleanup if SIGTERM*/
	{
		gSrPort[zPort].initializeElements();
	}

	if ( tmpFd != -1 )
	{
		gSrPort[zPort].setAppResponseFifo(tmpFifo);
		gSrPort[zPort].setAppResponseFd(tmpFd);
	}
	gSrPort[zPort].destroyMutex();

	if(gCanProcessSipThread)/*No cleanup if SIGTERM*/
	{
		gSrPort[zPort].eraseCallMapByPort(zPort);
	}

	return(0);
} // END: cleanUp()

/*------------------------------------------------------------------------------
sendRequestToDynMgr():
------------------------------------------------------------------------------*/
int sendRequestToDynMgr(int zPort, char *mod, struct MsgToDM *request)
{
    int     rc;

	// MR 4986
	if ( ! gSrPort[zPort].IsFdToDynmgrOpen() ) 
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, WARN,
        	"Unable to write to dynmgr fifo because the local file descriptor is invalid (%d).",
			gSrPort[zPort].getFdToDynMgr());
		return(0);
	}

    rc = write(gSrPort[zPort].getFdToDynMgr(),
                (char *)request, sizeof(struct MsgToDM));
    if (rc == -1)
    {
        if ( gSrPort[zPort].IsStopProcessingRecognition() )
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Unable to write message to dynmgr fifo. [%d, %s].",
                errno, strerror(errno));
		}
		else
		{
	        mrcpClient2Log(__FILE__, __LINE__, zPort,
                    mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "Unable to write message to dynmgr fifo. [%d, %s].",
                    errno, strerror(errno));
		}
        return(-1);
    }

    mrcpClient2Log(__FILE__, __LINE__, zPort,
        mod, REPORT_VERBOSE, MRCP_2_BASE, INFO, "Sent %d bytes to dynmgr fifo. "
            "msg={op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
            rc,
            request->opcode,
            request->appCallNum,
            request->appPid,
            request->appRef,
            request->appPassword);

    return(rc);
} // END: sendRequestToDynMgr()

/*------------------------------------------------------------------------------
sWaitForLicenceReserved():
	Check if IsLicenseReserved return zTrueOrFalse.  The sipThread updates
	the licenseReserved member; have to wait for it.
------------------------------------------------------------------------------*/
int sWaitForLicenceReserved(int zPort, bool zTrueOrFalse)
{ 
	time_t	startTime;
	time_t	stopTime;
	time_t	currentTime = 0;

	time(&startTime);
//	stopTime = startTime + 3;		 // give it 3 seconds
	//stopTime = startTime + 4;		 // give it 4 seconds
	stopTime = startTime + 5;		 // give it 5 seconds
//		mrcpClient2Log(__FILE__, __LINE__, zPort, "sWaitForLicenceReserved", 
//			REPORT_VERBOSE, MRCP_2_BASE, WARN,
//			"DJB: currentTime=%ld  stopTime=%ld",
//			currentTime, stopTime);
	while ( currentTime < stopTime )
	{
//		mrcpClient2Log(__FILE__, __LINE__, zPort, "sWaitForLicenceReserved", 
//			REPORT_VERBOSE, MRCP_2_BASE, WARN,
//			"Calling gSrPort[%d].IsLicenseReserved()", zPort);
		if ( gSrPort[zPort].IsLicenseReserved() == zTrueOrFalse )
		{
			return(0);			// Got the license
		}

    	if ( ! gSrPort[zPort].IsCallActive() )
		{
			return(-1);
		}
    	if ( gSrPort[zPort].DidReserveLicQuickFail() )
		{
			gSrPort[zPort].setReserveLicQuickFail(false); // BT-195
			return(-1);
		}

		usleep(1000);
		time(&currentTime);
	}
//		mrcpClient2Log(__FILE__, __LINE__, zPort, "sWaitForLicenceReserved", 
//			REPORT_VERBOSE, MRCP_2_BASE, WARN,
//			"DJB: currentTime=%ld  stopTime=%ld",
//			currentTime, stopTime);
	return(-1);			// Timed out. 

} // END: sWaitForLicenceReserved()

#if 0
/*------------------------------------------------------------------------------
srPrintGrammarList():
------------------------------------------------------------------------------*/
void srPrintGrammarList(int zPort)
{
	static char						mod[] = "srPrintGrammarList";
	list <GrammarData>::iterator	iter;
	GrammarData						yGrammarData;
	int								yCounter;
	
	iter = grammarList[zPort].begin();

	yCounter = 1;
	while ( iter != grammarList[zPort].end() )
    {
        yGrammarData = *iter;
		mrcpClient2Log(__FILE__, __LINE__, zPort,
			mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"grammarList[%d]:%d:type=%d; name=(%s); data=(%.*s).",
			zPort, yCounter, yGrammarData.type,
			yGrammarData.grammarName.c_str(), 100, yGrammarData.data.c_str());
        iter++;
		yCounter++;
    }
	
} // END: srPrintGrammarList()
#endif


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
    char        *p1;
    char        *p2;

    p2 = (char *)calloc(strlen(zStr) + 1, sizeof(char));

    p1 = zStr;

    if ( isspace(*p1) )
    {
        while(isspace(*p1))
        {
            p1++;
        }
    }

    sprintf(p2, "%s", p1);
    p1 = &(p2[strlen(p2)-1]);
    while(isspace(*p1))
    {
        *p1 = '\0';
        p1--;
    }

    sprintf(zStr, "%s", p2);
    free(p2);

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
	int	payloadType = atoi(gMrcpInit.getPayloadType(SR).c_str());
	zPayloadStr[0] = '\0';

			// default codec Type
	*zCodecType = atoi(gMrcpInit.getCodecType(SR).c_str());
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
		*zPayloadType = payloadType;
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
            REPORT_DETAIL, SR_INVALID_DATA, WARN,
            "Environment variable ISPBASE is not set; "
            "using default codec (%d) and payload type (%d).",
            zCodecType, *zPayloadType);
        return;
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
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
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
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
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
srReleaseResource():
	Called by SRReleaseResource to send Sip BYE message.
------------------------------------------------------------------------------*/
int srReleaseResource(const int zPort)
{
	char	mod[] ="srReleaseResource";
	int		cid, did;
	int		rc = -1;
	int		socketfd;
#ifdef EXOSIP_4
    struct eXosip_t *eXcontext;
#endif // EXOSIP_4

//    mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
//         MRCP_2_BASE, INFO, 
//			"gSrPort[%d].IsLicenseReserved()=%d",
//			zPort,  gSrPort[zPort].IsLicenseReserved());

	if ( ! gSrPort[zPort].IsLicenseReserved() )
	{
    	cid = gSrPort[zPort].getCid();
	    did = gSrPort[zPort].getDid();
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, "License is already released (%d); NOT sending BYE for callCid=%d, callDid=%d",
			zPort,  gSrPort[zPort].IsLicenseReserved(), cid, did);
		return(0);		// no need to send BYE, no license is reserved.
	}

    mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
         MRCP_2_BASE, INFO, "Attempting to release a resource.");

    // cid and did are set by sipThread when get INVITE response.
    cid = gSrPort[zPort].getCid();
    did = gSrPort[zPort].getDid();

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
    eXcontext=gMrcpInit.getExcontext();
    eXosip_lock (eXcontext);
    rc = eXosip_call_terminate(eXcontext, cid, did); // send SIP BYE to server
    eXosip_unlock (eXcontext);
#else
	eXosip_lock ();
	rc = eXosip_call_terminate(cid, did); // send SIP BYE to server
	eXosip_unlock ();
#endif // EXOSIP_4

    if (rc < 0)
    {
        mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
                MRCP_2_BASE, WARN, "Can't send BYE. Session already terminated. "
        "%d = eXosip_terminate_call(%d, %d);", rc, cid, did);
		return(-1);
    }

#if 0
	if ( (rc=sWaitForLicenceReserved(zPort, false)) == -1 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR, "Failed to release a license.");
		return(-1);
	}
#endif

	socketfd = gSrPort[zPort].getSocketfd();
	if ( socketfd >= 0 )
	{
		rc = close(socketfd);
		if( rc == 0)
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"MRCP socket fd (%d) is closed.", socketfd);
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Closing MRCP socket fd (%d) failed. [%d, %s]", socketfd,
				errno, strerror(errno));
		}
	}
	pthread_mutex_lock( &callMapLock); 			// BT-75
	gSrPort[zPort].setLicenseReserved(false);
	pthread_mutex_unlock( &callMapLock); 		// BT-75
	gSrPort[zPort].setSocketfd(zPort, -1);
	
	mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, "License successfully released.");
	return(0);

} // END: srReleaseResource()

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
	struct timeval		timeout;
	fd_set				set;

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
		"hostname = %s, serverPort=%d", serverName.c_str(), serverPort);

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

	rc = connect(sockfd, (struct sockaddr *)&serverAddr,
                   sizeof(struct sockaddr));
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
			"Connection to MRCP Server succeeded.");
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
//	static int		d = 1000;

	FD_ZERO (&readFdSet);

	FD_SET(zMrcpPort, &readFdSet);
	tval.tv_sec     = 0;
	tval.tv_usec    = zMilliSeconds;
	nEvents =  select (zMrcpPort + 1, &readFdSet, NULL, NULL, &tval);
		
	if ( nEvents < 0 )
	{
        if ( gSrPort[zPort].IsStopProcessingRecognition() )
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "Select failed for port %d.  "
                "Will be unable to read from MRCP socket.  [%d, %s]",
                zMrcpPort + 1, errno, strerror(errno));
        }
        else
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
                REPORT_NORMAL, MRCP_2_BASE, ERR,
                "Select failed for port %d.  "
                "Will be unable to read from MRCP socket.  [%d, %s]",
                zMrcpPort + 1, errno, strerror(errno));
        }
		return(0);
	}

	if ( nEvents == 0 )
	{
		return(0);
	}

	return(1);
} // END: sIsDataOnMrcpSocket()
