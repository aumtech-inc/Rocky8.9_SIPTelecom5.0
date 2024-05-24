#ifndef ARC_SIP_COMMON_DOT_H
#define ARC_SIP_COMMON_DOT_H

/*------------------------------------------------------------------------------
Program Name:	ArcSipCallMgr/ArcIPDynMgr
File Name:		ArcSipCommon.h
Purpose:  		Common #includes for SIP/2.0 Call Manager and Media manager
Author:			Aumtech, inc.
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
Update: 07/07/2004 DDN Created the file
Update: 10/12/2005 Added the variable gSipRedirection for Redirection Manager 
Update: 09/12/2007 djb Added mrcpTTS items.
Update: 06/22/2010 djb Added blastDial items
Update: 03/30/2016 djb MR 4558 - removed gWriteBLegCDR
Update: 10/18/2016 djb MR 4605/4642 Added alreadyClosed 
------------------------------------------------------------------------------*/

/*System header files*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <wchar.h>
#include <ctype.h>
#include <sched.h>
#include <iostream>
#include <time.h>

#include <stdint.h>
//#include "/home/devmaul/ddnsrc/dmalloc-5.5.2/dmalloc.h"

/*END: System header files*/

/*Third party header files*/

#include <eXosip2/eXosip.h>


#include <ortp.h>
#include <rtpsession.h>
#include <telephonyevents.h>
#include <payloadtype.h>
//#include <glib.h>

#include <arc_tones_ringbuff.h>
#include <arc_audio_frame.h>
#include <arc_decode.h>
#include <arc_encode.h>

// extern "C"
// {
// #include </home/devmaul/isp2.2/Common/flexNET/v11.5/src/license.h>
// #include </home/devmaul/isp2.2/Common/flexNET/v11.5/machind/lm_code.h>
// }

//#ifndef ARC_SIP_REDIRECTOR
// #include "arc_video_packet.hpp"
//#endif


/*END: Third party header files*/

/*ARC header files */

#include <Telecom.h>
#include <AppMessages.h>
#include <BNS.h>
#include <ISPSYS.h>
#include <shm_def.h>
#include <log_defs.h>
#include <TEL_LogMessages.h>

/*END: ARC header files*/

#define STOP_FAX_TONE_ACTIVITY  10


#ifdef ACU_RECORD_TRAILSILENCE

#include "acu_type.h"
#include "res_lib.h"
#include "acu_os_port.h"
#include "smrtp.h"

#include "cl_lib.h"
#include "sw_lib.h"
#include "smdrvr.h"
#include "smbesp.h"
#include "smhlib.h"
#include "smwavlib.h"
#include "prosody_x.h"
#include "iptel_lib.h"

#define RECORD_CHAN     0

int setChannel(int zChannel, int zPortValue, int zAppPid, int zType);
static pthread_mutex_t  gMutexCPChannel;
static int              gNumRecordingChansInUse = 0;
static int				gMaxPorts		= 10;
#define DEF_PORT	-77

typedef struct _module_data
{
        ACU_INT                         module;
        tSMModuleId                     module_id;
}MODULE_DATA;


typedef struct a_channel
{
    SM_VMPRX_CREATE_PARMS               rx_create;
    SM_VMPRX_EVENT_PARMS                rx_event;
    SM_VMPRX_STATUS_PARMS               rx_status;
	SM_VMPRX_CODEC_PARMS				rx_codec;
	SM_VMPRX_STOP_PARMS					rx_stop;
	SM_VMPRX_PORT_PARMS					rx_port;
    ACU_SDK_WAIT_OBJECT*                vmprx_wo;

	SM_CHANNEL_ALLOC_PLACED_PARMS		channelParms;
	tSMChannelId						TiNG_ChannelId;

	SM_VMPRX_DATAFEED_PARMS				rx_datafeed;
	SM_CHANNEL_DATAFEED_CONNECT_PARMS	channel_datafeed_connect;

	SM_LISTEN_FOR_PARMS					listenFor;
	SM_CATSIG_LISTEN_FOR_PARMS          catSigListenFor;
	SM_CHANNEL_SET_EVENT_PARMS			channelSetEvent;
	SM_RECOGNISED_PARMS					recogParms;

    int                         appPort;
    int                         pid;
    int                         recordingCancelled;
	int							ownPort;
	pthread_t 					aculabRecordThreadId;

} A_CHANNEL_DATA;

typedef struct a_data
{
	ACU_OPEN_CARD_PARMS		card;
	ACU_CARD_INFO_PARMS     cardInfo;
	ACU_OPEN_PROSODY_PARMS	speech;
	SM_CARD_INFO_PARMS		speechInfo;
	ACU_OPEN_SWITCH_PARMS	aswitch;

	MODULE_DATA				*pModule;
	A_CHANNEL_DATA			chan[MAX_PORTS];
}A_DATA;

typedef struct _rtpDetails
{
	int				remoteRtpPort;
	int				payload;
	int				codec;
} 	A_RTP_DETAILS;

#endif

#ifdef VOICE_BIOMETRICS
#define MAX_AVB_DATA                8192
#endif

#define SPAN_WIDTH 48

#define MAX_DWORD (0xFFFFFFFF)
#define RTPH263_BUFFER_SIZE			  5000

//#define MAX_PORTS 576

#define INACTIVE	100
#define SENDONLY	101
#define RECVONLY	102
#define SENDRECV	103

#define WITH_RTP	104
#define WITHOUT_RTP 105

#define EXIT_SLEEP 2

#define ERR 0
#define WARN 1
#define INFO 2

#define YES 103
#define ISP_FAIL 	-1
#define ISP_SUCCESS 0

#define MAX_CONF_PORTS 5

#define LOCAL_STARTING_RTP_PORT_DEFAULT 10500

#ifdef TTS_CHANGES
#define LOCAL_STARTING_TTS_RTP_PORT 12000
#endif // TTS_CHANGES

#define LOCAL_STARTING_CONFERENCE_RTP	24000

/**
Shared mem IPC related defs and vars
*/
#define SHMKEY_SIP	((key_t)9000)
#define SHMSIZE_SIP (MAX_PORTS * 4 *( sizeof(long) + 256 ))
/*END: Shared mem IPC related defs and vars*/

#define DMOP_EXITALL					9999

#define STREAMBUFF_NOTAVAILABLE        -998

#define MAX_NUM_CALLS 	MAX_PORTS
#define MAX_RTP_SESSIONS_PER_CALL 1
#define MAX_RTP_SESSIONS  MAX_NUM_CALLS * MAX_RTP_SESSIONS_PER_CALL
#define RTP_USE_SEPARATE_THREAD
#define MAX_THREADS_PER_CALL 2
#define REJECTED_CALL_ID -98
#define REJECTED_CHANNEL_ID -98
#define BIND_ERROR_RETRY_SECONDS 15

#define DONE_READING_APP_PID 0
#define MAX_LOG_MSG_LENGTH 512

#define FIFO_DIR_NAME_IN_NAME_VALUE_PAIR "FIFO_DIR"
#define DEFAULT_FIFO_DIR "/tmp"
#define DEFAULT_FILE_DIR "/tmp"
#define REQUEST_FIFO "RequestFifo"
#define GOOGLE_REQUEST_FIFO "ArcGSRRequestFifo"
#define GOOGLE_RESPONSE_FIFO "ArcGSRResponseFifo"
#define DYNMGR_RESP_PIPE_NAME "ResFifo"
#define RADVISION_CONFIG_FILE "config.val"

#define RING_DURATION_MSEC 3000
#define KILL_APP_GRACE_TIME_SEC_DEFAULT	3
#define DEFAULT_REGISTRATION_HEARTBEAT_INTERVAL 10

#define DYN_BASE 		1

#define REPORT_NORMAL 	1
#define REPORT_VERBOSE 	2
#define REPORT_CDR		16
#define REPORT_DIAGNOSE	64
#define REPORT_DETAIL 	128

#define SEPERATE_PROCESS_RTP_START			1
#define SEPERATE_PROCESS_RTP_STOP			2
#define SEPERATE_PROCESS_RTP_DISCONNECT		3

#define LEG_A 110
#define LEG_B 188

#define INBOUND 210
#define OUTBOUND 211


#define BRIDGE_DATA_ARRAY_SIZE 100
#define MAX_UDP_PACKET_SIZE (1500 - 28)

#define PRACK_DISABLED      0
#define PRACK_SUPPORTED     1
#define PRACK_REQUIRE       2

#define BLASTDIAL_RESULT_FILE_BASE "/tmp/.blastDialResult"

#define STOP_FAX_TONE_ACTIVITY	10

typedef enum dnisSource
{
	DNIS_SOURCE_TO,
	DNIS_SOURCE_RURI = 20
}dnisSource_;

enum SipRedirectorRule_e {
  SIP_REDIRECTION_RULE_ROUTE,
  SIP_REDIRECTION_RULE_REJECT
};


typedef enum goertzelProcessType
{
	PROCESS_DTMF = 10,
	PROCESS_DTMF_AND_RECORD,
	PROCESS_RECORD
} goertzelProcessType;


typedef enum last_error
{
	CALL_NO_ERROR,
	CALL_TERMINATE_FAILED
} last_error_;

typedef enum term_reason
{
	TERM_REASON_DEV,
	TERM_REASON_DTMF,
	TERM_REASON_USER_STOP,
	TERM_REASON_ERROR
} term_reason_;

/**
Port states to show if port is OFF or ON
*/
typedef enum port_state
{
    PORT_STATE_OFF,
    PORT_STATE_ON
}port_state_;

typedef enum _useragent
{
	USER_AGENT_UNKNOWN	= 0,
	USER_AGENT_AVAYA_CM	= 1,
	USER_AGENT_AVAYA_SM	= 2,
	USER_AGENT_AVAYA_XM	= 4,
	USER_AGENT_CISCO_CM	= 8,
	USER_AGENT_CISCO_SM	= 16,
	USER_AGENT_CISCO_XM	= 32,
	USER_AGENT_SONUS	= 64,
	USER_AGENT_GENERIC  = 128
} useragent;

/**
Call states that this state machine can be in
*/
typedef enum call_state
{
	CALL_STATE_IDLE,					///Call in idle state			0
	CALL_STATE_RESERVED_NEW_CALL,		///Call in reserved state		1
	CALL_STATE_CALL_TERMINATE_CALLED,	///Call in terminated state		2
	CALL_STATE_CALL_INITIATE_ISSUED,	///Call has been initiated		3
	CALL_STATE_CALL_TRANSFER_ISSUED,	///Call has transfer issued		4

/**
 * Registration Info 
 */
	CALL_STATE_REGISTRATION_NEW,		///Announce a new registration.			5
	CALL_STATE_REGISTRATION_SUCCESS,	///User is successfully registered.		6
	CALL_STATE_REGISTRATION_FAILURE,	///User is not succesfully registered.	7
	CALL_STATE_REGISTRATION_REFRESHED,	///Registration has been refreshed.		8
	CALL_STATE_REGISTRATION_TERMINATED,	///UA is not registered anymore.		9

/**
 * for UAC events 
 */
	CALL_STATE_CALL_NOANSWER,			///Announce no answer within the timeout.	10
	CALL_STATE_CALL_PROCEEDING,			///Announce processing by a remote app		11
	CALL_STATE_CALL_RINGING,			///Announce ringback						12
	CALL_STATE_CALL_ANSWERED,			///Announce the start of a call				13
	CALL_STATE_CALL_REDIRECTED,			///Announce a redirection					14
	CALL_STATE_CALL_REQUESTFAILURE,		///Announce a request failure				15
	CALL_STATE_CALL_SERVERFAILURE,		///Announce a server failure				16
	CALL_STATE_CALL_GLOBALFAILURE,		///Announce a global failure				17

/**
 * for UAS events 
 */
	CALL_STATE_CALL_NEW,				///Announce a new call							18
	CALL_STATE_CALL_ACK,				///ACK received for 200ok to INVITE				19
	CALL_STATE_CALL_CANCELLED,			///Announce that the call has been cancelled	20
	CALL_STATE_CALL_TIMEOUT,			///Announce that the call has failed			21
	CALL_STATE_CALL_HOLD,				///Audio must stop, call has been put on hold	22
	CALL_STATE_CALL_OFFHOLD,			///Audio must resume, call has been taken off hold	23
	CALL_STATE_CALL_CLOSED,				///A BYE was received for this call				24
	CALL_STATE_CALL_MODIFIED,			///A reINVITE was received for this call		25

/**
 * for both UAS & UAC events 
 */
	CALL_STATE_CALL_STARTAUDIO,			///Audio must be established			26	
	CALL_STATE_CALL_RELEASED,			///Call context is cleared				27
	CALL_STATE_CALL_BYE_ACK,			///Call BYE is acknowledged ... DDN added	28
	CALL_STATE_CALL_BRIDGED,			///The call is bridged ... DDN added		29
	CALL_STATE_CALL_MEDIACONNECTED,     ///The call media is bridged ... DDN added	30
	CALL_STATE_CALL_LISTENONLY,         ///The bridged call is listen only ... RG added	31
	CALL_STATE_CALL_LISTENONLY_CALEA,         ///The bridged call is listen only ... RG added	32
	CALL_STATE_CALL_BRIDGED_CALEA,			///The call is bridged ... DDN added		33
    CALL_STATE_CALL_BRIDGED_THIRDPARTY,  /// this has been added for testing for now    34

/**
 * for UAC events 
 */
	CALL_STATE_OPTIONS_NOANSWER,		///Announce no answer within the timeout	35
	CALL_STATE_OPTIONS_PROCEEDING,		///Announce processing by a remote app		36
	CALL_STATE_OPTIONS_ANSWERED,		///Announce a 200ok							37
	CALL_STATE_OPTIONS_REDIRECTED,		///Announce a redirection					38
	CALL_STATE_OPTIONS_REQUESTFAILURE,	///Announce a request failure				39
	CALL_STATE_OPTIONS_SERVERFAILURE,	///Announce a server failure				40
	CALL_STATE_OPTIONS_GLOBALFAILURE,	///Announce a global failure				41

	CALL_STATE_INFO_NOANSWER,			///Announce no answer within the timeout	42
	CALL_STATE_INFO_PROCEEDING,			///Announce processing by a remote app		43
	CALL_STATE_INFO_ANSWERED,			///Announce a 200ok							44
	CALL_STATE_INFO_REDIRECTED,			///Announce a redirection					45
	CALL_STATE_INFO_REQUESTFAILURE,		///Announce a request failure				46
	CALL_STATE_INFO_SERVERFAILURE,		///Announce a server failure				47
	CALL_STATE_INFO_GLOBALFAILURE,		///Announce a global failure				48

/**
 * for UAS events 
 */
	CALL_STATE_OPTIONS_NEW,				///Announce a new options method			49
	CALL_STATE_INFO_NEW,				///New info request received				50

/**
 * Presence and Instant Messaging 
 */
	CALL_STATE_SUBSCRIPTION_NEW,		///Announce new incoming SUBSCRIBE			51
	CALL_STATE_SUBSCRIPTION_UPDATE,		///Announce incoming SUBSCRIBE				52
	CALL_STATE_SUBSCRIPTION_CLOSED,		///Announce end of subscription				53

	CALL_STATE_SUBSCRIPTION_NOANSWER,	///Announce no answer						54
	CALL_STATE_SUBSCRIPTION_PROCEEDING,	///Announce a 1xx							55
	CALL_STATE_SUBSCRIPTION_ANSWERED,	///Announce a 200ok							56
	CALL_STATE_SUBSCRIPTION_REDIRECTED,	///Announce a redirection					57
	CALL_STATE_SUBSCRIPTION_REQUESTFAILURE,		///Announce a request failure		58
	CALL_STATE_SUBSCRIPTION_SERVERFAILURE,		///Announce a server failure		59
	CALL_STATE_SUBSCRIPTION_GLOBALFAILURE,		///Announce a global failure		60
	CALL_STATE_SUBSCRIPTION_NOTIFY,		///Announce new NOTIFY request				61

	CALL_STATE_SUBSCRIPTION_RELEASED,	///call context is cleared					62

	CALL_STATE_IN_SUBSCRIPTION_NEW,		///Announce new incoming SUBSCRIBE			63
	CALL_STATE_IN_SUBSCRIPTION_RELEASED,		///Announce end of subscription		64

	CALL_STATE_CALL_TRANSFERFAILURE,	///The transfer call has failed				65
	CALL_STATE_CALL_TRANSFERCOMPLETE,	///The transfer call has completed			66
	CALL_STATE_CALL_TRANSFERCOMPLETE_CLOSED,//										67
	CALL_STATE_CALL_TRANSFER_MESSAGE_ACCEPTED,		//								68
	CALL_STATE_CALL_TRANSFER_RINGING,		//										69

	CALL_STATE_CALLBACK_COUNT				//										70
} call_state_;

///These types are used for Media Manager control, in the input/ouput threads.
/**
In Media Manager when a call comes in we launch two threads to handle the
rtp streams.  The one that deals with caller input is called the inputThread().
The one that deals with what we want the caller to hear is called the
outputThread().  The way that these threads work is that they are stuck in
an infinite while loop and call the function getNextInputAction() or
getNextOutputAction().  This function returns to them the following ACTION
commands so the threads know what to do next.
*/
typedef enum action
{
	ACTION_EXIT,
	ACTION_RESTART_OUTPUT_SESSION,
	ACTION_CONTINUE,
	ACTION_WAIT,
	ACTION_GET_NEXT_REQUEST,
	ACTION_NEW_REQUEST_AVAILABLE,
	ACTION_STREAM_FINISHED = -999,
	ACTION_RETURN0
} action_t;

typedef enum output_dtmf
{
	OUTPUT_DTMF_RFC_2833,
	OUTPUT_DTMF_SIP_INFO,
	OUTPUT_DTMF_INBAND
} output_dtmf_t;

typedef enum debug_module
{
	DEBUG_MODULE_FILE = 0,		///Has to be zero, do not change.
	DEBUG_MODULE_AUDIO,
	DEBUG_MODULE_SR,
	DEBUG_MODULE_SIP,
	DEBUG_MODULE_RTP,
	DEBUG_MODULE_CALL,
	DEBUG_MODULE_MEMORY,
	DEBUG_MODULE_STACK,
	DEBUG_MODULE_ALWAYS
} debug_module_;

typedef enum rtp_direction
{
	RTP_DIRECTION_SENDONLY,
	RTP_DIRECTION_RECVONLY,
	RTP_DIRECTION_SENDRECV
} rtp_direction_;

typedef enum multi_purpose_opcode
{
	MP_OPCODE_STATIC_APP_GONE,
	MP_OPCODE_KILL_APP,
	MP_OPCODE_TERMINATE_APP,
	MP_OPCODE_OUTPUT_DTMF,
	MP_OPCODE_END_CALL,
	MP_OPCODE_REREGISTER,
	MP_OPCODE_CHECK_FAX_DELAY,
	MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP,
	MP_OPCODE_SESSION_EXPIRES_SEND_REFRESH  
} multi_purpose_opcode_;

typedef enum prack_actions
{
    PRACK_REJECT_420,       // 0
    PRACK_REJECT_403,       // 1
    PRACK_NORMAL_CALL,      // 2
    PRACK_SEND_REQUIRE      // 3
} prack_actions_;

struct DebugModuleStruct
{
	int module;
	char label[10];
	int enabled;
};

struct AuthenticateInfo
{
	char realm[512];
	char user[512];
	char id[512];
	char password[512];
	char proxy[512];
    int regid;
};


struct FaxInfo
{
	int isFaxCall;
	int version;
	int bitrate;
	int fillBitRemoval;
	int transcodingMMR;
	int transcodingJBIG;
	char rateManagement[100];
	int maxBuffer;
	int maxDatagram;
	char udpEC[100];
};

struct MultiPurposeTimerStrcut
{
	int opcode;
	int port;
	int interval;

	int data1;
	int data2;
	int deleted;

	struct timeval expires;
	struct timeval lastOccurance;

	struct MsgToDM msgToDM;
	struct MultiPurposeTimerStrcut *nextp;
	struct MultiPurposeTimerStrcut *prevp;
};

struct MsgCounter
{
	long count;
	struct MsgToDM msgToDM;
};

struct EncodedShmBase
{
	struct MsgCounter msgCounter[MAX_PORTS * 4];
};

typedef struct speakList SpeakList;

///This struct is the linked list we use in Media Manager to keep track of DMOP_SPEAK requests.
/**
When we get a DMOP_SPEAK request in Call Manager it is automatically sent to
Media Manager where it is put in a list of type speakList.
*/
struct speakList
{
	struct Msg_Speak 		msgSpeak;
	struct Msg_PlayMedia 	msgPlayMedia;
    struct Msg_SpeakMrcpTts msgSpeakTts;
    long                    msgMTtsSpeakId;

	int isAudioPresent;
    int isMTtsPresent;

	int isSpeakDone;
	int audioLooping;
	int doneAudioVideo;

	SpeakList *nextp;
};

typedef struct playList PlayList;
struct playList
{
	struct Msg_PlayMedia msgPlay;
	PlayList *nextp;
};

typedef struct requestList RequestList;

struct requestList
{
	struct MsgToDM msgToDM;
	RequestList *nextp;
};

struct InputAction
{
	int actionId;			///EXIT or CONTINUE
	RequestList *request;
};

struct OutputAction
{
	int actionId;			///EXIT or CONTINUE
	RequestList *request;
};

typedef struct
{
	int opcode;
	int appRef;
	pthread_t threadId;
} ThreadInfo;

typedef struct rtpMrcpData RtpMrcpData;

struct rtpMrcpData
{
	char data[4096];
	int dataSize;
	int dataType;
	RtpMrcpData *nextp;
};

#if 0
typedef struct gv_StreamPlayBackList GV_StreamPlayBackList;

struct gv_StreamPlayBackList
{
    int lastBuffer;
    char file[128];
    GV_StreamPlayBackList *prevp;
    GV_StreamPlayBackList *nextp;
};
#endif

struct BridgeCallData
{
	char	data[200];
	int		filled;
	int		dtmf;
	int 	length;
};

#if 0
struct BridgeCallVideoData
{
	char	data[4096];
	int		filled;
	int		dtmf;
	int		dataLength;
};
#endif

struct currentFile
{
	int appRef;
	int isReadyToPlay;
};

#define lastMsgToDM msgToDM

#if 0
typedef struct gv_StreamPlayBackList GV_StreamPlayBackList;

struct gv_StreamPlayBackList
{
    int lastBuffer;
    char file[128];
    GV_StreamPlayBackList *prevp;
    GV_StreamPlayBackList *nextp;
};
#endif

typedef struct speakFileDetails SpeakFileDetails;

struct speakFileDetails
{
	int appCallNum;
	int action;
	char rtpIP[20];
	int rtpPort;
	int rtcpPort;
	char speakFileName[220];
};

typedef struct struct_NameValue
{
	char name[256];
	char value[256];
} NameValue;


typedef struct struct_WavHeaderStruct
{   
    char rId[4];                    ///0 - 3 'RIFF'
    uint32_t rLen;                  ///4 - 7 wRiffLength, length of file minus the 8 byte riff header
    char wId[4];                    ///8 - 11 'WAVE'
    char fId[4];                    ///12 - 15 'fmt '
    uint32_t fLen;                  ///16 - 19 wFmtSize, length of format chunk minus 8 byte header
    unsigned short nFormatTag;      ///20 - 21 wFormatTag, identifies PCM, ULAW, etc
    unsigned short nChannels;       ///22 - 23 wChannels
    uint32_t nSamplesPerSec;        ///24 - 27 dwSamplesPerSecond, samples per second per channel
    uint32_t nAvgBytesPerSec;       ///28 - 31 dwAvgBytesPerSec, non-trivial for compressed formats
    unsigned short nBlockAlign;     ///32 - 33 wBlockAlign, basic block size
    unsigned short nBitsPerSample;  ///34 - 35 wBitsPerSample, non-trivial for compressed formats
    unsigned short extSize;         ///36 - 37 wExtSize = 0, the length of the format extension
    char dId[4];                    ///38 - 41 'DATA'
    uint32_t dLen;                  ///42 - 45 dwDataLength, length of data chunk minus 8 byte header

} WavHeaderStruct;
    
#if 0
typedef struct struct_WavHeaderStruct
{
	char rId[4];					///0 - 3 'RIFF'
	unsigned long rLen;				///4 - 7 wRiffLength, length of file minus the 8 byte riff header
	char wId[4];					///8 - 11 'WAVE'
	char fId[4];					///12 - 15 'FMT'
	unsigned long fLen;				///16 - 19 wFmtSize, length of format chunk minus 8 byte header
	unsigned short nFormatTag;		///20 - 21 wFormatTag, identifies PCM, ULAW, etc
	unsigned short nChannels;		///22 - 23 wChannels
	unsigned long nSamplesPerSec;	///24 - 27 dwSamplesPerSecond, samples per second per channel
	unsigned long nAvgBytesPerSec;	///28 - 31 dwAvgBytesPerSec, non-trivial for compressed formats
	unsigned short nBlockAlign;		///32 - 33 wBlockAlign, basic block size
	unsigned short nBitsPerSample;	///34 - 35 wBitsPerSample, non-trivial for compressed formats
	unsigned short extSize;			///36 - 37 wExtSize = 0, the length of the format extension
	char dId[4];					///38 - 41 'DATA'
	unsigned long dLen;				///42 - 45 dwDataLength, length of data chunk minus 8 byte header

} WavHeaderStruct;
#endif

typedef struct
{
	int appCallNum;
	int appRef;
	int synchronous;
	pthread_t aTid;
	pthread_t bTid;
} SpeakExitStruct;

typedef struct
{
	WavHeaderStruct wavHeaderStruct;
	int appCallNum;
	int appRef;
	char fileName[128];
} RecordExitStruct;

typedef struct
{
	int sleep_time;
	int iterrupt_option;
	char filename[128];
	int synchronous;
	int session_number;
} SpeakFileStruct;

struct ChannelToBeMade
{
	int value;
	struct ChannelToBeMade *nextp;
};

struct CallToBeDisconnected
{
	int value;
	struct CallToBeDisconnected *nextp;
};

struct CallToBeMade
{
	struct Msg_InitiateCall *pMsg_InitiateCall;
	struct CallToBeMade *nextp;
};


//

///This struct holds all call specific information.

class Call
{
	public:
		Call(){};
		~Call(){};

	char tmpBuffer0[256];

	char logicalName[20];

	int portState;

	int callNum;
	int callState;
	int callSubState;
	int callDirection; // MR 4964
// MR 4655
	int prevCallState;
// END: 4655
    int SentOk;

	int appPassword;
	int appPid;
	int prevAppPid;
	int canKillApp;

	int lastError;

	int cid;
	int did;
	int tid;
	int reinvite_tid;
	int reinvite_empty_sdp;

	int responseFifoFd;
	int responseFifoMade;

	uint32_t in_user_ts;
	uint32_t out_user_ts;


	int inTsIncrement;
	int outTsIncrement;

	int speakStarted;
	int firstSpeakStarted;

	int sendingSilencePackets;
	int receivingSilencePackets;

	int dtmfAvailable;
	int lastDtmf;

	int dtmfInCache[10];
	int dtmfCacheCount;

	int dtmfStarted;
	uint32_t lastDtmfTimestamp;
	uint32_t lastDtmfStartTimestamp;
	uint32_t lastDtmfStopTimestamp;
	double lastDtmfMilliSec;	
	struct timeval lastDtmfTime;

	int currentApi;

	int permanentlyReserved;

	int bandwidth;
	int stopSpeaking;

	pthread_t lastTimeThreadId;
	pthread_t outputThreadId;
	pthread_t inputThreadId;
	pthread_t cleanerThread;
	pthread_t initTelecomThreadId;
	pthread_t dropCallThreadId;		// BT-244

	pthread_mutex_t gMutexThreadSlot;

	int clear;

	int numberOfAsyncSpeaksInProgress;

	pthread_mutex_t gMutexSpeak;
	SpeakList *pFirstSpeak;
	SpeakList *pLastSpeak;
	int currentSpeakFd;
	size_t currentSpeakSize;
	int fdSpeak;

	pthread_mutex_t gMutexRequest;
	RequestList *pFirstRequest;
	RequestList *pLastRequest;

	int pendingInputRequests;
	int pendingOutputRequests;

	int speechRec;				///0=No speechRec 1=speechRec
	int speechRecActive;		///0=No speechRec 1=speechRec
	int speechRecFromClient;	///0=No speechRec 1=speechRec
	int canBargeIn;
	pthread_mutex_t gMutexSR;

#ifdef SR_MRCP
	RtpSession *rtp_session_mrcp;
	int prevRtpTimeStamp;
	unsigned long mrcpTs;
	RtpMrcpData *pFirstRtpMrcpData;
	RtpMrcpData *pLastRtpMrcpData;
	FILE *gUtteranceFileFp;
	long	utterance_location;

	int srRtpCounter;
	char srDTMF[100];

	int prevMrcpTtsRtpTime;
	RtpSession *mrcpTtsRtpStack;

	char gUtteranceFile[100];
	char mrcpServer[100];
	int mrcpRtpPort;
	int mrcpRtcpPort;

	int attachedSRClient;

	struct Msg_SRRecognize msgRecognizeFromClient;
#endif
    struct Msg_SpeakMrcpTts msgSpeakMrcpTts;
    RtpSession *rtp_session_mrcp_tts_recv;
    int         keepSpeakingMrcpTTS;
    int         finishTTSSpeakAndExit;
    int         ttsRequestReceived;

	int			sendCallProgressAudio;
#if 1

    RtpSession *rtp_session_tone;
	char 		toneServer[100];
	char 		toneServerSpecialFifo[255];
	char 		toneServerFaxSpecialFifo[255];
	int			toneRtpPort;
	int			toneRtcpPort;
	unsigned long toneTs;

	pthread_t   FaxThreadTid;
	
	char 		faxServerSpecialFifo[255]; 			//  djb - should be removed; don't need it.
	char        faxServer[100];
	int			faxRtpPort;
	int			sendFaxOnProceed;
	int			faxRtcpPort;

	int			conferenceStarted;

	int			confRtpRemotePort;
	int			confRtpLocalPort;
    RtpSession *rtp_session_conf_recv;
    RtpSession *rtp_session_conf_send;
	int			conf_in_user_ts;

	int			sendConfData;
	int			recvConfData;

	char 		confID[64];
	int			conf_ports[MAX_CONF_PORTS];
	int			confUserCount;

#endif


	time_t utteranceTime1;
	time_t utteranceTime2;

	int ttsRtpPort;
	int ttsRtcpPort;
	int ttsMrcp;
	int ttsMrcpBargeIn;
    int ttsMrcpFileFd;

	ThreadInfo threadInfo[MAX_THREADS_PER_CALL];

	char remoteRtpIp[50];
	char remoteSipIp[50];
	char remoteContactIp[50];

	char audioCodecString[256];

	char playBackFileExtension[256];

	int remoteRtpPort;
	int remoteSipPort;
	int remoteContactPort;
	int telephonePayloadType;
	int telephonePayloadType_2;
	int telephonePayloadType_3;
	int telephonePayloadType_4;
	int telephonePayloadPresent;
	int codecType;
	int silkcodecType;
	int rtpPayloadType;
	int codecBufferSize;
	int codecSleep;

	int full_duplex;
	int isInitiate;

	char sdp_body_remote[4096];
	char sdp_body_remote_new[4096];
	char sdp_body_local[4096];
	char sdp_body_fax_local[4096];
/*
 * Small Conferencing Variables
 */
    int conferenceIdx;
/*
 * other DSP like stuff for audio
 * this one is for 5 party mixing,  
 * mixed recording, 
 */

   int audio_mode;

   struct arc_audio_gain_parms_t audio_gain_parms[4];
   struct arc_audio_mixing_parms_t  audio_mix_parms[4];
   struct arc_audio_silence_parms_t sgen[4];

   float currentVolume;

   struct arc_audio_frame_t audio_frames[4];
   struct arc_conference_frame_t *conf_frame;

/*
 *  decoded audio routines 
 */
    struct arc_decoder_t decodeDtmfAudio[4];

    struct arc_decoder_t decodeAudio[4];
    struct arc_encoder_t encodeAudio[4];

    struct arc_g711_parms_t g711parms;
    // 
#ifdef SILK
    struct arc_silk_encoder_parms_t silkEncParms;
    struct arc_silk_decoder_parms_t silkDecParms;
#endif
    // 

/*
 * inband tone detection using spandsp
*/
	int isInbandToneDetectionThreadStarted;

    char decodedAudioBuff[4][3000]; // mtu * 2 for decoding space 
    pthread_t detectionTID;
    int runToneThread;
    int toneDetect;
    int toneDetected;
    arc_tones_ringbuff_t toneDetectionBuff;

    char decodedDtmfAudioBuff[4][3000]; // mtu * 2 for decoding space 
    arc_tones_ringbuff_t dtmfDetectionBuff;
/*
 * bridge third leg calling elements 
*/

    Call *crossRef[SPAN_WIDTH];
    int thirdParty;
    int parent_idx;
	int crossPort;
	int caleaPort;
	int leg;    
    // only for b leg bridge prompts for now 
	int in_speakfile;



/*
 * end third leg stuff 
*/

	int restart_rtp_session;
	RtpSession *rtp_session;
	RtpSession *rtp_session_in;
	int rtpSendGap;

	int localSdpPort;
#ifdef TTS_CHANGES
	int localTtsRtpPort;
#endif // TTS_CHANGES

	int callerRtpPort;
	int callerRtcpPort;
	char callerIP[20];

	int speakProcessWriteFifoFd;
	int speakProcessReadFifoFd;

	int srClientFifoFd;
	int ttsClientFifoFd;

	int lastBufferPartPlayed;
//	char streamErrString[128];
//	int			streamReturnCode;
	time_t      speakStartTime;
//	time_t      streamStartTime;
	int         GV_LastSpeakPlayedDuration;
//	int         GV_LastStreamPlayedDuration;

	///Fax Data
	struct FaxInfo faxData;
	struct Msg_RecvFax msgFax;
	struct Msg_SendFax msgSendFax;
	int isFaxCall;
	int sendFaxReinvite;
	int send200OKForFax;
	int isFaxReserverResourceCalled;
	int isSendRecvFaxActive;
	int sentFaxPlaytone;
	int faxDelayTimerStarted;

    int         faxAlreadyGotReinvite;  // djb - remove - testing for joe
    int         faxGotAPI;  // djb - remove - testing for joe

	pthread_mutex_t gMutexPlay;
	pthread_mutex_t gMutexPlayPutQueue;
	PlayList *pFirstPlay;
	PlayList *pLastPlay;

	///Playback Control
	char playBackControl[10];
	int  playBackOn;
	int  playBackIncrement;
	
	int pauseFileFd;

	char srClientFifoName[256];

	char responseFifo[256];
	char lastResponseFifo[256];
	char ani[256];
	char dnis[256];
	char originalDnis[256];
	char originalAni[256];
//	char remoteAgentString[256];
	char pAIdentity_ip[256];

	struct Msg_Record msgRecord;
	struct Msg_RecordMedia msgRecordMedia;
	struct MsgToDM msgToDM;
	struct MsgToApp answerCallResp;;
	int isItAnswerCall;
	int isIFrameDetected;
	int remoteAgent;

/**
 * DMOP_SETGLOBAL
 */
	int GV_CallProgress;
	int GV_PlaybackAndSpeechRec;
	int GV_LastCallProgressDetail;
	int GV_ClampBridgeDTMF;
	int GV_CancelOutboundCall;
	int GV_SRDtmfOnly;
	int GV_MaxPauseTime;
	int GV_PlayBackBeepInterval;
	int GV_KillAppGracePeriod;
	int GV_DtmfBargeinDigitsInt;
	int GV_CallDuration;
	char GV_RecordTermChar;
	int	GV_BridgeRinging;
	int	GV_FlushDtmfBuffer;
	int	GV_LastPlayPostion;
    int GV_HideDTMF;
    int GV_SkipTimeInSeconds;
	
/**
 * DMOP_SETGLOBALSTRING
 */
	char GV_DtmfBargeinDigits[20];
	char GV_PlayBackPauseGreetingDirectory[256];
	char GV_CallingParty[512];
	char GV_CustData1[9];
	char GV_CustData2[101];
	char GV_SkipTerminateKey[512];
	char GV_SkipRewindKey[512];
	char GV_SkipForwardKey[512];
	char GV_SkipBackwardKey[512];
	char GV_PauseKey[512];
	char GV_ResumeKey[512];
	char GV_VolumeUpKey[512];
	char GV_VolumeDownKey[512];
    char GV_SpeedUpKey[512];
    char GV_SpeedDownKey[512];
	int GV_Volume;
	int GV_Speed;
	char GV_OutboundCallParm[512];
	char GV_InboundCallParm[512];
	char GV_SipUriVoiceXML[512];
	char GV_SipUriPlay[512];
	char GV_SipUriEarly[512];
	char GV_PreferredCodec[512];
	char GV_SipAuthenticationUser[512];
	char GV_SipAuthenticationPassword[512];
	char GV_SipAuthenticationId[512];
	char GV_SipAuthenticationRealm[512];
	char GV_SipFrom[512];
	char GV_SipAsAddress[512];

	char GV_DefaultGateway[512];
	char GV_RouteHeader[512];
	char GV_PAssertedIdentity[512];
	char GV_PChargeInfo[512];
	char GV_Diversion[4096];
	char GV_CallInfo[512];
	char GV_CallerName[512];
	char transferAAI[512];
	char remoteAAI[512];
    // user to user header joes Thu Apr 23 11:51:15 EDT 2015
	char local_UserToUser[512];
	int local_UserToUser_len;
	char remote_UserToUser[512];
	int remote_UserToUser_len;
    // end user to user 

	int waitForPlayEnd;
	struct MsgToApp msgPortOperation;

	int eXosipStatusCode;
	int outboundRetCode;
	char eXosipReasonPhrase[100];
	char lastInData[161];

	char lastEventTime[40];
	char silenceBuffer[201];
	char silenceFile[256];

    // gv values for faxing
    int GV_FaxState;
    int GV_FaxFileName[256];

    int  GV_FaxDebug;
    char GV_T30FaxStationId[100];
    char GV_T30HeaderInfo[100];
    int  GV_T30ErrorCorrection;
    int  GV_T38Enabled;
    int  GV_T38Transport;
    int  GV_T38ErrorCorrectionMode;
    int  GV_T38ErrorCorrectionSpan;
    int  GV_T38ErrorCorrectionEntries;
    int  GV_T38MaxBitRate;
    int  GV_T38FaxFillBitRemoval;
    int  GV_T38FaxVersion;
    int  GV_T38FaxTranscodingMMR;
    int  GV_T38FaxTranscodingJBIG;
    int  GV_T38FaxRateManagement;
    int  GV_T38FaxMaxBuffer;
    int  GV_T38FaxMaxDataGram;
    int  GV_UdptlStartPort;
    int  GV_UdptlEndPort;
    // end fax 

/**
 * Variable sleep
 */

	unsigned long   m_dwPrevGrabTime_out;
	unsigned long   m_dwExpectGrabTime_out;

	unsigned long   m_dwPrevGrabTime_in;
	unsigned long   m_dwExpectGrabTime_in;

	struct timeval 	lastOutRtpTime;
	struct timeval	lastInRtpTime;

	clock_t			lastOutRtpClock;
	clock_t			lastInRtpClock;

/*END: Variable sleep*/

	struct BridgeCallData bridgeCallData[BRIDGE_DATA_ARRAY_SIZE];
	int lastReadPointer;
	int lastWritePointer;

	int responseFifoFdBkp;

	char tmpBuffer1[256];

	char	bridgeDestination[256];
	time_t  lastReleaseTime;
	time_t  lastConnectedTime;

//BackupQueue
	pthread_mutex_t gMutexBkpSpeak;
	SpeakList *pFirstBkpSpeak;
	SpeakList *pLastBkpSpeak;
	

	SpeakList *pPlayQueueList;

//RG Test for record
	int isRecordOn;
	osip_message_t *currentInvite;
	int last_serial_number;
//	int isItVideoCall;

	char originalContactHeader[1024];

	Msg_Speak 		*currentSpeak;
	Msg_PlayMedia 	*currentPlayMedia;

	char call_type[32];	

	int bytesToBeSkipped;
	int currentOpcode;
	int sendVFUCount;
	int recordStarted;
	int recordStartedForDisconnect;         // MR 5126

	char	sipMessage[8192];
	struct currentFile currentAudio;
	int		audioLooping;
	int		isBeepActive;

	char	sipBuf[2048];
	char	GV_SipURL[2048];
	int		isCallAnswered;	
    int     isCallInitiated;

#ifndef ARC_SIP_REDIRECTOR
	unsigned int  audioSsrc;
#endif

	int		m_dwLastFileSize;
	char 	rdn[32];
	char 	ocn[32];
	int		sendRecvStatus;
	int 	isCallAnswered_noMedia;

	time_t  yInitTime;
	time_t	yDropTime;	

	int 	recordOption;
	int 	syncAV;
	int		appRefToBePlayed;
	
#ifndef ARC_SIP_REDIRECTOR
	unsigned int vidoe_out_user_ts;
#endif

	int		amrIntCodec[10];
	int		silkIntCodec[10];
	char	amrFmtp[256];
	char	amrFmtpParams[256];
	int		amrIntIndex;
	int		silkIntIndex;

	int		isG729AnnexB;

	int		clearPlayList;
	
	unsigned long lastRecordedRtpTs;
	int		ttsFd;

	time_t	mrcpTime;

	sdp_message_t *remote_sdp;
	int outboundSocket;	
	int codecMissMatch;
	int lastStopActivityAppRef;
	int presentRestrict;

	int GV_RecordUtterance;
	char lastRecUtteranceFile[128];
	char lastRecUtteranceType[128];
	int	recordUtteranceWithWav;

    int silentRecordFlag;
    char silentInRecordFileName[100];
    char silentOutRecordFileName[100];
    FILE *gSilentInRecordFileFp;
    FILE *gSilentOutRecordFileFp;

#ifdef ACU_RECORD_TRAILSILENCE
	int trailSilence;
	int stop_aculabRecordThread;
	int zAcuPort;
#endif
	
	int isCalea;

/*Consult transfer*/
    int			bridgeType;

	osip_to_t   *sipTo;
	char	sipToUsername[256];
	char	sipToHost[256];
	char	sipToPort[10];

	osip_from_t   *sipFrom; 
	char	sipFromUsername[256];
	char	sipFromHost[256];
	char	sipFromPort[10];

	osip_contact_t *sipContact;

	char		sipToStr[256];
	char		sipFromStr[256];
	char		sipContactStr[256];
	char		sipContactUsername[256];

	char        tagToValue[256];
	char        tagFromValue[256];
	char        sipCallId[256];

/*  MR3069  */
    int         PRACKSendRequire;
    int         PRACKRSeqCounter;
    int         UAC_PRACKOKReceived;
    int         UAC_PRACKRingCounter;
    int         nextSendPrackSleep;
    int         UAS_PRACKReceived;
/*END: MR3069   */

    char *listOf300s[8];
    char redirectedContacts[1024];
    int RedirectStatusCode;


	char *data_buffer;

//  BlastDial
	int isInBlastDial;
	int blastDialAnswered;
	int blastDialPorts[5];
	int numDests;
	struct ArcSipDestinationList dlist[5];

	int isInputThreadStarted;
	int isOutputThreadStarted;
	int resetRtpSession;

	int confPacketCount;

	int finished;
	int tts_ts;

	int inboundAllowed;

	long	currentPhraseOffset;
	long	currentPhraseOffset_save;

	int		sessionTimersSet;
// MR 4642/4605
	int		alreadyClosed;
// END: 4642/4605
// MR 4655
	int						sendSavedInitiateCall;
	struct MsgToApp			respMsgInitiateCallToApp;
// END: 4655
#ifdef VOICE_BIOMETRICS
    char        avb_pin[32];
    void        *pv_VoiceID_conObj;
    void        *pv_voiceprint;
    int         avb_readyFlag;
    float       avb_score;
    float       avb_indThreshold;
    float       avb_confidence;
    int         continuousVerify;
    int         avb_bCounter;
    char        avb_buffer [MAX_AVB_DATA + 1];
    short       avb_bufferPCM [MAX_AVB_DATA + 1];
//  short       *avb_bufferPCM;
    FILE        *fp_pcmWavefile;

    struct Msg_AVBMsgToApp  msg_avbMsgToApp;
    struct arc_decoder_t    avb_decode_adc;
    struct arc_g711_parms_t avb_decode_parms;
#endif  // VOICE_BIOMETRICS
    int         issuedBlindTransfer;

	//BT-171
	int			canExitTransferCallThread;
    char        sipUserAgent[256];		// BT-62

	//Google Speech Rec
	int			googleUDPPort;
	int			googleUDPFd;
	int			google_slen;
	struct sockaddr_in google_si;
	int			googleRecord;
	int			googleRecordIsActive;
	int			googleFinalResult;
	int 		googleStreamingSR;
	struct MsgToApp googleSRResponse;;
	pthread_mutex_t gMutexSentGoogleRequest;
	int			googlePromptIsPlaying;
	int			googleBarginDidOccur;
	time_t		googleRecordEndTime;
	int			googleRecordEndTimeIsSet;
	time_t		googleHasPromptPlayed;
	int			googleAlreadyRepliedToApp;
	char		googleOptions[64];

	int			poundBeforeLeadSilence;	// BT-226
	char		callCDR[128];			// AT-2
};	/*END: class call*/


#ifdef IMPORT_GLOBALS

char GV_SkipTerminateKey[512];
char GV_SkipRewindKey[512];
char GV_SkipForwardKey[512];
char GV_SkipBackwardKey[512];
char GV_PauseKey[512];
char GV_ResumeKey[512];
char GV_VolumeUpKey[512];
char GV_VolumeDownKey[512];
char GV_SpeedUpKey[512];
char GV_SpeedDownKey[512];

int gLastShmUsed = 0;

long gShmWriteCount = 0;

struct EncodedShmBase *gEncodedShmBase = NULL;

pthread_mutex_t gNotifyMediaMgrLock;

pid_t gMediaMgrPid;
struct MsgToDM gExitAllStruct;
void *gShmBase;
key_t gShmKey;
int gShmId;


struct DebugModuleStruct gDebugModules[9] = {
    {DEBUG_MODULE_FILE, "FILE_IO", 0},
    {DEBUG_MODULE_AUDIO, "AUDIO", 0},
    {DEBUG_MODULE_SR, "SR", 0},
    {DEBUG_MODULE_SIP, "SIP", 0},
    {DEBUG_MODULE_RTP, "RTP", 0},
    {DEBUG_MODULE_CALL, "CALL", 0},
    {DEBUG_MODULE_MEMORY, "MEMORY", 0},
    {DEBUG_MODULE_STACK, "STACK", 0},
    {DEBUG_MODULE_ALWAYS, "DEBUG", 1}
};


NameValue gSipCodes[999];

class Call gCall[MAX_PORTS];

pid_t gParentPid;
int gResId = -1;
char gAuthUser[256];
char gAuthPassword[256];
char gAuthId[256];
char gAuthRealm[256];
char gFromUrl[256];

int	gMaxCallDuration = -1;
int gIsRtpPortSet = NO;

int gOutboundProxyRequired = 0;		///This variable is set to 1 if Call Manager will be using an outbound proxy.

// Header Lis
char gHeaderList[1024 * 10];
char gHeaderListArray[100][100];

// IPv6 support 
int gEnableIPv6Support = 0;

// Sip Redirector default rules 
int gSipRedirectionDefaultRule = 0;
int gSipRedirectionUseTable = 0;

int	gRejectEmptyInvite = 0;

// SIP RFC 3761
int gSipEnableEnum = 0;     ///This toggles outbound ENUM lookups 
char gSipEnumTLD[256] = ""; ///This is the top level domain needed for ENUM Lookups 

// SIP Signaled Digits 
int gSipSendKeepAlive = 0;
int gSipSignaledDigits = 0; 
int gSipSignaledDigitsMinDuration = 1000; 
int gSipSignaledDigitsEventExpires = 600; 

char gSipUserAgent[256];        // BT-62

// Sip default out of license response 
int gSipOutOfLicenseResponse = 503;

// SIP Session Expires
int gSipMinimumSessionExpires = 90;
int gSipSessionExpires = 1800;

int gTrailSilenceDetection = 0;	///This variable is SIP_TrailSilenceDetection from .TEL.cfg
int	gMaxAcuPorts	= 10;
int gSipOutputDtmfMethod;	///This variable tells us what form of output dtmf to send.
int gRfc2833Payload = 96;	///This variable tells us what payload type we will be using for receiving dtmf.
int gSipPort = 5060;		///This variable is the port that eXosip will listen on and is set to 5060 by default.
int gSipRedirection = 0; 	///
int gSipProxy = 0; 	///
int gVfuDuration = 0; 	   ///This variable is the duration in milliseconds after which VFU message will be sent repeatedly.
int gBeepSleepDuration= 0; 	///This variable is the duration for which silence will be recorded while playing the beep.
char gOutboundProxy[256];           ///This variable is the proxy that we will be using.
char gDefaultGateway[256];	///This variable tells us what the default gateway is.
char gPreferredCodec[256];	///This variable is set to the codec that we will be using for this IVR.
char gAMRIndex[256];		///This variable is set to the AMR Index mode that we will be using for this IVR.
int	gMaxPTime;				///This variable is set to the AMR Maxptime that we will be using for this IVR.
int gPreferredPTime;                ///This variable is set to the AMR Maxptime that we will be using for this IVR.
char g729Annexb[32];		///This variable is set to the G729 annexb support that we will be using for this IVR.
char *ispbase;				///This variable is set to the isp base directory which is usually /home/arc/.ISP .
int maxNumberOfCalls;    	///This variable is the maximum number of calls that Call Manager can receive.
char gHostName[256];		///This variable is the host name of the box Dynamic Manager is running on.
char gHostIp[48] = "";
char gContactIp[48] = "";
char gFaxSdpIp[48] = "";
char gSdpIp[48] = "";
char gHostIf[48] = "";
char gHostTransport[10] = "";
char	gCacheDirectory[256];
char	gSystemPhraseRoot[256];
char	gSystemLanguage[256];
char	gTransferAAIEncoding[256];
char	gTransferAAIParamName[256];
char    gTransferAAIPrefix[32];
int gUUIEncoding;

// this was added to mitigate the problem when the redirector will tell more than 1 span at a time to recycle
int gSipRedirectorSpans = 0; // this is the amount of spans the redirector needs to redirect the number of expected calls to


//
// Tone detection Variables, these are floats
// but they will have to accept 
//

int gToneDebug = 1;

// tone detection only 

float gToneThreshMinimum = 8000000.0;
float gToneThreshDifference = 5000.0;
float gToneDetectionNoiseThresh = 400.0;
float gHumanAnsweringThresh = 5000.0;

// calll recording 
float gCallRecordingNoiseThresh = 200.0;

// answering only 
float gAnsweringDetectionNoiseThresh = 750.0;
int   gAnsweringMachineThresh = 2500;
int   gAnsweringTrailSilenceTrigger = 750;
int   gAnsweringOverallTimeout  = 2500;
int   gAnsweringLeadingAudioTrim = 750;


//
// end tone detection 
//


int LOCAL_STARTING_RTP_PORT = 10500;	///This variable is the starting port that we will use for assigning incoming rtp packets.

char gToRespFifo[256];		///This variable gives us the name of the FIFO used to communicate with Responsibility.
char gFifoDir[256];			///This variable lets us know which directory to create and find FIFO's.
char gGlobalCfg[256];		///This variable contains the path where we can find the .Global.cfg file.
char gIpCfg[256];			///This variable contains the path where we can find the .TEL.cfg file.
char gIspBase[256];			///This variable contains the path to the ISP base directory.
char gProgramName[64];	
int gMrcpTTS_Enabled;
int gGoogleSR_Enabled;
int	gNumAculabResources;
int	gDnisSource;
//int	gAculabTrialSilenceEnabled;

char g729b_silenceBuffer[2] = {0x34, 0x40};

int gWritePCForEveryCall = 0;			///This variable is set to 1 if we need to write a Present Call Record for each call.
int gMrcpVersion = 1;
int gSymmetricRtp = 0;  ///This variable is SIP_SymmetricRtp  from .TEL.cfg
int	gPercentInboundAllowed = 100;


/* DTMF Tone Detection Values */
float gDtmfToneRange = 2.0;
float gDtmfToneThreshold = 100.0;
int	 gDtmfToneFlushTime = 100;
int	gEarlyMediaSupported = 1;
int	gRefreshRtpSession = 0;


/**
Variables of X instances of Dynamic Manager
*/
int gStartPort 	= -1;		///This is the start port for this Call Manager.
int gEndPort 	= -1;		///This is the end port for this Call Manager.
int gDynMgrId	= -1;		///This is the Id for this Dynamic Manager.
/*END: Variables of X instances of Dynamic Manager*/

/*Minimum interdigit delay for RFC2833*/
int gInterDigitDelay = 100;				///This is the minimum time that two digits can be pressed and detected by the ortp stack.

/*Whether rings should be simulated for outbound calls..*/
int gSimulateCallProgress = 0;

/*Recycle Values for ArcSipRedirector*/
int gCallMgrRecycleBase = 200000;
int gCallMgrRecycleDelta = 100000;

#ifdef ACU_LINUX
/*Aculab Fifo*/
char gSTonesFifo[256] = "";
char gSTonesFaxFifo[256] = "";
#endif

int  gFaxDebug                  = 0;
char gT30FaxStationId[100]      = "Arc Fax Station ID";
char gT30HeaderInfo[100]        = "Arc Fax Header Info";
int  gT30ErrorCorrection		= 1;
int  gT38Enabled                = 1;
int  gT38Transport              = 0; 	// 0 -> udptl; 1 -> rtp
int  gT38ErrorCorrectionMode    = 1;
int  gT38ErrorCorrectionSpan    = 3;
int  gT38ErrorCorrectionEntries = 3;
int  gT38MaxBitRate             = 14400;
int  gT38PacketInterval         = 20;
int  gT38FaxFillBitRemoval      = 0;
int  gT38FaxVersion             = 0;
int  gT38FaxTranscodingMMR      = 0;
int  gT38FaxTranscodingJBIG     = 0;
int  gT38FaxRateManagement      = 0;	// 0 -> LocalTCF; 1 -> TransferredTCF
int  gT38FaxMaxBuffer           = 1024;
int  gT38FaxMaxDataGram         = 1500;
int  gUdptlStartPort            = 4500;
int  gUdptlEndPort              = 5500;
int  gSendFaxTone				= 1;

/*Answering Machine Detection variable*/
int gDetectAnsweringMachine = false;

/*Send 183 with SDP if SIP_InboundEarlyMedia is set to 1*/
int gInboundEarlyMedia = 0;

/* PRACK */
int gPrackMethod = 0;

char offset127 = 127;
char offset0xD5 = 0xD5;
char offset0 = 0;

PayloadType pcmu8000 = {
	PAYLOAD_AUDIO_CONTINUOUS,
	8000,
	1,
	&offset127,
	1,
	64000,
	"PCMU/8000/1"
};

PayloadType pcma8000 = {
	PAYLOAD_AUDIO_CONTINUOUS,
	8000,
	1,
	&offset0xD5,
	1,
	64000,
	"PCMA/8000/1"
};

PayloadType silk8000 = {
    PAYLOAD_AUDIO_CONTINUOUS,
    8000,
    1,
    NULL,
    1,
    64000,
    "SILK/8000/1"
};


PayloadType g722_8000 = {
	PAYLOAD_AUDIO_CONTINUOUS,
	8000,
	1,
	&offset0xD5,
	1,
	64000,
	"G722/8000/1"
};

PayloadType pcm8000 = {
	PAYLOAD_AUDIO_CONTINUOUS,
	8000,
	2,
	&offset0,
	1,
	128000,
	"PCM/8000/1"
};

PayloadType g729_8000 = {
	PAYLOAD_AUDIO_CONTINUOUS,
	8000,
	10,
	NULL,
	0,
	8000,
	"G729/8000/1"
};

PayloadType lpc1016 = {
	PAYLOAD_AUDIO_PACKETIZED,
	8000,
	0,
	NULL,
	0,
	2400,
	"1016/8000/1"
};


PayloadType gsm = {
	PAYLOAD_AUDIO_PACKETIZED,
	8000,
	0,
	NULL,
	0,
	13500,
	"GSM/8000/1"
};

//TODO this is dummy amr structure, create proper amr structure
PayloadType amr = {
	PAYLOAD_AUDIO_PACKETIZED,
	8000,
	0,
	NULL,
	0,
	8000,
	"AMR/8000"
};


PayloadType mpv = {
	PAYLOAD_VIDEO,
	90000,
	0,
	NULL,
	0,
	0,
	"MPV/90000/1"
};


PayloadType h261 = {
	PAYLOAD_VIDEO,
	90000,
	0,
	NULL,
	0,
	0,
	"h261/90000/1"
};

struct eXosip_t *geXcontext;

//
#endif // Import Globals


#ifdef IMPORT_GLOBAL_EXTERNS


extern char GV_SkipTerminateKey[512];
extern char GV_SkipRewindKey[512];
extern char GV_SkipForwardKey[512];
extern char GV_SkipBackwardKey[512];
extern char GV_PauseKey[512];
extern char GV_ResumeKey[512];
extern char GV_VolumeUpKey[512];
extern char GV_VolumeDownKey[512];
extern char GV_SpeedUpKey[512];
extern char GV_SpeedDownKey[512];

extern long gShmWriteCount;
extern int gLastShmUsed;
extern struct EncodedShmBase *gEncodedShmBase;
extern pthread_mutex_t gNotifyMediaMgrLock;

extern pid_t gMediaMgrPid;
extern struct MsgToDM gExitAllStruct;
extern void *gShmBase;
extern key_t gShmKey;
extern int gShmId;

extern struct DebugModuleStruct gDebugModules[9];
 
extern NameValue gSipCodes[999];
extern class Call gCall[MAX_PORTS];

extern  pid_t	gParentPid;
extern int gResId;
extern char gAuthUser[];
extern char gAuthPassword[];
extern char gAuthId[];
extern char gAuthRealm[];
extern char gFromUrl[];

extern int	gMaxCallDuration;
extern int	gShutDownMaxTime;

extern int gSipOutOfLicenseResponse;

extern int gEnableIPv6Support; /// Enable IPv6 support 

extern int gSipRedirectionDefaultRule;
extern int gSipRedirectionUseTable;

extern int	gRejectEmptyInvite;
extern int gSipEnableEnum;     ///This toggles outbound ENUM lookups
extern char gSipEnumTLD[256]; ///This is the top level domain needed for ENUM Lookups

extern int  gFaxDebug;
extern char gT30FaxStationId[100];
extern char gT30HeaderInfo[100];
extern int  gT30ErrorCorrection;
extern int  gT38Enabled;
extern int  gT38Transport;
extern int  gT38ErrorCorrectionMode;
extern int  gT38ErrorCorrectionSpan;
extern int  gT38ErrorCorrectionEntries;
extern int  gT38MaxBitRate;
extern int  gT38PacketInterval;
extern int  gT38FaxFillBitRemoval;
extern int  gT38FaxVersion;
extern int  gT38FaxTranscodingMMR;
extern int  gT38FaxTranscodingJBIG;
extern int  gT38FaxRateManagement;
extern int  gT38FaxMaxBuffer;
extern int  gT38FaxMaxDataGram;
extern int  gUdptlStartPort;
extern int  gUdptlEndPort;
extern int  gSendFaxTone;

extern int gSipSendKeepAlive;
extern int gSipSignaledDigits;
extern int gSipSignaledDigitsMinDuration;
extern int gSipSignaledDigitsEventExpires;

extern char gSipUserAgent[];        // BT-62

// RFC 4028 Session Expires
extern int gSipMinimumSessionExpires;
extern int gSipSessionExpires;

extern int gOutboundProxyRequired;		///This variable is set to 1 if Call Manager will be using an outbound proxy.

// Header Lis
extern char gHeaderList[1024 * 10];
extern char gHeaderListArray[100][100];

extern int gTrailSilenceDetection;	///This variable is SIP_VideoSupport from .TEL.cfg
extern int gMaxAcuPorts;
extern int gSipOutputDtmfMethod;	///This variable tells us what form of output dtmf to send.
extern int gRfc2833Payload;	///This variable tells us what payload type we will be using for receiving dtmf.
extern int gSipPort;		///This variable is the port that eXosip will listen on and is set to 5060 by default.
extern int gSipRedirection; 	///This variable is set to 1 if there are multiple Dynamic Managers running.
extern int gSipRedirectorSpans; 
extern int gSipProxy; 	///
extern int gVfuDuration; 	///This variable is the duration in milliseconds after which VFU message will be sent repeatedly.
extern int gBeepSleepDuration; 	///This variable is the duration for which silence will be recorded while playing the beep.
extern char gOutboundProxy[256];           ///This variable is the outbound proxy that we will be using.
extern char gDefaultGateway[256];	///This variable tells us what the default gateway is.
extern char gPreferredCodec[256];	///This variable is set to the codec that we will be using for this IVR.
extern char gAMRIndex[256];		///This variable is set to the AMR Index mode that we will be using for this IVR.
extern int	gMaxPTime;				///This variable is set to the AMR Maxptime that we will be using for this IVR.
extern int	gPreferredPTime;    ///This variable is set to the AMR Maxptime that we will be using for this IVR.
extern char g729Annexb[32];		///This variable is set to the G729 annexb support that we will be using for this IVR.
extern char *ispbase;				///This variable is set to the isp base directory which is usually /home/arc/.ISP .
extern int maxNumberOfCalls;    	///This variable is the maximum number of calls that Call Manager can receive.
extern char gHostName[256];		///This variable is the host name of the box Dynamic Manager is running on.
extern char gHostIp[48];
extern char gContactIp[48];
extern char gFaxSdpIp[48];
extern char gSdpIp[48];
extern char gHostIf[48];
extern char gHostTransport[10];
extern char	gCacheDirectory[256];
extern char	gSystemPhraseRoot[256];
extern char	gSystemLanguage[256];
extern char	gTransferAAIEncoding[256];
extern char	gTransferAAIParamName[256];
extern char gTransferAAIPrefix[32];
extern int gUUIEncoding;

//
// Tone detection Variables, these are floats
// but they will have to accept 
//

extern int gToneDebug;

// tone detection only 

extern float gToneThreshMinimum;
extern float gToneThreshDifference;
extern float gToneDetectionNoiseThresh;
extern float gHumanAnsweringThresh;
extern int gAnsweringMachineThresh;
extern int   gAnsweringLeadingAudioTrim; 

// calll recording 
extern float gCallRecordingNoiseThresh;

// answering only 
extern float gAnsweringDetectionNoiseThresh;
extern int   gAnsweringMachineThresh;
extern int   gAnsweringTrailSilenceTrigger;
extern int   gAnsweringOverallTimeout;
extern int   gAnsweringLeadingAudioTrim;

//
// end tone detection 
//


extern int LOCAL_STARTING_RTP_PORT;	///This variable is the starting port that we will use for assigning incoming rtp packets.

extern char gToRespFifo[256];		///This variable gives us the name of the FIFO used to communicate with Responsibility.
extern char gFifoDir[256];			///This variable lets us know which directory to create and find FIFO's.
extern char gGlobalCfg[256];		///This variable contains the path where we can find the .Global.cfg file.
extern char gIpCfg[256];			///This variable contains the path where we can find the .TEL.cfg file.
extern char gIspBase[256];			///This variable contains the path to the ISP base directory.
extern char gProgramName[64];	
extern int gMrcpTTS_Enabled;
extern int gGoogleSR_Enabled;
extern int	gNumAculabResources;
extern int	gDnisSource;
//extern int	gAculabTrialSilenceEnabled;

extern char g729b_silenceBuffer[2];

extern int gWritePCForEveryCall;			///This variable is set to 1 if we need to write a Present Call Record for each call.
extern int gMrcpVersion;
extern int gSymmetricRtp;  ///This variable is SIP_SymmetricRtp  from .TEL.cfg
extern int	gPercentInboundAllowed;


/**
Variables of X instances of Dynamic Manager
*/
extern int gStartPort;		///This is the start port for this Call Manager.
extern int gEndPort;		///This is the end port for this Call Manager.
extern int gDynMgrId;		///This is the Id for this Dynamic Manager.
/*END: Variables of X instances of Dynamic Manager*/

/*Minimum interdigit delay for RFC2833*/
extern int gInterDigitDelay;				///This is the minimum time that two digits can be pressed and detected by the ortp stack.

/*Whether rings should be simulated for outbound calls..*/
extern int gSimulateCallProgress;

/*Recycle Values for ArcSipRedirector*/
extern int gCallMgrRecycleBase;
extern int gCallMgrRecycleDelta;


/*Answering Machine Detection variable*/
extern int gDetectAnsweringMachine;

extern char offset127;
extern char offset0xD5;
extern char offset0;

extern PayloadType pcmu8000; 
extern PayloadType pcma8000; 
extern PayloadType pcm8000;
extern PayloadType g729_8000;
extern PayloadType g722_8000;
extern PayloadType lpc1016;
extern PayloadType gsm;
extern PayloadType amr; 
extern PayloadType mpv;
extern PayloadType h261; 

/* DTMF Tone Detection Values */
extern float gDtmfToneRange;
extern float gDtmfToneThreshold;
extern int	 gDtmfToneFlushTime;
extern int	 gEarlyMediaSupported;
extern int	 gRefreshRtpSession;
extern int 	 gInboundEarlyMedia;
extern int   gPrackMethod;

extern struct eXosip_t *geXcontext;
#endif 

int
isModuleEnabled (int zModule);

int getBridgeCallPacket(int zCall, char*zBuffer, int *bufferLen);

int saveBridgeCallPacket(int zCall, char*zBuffer, int zDtmfValue, int bufSize);

int getBridgeCallVideoPacket(int zCall, char **zBuffer);


int saveBridgeCallVideoPacket(int zCall, char*zBuffer, int zDtmfValue, int dataLength);

int getWavHeaderLength(char *filebuff, int len);

int arc_fax_clone_globals_to_gCall(int zCall);

///Trim white space from the front and back of a string.
/*----------------------------------------------------------------------------
Trim white space from the front and back of a string.
----------------------------------------------------------------------------*/
int
trim_whitespace (char *string);

/**
------------------------------------------------------------------------------
Program:	get_name_value
Purpose:	This routine gets a name value pair from a file. If no header
		is specified, the first instance of the name found in the file
		is processed.
		This routine was adapted from gaGetProfileString, but has no
		dependencies on gaUtils.
		Parameters: 
		section: name of section in file, e.g. [Header]. If this is
			NULL (""), the whole file is searched regardless of
			headers.
		name: The "name" of a "name=value" pair.
		defaultVal: The value to return if name is not found in file.
		value: The "value" of a "name=value" pair. This what you are
			looking for in the file.
		bufSize: Maximum size of the value that can be returned. 
			This is so the routine will not cause you to core dump.
		file: the name of the file to search in
		err_msg: The errror message if any.
		Values returned are: 0 (success), -1 (failure)
Author:		George Wenzel
Date:		06/17/99
------------------------------------------------------------------------------
*/

int write_headers_to_file(int port, osip_message_t *msg);

int isItFreshDtmf (int zCall, struct timeval zTimeb);

int lookup_host (const char *host);

int notifyMediaMgr (int zLine, int zCall, struct MsgToDM *zpMsgToDM, char *mod);

int
get_name_value (char *section, char *name, char *defaultVal, char *value,
		int bufSize, char *file, char *err_msg);

void
getRingEventTime (char *current_time);

void
setTime (char *time_str, long *tics);

int
arc_mkdir(char *zDir, int mode_t);

int
loadConfigFile (const char *progname, const char *path);

int printFaxConfig ();
int printFaxByPort(int zCall);

int arg_switch(char *mod, int parameter, char *value);

int parse_arguments(char *mod, int argc, char *argv[]);

unsigned long mCoreAPITime();

void MSleep(unsigned long milsecs);

int util_u_sleep(long microSeconds);

void arcDynamicSleep(unsigned long  dwMilsecs, unsigned long  *dwPrevSleepTimestamp, unsigned long  *dwExpectSleepTimestamp);

int 
arc_exit (int zCode, char *zMessage);

int arc_kill(int zpid, int zsiginf);

int removeSharedMem (int zCall);

int writeToResp (char *mod, void *zpMsg);

/*
  these are for third leg calling routines to convert 
  bash to actual zCall indexes 
*/


#define ZCALL_INDEX_TO_CROSSREF(rc, zCall, spanwidth)\
 do { rc = (zCall % spanwidth); } while (0) \


#define CROSSREF_INDEX_TO_ZCALL(rc, idx, dynmgrid, spanwidth)\
 do { if(idx == -1) {rc = -1;} else if(dynmgrid == 0){ rc = idx; } else { rc = (dynmgrid * spanwidth) + (idx % 48); } } while(0)\


int gCallSetCrossRef(Call *gCall, size_t size, int zIndex, Call *ref, int zCall);

int gCallDelCrossRef(Call *gCall, size_t size, int zIndex, int zCall);

void gCallDumpCrossRefs(Call *gCall, int zIndex);

int gCallFindThirdLeg(Call *gCall, int size, int myleg, int crossport); 

int SendMediaMgrPortSwitch(int appCallNum, int deadleg, int aleg, int bleg, int disconnect);

int SendMediaManagerPortDisconnect(int portno, int notifyApp);

int arc_audio_codec_lookup(const char *a_line, int *rtpmap, int *bitrate);

/*ORTP*/ 

#endif
