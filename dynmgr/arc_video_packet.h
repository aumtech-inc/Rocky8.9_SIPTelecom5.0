#ifndef ARC_VIDEO_PACKET_DOT_H
#define ARC_VIDEO_PACKET_DOT_H

/*----------------------------------------------------------------------------
File Name   :   arc_video_packet.h
Purpose     :   API definitions to access the srgs library..
Author      :   Aumtech, inc
_____________________________________________________________________________*/

#include<stdio.h>
#include<stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

//typedef unsigned int   UINT32;
typedef unsigned short      USHORT;
typedef unsigned char       BYTE;


	int arc_video_packet_init();
	int arc_video_packetise(void *, int l_iBytesLen, int time_stamp);

#ifdef __cplusplus
}
#endif


#endif




