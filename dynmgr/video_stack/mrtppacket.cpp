/* Listed below are the RTP Payload formats that stacks codecs support.

        Codec   |  IETF RFC |      RFC Title
       _________|___________|______________________________________________
        H.261   |   2032    |   RTP Payload Format for H.261 Video Streams.
       _________|___________|______________________________________________
        H.263   |   2190    |   RTP Payload Format for H.263 Video Streams.
       _________|___________|______________________________________________
 MPEG-4 Video   |   N/A     |   N/A
       _________|___________|______________________________________________
        G.711   |   3551    |   RTP Profile for Audio and Video Conferences
                |           |   with Minimal Control.
                |   3389    |   Real-time Transport Protocol (RTP) Payload
                |           |   for Comfort Noise.
       _________|___________|______________________________________________
        G.723.1 |   3551    |   RTP Profile for Audio and Video Conferences
                |           |   with Minimal Control.
       _________|___________|______________________________________________
        GSM-AMR |   3267    |   Real-Time Transport Protocol (RTP) Payload
                |           |   Format and File Storage Format for the
                |           |   Adaptive Multi-Rate (AMR) and Adaptive
                |           |   Multi-Rate Wideband (AMR-WB) Audio Codecs.
       _________|___________|______________________________________________
*/

//#include "mrtpglobal.h"
#include "mrtppacket.h"
#include "mutils.h"


// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
  #if !defined (MEMCHECK)
    #define new DEBUG_NEW
  #endif // !MEMCHECK
  #undef THIS_FILE
  static char THIS_FILE[] = __FILE__;
#endif

//#define _LITTLE_ENDIAN 

#define MIN_BLOCK 32

// Text string of RTP payload type
static const struct
{
    unsigned int    l_uiRtpPayloadTypeIndex;
    const char      *l_pccRtpPayloadTypeName;
} gs_RtpPayloadType[] =
{
    {RTP_PCMU_PAYLOAD_TYPE, "PCMU"},
    {RTP_PCMA_PAYLOAD_TYPE, "PCMA"},
    {RTP_G722_PAYLOAD_TYPE, "G.722"},
    {RTP_G723_PAYLOAD_TYPE, "G.723"},
    {RTP_G728_PAYLOAD_TYPE, "G.728"},
    {RTP_G729_PAYLOAD_TYPE, "G.729"},
    {RTP_GSMAMR_PAYLOAD_TYPE, "GSMAMR"},
    {RTP_H261_PAYLOAD_TYPE, "H.261"},
    {RTP_H263_PAYLOAD_TYPE, "H.263"},
    {RTP_MP4VIDEO_PAYLOAD_TYPE, "MP4VIDEO"}
};

// Note: fixed header extension (X always set to 0) is not supported in this version
// Note: max data packet size includes the sizes of RTP fixed header and payload header

CMRTPPacket::CMRTPPacket(int sz) : CMPacket(sz)
{
    memset(&hdr, 0, sizeof(hdr));
    memset(&plhdr, 0, sizeof(plhdr));

    if (sz > 0)
    {
        buffer = new MOCTET[sz];
    }
    else
    {
        buffer = NULL;
    }
    buffer_count = 0;

    szFixedHdr = sizeof(hdr);
    szPayloadHdr = 0;

#ifdef _DEBUG
    isARtpPacket = TRUE;
#endif //_DEBUG
}


CMRTPPacket::~CMRTPPacket()     // we do not free buffer memory
{
}


BOOL CMRTPPacket::DataMakeSpace(int n)
{
    int inc;
    int sp = MaxDataSize() - DataCount();
    if (n <= sp)
    {
        return TRUE;
    }
    inc = n - sp;

    if (!CMPacket::DataMakeSpace(n))
    {
        return FALSE;  // memory allocation error
    }

    MOCTET *b;
    int new_size = max_data_size + max(MIN_BLOCK, inc);
    int count = BufferCount();

    b = new MOCTET [new_size];
    if (! b)
    {
        return FALSE;
    }

    if (buffer)
    {
        memcpy(b, buffer, count * sizeof(MOCTET));
        delete[] buffer;
    }
    buffer = b;
    return TRUE;
}



void CMRTPPacket::SetHeaders(void *header, void *plheader)
{
    MRTPHEADER *ph = (MRTPHEADER *) header;
    // copy headers
    memcpy(&hdr, ph, sizeof(MRTPHEADER) + (ph->cc - 1)*sizeof(UINT32));
    memcpy(&plhdr, plheader, sizeof(MRTPPAYLOADHEADER));


#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/setheader_hdr_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) &hdr, 1, (sizeof(MRTPHEADER) + (ph->cc - 1)*sizeof(UINT32)), fp);
		fclose(fp);
	}
}
#endif


#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/setheader_plhdr_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) &plhdr, 1, sizeof(MRTPPAYLOADHEADER), fp);
		fclose(fp);
	}
}
#endif



}



void CMRTPPacket::GetHeaders(void *header, void *plheader)
{
    // copy headers
    memcpy(header, &hdr, szFixedHdr);
    memcpy(plheader, &plhdr, sizeof(MRTPPAYLOADHEADER));
}



void CMRTPPacket::ConfigPayload(int ts)
{
    // determine RTP fixed and payload header sizes
    switch(media_type)
    {
#if defined(INCLUDE_G711)
        case MG711:
        case MG711ALAW64K:
        case MG711ALAW56K:
        case MG711ULAW64K:
        case MG711ULAW56K:
            // determine fixed header size
            szFixedHdr = sizeof(MRTPHEADER) + (hdr.cc - 1)*sizeof(UINT32);
            // determine payload header size
            szPayloadHdr = RTP_G711_HEADER_SIZE;
            break;
#endif // INCLUDE_G711
#if defined(INCLUDE_G7231)
        case MG7231:
        case MG7231_ANNEXC:
            // determine fixed header size
            szFixedHdr = sizeof(MRTPHEADER) + (hdr.cc - 1)*sizeof(UINT32);
            // determine payload header size
            szPayloadHdr = RTP_G723_HEADER_SIZE;
            break;
#endif // INCLUDE_G7231
#if defined(INCLUDE_GSMAMR)
        case MGSMAMR:
            // determine fixed header size
            szFixedHdr = sizeof(MRTPHEADER) + (hdr.cc - 1)*sizeof(UINT32);
            // determine payload header size
            szPayloadHdr = RTP_GSMAMR_HEADER_SIZE;
            break;
#endif // INCLUDEGSMAMR
#if defined(INCLUDE_H261)
        case MH261:
            // determine fixed header size
            szFixedHdr = sizeof(MRTPHEADER) + (hdr.cc - 1)*sizeof(UINT32);
            // determine payload header size
            szPayloadHdr = RTP_H261_HEADER_SIZE;
            break;
#endif // INCLUDE_H261
#if defined(INCLUDE_H263)
        case MH263:
            {
                // determine fixed header size
                MRTPH263PAYLOADHEADER *h263hdr = (MRTPH263PAYLOADHEADER *) &plhdr;
                szFixedHdr = sizeof(MRTPHEADER) + (hdr.cc - 1)*sizeof(UINT32);
                // determine payload header size
                if (!h263hdr->modeA.f)
                    szPayloadHdr = RTP_H263_MODEA_HEADER_SIZE;
                else if (!h263hdr->modeB.p)
                    szPayloadHdr = RTP_H263_MODEB_HEADER_SIZE;
                else
                    szPayloadHdr = RTP_H263_MODEC_HEADER_SIZE;
                break;
            }
#endif // INCLUDE_H263
#if defined(INCLUDE_MPEG4VIDEO)
        case MMPEG4VIDEO:
            // determine fixed header size
            szFixedHdr = sizeof(MRTPHEADER) + (hdr.cc - 1)*sizeof(UINT32);
            // determine payload header size
            szPayloadHdr = RTP_MP4VIDEO_HEADER_SIZE;
            break;
#endif // INCLUDE_MPEG4VIDEO
        default:
            break;
    }

	// determine maximum payload data size
	max_payload_size = max_data_size - szFixedHdr - szPayloadHdr;
	if (max_payload_size <= 0)
	{
        //MLOG(( MLOG_ERROR, 3823, "RTP Packet", "Max_payload_size <= 0.\n"));
	}

	// If being told the timestamp, then set it
	if (ts != 0)
		hdr.ts = ts;
}


void CMRTPPacket::Packetise(int ts)
{
    int j;

    // determine RTP fixed and payload header sizes
    ConfigPayload(ts);


#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/hdr_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) &hdr, 1, szFixedHdr, fp);
		fclose(fp);
	}
}
#endif


#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/plhdr_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) &plhdr, 1, szPayloadHdr, fp);
		fclose(fp);
	}
}
#endif




    // concatenate complete packet
#ifdef _BIG_ENDIAN
    memcpy(buffer, &hdr, szFixedHdr);                   // RTP fixed header
    memcpy(buffer+szFixedHdr, &plhdr, szPayloadHdr);    // RTP payload header
#elif defined(_LITTLE_ENDIAN)
    // little endian procedures
    // for RTP fixed header
    // the first 16 bits are in correct position, following 16 bits should be reversed
    for (j=0; j < (szFixedHdr+3)/4; j++)
        Mmemrse(buffer+sizeof(UINT32)*j, (unsigned char*)(&hdr)+sizeof(UINT32)*j, sizeof(UINT32));

    // for RTP payload header
    // the whole structure should be reversed in groups of every 32 bits
    for (j=0; j < (szPayloadHdr+3)/4; j++)
        Mmemrse(buffer+szFixedHdr+sizeof(UINT32)*j, (unsigned char*)(&plhdr)+sizeof(UINT32)*j, sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif



#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/before_memcpy_packet_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) buffer, 1, BufferCount(), fp);
		fclose(fp);
	}
}
#endif

#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/data_packet_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) Data(), 1, DataCount(), fp);
		fclose(fp);
	}
}
#endif



    memcpy(buffer+szFixedHdr+szPayloadHdr, Data(), DataCount());


#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/after_memcpy_packet_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) buffer, 1, BufferCount(), fp);
		fclose(fp);
	}
}
#endif


    buffer_count = szFixedHdr + szPayloadHdr + DataCount();
}



// return total number of bytes for RTP payload data
int CMRTPPacket::Segmentalise(int inbytes)
{
    int j;
    buffer_count = inbytes;


#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/before_seg_hdr_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) &hdr, 1, szFixedHdr, fp);
		fclose(fp);
	}
}
#endif


#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/before_seg_plhdr_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) &plhdr, 1, szPayloadHdr, fp);
		fclose(fp);
	}
}
#endif




#ifdef _BIG_ENDIAN
    memcpy(&hdr, buffer, sizeof(UINT32));     // RTP fixed header
#elif defined(_LITTLE_ENDIAN)
    // little endian procedures
    // for RTP fixed header, rearrange first 32-bit row 1st
    Mmemrse((unsigned char*)(&hdr), buffer, sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif

    // extract RTP fixed header size
    szFixedHdr = sizeof(MRTPHEADER) + (hdr.cc - 1)*sizeof(UINT32);

	//cout<<"RGDEBUG::szFixedHdr="<<szFixedHdr<<" buffer_count="<<buffer_count<<endl;

    if (szFixedHdr>buffer_count)
    {
	 	  //cout<<"RGDEBUG:: returning 0 from Segmentalise 1"<<endl;
        return 0; // Protects against invalid cc value
    }

#ifdef _BIG_ENDIAN
    memcpy(&plhdr, buffer+szFixedHdr, sizeof(UINT32));    // RTP payload header
#elif defined(_LITTLE_ENDIAN)
    // little endian procedures
    // for RTP payload header, rearrange first 32-bit row 1st
    Mmemrse((unsigned char*)(&plhdr), buffer+szFixedHdr, sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif

                // extract RTP H.263 payload header size
                MRTPH263PAYLOADHEADER *h263hdr = (MRTPH263PAYLOADHEADER *) &plhdr;
                if (!h263hdr->modeA.f)
				{
					//printf("RGDEBUG modeA header \n");fflush(stdout);
					 		
                    szPayloadHdr = RTP_H263_MODEA_HEADER_SIZE;
				}
				else if (!h263hdr->modeB.p)
				{
					//printf("RGDEBUG modeB header \n");fflush(stdout);
                    szPayloadHdr = RTP_H263_MODEB_HEADER_SIZE;
				}
				else
				{
					//printf("RGDEBUG modeC header \n");fflush(stdout);
					szPayloadHdr = RTP_H263_MODEC_HEADER_SIZE;
				}

    // determine RTP payload data size
    int count = buffer_count - szFixedHdr - szPayloadHdr;
    if (count<=0)
    {
        char *l_pcPayloadName = "unlisted RTP payload type";
        for (int i = 0; i < (int)(sizeof (gs_RtpPayloadType) / sizeof(gs_RtpPayloadType[0])); i++)
        {
            if (hdr.pt == gs_RtpPayloadType[i].l_uiRtpPayloadTypeIndex)
            {
                l_pcPayloadName = (char*)gs_RtpPayloadType[i].l_pccRtpPayloadTypeName;
                break;
            }
        }
        count = 0;
        DataCount(count);
//	 	printf("RGDEBUG:: returning count=%d, from Segmentalise 3\n",count);fflush(stdout);
        return count; //prevents corruption if Fixed Header is invalid
    }

//	printf("RGDEBUG Calling DataCount \n");fflush(stdout);
    DataCount(count);

    // separate headers and data
#ifdef _BIG_ENDIAN
    memcpy(&hdr, pfh, szFixedHdr);
    memcpy(&plhdr, buffer+szFixedHdr, szPayloadHdr);    // RTP payload header
#elif defined(_LITTLE_ENDIAN)
    // little endian procedures
    // for RTP fixed header
    for (j=0; j < (szFixedHdr+3)/4; j++)
        Mmemrse((unsigned char*)(&hdr)+sizeof(UINT32)*j, buffer+sizeof(UINT32)*j, sizeof(UINT32));
    // for RTP payload header
    // the whole structure should be reversed in groups of every 32 bits
    for (j=0; j < (szPayloadHdr+3)/4; j++)
        Mmemrse((unsigned char*)(&plhdr)+sizeof(UINT32)*j, buffer+szFixedHdr+sizeof(UINT32)*j, sizeof(UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif



#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/after_seg_hdr_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) &hdr, 1, szFixedHdr, fp);
		fclose(fp);
	}
}
#endif


#if 0
{
	FILE *fp;
	static int i = 0;
	char filename[64] = "";
	sprintf(filename, "/tmp/video_packets/after_seg_plhdr_%d", i);
	//printf("RGDEBUG:: filename=%s\n", filename);fflush(stdout);
	if(BufferCount() == 1399)
	{
		//printf("RGDEBUG:: going to write to the file \n");fflush(stdout);
		i++;
		fp = fopen(filename, "w+");
		if(fp == NULL)
		{
			//printf("RGDEBUG:: going to write to the file filename=%s \n", filename);fflush(stdout);
		}
		fwrite((unsigned char *) &plhdr, 1, szPayloadHdr, fp);
		fclose(fp);
	}
}
#endif





	// extract RTP payload data
    memcpy(base, buffer+szFixedHdr+szPayloadHdr, DataCount());

//	printf("RGDEBUG:: returning DataCount=%d, from Segmentalise 4", DataCount());fflush(stdout);
	return DataCount();
}


void CMRTPPacket::ShallowCopy(CMRTPPacket & p_Packet)
{
    // Clean up memory for our local data
    Free();

    // Copy all of the fields including the pointers to the source objects data
    hdr = p_Packet.hdr;
    plhdr = p_Packet.plhdr;
    szFixedHdr = p_Packet.szFixedHdr;
    szPayloadHdr = p_Packet.szPayloadHdr;
    media_type = p_Packet.media_type;

    buffer = p_Packet.buffer;
    buffer_count = p_Packet.buffer_count;
    max_payload_size = p_Packet.max_payload_size;
    error = p_Packet.error;

    base = p_Packet.base;
    cur_ptr = p_Packet.cur_ptr;
    max_data_size = p_Packet.max_data_size;

    // Assure source does NOT delete the data by nulling them out
    p_Packet.base = NULL;
    p_Packet.buffer = NULL;
}

void CMRTPPacket::setRTPBuffer(MOCTET &buf)
{
	buffer = &buf;
}



const unsigned char* CMRTPPacket::Mmemrse(unsigned char* dest, const unsigned char *src, int n)
{
	unsigned char *d=dest;

	//_ASSERTE(src!=NULL);
	//_ASSERTE(dest!=NULL);

	if ((!src) || (!dest)) 
	{
		return NULL;
	}
	for(int i=n-1; i >=0; i--) 
	{
		//cout<<"RGDEBUG::src["<<i<<"]="<<src[i]<<endl;
		*dest++ = src[i];
	}
	return d;
}
