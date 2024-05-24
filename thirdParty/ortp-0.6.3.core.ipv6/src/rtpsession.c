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

#ifdef TARGET_IS_HPUXKERNEL
#else
	#include <fcntl.h>
	#include <errno.h>

	#ifndef _WIN32
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
		#include <arpa/inet.h>
	#ifdef INET6
		#include <netdb.h>
	#endif
	#else

		#include <winsock2.h>
		#include "errno-win32.h"
	#endif
	#include <stdlib.h>

	#include "telephonyevents.h"
	#include "scheduler.h"
#endif

#include "port_fct.h"


gint msg_to_buf (mblk_t * mp, char *buffer, gint len);

void
rtp_session_init (RtpSession * session, gint mode)
{
	memset (session, 0, sizeof (RtpSession));

//printf("%s:%d ::RGDEBUG::Setting socket to -1, socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
	session->rtp.socket = -1;
	session->rtcp.socket = -1;
	session->lastTS = 0;
	session->lastSN = 0;
	session->lastDTMFTS = 0;
	session->isBridgeStarted = 0;

	session->rtp.max_rq_size = RTP_MAX_RQ_SIZE;
	session->rtp.jitt_comp_time = RTP_DEFAULT_JITTER;
	session->mode = mode;
	if ((mode == RTP_SESSION_RECVONLY) || (mode == RTP_SESSION_SENDRECV))
	{
		rtp_session_set_flag (session, RTP_SESSION_RECV_SYNC);
		rtp_session_set_flag (session, RTP_SESSION_RECV_NOT_STARTED);
	}
	if ((mode == RTP_SESSION_SENDONLY) || (mode == RTP_SESSION_SENDRECV))
	{
		rtp_session_set_flag (session, RTP_SESSION_SEND_NOT_STARTED);
		rtp_session_set_flag (session, RTP_SESSION_SEND_SYNC);
	}
	session->telephone_events_pt=-1;	/* not defined a priori */
	rtp_session_set_profile (session, &av_profile);
#ifndef TARGET_IS_HPUXKERNEL
	session->rtp.rq = &session->rtp._rq;
	session->rtp.wq = &session->rtp._wq;
#endif
	session->lock = g_mutex_new ();
	/* init signal tables */
	rtp_signal_table_init (&session->on_ssrc_changed, session);
	rtp_signal_table_init (&session->on_payload_type_changed, session);
	rtp_signal_table_init (&session->on_telephone_event, session);
	rtp_signal_table_init (&session->on_telephone_event_packet, session);
	rtp_signal_table_init (&session->on_timestamp_jump,session);
#ifdef BUILD_SCHEDULER
	session->rtp.wait_for_packet_to_be_sent_mutex = g_mutex_new ();
	session->rtp.wait_for_packet_to_be_sent_cond = g_cond_new ();
	session->rtp.wait_for_packet_to_be_recv_mutex = g_mutex_new ();
	session->rtp.wait_for_packet_to_be_recv_cond = g_cond_new ();
#endif
	session->max_buf_size = UDP_MAX_SIZE;
}

#if 0
/**
 * Resynchronize to the incoming RTP streams.
 * This can be useful to handle discontinuous timestamps.
 * For example, call this function from the timestamp_jump signal handler.
 * @param session the rtp session
**/
void rtp_session_resync(RtpSession *session){
	int ptindex=rtp_session_get_recv_payload_type(session);
	PayloadType *pt=rtp_profile_get_payload(session->rcv.profile,ptindex);

	flushq (&session->rtp.rq, FLUSHALL);
	rtp_session_set_flag(session, RTP_SESSION_RECV_SYNC);
	rtp_session_unset_flag(session,RTP_SESSION_FIRST_PACKET_DELIVERED);
	jitter_control_init(&session->rtp.jittctl,-1,pt);

	/* Since multiple streams might share the same session (fixed RTCP port for example),
	RTCP values might be erroneous (number of packets received is computed
	over all streams, ...). There should be only one stream per RTP session*/
	session->rtp.hwrcv_extseq = 0;
	session->rtp.hwrcv_since_last_SR = 0;
	session->rtp.hwrcv_seq_at_last_SR = 0;
	rtp_session_unset_flag(session, RTP_SESSION_RECV_SEQ_INIT);
}
#endif





/**
 *rtp_session_new:
 *@mode: One of the #RtpSessionMode flags.
 *
 *	Creates a new rtp session.
 *
 *Returns: the newly created rtp session.
**/

RtpSession *
rtp_session_new (gint mode)
{
	RtpSession *session;
#ifdef TARGET_IS_HPUXKERNEL
	MALLOC(session,RtpSession*,sizeof(RtpSession),M_IOSYS,M_WAITOK);
#else
	session = g_malloc (sizeof (RtpSession));
#endif
	rtp_session_init (session, mode);
	return session;
}

/**
 *rtp_session_set_scheduling_mode:
 *@session: a rtp session.
 *@yesno:	a boolean to indicate the scheduling mode.
 *
 *	Sets the scheduling mode of the rtp session. If @yesno is 1, the rtp session is in
 *	the scheduled mode: this means that packet input/output for that session
 *	is done by a thread that is in charge of getting and sending packet at regular time
 *	interval. This is very usefull for outgoing packets, that have to be sent at a time that
 *	matches their timestamp.
 *	If @yesno is zero, then the session is not scheduled. All recv and send operation will
 *	occur when calling respectively rtp_session_recv_with_ts() and rtp_session_send_with_ts().
 *
**/

void
rtp_session_set_scheduling_mode (RtpSession * session, gint yesno)
{
	if (yesno)
	{
#ifdef BUILD_SCHEDULER
		RtpScheduler *sched;
		sched = ortp_get_scheduler ();
		if (sched != NULL)
		{
			rtp_session_set_flag (session, RTP_SESSION_SCHEDULED);
			session->sched = sched;
			rtp_scheduler_add_session (sched, session);
		}
		else
			g_warning
				("rtp_session_set_scheduling_mode: Cannot use scheduled mode because the "
				 "scheduler is not started. Use ortp_scheduler_init() before.");
#else
		g_warning
			("rtp_session_set_scheduling_mode: Cannot use scheduled mode because the "
			 "scheduler is not compiled within this oRTP stack.");
#endif
	}
	else
		rtp_session_unset_flag (session, RTP_SESSION_SCHEDULED);
}


/**
 *rtp_session_set_blocking_mode:
 *@session: a rtp session
 *@yesno: a boolean
 *
 *	This function defines the behaviour of the rtp_session_recv_with_ts() and 
 *	rtp_session_send_with_ts() functions. If @yesno is 1, rtp_session_recv_with_ts()
 *	will block until it is time for the packet to be received, according to the timestamp
 *	passed to the function. After this time, the function returns.
 *	For rtp_session_send_with_ts(), it will block until it is time for the packet to be sent.
 *	If @yesno is 0, then the two functions will return immediately.
 *
**/
void
rtp_session_set_blocking_mode (RtpSession * session, gint yesno)
{
	if (yesno)
		rtp_session_set_flag (session, RTP_SESSION_BLOCKING_MODE);
	else
		rtp_session_unset_flag (session, RTP_SESSION_BLOCKING_MODE);
}

/**
 *rtp_session_set_profile:
 *@session: a rtp session
 *@profile: a rtp profile
 *
 *	Set the RTP profile to be used for the session. By default, all session are created by
 *	rtp_session_new() are initialized with the AV profile, as defined in RFC 1890. The application
 *	can set any other profile instead using that function.
 *
 *
**/

void
rtp_session_set_profile (RtpSession * session, RtpProfile * profile)
{
	session->profile = profile;

//printf("DDNDEGUB: %s %s %d Setting session->telephone_events_pt to %d\n", __FILE__, __FUNCTION__, __LINE__, rtp_session_telephone_events_supported(session));

	rtp_session_telephone_events_supported(session);
}

/**
 *rtp_session_signal_connect:
 *@session: 	a rtp session
 *@signal:		the name of a signal
 *@cb:			a #RtpCallback
 *@user_data:	a pointer to any data to be passed when invoking the callback.
 *
 *	This function provides the way for an application to be informed of various events that
 *	may occur during a rtp session. @signal is a string identifying the event, and @cb is 
 *	a user supplied function in charge of processing it. The application can register
 *	several callbacks for the same signal, in the limit of #RTP_CALLBACK_TABLE_MAX_ENTRIES.
 *	Here are name and meaning of supported signals types:
 *
 *	"ssrc_changed" : the SSRC of the incoming stream has changed.
 *
 *	"payload_type_changed" : the payload type of the incoming stream has changed.
 *
 *	"telephone-event_packet" : a telephone-event rtp packet (RFC1833) is received.
 *
 *	"telephone-event" : a telephone event has occured. This is a shortcut for "telephone-event_packet".
 *
 *	Returns: 0 on success, -EOPNOTSUPP if the signal does not exists, -1 if no more callbacks
 *	can be assigned to the signal type.
**/
int
rtp_session_signal_connect (RtpSession * session, char *signal,
			    RtpCallback cb, gpointer user_data)
{
	if (strcmp (signal, "ssrc_changed") == 0)
	{
		return rtp_signal_table_add (&session->on_ssrc_changed, cb,
					     user_data);
	}
	else if (strcmp (signal, "payload_type_changed") == 0)
	{
		return rtp_signal_table_add (&session->
					     on_payload_type_changed, cb,
					     user_data);
	}
	else if (strcmp (signal, "telephone-event")==0)
	{
		return rtp_signal_table_add (&session->on_telephone_event,cb,user_data);
	}
	else if (strcmp (signal, "telephone-event_packet")==0)
	{
		return rtp_signal_table_add (&session->on_telephone_event_packet,cb,user_data);
	}
	else if (strcmp (signal, "timestamp_jump")==0)
	{
		return rtp_signal_table_add (&session->on_timestamp_jump,cb,user_data);
	}
	g_warning ("rtp_session_signal_connect: inexistant signal.");
	return -EOPNOTSUPP;
}


/**
 *rtp_session_signal_disconnect_by_callback:
 *@session: a rtp session
 *@signal:	a signal name
 *@cb:		a callback function.
 *
 *	Removes callback function @cb to the list of callbacks for signal @signal.
 *
 *Returns: 0 on success, -ENOENT if the callbacks was not found.
**/

int
rtp_session_signal_disconnect_by_callback (RtpSession * session, char *signal,
					   RtpCallback cb)
{
	if (strcmp (signal, "ssrc_changed") == 0)
	{
		return rtp_signal_table_remove_by_callback (&session->
							    on_ssrc_changed,
							    cb);
	}
	else if (strcmp (signal, "payload_type_changed") == 0)
	{
		return rtp_signal_table_remove_by_callback (&session->
							    on_payload_type_changed,
							    cb);
	}
	else if (strcmp (signal,"telephone-event")==0){
		return rtp_signal_table_remove_by_callback(&session->on_telephone_event,cb);
	}
	else if (strcmp (signal,"telephone-event_packet")==0){
		return rtp_signal_table_remove_by_callback(&session->on_telephone_event_packet,cb);
	}
	g_warning
		("rtp_session_signal_disconnect_by_callback: callback not found.");
	return -ENOENT;
}

/**
 *rtp_session_set_local_addr:
 *@session:		a rtp session freshly created.
 *@addr:		a local IP address in the xxx.xxx.xxx.xxx form.
 *@port:		a local port.
 *
 *	Specify the local addr to be use to listen for rtp packets or to send rtp packet from.
 *	In case where the rtp session is send-only, then it is not required to call this function:
 *	when calling rtp_session_set_remote_addr(), if no local address has been set, then the 
 *	default INADRR_ANY (0.0.0.0) IP address with a random port will be used. Calling 
 *	rtp_sesession_set_local_addr() is mandatory when the session is send-only or duplex.
 *
 *	Returns: 0 on success.
**/
#ifdef TARGET_IS_HPUXKERNEL
gint
rtp_session_set_local_addr (RtpSession * session, gchar * addr, gint port)
{
	return EOPNOTSUPP;
}
#else
gint
rtp_session_set_local_addr (RtpSession * session, gchar * addr, gint port, const char *ifname)
{
	gint err;
	gint optval = 1;
#ifdef INET6
	char num[8];
	struct addrinfo hints, *res0, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	snprintf(num, sizeof(num), "%d",port);
	err = getaddrinfo(addr,num, &hints, &res0);
	if (err!=0) {
		g_warning ("Error: %s", gai_strerror(err));
		return err;
	}

	for (res = res0; res; res = res->ai_next) 
	{
		session->rtp.socket = socket(res->ai_family, res->ai_socktype, 0);
//printf("%s::%d::RGDEBUG::after setting socket=%d session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
		if (session->rtp.socket < 0)
		  continue;

        if(ifname && ifname[0] != '\0'){
         if(res->ai_family == AF_INET6){
          struct sockaddr_in6 *ptr;

          ptr = (struct sockaddr_in6 *) res->ai_addr;
          if(ptr){
            ptr->sin6_scope_id = if_nametoindex(ifname);
          }
         }
        }
                
		/* set non blocking mode */
		set_non_blocking_socket (session);
		memcpy(&session->rtp.loc_addr, res->ai_addr, res->ai_addrlen);

//printf("%s::%d::RGDEBUG::Calling bind on socket=%d session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
		err = bind (session->rtp.socket, res->ai_addr, res->ai_addrlen);
		if (err != 0)
		  {
		    g_warning ("%d::Fail to bind rtp socket to port %i: %s.", __LINE__, port, getSocketError());
		    close_socket (session->rtp.socket);
//printf("%s:%d ::RGDEBUG::Setting socket to -1, socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
			session->rtp.socket = -1;
		    freeaddrinfo(res0);
		    return -1;
		  }
		else
		  {
		    /* set the address reusable */
//printf("%s::%d::RGDEBUG::Setting setsockopt reusable socket=%d session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
		    err = setsockopt (session->rtp.socket, SOL_SOCKET, SO_REUSEADDR,
				      (void*)&optval, sizeof (optval));
		    if (err < 0)
		      g_warning ("%d::Fail to set rtp address reusable: %s.", __LINE__, getSocketError());

		    break;
		  }

	}
	freeaddrinfo(res0);
	if (session->rtp.socket < 0){
		if (session->mode==RTP_SESSION_RECVONLY) g_warning("Could not create rtp socket with address %s: %s",addr,getSocketError());
		return -1;
	}
//printf("%s::%d::RGDEBUG::Set Loacl port successful socket=%d session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	snprintf(num, sizeof(num), "%d", (port + 1));

	err = getaddrinfo(addr, num, &hints, &res0);
	for (res = res0; res; res = res->ai_next) {
		session->rtcp.socket = socket(res->ai_family, res->ai_socktype, 0);

		if (session->rtcp.socket < 0)
		  continue;

        if(ifname && ifname[0] != '\0'){
         if(res->ai_family == AF_INET6){
          struct sockaddr_in6 *ptr;

          ptr = (struct sockaddr_in6 *) res->ai_addr;
          if(ptr){
            ptr->sin6_scope_id = if_nametoindex(ifname);
          }
         }
        }
		
		memcpy( &session->rtcp.loc_addr, res->ai_addr, res->ai_addrlen);
		err = bind (session->rtcp.socket, res->ai_addr, res->ai_addrlen);
		if (err != 0)
		  {
		    g_warning ("%d::Fail to bind rtp socket to port %i: %s.", __LINE__, port, getSocketError());

		    close_socket (session->rtp.socket);
//printf("%s:%d ::RGDEBUG::Setting socket to -1, socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
			session->rtp.socket = -1;

		    close_socket (session->rtcp.socket);
			session->rtcp.socket = -1;

		    return -1;
		  }
		optval = 1;
		err = setsockopt (session->rtcp.socket, SOL_SOCKET, SO_REUSEADDR,
				  (void*)&optval, sizeof (optval));
		if (err < 0)
		  g_warning ("%d::Fail to set rtcp address reusable: %s.", __LINE__, getSocketError());
		
		break;
	}
	freeaddrinfo(res0);
	if (session->rtp.socket < 0){
		g_warning("Could not create rtcp socket with address %s: %s",addr,getSocketError());
		return -1;
	}
	return 0;
#else
	session->rtp.loc_addr.sin_family = AF_INET;

	err = inet_aton (addr, &session->rtp.loc_addr.sin_addr);

	if (err < 0)
	{
		g_warning ("Error in socket address:%s.", getSocketError());
		return err;
	}
	session->rtp.loc_addr.sin_port = htons (port);

	session->rtp.socket = socket (PF_INET, SOCK_DGRAM, 0);
//printf("%s:%d ::RGDEBUG:: Error sending rtp packet with send: <%s> error=%d, socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, getSocketError(), error, session->rtp.socket, session);fflush(stdout);
	g_return_val_if_fail (session->rtp.socket > 0, -1);
	
	/* set non blocking mode */
	set_non_blocking_socket (session);

	err = bind (session->rtp.socket,
		    (struct sockaddr *) &session->rtp.loc_addr,
		    sizeof (struct sockaddr_in));

	if (err != 0)
	{
		g_warning ("%d::Fail to bind rtp socket to port %i: %s.", __LINE__, port, getSocketError());
		close_socket (session->rtp.socket);
//printf("%s:%d ::RGDEBUG::Setting socket to -1, socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
		session->rtp.socket = -1;
		return -1;
	}
	/* set the address reusable */
	err = setsockopt (session->rtp.socket, SOL_SOCKET, SO_REUSEADDR,
			  (void*)&optval, sizeof (optval));
	if (err < 0)
	{
		g_warning ("%d::Fail to set rtp address reusable: %s.", __LINE__, getSocketError());
	}
	memcpy (&session->rtcp.loc_addr, &session->rtp.loc_addr,
		sizeof (struct sockaddr_in));
	session->rtcp.loc_addr.sin_port = htons (port + 1);
	session->rtcp.socket = socket (PF_INET, SOCK_DGRAM, 0);
	g_return_val_if_fail (session->rtcp.socket > 0, -1);
	err = bind (session->rtcp.socket,
		    (struct sockaddr *) &session->rtcp.loc_addr,
		    sizeof (struct sockaddr_in));
	if (err != 0)
	{
		g_warning ("%d::Fail to bind rtcp socket to port %i: %s.", __LINE__, port + 1, getSocketError());
		close_socket (session->rtp.socket);
//printf("%s:%d ::RGDEBUG::Setting socket to -1, socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
		session->rtp.socket = -1;
		close_socket (session->rtcp.socket);
		session->rtcp.socket = -1;
		return -1;
	}
	optval = 1;
	err = setsockopt (session->rtcp.socket, SOL_SOCKET, SO_REUSEADDR,
			  (void*)&optval, sizeof (optval));
	if (err < 0)
	{
		g_warning ("%d::Fail to set rtcp address reusable: %s.", __LINE__, getSocketError());
	}
	return 0;
#endif
}
#endif

/**
 *rtp_session_set_remote_addr:
 *@session:		a rtp session freshly created.
 *@addr:		a local IP address in the xxx.xxx.xxx.xxx form.
 *@port:		a local port.
 *
 *	Sets the remote address of the rtp session, ie the destination address where rtp packet
 *	are sent. If the session is recv-only or duplex, it sets also the origin of incoming RTP 
 *	packets. Rtp packets that don't come from addr:port are discarded.
 *
 *	Returns: 0 on success.
**/
#ifdef TARGET_IS_HPUXKERNEL

gint rtp_session_set_remote_addr(RtpSession *session, struct sockaddr_in *dest)
{
	mblk_t *mproto;
	struct T_unitdata_req *req;
	/* make a M_PROTO message to be linked with every outgoing rtp packet */
	mproto=allocb(sizeof(struct T_unitdata_req)+sizeof(struct sockaddr_in),BPRI_MED);
	if (mproto==NULL) return -1;
	mproto->b_datap->db_type=M_PROTO;
	req=(struct T_unitdata_req*)mproto->b_wptr;
	req->PRIM_type=T_UNITDATA_REQ;
	req->DEST_length=sizeof(struct sockaddr_in);
	req->DEST_offset=sizeof(struct T_unitdata_req);
	req->OPT_length=0;
	req->OPT_offset=0;
	mproto->b_wptr+=sizeof(struct T_unitdata_req);
	memcpy((void*)mproto->b_wptr,(void*)dest,sizeof(struct sockaddr_in));
	mproto->b_wptr+=sizeof(struct sockaddr_in);
	rtp_session_lock(session);
	if (session->dest_mproto!=NULL){
		freemsg(session->dest_mproto);
	}
	session->dest_mproto=mproto;
	rtp_session_unlock(session);
	return 0;
}

#else
gint
rtp_session_set_remote_addr (RtpSession * session, gchar * addr, gint port, const char *ifname)
{
	gint err;
    socklen_t soklen;
#ifdef INET6
	struct addrinfo hints, *res0, *res;
	char num[8];
#endif

	if (session->rtp.socket < 0)
	{
		int retry;
		/* the session has not its socket bound, do it */
		g_message ("Setting random local addresses.");

		for (retry=0;retry<20;retry++)
		{
			int localport;
//printf("%s::%d::RGDEBUG::Setting random local addresses retry=%d, session=%p\n", 
//__FUNCTION__, __LINE__, retry, session);fflush(stdout);
			do
			{
				localport = (rand () + 5000) & 0xfffe;
			}
			while ((localport < 5000) || (localport > 0xffff));

			//localport=10500;
//printf("%s::%d::RGDEBUG::Setting random local addresses retry=%d, localport=%d session=%p\n", 
//__FUNCTION__, __LINE__, retry, localport, session);fflush(stdout);
#ifdef INET6
			/* first try an ipv6 address, then an ipv4 */
			err = rtp_session_set_local_addr (session, "::", localport, NULL);
			if (err!=0) err=rtp_session_set_local_addr (session, "0.0.0.0", localport, NULL);
#else
			err = rtp_session_set_local_addr (session, "0.0.0.0", localport, NULL);
#endif

			if (err == 0)
			{
				break;
			}
		}

		if (retry == 20)
		{
//printf("%s::%d::RGDEBUG::Could not find a random local address for socket !\n",  __FUNCTION__, __LINE__);fflush(stdout);
			g_warning("rtp_session_set_remote_addr: Could not find a random local address for socket !");
			return -1;
		}
	}
//printf("%s::%d::RGDEBUG::Set local addresses successful, session=%p\n", 
//__FUNCTION__, __LINE__, session);fflush(stdout);

#ifdef INET6
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_ALL;
	snprintf(num, sizeof(num), "%d", port);
	err = getaddrinfo(addr, num, &hints, &res0);
	if (err) {
//printf("%s::%d::RGDEBUG::Error in socket address: %s\n", __FUNCTION__, __LINE__, gai_strerror(err));fflush(stdout);
		g_warning ("Error in socket address: %s", gai_strerror(err));
		return err;
	}
	
	for (res = res0; res; res = res->ai_next) {
		/*err = connect (session->rtp.socket, res->ai_addr, res->ai_addrlen);
		*/
		/*don't connect: after you are no more able to use recvfrom() and sendto() */
        
        if(ifname && ifname[0] != '\0'){
         if(res->ai_family == AF_INET6){
           struct sockaddr_in6 *ptr;
           ptr = (struct sockaddr_in6 *) res->ai_addr;
           if(ptr != NULL){
             ptr->sin6_scope_id = if_nametoindex(ifname);
           }
         }
        }
         
		err=0;
		if (err == 0) {
          soklen = res->ai_addrlen;
		  memcpy( &session->rtp.rem_addr, res->ai_addr, res->ai_addrlen);
		  break;
		}
	}
	freeaddrinfo(res0);
	if (err != 0)
	  {
//printf("%s::%d::RGDEBUG::Can't connect rtp socket: %s\n",  __FUNCTION__, __LINE__, getSocketError());fflush(stdout);
	    g_message ("Can't connect rtp socket: %s.",getSocketError());
	    return err;
	  }

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	snprintf(num, sizeof(num), "%d", (port + 1));
	err = getaddrinfo(addr, num, &hints, &res0);
	if (err) {
//printf("%s::%d::RGDEBUG::Error: %s\n",  __FUNCTION__, __LINE__, gai_strerror(err));fflush(stdout);
		g_warning ("Error: %s", gai_strerror(err));
		return err;
	}
	for (res = res0; res; res = res->ai_next) {
		/*err = connect (session->rtcp.socket, res->ai_addr, res->ai_addrlen);*/
		/*don't connect: after you are no more able to use recvfrom() and sendto() */

        if(ifname && ifname[0] != '\0'){
         if(res->ai_family == AF_INET6){
           struct sockaddr_in6 *ptr;
           ptr = (struct sockaddr_in6 *) res->ai_addr;
           if(ptr != NULL){
             ptr->sin6_scope_id = if_nametoindex(ifname);
           }
         }
        }

		err=0;
		if (err == 0) {
          soklen = res->ai_addrlen;
		  memcpy( &session->rtcp.rem_addr, res->ai_addr, res->ai_addrlen);
		  break;
		}
	}
	freeaddrinfo(res0);
#else
	session->rtp.rem_addr.sin_family = AF_INET;

	err = inet_aton (addr, &session->rtp.rem_addr.sin_addr);
	if (err < 0)
	{
//printf("%s::%d::RGDEBUG::Error in socket address:%s.\n",  __FUNCTION__, __LINE__, getSocketError());fflush(stdout);
		g_warning ("Error in socket address:%s.", getSocketError());
		return err;
	}
	session->rtp.rem_addr.sin_port = htons (port);

	memcpy (&session->rtcp.rem_addr, &session->rtp.rem_addr,
		sizeof (struct sockaddr_in));
	session->rtcp.rem_addr.sin_port = htons (port + 1);
#endif
#ifndef NOCONNECT
	if (session->mode == RTP_SESSION_SENDONLY)
	{
//printf("%s::%d::RGDEBUG::calling connect on socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
		err = connect (session->rtp.socket,
			       (struct sockaddr *) &session->rtp.rem_addr,
#ifdef INET6
			       //sizeof (session->rtp.rem_addr));
			       soklen);
#else
			       sizeof (struct sockaddr_in));
#endif
		if (err != 0)
		{
//printf("%s::%d::RGDEBUG::Can't connect rtp socket: %s.\n", __FUNCTION__, __LINE__, getSocketError());fflush(stdout);
			g_message ("Can't connect rtp socket: %s.",getSocketError());
			return err;
		}
//printf("%s::%d::RGDEBUG::calling connect on rtcp socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtcp.socket, session);fflush(stdout);
		err = connect (session->rtcp.socket,
			       (struct sockaddr *) &session->rtcp.rem_addr,
#ifdef INET6
			       // sizeof (session->rtcp.rem_addr));
			       soklen);
#else
			       sizeof (struct sockaddr_in));
#endif
		if (err != 0)
		{
//printf("%s::%d::RGDEBUG::Can't connect rtp socket: %s.\n", __FUNCTION__, __LINE__, getSocketError());fflush(stdout);
			g_message ("Can't connect rtp socket: %s.",getSocketError());
			return err;
		}
	}
#endif
	return 0;
}
#endif

void rtp_session_set_sockets(RtpSession *session, gint rtpfd, gint rtcpfd)
{
	session->rtp.socket=rtpfd;
	session->rtcp.socket=rtcpfd;
	session->flags|=RTP_SESSION_USING_EXT_SOCKETS;

//printf("%s:%d ::RGDEBUG:: after setting rtp packet with send:socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
}

/**
 *rtp_session_set_seq_number
 *@session:		a rtp session freshly created.
 *@addr:			a 16 bit unsigned number.
 *
 * sets the initial sequence number of a sending session.
 *
**/
void rtp_session_set_seq_number(RtpSession *session, guint16 seq){
	session->rtp.snd_seq=seq;
}


guint16 rtp_session_get_seq_number(RtpSession *session){
	return session->rtp.snd_seq;
}

#ifdef TARGET_IS_HPUXKERNEL
#ifdef WORDS_BIGENDIAN

#if 0
#define rtp_send(_session,_m) \
	do{\
		mblk_t *_destmp;\
		if ((_session)->dest_mproto!=NULL){\
			_destmp=dupb((_session)->dest_mproto);\
			_destmp->b_cont=(_m);\
			streams_put(putnext,(_session)->rtp.wq,(_destmp),(void*)(_session)->rtp.wq);\
		} else {\
			g_warning("rtp_send: ERROR - there is no destination addreess !");\
			freemsg(_m);\
		}\
	}while (0);
	
#endif
	
#define rtp_send(_session,_m) \
	do{\
		mblk_t *_destmp;\
		if ((_session)->dest_mproto!=NULL){\
			_destmp=dupb((_session)->dest_mproto);\
			_destmp->b_cont=(_m);\
			streams_put(putnext,(_session)->rtp.wq,(_destmp),(void*)(_session)->rtp.wq);\
		} else {\
			streams_put(putnext,(_session)->rtp.wq,(_m),(void*)(_session)->rtp.wq);\
		}\
	}while (0);	

#endif
#else
static gint
rtp_send (RtpSession * session, mblk_t * m)
{
	gint error;
	int i;
	rtp_header_t *hdr;

	if (m->b_cont!=NULL)
	{
		mblk_t *newm;
		newm=msgpullup(m,-1);
		freemsg(m);
		m=newm;
	}

	hdr = (rtp_header_t *) m->b_rptr;
	hdr->ssrc = htonl (hdr->ssrc);
	hdr->timestamp = htonl (hdr->timestamp);
	hdr->seq_number = htons (hdr->seq_number);
	
	
	for (i = 0; i < hdr->cc; i++)
	{
		hdr->csrc[i] = htonl (hdr->csrc[i]);
	}

//printf("%s:%d ::RGDEBUG:: sending rtp packet with send: socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);

	if (session->flags & RTP_SESSION_USING_EXT_SOCKETS)
	{
		error=send(session->rtp.socket, m->b_rptr, (m->b_wptr - m->b_rptr),0);
		if (error < 0)
		{
//printf("%s:%d ::RGDEBUG:: Error sending rtp packet with send: <%s> error=%d, socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, getSocketError(), error, session->rtp.socket, session);fflush(stdout);
		}
	}
	else
	{
		error = sendto (session->rtp.socket, m->b_rptr,
		(m->b_wptr - m->b_rptr), 0,
		(struct sockaddr *) &session->rtp.rem_addr,
		sizeof (session->rtp.rem_addr));
		

		if (error < 0)
		{
#if 0
			char yStrErrMsg[256] = "";
			sprintf(yStrErrMsg, "ls -alrt /proc/%d/fd/%d >> nohup.socket.out 2>> nohup.socket.out", getpid(), session->rtp.socket);

			system(yStrErrMsg);
#endif

//printf("%s:%d ::RGDEBUG:: Error sending rtp packet with sendto: <%s> error=%d socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, getSocketError(), error, session->rtp.socket, session);fflush(stdout);
		}
	}

	if (error < 0)
	{
//printf("%s:%d ::RGDEBUG:: Error sending rtp packet: <%s> error=%d session=%p\n", 
//__FUNCTION__, __LINE__, getSocketError(), error);fflush(stdout);
		g_warning ("Error sending rtp packet: %s.", getSocketError(), session);
	}
	freemsg (m);
//printf("%s:%d ::RGDEBUG:: after sending rtp packet with sendto: error<%d> socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, error, session->rtp.socket, session);fflush(stdout);
	return error;
}


static gint
rtp_send_cross (RtpSession * session, mblk_t * m)
{
	gint error;
	int i;
	rtp_header_t *hdr;

	if (m->b_cont!=NULL){
		mblk_t *newm;
		newm=msgpullup(m,-1);
		m=newm;
	}
	hdr = (rtp_header_t *) m->b_rptr;
	hdr->ssrc = htonl (hdr->ssrc);
	hdr->timestamp = htonl (hdr->timestamp);
	hdr->seq_number = htons (hdr->seq_number);
	
	
	for (i = 0; i < hdr->cc; i++)
		hdr->csrc[i] = htonl (hdr->csrc[i]);
	if (session->flags & RTP_SESSION_USING_EXT_SOCKETS){
		error=send(session->rtp.socket, m->b_rptr, (m->b_wptr - m->b_rptr),0);
	}else error = sendto (session->rtp.socket, m->b_rptr,
		(m->b_wptr - m->b_rptr), 0,
		(struct sockaddr *) &session->rtp.rem_addr,
		sizeof (session->rtp.rem_addr));

	if (error < 0)
		g_warning ("Error sending rtp packet: %s.", getSocketError());
	return error;
}


#endif

/**
 *rtp_session_set_jitter_compensation:
 *@session: a RtpSession
 *@milisec: the time interval in milisec to be jitter compensed.
 *
 * Sets the time interval for which packet are buffered instead of being delivered to the 
 * application.
 **/
void
rtp_session_set_jitter_compensation (RtpSession * session, gint milisec)
{
	PayloadType *payload = rtp_profile_get_payload (session->profile,
							session->
							payload_type);
	if (payload==NULL){
		g_warning("rtp_session_set_jitter_compensation: cannot set because the payload type is unknown");
		return;
	}
	/* REVISIT: need to take in account the payload description */
	session->rtp.jitt_comp = milisec;
	/* convert in timestamp unit: */
	session->rtp.jitt_comp_time =
		(gint) (((double) milisec / 1000.0) * (payload->clock_rate));
}


/**
 *rtp_session_set_ssrc:
 *@session: a rtp session.
 *@ssrc: an unsigned 32bit integer representing the synchronisation source identifier (SSRC).
 *
 *	Sets the SSRC of the session.
 *
**/
void
rtp_session_set_ssrc (RtpSession * session, guint32 ssrc)
{
	session->ssrc = ssrc;
}

/**
 *rtp_session_set_payload_type:
 *@session: a rtp session
 *@paytype: the payload type
 *
 *	Sets the payload type of the rtp session. It decides of the payload types written in the
 *	of the rtp header for the outgoing stream, if the session is SENDRECV or SENDONLY.
 *	For the incoming stream, it sets the waited payload type. If that value does not match
 *	at any time this waited value, then the application can be informed by registering
 *	for the "payload_type_changed" signal, so that it can make the necessary changes
 *	on the downstream decoder that deals with the payload of the packets.
 *
 *Returns: 0 on success, -1 if the payload is not defined.
**/

int
rtp_session_set_payload_type (RtpSession * session, int paytype)
{
	session->payload_type = paytype;
	return 0;
}

int rtp_session_get_payload_type(RtpSession *session){
	return session->payload_type;
}


/**
 *rtp_session_set_payload_type_with_string:
 *@session: a rtp session
 *@paytype: the payload type
 *
 *	Sets the payload type of the rtp session. It decides of the payload types written in the
 *	of the rtp header for the outgoing stream, if the session is SENDRECV or SENDONLY.
 * 	Unlike #rtp_session_set_payload_type(), it takes as argument a string referencing the
 *	payload type (mime type).
 *	For the incoming stream, it sets the waited payload type. If that value does not match
 *	at any time this waited value, then the application can be informed by registering
 *	for the "payload_type_changed" signal, so that it can make the necessary changes
 *	on the downstream decoder that deals with the payload of the packets.
 *
 *Returns: 0 on success, -1 if the payload is not defined.
**/

int
rtp_session_set_payload_type_with_string (RtpSession * session, const char * mime)
{
	int pt;
	pt=rtp_profile_get_payload_number_from_mime(session->profile,mime);
	if (pt<0) {
		g_warning("%s is not a know mime string within the rtpsession's profile.",mime);
		return -1;
	}
	session->payload_type = pt;
	return 0;
}


/**
 *rtp_session_create_packet:
 *@session:		a rtp session.
 *@header_size:	the rtp header size. For standart size (without extensions), it is #RTP_FIXED_HEADER_SIZE
 *@payload		:data to be copied into the rtp packet.
 *@payload_size	: size of data carried by the rtp packet.
 *
 *	Allocates a new rtp packet. In the header, ssrc and payload_type according to the session's
 *	context. Timestamp and seq number are not set, there will be set when the packet is going to be
 *	sent with rtp_session_sendm_with_ts().
 *
 *Returns: a rtp packet in a mblk_t (message block) structure.
**/
mblk_t * rtp_session_create_packet(RtpSession *session,gint header_size, char *payload, gint payload_size, int markBit)
{
	mblk_t *mp;
	gint msglen=header_size+payload_size;
	rtp_header_t *rtp;
	
	mp=allocb(msglen,BPRI_MED);
#ifdef _KERNEL
	if (mp==NULL) return NULL;
#endif
	rtp=(rtp_header_t*)mp->b_rptr;
	rtp->version = 2;
	rtp->padbit = 0;
	rtp->extbit = 0;
	rtp->markbit= markBit;
	rtp->cc = 0;
	//rtp_session_lock(session);
	rtp->paytype = session->payload_type;
	rtp->ssrc = session->ssrc;
	//rtp_session_unlock(session);
	rtp->timestamp = 0;	/* set later, when packet is sended */
	// rtp->seq_number = 0; /*set later, when packet is sended */
    // setting this explicitly to test a bug 
	rtp->seq_number = session->rtp.snd_seq; 
	/*copy the payload */
	mp->b_wptr+=header_size;
	memcpy(mp->b_wptr,payload,payload_size);
	mp->b_wptr+=payload_size;
	return mp;
}

#if 0
gint
rtp_session_get_queue(RtpSession * session, char **buffer, int *size, int len)
{
	mblk_t *oldfirst 	= NULL;
	mblk_t *mp      	= NULL;

	rtp_header_t *tmprtp = NULL;

	gint totalSize = 0;

	int	counter = 50;

	gint rlen = len;
	gint wlen, mlen;

	char *yTmpBuffer = NULL;

	*buffer = (char*)(malloc(160 * 50));

	if(*buffer == NULL)
	{
		*size = 0;
		return(0);
	}

	yTmpBuffer = *buffer;

	if(!session)
	{
		*size = 0;
		return(0);
	}

	oldfirst = session->rtp.rq->q_first;

	if (oldfirst==NULL)
	{
		*size = 0;
		return(0);
	}

	while(oldfirst != NULL && counter > 0)
	{
		mp  = oldfirst->b_cont;

		/*TO DO: Realloc the buffer, maintain  total  count*/

		//mlen = msgdsize(mp->b_cont);
		mlen = msgdsize(oldfirst->b_cont);


		wlen = msg_to_buf(oldfirst, *buffer, rlen);

		*buffer += wlen;

		totalSize+=wlen;
		/*END: TO DO*/

		oldfirst = oldfirst->b_next;

		counter--;
	}

	*size 	= totalSize;
	*buffer = yTmpBuffer;


	return(totalSize);

}/**/

#endif

/**
 *rtp_session_sendm_with_ts:
 *@session	: a rtp session.
 *@mp		:	a rtp packet presented as a mblk_t.
 *@timestamp:	the timestamp of the data to be sent. Refer to the rfc to know what it is.
 *
 *	Send the rtp datagram @mp to the destination set by rtp_session_set_remote_addr() 
 *	with timestamp @timestamp. For audio data, the timestamp is the number
 *	of the first sample resulting of the data transmitted. See rfc1889 for details.
 *
 *Returns: the number of bytes sent over the network.
**/
gint
rtp_session_sendm_with_ts (RtpSession * session, mblk_t *mp, guint32 timestamp)
{
	rtp_header_t *rtp;
	guint32 packet_time;
	gint error = 0;
	gint msgsize;
#ifdef BUILD_SCHEDULER
	RtpScheduler *sched;
#endif

	if (session->flags & RTP_SESSION_SEND_NOT_STARTED)
	{
		session->rtp.snd_ts_offset = timestamp;
#ifdef BUILD_SCHEDULER
		if (session->flags & RTP_SESSION_SCHEDULED)
		{
			sched = ortp_get_scheduler ();
			
			session->rtp.snd_time_offset = sched->time_;
			//g_message("setting snd_time_offset=%i",session->rtp.snd_time_offset);
		}
#endif
		rtp_session_unset_flag (session,RTP_SESSION_SEND_NOT_STARTED);
	}

	rtp=(rtp_header_t*)mp->b_rptr;
	
	msgsize = msgdsize(mp);
	rtp_session_lock (session);
	
	/* set a seq number */
	rtp->seq_number=session->rtp.snd_seq;
	rtp->timestamp=timestamp;
	session->rtp.snd_seq++;
	session->rtp.snd_last_ts = timestamp;


	ortp_global_stats.sent += msgsize;
	session->stats.sent += msgsize;
	ortp_global_stats.packet_sent++;
	session->stats.packet_sent++;

#ifdef TARGET_IS_HPUXKERNEL
	/* send directly the message through the stream */
	rtp_send (session, mp);

#else
	if (!(session->flags & RTP_SESSION_SCHEDULED))
	{
		error = rtp_send (session, mp);
if(error < 0 )
{
	//printf("%s:%d ::RGDEBUG:: failed in rtp_send, error=%d\n", __FUNCTION__, __LINE__, error);fflush(stdout);
}
	}
	else
	{
	//printf("%s:%d ::RGDEBUG:: calling putq for session=%p\n", __FUNCTION__, __LINE__, session);fflush(stdout);
		putq (session->rtp.wq, mp);
	}
#endif

	rtp_session_unlock (session);
	/* if we are in blocking mode, then suspend the process until the scheduler sends the
	 * packet */
	/* if the timestamp of the packet queued is older than current time, then you we must
	 * not block */
#ifdef BUILD_SCHEDULER
	if (session->flags & RTP_SESSION_SCHEDULED)
	{
		sched = ortp_get_scheduler ();
		packet_time =
			rtp_session_ts_to_t (session,
				     timestamp -
				     session->rtp.snd_ts_offset) +
					session->rtp.snd_time_offset;
		//g_message("rtp_session_send_with_ts: packet_time=%i time=%i",packet_time,sched->time_);
		if (TIME_IS_STRICTLY_NEWER_THAN (packet_time, sched->time_))
		{
			if (session->flags & RTP_SESSION_BLOCKING_MODE)
			{
				//g_message("waiting packet to be sent");
				g_mutex_lock (session->rtp.
				      wait_for_packet_to_be_sent_mutex);
				g_cond_wait (session->rtp.
				     wait_for_packet_to_be_sent_cond,
				     session->rtp.
				     wait_for_packet_to_be_sent_mutex);
				g_mutex_unlock (session->rtp.
					wait_for_packet_to_be_sent_mutex);
			}
			session_set_clr(&sched->w_sessions,session);	/* the session has written */

		}
		else session_set_set(&sched->w_sessions,session);	/*to indicate select to return immediately */
	}
#endif

if(error < 0)	
{
//printf("%s:%d ::RGDEBUG:: returning from rtp_session_sendm_with_ts, error=%d session=%p\n", __FUNCTION__, __LINE__, error, session);fflush(stdout);
}

	return error;
}



/**
 *rtp_session_send_with_ts:
 *@session: a rtp session.
 *@buffer:	a buffer containing the data to be sent in a rtp packet.
 *@len:		the length of the data buffer, in bytes.
 *@userts:	the timestamp of the data to be sent. Refer to the rfc to know what it is.
 *
 *	Send a rtp datagram to the destination set by rtp_session_set_remote_addr() containing
 *	the data from @buffer with timestamp @userts. This is a high level function that uses
 *	rtp_session_create_packet() and rtp_session_sendm_with_ts() to send the data.
 *
 *
 *Returns: the number of bytes sent over the network.
**/


extern gint
rtp_session_send_video_with_ts (RtpSession * session, gchar * buffer, gint len,
			  guint32 userts)
{
	return  sendto (session->rtp.socket, buffer,
            len, 0,
            (struct sockaddr *) &session->rtp.rem_addr,
            sizeof (session->rtp.rem_addr));
}


gint
rtp_session_send_with_ts (RtpSession * session, gchar * buffer, gint len,
			  guint32 userts, gint type, gint markBit)
{
	mblk_t *m;
	gint msgsize;


	if(type == 1)
	{
		return  sendto (session->rtp.socket, buffer,
            len, 0,
            (struct sockaddr *) &session->rtp.rem_addr,
            sizeof (session->rtp.rem_addr));
	}
	
	


	/* allocate a mblk_t, set the haeder. Perhaps if len>MTU, we should allocate a new
	 * mlkt_t to split the packet FIXME */
	msgsize = len + RTP_FIXED_HEADER_SIZE;
	m = rtp_session_create_packet(session,RTP_FIXED_HEADER_SIZE,buffer,len, markBit);
	if (m == NULL)
	{
//printf("%s:%d ::RGDEBUG:: Could not allocate message block for sending packet. returning -1\n", 
//__FUNCTION__, __LINE__);fflush(stdout);
		g_warning
			("Could not allocate message block for sending packet.");
		return -1;
	}
	
	return rtp_session_sendm_with_ts(session,m,userts);
}


#ifdef TARGET_IS_HPUXKERNEL
static gint
rtp_recv (RtpSession * session, RtpSession *cross_session = NULL)
{
	return EOPNOTSUPP;
}
#else
static gint
rtp_recv (RtpSession * session, RtpSession *cross_session)
{
	gint error = 0;
	struct sockaddr_in remaddr;
	int addrlen = sizeof (struct sockaddr_in);
	char *p;
	mblk_t *mp;
	fd_set fdset;
	struct timeval timeout = { 0, 0 };

	if (session->rtp.socket<1) return -1;  /*session has no sockets for the moment*/
	FD_ZERO (&fdset);
	if (!session)
		printf("Session null");
	FD_SET (session->rtp.socket, &fdset);

	while (1)
	{
		error = select (session->rtp.socket + 1, &fdset, NULL, NULL,
				&timeout);

		if ((error == 1) && (FD_ISSET (session->rtp.socket, &fdset)))	/* something to read */
		{

			mp = allocb (session->max_buf_size, 0);
			if (session->flags & RTP_SESSION_USING_EXT_SOCKETS){

				error=recv(session->rtp.socket, mp->b_wptr,session->max_buf_size, 0);
			}
			else
			{ 
				error = recvfrom (session->rtp.socket, mp->b_wptr,
						session->max_buf_size, 0,
						(struct sockaddr *) &remaddr,
						&addrlen);
			}
			if (error > 0)
			{
				if(cross_session != NULL)
				{
					guint32 tempTS;
					guint16 tempSeq;
					guint32 tempSsrc;
					guint16 currentSN;
					guint32 currentTS;
					guint32 ts_increment = 160;
					guint16 sn_increment = 1;

					rtp_header_t *rtp;
					rtp=(rtp_header_t*)mp->b_rptr;
					currentTS = htonl(rtp->timestamp);
					currentSN = htons(rtp->seq_number);

					if(session->lastTS >= currentTS)
					{
						ts_increment = currentTS-session->lastTS;
					}

					if(session->isBridgeStarted == 0)
					{
						session->lastSN = currentSN ;
						session->isBridgeStarted = 1;
					}

					sn_increment = currentSN - session->lastSN;

					tempSsrc = rtp->ssrc ;
					rtp->ssrc = cross_session->ssrc;

					tempSeq = rtp->seq_number;
					rtp->seq_number= cross_session->rtp.snd_seq + sn_increment;
					cross_session->rtp.snd_seq = rtp->seq_number;
					rtp->seq_number = htons (rtp->seq_number);

					if(rtp->paytype==session->telephone_events_pt)
					{
//printf("RGDEBUG::%s::%d::session=%p, cross_session=%p paytype=%d session_paytype=%d.\n", __FILE__, __LINE__, session, cross_session, rtp->paytype, session->telephone_events_pt);fflush(stdout);
						tempTS = rtp->timestamp;
						if(session->lastDTMFTS == htonl (rtp->timestamp))
						{
							rtp->timestamp = cross_session->lastDTMFTS;
//printf("RGDEBUG::%s::%d::session=%p, cross_session=%p NOT INCREAMENTING ts_increment=%ld.\n", __FILE__, __LINE__, session, cross_session, ts_increment);fflush(stdout);
						}
						else
						{
							rtp->timestamp = cross_session->rtp.snd_last_ts + ts_increment;
							cross_session->rtp.snd_last_ts = rtp->timestamp;
							rtp->timestamp = htonl (rtp->timestamp);
//printf("RGDEBUG::%s::%d::session=%p, cross_session=%p ts_increment=%ld.\n", __FILE__, __LINE__, session, cross_session, ts_increment);fflush(stdout);
						}
						cross_session->lastDTMFTS = rtp->timestamp;
						session->lastDTMFTS = htonl(tempTS);
					}
					else
					{
//printf("RGDEBUG::%s::%d::session=%p, cross_session=%p ts_increment=%ld.\n", __FILE__, __LINE__, session, cross_session, ts_increment);fflush(stdout);
						tempTS = rtp->timestamp;
						rtp->timestamp = cross_session->rtp.snd_last_ts + ts_increment;
						cross_session->rtp.snd_last_ts = rtp->timestamp;
						rtp->timestamp = htonl (rtp->timestamp);
					}

					sendto (cross_session->rtp.socket, mp->b_wptr,
									error, 0,
									(struct sockaddr *) &cross_session->rtp.rem_addr,
									sizeof (cross_session->rtp.rem_addr));

					rtp->timestamp = tempTS;
					rtp->seq_number = tempSeq;
					rtp->ssrc = tempSsrc;

					session->lastTS = currentTS;
					session->lastSN = currentSN;

				}

				p = g_realloc (mp->b_wptr, error);
				if (p != mp->b_wptr)
					ortp_debug("The recv area has moved during reallocation.");

				mp->b_datap->db_base = mp->b_rptr = mp->b_wptr = p;

				mp->b_wptr += error;
				mp->b_datap->db_lim = mp->b_wptr;

				/* then put the new message on queue */
				rtp_parse (session, mp, 1);
			}
			else
			{
				//printf("RGDEBUG::%s::%d::no data in rtp stack error = %d\n", __FUNCTION__, __LINE__, error);fflush(stdout);
				if (error == 0)
				{
					g_warning
						("rtp_stack_recv: strange... recv() returned zero.");
				}
				else if (errno != EWOULDBLOCK)
				{
					g_warning
						("Error receiving udp packet: %s.",getSocketError());
				}
				freemsg (mp);
				return -1;	/* avoids an infinite loop ! */
			}
		}
		else
		{
			return 0;
		}
	}
	return error;
}
#endif

/**
 *rtp_session_recvm_with_ts:
 *@session: a rtp session.
 *@user_ts:	a timestamp.
 *
 *	Try to get a rtp packet presented as a mblk_t structure from the rtp session.
 *	The @user_ts parameter is relative to the first timestamp of the incoming stream. In other
 *	words, the application does not have to know the first timestamp of the stream, it can
 *	simply call for the first time this function with @user_ts=0, and then incrementing it
 *	as it want. The RtpSession takes care of synchronisation between the stream timestamp
 *	and the user timestamp given here.
 *
 *Returns: a rtp packet presented as a mblk_t.
**/

mblk_t *
rtp_session_recvm_with_ts (RtpSession * session, guint32 user_ts, RtpSession * cross_session)
{
	mblk_t *mp = NULL;
	rtp_header_t *rtp;
	guint32 ts;
	guint32 packet_time;
#ifdef BUILD_SCHEDULER
	RtpScheduler *sched;
#endif
	/* if we are scheduled, remember the scheduler time at which the application has
	 * asked for its first timestamp */

//printf("%s:%d ::RGDEBUG:: 1 ts=%d. (pid=%d)\n", __FUNCTION__, __LINE__, user_ts, getpid());fflush(stdout);

	if (session->flags & RTP_SESSION_RECV_NOT_STARTED)
	{
		
		session->rtp.rcv_query_ts_offset = user_ts;
#ifdef BUILD_SCHEDULER
		if (session->flags & RTP_SESSION_SCHEDULED)
		{
			sched = ortp_get_scheduler ();
			session->rtp.rcv_time_offset = sched->time_;
			//g_message("setting snd_time_offset=%i",session->rtp.snd_time_offset);
		}
#endif
		rtp_session_unset_flag (session,RTP_SESSION_RECV_NOT_STARTED);
	}
	session->rtp.rcv_last_app_ts = user_ts;
#ifdef TARGET_IS_HPUXKERNEL
	/* nothing to do: packets are enqueued on interrupt ! */
#else
	if (!(session->flags & RTP_SESSION_SCHEDULED))	/* if not scheduled */
	{
//printf("%s:%d ::RGDEBUG:: 2 ts=%d. (pid=%d)\n", __FUNCTION__, __LINE__, user_ts, getpid());fflush(stdout);
		rtp_recv (session, cross_session);
	}
#endif
	/* then now try to return a packet, if possible */
	/* first condition: if the session is starting, don't return anything
	 * until the queue size reaches jitt_comp */
	/* first lock the session */
	rtp_session_lock (session);
	if (session->flags & RTP_SESSION_RECV_SYNC)
	{
		rtp_header_t *oldest, *newest;
		queue_t *q = session->rtp.rq;
		if (q->q_last == NULL)
		{
//printf("%s:%d ::RGDEBUG:: 3 ts=%d. (pid=%d)\n", __FUNCTION__, __LINE__, user_ts, getpid());fflush(stdout);
			//ortp_debug ("Queue is empty.");
			goto end;
		}
		oldest = (rtp_header_t *) q->q_first->b_rptr;
		newest = (rtp_header_t *) q->q_last->b_rptr;
		if ((guint32) (newest->timestamp - oldest->timestamp) <
		    session->rtp.jitt_comp_time)
		{
			//ortp_debug("Not enough packet bufferised.");
//printf("%s:%d ::RGDEBUG:: 4 ts=%d. (pid=%d)\n", __FUNCTION__, __LINE__, user_ts, getpid());fflush(stdout);
			goto end;
		}
		/* if enough packet queued continue but delete the starting flag */
		rtp_session_unset_flag (session, RTP_SESSION_RECV_SYNC);

		mp = getq (session->rtp.rq);
		rtp = (rtp_header_t *) mp->b_rptr;
		session->rtp.rcv_ts_offset = rtp->timestamp;
		session->rtp.rcv_app_ts_offset = user_ts;
		/* and also remember the timestamp offset between the stream timestamp (random)
		 * and the user timestamp, that very often starts at zero */
		session->rtp.rcv_diff_ts = rtp->timestamp - user_ts;
		session->rtp.rcv_last_ret_ts = user_ts;	/* just to have an init value */
		session->ssrc = rtp->ssrc;
		//ortp_debug("Returning FIRST mp with ts=%i", rtp->timestamp);
//printf("%s:%d ::RGDEBUG:: 5 ts=%d. (pid=%d)\n", __FUNCTION__, __LINE__, user_ts, getpid());fflush(stdout);

		goto end;
	}
	/* else this the normal case */
	/*calculate the stream timestamp from the user timestamp */
	ts = user_ts + session->rtp.rcv_diff_ts;
	session->rtp.rcv_last_ts = ts;
//printf("%s:%d ::RGDEBUG:: calling rtp_getq with ts=%d. (pid=%d)\n", __FUNCTION__, __LINE__, ts, getpid());fflush(stdout);
	mp = rtp_getq (session->rtp.rq, ts);

	/* perhaps we can now make some checks to see if a resynchronization is needed */
	/* TODO */
	goto end;

      end:
	if (mp != NULL)
	{
		int msgsize = msgdsize (mp);	/* evaluate how much bytes (including header) is received by app */
	
		static long pk_num = 0;		


		ortp_global_stats.recv += msgsize;
		session->stats.recv += msgsize;
		rtp = (rtp_header_t *) mp->b_rptr;
		//ortp_debug("Returning mp with ts=%i", rtp->timestamp);
		/* check for payload type changes */
		if (session->payload_type != rtp->paytype)
		{
			/* this is perhaps a telephony event */
			if (rtp->paytype==session->telephone_events_pt){
				rtp_signal_table_emit2(&session->on_telephone_event_packet,(gpointer)mp);
				if (session->on_telephone_event.count>0){
					if (mp==NULL) {
						g_warning("mp is null!");
					}else rtp_session_check_telephone_events(session,mp);
				}
				/************ warning**********/
				/* we free the telephony event packet and the function will return NULL */
				/* is this good ? */
				freemsg(mp);
				mp=NULL;
			}else{
				/* check if we support this payload type */
				PayloadType *pt=rtp_profile_get_payload(session->profile,rtp->paytype);
				if (pt!=0){
					g_message ("rtp_parse: payload type changed to %i !",
						 rtp->paytype);
					session->payload_type = rtp->paytype;

//printf("DDNDEBUG: Calling RtpCallback from %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

					rtp_signal_table_emit (&session->on_payload_type_changed);
				}else{
					g_warning("Receiving packet with unknown payload type %i.",rtp->paytype);
				}
			}
		}
		pk_num++;
		
	}
	else
	{
		//ortp_debug ("No mp for timestamp queried");
		session->stats.unavaillable++;
		ortp_global_stats.unavaillable++;
	}
	rtp_session_unlock (session);
#ifdef BUILD_SCHEDULER
	if (session->flags & RTP_SESSION_SCHEDULED)
	{
		/* if we are in blocking mode, then suspend the calling process until timestamp
		 * wanted expires */
		/* but we must not block the process if the timestamp wanted by the application is older
		 * than current time */
		sched = ortp_get_scheduler ();
		packet_time =
			rtp_session_ts_to_t (session,
				     user_ts -
				     session->rtp.rcv_query_ts_offset) +
			session->rtp.rcv_time_offset;
		//ortp_debug ("rtp_session_recvm_with_ts: packet_time=%i, time=%i",packet_time, sched->time_);
		if (TIME_IS_STRICTLY_NEWER_THAN (packet_time, sched->time_))
		{
			if (session->flags & RTP_SESSION_BLOCKING_MODE)
			{
				g_mutex_lock (session->rtp.
				      wait_for_packet_to_be_recv_mutex);
				g_cond_wait (session->rtp.
				     wait_for_packet_to_be_recv_cond,
				     session->rtp.
				     wait_for_packet_to_be_recv_mutex);
				g_mutex_unlock (session->rtp.
					wait_for_packet_to_be_recv_mutex);
			}
			session_set_clr(&sched->r_sessions,session);
		}
		else session_set_set(&sched->r_sessions,session);	/*to unblock _select() immediately */
	}
#endif
	return mp;
}


gint msg_to_buf (mblk_t * mp, char *buffer, gint len)
{
	gint rlen = len;
	mblk_t *m, *mprev;
	gint mlen;
	m = mp->b_cont;
	mprev = mp;
	while (m != NULL)
	{
		mlen = m->b_wptr - m->b_rptr;
		if (mlen <= rlen && mlen > 0)
		{
			mblk_t *consumed = m;
			memcpy (buffer, m->b_rptr, mlen);
			/* go to next mblk_t */
			mprev->b_cont = m->b_cont;
			m = m->b_cont;
			consumed->b_cont = NULL;
			freeb (consumed);
			buffer += mlen;
			rlen -= mlen;
		}
		else
		{		/*if mlen>rlen */
			memcpy (buffer, m->b_rptr, rlen);
			m->b_rptr += rlen;
			return len;
		}
	}
	return len - rlen;
}

/**
 *rtp_session_recv_with_ts:
 *@session: a rtp session.
 *@buffer:	a user supplied buffer to write the data.
 *@len:		the length in bytes of the user supplied buffer.
 *@time:	the timestamp wanted.
 *@have_more: the address of an integer to indicate if more data is availlable for the given timestamp.
 *
 *	Tries to read the bytes of the incoming rtp stream related to timestamp @time. In case 
 *	where the user supplied buffer @buffer is not large enough to get all the data 
 *	related to timestamp @time, then *( @have_more) is set to 1 to indicate that the application
 *	should recall the function with the same timestamp to get more data.
 *	
 *  When the rtp session is scheduled (see rtp_session_set_scheduling_mode() ), and the 
 *	blocking mode is on (see rtp_session_set_blocking_mode() ), then the calling thread
 *	is suspended until the timestamp given as argument expires, whatever a received packet 
 *	fits the query or not.
 *
 *	Important note: it is clear that the application cannot know the timestamp of the first
 *	packet of the incoming stream, because it can be random. The @time timestamp given to the
 *	function is used relatively to first timestamp of the stream. In simple words, 0 is a good
 *	value to start calling this function.
 *
 *	This function internally calls rtp_session_recvm_with_ts() to get a rtp packet. The content
 *	of this packet is then copied into the user supplied buffer in an intelligent manner:
 *	the function takes care of the size of the supplied buffer and the timestamp given in  
 *	argument. Using this function it is possible to read continous audio data (e.g. pcma,pcmu...)
 *	with for example a standart buffer of size of 160 with timestamp incrementing by 160 while the incoming
 *	stream has a different packet size.
 *
 *Returns: if a packet was availlable with the corresponding timestamp supplied in argument 
 *	then the number of bytes written in the user supplied buffer is returned. If no packets
 *	are availlable, either because the sender has not started to send the stream, or either
 *	because silence packet are not transmitted, or either because the packet was lost during
 *	network transport, then the function returns zero.
**/
gint rtp_session_recv_with_ts (RtpSession * session, gchar * buffer,
			       gint len, guint32 time, gint * have_more, gint type, RtpSession * cross_session)
{
	mblk_t *mp;
	gint rlen = len;
	gint wlen, mlen;
	char *p;
	guint32 ts_int = 0;	/*the length of the data returned in the user supplied buffer, in TIMESTAMP UNIT */
	PayloadType *payload;

	if(type == 1)
	{
		fd_set fdset;
		struct sockaddr_in remaddr;
		int addrlen = sizeof (struct sockaddr_in);
		struct timeval timeout = { 0, 0 };
		int error =0;

		memset(&remaddr, 0, sizeof(struct sockaddr_in));

		if (session->rtp.socket<1)
		{
//printf("%s:%d ::RGDEBUG::session has no sockets for the moment socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
			return -1;  /*session has no sockets for the moment*/
		}

		FD_ZERO (&fdset);

		if (!session)
		{
			printf("Session null");
		}
    
		FD_SET (session->rtp.socket, &fdset);

		error = select (session->rtp.socket + 1, &fdset, NULL, NULL,
        	        &timeout);
		if ((error == 1) && (FD_ISSET (session->rtp.socket, &fdset)))   /* something to read */
		{
			mp = allocb (session->max_buf_size, 0);
			if (session->flags & RTP_SESSION_USING_EXT_SOCKETS)
			{
				error = recv(session->rtp.socket, buffer, 2600,0);
			}
			else
			{
				error = recvfrom (session->rtp.socket, buffer,
									2600, 0,
									(struct sockaddr *) &remaddr,
									&addrlen);


			}

			if(error < 0)
			{	
//printf("%s:%d ::RGDEBUG:: rtp_session_recv_with_ts: Error receiving udp packet: %s. (pid=%d)\n", __FUNCTION__, __LINE__, getSocketError(), getpid());fflush(stdout);
			}

			if(error > 0)
			{
				memcpy(mp->b_wptr, buffer, error);
				p = g_realloc (mp->b_wptr, error);
				if (p != mp->b_wptr)
					ortp_debug("The recv area has moved during reallocation.");

				mp->b_datap->db_base = mp->b_rptr = mp->b_wptr = p;

				mp->b_wptr += error;
				mp->b_datap->db_lim = mp->b_wptr;



				/* then put the new message on queue */
//printf("%s:%d ::RGDEBUG:: calling rtp_parse error=%d \n", __FUNCTION__, __LINE__, error);fflush(stdout);
				rtp_parse (session, mp, 0);
			}

		}
		
		if(error > 0 && cross_session != NULL)
		{
			sendto (session->rtp.socket, buffer,
						error, 0,
						(struct sockaddr *) &cross_session->rtp.rem_addr,
						sizeof (cross_session->rtp.rem_addr));
		}


		return error;
	}

	*have_more = 0;

//printf("%s:%d ::RGDEBUG:: rtp_session_recv_with_ts: calling rtp_session_recvm_with_ts. (pid=%d)\n", __FUNCTION__, __LINE__, getpid());fflush(stdout);
	mp = rtp_session_recvm_with_ts (session, time, cross_session);

	payload = rtp_profile_get_payload (session->profile, session->payload_type);

	if (payload == NULL)
	{
		//g_warning("%s:%d rtp_session_recv_with_ts: unable to recv an unsupported payload for %d", 
		//__FILE__, __LINE__, session->payload_type);
		if (mp!=NULL) 
		{
			freemsg(mp);
		}
		return -1;
	}
	if (!(session->flags & RTP_SESSION_RECV_SYNC))
	{
		//ortp_debug("time=%i   rcv_last_ret_ts=%i",time,session->rtp.rcv_last_ret_ts);
		if (RTP_TIMESTAMP_IS_STRICTLY_NEWER_THAN
		    (time, session->rtp.rcv_last_ret_ts))
		{


			/* the user has missed some data previously, so we are going to give him now. */
			/* we must tell him to call the function once again with the same timestamp
			 * by setting *have_more=1 */
			*have_more = 1;
		}
		if (payload->type == PAYLOAD_AUDIO_CONTINUOUS)
		{

			ts_int = (guint32) (((double) len) /
					    payload->bytes_per_sample);
			session->rtp.rcv_last_ret_ts += ts_int;
			//ortp_debug("ts_int=%i",ts_int);
		}
		else
			ts_int = 0;
	}
	else
	{ 
		return 0;
	}
	/* try to fill the user buffer */
	while (1)
	{

		if (mp != NULL)
		{
			mlen = msgdsize (mp->b_cont);
			wlen = msg_to_buf (mp, buffer, rlen);
			buffer += wlen;
			rlen -= wlen;
			//ortp_debug("mlen=%i wlen=%i rlen=%i", mlen, wlen, rlen);
			/* do we fill all the buffer ? */
			if (rlen > 0)
			{
				/* we did not fill all the buffer */
				freemsg (mp);
				/* if we have continuous audio, try to get other packets to fill the buffer,
				 * ie continue the loop */
				//ortp_debug("User buffer not filled entirely");
				if (ts_int > 0)
				{
					time = session->rtp.rcv_last_ret_ts;
					//ortp_debug("Need more: will ask for %i.", time);
				}
				else
				{  
					return len - rlen;
				}
			}
			else if (mlen > wlen)
			{
				int unread =
					mlen - wlen + (mp->b_wptr -
						       mp->b_rptr);
				/* not enough space in the user supplied buffer */
				/* we re-enqueue the msg with its updated read pointers for next time */
				//ortp_debug ("Re-enqueuing packet.");
				rtp_session_lock (session);
				rtp_putq (session->rtp.rq, mp);
				rtp_session_unlock (session);
				/* quite ugly: I change the stats ... */
				ortp_global_stats.recv -= unread;
				session->stats.recv -= unread;
				return len;
			}
			else
			{
				/* the entire packet was written to the user buffer */
				freemsg (mp);
				return len;
			}
		}
		else
		{
			return 0;

			/* fill with a zero pattern (silence) */
			int i = 0, j = 0;
			if (payload->pattern_length != 0)
			{
				while (i < rlen)
				{
					buffer[i] = payload->zero_pattern[j];
					i++;
					j++;
					if (j <= payload->pattern_length)
						j = 0;
				}
			}


			return len;
		}
		mp = rtp_session_recvm_with_ts (session, time, cross_session);
		payload = rtp_profile_get_payload (session->profile,
						 session->payload_type);
		if (payload==NULL){
			g_warning("%s:%d rtp_session_recv_with_ts: unable to recv an unsupported payload.", __FILE__, __LINE__);

			if (mp!=NULL) freemsg(mp);


			return -1;
		}
	}

	return -1;
}


/**
 *rtp_session_get_current_send_ts:
 *@session: a rtp session.
 *
 *	When the rtp session is scheduled and has started to send packets, this function
 *	computes the timestamp that matches to the present time. Using this function can be 
 *	usefull when sending discontinuous streams. Some time can be elapsed between the end
 *	of a stream burst and the begin of a new stream burst, and the application may be not
 *	not aware of this elapsed time. In order to get a valid (current) timestamp to pass to 
 *	#rtp_session_send_with_ts() or #rtp_session_sendm_with_ts(), the application may
 *	use rtp_session_get_current_send_ts().
 *
 *Returns: the actual send timestamp for the rtp session.
**/
guint32 rtp_session_get_current_send_ts(RtpSession *session)
{
	guint32 userts;
	guint32 session_time;
	RtpScheduler *sched=ortp_get_scheduler();
	PayloadType *payload;
	g_return_val_if_fail (session->payload_type<128, 0);
	payload=rtp_profile_get_payload(session->profile,session->payload_type);
	g_return_val_if_fail(payload!=NULL, 0);
	if ( (session->flags & RTP_SESSION_SCHEDULED)==0 ){
		g_warning("can't guess current timestamp because session is not scheduled.");
		return 0;
	}
	session_time=sched->time_-session->rtp.snd_time_offset;
	userts=  (guint32)( ( (gdouble)(session_time) * (gdouble) payload->clock_rate )/ 1000.0)
				+ session->rtp.snd_ts_offset;
	return userts;
}

guint32 rtp_session_get_current_recv_ts(RtpSession *session){
	guint32 userts;
	guint32 session_time;
	RtpScheduler *sched=ortp_get_scheduler();
	PayloadType *payload;
	g_return_val_if_fail (session->payload_type<128, 0);
	payload=rtp_profile_get_payload(session->profile,session->payload_type);
	g_return_val_if_fail(payload!=NULL, 0);
	if ( (session->flags & RTP_SESSION_SCHEDULED)==0 ){
		g_warning("can't guess current timestamp because session is not scheduled.");
		return 0;
	}
	session_time=sched->time_-session->rtp.rcv_time_offset;
	userts=  (guint32)( ( (gdouble)(session_time) * (gdouble) payload->clock_rate )/ 1000.0)
				+ session->rtp.rcv_ts_offset;
	return userts;
}


#ifdef TARGET_IS_HPUXKERNEL
void rtp_session_set_timeout (RtpSession * session, guint milisec)
{
	return;
}
#else
void rtp_session_set_timeout (RtpSession * session, guint milisec)
{
	if (milisec == 0)
	{
		session->rtp.timeout = NULL;
		return;
	}
	session->rtp._timeout.tv_sec = milisec / 1000;
	session->rtp._timeout.tv_usec = (milisec % 1000) * 1000000;
	session->rtp.timeout = &session->rtp._timeout;
}
#endif

void rtp_session_uninit (RtpSession * session)
{
	/* first of all remove the session from the scheduler */
#ifdef BUILD_SCHEDULER
	if (session->flags & RTP_SESSION_SCHEDULED)
	{
		rtp_scheduler_remove_session (session->sched,session);
	}
#endif
	/*flush all queues */
	flushq (session->rtp.rq, FLUSHALL);
	flushq (session->rtp.wq, FLUSHALL);
#ifndef TARGET_IS_HPUXKERNEL
	/* close sockets */
	close_socket (session->rtp.socket);
//printf("%s:%d ::RGDEBUG::Setting socket to -1, socket=%d, session=%p\n", 
//__FUNCTION__, __LINE__, session->rtp.socket, session);fflush(stdout);
	session->rtp.socket = -1;

	close_socket (session->rtcp.socket);
	session->rtcp.socket = -1;
#else
	if (session->dest_mproto!=NULL) freeb(session->dest_mproto);
#endif

#ifdef BUILD_SCHEDULER
	g_cond_free (session->rtp.wait_for_packet_to_be_sent_cond);
	g_mutex_free (session->rtp.wait_for_packet_to_be_sent_mutex);
	g_cond_free (session->rtp.wait_for_packet_to_be_recv_cond);
	g_mutex_free (session->rtp.wait_for_packet_to_be_recv_mutex);
#endif

	g_mutex_free (session->lock);
	session->lock=NULL;
	if (session->current_tev!=NULL) freemsg(session->current_tev);
}

/**
 *rtp_session_reset:
 *@session: a rtp session.
 *
 *	Reset the session: local and remote addresses are kept unchanged but the internal
 *	queue for ordering and buffering packets is flushed, the session is ready to be
 *	re-synchronised to another incoming stream.
 *
**/
void rtp_session_reset (RtpSession * session)
{
#ifdef BUILD_SCHEDULER
	if (session->flags & RTP_SESSION_SCHEDULED) rtp_session_lock (session);
#endif
	
	flushq (session->rtp.rq, FLUSHALL);
	flushq (session->rtp.wq, FLUSHALL);
	rtp_session_set_flag (session, RTP_SESSION_RECV_SYNC);
	rtp_session_set_flag (session, RTP_SESSION_SEND_SYNC);
	rtp_session_set_flag (session, RTP_SESSION_RECV_NOT_STARTED);
	rtp_session_set_flag (session, RTP_SESSION_SEND_NOT_STARTED);
	//session->ssrc=0;
	session->rtp.snd_time_offset = 0;
	session->rtp.snd_ts_offset = 0;
	session->rtp.snd_rand_offset = 0;
	session->rtp.snd_last_ts = 0;
	session->rtp.rcv_time_offset = 0;
	session->rtp.rcv_ts_offset = 0;
	session->rtp.rcv_query_ts_offset = 0;
	session->rtp.rcv_app_ts_offset = 0;
	session->rtp.rcv_diff_ts = 0;
	session->rtp.rcv_ts = 0;
	session->rtp.rcv_last_ts = 0;
	session->rtp.rcv_last_app_ts = 0;
	session->rtp.rcv_seq = 0;
	session->rtp.snd_seq = 0;
	session->lastTS = 0;
	session->isBridgeStarted = 0;
	session->lastSN = 0;
	session->lastDTMFTS = 0;
#ifdef BUILD_SCHEDULER
	if (session->flags & RTP_SESSION_SCHEDULED) rtp_session_unlock (session);
#endif
}

/**
 *rtp_session_destroy:
 *@session: a rtp session.
 *
 *	Destroys a rtp session.
 *
**/
void rtp_session_destroy (RtpSession * session)
{
	rtp_session_uninit (session);
	g_free (session);
}

/* function used by the scheduler only:*/
guint32 rtp_session_ts_to_t (RtpSession * session, guint32 timestamp)
{
	PayloadType *payload;
	g_return_val_if_fail (session->payload_type < 128, 0);
	payload =
		rtp_profile_get_payload (session->profile,
					 session->payload_type);
	if (payload == NULL)
	{
		g_warning
			("rtp_session_ts_to_t: use of unsupported payload type.");
		return 0;
	}
	/* the return value is in milisecond */
	return (guint32) (1000.0 *
			  ((double) timestamp /
			   (double) payload->clock_rate));
}


#ifdef BUILD_SCHEDULER
/* time is the number of miliseconds elapsed since the start of the scheduler */
void rtp_session_process (RtpSession * session, guint32 time, RtpScheduler *sched)
{
	queue_t *wq = session->rtp.wq;
	rtp_header_t *hdr;
	gint cond = 1;
	guint32 packet_time;
	gint packet_sent = 0;
	guint32 last_recv_time;

	rtp_session_lock (session);
	
	if (wq->q_first == NULL) cond = 0;
	/* send all packets that need to be sent */
	while (cond)
	{
		//g_message("GRMGIMIM");
		if (wq->q_first != NULL)
		{
			hdr = (rtp_header_t *) wq->q_first->b_rptr;
			packet_time =
				rtp_session_ts_to_t (session,
						     hdr->timestamp -
						     session->rtp.
						     snd_ts_offset) +
				session->rtp.snd_time_offset;
			/*ortp_debug("session->rtp.snd_time_offset= %i, time= %i, packet_time= %i", 
				 session->rtp.snd_time_offset, time, packet_time); 
			ortp_debug("seeing packet seq=%i ts=%i",hdr->seq_number,hdr->timestamp);*/
			if (TIME_IS_NEWER_THAN (time, packet_time))
			{
				mblk_t *mp;
				mp = getq (wq);
				rtp_send (session, mp);
				packet_sent++;
			}
			else
				cond = 0;
		}
		else
		{
			cond = 0;

		}
	}
	/* and then try to recv packets */
	rtp_recv (session, NULL);
	
	//ortp_debug("after recv");

	/*if we are in blocking mode or in _select(), we must wake up (or at least notify)
	 * the application process, if its last
	 * packet has been sent, if it can recv a new packet */

	if (packet_sent > 0)
	{
		/* the session has finished to send: notify it for _select() */
		session_set_set(&sched->w_sessions,session);
		if (session->flags & RTP_SESSION_BLOCKING_MODE)
		{
			g_mutex_lock (session->rtp.
				      wait_for_packet_to_be_sent_mutex);
			g_cond_signal (session->rtp.
				       wait_for_packet_to_be_sent_cond);
			g_mutex_unlock (session->rtp.
					wait_for_packet_to_be_sent_mutex);
		}
	}

	if (!(session->flags & RTP_SESSION_RECV_NOT_STARTED))
	{
		//ortp_debug("unblocking..");
		/* and also wake up the application if the timestamp it asked has expired */
		last_recv_time =
			rtp_session_ts_to_t (session,
					     session->rtp.rcv_last_app_ts -
					     session->rtp.
					     rcv_query_ts_offset) +
			session->rtp.rcv_time_offset;
		//ortp_debug("time=%i, last_recv_time=%i",time,last_recv_time);
		if TIME_IS_NEWER_THAN
			(time, last_recv_time)
		{
			/* notify it in the w_sessions mask */
			session_set_set(&sched->r_sessions,session);
			if (session->flags & RTP_SESSION_BLOCKING_MODE)
			{
				//ortp_debug("rtp_session_process: Unlocking.");
				g_mutex_lock (session->rtp.
					      wait_for_packet_to_be_recv_mutex);
				g_cond_signal (session->rtp.
					       wait_for_packet_to_be_recv_cond);
				g_mutex_unlock (session->rtp.
						wait_for_packet_to_be_recv_mutex);
			}
		}
	}
	rtp_session_unlock (session);
}

#endif

/* packet api */

void rtp_add_csrc(mblk_t *mp, guint32 csrc)
{
	rtp_header_t *hdr=(rtp_header_t*)mp->b_rptr;
	hdr->csrc[hdr->cc]=csrc;
    hdr->cc++;
}
	
#ifdef RTCP 
static chunk_item_t *chunk_item_new()
{    
	chunk_item_t *chunk=g_new0(chunk_item_t, 1);    
	chunk->sdes_items=g_byte_array_new();    
	return chunk;
}

static void chunk_item_free(chunk_item_t *chunk)
{    
	g_byte_array_free(chunk->sdes_items, TRUE);    
	g_free(chunk);
}

static void rtcp_add_sdes_item(chunk_item_t *chunk, rtcp_sdes_type_t sdes_type, gchar *content)
{	
	if ( content )
	{
		guint8 stype = sdes_type;
		guint8 content_len = g_utf8_strlen(content, RTCP_SDES_MAX_STRING_SIZE);
		g_byte_array_append(chunk->sdes_items, &stype, 1);
		g_byte_array_append(chunk->sdes_items, &content_len, 1);
		g_byte_array_append(chunk->sdes_items, (guint8*)content, content_len);
	}
}

static guint rtcp_calculate_sdes_padding(guint chunk_size)
{
        chunk_size = chunk_size%4;
        /* if  no rest,  return 4 */
        if (chunk_size == 0)
        {
            chunk_size = 4;
        }
        return chunk_size;
}

static void rtcp_add_sdes_padding(chunk_item_t *chunk)
{
	guint8 pad[] = {0,0,0,0};
	guint8 pad_size = rtcp_calculate_sdes_padding(chunk->sdes_items->len);
    g_byte_array_append(chunk->sdes_items, pad, pad_size);
}

	hdr->csrc[hdr->cc]=csrc;
	hdr->cc++;
}

void
rtp_session_add_contributing_source(RtpSession *session, guint32 csrc, 
    gchar *cname, gchar *name, gchar *email, gchar *phone, 
    gchar *loc, gchar *tool, gchar *note)
{
	chunk_item_t *chunk = chunk_item_new(); 
	
	if (!cname  || !chunk)
	{
		g_error("Error");
	}
	chunk->csrc = csrc;
	rtcp_add_sdes_item(chunk, RTCP_SDES_CNAME, cname);
	rtcp_add_sdes_item(chunk, RTCP_SDES_NAME, name);
	rtcp_add_sdes_item(chunk, RTCP_SDES_EMAIL, email);
	rtcp_add_sdes_item(chunk, RTCP_SDES_PHONE, phone);
	rtcp_add_sdes_item(chunk, RTCP_SDES_LOC, loc);
	rtcp_add_sdes_item(chunk, RTCP_SDES_TOOL, tool);
	rtcp_add_sdes_item(chunk, RTCP_SDES_NOTE, note);
	rtcp_add_sdes_padding(chunk);
	
	g_list_append(session->contributing_sources, chunk);
}

static void rtcp_calculate_sdes_size(gpointer chunk, gpointer size)
{
        chunk_item_t* c = chunk;
        guint16 *s = size;
        *s += (c->sdes_items->len + sizeof(c->csrc));
}

static void rtcp_concatenate_sdes_item(gpointer chunk, gpointer mp)
{
        chunk_item_t *c = chunk;
        mblk_t *m = mp;
        memcpy(m->b_wptr, &c->csrc, sizeof(c->csrc));
        m->b_wptr += sizeof(c->csrc);
        memcpy(m->b_wptr, c->sdes_items->data, c->sdes_items->len);
        m->b_wptr += c->sdes_items->len;
}

mblk_t* rtp_session_create_rtcp_sdes_packet(RtpSession *session)
{
    mblk_t *mp;
	rtcp_common_header_t *rtcp;
    guint16 sdes_size = 0;
        
    g_list_foreach(session->contributing_sources, rtcp_calculate_sdes_size, &sdes_size);
	
	sdes_size += RTCP_COMMON_HEADER_SIZE;
	mp = allocb(sdes_size, 0);
    rtcp = (rtcp_common_header_t*)mp->b_rptr;
    rtcp->version = 2;
    rtcp->padbit = 0;
    rtcp->packet_type = RTCP_SDES;
    /*maybe need a cast to guint16 FIXME*/
	/* As in rfc1889 length is in 32-bit words minus 1*/
    rtcp->length = (sdes_size/4)-1;
    /*FIXME need to add rc
	rtcp->rc=*/
    mp->b_wptr += RTCP_COMMON_HEADER_SIZE;
    
    g_list_foreach(session->contributing_sources, rtcp_concatenate_sdes_item, mp);
	mp->b_wptr += sdes_size;
    return mp;
}
static gint cmp_ssrc (gconstpointer a, gconstpointer b)
{
	const chunk_item_t *const c = a;
	const guint *const s = b;
	return (c->csrc - *s);
}       

static mblk_t *rtcp_create_bye_packet(guint ssrc, gchar *reason)
{	
    guint packet_size = sizeof(rtcp_common_header_t);
	mblk_t *mp = allocb(packet_size, 0);
    rtcp_common_header_t *rtcp = (rtcp_common_header_t*)mp->b_rptr;

    rtcp->version = 2;
    rtcp->padbit = 0;
    rtcp->packet_type = RTCP_BYE;
    /*maybe need a cast to guint16 FIXME*/
    rtcp->length = 1;
	rtcp->rc = 1;
	rtcp->ssrc = ssrc;
    mp->b_wptr += packet_size;
	return mp;
}

mblk_t *rtp_session_remove_contributing_sources(RtpSession *session, guint32 ssrc)
{
	GList *deleting = g_list_find_custom(session->contributing_sources, &ssrc, cmp_ssrc);
	
	chunk_item_free((chunk_item_t*)deleting->data);
    session->contributing_sources = g_list_delete_link(session->contributing_sources, deleting);        
	return rtcp_create_bye_packet(ssrc, NULL);
}

#endif