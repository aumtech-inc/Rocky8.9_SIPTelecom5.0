  /*
  The oRTP LinPhone RTP library intends to provide basics for a RTP stack.
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
#include <signal.h>
#include <stdlib.h>

#ifndef _WIN32 
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#endif

int runcond=1;

void stophandler(int signum)
{
	runcond=0;
}

static char *help="usage: rtpsend	filename dest_ip4addr dest_port\n";

int main(int argc, char *argv[])
{
	RtpSession *session;
	char buffer[160];
	int i;
	FILE *infile;
	char *ssrc;
	guint32 user_ts=0;
	int totalCount = 0;
	int sentBytes = 0;
		
	if (argc<4){
		printf(help);
		return -1;
	}
	
	ortp_init();
	//ortp_scheduler_init();
	//ortp_set_debug_file("oRTP",stdout);
	session=rtp_session_new(RTP_SESSION_SENDONLY);	
	
	//rtp_session_set_scheduling_mode(session,1);
	rtp_session_set_blocking_mode(session, 0);
	rtp_session_set_ssrc (session, atoi("3197096732"));
	rtp_session_set_profile (session, &av_profile);
	rtp_session_set_remote_addr(session,argv[2],atoi(argv[3]), NULL);
	rtp_session_set_payload_type(session,0);
	rtp_profile_set_payload(&av_profile, 96, &telephone_event);
	
	ssrc=getenv("SSRC");
	if (ssrc!=NULL) {
		printf("using SSRC=%i.\n",atoi(ssrc));
		rtp_session_set_ssrc(session,atoi(ssrc));
	}
		
	#ifndef _WIN32
	infile=fopen(argv[1],"r");
	#else
	infile=fopen(argv[1],"rb");
	#endif

	if (infile==NULL) {
		perror("Cannot open file");
		return -1;
	}

	signal(SIGINT,	stophandler);
	signal(SIGTERM,	stophandler);

	while( ((i=fread(buffer, 1, 160, infile))>0) && (runcond) )
	{
		totalCount+=i;
		sentBytes = rtp_session_send_with_ts(session, buffer, i, user_ts, 0, 0);
		printf("Sent bytes %d\n.", sentBytes);
		user_ts+=160;
		usleep(20 * 1000);
	}

	printf("Finished sending %d bytes\n", totalCount);

	fclose(infile);
	printf("Finished closing file.\n");
	rtp_session_destroy(session);
	printf("Finished destroying session.\n");
	//ortp_exit();
	//printf("Finished exiting ortp.\n");

	//ortp_global_stats_display();

	exit(0);
}
