/*------------------------------------------------------------------------------
mod,   sipThread.cpp
Author      :   Aumtech, Inc.
Update      :   06/10/06 yyq Created the file.
Purpose     :   MRCP Client v2.0 
  - singleton sipThread waits for event from Mrcp2 server, 
  - For each event of type CALL_ANSWERED, it creates a MrcpThread to
    handle media control communication     
Update: 10/26/2014 djb MR 4241; added exosip_event_wait() can cancelCid logic
------------------------------------------------------------------------------*/
#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/signal.h>

#include "eXosip2/eXosip.h"
//#include "eXosip2/eXosipArcCommon.h"
//#include "aumtech/eXosip2.h"

#include "mrcpThread.hpp"
#include "sipThread.hpp"
#include "mrcpCommon2.hpp"

using namespace std;
using namespace mrcp2client;

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <string.h>
};

static void *sipThreadFunc(void*);
static void processCallAnswered(char *theRealCallID, eXosip_event_t *eXosipEvent);

extern int  gCanProcessReaderThread;
extern int  gCanProcessSipThread;
extern int  gCanProcessMainThread;

/*
class CException
{
public:
  string message;
  CException( string m ) { message = m; }
  void report() {cout << message << endl; };
};
*/

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

/*------------------------------------------------------------------------------
 * SipThreadFunc() 
 *----------------------------------------------------------------------------*/
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
	char 			theRealCallId[256];

    struct sigaction act;

	int callDid  = -1;
	int callCid = -1; 

	int mrcpPort = -1;
	int serverRtpPort = -1;
	int pos;

#ifdef EXOSIP_4
    struct eXosip_t     *eXcontext;
#endif // EXOSIP_4


	sdp_message_t   *remoteSdp = NULL;

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, REPORT_VERBOSE, MRCP_2_BASE,
			INFO, "Wait to receive eXosip event.");

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
			callDid = eXosipEvent->did;

            pthread_mutex_lock( &callMapLock);
            callNum = callMap[callCid];
            pthread_mutex_unlock( &callMapLock);

//			mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
//                REPORT_VERBOSE, MRCP_2_BASE, INFO, 
//				"DJB: Retrieved callNum %d from callMap[%d]; callCid=%d  callDid=%d",
//				callNum, callMap[callCid], callCid, callDid);

			mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                REPORT_VERBOSE, MRCP_2_BASE, INFO, 
                "Received server event [%d:%s]; callCid=%d, callDid=%d",
                eXosipEvent->type, eXosipEvent->textinfo,
                eXosipEvent->cid, eXosipEvent->did);

			if (eXosipEvent->request &&
					eXosipEvent->request->call_id &&
					eXosipEvent->request->call_id->number)
			{
                       if (eXosipEvent->request->call_id->host
                            && eXosipEvent->request->call_id->host[0])
                        {
                            sprintf (theRealCallId, "%s@%s",
                                    eXosipEvent->request->call_id->number,
                                   eXosipEvent->request->call_id->host);
                        }
                        else
                        {
                            sprintf (theRealCallId, "%s",
                                     eXosipEvent->request->call_id->number);
                        }
					
					mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                        REPORT_VERBOSE, MRCP_2_BASE, INFO,
                        "Session Call ID=(%s)",  theRealCallId);
			}

			switch(eXosipEvent->type)
			{
				case EXOSIP_CALL_MESSAGE_NEW:
                    if(eXosipEvent->request && MSG_IS_BYE(eXosipEvent->request)){
                    }
                    break;
				case EXOSIP_CALL_ANSWERED:   // 200 OK
					processCallAnswered(theRealCallId, eXosipEvent);
					break;
				case EXOSIP_CALL_MESSAGE_ANSWERED:
					if ( gSrPort[callNum].IsInviteSent() ) 
					{
						mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
							REPORT_VERBOSE, MRCP_2_BASE, INFO, 
							"INVITE sent and received EXOSIP_CALL_MESSAGE_ANSWERED server event [%d, %s] "
							"from the server. Processing Answered.",
							eXosipEvent->type, eXosipEvent->textinfo);
						processCallAnswered(theRealCallId, eXosipEvent);
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
				case EXOSIP_CALL_RELEASED:  // received BYE event
					break;
				case EXOSIP_CALL_CLOSED:  // received BYE event
#ifndef EXOSIP_4
				case EXOSIP_CALL_BYE_ACK:
#endif // EXOSIP_4
				//-----------------------
		
				// identify who says BYE
					callCid = eXosipEvent->cid;
					pthread_mutex_lock(&callMapLock);
					callNum = callMap[callCid];

//                    mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
//                        REPORT_VERBOSE, MRCP_2_BASE, INFO,
//						"DJB: Retrieved callNum %d from callMap[%d]", callNum, callMap[callCid]);

					callMap.erase(callCid);
					pthread_mutex_unlock(&callMapLock);
					gSrPort[callNum].setLicenseReserved(false);  //MR 4513


                    mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
                        REPORT_VERBOSE, MRCP_2_BASE, INFO,
                        "Received a BYE/ACK event from the server[%s] for "
                        "callId(%d) did(%d).",
                        eXosipEvent->textinfo,
						eXosipEvent->cid, eXosipEvent->did);
					break;
	
				case EXOSIP_CALL_SERVERFAILURE:
				case EXOSIP_CALL_GLOBALFAILURE:
				case EXOSIP_CALL_REQUESTFAILURE:
				case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:
					callCid = eXosipEvent->cid;
					pthread_mutex_lock(&callMapLock);
					callNum = callMap[callCid];

//                    mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
//                        REPORT_VERBOSE, MRCP_2_BASE, INFO,
//						"DJB: Retrieved callNum %d from callMap[%d]", callNum, callMap[callCid]);

					gSrPort[callNum].setLicenseRequestFailed(true);
					pthread_mutex_unlock(&callMapLock);
					mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
						REPORT_VERBOSE, MRCP_2_BASE, INFO, 
						"Received server event [%d, %s]; callId=%d",
						eXosipEvent->type, eXosipEvent->textinfo,
						eXosipEvent->cid);
					break;

				case EXOSIP_CALL_PROCEEDING:
                    callDid = eXosipEvent->did;
                    pthread_mutex_lock(&callMapLock);
                    callNum = callMap[callCid];

//                    mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
//                        REPORT_VERBOSE, MRCP_2_BASE, INFO,
//					   "DJB: Retrieved callNum %d from callMap[%d]", callNum, callMap[callCid]);
//
					gSrPort[callNum].setDid(callDid);	
                    pthread_mutex_unlock(&callMapLock);

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

	
	    	}// END: switch (eXosipEvent->type) 

		}  //END: if(eXosipEvent != NULL) 

    usleep (20 * 1000);

  } // END: while(1) 


	pthread_detach(pthread_self());
	pthread_exit(NULL);


} // END: sipThreadFunc()

/*------------------------------------------------------------------------------
processCallAnswered():
------------------------------------------------------------------------------*/
static void processCallAnswered(char *theRealCallID, eXosip_event_t *eXosipEvent)
{
	static char		mod[]="processCallAnswered";
	int 			rc = -1;
	int 			callNum = -1;
    struct MsgToApp	response;
	string			channelId = "";
	string			serverAddress = "";
	int				callDid  = -1;
	int				callCid = -1; 
    int             xCallCid = -1; 
    int             xCancelCid = -1;
	int				mrcpPort = -1;
	int				serverRtpPort = -1;
	int				yIntSendAckCounter = 10;
#ifdef EXOSIP_4
    struct eXosip_t     *eXcontext;
#endif // EXOSIP_4

	sdp_message_t   *remoteSdp = NULL;

	int				pos;
    int             yIntAckCounter = 10;
	
	callCid = eXosipEvent->cid;
	callDid = eXosipEvent->did;

    pthread_mutex_lock( &callMapLock);
    callNum = callMap[callCid];

//    mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
//			REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"DJB: Retrieved callNum %d from callMap[%d]; callCid=%d; callDid=%d", callNum, callMap[callCid], callCid, callDid);

    pthread_mutex_unlock( &callMapLock);

    gSrPort[callNum].setInviteSent(callNum, false);
    xCallCid   = gSrPort[callNum].getCid();
    xCancelCid = gSrPort[callNum].getCancelCid();

#ifdef EXOSIP_4
	eXcontext=gMrcpInit.getExcontext();
#endif // EXOSIP_4

    // if ( ( callCid == xCallCid ) && ( xCancelCid == 1 ) )
    if ( callCid != xCallCid )
    {
        mrcpClient2Log(__FILE__, __LINE__, callNum, mod,
            REPORT_DETAIL, MRCP_2_BASE, WARN,
            "CallIds (%d, %d) don't match; it should be cancelled.  "
            "Calling eXosip_call_terminate(%d, %d).",
                callCid, xCallCid, callCid, callDid);

#ifdef EXOSIP_4
        eXosip_lock (eXcontext);
        eXosip_call_terminate (eXcontext, callCid, callDid);
        eXosip_unlock (eXcontext);
#else
        eXosip_lock ();
        eXosip_call_terminate (callCid, callDid);
        eXosip_unlock ();
#endif // EXOSIP_4

//      // return failure to app here
        gSrPort[callNum].setLicenseReserved(false);
        return;
    }
  
	// send ACK to server
	rc = -1;

	while(rc == -1 && yIntSendAckCounter--)
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


		mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "%d= eXosip_call_send_ack(%d)",
			rc, callDid);

		usleep(20 * 1000);
	}

	// get callNum from callMap, set callDid and callActive 
//	pthread_mutex_lock(&callMapLock);
//	callNum = callMap[callCid];
//	pthread_mutex_unlock(&callMapLock);
	gSrPort[callNum].setDid(callDid);

	// find server TCP port, RTP port, codec,
	//    payload,and channelId.

#ifdef EXOSIP_4
    eXosip_lock(eXcontext);
    remoteSdp = eXosip_get_remote_sdp (eXcontext, callDid);
    eXosip_unlock(eXcontext);
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
        eXosip_lock(eXcontext);
        eXosip_call_terminate (eXcontext, callCid, callDid);
        eXosip_unlock(eXcontext);
#else
		eXosip_lock ();
		eXosip_call_terminate (callCid, callDid); 
		eXosip_unlock ();
#endif // EXOSIP_4

		// return failure to app here
		gSrPort[callNum].setLicenseReserved(false);
		return;
	}

	pos = 0;
	while(!osip_list_eol (&(remoteSdp->m_medias), pos))
	{
		sdp_media_t     *sdpMedia = NULL;
		sdpMedia = (sdp_media_t*) osip_list_get(&(remoteSdp->m_medias), pos);

		mrcpClient2Log(__FILE__, __LINE__, callNum, mod, 
			REPORT_VERBOSE, MRCP_2_BASE,
			INFO, "pos = %d, mrcpPort = (%s), media=(%s) "
			"numberOfPort=(%s), protocol=(%s) sipCallId=(%s)", 
			pos, sdpMedia->m_port, sdpMedia->m_media,
			sdpMedia->m_number_of_port, sdpMedia->m_proto, theRealCallID);
	
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
					MRCP_2_BASE, INFO, "codec/payload=(%s)", field.data());
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
					MRCP_2_BASE, INFO, "attribute content = [%s:%s]", 
					pAttribute->a_att_field, pAttribute->a_att_value);
			if(attField == string("channel"))
			{
				channelId = pAttribute->a_att_value;
			}
			k++;
		}
		pos++;
	}

	gSrPort[callNum].setMrcpPort(mrcpPort);
	gSrPort[callNum].setChannelId(channelId);
	gSrPort[callNum].setLicenseReserved(true);//DDN MR 4241: Timing issue..moved this line to the bottom.
	
	sdp_message_free(remoteSdp);
    remoteSdp = NULL;

	return;
} // END: processCallAnswered()
