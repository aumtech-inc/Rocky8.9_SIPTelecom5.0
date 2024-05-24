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


#ifndef RTCP_H
#define RTCP_H

#include "rtpport.h"


#ifdef RTCP

#define RTCP_COMMON_HEADER_SIZE 4
#define RTCP_SENDER_INFO_SIZE 20
#define RTCP_REPORT_BLOCK_SIZE 24
#define RTCP_SDES_MAX_STRING_SIZE 255
#define RTCP_SDES_ITEM_HEADER_SIZE 2


 
typedef enum {
    RTCP_SR	= 200,
    RTCP_RR	= 201,
    RTCP_SDES	= 202,
    RTCP_BYE	= 203,
    RTCP_APP	= 204
} rtcp_type_t;
 
typedef enum {
    RTCP_SDES_END		= 0,
    RTCP_SDES_CNAME 	= 1,
    RTCP_SDES_NAME	= 2,
    RTCP_SDES_EMAIL	= 3,
    RTCP_SDES_PHONE	= 4,
    RTCP_SDES_LOC		= 5,
    RTCP_SDES_TOOL	= 6,
    RTCP_SDES_NOTE	= 7,
    RTCP_SDES_PRIV		= 8,
    RTCP_SDES_MAX		= 9
} rtcp_sdes_type_t;
 
typedef struct rtcp_common_header
{
#ifdef WORDS_BIGENDIAN
        guint16 version:2;
        guint16 padbit:1;
        guint16 rc:5;
        guint16 packet_type:8;
#else
        guint16 rc:5;
        guint16 padbit:1;
        guint16 version:2;
		guint16 packet_type:8;
#endif
        guint16 length:16;
		guint32 ssrc;
} rtcp_common_header_t;
 
 
typedef struct sender_info
{
        guint32 ntp_timestamp_msw;
        guint32 ntp_timestamp_lsw;
        guint32 rtp_timestamp;
        guint32 senders_packet_count;
        guint32 senders_octet_count;
} sender_info_t;
 
typedef struct report_block
{
        guint32 ssrc;
        guint32 fraction_lost:8;
        guint32 cum_num_packet_lost:24; /*cumulative number of packet lost*/
        guint32 ext_high_seq_num_rec; /*extended highest sequence number received */
        guint32 interarrival_jitter;
        guint32 lsr; /*last SR */
        guint32 delay_snc_last_sr; /*delay since last sr*/
} report_block_t;

typedef struct chunk_item
{
	guint32 csrc;
	GByteArray *sdes_items;
} chunk_item_t;

typedef struct sdes_item
{
	guint8 item_type;
    guint8 len;
	gchar content[1];	
} sdes_item_t;
 
 
typedef struct rtcp_bye_reason
{
    guint8 len;
	gchar content[1];
} rtcp_bye_t;
 

#endif

#endif
