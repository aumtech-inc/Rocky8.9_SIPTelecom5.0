/***************************************************************************
        File:           $Archive: /stacks/src/rtp/mrtp.cpp $
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri, Albert Wong
        Synopsys:       RTP class implementation
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

#if defined(INCLUDE_RTP)    // Remainder of file skipped if not defined

#include "mrtp.h"
#include "mtimer.h"
#include "mbasictypes.h"
#include "mutils.h"
#include "mtime.h"


// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
  #if !defined (MEMCHECK)
    #define new DEBUG_NEW
  #endif // !MEMCHECK
  #undef THIS_FILE
  static char THIS_FILE[] = __FILE__;
#endif


// set to 1 to enable rtcp reports, 0 to disable rtcp reports
#define _ENABLE_RTCP_REPORTS    0

#if _ENABLE_RTCP_REPORTS
     // for console debugging (available only in debug mode)
#    define _TRACE_RTCP_REPORTS
#endif // _ENABLE_RTCP_REPORTS

// uncomment below to save UDP packets
//#define _SAVE_UDP_DATA

// RTP address retries
#define RTP_ADDRESS_RETRIES     100


// Max Ethernet packet (1518 bytes) minus 802.3/CRC, 802.3, IP, UDP an RTP headers
#define MAXPACKETSIZE (1518-14-4-8-20-16-12) // only used as default if getsockopt fails


#if defined(_SAVE_UDP_DATA) & defined(EXCLUDE_FILEIO)
    #undef _SAVE_UDP_DATA
#endif // _SAVE_UDP_DATA & EXCLUDE_FILEIO


// List and CMRTP RTCP status message processing
class CMRTPList : public CMObjectPtrQueue
{
#if _ENABLE_RTCP_REPORTS
    friend void RTCPReceiveThread(void *not_used);
    friend void RTCPSendThread(void *not_used);
#endif // _ENABLE_RTCP_REPORTS
    friend class CMRTPSSRC;

private:
    CMMutex         access;
#if _ENABLE_RTCP_REPORTS
    CMThread        *rtcp_receive_thread;
    CMThread        *rtcp_send_thread;
    void            CreateReportsThreads();
    void            DestroyReportsThreads();
#endif // _ENABLE_RTCP_REPORTS
    inline void     Lock(){access.Lock();}
    inline void     Release(){access.Release();}

public:
    int             Add(CMRTP *r);
    CMRTP*          Remove(CMRTP *r);
    CMRTPList();
    ~CMRTPList();
};


// note order of static declaration below is important
static CMRTPList gs_RTPList;




#if _ENABLE_RTCP_REPORTS


#define MRTCP_RECEIVE_SLEEP     2000     // in millisecs. Time to sleep between receive in thread loop
#define MRTCP_SEND_SLEEP        5000     // in millisecs. Time to sleep between send in thread loop
#define MRTCP_QUITORDIE_TIMEOUT  200     // time to wait after telling thread to end and giving up so killing it


// RTCP Report reception thread (for all RTCP connections)
void RTCPReceiveThread(void *arg)
{
    CMRTPList *rtp_list = (CMRTPList*)arg;
    assert((int) (rtp_list && rtp_list->rtcp_receive_thread));

    MLOG(( MLOG_DEBUGINFO, 1617, "RTCP Rx", "Starts.\n"));
    while (rtp_list && rtp_list->rtcp_receive_thread && (!rtp_list->rtcp_receive_thread->ShouldQuit()))
    {
        // go through RTP objects and service a status receive
        rtp_list->Lock();
        for (int i=0; i<rtp_list->Length(); i++)
        {
            CMRTP *rtp = (CMRTP *) (*rtp_list)[i];
            if (rtp->ChannelDirection() == MMEDIACHANNEL_TX)   // conditional receive report
                rtp->ReceiveReport();
        }
        rtp_list->Release();
        MSleep(MRTCP_RECEIVE_SLEEP);
    }
}


// RTCP Report sending thread (for all connections)
void RTCPSendThread(void *arg)
{
    CMRTPList *rtp_list = (CMRTPList*)arg;
    assert((int) (rtp_list && rtp_list->rtcp_send_thread));

    MLOG(( MLOG_DEBUGINFO, 1618, "RTCP Tx", "Starts.\n"));
    while (rtp_list && rtp_list->rtcp_send_thread && (!rtp_list->rtcp_send_thread->ShouldQuit()))
    {
        // go through RTP object and service a status send
        rtp_list->Lock();
        for (int i=0; i<rtp_list->Length(); i++)
        {
            CMRTP *rtp =  (CMRTP *) (*rtp_list)[i];
            if (rtp->ChannelDirection() == MMEDIACHANNEL_RX)
            {   // conditional send report
                for (int j=0; j<rtp_list->Length(); j++)
                {
                    CMRTP *rrtp =  (CMRTP *) (*rtp_list)[j];
                    if ((rrtp->ChannelDirection() == MMEDIACHANNEL_TX) && (rrtp->MediaType() == rtp->MediaType()))
                    {
                        // okay only for pt-to-pt without destination.  no destination is available in the structure.
                        rtp->ReverseRTP(rrtp);
                        break;
                    }
                }
                if (!rtp->SendReport())
                {
                    // assuming RTCP channel has died and/or RTP channel has been closed
                    rtp_list->Release();
                    rtp_list->Remove(rtp);    // temp disabling RTCP for debug
                    if (rtp_list && (rtp_list->Length() > 0))   // rtp_list might have been destroyed by Remove()
                        rtp_list->Lock();
                    else
                        return;
                }
            }
        }
        rtp_list->Release();
        MSleep(MRTCP_SEND_SLEEP);
    }
}


#endif // _ENABLE_RTCP_REPORTS




int  CMRTPList::Add(CMRTP *r)
{
    Lock();
#if _ENABLE_RTCP_REPORTS
    CreateReportsThreads();
#endif // _ENABLE_RTCP_REPORTS
    int i = CMObjectPtrQueue::Add((CMObject *) r);
    Release();
    return i;
}


CMRTP * CMRTPList::Remove(CMRTP *r)
{
    Lock();
    CMRTP *rr;
    rr = (CMRTP *) CMObjectPtrQueue::Remove((CMObject *) r);
#if _ENABLE_RTCP_REPORTS
    if (Length()== 0)
    {
        DestroyReportsThreads();
    }
#endif // _ENABLE_RTCP_REPORTS
    Release();
    return rr;
}




#if _ENABLE_RTCP_REPORTS

// Create Report sending and receiving threads. One thread for all reception and one thread for
// all sending
void CMRTPList::CreateReportsThreads()
{
    if (! rtcp_receive_thread)
    {
        if (!(rtcp_receive_thread = new CMThread) || !rtcp_receive_thread->CreateAndRun((MTHREADFUNCTION)RTCPReceiveThread, this, MTHREAD_PRIORITY_LOWEST))
        {
            MLOG(( MLOG_DEBUGERROR, 555, "RTP", "Could not create RTCP status receive thread, status transmission will not be performed.\n"));
            if (rtcp_receive_thread)
            {
                delete rtcp_receive_thread;
                rtcp_receive_thread = NULL;
            }
        }
    }
    if (! rtcp_send_thread)
    {
        if (!(rtcp_send_thread = new CMThread) || !rtcp_send_thread->CreateAndRun((MTHREADFUNCTION)RTCPSendThread, this, MTHREAD_PRIORITY_LOWEST))
        {
            MLOG(( MLOG_DEBUGERROR, 556, "RTP", "Could not create RTCP status send thread, status reception will not be performed.\n"));
            if (rtcp_send_thread)
            {
                delete rtcp_send_thread;
                rtcp_send_thread = NULL;
            }
        }
    }
}

// Destroy the threads and delete the thread objects
void CMRTPList::DestroyReportsThreads()
{
    if (rtcp_send_thread)
    {
        if (rtcp_send_thread->Running())
            rtcp_send_thread->QuitOrDie(MRTP_QUITORDIE_TIMEOUT);
        delete rtcp_send_thread;
        rtcp_send_thread = NULL;
    }
    if (rtcp_receive_thread)
    {
        if (rtcp_receive_thread->Running())
            rtcp_receive_thread->QuitOrDie(MRTP_QUITORDIE_TIMEOUT);
        delete rtcp_receive_thread;
        rtcp_receive_thread = NULL;
    }
}

#endif // _ENABLE_RTCP_REPORTS




CMRTPList::CMRTPList()
{
#if _ENABLE_RTCP_REPORTS
    rtcp_send_thread = rtcp_receive_thread = NULL;
#endif // _ENABLE_RTCP_REPORTS
}


CMRTPList::~CMRTPList()
{
    Lock();
#if _ENABLE_RTCP_REPORTS
    DestroyReportsThreads();
#endif // _ENABLE_RTCP_REPORTS
    Clear();
    Release();
}

// End of RTP status message send/receive



////////////////////////////////////// CMRTP ///////////////////////////////////////////

CMRTP::CMRTP()
{
    rtp_dev = NULL;
    rtcp_dev = NULL;
    reverse_rtp = NULL;
    serial_number_window=0;
    serial_number_modulo=0;
    v=0;
    serial_number_to_read=0;
    serial_number_to_send = GenerateRandomSequenceNumber();

    m_first_read = TRUE;        // for Read()
    m_first_write = TRUE;       // for Write()
    m_zStartWithQueues = FALSE;  // for Read()

    m_base_timestamp = 0;       // NOTE that receiver will overwrite this variable
                              // after receiving first SR report

    gs_RTPList.Add(this);
    rx_thread = NULL;
}


// CMRTP::Create()
//   To create and initialise RTP object
//   some parts are moved from constructor
BOOL CMRTP::Create(MOBJECTID type, MMEDIA_DIRECTIONS channel_dir, MTERMINALLABEL dest)
{
#if defined(_SAVE_UDP_DATA)
    FILE *fdtx, *fdrx;
    if (!(fdtx = fopen("\\temp\\udptx.dat","w")))
    {
        MLOG(( MLOG_DEBUGWARNING, 2098, "RTP", "\\temp\\udptx.dat cannot be created for debugging.\n"));
    }
    else
    {
        fclose(fdtx);
    }
    if (!(fdrx=fopen("\\temp\\udprx.dat","w")))
    {
        MLOG(( MLOG_DEBUGWARNING, 2099, "RTP", "\\temp\\udprx.dat cannot be created for debugging.\n"));
    } else {
        fclose(fdrx);
    }
#endif // defined(_SAVE_UDP_DATA)

#if _ENABLE_RTCP_REPORTS
    // initialise RTCP statistics
    rtcp_access.Lock();
    memset(&rtcp_status, 0, sizeof(rtcp_status));
    rtcp_access.Release();

    // initialise RTCP report container
    memset(&report, 0, sizeof(report));

    // initialise sender report
    report.sr.header.version = RTP_VERSION;
    report.sr.header.count = 1;     // only one report is sent
    report.sr.header.pt = RTCP_SR_PAYLOAD_TYPE;
    report.sr.header.length = 12;

    // initialise reception report
    report.rr.header.version = RTP_VERSION;
    report.rr.header.count = 1;     // only one report is sent
    report.rr.header.pt = RTCP_RR_PAYLOAD_TYPE;
    report.rr.header.length = 7;

    // initialise source description RTCP packet
    report.sdes.header.count = 1;     // assume one SDES item
    report.sdes.header.version = RTP_VERSION;
    report.sdes.header.pt = RTCP_SDES_PAYLOAD_TYPE;

    // initialise goodbye RTCP packet
    report.bye.header.version = RTP_VERSION;
    report.bye.header.pt = RTCP_BYE_PAYLOAD_TYPE;

    // application-defined RTCP packet to be initialised here
    if (channel_dir == MMEDIACHANNEL_TX)
    {
        report.sr.ssrc = UINT32(ssrc);
        report.rr.ssrc = UINT32(ssrc);
        report.sdes.chunk[0].src = UINT32(ssrc);
        report.bye.src[0] = UINT32(ssrc);
    }
#endif // _ENABLE_RTCP_REPORTS

    prev_ssrc = ssrc;

    // must be last
    media_type = type;
    direction = channel_dir;
    destination = dest;

    return TRUE;
}


// CMRTP::GenerateRandomSequenceNumber()
//   To generate a random number for the start of packet sequence number
//   Currently, a random number is generated using the system's seed method.
//   A more appropriate implementation should follow RFC1750.
USHORT CMRTP::GenerateRandomSequenceNumber()
{
    return MGenerateRandomNumber();
}


BOOL CMRTP::Close()
{
    CMRTPPacket *pkt=NULL;

    if (rx_thread)
    {
        if (rtp_dev)
        {
            rtp_dev->Close();
        }
        if (rx_thread->Running())
        {
            rx_thread->QuitOrDie(1000);  // minimal timout should be 100ms
        }
        delete rx_thread;
        rx_thread = NULL;
    }
    PacketsLock();
    while (m_receive_packets.Length())
    {
        pkt = m_receive_packets.Remove(0);
        if (pkt)
        {
            pkt->Free();
            delete pkt;
        }
    }
    PacketsRelease();

    if (rtp_dev)
    {
        rtp_dev->Close();
        rtp_dev->Terminate();
        delete rtp_dev;
        rtp_dev = NULL;
    }
    if (rtcp_dev)
    {
        rtcp_dev->Close();
        rtcp_dev->Terminate();
        delete rtcp_dev;
        rtcp_dev = NULL;
    }
#if _ENABLE_RTCP_REPORTS

    // disconnect reverse RTP relationship
    // this part should be done after appropriate RTCP control thread has been killed
    if (reverse_rtp)
    {
        // "this" is an RTP receive channel, which sends RTCP report
        if (reverse_rtp->ReverseRTP())
        {
            if (reverse_rtp->ReverseRTP() != this)
            {
                MLOG(( MLOG_ERROR, 3817, "RTP Close", "Bi-directional RTP channel does not match.\n"));
            }
        }
        reverse_rtp->ReverseRTP(NULL);
        reverse_rtp = NULL;
    }
    else if (direction == MMEDIACHANNEL_TX)
    {
        // we have to check for any reverse RTP relationship from the rtp_list
        if (rtp_list)
        {
            for (int i=0; i<rtp_list->Length(); i++)
            {
                CMRTP *rtp =  (CMRTP *) (*rtp_list)[i];
                if (rtp->ReverseRTP() == this)
                {
                    // conditional send report
                    rtp->ReverseRTP(NULL);
                }
            }
        }
    }
#endif // _ENABLE_RTCP_REPORTS

    gs_RTPList.Remove(this);

    // free packets in queues
    PacketsLock();
    while (m_in_seq_packets.Length())
    {
        pkt = m_in_seq_packets.Remove(0);
        if (pkt)
        {
            pkt->Free();
            delete pkt;
        }
    }
    while (m_out_of_seq_packets.Length())
    {
        pkt = m_out_of_seq_packets.Remove(0);
        if (pkt)
        {
            pkt->Free();
            delete pkt;
        }
    }
    PacketsRelease();

    return TRUE;
}


CMRTP::~CMRTP()
{
    Close();
}


// CMRTP::Write()
int CMRTP::Write(CMBuffer &pkt)
{
    CMPacket &packet = (CMPacket &) pkt;
    int wrote;
    int ret;
    CMRTPPacket *rtppacket = (CMRTPPacket *)&packet;



#if 1
{
		FILE *fp;
		static int i = 0;
		char filename[64] = "";
		sprintf(filename, "/tmp/video_packets/before_packetise_hdr_%d", i);
		printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
		if(packet.BufferCount() == 1399)
		{
			printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
			i++;
			fp = fopen(filename, "w+");
			if(fp == NULL)
			{
				printf("RGDEBUG:: igoing to write to the file filename=%s \n", filename);fflush(stdout);
				
			}
			fwrite((unsigned char *)&rtppacket->Header(), 1, rtppacket->FixedHdrSize(), fp);
			fclose(fp);
		}
}
#endif



	 printf("RGDEBUG::inside Write DataCount of pkt=%d\n", pkt.DataCount());
	 printf("RGDEBUG::inside Write BufferCount of pkt=%d\n",rtppacket->BufferCount());
	 printf("RGDEBUG::inside Write BufferCount of m_base_timestamp=%d\n",m_base_timestamp);
	 printf("RGDEBUG::inside Write BufferCount of serial_number_to_send=%d\n",serial_number_to_send);
	 printf("RGDEBUG::inside Write BufferCount of ssrc=%d\n",UINT(ssrc));
    // label sequence number is now done within CMRTPPacket
    rtppacket->Header().ts += m_base_timestamp;             // add random timestamp offset
    rtppacket->Header().seq = serial_number_to_send++;    // packet header may be rewritten every time without knowing SSRC
#if _ENABLE_RTCP_REPORTS
    if (ssrc != prev_ssrc)
    {
        // SSRC changed (by external means)
        rtcp_access.Lock();
        rtcp_status.packet_count = 0;
        rtcp_status.octet_count = 0;
        rtcp_access.Release();
    }
#endif // _ENABLE_RTCP_REPORTS
    rtppacket->Header().ssrc = UINT32(ssrc);
    rtppacket->SetMediaType(media_type);

    // actual timestamp value has been assigned in TxRTP



#if 1
{
		FILE *fp;
		static int i = 0;
		char filename[64] = "";
		sprintf(filename, "/tmp/video_packets/after_packetise_hdr_%d", i);
		printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
		if(packet.BufferCount() == 1399)
		{
			printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
			i++;
			fp = fopen(filename, "w+");
			if(fp == NULL)
			{
				printf("RGDEBUG:: igoing to write to the file filename=%s \n", filename);fflush(stdout);
				
			}
			fwrite((unsigned char *)&rtppacket->Header(), 1, rtppacket->FixedHdrSize(), fp);
			fclose(fp);
		}
}
#endif


#if 1
{
		FILE *fp;
		static int i = 0;
		char filename[64] = "";
		sprintf(filename, "/tmp/video_packets/before_packetise_packet_%d", i);
		printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
		if(packet.BufferCount() == 1399)
		{
			printf("RGDEBUG:: igoing to write to the file \n");fflush(stdout);
			i++;
			fp = fopen(filename, "w+");
			if(fp == NULL)
			{
				printf("RGDEBUG:: igoing to write to the file filename=%s \n", filename);fflush(stdout);
				
			}
			fwrite((unsigned char *) packet.Buffer(), 1, packet.BufferCount(), fp);
			fclose(fp);
		}
}
#endif

	packet.Packetise(0);


#if 1
{
		FILE *fp;
		static int i = 0;
		char filename[64] = "";
		sprintf(filename, "/tmp/video_packets/after_packetise_packet_%d", i);
		printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
		if(packet.BufferCount() == 1399)
		{
			printf("RGDEBUG:: igoing to write to the file \n");fflush(stdout);
			i++;
			fp = fopen(filename, "w+");
			if(fp == NULL)
			{
				printf("RGDEBUG:: igoing to write to the file filename=%s \n", filename);fflush(stdout);
				
			}
			fwrite((unsigned char *) packet.Buffer(), 1, packet.BufferCount(), fp);
			fclose(fp);
		}
}
#endif

    if (DataDevice())
    {
	 		printf("RGDEBUG::Going to send packet of BufferCount=%d\n",packet.BufferCount());
	 		printf("RGDEBUG::Going to send packet of DataCount=%d\n",packet.DataCount());

#if 1
{
		FILE *fp;
		static int i = 0;
		char filename[64] = "";
		sprintf(filename, "/tmp/video_packets/packet_%d", i);
		printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
		if(packet.BufferCount() == 1399)
		{
			printf("RGDEBUG:: igoing to write to the file \n");fflush(stdout);
			i++;
			fp = fopen(filename, "w+");
			if(fp == NULL)
			{
				printf("RGDEBUG:: igoing to write to the file filename=%s \n", filename);fflush(stdout);
				
			}
			fwrite((unsigned char *) packet.Buffer(), 1, packet.BufferCount(), fp);
			fclose(fp);
		}
}
#endif


			
        wrote = DataDevice()->Write((unsigned char *) packet.Buffer(), packet.BufferCount());

#if defined(_SAVE_UDP_DATA)
        FILE *fdtx = fopen("\\temp\\udptx.dat","a+");
        if (fdtx)
        {
            fprintf(fdtx, "%4d  %4d  %4d\n", wrote, packet.BufferCount(), packet.DataCount() );
            for (int j=0; j<packet.DataCount(); j++)
            {
                fprintf(fdtx, "%2x ", *(packet.Data()+j));
            }
            fprintf(fdtx, "\n\n");
        }
        while (fclose(fdtx));
#endif // defined(_SAVE_UDP_DATA)

#if _ENABLE_RTCP_REPORTS
        // update RTCP sender statistics
        rtcp_access.Lock();
        rtcp_status.packet_count++;
        rtcp_status.octet_count += packet.DataCount();  // should all payload octet count be accumulated even if not all being sent?
        rtcp_access.Release();
#endif // _ENABLE_RTCP_REPORTS

        if (wrote == packet.BufferCount())
            ret = packet.DataCount();
        else
        {
            MLOG(( MLOG_ERROR, 3835, "RTP Write", "Wrote %d out of %d bytes.\n", wrote, packet.BufferCount()));
            ret = -1;
        }
    } else
        ret = -1;

    return ret;
}

//
// MAJ: What is the difference between RTCPUpdateSN and RTCPUpdateSN2??
//
#if _ENABLE_RTCP_REPORTS
void CMRTP::RTCPUpdateSN1(CMRTPPacket *p_pRtpPkt)
{
    // update RTCP reception statistics, assuming unicast
    rtcp_access.Lock();
    rtcp_status.rx_src[0].ssrc = p_pRtpPkt->Header().ssrc;
    rtcp_access.Release();
    // serial number difference must always be either +1 or +2.
    int l_iPacketsExpected = p_pRtpPkt->Header().seq - serial_number_to_read + 1;
    if (l_iPacketsExpected < -(RTP_PACKET_SN_LENGTH>>1) )
    {
        l_iPacketsExpected += RTP_PACKET_SN_LENGTH;
    }
    assert( (l_iPacketsExpected == 1) || (l_iPacketsExpected == 2) );
    rtcp_access.Lock();
    rtcp_status.rx_src[0].expected_packet_count += l_iPacketsExpected;
    rtcp_status.rx_src[0].actual_packet_count++;
    if (p_pRtpPkt->Header().seq > rtcp_status.rx_src[0].last_low_seq_no)
    {
        rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
    }
    else if ((rtcp_status.rx_src[0].last_low_seq_no - p_pRtpPkt->Header().seq) > (RTP_PACKET_SN_LENGTH>>1))
    {
        rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
        rtcp_status.rx_src[0].last_high_seq_no++;
    }
    rtcp_access.Release();
}

void CMRTP::RTCPUpdateSN2(CMRTPPacket *p_pRtpPkt)
{
    // packet loss accumulation should be done here
    // update RTCP reception statistics, assuming unicast
    rtcp_access.Lock();
    rtcp_status.rx_src[0].ssrc = p_pRtpPkt->Header().ssrc;
    rtcp_access.Release();
    // serial number difference must always > 0.
    int l_iPacketsExpected = p_pRtpPkt->Header().seq - serial_number_to_read + 1;
    if (l_iPacketsExpected < -(RTP_PACKET_SN_LENGTH>>1) )
    {
        l_iPacketsExpected += RTP_PACKET_SN_LENGTH;
    }
    assert(l_iPacketsExpected > 0);
    rtcp_access.Lock();
    rtcp_status.rx_src[0].expected_packet_count += l_iPacketsExpected;
    rtcp_status.rx_src[0].actual_packet_count++;
    if (p_pRtpPkt->Header().seq > rtcp_status.rx_src[0].last_low_seq_no)
    {
        rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
    }
    else if ((rtcp_status.rx_src[0].last_low_seq_no - p_pRtpPkt->Header().seq) > (RTP_PACKET_SN_LENGTH>>1))
    {
        rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
        rtcp_status.rx_src[0].last_high_seq_no++;
    }
    rtcp_access.Release();
}

void CMRTP::RTCPUpdateSN3(CMRTPPacket *p_pRtpPkt)
{
    // update RTCP reception statistics, assuming unicast
    rtcp_access.Lock();
    rtcp_status.rx_src[0].ssrc = p_pRtpPkt->Header().ssrc;
    rtcp_status.rx_src[0].expected_packet_count ++;
    rtcp_status.rx_src[0].actual_packet_count++;
    if (p_pRtpPkt->Header().seq > rtcp_status.rx_src[0].last_low_seq_no)
    {
        rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
    }
    else if ((rtcp_status.rx_src[0].last_low_seq_no - p_pRtpPkt->Header().seq) > (RTP_PACKET_SN_LENGTH>>1))
    {
        rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
        rtcp_status.rx_src[0].last_high_seq_no++;
    }
    rtcp_access.Release();
}

void RTCPUpdateSN4(CMRTPPacket *p_pRtpPkt)
{
    rtcp_access.Lock();
    rtcp_status.rx_src[0].ssrc = p_pRtpPkt->Header().ssrc;
    rtcp_status.rx_src[0].expected_packet_count++;
    rtcp_status.rx_src[0].actual_packet_count++;
    if ((rtcp_status.rx_src[0].last_low_seq_no == 0) && (rtcp_status.rx_src[0].last_high_seq_no == 0))
    {
        // it is really the first packet for this session
        rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
        rtcp_status.rx_src[0].last_rtp_timestamp = p_pRtpPkt->Header().ts;
        rtcp_status.rx_src[0].last_arrival_timestamp = MMakeRTPTimeStamp();
    }
    else
    {
        if (p_pRtpPkt->Header().seq > rtcp_status.rx_src[0].last_low_seq_no)
        {
            rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
        }
        else if ((rtcp_status.rx_src[0].last_low_seq_no - p_pRtpPkt->Header().seq) > (RTP_PACKET_SN_LENGTH>>1))
        {
            rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
            rtcp_status.rx_src[0].last_high_seq_no++;
        }
        // update interarrival jitter
        MUpdateInterarrivalJitter(p_pRtpPkt->Header().ts, rtcp_status.rx_src[0].last_rtp_timestamp,
                    rtcp_status.rx_src[0].last_arrival_timestamp, rtcp_status.rx_src[0].prev_jitter);
    }
    rtcp_access.Release();
}

void CMRTP::RTCPUpdateSN5(CMRTPPacket *cp)
{
    // update RTCP reception statistics, assuming unicast,
    rtcp_access.Lock();
    rtcp_status.rx_src[0].ssrc = cp->Header().ssrc;

    // update interarrival jitter, it doesn't matter for the correct packet sequence
    MUpdateInterarrivalJitter(cp->Header().ts, rtcp_status.rx_src[0].last_rtp_timestamp,
                rtcp_status.rx_src[0].last_arrival_timestamp, rtcp_status.rx_src[0].prev_jitter);
    rtcp_access.Release();
}

void CMRTP::RTCPUpdateSN6(CMRTPPacket *p_pRtpPkt)
{
    // update RTCP reception statistics
    rtcp_access.Lock();
    rtcp_status.rx_src[0].expected_packet_count++;
    rtcp_status.rx_src[0].actual_packet_count++;
    if (p_pRtpPkt->Header().seq > rtcp_status.rx_src[0].last_low_seq_no)
    {
        rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
    }
    else if ((rtcp_status.rx_src[0].last_low_seq_no - p_pRtpPkt->Header().seq) > (RTP_PACKET_SN_LENGTH>>1))
    {
        rtcp_status.rx_src[0].last_low_seq_no = p_pRtpPkt->Header().seq;
        rtcp_status.rx_src[0].last_high_seq_no++;
    }
    rtcp_access.Release();
}

void CMRTP::RTCPIncrementPacketCount()
{
    rtcp_access.Lock();
    rtcp_status.rx_src[0].actual_packet_count++;    // this packet is not expected
    rtcp_access.Release();
}

#else

void CMRTP::RTCPUpdateSN1(CMRTPPacket *p_pRtpPkt) {}
void CMRTP::RTCPUpdateSN2(CMRTPPacket *p_pRtpPkt) {}
void CMRTP::RTCPUpdateSN3(CMRTPPacket *p_pRtpPkt) {}
void CMRTP::RTCPUpdateSN4(CMRTPPacket *p_pRtpPkt) {}
void CMRTP::RTCPUpdateSN5(CMRTPPacket *p_pRtpPkt) {}
void CMRTP::RTCPUpdateSN6(CMRTPPacket *p_pRtpPkt) {}

void CMRTP::RTCPIncrementPacketCount(){}
#endif // _ENABLE_RTCP_REPORTS

void CMRTP::MoveFromOSQ2ISQ(USHORT p_usNextSN)
{
    int l_iCnt;
    while ( (l_iCnt = m_out_of_seq_packets.Length()) != 0 )
    {
        // check SN within available range
        BOOL l_zConsecSNFound = FALSE;  // whether consecutive packet SN is found
        for (int i=0; i < l_iCnt; i++)
        {
            CMRTPPacket *l_pTempRtpPkt = m_out_of_seq_packets[i];
            if (l_pTempRtpPkt->Header().seq == p_usNextSN)
            {
                // move packet to in_sequence queue
                m_out_of_seq_packets.Remove(i);
                m_in_seq_packets.Add(l_pTempRtpPkt);
                ++p_usNextSN %= RTP_PACKET_SN_LENGTH;
                l_zConsecSNFound = TRUE;
                break;
            }
        }
        if (! l_zConsecSNFound)
        {
            break;
        }
    }
}

int CMRTP::ProcessISQ(CMRTPPacket *p_pRtpPkt)
{
    CMRTPPacket *l_pCurrRtpPkt = NULL;

    assert(p_pRtpPkt);
    p_pRtpPkt->ShallowCopy(*m_in_seq_packets[0]);

    MLOG(( MLOG_DEBUGWARNING, 5487, "RTP Read", "Packet with SN %d pumped out from in sequence packets queue.\n", p_pRtpPkt->Header().seq));
    
    RTCPUpdateSN1(p_pRtpPkt);

    l_pCurrRtpPkt = m_in_seq_packets.Remove(0);
    if (!l_pCurrRtpPkt)
    {
        MLOG(( MLOG_DEBUGERROR, 560, "RTP Read", "Error in freeing a packet.\n"));
        return -1;
    }
    delete l_pCurrRtpPkt;  // note that there is no need to perform l_pCurrRtpPkt->Free() as internal buffers are passed straight to rtppacket, AW010813
    l_pCurrRtpPkt = NULL;

    serial_number_to_read = (unsigned short) ((p_pRtpPkt->Header().seq + 1) % RTP_PACKET_SN_LENGTH);
    return p_pRtpPkt->DataCount();
}

int CMRTP::ProcessOSQ(CMRTPPacket * p_pRtpPkt)
{
    CMRTPPacket *l_pCurrRtpPkt = NULL;
    // m_in_seq_packets length == 0. Look if any packets in OSQ are valid.
    while (m_out_of_seq_packets.Length() > 0) // MAJ: Was a logical test
    {
        l_pCurrRtpPkt = m_out_of_seq_packets[0];
        USHORT l_usFirstSN = l_pCurrRtpPkt->Header().seq;
        int l_iRange = l_usFirstSN - serial_number_to_read;
        if (l_iRange < 0) l_iRange += RTP_PACKET_SN_LENGTH;   // l_iRange should now be >= 0
        if (l_iRange > (RTP_PACKET_SN_LENGTH>>1) )
        {
            // packet way out of sequence, has to be discarded
            RTCPIncrementPacketCount();
            // delete from OSQ
            m_out_of_seq_packets.Remove(0);
            MLOG(( MLOG_DEBUGWARNING, 5488, "RTP Read", "Packet with SN %d discarded from out of sequence packets queue.\n", l_pCurrRtpPkt->Header().seq));
            // Free it
            if (l_pCurrRtpPkt)
            {
                l_pCurrRtpPkt->Free();
                delete l_pCurrRtpPkt;
                l_pCurrRtpPkt = NULL;
            }
        }
        else
        {
            // try to get packet to sequence if necessary
            if ((m_out_of_seq_packets.Length() > 1) && (l_iRange > 0))
            {
                // not serial_number_to_read
                CMRTPPacket *l_pOutOfSeqRtpPkt;         // out_of_seq packet
                USHORT l_usNextSN = serial_number_to_read;
                while (l_usNextSN != l_usFirstSN)
                {
                    int l_iOutOfSeqRtpPktNext = 1;    // first out_of_seq packet to start
                    while (l_iOutOfSeqRtpPktNext < m_out_of_seq_packets.Length())
                    {
                        l_pOutOfSeqRtpPkt = m_out_of_seq_packets[l_iOutOfSeqRtpPktNext];
                        if (l_usNextSN == l_pOutOfSeqRtpPkt->Header().seq)
                        {
                            // packet with serial_number_to_read found in out_of_seq_packets queue
                            MLOG(( MLOG_DEBUGINFO, 4792, "RTP Read", "Packet with SN %d put into order from out of sequence packets queue.\n", l_usNextSN));
                            l_pCurrRtpPkt = l_pOutOfSeqRtpPkt;
                            l_usNextSN = l_usFirstSN;     // this is for escaping previous level while loop
                            break;  // pump out packet
                        }
                        l_iOutOfSeqRtpPktNext++;
                    }
                    if (l_usNextSN != l_usFirstSN) 
                    {
                        // this is to allow while loop escape if l_pCurrRtpPkt has been found
                        ++l_usNextSN %= RTP_PACKET_SN_LENGTH;  // set next SN to search for next while loop
                    }
                }
            }
            // pump out packet from out_of_sequence queue from top to bottom
            p_pRtpPkt->ShallowCopy(*l_pCurrRtpPkt);
            USHORT sn = p_pRtpPkt->Header().seq;
            MLOG(( MLOG_DEBUGWARNING, 5489, "RTP Read", "Packet with SN %d pumped out from out of sequence packets queue.\n", sn));
            RTCPUpdateSN2(p_pRtpPkt);
            m_out_of_seq_packets.Remove(l_pCurrRtpPkt);
            if (l_pCurrRtpPkt)
            {
                delete l_pCurrRtpPkt;
                l_pCurrRtpPkt = NULL;
            }
            serial_number_to_read = (unsigned short) ((sn + 1) % RTP_PACKET_SN_LENGTH);
            return p_pRtpPkt->DataCount();
        }
    } // while (m_out_of_seq_packets.Length() > 0)
    m_zStartWithQueues = FALSE;
    m_first_read = TRUE;   // restart serial number sync
    return 0; // no packet found
}

int CMRTP::GetFromISQ(CMRTPPacket *p_pRtpPkt)
{
    p_pRtpPkt->ShallowCopy(*m_in_seq_packets[0]);
    CMRTPPacket *l_pCurrRtpPkt = m_in_seq_packets.Remove(0);
    if ( l_pCurrRtpPkt == NULL )
    {
        MLOG(( MLOG_DEBUGERROR, 561, "RTP Read", "Error in freeing a packet.\n"));
        return -1;
    }
    delete l_pCurrRtpPkt;  // note that there is no need to perform l_pCurrRtpPkt->Free() as internal buffers are passed straight to rtppacket
    l_pCurrRtpPkt = NULL;

    RTCPUpdateSN3(p_pRtpPkt);

    ++serial_number_to_read %= RTP_PACKET_SN_LENGTH;

    if (m_in_seq_packets.Length() == 0)
    {
        // check out_of_seq queue if there is any packets that can be moved to in sequence queue
        USHORT l_usNextSN = (USHORT) ((serial_number_to_read + 1) % RTP_PACKET_SN_LENGTH);
        MoveFromOSQ2ISQ(l_usNextSN);
    }
    return p_pRtpPkt->DataCount();
}

int CMRTP::GetFromFirstRead(CMRTPPacket *p_pRtpPkt)
{
    // this 'm_first_read' block put here for program flow optimisation
    // first time packet read or resync from losing too many packets
    int i = DataDevice()->Read((unsigned char *) p_pRtpPkt->Buffer(), p_pRtpPkt->MaxDataSize());
    if (i <= 0)
    {
        MLOG(( MLOG_DEBUGWARNING, 2100, "RTP Read", "Problem on data read (%d), assuming remote has closed the channel.\n", i));
        return -1;
    }
    p_pRtpPkt->SetMediaType(media_type);
    p_pRtpPkt->Segmentalise(i);
    serial_number_to_read = (unsigned short) ((p_pRtpPkt->Header().seq + 1) % RTP_PACKET_SN_LENGTH);
    MLOG(( MLOG_DEBUGINFO, 4793, "RTP Read", "First RTP packet has SN %d.\n", p_pRtpPkt->Header().seq));
    m_first_read = FALSE;
    if (m_first_write)
    {
        m_base_timestamp = p_pRtpPkt->Header().ts;
        m_first_write = FALSE;
    }
    // update RTCP reception statistics, assuming unicast
    RTCPUpdateSN4(p_pRtpPkt);
    return p_pRtpPkt->DataCount();
}

// CMRTP::ReadPacket()
// NOTE that CMPacket &packet has to have memory allocated before
//      this function is called.
int CMRTP::ReadPacket(CMBuffer &packet)
{
    CMRTPPacket *rtppacket = (CMRTPPacket *)&packet;
    CMRTPPacket *l_pCurrRtpPkt = NULL;    // current packet to read

    if (! DataDevice())
    {
        return -1;
    }

    // was time_out label. Note l_zTimeOut flag will never be set to FALSE, so this is like while(1). 
    // Only way to get out is to have a packet or an error.
    bool l_zTimeOut = TRUE;
    while (l_zTimeOut) 
    {
        // time out, pump out packets from all queues
        if (m_zStartWithQueues)
        {
            if (m_in_seq_packets.Length() > 0) // MAJ: was a logical test
            {
                return ProcessISQ(rtppacket);
            }
            else
            {
                int ret = ProcessOSQ(rtppacket);
                // only return if we got a packet
                if (ret > 0) 
                {
                    return ret;
                }
            }
        }
        // now look if we have any packets in the ISQ
        if (m_in_seq_packets.Length() > 0) // MAJ: Was logical test
        {
            if ( m_in_seq_packets[0]->Header().seq == serial_number_to_read)
            {
                // read from in_sequence packets queue
                return GetFromISQ(rtppacket);
            }
        }

        if (m_first_read)
        {
            return GetFromFirstRead(rtppacket);
        }
        // not first read
        int serial_number_range = serial_number_to_read + serial_number_window;
        bool wrapped = FALSE;  // whether packet window range overlaps the edges
        if (serial_number_range >= RTP_PACKET_SN_LENGTH)
        {
            serial_number_range -= RTP_PACKET_SN_LENGTH;
            wrapped = TRUE;
        }
        // read_packet was a label.
        BOOL l_zReadPacket = TRUE; // l_zReadPacket will be set to FALSE when we need to break out of the 
                            // loop to continue in the outer loop.
        while (l_zReadPacket)
        {
            // create new packet.
            if ( (l_pCurrRtpPkt = new CMRTPPacket) == NULL )
            {
                MLOG(( MLOG_ERROR, 3827, "RTP Read", "Failed to create a temporary RTP packet.\n"));
                return -1;
            }
            l_pCurrRtpPkt->SetMediaType(media_type);
            l_pCurrRtpPkt->DataMakeSpace(rtppacket->MaxDataSize());
            // usual way of reading a packet
            int i = DataDevice()->Read((unsigned char *) l_pCurrRtpPkt->Buffer(), l_pCurrRtpPkt->MaxDataSize());  // new
            if (i <= 0)
            {
                MLOG(( MLOG_WARNING, 5490, "RTP Read", "Problem on data read (%d), assuming remote has closed the channel.\n", i));
                if (l_pCurrRtpPkt)
                {
                    l_pCurrRtpPkt->Free();
                    delete l_pCurrRtpPkt;
                    l_pCurrRtpPkt = NULL;
                }
                return -1;
            }
            l_pCurrRtpPkt->Segmentalise(i);
            USHORT sn = l_pCurrRtpPkt->Header().seq;
            RTCPUpdateSN5(l_pCurrRtpPkt);
            if (sn == serial_number_to_read)
            {
                // correct sequence, pass this packet straight to next stage. MAJ: What is next stage?
                rtppacket->ShallowCopy(*l_pCurrRtpPkt);
                if (!l_pCurrRtpPkt) // MAJ: Why this test here?
                {
                    MLOG(( MLOG_ERROR, 3828, "RTP Read", "Error in freeing a packet.\n"));
                    return -1;
                }
                delete l_pCurrRtpPkt;  // note that there is no need to perform l_pCurrRtpPkt->Free() as internal buffers are passed straight to rtppacket, AW010813
                l_pCurrRtpPkt = NULL;
                ++serial_number_to_read %= RTP_PACKET_SN_LENGTH;
                RTCPUpdateSN6(rtppacket);
                return rtppacket->DataCount();
            }

            // Packet does not have the expected sequence number. Find our what to do with the packet.
            if ( ( (wrapped) && ((sn > serial_number_to_read) || (sn <= serial_number_range)) )
                    || ( (!wrapped) && ((sn > serial_number_to_read) && (sn <= serial_number_range)) ) )
            {
                // serial number within sequence number window
                if ( ( (m_in_seq_packets.Length()==0) && ( (sn == ((serial_number_to_read + 1) % RTP_PACKET_SN_LENGTH)) ) )
                        || ( (m_in_seq_packets.Length()>0) && ( (sn == ((m_in_seq_packets.Last()->Header().seq + 1) % RTP_PACKET_SN_LENGTH)) ) ) )
                {
                    // add to in_sequence queue
                    m_in_seq_packets.Add(l_pCurrRtpPkt);
                    // check out_of_sequence queue for any packets that can be moved to in_sequence_queue
                    USHORT l_usNextSN = (USHORT) ((l_pCurrRtpPkt->Header().seq + 1) % RTP_PACKET_SN_LENGTH);
                    MoveFromOSQ2ISQ(l_usNextSN);
                }
                else
                {
                    // Add packet to out_of_sequence queue
                    m_out_of_seq_packets.Add(l_pCurrRtpPkt);
                }
            }
            else
            {
                // serial number outside sequence number window, time-out and clear all queue
                MLOG(( MLOG_DEBUGWARNING, 5491, "RTP Read", "Time out reading packet with SN %d, probably out of sequence (actual SN %d).\n", serial_number_to_read, sn));
                // packet loss accumulation should be done here
                m_out_of_seq_packets.Add(l_pCurrRtpPkt);
                m_zStartWithQueues = TRUE;
                l_zReadPacket = FALSE; // was goto time_out; Here we break out of the while(l_zReadPacket) -> goto begining of while(l_zTimeOut) loop
            }
        } // while (l_zReadPacket)
    } // while(l_zTimeOut)

    return -1;
}


int CMRTP::Read(CMBuffer &packet, unsigned long waittime)
{
    CMRTPPacket *read_pkt = (CMRTPPacket*)&packet;
    CMRTPPacket *pkt;
    CMThread* caller = CMThread::MyThread();
    DWORD wait_interval = 5;    // 5ms wait per block read
    DWORD curr_wait_time, start_wait_time, difftime;

    curr_wait_time = start_wait_time = MCoreAPITime();

    read_pkt->ResetPointer();

    while ((caller && !caller->ShouldQuit()) && (rx_thread && rx_thread->Running()))
    {
        PacketsLock();
        if (m_receive_packets.Length() > 0)
        {
            // get first packet to return
            pkt = m_receive_packets.Remove(0);
            if (pkt)
            {
                PacketsRelease();
                read_pkt->ShallowCopy(*pkt);
                delete pkt;     // Note that there is no need to Free() pkt as it has 
                                // been transferred to packet
                break;
            }
        }
        PacketsRelease();

        curr_wait_time = MCoreAPITime();
        if (curr_wait_time >= start_wait_time)
        {
            difftime = curr_wait_time - start_wait_time;
        }
        else
        {
            difftime = start_wait_time + (0xffffffff - curr_wait_time);
        }
        if (difftime >= waittime)
        {
            // non-block
            break;
        }

        // block read
        MSleep(wait_interval);
    }
    return read_pkt->DataCount();
}

#define RTP_MAX_RX_PACKET_SIZE  2600    // should be at least equal to worst case media payload size
                                        // and at most equal to UDP buffer size

void CMRTP::ReceiveThread(void * p_pArg)
{
    if (p_pArg != NULL)
    {
        ((CMRTP *)p_pArg)->ReceiveThread();
    }
}


void CMRTP::ReceiveThread()
{
    CMRTPPacket *rxpkt=NULL;
    int sz;
    int max_queue_length;

#if defined(INCLUDE_LOGGING)
    int iRxQueueFullWarningCnt = 0;
    DWORD uiCurrWarningTime, uiPrevWarningTime, uiWarningInterval = 5000;    // warning interval in ms
    uiPrevWarningTime = MCoreAPITime() - uiWarningInterval;
    MLOG(( MLOG_DEBUGINFO, 1624, "RTP Rx", "Starts.\n"));
#endif // defined(INCLUDE_LOGGING)

    if (!rtp_dev->Listen())
    {
        return;
    }

    if ((MediaType() == MG7231))
    {
        // all audio channels get shorter queue length
        max_queue_length = serial_number_window >> 1;

        if (max_queue_length < 2)
        {
            max_queue_length = 2;
        }
    }
    else if (MediaType() == MGSMAMR)
    {
         max_queue_length = serial_number_window >> 1;
    }
    else if (MediaType() == MG711)
    {
        // Note that for short frame size of G.711, queue length should not be shortened.
        max_queue_length = serial_number_window >> 1;
        if (max_queue_length < 60)     // 60ms eqv for frame size = 1
        {
            max_queue_length = 60;
        }
    }
    else
    {
        max_queue_length = serial_number_window;
    }

    while (!rx_thread->ShouldQuit())
    {
        rxpkt = new CMRTPPacket();
        rxpkt->DataMakeSpace(RTP_MAX_RX_PACKET_SIZE);     // we need to create worst case buffer size here
        sz = ReadPacket(*rxpkt);
        if (sz <= 0)
        {
            // socket error, slow down next read to avoid overloading CPU
            rxpkt->Free();
            delete rxpkt;
            rxpkt = NULL;
            MSleep(5);
            continue;
        }
        MSleep(0);  // yield task

        PacketsLock();

        if (m_receive_packets.Length() >= max_queue_length)
        {
        #if defined(INCLUDE_LOGGING)
            iRxQueueFullWarningCnt++;
            uiCurrWarningTime = MCoreAPITime();
            if ((uiCurrWarningTime - uiPrevWarningTime) > uiWarningInterval)
            {
                MLOG(( MLOG_WARNING, 5498, "RTP Rx", "Receive packet queue for RTP 0x%08x is full (=%d) for %d times, discard packet with SN %d.\n", this, max_queue_length, iRxQueueFullWarningCnt, rxpkt->Header().seq));
                uiPrevWarningTime = uiCurrWarningTime;
            }
        #endif // defined(INCLUDE_LOGGING)
            // discard current packet
            rxpkt->Free();
            delete rxpkt;
            rxpkt = NULL;
        }
        else
        {
            m_receive_packets.Add(rxpkt);
            // Note that there is no need to Free() rxpkt and destroy rxpkt as it is stored into receive_packets queue.
        }
        PacketsRelease();

        MSleep(0);  // yield task
    }

    MLOG(( MLOG_DEBUGINFO, 1625, "RTP Rx", "Terminated gracefully.\n"));
}


BOOL CMRTP::SendReport()
{
#if _ENABLE_RTCP_REPORTS
    // initialise report update status
    report.sr_updated = report.rr_updated = report.sdes_updated = report.bye_updated = report.app_updated = FALSE;

    UINT32 timestamp = MMakeRTPTimeStamp();    // an NTP timestamp could be used here

    // collect report details and packetise
    // encryption prefix, if required

    // SR or RR
    // unicast communication shall always have a maximum of one reception report block
    rtcp_access.Lock();
    int curr_expected = rtcp_status.rx_src[0].expected_packet_count - rtcp_status.rx_src[0].last_expected_packet_count;
    int curr_lost = curr_expected - (rtcp_status.rx_src[0].actual_packet_count - rtcp_status.rx_src[0].last_actual_packet_count);
    rtcp_access.Release();

    if (curr_lost != curr_expected)
    {
        // a reception report is to be sent, prepare reception report
        int i=0;                        // only one report is to be generated

        report.sr.rr[i].ssrc = rtcp_status.rx_src[i].ssrc;                      // SSRC_1

        if ( (curr_lost <= 0) || (curr_expected == 0) )
            report.sr.rr[i].fraction = 0;  // fraction lost
        else
            report.sr.rr[i].fraction = (curr_lost * 256) / curr_expected;       // fraction lost

        rtcp_access.Lock();
        report.sr.rr[i].lost = (UINT32)(rtcp_status.rx_src[i].expected_packet_count - rtcp_status.rx_src[i].actual_packet_count);    // cumulative number of packets lost, value is signed
        UINT32 highest_seq = rtcp_status.rx_src[i].last_low_seq_no + (rtcp_status.rx_src[i].last_high_seq_no<<16);
        if (report.sr.rr[i].last_seq < highest_seq )
            // this assumes no wrapped of the 32 bit numbers
            report.sr.rr[i].last_seq = highest_seq;
        report.sr.rr[i].jitter = rtcp_status.rx_src[i].prev_jitter;             // interarrival jitter
        if (reverse_rtp)
        {
            report.sr.rr[0].lsr = reverse_rtp->RTCPStatus().last_sr_timestamp;  // last SR timestamp
            if (report.sr.rr[0].lsr)
            {
                UINT32 ts = ((((report.sr.rr[0].lsr>>5) * 25)>>3) * 5)>>5;      // convert lsr to millisec, eqv. to multiplying factor (1000/(2^16))
                                                                                // this way is to minimise lose of accuracy while avoiding possible overflow
                // dlsr is in units of 1/(2^16) seconds
                UINT32 delay = (UINT32)((timestamp - ts) % RTP_TIMESTAMP_LENGTH);
                if (delay < ((RTP_TIMESTAMP_LENGTH>>16) * 1000) )               // i.e. len/((len>>16)/1000))
                {
                    report.sr.rr[0].dlsr = (((((delay/5)<<2)/5)<<2)/5)<<9;      // eqv. multiplying factor ((2^16)/1000)
                }
                else
                {
                    report.sr.rr[0].dlsr = 0xffffffff;                          // overflow
                }
            }
            else
                report.sr.rr[0].dlsr = 0;                                       // delay since last SR
        }
        rtcp_access.Release();

        report.sr.header.count = 1;     // only one reception report is to be sent
    }
    else
    {
        report.sr.header.count = 0;     // no reception report is to be sent
    }

    if ( (reverse_rtp) && !((report.sr.psent == reverse_rtp->RTCPStatus().packet_count)
                && (report.sr.osent == reverse_rtp->RTCPStatus().octet_count)) )
    {
        // this RTP channel has the corresponding sender channel
        // and the sender channel still keeps sending packets, send sender report
        report.sr.ssrc = UINT32(ssrc);

        // determine timestamps
        report.sr.rtp_ts = MMakeRTPTimeStamp() + reverse_rtp->BaseTimeStamp();    // RTP timestamp,
                                                                                  // use local terminal's m_base_timestamp

        // Note: NTP timestamp generator is not implemented
        //       we assume there is no wallclock available and set the timestamp to zero
        //       according to RFC1889
        //       If NTP is implemented, RTP timestamp should be derived from it.
        report.sr.ntp_sec = 0;   // NTP timestamp
        report.sr.ntp_frac = 0;

        report.sr.psent = reverse_rtp->RTCPStatus().packet_count;    // sender's packet count
        report.sr.osent = reverse_rtp->RTCPStatus().octet_count;     // sender's octet count

        if (curr_lost != curr_expected)
            report.sr.header.count = 1;       // only one report is to be sent
        else
            report.sr.header.count = 0;       // no report is to be sent

        report.sr.header.length = 7 + report.sr.header.count * 6 - 1;
        report.sr_updated = TRUE;
    }
    else
    {
        // no corresponding sender channel identified, send reception report
        report.rr.ssrc = UINT32(ssrc);

        // additional RRs, assuming one report by now due to unicast.
        if (curr_lost != curr_expected)
        {
            report.rr.header.count = 1;                     // only one report is to be sent
            memcpy(&(report.rr.rr[0]), &(report.sr.rr[0]), sizeof(MRTCPREPORTBLOCK));
        }
        report.rr.header.count = report.sr.header.count;    // no report is to be sent
        report.rr.header.length = 2 + report.rr.header.count * 6 - 1;
        report.rr_updated = TRUE;
    }

    // SDES
    // there is only 1 chunk for unicast, hence count = 1
    // NOTE: could have memory problem for item[] since it has not be readjusted
    int total_len = 0;        // total number of UINT32 - 1
    int len;
    report.sdes.chunk[0].src = UINT32(ssrc);
    total_len++;
    len = 0;
    report.sdes.chunk[0].item[0].type = RTCP_SDES_CNAME;
    memset(report.sdes.chunk[0].item[0].data, 0, sizeof(report.sdes.chunk[0].item[0].data));
    memcpy(report.sdes.chunk[0].item[0].data, "sample", sizeof("sample"));
    report.sdes.chunk[0].item[0].length = strlen(report.sdes.chunk[0].item[0].data);
    len += 2 + report.sdes.chunk[0].item[0].length;
    report.sdes.chunk[0].item[1].type = RTCP_SDES_END;
    len++;
    total_len += ((len + 3)/ sizeof(UINT32));

    report.sdes.header.length = total_len;
    report.sdes_updated = TRUE;

    // BYE or APP
    // nothing sent

    // send RTCP packet
    return WriteControlPacket(report);
#endif // _ENABLE_RTCP_REPORTS

    return TRUE;
}


BOOL CMRTP::ReceiveReport()
{
#if _ENABLE_RTCP_REPORTS
    // receive RTCP packet
    if (!ReadControlPacket(report))
        return FALSE;

#if defined(_DEBUG_RTP) & defined(_TRACE_RTCP_REPORTS)
    if (report.sr_updated)
    {
        MLOG(( MLOG_INFO, 4763, "RTCP", "RTCP Sender Report:\n"));
        MLOG(( MLOG_INFO, 4764, "RTCP", "SSRC: 0x%08x\n", report.sr.ssrc));
        MLOG(( MLOG_INFO, 4765, "RTCP", "NTP timestamp: %d sec.%d, RTP timestamp: %d\n",
                report.sr.ntp_sec, report.sr.ntp_frac, report.sr.rtp_ts));
        MLOG(( MLOG_INFO, 4766, "RTCP", "Packet count: %d Octet count: %d\n", report.sr.psent, report.sr.osent));
        MLOG(( MLOG_INFO, 4767, "RTCP", "SSRC_1: 0x%08x\n", (UINT32)report.sr.rr[0].ssrc));
        MLOG(( MLOG_INFO, 4768, "RTCP", "Loss: %.2f, %d packets, jitter = %d, highest seq = 0x%08x\n", report.sr.rr[0].fraction/256.0, report.sr.rr[0].lost, report.sr.rr[0].jitter, report.sr.rr[0].last_seq));
        MLOG(( MLOG_INFO, 4769, "RTCP", "LSR = %d DLSR = %d\n", report.sr.rr[0].lsr, report.sr.rr[0].dlsr));
        MLOG(( MLOG_INFO, 4228, "------------------------------------------\n"));
        
        rtcp_access.Lock();
        rtcp_status.last_sr_timestamp = ((report.sr.ntp_sec & 0x0000ffff)<<16) | ((report.sr.ntp_frac & 0xffff0000)>>16);
        rtcp_access.Release();
    }

    if (report.rr_updated)
    {
        MLOG(( MLOG_INFO, 4229, "RTCP", "RTCP Reception Report:\n"));
        MLOG(( MLOG_INFO, 4230, "RTCP", "SSRC: 0x%08x\n", report.rr.ssrc));
        MLOG(( MLOG_INFO, 4231, "RTCP", "SSRC_1: 0x%08x\n", (UINT32)report.rr.rr[0].ssrc));
        MLOG(( MLOG_INFO, 4232, "RTCP", "Loss: %.2f, %d packets, jitter = %d, highest seq = 0x%08x\n", report.rr.rr[0].fraction/256.0, report.rr.rr[0].lost, report.rr.rr[0].jitter, report.rr.rr[0].last_seq));
        MLOG(( MLOG_INFO, 4233, "RTCP", "LSR = %d DLSR = %d\n", report.rr.rr[0].lsr, report.rr.rr[0].dlsr));
        MLOG(( MLOG_INFO, 4234, "RTCP", "------------------------------------------\n"));
    }

    if (report.sdes_updated)
    {
        MLOG(( MLOG_INFO, 4235, "RTCP", "RTCP Source Description Report:\n"));
        MLOG(( MLOG_INFO, 4236, "RTCP", "Source count: %d\n", report.sdes.header.count));
        for (unsigned int i=0; i<report.sdes.header.count; i++)
        {
            int j=0;
            MLOG(( MLOG_INFO, 4237, "RTCP", "Chunk %d: SSRC/CSRC: 0x%08x\n", i+1, report.sdes.chunk[i].src));
            while (report.sdes.chunk[i].item[j].type != RTCP_SDES_END)
            {
                MLOG(( MLOG_INFO, 4238, "RTCP", "Item #%d Type: %d, Length = %d\n", j+1, report.sdes.chunk[i].item[j].type, report.sdes.chunk[i].item[j].length));
                MLOG(( MLOG_INFO, 4239, "RTCP", "Item: %s\n", report.sdes.chunk[i].item[j].data));
                j++;
            }
        }
        MLOG(( MLOG_INFO, 4240, "RTCP", "------------------------------------------\n"));
    }

    if (report.bye_updated)
    {
        MLOG(( MLOG_INFO, 4241, "RTCP", "RTCP Goodbye Report:\n"));
        MLOG(( MLOG_INFO, 4242, "RTCP", "------------------------------------------\n"));
    }
#endif // defined(_DEBUG_RTP) & defined(_TRACE_RTCP_REPORTS)

#endif // _ENABLE_RTCP_REPORTS

    return TRUE;
}


// WriteControlPacket()
// Function: to combine all necessary control reports and construct a packet
//           to send via UDP
// Parameters affected: nil
// Return value: TRUE if succeed; FALSE otherwise
BOOL CMRTP::WriteControlPacket(MRTCPREPORTS& report)
{
    BOOL retval = FALSE;

#if _ENABLE_RTCP_REPORTS
    int wrote;
    MOCTET buffer[128], *pbuf;
    pbuf = buffer;      // default for little endian system (pbuf points to buffer)
    CMPacket rtcppacket(MAXPACKETSIZE);

    // construct rtcp combined packet

    // a SR or RR shall always be sent
    if (report.sr_updated)
    {
        // send Sender Report only
#ifdef _BIG_ENDIAN
        pbuf = (MOCTET*)&report.sr;
#elif defined(_LITTLE_ENDIAN)
        for (unsigned int j=0; j < (report.sr.header.length+1); j++)
            Mmemrse(buffer+sizeof(UINT32)*j, (MOCTET*)(&report.sr)+sizeof(UINT32)*j, sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
        rtcppacket.DataAppend(pbuf, (report.sr.header.length + 1)*sizeof(UINT32));
    }
    else
    {
        // send Receiver Report only
#ifdef _BIG_ENDIAN
        pbuf = (MOCTET*)&report.rr;
#elif defined(_LITTLE_ENDIAN)
        for (unsigned int j=0; j < (report.rr.header.length+1); j++)
            Mmemrse(buffer+sizeof(UINT32)*j, (MOCTET*)(&report.rr)+sizeof(UINT32)*j, sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
        rtcppacket.DataAppend(pbuf, (report.rr.header.length + 1)*sizeof(UINT32));
    }

    // send source decription report
    if (report.sdes_updated)
    {
#ifdef _BIG_ENDIAN
        pbuf = (MOCTET*)&report.sdes;
#elif defined(_LITTLE_ENDIAN)
        Mmemrse(buffer, (MOCTET*)(&report.sdes), sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
        rtcppacket.DataAppend(pbuf, sizeof(UINT32));

        for (unsigned int i=0; i < report.sdes.header.count; i++)
        {
#ifdef _BIG_ENDIAN
            pbuf = (MOCTET*)&report.sdes.chunk[i].src;
#elif defined(_LITTLE_ENDIAN)
            Mmemrse(buffer, (MOCTET*)(&report.sdes.chunk[i].src), sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
            rtcppacket.DataAppend(pbuf, sizeof(UINT32));

            int len = 0;
            unsigned int j=0, k;
            while (TRUE)
            {
                if (report.sdes.chunk[i].item[j].type == RTCP_SDES_END)
                {
                    for (k=0; k<((3-len)%sizeof(UINT32))+1; k++)
                        rtcppacket.DataAppend(0);
                    len += k;
                    break;
                }
                else
                {
                    rtcppacket.DataAppend(report.sdes.chunk[i].item[j].type);
                    rtcppacket.DataAppend(report.sdes.chunk[i].item[j].length);
                    rtcppacket.DataAppend((const unsigned char*)report.sdes.chunk[i].item[j].data, report.sdes.chunk[i].item[j].length);
                    len += 2 + report.sdes.chunk[i].item[j].length;
                }
                j++;
            }
        }
    }

    // send goodbye report
    if (report.bye_updated)
    {
        // do nothing
    }

    // send combined RTCP packet
    if (ControlDevice())
    {
        wrote = ControlDevice()->Write((unsigned char *) rtcppacket.Data(), rtcppacket.DataCount());

        if (wrote == rtcppacket.DataCount())
            retval = TRUE;
        else
        {
            MLOG(( MLOG_ERROR, 4243, "RTCP", "Wrote %d out of %d bytes.\n", wrote, rtcppacket.DataCount()));
            retval = FALSE;
        }
    }
    else
    {
        MLOG(( MLOG_DEBUGERROR, 4244, "RTCP", "ControlDevice() not created yet.\n"));
    }

    rtcppacket.Free();
#endif // _ENABLE_RTCP_REPORTS

    return retval;
}


// ReadControlPacket()
// Function: to receive a packet from UDP and separate it into appropriate
//           RTCP reports
// Input: current report
// Parameters affected: updated report
// Return value: TRUE if succeed; FALSE otherwise
BOOL CMRTP::ReadControlPacket(MRTCPREPORTS& report)
{
#if _ENABLE_RTCP_REPORTS
    CMPacket rtcppacket(MAXPACKETSIZE);

    // initialise report update status: nothing is updated
    report.sr_updated = report.rr_updated = report.sdes_updated = report.bye_updated = report.app_updated = FALSE;

    // read packet from network
    if (! ControlDevice())
        return -1;
    int i = ControlDevice()->Read((unsigned char *) rtcppacket.Data(), rtcppacket.MaxDataSize());

    if (i <= 0)
    {
        // nothing received
        rtcppacket.Free();
        return FALSE;
    }

    MOCTET  buffer[12], *pbuf;
    pbuf = (MOCTET*)rtcppacket.Data();      // pointer to beginning of each report
    int size;
    int total_size = 0;
    unsigned int j, k;
    int len;

#if defined(_DEBUG_RTP)
    MLOG(( MLOG_INFO, 4245, "RTCP", "Packet received with size = %d bytes\n", i));  // for debugging, added AW
    MLOG(( MLOG_INFO, 4246, "RTCP", "RTCP Packet first 16 bytes:\n"));
    MLOG(( MLOG_INFO, 4247, "RTCP", "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %2x\n", pbuf[0], pbuf[1], pbuf[2], pbuf[3],
                pbuf[4], pbuf[5], pbuf[6], pbuf[7], pbuf[8], pbuf[9], pbuf[10], pbuf[11], pbuf[12], pbuf[13], pbuf[14], pbuf[15]));
    MLOG(( MLOG_INFO, 4248, "RTCP", "------------------------------------------\n"));
#endif // defined(_DEBUG_RTP)

    while (total_size < i)
    {

#ifdef _BIG_ENDIAN
        MRTCPCOMMONHEADER *rtcp_hdr = (MRTCPCOMMONHEADER *) pbuf;
#elif defined(_LITTLE_ENDIAN)
        Mmemrse(buffer, pbuf, sizeof(UINT32));
        MRTCPCOMMONHEADER *rtcp_hdr = (MRTCPCOMMONHEADER *) buffer;
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif

        // check RTCP header validity
        if (rtcp_hdr->version == RTP_VERSION)
        {
            // determine report size in bytes
            size = (rtcp_hdr->length + 1)*sizeof(UINT32);

            switch (rtcp_hdr->pt)
            {
                case RTCP_SR_PAYLOAD_TYPE:  // sender report
                    if (report.sr_updated)
                    {
                        MLOG(( MLOG_WARNING, 4249, "RTCP", "Extra SR detected and updated.\n"));
                    }
                    if (report.rr_updated)
                    {
                        MLOG(( MLOG_WARNING, 4250, "RTCP", "SR detected but RR received before. Updated.\n"));
                    }

#ifdef _BIG_ENDIAN
                    memcpy(&report.sr, pbuf, size);
#elif defined(_LITTLE_ENDIAN)
                    for (j=0; j < (rtcp_hdr->length+1); j++)
                    {
                        Mmemrse((MOCTET*)(&report.sr)+sizeof(UINT32)*j, pbuf+sizeof(UINT32)*j, sizeof(UINT32));
                    }
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
                    report.sr_updated = TRUE;
                    break;
                case RTCP_RR_PAYLOAD_TYPE:  // reception report
                    if (report.rr_updated)
                    {
                        MLOG(( MLOG_WARNING, 4251, "RTCP", "Extra RR detected and updated.\n"));
                    }
                    if (report.sr_updated)
                    {
                        MLOG(( MLOG_WARNING, 4252, "RTCP", "RR detected but SR received before. Updated.\n"));
                    }

#ifdef _BIG_ENDIAN
                    memcpy(&report.rr, pbuf, size);
#elif defined(_LITTLE_ENDIAN)
                    for (j=0; j < (rtcp_hdr->length+1); j++)
                    {
                        Mmemrse((MOCTET*)(&report.rr)+sizeof(UINT32)*j, pbuf+sizeof(UINT32)*j, sizeof(UINT32));
                    }
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
                    if (report.rr.header.count)
                    {
                        memcpy(&report.sr.rr[0], &report.rr.rr[0], size - 2 * sizeof(UINT32));
                    }
                    report.rr_updated = TRUE;
                    break;
                case RTCP_SDES_PAYLOAD_TYPE:    // source description report
                    if (report.sdes_updated)
                    {
                        MLOG(( MLOG_WARNING, 4253, "RTCP", "Extra SDES detected and updated.\n"));
                    }
#ifdef _BIG_ENDIAN
                    memcpy(&report.sdes.header, pbuf, sizeof(UINT32));
#elif defined(_LITTLE_ENDIAN)
                    Mmemrse((MOCTET*)(&report.sdes.header), pbuf, sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif

                    len = sizeof(UINT32);
                    for (k=0; k < report.sdes.header.count; k++)
                    {
#ifdef _BIG_ENDIAN
                        memcpy(&report.sdes.chunk[k].src, pbuf+len, sizeof(UINT32));
#elif defined(_LITTLE_ENDIAN)
                        Mmemrse((MOCTET*)(&report.sdes.chunk[k].src), pbuf+len, sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
                        len += 4;

                        unsigned int j=0;
                        while (TRUE)
                        {
                            report.sdes.chunk[k].item[j].type = *(pbuf + (len++));
                            if (report.sdes.chunk[k].item[j].type == RTCP_SDES_END)
                            {
                                len += ((-len)%sizeof(UINT32));
                                break;
                            }
                            else
                            {
                                report.sdes.chunk[k].item[j].length = *(pbuf + (len++));
                                memset(report.sdes.chunk[k].item[j].data, 0, sizeof(report.sdes.chunk[k].item[j].data));
                                memcpy(report.sdes.chunk[k].item[j].data, pbuf + len, report.sdes.chunk[k].item[j].length);
                                len += report.sdes.chunk[k].item[j].length;
                            }
                            j++;
                        }
                    }
                    report.sdes_updated = TRUE;
                    break;
                case RTCP_BYE_PAYLOAD_TYPE: // bye report
                    if (report.bye_updated)
                    {
                        MLOG(( MLOG_WARNING, 4254, "RTCP", "Extra BYE detected and updated.\n"));
                    }
#ifdef _BIG_ENDIAN
                    memcpy(&report.bye, pbuf, size);
#elif defined(_LITTLE_ENDIAN)
                    for (j=0; j < (rtcp_hdr->length+1); j++)
                    {
                        Mmemrse((MOCTET*)(&report.bye)+sizeof(UINT32)*j, pbuf+sizeof(UINT32)*j, sizeof(UINT32));
                    }
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
                    report.bye_updated = TRUE;
                    break;
                case RTCP_APP_PAYLOAD_TYPE: // application report
                    MLOG(( MLOG_ERROR, 4255, "RTCP", "Unsupported APP RTCP packet detected. Discarded.\n"));
                    total_size = i;
                    size = 0;
                    break;
                default:
                    MLOG(( MLOG_ERROR, 4256, "RTCP", "Invalid RTCP packet received. Discarded.\n"));
                    total_size = i;
                    size = 0;
                    break;
            }
            // proceed to next RTCP packet
            total_size += size;
            pbuf += size;
        }
        else
        {
            MLOG(( MLOG_ERROR, 4257, "RTCP", "Invalid RTCP packet received. Discarded.\n"));
            break;
        }   // end if
    }   // end while

    rtcppacket.Free();
#endif // _ENABLE_RTCP_REPORTS

    return TRUE;
}


BOOL CMRTP::SendBye()
{
    return TRUE;
}


BOOL CMRTP::Open(CMUDPOptions & p_Media, CMUDPOptions & p_MediaControl)
{
    // remove previously connected data device and control device that may have been reserved
    Close();

    if ((rtp_dev = p_Media.CreateDevice()) == NULL || (rtcp_dev = p_MediaControl.CreateDevice()) == NULL)
    {
        Close();
        return FALSE;
    }

    WORD l_wLowPort, l_wHighPort;
    MGetIPPortNumberRange(l_wLowPort, l_wHighPort);

    unsigned l_wPort;
    for (l_wPort = ((l_wLowPort+1)&0xfffe); l_wPort < l_wHighPort; l_wPort += 2)
    {
        if (rtp_dev->ReservePort((WORD)l_wPort) && rtcp_dev->ReservePort((WORD)(l_wPort+1)))
        {
            break;
        }
    }

    if (l_wPort >= l_wHighPort)
    {
        Close();
        return FALSE;
    }

    sprintf(p_Media.port_id, "%u", l_wPort);
    ((CMUDP*)rtp_dev)->GetLocalAddress().GetAddress().AsString(p_Media.m_local_interface);
    sprintf(p_MediaControl.port_id, "%u", l_wPort+1);
    ((CMUDP*)rtcp_dev)->GetLocalAddress().GetAddress().AsString(p_MediaControl.m_local_interface);

    // The code below has to go at some point, allowing the user to specify a config
    // parameter and then promptly overriding it is really not a good thing to do.
    serial_number_window = (unsigned short) p_Media.packet_queue_length;
    if ((MediaType() == MG7231) || (MediaType() == MG711))
    {
        // all audio channels get shorter queue length with a max of 4 packets
        if (serial_number_window > 2)
        {
             serial_number_window = 2;
        }
    }

    return TRUE;
}


BOOL CMRTP::Listen(const char * p_szMediaControl)
{
    if (rtp_dev == NULL || rtcp_dev == NULL)
    {
        return FALSE;
    }

    if (!rtcp_dev->Call(p_szMediaControl))
    {
        return FALSE;
    }

    rx_thread = new CMThread;
    if (!rx_thread->CreateAndRun(ReceiveThread, this, MTHREAD_PRIORITY_TIME_CRITICAL, 0, "RTPR") )
    {
        MLOG(( MLOG_DEBUGERROR, 4263, "RTCP", "Could not create RTP receive thread.\n"));
        delete rx_thread;
        rx_thread = NULL;
    }

    return TRUE;
}


BOOL CMRTP::Connect(const char * p_szMedia)
{
    if (rtp_dev == NULL || rtcp_dev == NULL)
    {
        return FALSE;
    }

    if (!rtp_dev->Call(p_szMedia))
    {
        return FALSE;
    }

    return TRUE;
}


////////////////////////////////////////////////  CMRTPSSRC  /////////////////////////////////////////////////////

void CMRTPSSRC::Generate()
{
    struct {
        int     type;
        DWORD   cpu;
        int     pid;
    } s;
    static int type = 0;

    CMTime now;
    now.GetLocalTime();

    BOOL clashed = FALSE;
    while (TRUE)
    {
        s.type = type++;
        s.cpu = MCoreAPITime();;
        s.pid = MGetThreadId();

        ssrc = MGenMD32((unsigned char *) &s, sizeof(s));

        gs_RTPList.Lock();
        for (int i=0; i<gs_RTPList.Length(); i++)
        {
            CMRTP *rtp = (CMRTP *) gs_RTPList[i];
            if (ssrc == rtp->Ssrc())
            {
                clashed = TRUE;
                break;
            }
        }
        gs_RTPList.Release();

        if (!clashed)
        {
            break;    // ssrc determined when there is no other same ssrc.
        }
    }
}


CMRTPSSRC::CMRTPSSRC()
{
    Generate();
}


///////////////////////////////// Utilities ///////////////////////////////////

CMMutex *rtp_port_mutex=NULL;

void MRTPCleanup()
{
    CMMutex *rtp_port_mutex_to_delete;
    if (rtp_port_mutex)
    {
        rtp_port_mutex_to_delete = rtp_port_mutex;
        rtp_port_mutex = NULL;
        delete rtp_port_mutex_to_delete;
    }
}


// MUpdateInterarrivalJitter()
//   - to calculate and update the interarrival jitter value.
//   - Calculation is skipped when rtp_ts is the same as prev_rtp_ts and function returns FALSE.
//   Rtp_ts:           timestamp when sender sent current packet (specified in packet header)
//   Prev_rtp_ts:      timestamp when sender sent previous packet (marked in rtcp_status.
//                       Updated by rtp_ts after calculating the jitter)
//   Arrival_ts:       (local variable) timestamp when receiver received current packet.
//                       Assigned instantly by MMakeRTPTimeStamp().
//   Prev_arrival_ts:  timestamp when receiver received previous packet (marked in rtcp_status.
//                       Updated by arrival_ts after calculating the jitter)
//   Prev_jitter:      used to calculate the updated jitter and result stored in the same variable.
M_EXPORT BOOL MUpdateInterarrivalJitter(UINT32 rtp_ts, UINT32 &prev_rtp_ts, UINT32 &prev_arrival_ts, UINT32 &prev_jitter)
{
    // skip jitter calculation, as this can happen for consecutive video packets to have the same timestamp
    if (rtp_ts == prev_rtp_ts)
    {
        return FALSE;
    }

    int     arrival_ts = MMakeRTPTimeStamp();
    int     diff = abs( (arrival_ts - rtp_ts) - (int)(prev_arrival_ts - prev_rtp_ts) );
    int     jitter = (int)prev_jitter + ((diff - (int)prev_jitter)>>4);

    // update parameters
    prev_rtp_ts = rtp_ts;
    prev_arrival_ts = arrival_ts;
    prev_jitter = (UINT32)jitter;

    return TRUE;
}


M_EXPORT UINT32 MMakeRTPTimeStamp()
{
    return (MCoreAPITime() - MCoreAPIReferenceTime());
}


M_EXPORT UINT32 MNTPToRTPTimeStamp(UINT64 ntp_ts)
{
    return (UINT32)(ntp_ts >> 16);
}

#endif // INCLUDE_RTP
