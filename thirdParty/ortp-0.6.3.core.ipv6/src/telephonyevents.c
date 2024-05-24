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

#include "telephonyevents.h"
#include "time.h"


PayloadType	telephone_event={
	PAYLOAD_AUDIO_PACKETIZED, /*type */
	8000,	/*clock rate */
	0,		/* bytes per sample N/A */
	NULL,	/* zero pattern N/A*/
	0,		/*pattern_length N/A */
	0,		/*	normal_bitrate */
	"telephone-event",
	0	/*flags */
};

/* tell if the session supports telephony events. For this the telephony events payload_type 
	must be present in the rtp profile used by the session */
/**
 *rtp_session_telephone_events_supported:
 *@session	:	a rtp session 
 *
 *	Tells whether telephony events payload type is supported within the context of the rtp
 *	session.
 *
 *Returns: the payload type number used for telephony events if found, -1 if not found.
**/
gint rtp_session_telephone_events_supported(RtpSession *session)
{
	//if (session->telephone_events_pt==-1)
	{
		/* search for a telephony event payload in the current profile */

//printf("DDNDEGUB: %s %s %d Setting session->telephone_events_pt to %d\n", __FILE__, __FUNCTION__, __LINE__, rtp_profile_get_payload_number_from_mime(session->profile,"telephone-event"));

		session->telephone_events_pt = rtp_profile_get_payload_number_from_mime(session->profile,"telephone-event");
	}

	return session->telephone_events_pt;
}


/**
 *rtp_session_create_telephone_event_packet:
 *@session: a rtp session.
 *@start:	boolean to indicate if the marker bit should be set.
 *
 *	Allocates a new rtp packet to be used to add named telephony events. The application can use
 *	then rtp_session_add_telephone_event() to add named events to the packet.
 *	Finally the packet has to be sent with rtp_session_sendm_with_ts().
 *
 *Returns: a message block containing the rtp packet if successfull, NULL if the rtp session
 *cannot support telephony event (because the rtp profile it is bound to does not include
 *a telephony event payload type).
**/
mblk_t	*rtp_session_create_telephone_event_packet(RtpSession *session, int start)
{
	mblk_t *mp;
	rtp_header_t *rtp;
	time_t		tm;
	
//	g_return_val_if_fail(session->telephone_events_pt!=-1,NULL);
	if ( session->telephone_events_pt == -1 )
	{
		time(&tm);
		fprintf(stderr, "[%s, %d] timestamp=%ld; assertion #expr failed\n",
				__FILE__, __LINE__, tm);
		return(NULL);
	}

	
	mp=allocb(RTP_FIXED_HEADER_SIZE+TELEPHONY_EVENTS_ALLOCATED_SIZE,BPRI_MED);
	if (mp==NULL) return NULL;
	rtp=(rtp_header_t*)mp->b_rptr;
	rtp->version = 2;
	rtp->markbit=start;
	rtp->padbit = 0;
	rtp->extbit = 0;
	rtp->cc = 0;
	rtp->ssrc = session->ssrc;
	/* timestamp set later, when packet is sended */
	/*seq number set later, when packet is sended */
	
	/*set the payload type */
	rtp->paytype=session->telephone_events_pt;
	
	/*copy the payload */
	mp->b_wptr+=RTP_FIXED_HEADER_SIZE;
	return mp;
}


/**
 *rtp_session_add_telephone_event:
 *@session:	a rtp session.
 *@packet:	a rtp packet as a #mblk_t
 *@event:	the event type as described in rfc2833, ie one of the TEV_ macros.
 *@end:		boolean to indicate if the end bit should be set. (end of tone)
 *@volume:	the volume of the telephony tone, as described in rfc2833
 *@duration:the duration of the telephony tone, in timestamp unit.
 *
 *	Adds a named telephony event to a rtp packet previously allocated using
 *	rtp_session_create_telephone_event_packet().
 *
 *Returns 0 on success.
**/
gint rtp_session_add_telephone_event(RtpSession *session,
			mblk_t *packet, guchar event, gint end, guchar volume, guint16 duration)
{
	mblk_t *mp=packet;
	telephone_event_t *event_hdr;


	/* find the place where to add the new telephony event to the packet */
	while(mp->b_cont!=NULL) mp=mp->b_cont;
	/* see if we need to allocate a new mblk_t */
	if ( (gint)mp->b_wptr >= (gint) mp->b_datap->db_lim){
		mblk_t *newm=allocb(TELEPHONY_EVENTS_ALLOCATED_SIZE,BPRI_MED);
		mp->b_cont=newm;
		mp=mp->b_cont;
	}
	if (mp==NULL) return -1;
	event_hdr=(telephone_event_t*)mp->b_wptr;
	event_hdr->event=event;
	event_hdr->R=0;
	event_hdr->E=end;
	event_hdr->volume=volume;
	event_hdr->duration=htons(duration);
	mp->b_wptr+=sizeof(telephone_event_t);
	return 0;
}
/**
 *rtp_session_send_dtmf:
 *@session	: a rtp session
 *@dtmf		: a character meaning the dtmf (ex: '1', '#' , '9' ...)
 *@userts	: the timestamp
 *
 *	This functions creates telephony events packets for @dtmf and sends them.
 *	It uses rtp_session_create_telephone_event_packet() and
 *	rtp_session_add_telephone_event() to create them and finally
 *	rtp_session_sendm_with_ts() to send them.
 *
 *Returns:	0 if successfull, -1 if the session cannot support telephony events or if the dtmf
 *	given as argument is not valid.
**/
gint rtp_session_send_dtmf(RtpSession *session, gchar dtmf, guint32 userts)
{
	mblk_t *m1,*m2,*m3, *m4;
	int tev_type;
	/* create the first telephony event packet */
	switch (dtmf){
		case '1':
			tev_type=TEV_DTMF_1;
		break;
		case '2':
			tev_type=TEV_DTMF_2;
		break;
		case '3':
			tev_type=TEV_DTMF_3;
		break;
		case '4':
			tev_type=TEV_DTMF_4;
		break;
		case '5':
			tev_type=TEV_DTMF_5;
		break;
		case '6':
			tev_type=TEV_DTMF_6;
		break;
		case '7':
			tev_type=TEV_DTMF_7;
		break;
		case '8':
			tev_type=TEV_DTMF_8;
		break;
		case '9':
			tev_type=TEV_DTMF_9;
		break;
		case '*':
			tev_type=TEV_DTMF_STAR;
		break;
		case '0':
			tev_type=TEV_DTMF_0;
		break;
		case '#':
			tev_type=TEV_DTMF_POUND;
		break;
		default:
		g_warning("Bad dtmf: %c.",dtmf);
		return -1;
	}

	m1=rtp_session_create_telephone_event_packet(session,1);
	if (m1==NULL) return -1;
	rtp_session_add_telephone_event(session,m1,tev_type,0,0,160);
	/* create a second packet */
	m2=rtp_session_create_telephone_event_packet(session,0);
	if (m2==NULL) return -1;
	rtp_session_add_telephone_event(session,m2,tev_type,0,0,320);
		
	/* create a third packet */
	m3=rtp_session_create_telephone_event_packet(session,0);
	if (m3==NULL) return -1;
	rtp_session_add_telephone_event(session,m3,tev_type,0,0,480);
	
	/* create a fourth and final packet */
	m4=rtp_session_create_telephone_event_packet(session,0);
	if (m4==NULL) return -1;
	rtp_session_add_telephone_event(session,m4,tev_type,1,0,640);

	/* and now sends them */
	rtp_session_sendm_with_ts(session,m1,userts);
	rtp_session_sendm_with_ts(session,m2,userts);
	rtp_session_sendm_with_ts(session,m3,userts);
	/* the last packet is sent three times in order to improve reliability*/
	m1=copymsg(m4);
	m2=copymsg(m4);
	/*			NOTE:			*/
	/* we need to copymsg() instead of dupmsg() because the buffers are modified when
	the packet is sended because of the host-to-network conversion of timestamp,ssrc, csrc, and
	seq number.
	It could be avoided by making a copy of the buffer when sending physically the packet, but
	it add one more copy for every buffer.
	Using iomapped socket, it is possible to avoid the user to kernel copy.
	*/
	rtp_session_sendm_with_ts(session,m4,userts);
	rtp_session_sendm_with_ts(session,m1,userts);
	rtp_session_sendm_with_ts(session,m2,userts);
	return 0;
}

/**
 *rtp_session_read_telephone_event:
 *@session: a rtp session from which telephony events are received.
 *@packet:	a rtp packet as a mblk_t.
 *@tab:		the address of a pointer.
 *
 *	Reads telephony events from a rtp packet. *@tab points to the beginning of the event buffer.
 *
 *Returns: the number of events in the packet if successfull, 0 if the packet did not
 *	contain telephony events.
**/
gint rtp_session_read_telephone_event(RtpSession *session,
		mblk_t *packet,telephone_event_t **tab)
{
	int datasize;
	gint num;
	int i;
	telephone_event_t *tev;
	rtp_header_t *hdr=(rtp_header_t*)packet->b_rptr;

	g_return_val_if_fail(packet->b_cont!=NULL,-1);
	if (hdr->paytype!=session->telephone_events_pt) return 0;  /* this is not tel ev.*/
	datasize=msgdsize(packet);
	tev=*tab=(telephone_event_t*)packet->b_cont->b_rptr;
	/* convert from network to host order what should be */
	num=datasize/sizeof(telephone_event_t);
	for (i=0;i<num;i++)
	{
		tev[i].duration=ntohs(tev[i].duration);
	}
	return num;
}


/* for high level telephony event callback */
void rtp_session_check_telephone_events(RtpSession *session, mblk_t *m0)
{
	telephone_event_t *events,*evbuf;
	int num;
	int i;
	mblk_t *mp;
	rtp_header_t *hdr;
	mblk_t *cur_tev;
	
	hdr=(rtp_header_t*)m0->b_rptr;
	mp=m0->b_cont;
	if (hdr->markbit==1)
	{
//		g_warning("RGDEBUG::%s::%d::found marker bit", __FILE__, __LINE__);
		/* this is a start of new events. Store the event buffer for later use*/
		if (session->current_tev!=NULL) {
		
//			g_warning("RGDEBUG::%s::%d::inside session->current_tev!=NULL", __FILE__, __LINE__);
			freemsg(session->current_tev);
			session->current_tev=NULL;
		}
		session->current_tev=copymsg(m0);
		return;
	}
	/* this is a not a begin of event, so update the current event buffer */
	num=(mp->b_wptr-mp->b_rptr)/sizeof(telephone_event_t);
	events=(telephone_event_t*)mp->b_rptr;
	cur_tev=session->current_tev;
	if (cur_tev!=NULL)
	{
//		g_warning("RGDEBUG::%s::%d::inside cur_tev!=NULL", __FILE__, __LINE__);
		/* first compare timestamp, they must be identical */
		if (((rtp_header_t*)cur_tev->b_rptr)->timestamp==
			((rtp_header_t*)m0->b_rptr)->timestamp)
		{
//			g_warning("RGDEBUG::%s::%d::timestamp is identical", __FILE__, __LINE__);
			evbuf=(telephone_event_t*)cur_tev->b_cont;
			for (i=0;i<num;i++)
			{
				if (events[i].E==1)
				{
					/* update events that have ended */
					if (evbuf[i].E==0){
			//			g_warning("RGDEBUG::%s::%d::found end flag", __FILE__, __LINE__);
						evbuf[i].E=1;
						/* this is a end of event, report it */
						rtp_signal_table_emit2(&session->on_telephone_event,(gpointer)(gint)events[i].event);
					}
				}
			}
		}
		else
		{
//			g_warning("RGDEBUG::%s::%d::timestamp is not identical", __FILE__, __LINE__);
			/* timestamp are not identical: this is not the same events*/
			if (session->current_tev!=NULL) {
				freemsg(session->current_tev);
				session->current_tev=NULL;
			}
			session->current_tev=dupmsg(m0);
		}
	}
	else
	{
//		g_warning("RGDEBUG::%s::%d::no marker bit found", __FILE__, __LINE__);
		/* there is no pending events, but we did not received marked bit packet
		either the sending implementation is not compliant, either it has been lost, 
		we must deal with it anyway.*/
		session->current_tev=copymsg(m0);
		/* inform the application if there are tone ends */
		for (i=0;i<num;i++)
		{
			if (events[i].E==1)
			{
//				g_warning("RGDEBUG::%s::%d::sending dtmf", __FILE__, __LINE__);
				
				rtp_signal_table_emit2(&session->on_telephone_event,(gpointer)(gint)events[i].event);
				
			}
		}
	}
}

