/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc1889) stack.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "ortp.h"
#include "rtpmod.h"
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>

#include "telephonyevents.h"

#define TIMESTAMP_JUMP	32000

static int temp_dtmf_tab[16]=
    {'0','1','2','3','4','5','6','7','8','9','*','#','A','B','C','D'};

void writeLog( char *msg)
{
    struct timeb tb;
    char t[100];
    char *tmpTime;

    char    yStrCTime[256];

	ftime(&tb);

    tmpTime = ctime_r(&(tb.time), yStrCTime);
    //tmpTime = ctime(&(tb.time));

    sprintf(t, "%s", yStrCTime);
    //sprintf(t, "%s", tmpTime);

    t[strlen(t) - 1] = '\0';

	printf("rtp_parse::%s: ms=%d msg<%s>\n",  t, tb.millitm,msg);fflush(stdout);

}


void rtp_parse(RtpSession *session,mblk_t *mp, int putInQueue)
{
	gint i,header_size;
	mblk_t *mdata;
	rtp_header_t *rtp = NULL;
	gint msgsize;
	static long pk_num = 0;		

	int selfthreadPid = pthread_self();
	int selfPid = getpid();
	struct stat yStat;

	int printDTMFDebug = 0;    
#if 0
	char zFile[128];

	memset(zFile, 0 ,128);
	sprintf(zFile, "/tmp/dtmf_debug.%d", getpid());

	if( (stat(zFile, &yStat)) < 0)
    {
    	printDTMFDebug = 0;
	}
	else
	{
		printDTMFDebug = 1;
	}
#endif


	g_return_if_fail(mp!=NULL);
#ifdef TARGET_IS_HPUXKERNEL
	if (mp->b_cont!=NULL){
		mblk_t *newm;
		/* the message is fragmented, we need to reassemble it*/
		newm=msgpullup(mp,-1);
		freemsg(mp);
		mp=newm;
	}
#endif
	
	msgsize=msgdsize(mp);
	ortp_global_stats.hw_recv+=msgsize;
	session->stats.hw_recv+=msgsize;
	ortp_global_stats.packet_recv++;
	session->stats.packet_recv++;
	
	rtp=(rtp_header_t*)mp->b_rptr;
	if (rtp->version!=2)
	{
		ortp_debug("Receiving rtp packet with version number !=2...discarded");
//printf("RGDEBUG::%s::%d::Receiving rtp packet with version number !=2...discarded\n", __FUNCTION__, __LINE__);fflush(stdout);
		session->stats.bad++;
		ortp_global_stats.bad++;
		freemsg(mp);
		return;
	}
	
	/* convert all header data from network order to host order */
	rtp->seq_number=ntohs(rtp->seq_number);
	rtp->timestamp=ntohl(rtp->timestamp);
	rtp->ssrc=ntohl(rtp->ssrc);
	/* convert csrc if necessary */

//printf("ARCDEBUG:%s Got DTMF packet for session(%p) pid(%d) ts(%i) pt(%d)\n", __FUNCTION__, session, getpid(), rtp->timestamp, rtp->paytype);fflush(stdout);

	if(96 == rtp->paytype || 101 == rtp->paytype || 120 == rtp->paytype || 127 == rtp->paytype)
	{
		struct timeb tb;
    	char yStrCTime[256];
		char t[100];
    	char *tmpTime;
		ftime(&tb);

		tmpTime = ctime_r(&(tb.time), yStrCTime);

		sprintf(t, "%s", yStrCTime);

		t[strlen(t) - 1] = '\0';
			
//printf("ARCDEBUG:%s Got DTMF packet for session(%p) pid(%d) ts(%i) pt(%d) %s:%d\n", __FUNCTION__, session, getpid(), rtp->timestamp, rtp->paytype, t, tb.millitm);fflush(stdout);
	}
	

	for (i=0;i<rtp->cc;i++)
		rtp->csrc[i]=ntohl(rtp->csrc[i]);

	if (session->ssrc!=0)
	{
		/*the ssrc is set, so we must check it */
		if (session->ssrc!=rtp->ssrc)
		{
			ortp_debug("rtp_parse: bad ssrc - 0x%x",rtp->ssrc);
			ortp_debug("rtp_parse: updated ssrc - 0x%x,  emitting ssrc_changed event", rtp->ssrc);
			session->ssrc=rtp->ssrc;
			rtp_signal_table_emit(&session->on_ssrc_changed);
		}
	}
	else
	{
		ortp_debug("rtp_parse: updated ssrc - 0x%x,  but not emitting any event",rtp->ssrc);
		session->ssrc=rtp->ssrc;
	}


#if 0
{
    FILE *fp;
    char filename[64] = "";
    char buffer[32] = "";
	sprintf(filename, "/tmp/recv_audio_packet_%d", pthread_self());
	fp = fopen(filename, "a+");
	if(fp == NULL)
	{
		return -1;
	}
	msg_to_buf (mp, buffer, 20);
	fwrite(buffer, 1, 20, fp);
	fclose(fp);
}
#endif



	
	if (!(session->flags & RTP_SESSION_RECV_SYNC))
	{

//printf("RGDEBUG:: Inside ortpstack rtp_parse: RTP_SESSION_RECV_SYNC (ts=%i)(pid=%d)\n",rtp->timestamp, pthread_self());fflush(stdout);

		/* detect timestamp important jumps in the future, to workaround stupid rtp senders */
		if (RTP_TIMESTAMP_IS_NEWER_THAN(rtp->timestamp,session->rtp.rcv_last_ts+TIMESTAMP_JUMP)){
			ortp_debug("rtp_parse: timestamp jump ?");

			rtp_signal_table_emit2(&session->on_timestamp_jump,&rtp->timestamp);
		}
		else if (RTP_TIMESTAMP_IS_NEWER_THAN(session->rtp.rcv_last_ts,rtp->timestamp)){
			/* avoid very old packet to enqueued, because the user is no more supposed to get them */
			//ortp_debug("rtp_parse: silently discarding very old packet (ts=%i)",rtp->timestamp);

			if (session->payload_type != rtp->paytype && rtp->paytype==session->telephone_events_pt)
			{
				;

#if 0
//if(printDTMFDebug == 1)
{
//printf("RGDEBUG:: Inside ortpstack rtp_parse: very old packet but its DTMF so not discarding(ts=%i)(pid=%d)\n",rtp->timestamp, pthread_self());fflush(stdout);
}
#endif
			}
			else
			{
				;
#if 0
//if(printDTMFDebug == 1)
{
//printf("RGDEBUG:: Inside ortpstack rtp_parse: silently discarding very old packet (ts=%i)(pid=%d)\n",rtp->timestamp, pthread_self());fflush(stdout);
}
#endif

#if 0
				if(rtp->paytype == session->telephone_events_pt)
				{
					freemsg(mp);
					session->stats.outoftime+=msgsize;
					ortp_global_stats.outoftime+=msgsize;
					return;
				}
#endif

			}
		}
	}
	else
	{
//printf("RGDEBUG:: Inside ortpstack rtp_parse: inside else of RTP_SESSION_RECV_SYNC (ts=%i)(pid=%d)\n",rtp->timestamp, pthread_self());fflush(stdout);
	}

	/* creates a new mblk_t to be linked with the rtp header*/
	mdata=dupb(mp);
	header_size=RTP_FIXED_HEADER_SIZE+ (4*rtp->cc);
	mp->b_wptr=mp->b_rptr+header_size;
	mdata->b_rptr+=header_size;
	/* link proto with data */
	mp->b_cont=mdata;
	/* and then add the packet to the queue */
	
	//g_warning("RGDEBUG::putting packet num = %ld in the queue", pk_num);

#if 1

	//if (session->payload_type != rtp->paytype && session->payload_type == 18)
	if (session->payload_type != rtp->paytype)
	{
		if (rtp->paytype==session->telephone_events_pt)
		{
			//g_warning("RGDEBUG::rtp_parse: Inside paytype");
			mblk_t *temp_myBlk   = NULL;                 /*Entire packet*/
    		mblk_t *temp_mp      = NULL;                 /*Data part*/
			rtp_header_t *myRtp = NULL;

			telephone_event_t * myEvent = NULL;
			gint event = -1;


			temp_myBlk = mp;
			myRtp = (rtp_header_t*)(temp_myBlk->b_rptr);
			temp_mp  = temp_myBlk->b_cont;

			myEvent = (telephone_event_t * )temp_mp->b_rptr;
			event = myEvent->event;

#if 0
			char tempMsg[256] = "";
			sprintf(tempMsg, "payload_type=%d, rtp->paytype=%d, socket=%d, ts=%d, session=%p dtmf=%c", 
						session->payload_type, rtp->paytype, session->rtp.socket, rtp->timestamp, 
						session, temp_dtmf_tab[event]);

			writeLog(tempMsg);
#endif

			rtp_signal_table_emit2(&session->on_telephone_event_packet,(gpointer)mp);
			//g_warning("RGDEBUG::rtp_parse: Called on_telephone_event_packet");

			if (session->on_telephone_event.count > 0)
			{
				if (mp==NULL) 
				{
					//g_warning("RGDEBUG:: mp is null!");
				}
				else 
				{
					//g_warning("RGDEBUG::rtp_parse: Calling on_telephone_eventt");
					rtp_session_check_telephone_events(session,mp);
					//g_warning("RGDEBUG::rtp_parse: Called on_telephone_eventt");
				}
			}
			else
			{	
				//g_warning("RGDEBUG::rtp_parse: ERROR not inside session->on_telephone_event.count>0");
			}
			/* we free the telephony event packet and the function will return NULL */
			/* is this good ? */
	
			freemsg(mp);
			mp = NULL;
			return;
		}
	}

#endif
	if(putInQueue == 0)
	{

//printf("RGDEBUG::%s::%d::putInQueue == 0 discarding data\n", __FUNCTION__, __LINE__);fflush(stdout);
		freemsg(mp);
		mp = NULL;
		return;
	}

	rtp_putq(session->rtp.rq,mp);
	/* make some checks: q size must not exceed RtpStream::max_rq_size */
	pk_num++;
	while (session->rtp.rq->q_mcount > session->rtp.max_rq_size)
	{
		/* remove the oldest mblk_t */
		mp=getq(session->rtp.rq);
		if (mp!=NULL)
		{
			msgsize=msgdsize(mp);

//printf("RGDEBUG:: Inside ortpstack rtp_putq: Queue is full. Discarding message with (ts=%i)(pid=%d)\n",((rtp_header_t*)mp->b_rptr)->timestamp, pthread_self());fflush(stdout);
			freemsg(mp);
			session->stats.discarded+=msgsize;
			ortp_global_stats.discarded+=msgsize;
		}
	}
}
