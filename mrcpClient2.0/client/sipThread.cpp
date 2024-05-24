/*------------------------------------------------------------------------------
Port,   sipThread.cpp
#endif
Author      :   Aumtech, Inc.
Update      :   06/10/06 yyq Created the file.
Purpose     :   MRCP Client v2.0 
  - singleton sipThread waits for event from Mrcp2 server, 
  - For each event of type CALL_ANSWERED, it creates a MrcpThread to
    handle media control communication     
Update: 07-22-14 djb	MR 4327 - changed exosip_event_wait() to not block
                    	forever, but come out every second.
Update: 08/28/2014 djb MR 4241. Added cid and did to log messages for clarity,
                                add cancelCid logic
------------------------------------------------------------------------------*/
#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/signal.h>
//#include <sys/siginfo.h>

#include "eXosip2/eXosip.h"

#include "mrcpThread.hpp"
#include "sipThread.hpp"
#include "mrcpCommon2.hpp"

using namespace std;
using namespace mrcp2client;

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include "unistd.h"
};

static void *sipThreadFunc(void*);
static void processCallAnswered(eXosip_event_t *eXosipEvent);
//static void sigterm(int x);
static void sigterm(int sig, siginfo_t *info, void *ctx);
static void sigpipe(int x);

extern int	gCanProcessReaderThread;
extern int	gCanProcessSipThread;
extern int	gCanProcessMainThread;

/*
class CException
{
public:
  string message;
  CException( string m ) { message = m; }
  void report() {cout << message << endl; };
};
*/
// static void sigterm(int x)

static void sigterm(int sig, siginfo_t *info, void *ctx)
{
    char mod[]="sigterm";

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
	        REPORT_DETAIL, MRCP_2_BASE, WARN,
			"SIGTERM (%d) caught from pid (%ld), user (%ld).  Exiting.",
            sig, (long)info->si_pid, (long)info->si_uid);

#if 0
	pthread_detach(pthread_self());
	pthread_exit(NULL);
#endif
	gCanProcessReaderThread = 0;
	gCanProcessSipThread = 0;
	gCanProcessMainThread = 0;

    return;
}/*END: void sigterm*/

static void sigpipe(int x)
{
    char mod[]="sigpipe";

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
	        REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"SIGPIPE caught - %d.  Ignoring.", x);

    return;
}/*END: int sigpipe*/


/*---------------------------------------------------------------------------------
 * SipThread implementation
 *--------------------------------------------------------------------------------*/

pthread_t SipThread::threadId;

SipThread* SipThread::getInstance() 
{
    static SipThread thread;
    return &thread;
}


pthread_t SipThread::getThreadId() const
{
	return threadId;
}


void SipThread::start()
{
	static char	mod[]="SipThread::start";
	pthread_t sipThreadId;

    pthread_attr_t threadAttr;
    pthread_attr_init (&threadAttr);


    pthread_attr_setdetachstate (&threadAttr, PTHREAD_CREATE_DETACHED);
    //pthread_attr_setdetachstate (&threadAttr, PTHREAD_CREATE_JOINABLE);

	int rc = pthread_create(&sipThreadId, &threadAttr, sipThreadFunc, NULL);

	if (rc != 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, "Failure in creating sipThread.");
		pthread_detach ( pthread_self());
		pthread_exit ( NULL);
	}
	threadId = sipThreadId;
}



/*---------------------------------------------------------------------------------
 * SipThreadFunc() 
 *--------------------------------------------------------------------------------*/
static void *sipThreadFunc(void*)
{
	static char		mod[]="sipThreadFunc";
	int 			rc = -1;
	int 			callNum = -1;
	eXosip_event_t *eXosipEvent = NULL;
    struct MsgToApp	response;
	string			channelId = "";
	string			serverAddress = "";
	int				socketfd;

	struct sigaction act;

	int callDid  = -1;
	int callCid = -1; 

	int mrcpPort = -1;
	int serverRtpPort = -1;
	int pos;
	struct  sigaction   sig_act, sig_oact;

#ifdef EXOSIP_4
    struct eXosip_t     *eXcontext;
#endif // EXOSIP_4

    signal(SIGPIPE, sigpipe);

	memset (&act, '\0', sizeof(act));
	act.sa_sigaction = &sigterm;
    act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTERM, &act, NULL) < 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "sigaction(SIGTERM): system call failed. [%d, %s]",
            errno, strerror(errno));
		pthread_detach(pthread_self());
		pthread_exit(NULL);
    }    

    /* set death of child function */
	memset (&sig_act, '\0', sizeof(sig_act));
    sig_act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
    if(sigaction(SIGCHLD, &sig_act, &sig_oact) != 0)
    {
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "sigaction(SIGCHLD, SA_NOCLDWAIT): system call failed. [%d, %s]",
            errno, strerror(errno));
		pthread_detach(pthread_self());
		pthread_exit(NULL);
    }

    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
		mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
            "signal(SIGCHLD, SIG_IGN): system call failed. [%d, %s]",
            errno, strerror(errno));
		pthread_detach(pthread_self());
		pthread_exit(NULL);
    }
    /*END: set death of child function */

	sdp_message_t   *remoteSdp = NULL;

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE, MRCP_2_BASE,
			INFO, "wait to receive eXosip event.");

#ifdef EXOSIP_4
    eXcontext=gMrcpInit.getExcontext();
#endif // EXOSIP_4

	gCanProcessSipThread = 1;
	while(gCanProcessSipThread)
	{
		if (eXosipEvent != NULL)
		{
#ifdef EXOSIP_4
            eXosip_lock(eXcontext);
            eXosip_event_free (eXosipEvent);
            eXosipEvent = NULL;
            eXosip_unlock(eXcontext);
#else
			eXosip_lock();
	    	eXosip_event_free (eXosipEvent);
			eXosipEvent = NULL;
			eXosip_unlock();
#endif // EXOSIP_4
		}

#ifdef EXOSIP_4
        eXosipEvent = eXosip_event_wait (eXcontext, 1, 0);
#else
		eXosipEvent = eXosip_event_wait (1, 0);
#endif // EXOSIP_4

		if (eXosipEvent != NULL)
		{
			callCid = eXosipEvent->cid;

			pthread_mutex_lock( &callMapLock);
			callNum = callMap[callCid];
			pthread_mutex_unlock( &callMapLock);

			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO, 
				"Received server event [%d:%s]; callCid=%d, callDid=%d",
				eXosipEvent->type, eXosipEvent->textinfo,
				eXosipEvent->cid, eXosipEvent->did);

			switch(eXosipEvent->type)
			{
                case EXOSIP_CALL_MESSAGE_NEW:
                    if(eXosipEvent->request && MSG_IS_BYE(eXosipEvent->request)){
                    }
                    break;
				case EXOSIP_CALL_ANSWERED:   // 200 OK
					processCallAnswered(eXosipEvent);
					break;
                case EXOSIP_CALL_MESSAGE_ANSWERED:
                    if ( gSrPort[callNum].IsInviteSent() )
                    {
                        mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                            REPORT_VERBOSE, MRCP_2_BASE, INFO,
                            "INVITE sent and received EXOSIP_CALL_MESSAGE_ANSWERED server event [%d, %s] "
                            "from the server. Processing Answered.",
                            eXosipEvent->type, eXosipEvent->textinfo);
                        processCallAnswered(eXosipEvent);
                    }
                    else
                    {
                        mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                            REPORT_VERBOSE, MRCP_2_BASE, INFO,
                            "Received EXOSIP_CALL_MESSAGE_ANSWERED server event [%d, %s] "
                            "from the server.  Ignoring.",
                            eXosipEvent->type, eXosipEvent->textinfo);
                    }
                    break;
				case EXOSIP_CALL_RELEASED:  // received RELEASED event
					/*EXOSIP_RELEASED is received for every CLOSED and BYE_ACK*/
					break;

				case EXOSIP_CALL_CLOSED:  // received BYE event
#ifndef EXOSIP_4
				case EXOSIP_CALL_BYE_ACK:	//Received ACK for our BYE
#endif // EXOSIP_4
				//-----------------------
		
				// identify who says BYE
					callCid = eXosipEvent->cid;
					pthread_mutex_lock( &callMapLock);
					callNum = callMap[callCid];
					callMap.erase(callCid);
					pthread_mutex_unlock( &callMapLock);
					mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
						REPORT_VERBOSE, MRCP_2_BASE, INFO, 
						"Received a BYE/ACK event from the server(%s) for "
						"callId(%d) did(%d).",
						eXosipEvent->textinfo, eXosipEvent->cid, eXosipEvent->did);

#if 0  // This is now done in srCommon.cpp
					socketfd = gSrPort[callNum].getSocketfd();
					if ( socketfd >= 0 )
					{
						mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
							REPORT_NORMAL, MRCP_2_BASE, ERR, 
							"Closing MRCP socket fd (%d).", socketfd);
	
		    			rc = close(socketfd);
						if( rc == 0)
						{
							mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
								REPORT_NORMAL, MRCP_2_BASE, ERR, 
								"tcp socket(%d) closed.", socketfd);
						}
					}
					gSrPort[callNum].setLicenseReserved(false);
					gSrPort[callNum].setSocketfd(-1);
#endif
					break;
	
				case EXOSIP_CALL_SERVERFAILURE:
				case EXOSIP_CALL_GLOBALFAILURE:
				case EXOSIP_CALL_REQUESTFAILURE:
					gSrPort[callNum].setReserveLicQuickFail(true);	// BT-195
					callCid = eXosipEvent->cid;
					pthread_mutex_lock( &callMapLock);
					callNum = callMap[callCid];
					pthread_mutex_unlock( &callMapLock);
					mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
						REPORT_VERBOSE, MRCP_2_BASE, INFO, 
						"Received server event [%d, %s]; callId=%d",
						eXosipEvent->type, eXosipEvent->textinfo,
						eXosipEvent->cid);

#if 0
    			response.returnCode = -1;
    			response.appCallNum = callNum;
    			response.opcode = gSrPort[callNum].getOpCode();
		    	response.appPid = gSrPort[callNum].getPid();
			    response.appRef = 0;
				response.appPassword = 0;
			    sprintf(response.message, "%s",
						eXosipEvent->response->reason_phrase);
  				(void) writeResponseToApp(callNum, __FILE__, __LINE__,
						&response, sizeof(response));

#endif
					break;


				case EXOSIP_CALL_PROCEEDING:
                    callDid = eXosipEvent->did;
					pthread_mutex_lock( &callMapLock);
                    callNum = callMap[callCid];
					pthread_mutex_unlock( &callMapLock);
					gSrPort[callNum].setDid(callDid);	

					mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
						REPORT_VERBOSE, MRCP_2_BASE, INFO, 
						"Received EXOSIP_CALL_PROCEEDING server event [%d, %s]",
						eXosipEvent->type, eXosipEvent->textinfo);

					break;
				default:
					mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
						REPORT_DETAIL, MRCP_2_BASE, WARN,
						"Event (%s:%d) received for cid(%d), did(%d), unable to process.",
	                    eXosipEvent->textinfo, eXosipEvent->type,
						eXosipEvent->cid, eXosipEvent->did);
			}    	// END: switch (eXosipEvent->type)

		}  //END: if(eXosipEvent != NULL)

    usleep (20 * 1000);

  } // END: while(1)


#ifdef EXOSIP_4
    eXosip_quit(eXcontext);
#else
	eXosip_quit();
#endif // EXOSIP_4

	pthread_detach(pthread_self());
	pthread_exit(NULL);

} // END: sipThreadFunc()

/*------------------------------------------------------------------------------
processCallAnswered():
------------------------------------------------------------------------------*/
static void processCallAnswered(eXosip_event_t *eXosipEvent)
{
	static char		mod[]="processCallAnswered";
	int 			rc = -1;
	int 			callNum = -1;
    struct MsgToApp	response;
	string			channelId = "";
	string			serverAddress = "";
	int				callDid  = -1;
	int				callCid = -1; 
	int				xCallCid = -1; 
	int				xCancelCid = -1; 
	int				mrcpPort = -1;
	int				serverRtpPort = -1;

	sdp_message_t   *remoteSdp = NULL;
#ifdef EXOSIP_4
    struct eXosip_t     *eXcontext;
#endif // EXOSIP_4

	int				pos;
	int				yIntAckCounter = 10;
	
	callCid = eXosipEvent->cid;
	callDid = eXosipEvent->did;
		
	pthread_mutex_lock( &callMapLock);
	callNum = callMap[callCid];

    gSrPort[callNum].setInviteSent(callNum, false);
	xCallCid   = gSrPort[callNum].getCid();
	xCancelCid = gSrPort[callNum].getCancelCid();

	pthread_mutex_unlock( &callMapLock); // BT-99

#ifdef EXOSIP_4
    eXcontext=gMrcpInit.getExcontext();
#endif // EXOSIP_4

	// if ( ( callCid == xCallCid ) && ( xCancelCid == 1 ) )
	if ( callCid != xCallCid )
	{
		if ( callCid == xCancelCid )
		{
			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"CallId (%d) is from the previous unresponive INVITE. "
				"Sending ACK, BYE for callId (%d) and did (%d). "
				"The current application SR license will NOT be cancelled."); 
			gSrPort[callNum].setCancelCid(0);
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
				REPORT_DETAIL, MRCP_2_BASE, WARN,
				"CallIds (%d, %d) do not match. "
				"Sending ACK, BYE for callId (%d) and did (%d); current session callId(%d) will continue.", 
				callCid, xCallCid, callCid, callDid, xCallCid);
//			gSrPort[callNum].setLicenseReserved(false);    // BT-162 - commented this out to 
			                                               //          process new established connection.
			                                               //          Just terminate the new one.
		}

#ifdef EXOSIP_4
        eXosip_lock (eXcontext);
        rc = eXosip_call_send_ack(eXcontext, callDid, NULL);
        eXosip_call_terminate (eXcontext, callCid, callDid);
        eXosip_unlock (eXcontext);
#else
		eXosip_lock ();
		rc = eXosip_call_send_ack(callDid, NULL);
		eXosip_call_terminate (callCid, callDid); 
		eXosip_unlock ();
#endif // EXOSIP_4

		return;
	}


	// send ACK to server
	rc = -1;
	while(rc < 0 && yIntAckCounter--)
	{
#ifdef EXOSIP_4
        eXosip_lock(eXcontext);
        rc = eXosip_call_send_ack(eXcontext, callDid, NULL);
        eXosip_unlock(eXcontext);
#else
		eXosip_lock();
		rc = eXosip_call_send_ack(callDid, NULL);
		eXosip_unlock();
#endif // EXOSIP_4

//		sleep(1);

		usleep(20 * 1000);
	}

	// get callNum from callMap, set callDid and callActive 
//	pthread_mutex_lock( &callMapLock);
//	callNum = callMap[callCid];
//	pthread_mutex_unlock( &callMapLock);
	gSrPort[callNum].setDid(callDid);

	// find server TCP port, RTP port, codec,
	//    payload,and channelId.

#ifdef EXOSIP_4
    eXosip_lock (eXcontext);
    remoteSdp = eXosip_get_remote_sdp (eXcontext, callDid);
    eXosip_unlock (eXcontext);
#else
	eXosip_lock ();
	remoteSdp = eXosip_get_remote_sdp (callDid);
	eXosip_unlock ();
#endif // EXOSIP_4

	if(remoteSdp == NULL)
	{
		mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			REPORT_DETAIL, MRCP_2_BASE, WARN,
			"Empty SDP received from MRCP Server.");

#ifdef EXOSIP_4
        eXosip_lock (eXcontext);
        eXosip_call_terminate (eXcontext, callCid, callDid);
        eXosip_unlock (eXcontext);
#else
		eXosip_lock ();
		eXosip_call_terminate (callCid, callDid); 
		eXosip_unlock ();
#endif // EXOSIP_4

		// return failure to app here
//		gSrPort[callNum].setLicenseReserved(false);
		return;
	}

	pos = 0;
	while(!osip_list_eol (&(remoteSdp->m_medias), pos))
	{
		sdp_media_t     *sdpMedia = NULL;
		sdpMedia = (sdp_media_t*) osip_list_get(&(remoteSdp->m_medias), pos);

		mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"[%d, %d] pos=%d, mrcpPort = (%s), media=(%s) numberOfPort=(%s), protocol=(%s)", 
			callCid, callDid, pos, sdpMedia->m_port, sdpMedia->m_media,
			sdpMedia->m_number_of_port, sdpMedia->m_proto);
	
		// find server TCP port and RTP port
		if ( strncmp(sdpMedia->m_proto, "TCP", 3) == 0 )
		{
			mrcpPort = atoi(sdpMedia->m_port); // TCP port
		}
	
		if ( strncmp(sdpMedia->m_proto, "RTP", 3) == 0 )
		{
			serverRtpPort = atoi(sdpMedia->m_port);

			//find codec and  payload
			int k = 0;
			osip_list_t *payloadList = &(sdpMedia->m_payloads);

			while(!osip_list_eol (payloadList, k))
			{
				sdp_attribute_t *pPayload = NULL;
				string field = (char*)osip_list_get(payloadList, k);
				if(k == 0)
				{
					gSrPort[callNum].setCodec(field);
				}
				else if (k == 1)
				{
					gSrPort[callNum].setPayload(field);
				}

				mrcpClient2Log(__FILE__, __LINE__, callNum, mod, REPORT_VERBOSE,
					MRCP_2_BASE, INFO, "[%d, %d] codec/payload=(%s)", callCid, callDid, field.data());

				k++;

			} // END: while()

		} // END: if (strncmp)
	
// find channelId 
		int k = 0;
		osip_list_t     *attributeList = &(sdpMedia->a_attributes);
		while(!osip_list_eol (attributeList, k))
		{
			sdp_attribute_t *pAttribute = NULL;

			pAttribute = (sdp_attribute_t *)osip_list_get(attributeList, k);
			string attField = string(pAttribute->a_att_field);

			mrcpClient2Log(__FILE__, __LINE__, callNum, mod, REPORT_VERBOSE, 
					MRCP_2_BASE, INFO, "[%d, %d] attribute content = [%s:%s]", 
					callCid, callDid, pAttribute->a_att_field, pAttribute->a_att_value);

			if(attField == string("channel"))
			{
				channelId = pAttribute->a_att_value;
			}

			k++;
		}

		pos++;
	}

	gSrPort[callNum].setMrcpPort(mrcpPort);
	gSrPort[callNum].setServerRtpPort(serverRtpPort);
	gSrPort[callNum].setChannelId(channelId);
	gSrPort[callNum].setMrcpConnectionIP(string(remoteSdp->c_connection->c_addr));
	gSrPort[callNum].setLicenseReserved(true);

//printf("DDNDEBUG: %s %d Setting MRCP IP to <%s>\n", __FILE__, __LINE__, remoteSdp->c_connection->c_addr);fflush(stdout);
	
    sdp_message_free(remoteSdp);
    remoteSdp = NULL;

	return;

} // END: processCallAnswered()
