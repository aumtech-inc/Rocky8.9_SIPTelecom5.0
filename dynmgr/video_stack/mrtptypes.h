/***************************************************************************
        File:           $Archive: /stacks/include/mrtptypes.h $
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Albert Wong
        Synopsys:       RTP data types and structure definition
        Copyright 1997-2003 by Dilithium Networks.
The material in this file is subject to copyright. It may not
be used, copied or transferred by any means without the prior written
approval of Dilithium Networks.
DILITHIUM NETWORKS DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL DILITHIUM NETWORKS BE LIABLE FOR
ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
****************************************************************************/
#ifndef __MRTPTYPES_H__
#define __MRTPTYPES_H__

#include "mbasictypes.h"

#if ! defined(_LITTLE_ENDIAN)
#define _LITTLE_ENDIAN  1
#endif


#define RTP_VERSION                     2
#define RTP_PCMU_PAYLOAD_TYPE           0
#define RTP_PCMA_PAYLOAD_TYPE           8
#define RTP_G722_PAYLOAD_TYPE           9
#define RTP_G723_PAYLOAD_TYPE           4
#define RTP_G728_PAYLOAD_TYPE           15
#define RTP_G729_PAYLOAD_TYPE           18
#define RTP_GSMAMR_PAYLOAD_TYPE         96
#define RTP_H261_PAYLOAD_TYPE           31
#define RTP_H263_PAYLOAD_TYPE           34
#define RTP_MP4VIDEO_PAYLOAD_TYPE       98
#define RTP_H263PLUS_PAYLOAD_TYPE       104

#define RTP_G711_HEADER_SIZE            0
#define RTP_G723_HEADER_SIZE            0
#define RTP_GSMAMR_HEADER_SIZE          0
#define RTP_H261_HEADER_SIZE            4
#define RTP_H261A_HEADER_SIZE           8       // payload type is dynamic
#define RTP_H263_MODEA_HEADER_SIZE      4
#define RTP_H263_MODEB_HEADER_SIZE      8
#define RTP_H263_MODEC_HEADER_SIZE      12
#define RTP_MP4VIDEO_HEADER_SIZE        0

#define RTP_SEQ_MOD             (1<<16)
#define RTP_MAX_SDES            255         // maximum text length for SDES
#define RTP_PACKET_SN_LENGTH    0x10000     // maximum rtp packet serial number + 1
#define RTP_TIMESTAMP_LENGTH    0x100000000

// RTCP payload type constants were added
#define RTCP_SR_PAYLOAD_TYPE    200     // sender report
#define RTCP_RR_PAYLOAD_TYPE    201     // reception report
#define RTCP_SDES_PAYLOAD_TYPE  202     // source description
#define RTCP_BYE_PAYLOAD_TYPE   203     // goodbye
#define RTCP_APP_PAYLOAD_TYPE   204     // application-defined

// RTCP SDES item constants were added
#define RTCP_SDES_END           0
#define RTCP_SDES_CNAME         1
#define RTCP_SDES_NAME          2
#define RTCP_SDES_EMAIL         3
#define RTCP_SDES_PHONE         4
#define RTCP_SDES_LOC           5
#define RTCP_SDES_TOOL          6
#define RTCP_SDES_NOTE          7
#define RTCP_SDES_PRIV          8


/*
 * The type definitions below are valid for 32-bit architectures and
 * may have to be adjusted for 16- or 64-bit architectures.
 */
typedef unsigned char  UINT8;
typedef unsigned short UINT16;
typedef unsigned int   UINT32;


typedef enum
{
    MRTCP_SR   = 200,
    MRTCP_RR   = 201,
    MRTCP_SDES = 202,
    MRTCP_BYE  = 203,
    MRTCP_APP  = 204
} rtcp_type_t;

typedef enum
{
    MRTCP_SDES_END   = 0,
    MRTCP_SDES_CNAME = 1,
    MRTCP_SDES_NAME  = 2,
    MRTCP_SDES_EMAIL = 3,
    MRTCP_SDES_PHONE = 4,
    MRTCP_SDES_LOC   = 5,
    MRTCP_SDES_TOOL  = 6,
    MRTCP_SDES_NOTE  = 7,
    MRTCP_SDES_PRIV  = 8
} rtcp_sdes_type_t;

/*
 * RTP data header
 */
typedef struct
{
#ifdef _BIG_ENDIAN
    UINT32 version:2;   /* protocol version */
    UINT32 p:1;         /* padding flag */
    UINT32 x:1;         /* header extension flag */
    UINT32 cc:4;        /* CSRC count */
    UINT32 m:1;         /* marker bit */
    UINT32 pt:7;        /* payload type */
    UINT32 seq:16;      /* sequence number */
#elif defined(_LITTLE_ENDIAN)
    UINT32 seq:16;      /* sequence number */
    UINT32 pt:7;        /* payload type */
    UINT32 m:1;         /* marker bit */
    UINT32 cc:4;        /* CSRC count */
    UINT32 x:1;         /* header extension flag */
    UINT32 p:1;         /* padding flag */
    UINT32 version:2;   /* protocol version */
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
    UINT32 ts;               /* timestamp */
    UINT32 ssrc;             /* synchronization source */
    UINT32 csrc[1];          /* optional CSRC list */
} MRTPHEADER;

/*
 * RTP payload header for G.723
 */
typedef struct
{
} MRTPG723PAYLOADHEADER;

/*
 * RTP payload header for GSM-AMR
 */
typedef struct
{
} MRTPGSMAMRPAYLOADHEADER;

/*
 * RTP payload header for H.261
 */
typedef struct
{
#ifdef _BIG_ENDIAN
    UINT32 sbit:3;         // start bit position
    UINT32 ebit:3;         // end bit position
    UINT32 i:1;            // INTRA-frame encoded data flag
    UINT32 v:1;            // motion vector flag
    UINT32 gobn:4;         // GOB number at the start of the packet
    UINT32 mbap:5;         // macroblock address predictor in previous packet
    UINT32 quant:5;        // quantization value for the first MB in the packet
    UINT32 hmvd:5;         // reference horizontal motion vector data for 1st MB in the packet
    UINT32 vmvd:5;         // reference vertical motion vector data for 1st MB in the packet

    // Following fields are for H.261A video streams only (see H.323 Annex D (11.4.1))
    UINT32 lgobn:4;        // last GOB number
    UINT32 r:4;            // reserved
    UINT32 bytecount:24;   // cumulative number of octets having been sent
#elif defined(_LITTLE_ENDIAN)
    UINT32 vmvd:5;         // reference vertical motion vector data for 1st MB in the packet
    UINT32 hmvd:5;         // reference horizontal motion vector data for 1st MB in the packet
    UINT32 quant:5;        // quantization value for the first MB in the packet
    UINT32 mbap:5;         // macroblock address predictor in previous packet
    UINT32 gobn:4;         // GOB number at the start of the packet
    UINT32 v:1;            // motion vector flag
    UINT32 i:1;            // INTRA-frame encoded data flag
    UINT32 ebit:3;         // end bit position
    UINT32 sbit:3;         // start bit position

    // Following fields are for H.261A video streams only (see H.323 Annex D (11.4.1))
    UINT32 bytecount:24;   // cumulative number of octets having been sent
    UINT32 r:4;            // reserved
    UINT32 lgobn:4;        // last GOB number
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
} MRTPH261PAYLOADHEADER;

/*
 * RTP payload header for H.263
 */
typedef struct
{

    union
    {
        struct
        {
#ifdef _BIG_ENDIAN
            UINT32 f:1;            // payload mode flag
            UINT32 p:1;            // PB-frames mode flag
            UINT32 sbit:3;         // start bit position
            UINT32 ebit:3;         // end bit position
            UINT32 src:3;          // source picture format
            UINT32 i:1;            // picture coding type flag
            UINT32 u:1;            // unrestricted motion vector option
            UINT32 s:1;            // syntax-based arithmetic coding option
            UINT32 a:1;            // advanced prediction option
            UINT32 r:4;            // reserved
            UINT32 dbq:2;          // differential quantization parameter
            UINT32 trb:3;          // temporal reference for the B frame
            UINT32 tr:8;           // temporal reference for the P frame
#elif defined(_LITTLE_ENDIAN)
            UINT32 tr:8;           // temporal reference for the P frame
            UINT32 trb:3;          // temporal reference for the B frame
            UINT32 dbq:2;          // differential quantization parameter
            UINT32 r:4;            // reserved
            UINT32 a:1;            // advanced prediction option
            UINT32 s:1;            // syntax-based arithmetic coding option
            UINT32 u:1;            // unrestricted motion vector option
            UINT32 i:1;            // picture coding type flag
            UINT32 src:3;          // source picture format
            UINT32 ebit:3;         // end bit position
            UINT32 sbit:3;         // start bit position
            UINT32 p:1;            // PB-frames mode flag
            UINT32 f:1;            // payload mode flag
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
        } modeA;
        struct
        {
#ifdef _BIG_ENDIAN
            UINT32 f:1;            // payload mode flag
            UINT32 p:1;            // PB-frames mode flag
            UINT32 sbit:3;         // start bit position
            UINT32 ebit:3;         // end bit position
            UINT32 src:3;          // source picture format
            UINT32 quant:5;        // quantization value for the first MB in the packet
            UINT32 gobn:5;         // GOB number at the start of the packet
            UINT32 mba:9;          // address of first MB within the GOB in the packet
            UINT32 r:2;            // reserved

            UINT32 i:1;            // picture coding type flag
            UINT32 u:1;            // unrestricted motion vector option
            UINT32 s:1;            // syntax-based arithmetic coding option
            UINT32 a:1;            // advanced prediction option
            UINT32 hmv1:7;         // horizontal motion vector predictor for 1st MB in the packet
            UINT32 vmv1:7;         // vertical motion vector predictor for 1st MB in the packet
            UINT32 hmv2:7;         // horizontal motion vector predictor for block 3 in 1st MB in the packet when 4 motion vectors are used
            UINT32 vmv2:7;         // vertical motion vector predictor for block 3 in 1st MB in the packet when 4 motion vectors are used
#elif defined(_LITTLE_ENDIAN)
            UINT32 r:2;            // reserved
            UINT32 mba:9;          // address of first MB within the GOB in the packet
            UINT32 gobn:5;         // GOB number at the start of the packet
            UINT32 quant:5;        // quantization value for the first MB in the packet
            UINT32 src:3;          // source picture format
            UINT32 ebit:3;         // end bit position
            UINT32 sbit:3;         // start bit position
            UINT32 p:1;            // PB-frames mode flag
            UINT32 f:1;            // payload mode flag

            UINT32 vmv2:7;         // vertical motion vector predictor for block 3 in 1st MB in the packet when 4 motion vectors are used
            UINT32 hmv2:7;         // horizontal motion vector predictor for block 3 in 1st MB in the packet when 4 motion vectors are used
            UINT32 vmv1:7;         // vertical motion vector predictor for 1st MB in the packet
            UINT32 hmv1:7;         // horizontal motion vector predictor for 1st MB in the packet
            UINT32 a:1;            // advanced prediction option
            UINT32 s:1;            // syntax-based arithmetic coding option
            UINT32 u:1;            // unrestricted motion vector option
            UINT32 i:1;            // picture coding type flag
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
        } modeB;
        struct
        {
#ifdef _BIG_ENDIAN
            UINT32 f:1;            // payload mode flag
            UINT32 p:1;            // PB-frames mode flag
            UINT32 sbit:3;         // start bit position
            UINT32 ebit:3;         // end bit position
            UINT32 src:3;          // source picture format
            UINT32 quant:5;        // quantization value for the first MB in the packet
            UINT32 gobn:5;         // GOB number at the start of the packet
            UINT32 mba:9;          // address of first MB within the GOB in the packet
            UINT32 r:2;            // reserved

            UINT32 i:1;            // picture coding type flag
            UINT32 u:1;            // unrestricted motion vector option
            UINT32 s:1;            // syntax-based arithmetic coding option
            UINT32 a:1;            // advanced prediction option
            UINT32 hmv1:7;         // horizontal motion vector predictor for 1st MB in the packet
            UINT32 vmv1:7;         // vertical motion vector predictor for 1st MB in the packet
            UINT32 hmv2:7;         // horizontal motion vector predictor for block 3 in 1st MB in the packet when 4 motion vectors are used
            UINT32 vmv2:7;         // vertical motion vector predictor for block 3 in 1st MB in the packet when 4 motion vectors are used

            UINT32 rr:19;          // reserved
            UINT32 dbq:2;          // differential quantization parameter
            UINT32 trb:3;          // temporal reference for the B frame
            UINT32 tr:8;           // temporal reference for the P frame
#elif defined(_LITTLE_ENDIAN)
            UINT32 r:2;            // reserved
            UINT32 mba:9;          // address of first MB within the GOB in the packet
            UINT32 gobn:5;         // GOB number at the start of the packet
            UINT32 quant:5;        // quantization value for the first MB in the packet
            UINT32 src:3;          // source picture format
            UINT32 ebit:3;         // end bit position
            UINT32 sbit:3;         // start bit position
            UINT32 p:1;            // PB-frames mode flag
            UINT32 f:1;            // payload mode flag

            UINT32 vmv2:7;         // vertical motion vector predictor for block 3 in 1st MB in the packet when 4 motion vectors are used
            UINT32 hmv2:7;         // horizontal motion vector predictor for block 3 in 1st MB in the packet when 4 motion vectors are used
            UINT32 vmv1:7;         // vertical motion vector predictor for 1st MB in the packet
            UINT32 hmv1:7;         // horizontal motion vector predictor for 1st MB in the packet
            UINT32 a:1;            // advanced prediction option
            UINT32 s:1;            // syntax-based arithmetic coding option
            UINT32 u:1;            // unrestricted motion vector option
            UINT32 i:1;            // picture coding type flag

            UINT32 tr:8;           // temporal reference for the P frame
            UINT32 trb:3;          // temporal reference for the B frame
            UINT32 dbq:2;          // differential quantization parameter
            UINT32 rr:19;          // reserved
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
        } modeC;
    };
} MRTPH263PAYLOADHEADER;

/*
 * RTP payload header for MPEG4-Video
 */
typedef struct
{
} MRTPMP4VIDEOPAYLOADHEADER;

// RTP Payload Header
typedef struct
{
    union
    {
        MRTPG723PAYLOADHEADER       g723;
        MRTPGSMAMRPAYLOADHEADER     gsmamr;
        MRTPH261PAYLOADHEADER       h261;
        MRTPH263PAYLOADHEADER       h263;
        MRTPMP4VIDEOPAYLOADHEADER   mp4video;
    };
} MRTPPAYLOADHEADER;

/*
 * RTCP common header word
 */
typedef struct
{
#ifdef _BIG_ENDIAN
    UINT32 version:2;   /* protocol version */
    UINT32 p:1;         /* padding flag */
    UINT32 count:5;     /* varies by packet type */
    UINT32 pt:8;        /* RTCP packet type */
    UINT32 length:16;   /* pkt len in words, w/o this word */
#elif defined(_LITTLE_ENDIAN)
    UINT32 length:16;   /* pkt len in words, w/o this word */
    UINT32 pt:8;        /* RTCP packet type */
    UINT32 count:5;     /* varies by packet type */
    UINT32 p:1;         /* padding flag */
    UINT32 version:2;   /* protocol version */
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
} MRTCPCOMMONHEADER;

/*
 * Big-endian mask for version, padding bit and packet type pair
 */
#define RTCP_VALID_MASK (0xc000 | 0x2000 | 0xfe)
#define RTCP_VALID_VALUE ((RTP_VERSION << 14) | RTCP_SR)

/*
 * Reception report block
 */
typedef struct
{
    UINT32 ssrc;             /* data source being reported */
#ifdef _BIG_ENDIAN
    UINT32 fraction:8;  /* fraction lost since last SR/RR */
    UINT32 lost:24;              /* cumul. no. pkts lost (signed!) */
#elif defined(_LITTLE_ENDIAN)
    UINT32 lost:24;              /* cumul. no. pkts lost (signed!) */
    UINT32 fraction:8;  /* fraction lost since last SR/RR */
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
    UINT32 last_seq;         /* extended last seq. no. received */
    UINT32 jitter;           /* interarrival jitter */
    UINT32 lsr;              /* last SR packet from this source */
    UINT32 dlsr;             /* delay since last SR packet */
} MRTCPREPORTBLOCK;

/*
 * SDES item
 */
typedef struct
{
    UINT8               type;               /* type of item (rtcp_sdes_type_t) */
    UINT8               length;             /* length of item (in octets) */
    char                data[256];          /* text, not null-terminated */
} MRTCPSDESITEM;
typedef struct
{
    UINT32          src;       /* SSRC/CSRC_n */
    MRTCPSDESITEM   item[3];   /* SDES items */
} MRTCPSDESCHUNK;

typedef struct rtcp_sdes rtcp_sdes_t;

/* sender report (SR) */
typedef struct MRTCPSENDERREPORT
{
    MRTCPCOMMONHEADER   header;     /* common header */
    UINT32              ssrc;     /* sender generating this report */
    UINT32              ntp_sec;  /* NTP timestamp */
    UINT32              ntp_frac;
    UINT32              rtp_ts;   /* RTP timestamp */
    UINT32              psent;    /* packets sent */
    UINT32              osent;    /* octets sent */
    MRTCPREPORTBLOCK    rr[1];  /* variable-length list */
} MRTCPSENDERREPORT;

/* reception report (RR) */
typedef struct MRTCPRECEIVERREPORT
{
    MRTCPCOMMONHEADER   header;     /* common header */
    UINT32              ssrc;       /* receiver generating this report */
    MRTCPREPORTBLOCK    rr[1];      /* variable-length list */
} MRTCPRECEIVERREPORT;

/* source description (SDES) */
typedef struct MRTCPSOURCEDESCRIPTION
{
    MRTCPCOMMONHEADER   header;     /* common header */
    MRTCPSDESCHUNK      chunk[1];   /* list of SDES items */
} MRTCPSOURCEDESCRIPTION;

/* BYE */
typedef struct MRTCPGOODBYE
{
    MRTCPCOMMONHEADER   header;     /* common header */
    UINT32              src[1];     /* list of sources */
    UINT8               length;     /* length of reason string */
    char                reason[1];
} MRTCPGOODBYE;

/* APP */
typedef struct MRTCPAPPLICATIONDEFINED
{
    MRTCPCOMMONHEADER   header;     /* common header */
    UINT32              src;        /* SSRC/CSRC */
    char                name[4];
    char                data[1];
} MRTCPAPPLICATIONDEFINED;

typedef struct MRTCPREPORTS
{
    MBOOL                            sr_updated;
    MRTCPSENDERREPORT               sr;
    MBOOL                            rr_updated;
    MRTCPRECEIVERREPORT             rr;
    MBOOL                            sdes_updated;
    MRTCPSOURCEDESCRIPTION          sdes;
    MBOOL                            bye_updated;
    MRTCPGOODBYE                    bye;
    MBOOL                            app_updated;
    MRTCPAPPLICATIONDEFINED         app;
} MRTCPREPORTS;


// RTCP reception statistics for RTP data packet
typedef struct
{
    UINT32             ssrc;
    UINT32             expected_packet_count;
    UINT32             actual_packet_count;
    UINT32             last_expected_packet_count;
    UINT32             last_actual_packet_count;
    UINT16             last_low_seq_no;
    UINT16             last_high_seq_no;
    UINT32             last_rtp_timestamp;
    UINT32             last_arrival_timestamp;
    UINT32             prev_jitter;
} MRTCP_RR_STATUS;

// RTCP statistics for RTP data packet


typedef struct
{
    // for sender's report
    UINT32         packet_count;
    UINT32         octet_count;
    UINT32         last_sr_timestamp;
    // for reception report
    MRTCP_RR_STATUS rx_src[1];
} MRTCPSTATUS;

#endif // !__MRTPTYPES_H__
