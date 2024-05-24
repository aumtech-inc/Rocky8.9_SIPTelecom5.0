/***************************************************************************
        File:           $Archive: /stacks/include/mrtp.h $
        Revision:       $Revision: 1.1 $
        Date:           $Date: 2006/04/10 16:24:42 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       RTP class definition
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
#ifndef __MRTP_H__
#define __MRTP_H__

/** @file mrtp.h    */

#include "mudp.h"
#include "mrtppacket.h"
#include "mmuxoptions.h"
#include "mediaoptions.h"


class M_IMPORT CMRTPSSRC
{
private:
    UINT32  ssrc;
public:
    CMRTPSSRC();
    CMRTPSSRC(UINT32 val){ssrc = val;}
    CMRTPSSRC(CMRTPSSRC& s){ssrc = s.ssrc;}
    ~CMRTPSSRC(){}
    friend int operator==(CMRTPSSRC &s1, CMRTPSSRC &s2){if(s1.ssrc == s2.ssrc) return 1; else   return 0;}
    void Generate();
    operator UINT32() {return ssrc;}
};

#if defined(_PTS)
class CMRTP;

class M_IMPORT CMRTPPort : public CMUDP
{
protected:
    CMRTP*   m_Rtp;

public:
    CMRTPPort(CMRTP* p_pRtp, const char * p_pLocal_Interface = NULL) : CMUDP(p_pLocal_Interface)
                                { m_Rtp = p_pRtp; Id(MRTP); }
    CMRTPPort()                 { ; }
    CMRTP*   Rtp()              { return m_Rtp; }
};
#endif // _PTS


typedef CMList<CMRTPPacket> CMRTPPacketList;


class M_IMPORT CMRTP : public CMObject
{
private:
    //data
    BOOL            m_first_read;           // flag to synchronise interrupted packet read sequence
    BOOL            m_first_write;          // flag for assigning timestamp offset
                                            // independent for Read() and Write()
    BOOL            m_zStartWithQueues;     // flag to indicate next read packet is from queues
    UINT32          m_base_timestamp;       // random RTP timestamp offset
protected:
    // data
    CMCommDevice    *rtp_dev;
    CMCommDevice    *rtcp_dev;
    CMRTP           *reverse_rtp;           // reverse RTP channel
    CMRTPSSRC       ssrc;                   // sync source
                                            // (held xxx ignore this comment)Note: ssrc value is only valid in tx channel
                                            //   rx channel shall refer the ssrc from its tx channel partner
    CMRTPSSRC       prev_ssrc;              // SSRC sent/received in the previous RTP packet
    MOBJECTID       media_type;
    MMEDIA_DIRECTIONS direction;
    MTERMINALLABEL  destination;            // destination to communicate with
    USHORT          serial_number_window;   // window of serial number to look back in retransmission
    USHORT          serial_number_modulo;   // modulo of serial number, either 0xff or 0xffff
    USHORT          v;                      // index of expected serial number of next packet
    USHORT          serial_number_to_read;  // serial number of next packet to read
    USHORT          serial_number_to_send;  // serial number of current packet to send
    CMRTPPacketList m_in_seq_packets;     // in-sequence packets. that is good packets
    CMRTPPacketList m_out_of_seq_packets; // out-of-sequence packets that are in random order
    CMRTPPacketList m_receive_packets;    // received packets ready to be retrieved by users
    CMMutex         pkt_queue_access;       // access mutex to all packet queues
    CMThread        *rx_thread;             // RTP packet receive thread

    // RTCP variables
    MRTCPREPORTS    report;                 // rtcp report container
    CMMutex         rtcp_access;            // access to common RTCP statistics data
    MRTCPSTATUS     rtcp_status;            // RTCP statistics

protected:
    // methods
    static  void ReceiveThread(void* arg);
    virtual void ReceiveThread();

    inline BOOL                     PacketsLock() {return pkt_queue_access.Lock();}
    inline BOOL                     PacketsRelease() {return pkt_queue_access.Release();}
    static USHORT                   GenerateRandomSequenceNumber();
public:
#if defined(_PTS)
    CMRTP(const char * p_pLocal_Interface = NULL);
#else // !_PTS
    CMRTP();
#endif // _PTS
    ~CMRTP();
    virtual BOOL                    Create(MOBJECTID type, MMEDIA_DIRECTIONS channel_dir, MTERMINALLABEL dest);
    virtual BOOL                    Close();
    virtual int                     Write(CMBuffer &packet);
    virtual int                     Read(CMBuffer &packet, unsigned long waittime=MINFINITE);
    virtual int                     ReadPacket(CMBuffer &packet);
    virtual BOOL                    SendReport();
    virtual BOOL                    ReceiveReport();
    virtual BOOL                    WriteControlPacket(MRTCPREPORTS& report);
    virtual BOOL                    ReadControlPacket(MRTCPREPORTS& report);
    virtual BOOL                    SendBye();
    virtual UINT32                  Ssrc(){ return UINT32(ssrc); }
    virtual int                     MediaType(){return media_type;}
    virtual int                     ChannelDirection(){return direction;}
    virtual MTERMINALLABEL          Destination(){return destination;}
    virtual UINT32                  BaseTimeStamp(){return m_base_timestamp;}
    virtual MRTCPSTATUS &           RTCPStatus(){return rtcp_status;}
    virtual CMRTP*                  ReverseRTP(){return reverse_rtp;}
    virtual void                    ReverseRTP(CMRTP* rrtp){ reverse_rtp = rrtp; }
    virtual CMCommDevice*           DataDevice() const {return rtp_dev;}
    virtual CMCommDevice*           ControlDevice() const {return rtcp_dev;}
    virtual BOOL                    Open(CMUDPOptions & p_Media, CMUDPOptions & p_MediaControl);
    virtual BOOL                    Listen(const char * p_szMediaControl);
    virtual BOOL                    Connect(const char * p_szMedia);

    void                            MoveFromOSQ2ISQ(USHORT p_usNextSN);
    int                             ProcessISQ(CMRTPPacket *p_pRtpPkt);
    int                             ProcessOSQ(CMRTPPacket * p_pRtpPkt);
    int                             GetFromISQ(CMRTPPacket *p_pRtpPkt);
    int                             GetFromFirstRead(CMRTPPacket *p_pRtpPkt);
    void                            RTCPUpdateSN1(CMRTPPacket *p_pRtpPkt);
    void                            RTCPUpdateSN2(CMRTPPacket *p_pRtpPkt);
    void                            RTCPUpdateSN3(CMRTPPacket *p_pRtpPkt);
    void                            RTCPUpdateSN4(CMRTPPacket *p_pRtpPkt);
    void                            RTCPUpdateSN5(CMRTPPacket *p_pRtpPkt);
    void                            RTCPUpdateSN6(CMRTPPacket *p_pRtpPkt);
    void                            RTCPIncrementPacketCount();
};

M_IMPORT BOOL MUpdateInterarrivalJitter(UINT32 rtp_ts, UINT32 &prev_rtp_ts, UINT32 &prev_arrival_ts, UINT32 &prev_jitter);
M_IMPORT UINT32 MMakeRTPTimeStamp();
M_IMPORT UINT32 MNTPToRTPTimeStamp(UINT64 ntp_ts);
M_IMPORT void MRTPCleanup();

#endif // !__MRTP_H__
