#include "mrtppacket.h"
#include "md5.h"
//#include <iostream.h>


//using namespace std;

#define VIDEO_PACKET_BUFFER_SIZE     5000




class arc_video_packet
{
	public :
		arc_video_packet(int sn);
		MOCTET *get_buffer()
		{
			return packet->Buffer();
		}
		int packetize(void *buf, int buf_len, UINT32 ts);



	private :
		BOOL            code_intra;
		CMPacket        *packet;
		UINT32          m_base_timestamp ;
		UINT32  ssrc;
		USHORT          serial_number_to_send;  // serial number of current packet to send
		
		int initialise(int sn);
		int Write(CMBuffer &pkt);
		void Msrand();
		unsigned long MCoreAPITime();
		USHORT MGenerateRandomNumber();
		void Generate();	
		unsigned long arc_video_packet::MGenMD32(
				unsigned char * string, // concatenated "randomness values"
				long len            // size of randomness values
				);
								

int i;

long l_lStartTime;
long l_lCurrentTime;
long l_lElapsedTime;
unsigned int uiCurr_bitrate;          // in bits per 100 second
unsigned int uiCurr_frame_rate;       // in frames per 100 second
unsigned int uiPrev_bitrate;        // in bits per 100 second
unsigned int uiPrev_frame_rate;     // in frames per 100 second
unsigned int uiFinal_bitrate;          // in bits per 100 second
unsigned int uiLocal_target_bitrate;    // in bits per second
unsigned int uiIncrement_bitrate;
int iDiff_bitrate;        // in bits per 100 second
int iDiff_frame_rate;     // in frames per 100 second
unsigned int uiNext_frame_to_check_rate;
DWORD dwSleep_time;
int l_iEncodeErrors;





	int qp;     // current qp
	BOOL prev_intra;
	BOOL intra;      // trace mode of frame to be encoded.
    int              codesz;
    int              l_bitstreamChunkSize;
    int              l_iBitRate;

	DWORD prevtime; 
	DWORD expecttime; 
	DWORD dprd;  // for periodic timer fine-tuning













};


