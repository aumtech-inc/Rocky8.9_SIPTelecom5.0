 /*
  The lprtplib LinPhone RTP library intends to provide basics for a RTP stack.
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
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#endif

typedef unsigned char       BYTE;
gint cond=1;

void stop_handler(int signum)
{
	cond=0;
}

void ssrc_cb(RtpSession *session)
{
	printf("Hey, the ssrc has changed! Calling rtp_session_resync()\n");
//	rtp_session_resync(session);
}

static char *help="usage: rtprecv  filename loc_port [--format format] [--soundcard]\n";

#define MULAW 0
#define ALAW 1

#if HOST_IS_HPUX && HAVE_SYS_AUDIO_H

#include <sys/audio.h>

gint sound_init(gint format)
{
	int fd;
	fd=open("/dev/audio",O_WRONLY);
	if (fd<0){
		perror("Can't open /dev/audio");
		return -1;
	}
	ioctl(fd,AUDIO_RESET,0);
	ioctl(fd,AUDIO_SET_SAMPLE_RATE,8000);
	ioctl(fd,AUDIO_SET_CHANNELS,1);
	if (format==MULAW)
		ioctl(fd,AUDIO_SET_DATA_FORMAT,AUDIO_FORMAT_ULAW);
	else ioctl(fd,AUDIO_SET_DATA_FORMAT,AUDIO_FORMAT_ALAW);
	return fd;	
}
#else
gint sound_init(gint format)
{
	return -1;
}
#endif

int isIFrame;


int checkForIFrame(const void *p_data)
{
   const unsigned char * l_pData = (const unsigned char *)p_data;
   if(isIFrame == 0 )
   {

      // Assume no RTP header extensions and skip over it
      l_pData += 12;

      // Skip over the RFC2190 header and detect i-frame flags
      switch (l_pData[0]&0xC0)
      {
         case 0xc0:  // mode c
			isIFrame = (l_pData[4] & 0x80) == 0;
			l_pData += 12;
            break;
         case 0x80:  // mode b
			isIFrame = (l_pData[4] & 0x80) == 0;
			l_pData += 8;
            break;
         default:    // mode a
			isIFrame = ((l_pData[0] & 0x40) == 0) && ((l_pData[1] & 0x10) == 0);
			l_pData += 4;
            break;
      }

	//if the RFC 2190 header says no i-frame, then check no further
      if (isIFrame == 0)
      {
         return 0;
      }

      // Now look for the H.263 START PICTURE code and I-Frame  bit pattern:
      // 00000000  00000000  100000xx  xxxxxxxx  00001000
      if (l_pData[0] != 0 || l_pData[1] != 0 || (l_pData[2]&0xfc) != 0x80 || l_pData[4] != 8)
      {
         isIFrame = 0;
         return 0;
      }

// Met of the criteria here for it being the first RTP packet of an I-Frame, start recording!

   }

	isIFrame = 1;
	return 1;
}


int main(int argc, char*argv[])
{
	RtpSession *session;
	char buffer[4096];
	int err;
	guint32 ts=0;
	gint stream_received=0;
	FILE *outfile;
	gint local_port = 5060;
	gint have_more;
	gint i;
	gint format=0;
	gint soundcard=0;
	gint sound_fd=0;
	int totallen = 0;
    char address[100] = "0.0.0.0";

	isIFrame = 0;
	
	/* init the lib */
	if (argc<3){
		printf(help);
		return -1;
	}

	local_port=atoi(argv[argc - 1]);
	if (local_port<=0) {
		printf(help);
		printf("\n\n%d::Wrong RTP Port %d\n", __LINE__, local_port);
		return -1;
	}

	for (i=0;i<argc;i++)
	{
		if (strcmp(argv[i],"--format")==0){
			i++;
			if (i<argc){
				if (strcmp(argv[i],"mulaw")==0){
					format=MULAW;
				}else
				if (strcmp(argv[i],"alaw")==0){
					format=ALAW;
				}else{
					printf("Unsupported format %s\n",argv[i]);
					return -1;
				}
			}
        }
        else if (strcmp(argv[i], "--address") == 0){
            i++;
            snprintf(address, sizeof(address), "%s", argv[i]);
            fprintf(stderr, " %s() address=%s\n", __func__, address);
        }
		else if (strcmp(argv[i],"--soundcard")==0){
			soundcard=1;
		}
	}
	
	outfile=fopen(argv[1], "wb");

	if (outfile==NULL) {
		perror("Cannot open file for writing");
		return -1;
	}
	
	
	if (soundcard){
		sound_fd=sound_init(format);
	}
	
	ortp_init();
	ortp_scheduler_init();
	
	signal(SIGINT,	stop_handler);
	signal(SIGTERM,	stop_handler);
	ortp_set_debug_file("oRTP",stderr);
	session=rtp_session_new(RTP_SESSION_RECVONLY);	
	rtp_session_set_scheduling_mode(session,1);
	rtp_session_set_blocking_mode(session,1);
	// rtp_session_set_local_addr(session,"0.0.0.0",atoi(argv[2]));
	rtp_session_set_local_addr(session,address,atoi(argv[2]), NULL);
	rtp_session_set_payload_type(session,0);
	rtp_session_signal_connect(session,"ssrc_changed",(RtpCallback)ssrc_cb,NULL);
	rtp_session_signal_connect(session,"ssrc_changed",(RtpCallback)rtp_session_reset,NULL);
	
	while(cond)
	{
		have_more=1;
		while (have_more)
		{
			memset(buffer, 0 , 4096);
			err = rtp_session_recv_with_ts(session, buffer, 160, ts, &have_more, 0, NULL);
			/* this is to avoid to write to disk some silence before the first RTP packet is returned*/	
			if ((err>0) && (buffer != NULL))
			{
				int rc = checkForIFrame(buffer);
				printf("RGDEBUG:: err=%d\n",err);fflush(stdout);
				if(rc > 0)
				{	
					int write_len;
					int write_len1;
					BYTE l_caLength[2];
					l_caLength[0] = (BYTE)(err >> 8);
					l_caLength[1] = (BYTE) err;
			
					write_len = fwrite(l_caLength, 1, 2, outfile);
					printf("write_len=%d\n",write_len);fflush(stdout);
					totallen += write_len;	

					write_len1 = fwrite(buffer, 1, err, outfile);
					printf("write_len1=%d\n",write_len1);fflush(stdout);
					totallen += write_len1;	
					printf("total=%d \n",totallen);fflush(stdout);
					if (sound_fd>0) 
					{
						write(sound_fd,buffer,err);
					}
				}
			}
		}
		ts+=err;
		g_message("Receiving packet.");
	}
	
	rtp_session_destroy(session);
	ortp_exit();
	
	ortp_global_stats_display();
	
	return 0;
}
