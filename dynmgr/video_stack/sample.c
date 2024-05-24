#include <iostream>
#include <stdio.h>
#include "arc_video_packet.h"



//#include "ortp.h"
//#include "rtpsession.h"
//#include "payloadtype.h"


using namespace std;

#define VIDEO_PACKET_BUFFER_SIZE     5000

int main()
{
//	RtpSession *session;
	FILE *infile;
	char *ssrc;
//	guint32 user_ts=0;
	char *buffer;

//	ortp_init();
//	ortp_scheduler_init();
	//ortp_set_debug_file("oRTP",stdout);
//	session=rtp_session_new(RTP_SESSION_SENDONLY);	
	
//	rtp_session_set_scheduling_mode(session,1);
//	rtp_session_set_blocking_mode(session,1);
//	rtp_session_set_remote_addr(session,"10.0.0.69",10000);
//	rtp_session_set_payload_type(session,0);
	
	ssrc=getenv("SSRC");
	if (ssrc!=NULL) {
		printf("using SSRC=%i.\n",atoi(ssrc));
//		rtp_session_set_ssrc(session,atoi(ssrc));
	}


	unsigned char header[2];
	int i;

	char filename[] = "AddressingMenu-48br.glv";
	infile = fopen(filename, "rb");
	if(infile == NULL)
	{
		//cout<<"infile=NULL exiting out"<<endl;
		return -1;
	}


	long l_iBytesRead;
	memset(header, 0, 2);
	
	
	
	int rc = arc_video_packet_init();
	if(rc != 0)
	{
		return -1;
	}
	
	
	
#if 1	
	while( ((i=fread(header,1,2,infile))>0))
	{

		int l_iBytesRead = 0;
		int tempNumOfBytes = 0;
						
		
	printf("----------------------------------------------------------\n");fflush(stdout);
		if(i != 2)
		{
			fclose(infile);
 			break;
		}
		else
		{
			l_iBytesRead = (header[0] << 8) | header[1];
			printf("%ld\n", l_iBytesRead);fflush(stdout);
			if (l_iBytesRead < 12 || l_iBytesRead > 5000)  // RTP header is at least 12 bytes
			{
				//cout<<"RGDEBUG:: no rtp header"<<endl;
				fclose(infile);
				break;
			}

			buffer = (char *)malloc(l_iBytesRead);
			memset(buffer, 0, l_iBytesRead);
			if ((tempNumOfBytes = fread(buffer, 1, l_iBytesRead,infile)) != l_iBytesRead)
			{
				//cout<<"RGDEBUG::can't read l_iBytesRead="<<l_iBytesRead<<endl;
				fclose(infile);
				break;
			}

			const BYTE *l_ptr = (const BYTE *)buffer;
			if((l_ptr[1] & 0x80) != 0)
			{
				//cout<<"RGDEBUG marker bit not found"<<endl;
				fclose(infile);
				break;
				
			}

		}


#if 1
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/original_packet_%d", i);
	printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if( l_iBytesRead== 1399)
	{
		printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite(buffer , 1, l_iBytesRead, fp);
		fclose(fp);
	}
}
#endif



		//cout<<"RGDEBUG::calling arc_video_packetise from main"<<endl;
		arc_video_packetise(buffer, l_iBytesRead, 0 );	
		//cout<<"RGDEBUG::calling rtp_session_send_with_ts from main"<<endl;

		

#if 1
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/packet_%d", i);
	printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(l_iBytesRead == 1399)
	{
		printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite(buffer, 1, l_iBytesRead, fp);
		fclose(fp);
	}
}
#endif
		
//		rtp_session_send_with_ts(session, buffer, l_iBytesRead, user_ts);	

	}


#endif

#if 0

	l_iBytesRead = 1399;
	buffer = (char *)malloc(1400);
	fread(buffer, 1, 1399,infile);


#if 1
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/original_packet_%d", i);
	printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if( l_iBytesRead== 1399)
	{
		printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite(buffer , 1, l_iBytesRead, fp);
		fclose(fp);
	}
}
#endif
	
	arc_video_packetise(buffer, 1399, 0 );


#if 1
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/packet_%d", i);
	printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if( l_iBytesRead== 1399)
	{
		printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite(buffer , 1, l_iBytesRead, fp);
		fclose(fp);
	}
}
#endif

#endif


	fclose(infile);
	return 0;

}


