/*
 *
 * Author:	Aumtech, inc.
 *
 */

#include "arc_video_packet.hpp"


unsigned long arc_video_packet::MCoreAPITime()
{
	unsigned long t=0;
	struct timeval tm;
	int         st;
	st = gettimeofday(&tm, NULL);
	if (! st) 
	{   //success
		t = (unsigned long) (tm.tv_sec*1000 + tm.tv_usec/1000);    // in millisec
	} else 
	{
		t = 0;
	}
	// for Linux RedHat 6.0, it is unknown why t was only 0.1 nanosec.
	return t;
}
																						 


/////////////////////////////////////////////////////////////////////////////

// Msrand()
//   To set the seed for random number generation
void arc_video_packet::Msrand()
{
   // use MCoreAPITime() as time() was returning very
	// close numbers under linux.
	unsigned int t = MCoreAPITime();
	srand(t);
}
					 


// MGenerateRandomNumber()
//   To generate a random number
// moved from CMRTP::GenerateRandomSequenceNumber()
// extracted as an independent MGenerateSequenceNumber()
USHORT arc_video_packet::MGenerateRandomNumber()
{
	static bool l_zFirstTime = true;
	USHORT      l_uiRandNum = 0;

	if (l_zFirstTime)
	{
		Msrand();
		l_zFirstTime = false;
	}

	l_uiRandNum = (USHORT)rand();

	return l_uiRandNum;
}
																				


arc_video_packet::arc_video_packet(int sn)
{
	//cout<<"RGDEBUG::inside arc_video_packet"<<endl;
	packet = NULL;
	initialise(sn);
}


int arc_video_packet::initialise( int sn)
{


	//serial_number_to_send = MGenerateRandomNumber();
	
	serial_number_to_send = sn;
	Generate();
// construct RTP packets for video bit stream
	if (packet)
	{
		packet->Free();
		delete packet;
		packet = NULL;
	}
	packet = new CMRTPPacket(VIDEO_PACKET_BUFFER_SIZE);
	if (!packet)
	{
		return FALSE;
	}
	CMRTPPacket *rtppacket = (CMRTPPacket *)packet;
	
	
// Initialise fixed header, elw
	memset(&(rtppacket->Header()), 0, sizeof(MRTPHEADER));

// protocol version
	rtppacket->Header().version = RTP_VERSION;
// marker bit
	rtppacket->Header().m = 1;
// payload type
	rtppacket->Header().pt = RTP_H263_PAYLOAD_TYPE;
	rtppacket->Header().ts = 0;               /* timestamp */
	int szFixedHdr = sizeof(rtppacket->Header()) + sizeof(UINT32)*(rtppacket->Header().cc - 1); // size of RTP fixed header for H.263


	//cout<<"RGDEBUG::szFixedHdr ="<<szFixedHdr<<endl;

	
	rtppacket->SetFixedHdrSize(szFixedHdr);

// set media_type
	rtppacket->SetMediaType(MH263);

// Initialise payload header
	MRTPH263PAYLOADHEADER *h263hdr = (MRTPH263PAYLOADHEADER *) &(rtppacket->PayloadHeader());

	memset(h263hdr, 0, sizeof(MRTPPAYLOADHEADER));
// assuming mode C for max possible payload header size
	h263hdr->modeC.f = 1;
	h263hdr->modeC.p = 1;
	rtppacket->SetHeaders(&(rtppacket->Header()), h263hdr);
	rtppacket->ConfigPayload(0);
	rtppacket->SetMediaType(MH263);

	uiPrev_bitrate=0;        // in bits per 100 second
	uiPrev_frame_rate=0;     // in frames per 100 second
	iDiff_bitrate=0;        // in bits per 100 second
	iDiff_frame_rate=0;     // in frames per 100 second
	uiNext_frame_to_check_rate=0;
	l_iEncodeErrors = 0;

	qp = 0;     // current qp
	prev_intra = FALSE;
	intra = TRUE;      // trace mode of frame to be encoded.
	codesz = 0;
	l_bitstreamChunkSize = 0;
	l_iBitRate = 128000;

		
	return 0;

}


int arc_video_packet::packetize(void *buf, int buf_len, unsigned int ts)
{

	CMRTPPacket *rp = (CMRTPPacket *)packet;
	
	rp->setRTPBuffer( *(MOCTET *)buf);

	//cout<<"RGDEBUG::inside packetize, calling Segmentalise"<<endl;
	codesz = rp->Segmentalise(buf_len);
#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/after_seg_packet_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(rp->BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) rp->Buffer(), 1, rp->BufferCount(), fp);
		fclose(fp);
	}
}
#endif
	//cout<<"RGDEBUG::after segmentalise codesz="<<codesz<<endl;
	
	
	
	if (codesz > 0)
	{
		int wrote;
		int ret;


		//printf("RGDEBUG::inside Write BufferCount of ts=%d\n", ts);
		//printf("RGDEBUG::inside Write BufferCount of serial_number_to_send=%d\n",serial_number_to_send);
		//printf("RGDEBUG::inside Write BufferCount of ssrc=%d\n",UINT(ssrc));
				

//		rp->Header().ts += m_base_timestamp;             // add random timestamp offset
		rp->Header().ts = ts;             // add random timestamp offset
		rp->Header().seq = serial_number_to_send++;    // packet header may be rewritten every time without knowing SSRC

		rp->Header().pt = RTP_H263_PAYLOAD_TYPE;
		rp->Header().ssrc = UINT32(ssrc);
		rp->SetMediaType(MH263);

		// actual timestamp value has been assigned in TxRTP
#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/beforepacketise_packet_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(rp->BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) rp->Buffer(), 1, rp->BufferCount(), fp);
		fclose(fp);
	}
}
#endif
		rp->Packetise(0);

		//cout<<"RGDEBUG:: Buffer count after Packetise="<<rp->BufferCount()<<endl;
		
#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/after_packetise_packet_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(rp->BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) rp->Buffer(), 1, rp->BufferCount(), fp);
		fclose(fp);
	}
}
#endif


		buf = rp->Buffer();
		return rp->BufferCount();

	}
//	((CMRTPPacket *)packet)->ShallowCopy(*rp);	
	buf = rp->Buffer();
	return rp->BufferCount();
}




void arc_video_packet::Generate()
{
    struct {
        int     type;
        DWORD   cpu;
        int     pid;
    } s;
    static int type = 0;

//    CMTime now;
//    now.GetLocalTime();

    BOOL clashed = FALSE;
//    while (TRUE)
    {
        s.type = type++;
        s.cpu = MCoreAPITime();;
        s.pid = (unsigned int) pthread_self();

        ssrc = MGenMD32((unsigned char *) &s, sizeof(s));
#if 0
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
#endif

    }
}


unsigned long arc_video_packet::MGenMD32(
			unsigned char * string, // concatenated "randomness values"
			long len            // size of randomness values
		)
{
	union
	{
		char c[16];
		unsigned long x[4];
	} digest;
	int i;
	unsigned long r=0;
	MD5_CTX context;

	MD5Init (&context);
	MD5Update (&context, string, len);
	MD5Final ((unsigned char *) & digest, &context);

	for (i = 0; i < 3; i++)
	{
		r ^= digest.x[i];
	}

	return r;
};
																													 
