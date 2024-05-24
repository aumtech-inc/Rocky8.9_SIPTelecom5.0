/*------------------------------------------------------------------------------
Program Name:	ArcSipMediaMgr
File Name:		ArcSipMediaMgr.c
Purpose:  		Media Manager for osip/ortp based sip-ivr.
Author:			Aumtech, inc.
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
Update: 08/02/2004 DDN Created the file
Update: 09/16/2005 DDN set_local_address pass 0.0.0.0
Update: 10/05/2005 Added ability to have x instances of Media Manager
Update: 12/01/2005 RG Changed inTsIncrement to codecBufferSize
Update: 03/09/2006 djb MR# 975.  Added payload type logic to the rtp audio.
Update: 09/12/2007 djb Added mrcpTTS logic.
Update: 06/14/2010 RG Added G722 codec.
Update: 06/22/2010 djb Added blastDial logic
Update: 07/08/2011 djb MR 3371/3427.  Updated logging messages.
Update: 09/24/2014 djb MR 3839 - removed references to MRCPv1
Update: 09/24/2014 DDN MR 47XX - Core in spandsp routine: Added -99 for no rfc 2833.
------------------------------------------------------------------------------*/
/*
 * Design doc: ../doc/DESIGN
 */
#define IMPORT_GLOBALS
#include <ArcSipMediaMgr.h>
#include "dynVarLog.h"

// DJB debug
#include <mcheck.h>
// end DJB debug

#include "mbuffer.h"
#include "mutils.h"
#include "mrtppacket.h"
#include "md5.h"

#include "dl_open.h"

#include "arc_tones_ringbuff.h"
#include "arc_small_conference.h"
#include "arc_audio_frame.h"
#include "arc_decode.h"
#include "arc_encode.h"
#include "clock_sync.h"
#include "arc_fax_session.h"
#include "arc_udptl.h"

// playback speed routines 
#include "arc_prompt_control.hh"

// debug macro for hiding fprintfs 
#include "jsprintf.h" 
#define ARC_DEBUG  0

#ifdef VOICE_BIOMETRICS 
extern "C"
{
#include "VoiceID_Au_publicAPI.h"
#include "VoiceIDutils.h"
}
#endif // END: VOICE_BIOMETRICS

// GOOGLE_SR	- BT-13
#define	GSR_STREAMING_REQUEST	1		// mm -> java client
#define	GSR_RESPONSE			2		// java client -> mm
#define	GSR_RESULT				3		// java client -> mm
#define	GSR_CANCEL_RECOG		4		// mm -> java client ---  not used
#define	GSR_RECORD_REQUEST		5		// mm -> java client

// defines for indexes into the 
// audio decoding routines 

#define AUDIO_IN 0
#define AUDIO_OUT 1
#define AUDIO_MIXED 2
#define CONFERENCE_AUDIO 3

// int rtpSleep_orig(int millisec, struct timeb *zLastSleepTime)
//#define rtpSleep(millisec, zLastSleepTime) rtpSleep_orig(millisec, zLastSleepTime, __LINE__) 
#define rtpSleep(millisec, zLastSleepTime, zLine, zCall) rtpSleep_orig(millisec, zLastSleepTime, zLine, zCall)

#ifdef ACU_LINUX

#include "acu_type.h"
#include "res_lib.h"
#include "acu_os_port.h"
#include "smrtp.h"

int             gSTonesFifoFd = -1;
int             gSTonesFaxFifoFd = -1;

//char           gSTonesFifo[256];

int             gConfMgrFifoFd = -1;
char            gConfMgrFifo[256];

#endif

extern          "C"
{
#include "ttsStruct.h"
#include "UTL_accessories.h"
#include "arcFifos.h"
}

struct dtmf_data_holder arcDtmfData[MAX_PORTS];

struct arc_small_conference_t Conferences[48];

pthread_mutex_t ClearPortLock;

/* speed control */

enum speedDoNext_e
{
	ARC_SPEED_DO_NOTHING = 0,
	ARC_SPEED_NEED_MORE,
	ARC_SPEED_HAVE_MORE,
	ARC_SPEED_DONE
};

ArcPromptControl *promptControl[48] = { NULL };

/* fax */
arc_fax_session_t FaxSession[48] = { NULL };
arc_udptl_tx_t  FaxUdptlTx[48] = { NULL };
arc_udptl_rx_t  FaxUdptlRx[48] = { NULL };
arc_fax_session_parms_t FaxParms[48];

struct t38_parms_t
{
	int             t38_ecm;
	int             t38_span;
	int             t38_entries;
	char            local_ip[100];
	unsigned short  local_port;
	char            remote_ip[100];
	unsigned short  remote_port;
};

t38_parms_t     T38Parms[48];

/*     */

#ifdef VOICE_BIOMETRICS

#define CONTINUOUS_VERIFY_INACTIVE          0
#define CONTINUOUS_VERIFY_ACTIVE            1
#define CONTINUOUS_VERIFY_ACTIVE_COMPLETE   2

int     gAvb_SpeechSize;
int     gAvb_SpeechSleep;

char    gAvb_PCM_PhraseDir[128] = "";

#endif  // END: VOICE_BIOMETRICS



/* speed control */

int             gCanExit = 0;
int             gCanReadShmData = 1;
long            gShmReadCount = 0;

int             gTotalOutputThreads = 0;
int             gTotalInputThreads = 0;

long            gMallocCount = 0;
long            gMallocAmount = 0;
long            gFreeCount = 0;
long            gFreeAmount = 0;

int             gRequestFifoFd;
char            gRequestFifo[256];

int             gGoogleSRRequestFifoFd = -1;
char            gGoogleSRRequestFifo[256];

int             gGoogleSRResponseFifoFd = -1;
char            gGoogleSRResponseFifo[256];


time_t          yTmpApproxTime;

pthread_attr_t  gShmReaderThreadAttr;
pthread_t       gShmReaderThreadId;

pthread_attr_t  gGoogleReaderThreadAttr;
pthread_t       gGoogleReaderThreadId;

pthread_attr_t  gpthread_attr;

pthread_mutex_t gMutexDebug;
pthread_mutex_t gMutexLog;

pthread_mutex_t gOpenFdLock;

struct arc_clock_sync_t *clock_sync_in = NULL;

// struct arc_clock_sync_t *clock_sync_out = NULL;

#ifdef __DEBUG__
#define __DDNDEBUG__(module, xStr, yStr, zInt) arc_print_port_info(mod, module, __LINE__, zCall, xStr, yStr, zInt)
#else
#define __DDNDEBUG__(module, xStr, yStr, zInt) ;
#endif

#define ARC_TYPE_FILE 0
#define ARC_TYPE_FIFO 1

#define MAX_DWORD           (0xFFFFFFFF)
char            gSilenceBuffer[2 * 1600];
int             gIntSilenceBufferSize = 1600;

int             addToPlayList (struct Msg_PlayMedia *zpPlay,
							   long *zpQueueElementId);
unsigned long   MCoreAPITime ();
void            MSleep (unsigned long milsecs);
void            ArcDynamicSleep (unsigned long dwMilsecs,
								 unsigned long *dwPrevSleepTimestamp,
								 unsigned long *dwExpectSleepTimestamp);
unsigned long   msec_gettimeofday ();
int             speakDtmf (int sleep_time,
						   int interrupt_option,
						   char *zFileName,
						   int synchronous,
						   int zCall,
						   int zAppRef,
						   RtpSession * zSession,
						   int *zpTermReason, struct MsgToApp *zResponse);
int             speakStream (int sleep_time,
							 int interrupt_option,
							 char *zFileName,
							 int synchronous,
							 int zCall,
							 int zAppRef,
							 RtpSession * zSession,
							 int *zpTermReason, struct MsgToApp *zResponse);
int             clearPort (int zCall, char *mod, int canClearStreamData,
						   int canDeleteResponseFifo);
int             getStreamingType (char *zUrl);
int             printSpeakList (SpeakList * zpList);
int             processSpeakMrcpTTS (int zCall, MsgToDM * zMsgToDM,
									 int *zpTermReason,
									 struct MsgToApp *zResponse,
									 int interrupt_option);
int             process_DMOP_END_CALL_PROGRESS (int zCall);

/* wrappers for various audio functions */

int				arc_conference_delete_by_name (int zCall, struct arc_small_conference_t *c, size_t size,
							   char *name);
int             arc_conference_find_by_name (int zCall, struct arc_small_conference_t *c,
											 size_t size, char *name);

int             arc_conference_decode_and_post (int zCall, char *buff,
												size_t bufsize,
												int audio_opts, char *func,
												int line);

int             arc_conference_read_and_encode (int zCall, char *buff,
												size_t bufsize,
												int audio_opts, char *func,
												int line);

// end conference

int             arc_do_inband_detection (char *buff, size_t size, int zCall,
										 char *func, int line);

int             arc_frame_decode_and_post (int zCall, char *buff,
										   size_t bufsize, int which,
										   int audio_opts, char *func,
										   int line);

int             arc_frame_read_and_encode (int zCall, char *buff,
										   size_t bufsize, int which,
										   char *func, int line);

int             arc_frame_reset_with_silence (int zCall, int which,
											  int numframes, char *func,
											  int line);

int             arc_frame_record_to_file (int zCall, int what, char *func,
										  int line);

int             arc_frame_apply (int zCall, char *buff, size_t bufsize,
								 int which, int op, char *func, int line);

int             initializeDtmfToneDetectionBuffers (int zCall);

void sendAudioToSomewhere(char *buf, int bufSize, FILE *fp);

int startOutputThread(int zCall);
int restartOutputSession(int zCall);

int googleStopActivity(int zCall);
/* */
#ifdef VOICE_BIOMETRICS

extern "C"
{

static int process_DMOP_VERIFY_CONTINUOUS_SETUP (int zCall);
static int process_DMOP_VERIFY_CONTINUOUS_GET_RESULTS (int zCall);
static void stop_avb_processsing(int zCall, int zReturnCode, int zVendorCode,
                                            char *zErrMsg);
static void avb_process_buffer(int zCall, int bufferSize);
static void closeVoiceID(int zCall);
}

static void readAVBCfg();
#endif // END: VOICE_BIOMETRICS




#ifdef ACU_RECORD_TRAILSILENCE
A_DATA          gACard;

#define TONES_LOG_BASE      3500
ACU_OS_CRIT_SEC *LOG_CS;
#endif

/*
 * Quick converter for telephony type to DTMF char
 * The call back functions for RFC 2833 need this array
 */
static int      dtmf_tab[17] =
	{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '#', 'A', 'B',
'C', 'D', '\0' };
int
get_dtmf_position (char dtmf)
{
	switch (dtmf)
	{
	case '0':
		return 0;
		break;
	case '1':
		return 1;
		break;
	case '2':
		return 2;
		break;
	case '3':
		return 3;
		break;
	case '4':
		return 4;
		break;
	case '5':
		return 5;
		break;
	case '6':
		return 6;
		break;
	case '7':
		return 7;
		break;
	case '8':
		return 8;
		break;
	case '9':
		return 9;
		break;
	case '*':
		return 10;
		break;
	case '#':
		return 11;
		break;
	case 'A':
		return 12;
		break;
	case 'B':
		return 13;
		break;
	case 'C':
		return 14;
		break;
	case 'D':
		return 15;
		break;
	default:
		return 16;
	}

	return 16;

}/*END: get_dtmf_position */

int             startInbandToneDetectionThread (int zCall, int zLine, int mode);
int             stopInbandToneDetectionThread (int zLine, int zCall);

int				googleStartInbandToneDetectionThread (int zCall, int zLine, int mode);

#if 0

int
is_all_zeros (char *buff, size_t size, int line, const char *func)
{

	int             i;
	int             rc = 0;

	for (i = 0; i < size; i++)
	{
		if (buff[i] == 0)
		{
			rc++;
		}
	}

	if (rc == size && rc)
	{
//		fprintf (stderr,
//				 " %s() buff is all zeros, line=%d function=%s zero count=%d\n",
//				 __func__, line, func, rc);
	}

	return rc;
}

#endif

int
arc_kill_mm (int zpid, int zsiginf)
{
	static char     mod[] = "arc_kill";
	int				rc;

	rc = kill (zpid, zsiginf);

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Killed pid=%d from ArcSipMediaMgr with %d; rc=%d.", zpid, zsiginf, rc);
	return(rc);

}

/*print debug info viz. function name, line number, port num.*/
///Prints debug info and contains the function that called it, the line, the port, and the time into nohup.out.
void
arc_print_port_info (const char *function,
					 int module,
					 int zLine,
					 int zCall, char *zData1, char *zData2, int zData3)
{
	struct timeb    tb;
	char            t[100];
	char           *tmpTime;
	char            yStrCTime[256];

	if (isModuleEnabled (module) == 0)
	{
		return;
	}

	ftime (&tb);

	tmpTime = ctime_r (&(tb.time), yStrCTime);

	sprintf (t, "%s", yStrCTime);

	t[strlen (t) - 1] = '\0';

	if (zCall < gStartPort || zCall > gEndPort)
	{
		printf ("ARCDEBUG:%d:%s:%d:State_%d:%s:%s:%d:%s:%d:%s:%s:%d\n",
				gDynMgrId,
				gDebugModules[module].label,
				zCall,
				-1,
				__FILE__,
				function, zLine, t, tb.millitm, zData1, zData2, zData3);
	}
	else
	{
		printf ("ARCDEBUG:%d:%s:%d:State_%d:%s:%s:%d:%s:%d:%s:%s:%d\n",
				gDynMgrId,
				gDebugModules[module].label,
				zCall,
				gCall[zCall].callState,
				__FILE__,
				function, zLine, t, tb.millitm, zData1, zData2, zData3);
	}

	fflush (stdout);

}								/*END: void arc_printf */

/**
  * Generates a random 32-bit value.
  *
  * Message-digest algorithm (MD5) is adopted for generating random numbers.  
  * See RFC1321 documentation.
  * @return random 32-bit value.
  */
unsigned long
ArcMGenMD32 (unsigned char *string,	// concatenated "randomness values"
			 long len			// size of randomness values
	)
{
	union
	{
		char            c[16];
		unsigned long   x[4];
	} digest;
	int             i;
	unsigned long   r = 0;
	MD5_CTX         context;

	MD5Init (&context);
	MD5Update (&context, string, len);
	MD5Final ((unsigned char *) &digest, &context);

	for (i = 0; i < 3; i++)
	{
		r ^= digest.x[i];
	}

	return r;
};

void
generateAudioSsrc (int zCall)
{
	struct
	{
		int             type;
		DWORD           cpu;
		int             pid;
	} s;
	static int      type = 0;

	s.type = type++;
	s.cpu = MCoreAPITime ();;
	s.pid = (unsigned int) pthread_self ();
	gCall[zCall].audioSsrc = ArcMGenMD32 ((unsigned char *) &s, sizeof (s));
}								/*END: void generateAudioSsrc */

int
rtpSleep_orig (int millisec, struct timeb *zLastSleepTime, int line,
			   int zCall)
{
	int             rc = -1;
	static char     mod[] = "rtpSleep";
	long            yIntSleepTime = 0;
	long            yIntActualSleepTime = 0;
	long            yIntSleepTimeDelta = 0;
	struct timeb    yCurrentTime;

#if 0
	long            yIntStartMilliSec;
	long            yIntStopMilliSec;
#endif

	ftime (&yCurrentTime);

	if (zLastSleepTime->time == 0)
	{
		util_u_sleep (millisec * 1000);
		ftime (zLastSleepTime);
	}
	else
	{
	long            current = 0;
	long            last = 0;

		current = (1000 * yCurrentTime.time) + yCurrentTime.millitm;
		last = (1000 * zLastSleepTime->time) + zLastSleepTime->millitm;

		yIntSleepTime = millisec - (current - last);

		if (yIntSleepTime > 0)
		{

			if (yIntSleepTime > millisec * 10)
			{
#if 0
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Unusually high sleep time(%d). Expected (%d)",
						   yIntSleepTime, millisec);
#endif

				if (!canContinue ("rtpSleep", zCall, line))
				{
					util_u_sleep (millisec * 1000);
					return (0);
				}

				ftime (zLastSleepTime);
				util_u_sleep (millisec * 1000);
				return (0);
			}

			util_u_sleep (yIntSleepTime * 1000);
		}

		ftime (zLastSleepTime);

		current = (1000 * yCurrentTime.time) + yCurrentTime.millitm;
		last = (1000 * zLastSleepTime->time) + zLastSleepTime->millitm;

		yIntActualSleepTime = last - current;
		yIntSleepTimeDelta = yIntActualSleepTime - yIntSleepTime;

		if (yIntSleepTimeDelta > 0)
		{
			if (zLastSleepTime->millitm < yIntSleepTimeDelta)
			{
				zLastSleepTime->time--;
				zLastSleepTime->millitm += 1000;
			}
			zLastSleepTime->millitm =
				zLastSleepTime->millitm - yIntSleepTimeDelta;
		}

	}

#if 0
	yIntStopMilliSec = zLastSleepTime->millitm + zLastSleepTime->time * 1000;
	yIntStartMilliSec = yCurrentTime.millitm + yCurrentTime.time * 1000;

	if ((yIntStopMilliSec - yIntStartMilliSec) > 100)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, 3278, INFO,
				   "Sleeping for (%d ms), but slept for(%d ms)",
				   millisec, yIntStopMilliSec - yIntStartMilliSec);
	}
#endif

	return 0;

}/*END: int rtpSleep() */

///This function is designed for sleeping after sending/receiving RTP packets.
int
rtpSleep_withbreaks (int sleepTime, struct timeb *zLastSleepTime,
					 int zCall, int interrupt_option, int zLine)
{
	int             rc = -1;
	char            mod[] = "rtpSleep_withbreaks";
	long            yIntSleepTime = 0;
	struct timeb    yCurrentTime;
	int             millisec = sleepTime;
	int             numOfTimesSleep = 1;

	if (zLastSleepTime->time == 0)
	{
		if (millisec > 100)
		{
			numOfTimesSleep = millisec / 20;
			millisec = 20;
		}
		for (; numOfTimesSleep > 0; numOfTimesSleep--)
		{

			if (!canContinue (mod, zCall, __LINE__))
			{
				return 0;
			}
			else if (gCall[zCall].pendingOutputRequests > 0)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Got pendingOutputRequests=%d. Returning from sleep.",
						   gCall[zCall].pendingOutputRequests);
				return 0;
			}

			if (gCall[zCall].speechRec == 0 &&
				gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
			{
				if (interrupt_option != 0 && interrupt_option != NONINTERRUPT &&	/*0: No interrupt */
					((gCall[zCall].GV_DtmfBargeinDigitsInt == 0) ||
					 (gCall[zCall].GV_DtmfBargeinDigitsInt == 1 &&
					  strchr (gCall[zCall].GV_DtmfBargeinDigits,
							  dtmf_tab[gCall[zCall].lastDtmf]) != NULL)))
				{
					if (gCall[zCall].dtmfAvailable == 1)
					{
						break;
					}
				}
			}
			util_u_sleep (millisec * 1000);
		}
	}
	else
	{
	long            current = 0;
	long            last = 0;

		ftime (&yCurrentTime);

		current = (1000 * yCurrentTime.time) + yCurrentTime.millitm;
		last = (1000 * zLastSleepTime->time) + zLastSleepTime->millitm;

		yIntSleepTime = sleepTime - (current - last);

		if (yIntSleepTime > 100)
		{
			numOfTimesSleep = yIntSleepTime / 20;
			yIntSleepTime = 20;
		}

		for (; numOfTimesSleep > 0; numOfTimesSleep--)
		{
			if (!canContinue (mod, zCall, __LINE__))
			{
				return 0;
			}
			else if (gCall[zCall].pendingOutputRequests > 0)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Got pendingOutputRequests=%d. Returning from sleep.",
						   gCall[zCall].pendingOutputRequests);
				return 0;
			}

			if (gCall[zCall].speechRec == 0 &&
				gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
			{
				if (interrupt_option != 0 && interrupt_option != NONINTERRUPT &&	/*0: No interrupt */
					((gCall[zCall].GV_DtmfBargeinDigitsInt == 0) ||
					 (gCall[zCall].GV_DtmfBargeinDigitsInt == 1 &&
					  strchr (gCall[zCall].GV_DtmfBargeinDigits,
							  dtmf_tab[gCall[zCall].lastDtmf]) != NULL)))
				{
					if (gCall[zCall].dtmfAvailable == 1)
					{
						break;
					}
				}
			}
			util_u_sleep (yIntSleepTime * 1000);
		}
	}
	ftime (zLastSleepTime);

	return 0;

}/*END: int rtpSleep_withbreaks() */

///This function is simply a sleep for doing a nanosecond sleep
int
nano_sleep (int Seconds, int nanoSeconds)
{
	static char     mod[] = { "nano_sleep" };
	struct timespec lTimeSpec1, lTimeSpec2;

	lTimeSpec1.tv_sec = Seconds;
	lTimeSpec1.tv_nsec = nanoSeconds;
	nanosleep (&lTimeSpec1, &lTimeSpec2);

	return (0);

}/*END: int nano_sleep */

#if 0
///This function prints out log messages into the /home/arc/.ISP/Log/ISP.cur file.
int
dynVarLog (int zLine, int zCall,
		   char *zpMod, int mode,
		   int msgId, int msgType, char *msgFormat, ...)
{
	static char     mod[] = { "dynVarLog" };
	va_list         ap;
	char            m[1024];
	char            m1[512];
	char            type[32];
	int             rc;
	char            lPortStr[10];

	switch (msgType)
	{
	case 0:
		strcpy (type, "ERR: ");
		break;
	case 1:
		strcpy (type, "WRN: ");
		break;
	case 2:
		strcpy (type, "INF: ");
		break;
	default:
		strcpy (type, "INF: ");
		break;
	}

	pthread_mutex_lock (&gMutexLog);

	memset (m1, '\0', sizeof (m1));
	va_start (ap, msgFormat);
	vsprintf (m1, msgFormat, ap);
	va_end (ap);

	sprintf (m, "%s[%d]%s:pid=%d", type, zLine, m1, getpid ());

	sprintf (lPortStr, "%d", zCall);

	LogARCMsg (zpMod, mode, lPortStr, "DYN", "ArcSipMediaMgr", msgId, m);

	pthread_mutex_unlock (&gMutexLog);

	return (0);

}								/*END: int dynVarLog */
#endif

///This is a function opens either a FIFO or a file for writing and writes a debug message.
int
arc_open (int zCall, char *mod, int zLine, char *zFile, int flag,
		  int fileType)
{
	int             yFd = -1;

	yFd = open (zFile, flag);

	switch (fileType)
	{
	case ARC_TYPE_FIFO:
		__DDNDEBUG__ (DEBUG_MODULE_FILE, "Opened FIFO for ", zFile, yFd);
		break;

	default:
		__DDNDEBUG__ (DEBUG_MODULE_FILE, "Opened FILE for ", zFile, yFd);
		break;
	}

	return (yFd);

}								/*END: int arc_open */

///This is a function closes any file descriptor and writes a debug message.
int
arc_close (int zCall, char *mod, int zLine, int *zFd)
{
	int             yRc = -1;

	__DDNDEBUG__ (DEBUG_MODULE_FILE, "Closing fd", "", *zFd);

//	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//			"Called from %d; closing fd %d.", zLine, *zFd);
	if (*zFd == 0)
	{
		*zFd = -1;
		return 0;
	}

	yRc = close (*zFd);
	*zFd = -1;
	return yRc;

}								/*END: int arc_close */

///This function mallocs a specific amount of memory and writes a debug message in nohup.out.
void           *
arc_malloc (int zCall, char *mod, int zLine, int zSize)
{
	void           *lpData = NULL;
	char            yMessage[256];

	lpData = malloc (zSize);

	gMallocCount++;

	gMallocAmount += zSize;

	sprintf (yMessage,
			 "[%s:%d] Malloced location(%p), Malloc(%ld) for (%ld) bytes. "
			 "Free(%ld) for (%ld) bytes.",
			 mod,
			 zLine,
			 lpData, gMallocCount, gMallocAmount, gFreeCount, gFreeAmount);

	__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "", yMessage, -1);

	return (lpData);

}								/*END: void * arc_malloc */

///This function frees the memory pointed to by zPts and writes a debug message in nohup.out.
void
arc_free (int zCall, char *mod, int zLine, void *zPtr, int zSize)
{
	char            yMessage[256];

	gFreeCount++;

	gFreeAmount += zSize;

	sprintf (yMessage,
			 "[%s:%d] Freeing location(%p), Malloc(%ld) for (%ld) bytes. "
			 "Free(%ld) for (%ld) bytes.",
			 mod,
			 zLine,
			 zPtr, gMallocCount, gMallocAmount, gFreeCount, gFreeAmount);

	__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "", yMessage, -1);
//	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//			"DJB:[called from %d] %s", zLine, yMessage);

}								/*END: void arc_free */

///This function will unlink the file pointed to by zFile.
int
arc_unlink (int zCall, char *mod, char *zFile)
{

	__DDNDEBUG__ (DEBUG_MODULE_FILE, "Unlinking", zFile, zCall);
	return (unlink (zFile));

}								/*END: int arc_unlink */

///Debug purpose wrapper for rtp_session_send_with_ts
gint
arc_rtp_session_send_with_ts (char *mod, int line, int zCall,
							  RtpSession * session,
							  char *buffer,
							  gint len, guint32 userts, gint markBit = 0)
{
	char            yStrMessage[256];

	if (gCall[zCall].restart_rtp_session == 1
		 && session == gCall[zCall].rtp_session)
	{
		gCall[zCall].restart_rtp_session = 0;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		   "arc_rtp_session_send_with_ts calling restartOutputSession.");

		restartOutputSession(zCall);
		session = gCall[zCall].rtp_session;
	}

	if (session != NULL)
	{

		int rc =  rtp_session_send_with_ts (session,
										 (gchar *) buffer,
										 len, userts, 0, markBit);

		return rc;
	}
	else
	{
		return -1;
	}

}/*END: gint arc_rtp_session_send_with_ts */



/* 
  Only if the ssrc is set from clearPort and and session is the outgoung rtp session from call manager do we set this 
*/

int 
arc_rtp_session_set_ssrc(int zCall, RtpSession *session){

   if(zCall < 0 || zCall >= MAX_PORTS){
//	 fprintf(stderr, " %s() failed z-index test returning -1\n", __func__);
     return -1;
   }

   // only set the ssrc if it is the telecom 

   if((session != NULL) && (session == gCall[zCall].rtp_session)){
	 //fprintf(stderr, " rtp session on entry=%X htonl(%X)\n", session->ssrc, htonl(session->ssrc));
     if(gCall[zCall].audioSsrc != 0){
       rtp_session_set_ssrc(session, gCall[zCall].audioSsrc);
	   //fprintf(stderr, " rtp session on exit=%X htonl(%X)\n", session->ssrc, htonl(session->ssrc));
     }
   }
   return 0;
}

static int reset_cnt[48] = {0};


enum arc_rtp_which_e {
  ARC_RTP_IN = (1<<0),
  ARC_RTP_OUT = (1<<1)
};


static int arc_rtp_session_reset_by_counter(int zCall, int rc, int which){

   int idx = zCall % 48;
   int thresh = 10;

   if(rc == 0){
    reset_cnt[idx]++;
   } else {
     reset_cnt[idx] = 0;
   }

   if(reset_cnt[idx] == thresh){
     // fprintf(stderr, " %s() resetting rtp after %d lost packets\n", __func__, thresh);
     gCall[zCall].resetRtpSession = 1;

     if(which & ARC_RTP_IN){
      rtp_session_reset(gCall[zCall].rtp_session_in);
     }
     if(which & ARC_RTP_OUT){
      rtp_session_reset(gCall[zCall].rtp_session);
     }
   }

   return 0;
}


static inline int
arc_rtp_session_update_ts_in(int zCall, int rc, int *more){

 switch(rc){
    case -1:
     JSPRINTF(" %s() recieved -1 from ortp_recv zCall=%d\n", __func__, zCall);
     break;
	case 0:
     JSPRINTF(" %s() recieved 0 from ortp_recv ts=%d zCall=%d\n", __func__, gCall[zCall].in_user_ts, zCall);
     gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement * 2;
     //gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
     break;
    default:
     JSPRINTF(" %s() recieved %d from ortp_recv ts=%d zCall=%d\n", __func__, rc, gCall[zCall].in_user_ts, zCall);
     if(more && (*more == 0)){
       gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
       JSPRINTF(stderr, " %s() more=%d zCall=%d\n", __func__, *more, zCall);
     }  else {
       JSPRINTF(" %s() more=%d zCall=%d\n", __func__, *more, zCall);
       gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
     } 
     break;
  }

  arc_rtp_session_reset_by_counter(zCall, rc, ARC_RTP_IN);
  return 0;

}


/* Wrapper for rtp recv added to try and regain sync during tel_record */

int 
arc_rtp_session_recv_in(const char *mod, int zCall, int line, RtpSession *session, char *dest, size_t destsize, int *more){

  int rc = 0;

  if(zCall < 0 || zCall >= MAX_PORTS){
//    fprintf(stderr, " %s() failed zCall index check, look at calling function! mod=%s line=%d\n", __func__, mod, line);
    return -1;
  }

  if(session != gCall[zCall].rtp_session_in){
 //   fprintf(stderr, " %s() session != gCall[zCall].rtp_session_in! returning -1\n", __func__);
    return -1;
  }

  if(gCall[zCall].resetRtpSession == 1){
    JSPRINTF(" %s() rtp session reset was set ... doing it...\n", __func__); 
    rtp_session_reset(gCall[zCall].rtp_session_in);
    gCall[zCall].resetRtpSession = 0;
  }

  if(session != NULL){
    rc = rtp_session_recv_with_ts (gCall[zCall].rtp_session_in, dest, destsize, gCall[zCall].in_user_ts, more, 0, NULL);
    arc_rtp_session_update_ts_in(zCall, rc, more);
  }

  return rc;
}

/* wrapper for cross ports where we need to stop sending to the cross port */

gint 
arc_rtp_bridge_with_ts(const char *mod, int zCall, int line, RtpSession *in, char *buffer, gint len, gint32 userts, 
						int *more, int markbit, RtpSession *cross_session, int force=0)
{

    int rc = -1;
    int crossport = -1;

    JSPRINTF(" %s() entering from %s() lineno=%d zCall=%d session=%p buffer=%p len=%d userts=%d makrbit=%d cross_session=%p\n", 
						__func__, mod, line, zCall, in, buffer, len, userts, markbit, cross_session);

    if(gCall[zCall].resetRtpSession == 1)
	{
      JSPRINTF(" %s() rtp session reset was set ... doing it...\n", __func__); 
      rtp_session_reset(gCall[zCall].rtp_session_in);
      gCall[zCall].resetRtpSession = 0;
    }

    crossport = gCall[zCall].crossPort;

    if(crossport != -1)
	{
      JSPRINTF(" crossport =%d in speakfile==%d\n", crossport, gCall[crossport].in_speakfile);
      if(gCall[crossport].in_speakfile && (force == 0)){
      rc = rtp_session_recv_with_ts (in, buffer, len, userts, more, markbit, NULL);
      arc_rtp_session_update_ts_in(zCall, rc, more);
      } else {
        rc = rtp_session_recv_with_ts (in, buffer, len, userts, more, markbit, cross_session);
        arc_rtp_session_update_ts_in(zCall, rc, more);
      }
    } else {
      rc = rtp_session_recv_with_ts (in, buffer, len, userts, more, markbit, cross_session);
      arc_rtp_session_update_ts_in(zCall, rc, more);
    }

    JSPRINTF(" %s() exiting rc=%d more=%d\n", __func__, rc, *more);

    return rc;
}


///Debug purpose wrapper for rtp_session_sendm_with_ts
gint
arc_rtp_session_Sendm_with_ts (int zCall,
							   RtpSession * session,
							   char *buffer, gint len, guint32 userts)
{
static char     mod[] = { "arc_rtp_session_sendm_with_ts" };
char            yStrMessage[256];

mblk_t         *m = (mblk_t *) buffer;

	return rtp_session_sendm_with_ts (session, m, userts);

}								/*END: gint arc_rtp_session_send_with_ts */

///This function is used to destroy rtp sessions.
int
arc_rtp_session_destroy (int zCall, RtpSession ** prtp_session_mrcp)
{
	static char     mod[] = { "arc_rtp_session_destroy" };

	if (*prtp_session_mrcp == NULL)
	{
		return (0);
	}

	/*If it is output session, destroy it */
	if (*prtp_session_mrcp == gCall[zCall].rtp_session)
	{

		__DDNDEBUG__ (DEBUG_MODULE_RTP, "Destroying", "rtp_session", zCall);
#if 1
		if (gSymmetricRtp == 1 && gCall[zCall].outboundSocket != -1)
		{
			gCall[zCall].rtp_session->rtp.socket =
				gCall[zCall].outboundSocket;
			gCall[zCall].outboundSocket = -1;
		}
#endif
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Destroying prtp_session_mrcp->rtp.socket=%d.",
				   (*prtp_session_mrcp)->rtp.socket);

		if ((*prtp_session_mrcp)->rtp.socket == -1)
		{
			*prtp_session_mrcp = NULL;
		}
		else
		{
			rtp_session_destroy (*prtp_session_mrcp);
			*prtp_session_mrcp = NULL;
		}
	}
	/*  If it is input session, and gRefreshRtpSession is set in conf file, destroy it.   
	 *  Otherwise dont destroy it, just reset the values.
	 */
	else if (*prtp_session_mrcp == gCall[zCall].rtp_session_in)
	{

		if (gRefreshRtpSession)
		{
			__DDNDEBUG__ (DEBUG_MODULE_RTP,
						  "Destroying since gRefreshRtpSession is 1",
						  "rtp_session_in", zCall);
			rtp_session_destroy (*prtp_session_mrcp);
			*prtp_session_mrcp = NULL;
		}
		else
		{

			__DDNDEBUG__ (DEBUG_MODULE_RTP, "Resetting", "rtp_session_in",
						  zCall);

			rtp_session_reset (*prtp_session_mrcp);
		}
	}
	else if (*prtp_session_mrcp == gCall[zCall].rtp_session_mrcp_tts_recv)
	{

		__DDNDEBUG__ (DEBUG_MODULE_RTP, "Resetting",
					  "rtp_session_mrcp_tts_recv", zCall);
		rtp_session_reset (*prtp_session_mrcp);
	}
#ifdef ACU_LINUX
	/*If it is tone session, destroy it. */
	else if (*prtp_session_mrcp == gCall[zCall].rtp_session_tone)
	{
		__DDNDEBUG__ (DEBUG_MODULE_RTP, "Destroying", "rtp_session", zCall);

		rtp_session_destroy (*prtp_session_mrcp);
		*prtp_session_mrcp = NULL;
	}
	else if (*prtp_session_mrcp == gCall[zCall].rtp_session_conf_recv)
	{
		__DDNDEBUG__ (DEBUG_MODULE_RTP, "Destroying", "rtp_session", zCall);

		rtp_session_destroy (*prtp_session_mrcp);
		*prtp_session_mrcp = NULL;
	}
	else if (*prtp_session_mrcp == gCall[zCall].rtp_session_conf_send)
	{
		__DDNDEBUG__ (DEBUG_MODULE_RTP, "Destroying", "rtp_session", zCall);

		rtp_session_destroy (*prtp_session_mrcp);
		*prtp_session_mrcp = NULL;
	}
#endif

	return (0);

}								/*END: int arc_rtp_session_destroy */

///This functions checks if the current packet is the start of I-Frame
int
checkForIFrame (const void *p_data, int zCall, int zDataLen)
{
	static char     mod[] = { "checkForIFrame" };
	const unsigned char *l_pData = (const unsigned char *) p_data;

	/*Core dumping when p_data is 11 or less */

	if (zDataLen < 13)
	{
		return 0;
	}

	// Assume no RTP header extensions and skip over it
	l_pData += 12;

	// Skip over the RFC2190 header and detect i-frame flags
	switch (l_pData[0] & 0xC0)
	{
	case 0xc0:					// mode c
		//isIFrame = (l_pData[4] & 0x80) == 0;
		l_pData += 12;
		break;
	case 0x80:					// mode b
		//isIFrame = (l_pData[4] & 0x80) == 0;
		l_pData += 8;
		break;
	default:					// mode a
		//isIFrame = ((l_pData[0] & 0x40) == 0) && ((l_pData[1] & 0x10) == 0);
		l_pData += 4;
		break;
	}

#if 0
	//if the RFC 2190 header says no i-frame, then check no further
	if (isIFrame == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Its not an I-Frame.");
		return 0;
	}
#endif

	// Now look for the H.263 START PICTURE code and I-Frame  bit pattern:
	// 00000000  00000000  100000xx  xxxxxxxx  00001000
	//if (l_pData[0] != 0 || l_pData[1] != 0 || (l_pData[2]&0xfc) != 0x80 || l_pData[4] != 8)
	if (l_pData[0] != 0 || l_pData[1] != 0 || (l_pData[2] & 0xfc) != 0x80
		|| (l_pData[4] & 2) != 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Its not an I-Frame.");
		//printf("Its not an I-Frame.\n");
		return 0;
	}
	// Met of the criteria here for it being the first RTP packet of an I-Frame, start recording!

	return 1;

}								/*END: int checkForIFrame */

int
readRecordMediaFile (struct Msg_RecordMedia *zpMediaMessage,
					 struct MsgToDM *zpMsgToDM)
{
	static char     mod[] = { "readRecordMediaFile" };
	char            yStrTempFileName[128];
	char            line[256];
	FILE           *fp;

	char            lhs[128];
	char            rhs[256];

	memset (zpMediaMessage, 0, sizeof (struct Msg_RecordMedia));

	sprintf (yStrTempFileName, "%s", zpMsgToDM->data);

	if ((fp = fopen (yStrTempFileName, "r")) == NULL)
	{
		if ( canContinue (mod, zpMsgToDM->appCallNum, __LINE__) )
		{
			dynVarLog (__LINE__, zpMsgToDM->appCallNum, mod, REPORT_NORMAL,
				   TEL_FILE_IO_ERROR, ERR,
				   "Unable to open recorded media file (%s); "
				   "Cannot obtain media information. [%d, %s]",
				   yStrTempFileName, errno, strerror (errno));
		}
		else
		{
			dynVarLog (__LINE__, zpMsgToDM->appCallNum, mod, REPORT_DETAIL,
				   TEL_FILE_IO_ERROR, WARN,
				   "Unable to open recorded media file (%s); "
				   "Cannot obtain media information. [%d, %s]",
				   yStrTempFileName, errno, strerror (errno));
		}
		return (-1);
	}

	memset (line, 0, 256);

	while (fgets (line, sizeof (line) - 1, fp))
	{
		memset (lhs, 0, 128);
		memset (rhs, 0, 256);

		/*  Strip \n from the line if it exists */
		sscanf (line, "%[^=]=%s", lhs, rhs);

		if ((lhs != NULL) && (rhs != NULL) && (lhs[0] != 0) && (rhs[0] != 0))
		{
			if (strcmp (lhs, "party") == 0)
			{
				zpMediaMessage->party = atoi (rhs);
			}
			else if (strcmp (lhs, "interruptOption") == 0)
			{
				zpMediaMessage->interruptOption = atoi (rhs);
			}
			else if (strcmp (lhs, "audioFileName") == 0)
			{
				sprintf (zpMediaMessage->audioFileName, "%s", rhs);
			}
			else if (strcmp (lhs, "sync") == 0)
			{
				zpMediaMessage->sync = atoi (rhs);
			}
			else if (strcmp (lhs, "recordTime") == 0)
			{
				zpMediaMessage->recordTime = atoi (rhs);
			}
			else if (strcmp (lhs, "audioCompression") == 0)
			{
				zpMediaMessage->audioCompression = atoi (rhs);

			}
			else if (strcmp (lhs, "audioOverwrite") == 0)
			{
				zpMediaMessage->audioOverwrite = atoi (rhs);
			}
			else if (strcmp (lhs, "lead_silence") == 0)
			{
				zpMediaMessage->lead_silence = atoi (rhs);
			}
			else if (strcmp (lhs, "trail_silence") == 0)
			{
				zpMediaMessage->trail_silence = atoi (rhs);
			}
			else if (strcmp (lhs, "beep") == 0)
			{
				zpMediaMessage->beep = atoi (rhs);
			}
			else if (strcmp (lhs, "terminateChar") == 0)
			{
				if (rhs[0] == '!')
				{
					zpMediaMessage->terminateChar = 32;
				}
				else
				{
					zpMediaMessage->terminateChar = rhs[0];
				}
			}
			else if (strcmp (lhs, "beepFile") == 0)
			{
				sprintf (zpMediaMessage->beepFile, "%s", rhs);
			}
		}

		memset (line, 0, 256);
	}

	fclose (fp);

	unlink (yStrTempFileName);

	return (0);

}								/*END: int readRecordMediaFile */

int
readPlayMediaFile (struct Msg_PlayMedia *zpMediaMessage,
				   struct MsgToDM *zpMsgToDM, int zCall)
{
	static char     mod[] = { "readPlayMediaFile" };
	char            yStrTempFileName[128];
	char            line[256];
	FILE           *fp;

	char            lhs[256];
	char            rhs[256];

	memset (zpMediaMessage, 0, sizeof (struct Msg_PlayMedia));
	sprintf (yStrTempFileName, "%s", zpMsgToDM->data);

	if ((fp = fopen (yStrTempFileName, "r")) == NULL)
	{
		// MR 4613
		if ( ( ! canContinue (mod, zCall, __LINE__) ) ||
		     ( gCall[zCall].callState == CALL_STATE_CALL_CLOSED  ) ||
		     ( zpMsgToDM->opcode == DMOP_DISCONNECT ) ||
		     ( zpMsgToDM->opcode == DMOP_EXITTELECOM ) )
		{
			return(-1);
		}
// #if 0		// MR 4613
//DDN 04302016: App will log the error message
//djb 12-13-2018: It will log this message if it's a real error.

//		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR, ERR,
		// MR-5004
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR, INFO,
				   "Unable to open/play media file (%s); "
				   "Cannot obtain media information. [%d, %s] [op=%d, cs=%d]",
				   yStrTempFileName, errno, strerror (errno), zpMsgToDM->opcode, gCall[zCall].callState);
//#endif

		return (-1);
	}

	zpMediaMessage->opcode = DMOP_PLAYMEDIA;	//35

	while (fgets (line, sizeof (line) - 1, fp))
	{
		lhs[0] = '\0';
		rhs[0] = '\0';

		/*  Strip \n from the line if it exists */

		if (line[(int) strlen (line) - 1] == '\n')
		{
			line[(int) strlen (line) - 1] = '\0';
		}

		trim_whitespace (line);

		sscanf (line, "%[^=]=%s", lhs, rhs);
		if ((lhs != NULL) && (rhs != NULL) && (lhs[0] != 0) && (rhs[0] != 0))
		{
			if (strcmp (lhs, "party") == 0)
			{
				zpMediaMessage->party = atoi (rhs);
			}
			else if (strcmp (lhs, "interruptOption") == 0)
			{
				zpMediaMessage->interruptOption = atoi (rhs);
			}
			else if (strcmp (lhs, "audioFileName") == 0)
			{
				sprintf (zpMediaMessage->audioFileName, "%s", rhs);
			}
			else if (strcmp (lhs, "sync") == 0)
			{
				zpMediaMessage->sync = atoi (rhs);
			}
			else if (strcmp (lhs, "audioinformat") == 0)
			{
				zpMediaMessage->audioinformat = atoi (rhs);
			}
			else if (strcmp (lhs, "audiooutformat") == 0)
			{
				zpMediaMessage->audiooutformat = atoi (rhs);
			}
			else if (strcmp (lhs, "audioLooping") == 0)
			{
				zpMediaMessage->audioLooping = atoi (rhs);
			}
			else if (strcmp (lhs, "addOnCurrentPlay") == 0)
			{
				zpMediaMessage->addOnCurrentPlay = atoi (rhs);
				if (zpMediaMessage->addOnCurrentPlay == 0)
					zpMediaMessage->addOnCurrentPlay = NO;
			}
		}
	}

	fclose (fp);

	zpMediaMessage->appPassword = zpMsgToDM->appPassword;
	zpMediaMessage->appCallNum = zpMsgToDM->appCallNum;
	zpMediaMessage->appRef = zpMsgToDM->appRef;
	zpMediaMessage->appPid = zpMsgToDM->appPid;
	memset (zpMediaMessage->key, 0, sizeof (zpMediaMessage->key));

	unlink (yStrTempFileName);

	return (0);

}								/*END: int readPlayMediaFile */

#ifdef SR_MRCP

int             addToMrcpRtpData (int zCall, char *zData, int zDataSize,
								  int zDataType);
int             sendMsgToSRClient (int zLine, int zCall, struct MsgToDM *zpMsgToDM);
int             sendMsgToTTSClient (int zCall,
									ARC_TTS_REQUEST_SINGLE_DM * zpMsgToTTS);
int             openSRClientRequestFifo (int zCall);
static int      openTTSClientRequestFifo (int zCall, int *zFd);

char            gSilencePacketBuf_4k[4097];
char            gMrcpConfigFile[256];
char            gMrcpTtsServer[256];

extern int      SR_CleanUp (int zCall);
extern int      saveSpeechData (int zCall, char *zData, int zDataSize,
								int zDataType);
extern int      DM_SRExit (int zCall);
extern int      DM_SRGetParameter (int zCall, char *zParameterName,
								   char *zParameterValue);
extern int      DM_SRGetResult (int zCall, int zAlternativeNumber,
								int zFutureUse, char *zDelimiter,
								char *zResult, int *zOverallConfidence);
extern int      DM_SRGetXMLResult (int zCall, char *zXMLResultFile);
extern int      DM_SRInit (int zCall, const char *zFutureUse,
						   char *zAppName, int *zVendorCode);
extern int      DM_SRLoadGrammar (int zCall, int zGrammarType, char *zGrammar,
								  char *zParameters, int *zVendorCode);
extern int      DM_SRRecognize (int zCall, int zParty, int zBargeIn,
								int zTouchToneInterrupt,
								int zBeep, int zLeadSilence,
								int zTrailSilence, int zTotalTime,
								int *zNumberOfAlternatives,
								char *zResourceName, int zFutureUse,
								int *zVendorCode);
extern int      DM_SRReleaseResource (int zCall, int *zVendorCode);
extern int      DM_SRReserveResource (int zCall, int *zVendorCode);
extern int      DM_SRSetParameter (int zCall, char *zParameterName,
								   char *zParameterValue);
extern int      DM_SRTerminate ();
extern int      DM_SRUnloadGrammars (int zPort, int *zVendorCode);
extern int      DM_SRAddWords (int callNum, char *resourceName,
							   char *categoryName, struct SR_WORDS *wordList,
							   int numWords, int *vendorErrorCode);
extern int      DM_SRDeleteWords (int callNum, char *resourceName,
								  char *categoryName,
								  struct SR_WORDS *wordList, int numWords,
								  int *vendorErrorCode);
extern int      DM_SRGetPhoneticSpelling (int callNum, char *resourceName,
										  char *audioPath,
										  char *phoneticSpelling,
										  int *vendorCode);
extern int      DM_SRLearnWord (int callNum, char *resourceName,
								char *categoryName, struct SR_WORDS *word,
								int addTrans, int *vendorErrorCode);

///This function is not used in Aumtech's SIPIVR application.
int
DM_SRLogEvent (int callNum, char *srLogEvent, int *vendorCode)
{
	return (0);
}

///This function is not used in Aumtech's SIPIVR application.
int
DM_SRStart ()
{
	return (0);
}
#endif

int
addLoopFile (char *mod, SpeakList * zpSpeakList)
{
	SpeakList      *lpSpeakList = zpSpeakList;
	SpeakList      *prev = zpSpeakList;
	SpeakList      *currentSpeakList = zpSpeakList;
	int             zCall = lpSpeakList->msgPlayMedia.appCallNum;

	pthread_mutex_lock (&gCall[zCall].gMutexSpeak);

	while (lpSpeakList != NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Inside addLoopFile, lpSpeakList != NULL.");
		prev = lpSpeakList;
		lpSpeakList = lpSpeakList->nextp;
		if (lpSpeakList != NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "lpSpeakList->msgPlayMedia.audioFileName.",
					   lpSpeakList->msgPlayMedia.audioFileName);
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Inside addLoopFile, lpSpeakList == NULL");
			break;
		}
		if (lpSpeakList != NULL &&
			(lpSpeakList->msgSpeak.synchronous == PLAY_QUEUE_SYNC ||
			 lpSpeakList->msgSpeak.synchronous == PLAY_QUEUE_ASYNC))
		{
	SpeakList      *tempSpeak =
		(SpeakList *) arc_malloc (zCall, mod, __LINE__, sizeof (SpeakList));
			memcpy (tempSpeak, currentSpeakList, sizeof (SpeakList));
			prev->nextp = tempSpeak;
			tempSpeak->nextp = lpSpeakList;
			pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
			return 0;
		}
	}

	lpSpeakList = prev;

	if (lpSpeakList == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR, ERR,
				   "Failed to allocate memory for speakfile list. [%d, %s]",
				   errno, strerror (errno));

		pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
		return (-1);
	}
	SpeakList      *tempSpeak =
		(SpeakList *) arc_malloc (zCall, mod, __LINE__, sizeof (SpeakList));
	memcpy (tempSpeak, currentSpeakList, sizeof (SpeakList));
	tempSpeak->nextp = NULL;
	lpSpeakList->nextp = tempSpeak;
	pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);

	return 0;

}								/*END: int addLoopFile */

int
getValue (char *zStrInput,
		  char *zStrOutput,
		  char *zStrKey, char zChStartDelim, char zChStopDelim)
{
	int             yIntLength = 0;
	int             yIntIndex = 0;
	char           *yStrTempStart;
	char           *yStrTempStop;
	char            yNext;
	char            yPrev;

	/*
	 * Initialize vars
	 */
	zStrOutput[0] = 0;

	if (!zStrInput || !(*zStrInput))
	{
		return (-1);
	}

	if (!zStrKey || !(*zStrKey))
	{

		return (-1);
	}

	yIntLength = strlen (zStrInput);

	for (yIntIndex = 0; yIntIndex < yIntLength; yIntIndex++)
	{
		if (yIntIndex == 0)
			yPrev = zStrInput[yIntIndex];
		else
			yPrev = zStrInput[yIntIndex - 1];

		if (yIntIndex == yIntLength - 1)
			yNext = zStrInput[yIntIndex];
		else
			yNext = zStrInput[yIntIndex + 1];

		if ((yIntIndex == 0 ||
			 yPrev == ' ' ||
			 yPrev == '\t' ||
			 yPrev == '\n') &&
			strstr (zStrInput + yIntIndex, zStrKey) == zStrInput + yIntIndex)
		{
			yStrTempStart = strchr (zStrInput + yIntIndex, zChStartDelim) + 1;

			yStrTempStop = strchr (zStrInput + yIntIndex, zChStopDelim);

			if (!yStrTempStop ||
				!yStrTempStart || yStrTempStop < yStrTempStart)
			{
				zStrOutput[0] = 0;
				return (0);
			}

			if (strchr (zStrInput + yIntIndex, '\n') &&
				strchr (zStrInput + yIntIndex, '\n') < yStrTempStop)
			{
				zStrOutput[0] = 0;
				return (0);
			}

			strncpy (zStrOutput, yStrTempStart, yStrTempStop - yStrTempStart);

			zStrOutput[yStrTempStop - yStrTempStart] = 0;

			return (0);
		}

	}

	return (0);

}								/* END: getValue() */

/*END: Streaming functions */

#ifdef SR_MRCP

///This function is used to open the Speech Recognition Request Fifo.
/**
In order for us to send speech recognition requests to the speech server,
Media Manager needs to inform the Speech Rec (SR) client.  The way that
Media Manager talks to this client is by writing to a FIFO which is opened
by this function. 
*/
int
openSRClientRequestFifo (int zCall)
{
	static char     mod[] = { "openSRClientRequestFifo" };
	int             yRc;
	int             yFd = -1;
	char            yStrFifoName[128];
	char            yFifoDir[128];
	char            tmpMsg1[256];

	__DDNDEBUG__ (DEBUG_MODULE_FILE, "", "Value of srClientFifoFd",
				  gCall[zCall].srClientFifoFd);

	memset ((char *) yFifoDir, '\0', sizeof (yFifoDir));
	if ((yRc = UTL_GetArcFifoDir (MRCP_FIFO_INDEX, yFifoDir,
								  sizeof (yFifoDir), tmpMsg1,
								  sizeof (tmpMsg1))) != 0)
	{
		sprintf (yFifoDir, "%s", "/tmp");
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Defaulting fifo directory to %s.  %s",
				   yFifoDir, tmpMsg1);
	}
	// sprintf (yStrFifoName, "%s/%s", yFifoDir, MRCP_TO_SR_CLIENT2);
	sprintf (yStrFifoName, "%s/%s.%d", yFifoDir, MRCP_TO_SR_CLIENT2,
				 gCall[zCall].attachedSRClient);

	//BT-189 DDN 11202021
	if (gCall[zCall].srClientFifoFd > 0)
	{
		if(strcmp(yStrFifoName, gCall[zCall].srClientFifoName) != 0)
		{
			//It is possible that the SR resource was released and reserved again
			//Close The previouse one
			arc_close (zCall, mod, __LINE__, &gCall[zCall].srClientFifoFd);
			sprintf(gCall[zCall].srClientFifoName, "%s", yStrFifoName);
		}
		else
		{
			return (0);
		}
	}
	//END: BT-189

	if ((yFd =
		 arc_open (zCall, mod, __LINE__, yStrFifoName, O_RDWR,
				   ARC_TYPE_FIFO)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to open request fifo (%s). [%d, %s]",
				   yStrFifoName, errno, strerror (errno));

		return (-1);
	}

	gCall[zCall].srClientFifoFd = yFd;

	__DDNDEBUG__ (DEBUG_MODULE_FILE, "", yStrFifoName, 0);

	return (0);

}	/*END: int openSRClientRequestFifo */

///This function is used to send Speech Recognition Requests to the Speech Rec Client.
/**
The function openSRClientRequestFifo() opens a FIFO so it is possible to
communicate with the Speech, but to actually write the message to the
client we use this function.  The message is a pointer to a MsgToDM struct.
*/
int
sendMsgToSRClient (int zLine, int zCall, struct MsgToDM *zpMsgToDM)
{
	static char     mod[] = { "sendMsgToSRClient" };
	int             yRc;

	openSRClientRequestFifo (zCall);

	__DDNDEBUG__ (DEBUG_MODULE_SR, "", "Sending message to SR client", 0);

	yRc =
		write (gCall[zCall].srClientFifoFd, zpMsgToDM,
			   sizeof (struct MsgToDM));
	if (yRc < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "From [%d]: failed to write a message to SR client. [%d, %s]", zLine, 
					errno, strerror (errno));

		return (-1);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"From [%d]: Wrote op:%d to mrcpClient2.", zLine, zpMsgToDM->opcode);

	return (0);

}	/*END: int sendMsgToSRClient */

int
sendMsgToTTSClient (int zCall, ARC_TTS_REQUEST_SINGLE_DM * zpMsgToTTS)
{
	static char     mod[] = { "sendMsgToTTSClient" };
	int             yRc;

//  static int  ttsFd = -1;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "ttsFd = %d", gCall[zCall].ttsFd);

	if (gCall[zCall].ttsFd == -1)
	{
		if ((yRc =
			 openTTSClientRequestFifo (zCall, &gCall[zCall].ttsFd)) == -1)
		{
			return (-1);		// message logged in routine
		}
	}

	yRc =
		write (gCall[zCall].ttsFd, zpMsgToTTS,
			   sizeof (ARC_TTS_REQUEST_SINGLE_DM));
	if (yRc < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to write a message to tts client. fd=%d; rc=%d. "
				   "[%d, %s].", gCall[zCall].ttsFd, yRc, errno,
				   strerror (errno));

		return (-1);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Sent %d bytes to ttsClient. fd=%d "
			   "msg={op:%d,c#:%s,pid:%s,ref:%d,sync=%d,%s}",
			   yRc, gCall[zCall].ttsFd,
			   zpMsgToTTS->speakMsgToDM.opcode,
			   zpMsgToTTS->port,
			   zpMsgToTTS->pid,
			   zpMsgToTTS->speakMsgToDM.appRef,
			   zpMsgToTTS->async, zpMsgToTTS->string);

	return (0);

}								/*END: int sendMsgToTTSClient */

static int
openTTSClientRequestFifo (int zCall, int *zFd)
{
	static char     mod[] = { "openTTSClientRequestFifo" };
	int             yRc;
	int             yFd = -1;
	char            yStrFifoName[128];
	char            yFifoDir[128];
	char            tmpMsg1[256];

	memset ((char *) yFifoDir, '\0', sizeof (yFifoDir));
	if ((yRc = UTL_GetArcFifoDir (MRCP_TTS_FIFO_INDEX, yFifoDir,
								  sizeof (yFifoDir), tmpMsg1,
								  sizeof (tmpMsg1))) != 0)
	{
		sprintf (yFifoDir, "%s", "/tmp");
		dynVarLog (__LINE__, 0, mod, REPORT_DETAIL,
				   TEL_CONFIG_VALUE_NOT_FOUND, WARN,
				   "Defaulting fifo directory to %s.  %s", yFifoDir, tmpMsg1);
	}

	int             ttsClientId = zCall / 12;

	sprintf (yStrFifoName, "%s/%s.%d", yFifoDir, TTS_FIFO, ttsClientId);

	if ((yFd = arc_open (zCall, mod, __LINE__, yStrFifoName,
						 O_RDWR, ARC_TYPE_FIFO)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to open ttsClient request fifo (%s). [%d, %s]",
				   yStrFifoName, errno, strerror (errno));
		return (-1);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Successfully opened ttsClient fifo (%s).  fd=%d",
			   yStrFifoName, yFd);

	//*zFd = yFd;

	gCall[zCall].ttsFd = yFd;

	return (0);

}								/*END: int openTTSClientRequestFifo */

///This function adds to MRCP data.
int
addToMrcpRtpData (int zCall, char *zData, int zDataSize, int zDataType)
{
	static char     mod[] = { "addToMrcpRtpData" };
	RtpMrcpData    *pRtpMrcpData;

	if (gCall[zCall].pFirstRtpMrcpData == NULL)
	{
		gCall[zCall].pFirstRtpMrcpData =
			(RtpMrcpData *) arc_malloc (zCall, mod, __LINE__,
										sizeof (RtpMrcpData));

		if (gCall[zCall].pFirstRtpMrcpData == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR, "Failed to allocate memory. [%d, %s]", errno,
					   strerror (errno));
			return (-1);
		}
		memcpy (gCall[zCall].pFirstRtpMrcpData->data, zData, zDataSize);
		gCall[zCall].pFirstRtpMrcpData->dataSize = zDataSize;
		gCall[zCall].pFirstRtpMrcpData->dataType = zDataType;
		gCall[zCall].pFirstRtpMrcpData->nextp = NULL;
		gCall[zCall].pLastRtpMrcpData = gCall[zCall].pFirstRtpMrcpData;
	}
	else
	{
		pRtpMrcpData =
			(RtpMrcpData *) arc_malloc (zCall, mod, __LINE__,
										sizeof (RtpMrcpData));
		if (pRtpMrcpData == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR, "Failed to allocate memory. [%d, %s]", errno,
					   strerror (errno));
			return (-1);
		}

		memcpy (pRtpMrcpData->data, zData, zDataSize);
		pRtpMrcpData->dataSize = zDataSize;
		pRtpMrcpData->dataType = zDataType;
		pRtpMrcpData->nextp = NULL;
		gCall[zCall].pLastRtpMrcpData->nextp = pRtpMrcpData;
		gCall[zCall].pLastRtpMrcpData = pRtpMrcpData;
	}

	return (0);

}								/*END: int addToMrcpRtpData */

#endif

///This function is used to remove Media Manager's shared memory.
/**
In order for Media Manager to communicate with Call Manager we use shared
memory.  This function removes the shared memory between the two Managers.
We call it before we create and attach shared memory.
*/

// renaming this if this is the same as the one I created 
// in 

int
removeSharedMem_mm (int zCall)
{
	static char     mod[] = "removeSharedMem";
	int             yRc = -1;

	/* set shared memory key and queue */
	//gShmKey = SHMKEY_SIP;
	gShmKey = SHMKEY_SIP + ((key_t) gDynMgrId);

	/* remove shared memory  -  get shared memory segments id */
	gShmId = shmget (gShmKey, SHMSIZE_SIP, PERMS);
	yRc = shmctl (gShmId, IPC_RMID, 0);

	__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "", "", 0);

	if ((yRc < 0) && (errno != EINVAL))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR, ERR,
				   "Failed to remove shared memory. [%d, %s]", errno,
				   strerror (errno));
		return (ISP_FAIL);
	}

	return (ISP_SUCCESS);

}								/*removeSharedMem */

///This function is used to create Media Manager's shared memory.
/**
In order for Media Manager to communicate with Call Manager we use shared
memory.  This function creates the shared memory used to communicate between
Call Manager and Media Manager.  We remove the memory before creating it.
*/
int
createSharedMem (int zCall)
{
	static char     mod[] = "createSharedMem";
	int             yrc = -1;

	//gShmKey = SHMKEY_SIP;
	gShmKey = SHMKEY_SIP + ((key_t) gDynMgrId);

	/* create shared memory segment */
	if ((gShmId = shmget (gShmKey, SHMSIZE_SIP, PERMS)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR, ERR,
				   "Failed to get shared memory segment for key %ld. [%d, %s]",
				   gShmKey, errno, strerror (errno));

		return (-1);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "Shared memory key:id for SIP is [%ld:%d].", gShmKey, gShmId);

	return (ISP_SUCCESS);

}								/*END: int createSharedMem */

///This function is used to attach Media Manager's shared memory.
/**
In order for Media Manager to communicate with Call Manager we use shared
memory.  This function attaches the shared memory used to communicate between
Call Manager and Media Manager.  We attach the shared memory only after
removing it and creating it.
*/
int
attachSharedMem (int zCall)
{
	static char     mod[] = "attachSharedMem";
	int             yRc = -1;
	int             i = 0;

	if ((gShmBase = shmat (gShmId, 0, SHM_RDONLY)) == (char *) -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to attach shared memory segment, [%d, %s]", errno,
				   strerror (errno));

		return (-1);
	}

	gEncodedShmBase = (struct EncodedShmBase *) gShmBase;

	return (ISP_SUCCESS);

}								/*END: int attachSharedMem */

#ifdef ACU_LINUX
int
clearConfData (int zCall)
{
	char            mod[] = "clearConfData";

	for (int i = 0; i < MAX_CONF_PORTS; i++)
	{
		gCall[zCall].conf_ports[i] = -1;
	}
	gCall[zCall].confUserCount = 0;

	//memset(gCall[zCall].confID, 0, sizeof(gCall[zCall].confID));

	return 0;
}
#endif

int
setCallSubState (int zCall, int zState)
{
	static char     mod[] = "setCallSubState";

	gCall[zCall].callSubState = zState;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Setting callSubState to %d.", zState);
	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "Call SubState changed to", zState);

}								/*END: int setCallSubState */

///This function sets the call stat for port zCall to zState and 
///prints a debug message for this change in nohup.out.
int
setCallState (int zCall, int zState, int zLine)
{
	static char     mod[] = "changeCallState";

	gCall[zCall].callState = zState;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] Setting callState to %d.", zLine, zState);

#ifdef ACU_LINUX
	if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
		|| gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
		|| gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
		|| gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
	{
		clearConfData (zCall);
	}
#endif

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "Call State changed to", zState);
    if ( gCall[zCall].callState == CALL_STATE_IDLE )
    {
        gCall[zCall].issuedBlindTransfer = 0;
    }
}								/*END: int changeCallState */

///This function is no longer used
int
getPassword (int zCall)
{
	return zCall;

}								/*END: int getPassword */

#define isCallActive canContinue

///This function checks whether port zCall is still active (i.e. the caller is still there).
/**
In Media Manager there are while loops which need to know when the call is
over.  Whether it is because the caller hung up or Media Manager hung up.
The way we do this is by including another condition in our while loops:
canContinue().  The function will return true if and only if the call is
still going on.  A call is defined as UP if the call is not in any of the
following states:
CALL_STATE_CALL_CLOSED,
CALL_STATE_CALL_CANCELLED,
CALL_STATE_CALL_TERMINATE_CALLED,
or CALL_STATE_CALL_RELEASED.
*/
int
canContinue (char *mod, int zCall, int line)
{
	char            ystrMsg[256] = "";

	if (!gCanReadShmData)
	{
		dynVarLog (line, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
				   "Switch gCanReadShmData set to off.  Returning can't continue.");
		return (0);
	}

	if (gCanExit)
	{
		dynVarLog (line, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
				   "Switch gCanExit is set. Returning can't continue.");
		return (0);
	}

	if (zCall < gStartPort || zCall > gEndPort)
	{
		dynVarLog (line, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Invalid current port zCall=%d; out of range. "
				   "Start port=%d, end port=%d.", zCall, gStartPort,
				   gEndPort);
		return (0);
	}

	if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED ||
		gCall[zCall].callState == CALL_STATE_CALL_CANCELLED ||
		gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED ||
		gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
	{
		return (0);
	}

	if (gCall[zCall].yInitTime < gCall[zCall].yDropTime)		// yDropTime would be for previous call
	{
//      dynVarLog(line, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//          "DJB: gCall[%d].yInitTime(%d) < gCall[%d].yDropTime(%d).",
//          zCall, gCall[zCall].yInitTime,
//          zCall, gCall[zCall].yDropTime);

		return (0);
	}

#if 0
	if (gMaxCallDuration > 0)
	{
		/*  
		 *  Note: To ensure better CPU performance, yTmpApproxTime is used,
		 *  which is set from readAndProcessShmData */

		if (yTmpApproxTime > gCall[zCall].yInitTime + gMaxCallDuration)
		{
			time (&gCall[zCall].yDropTime);
			return (0);
		}
	}
#endif

	return (1);

}								/*END: int canContinue */

///This function is used to find out what situation this port is in.
int
getFunctionalStatus (int zCall)
{

	if (gCall[zCall].speechRec == 1)
	{
		return 4;				/*SPEECH REC */
	}

	if (gCall[zCall].receivingSilencePackets &&
		!gCall[zCall].sendingSilencePackets)
	{
		return 2;
	/*SPEAK*/}
	else if (!gCall[zCall].receivingSilencePackets &&
			 gCall[zCall].sendingSilencePackets)
	{
		return 3;
	/*RECORD*/}
	else if (gCall[zCall].receivingSilencePackets &&
			 gCall[zCall].sendingSilencePackets)
	{
		return 5;				/*Neither record nor speak is going on but call is active */
	}
	else
	{
		return (1);				/*Channel is idle */
	}

}								/*int getFunctionalStatus */

int
writeMRCPResultToApp (int zCall,
					  struct MsgToApp *zpResponse, char *mod, int zLine)
{
	char            resultFifo[128] = "";
	int             fd = -1;
	int             rc = 0;
	struct MsgToApp response = *zpResponse;

	sprintf (resultFifo, "/tmp/ttsResult.%d.fifo", zCall);
	if (mknod (resultFifo, S_IFIFO | PERMS, 0) < 0 && errno != EEXIST)
	{
		return (-1);
	}

	if ((fd = open (resultFifo, O_CREAT | O_RDWR)) < 0)
	{
		return (-1);
	}
	rc = write (fd, &response, sizeof (struct MsgToApp));
	if (rc == -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to write mrcp result [op=%d, rtc=%d, cs=%d] to application fifo (%d:%s). [%d, %s]",
				   response.opcode, response.returnCode, gCall[zCall].callState,
					fd, resultFifo, errno, strerror (errno));
		return (-1);
	}

	close (fd);

	if(gCall[zCall].GV_HideDTMF)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling line(%d) fd(%d / %d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,rtc=%d,data:%s [cs=%d]",
			   zLine, fd, fd,
			   rc, resultFifo,
			   response.opcode,
			   response.appCallNum,
			   response.appRef, response.appPid, response.returnCode, "X", gCall[zCall].callState);
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling line(%d) fd(%d / %d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,rtc=%d,data:%s [cs=%d]",
			   zLine, fd, fd,
			   rc, resultFifo,
			   response.opcode,
			   response.appCallNum,
			   response.appRef, response.appPid, response.returnCode, response.message, gCall[zCall].callState);
	}

	return (0);

}

///This function writes the response to the application.
/**
After Call Manager and Media Manager are done with the DMOP operation sent to
them by an application they write a response to the appliation on the Response
fifo.  This function is what writes that response.  The function must be called
with a struct MsgToApp that is already populated in the function calling
writeGenericResponseToApp().
*/
int
writeGenericResponseToApp (int zCall,
						   struct MsgToApp *zpResponse, char *mod, int zLine)
{
	struct MsgToApp response = *zpResponse;
	int             rc;
	int             shouldCloseFd = 0;

	int             yIntALeg = -1;

	static char            fifoName[256];

	if (gCall[zCall].leg == LEG_B)
	{
		yIntALeg = gCall[zCall].crossPort;
		if (yIntALeg < 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT,
					   INFO,
					   "Failed to get A Leg for LEG_B port (%d).  Unable to write response [op=%d, rtc=%d, cs=%d] to app.",
					   zCall,
						zpResponse->opcode, zpResponse->returnCode, gCall[zCall].callState);
			return (-1);
		}
	}
	else
	{
		yIntALeg = zCall;
	}

	if (arc_kill (gCall[yIntALeg].appPid, 0) == -1)
	{
		if (errno == ESRCH)
		{

			__DDNDEBUG__ (DEBUG_MODULE_FILE, "App doesn't exist for FIFO ",
						  gCall[yIntALeg].responseFifo, 0);

			arc_close (yIntALeg, mod, __LINE__,
					   &(gCall[yIntALeg].responseFifoFd));
			gCall[yIntALeg].responseFifoFd = -1;
			arc_unlink (zCall, mod, gCall[yIntALeg].responseFifo);
//			dynVarLog (__LINE__, yIntALeg, mod, REPORT_DETAIL, DYN_BASE, ERR,
//							"DJB: unlink (%s)", gCall[yIntALeg].responseFifo);
			gCall[yIntALeg].responseFifo[0] = 0;
			return (-1);
		}
	}

	__DDNDEBUG__ (DEBUG_MODULE_FILE, "APP_FIFO", gCall[yIntALeg].responseFifo,
				  gCall[yIntALeg].responseFifoFd);

	if (zpResponse->opcode == DMOP_SPEAKMRCPTTS)
	{
		return writeMRCPResultToApp (zCall, zpResponse, mod, zLine);
	}

	sprintf (fifoName, "%s", gCall[yIntALeg].responseFifo);

	if (!fifoName[0] && gCall[yIntALeg].lastResponseFifo[0])
	{
		shouldCloseFd = 1;
		sprintf (fifoName, "%s", gCall[yIntALeg].lastResponseFifo);
	}

	if (!fifoName[0])
	{
        if ( ( ! canContinue (mod, zCall, __LINE__) ) ||        // MR 4836
             ( zpResponse->opcode == DMOP_DISCONNECT ) ||
             ( zpResponse->opcode == DMOP_EXITTELECOM ) )
        {
            dynVarLog (zLine, yIntALeg, mod, REPORT_NORMAL, DYN_BASE, WARN,
                       "Unable to respond back to app with [op=%d, rtc=%d, cs=%d]. "
						"Application is disconnected and the fifoname is empty.",
                        zpResponse->opcode, zpResponse->returnCode, gCall[yIntALeg].callState);
        }
        else
        {
            dynVarLog (zLine, yIntALeg, mod, REPORT_NORMAL, DYN_BASE, ERR,
                       "Failed to create fifo, empty fifo name.  Unable to respond back to app with [op=%d, rtc=%d, cs=%d]",
						zpResponse->opcode, zpResponse->returnCode, gCall[yIntALeg].callState);
        }
		return (-1);
	}

	if (gCall[yIntALeg].responseFifoFd < 0)
	{
		gCall[yIntALeg].responseFifoFd =
			arc_open (yIntALeg, mod, __LINE__, fifoName, O_RDWR | O_NONBLOCK,
					  ARC_TYPE_FIFO);

		gCall[yIntALeg].responseFifoFdBkp = gCall[yIntALeg].responseFifoFd;

		if (gCall[yIntALeg].responseFifoFd < 0)
		{
			// MR 4989
            // No need to write message for B Leg or for already disconnected calls
            if ( gCall[zCall].leg == LEG_A )
            {
                if ( ( ! canContinue (mod, zCall, __LINE__) ) ||
                     ( gCall[zCall].callState == CALL_STATE_CALL_CLOSED  ) ||
                     ( zpResponse->opcode == DMOP_DISCONNECT ) ||
                     ( zpResponse->opcode == DMOP_EXITTELECOM ) )
                {      
                    dynVarLog (zLine, yIntALeg, mod, REPORT_NORMAL,
                               TEL_FILE_IO_ERROR, WARN,
                               "Call is disconnected. Failed to open fifo (%s). [%d, %s]  "
                               "Unable to write response [op=%d, rtc=%d, cs=%d] to app.", fifoName,
                               errno, strerror (errno),
								zpResponse->opcode, zpResponse->returnCode, gCall[zCall].callState);
                }
				else
                {
                    dynVarLog (zLine, yIntALeg, mod, REPORT_NORMAL,
                               TEL_FILE_IO_ERROR, ERR,
                               "Failed to open fifo (%s). [%d, %s]  "
                               "Unable to write response [op=%d, rtc=%d, cs=%d] to app.", fifoName,
                               errno, strerror (errno),
								zpResponse->opcode, zpResponse->returnCode, gCall[zCall].callState);
                }
        
                return (-1);
            }         
		}

//		if ((stat(fifoName, &(gCall[zCall].DJBStat))) == -1)
//		{
//			dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//						"stat(%s) failed. [%d, %s]\n", buf, errno, strerror(errno));
//		}
//		dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//				"DJB: fifo (%s - inode=%ld).", fifoName, gCall[zCall].DJBStat.st_ino);
	}

	if (gCall[zCall].leg == LEG_A)
	{
		switch (response.opcode)
		{
		case DMOP_SPEAK:
		case DMOP_RECORD:
		case DMOP_PLAYMEDIA:
		case DMOP_RECORDMEDIA:
		case DMOP_OUTPUTDTMF:
		case DMOP_DISCONNECT:
		case DMOP_SETGLOBAL:
		case DMOP_SETGLOBALSTRING:
		case DMOP_GETGLOBAL:
		case DMOP_GETGLOBALSTRING:
		case DMOP_ANSWERCALL:
		case DMOP_DROPCALL:
		case DMOP_GETDTMF:

#ifdef VOICE_BIOMETRICS
        case DMOP_VERIFY_CONTINUOUS_SETUP:
        case DMOP_VERIFY_CONTINUOUS_GET_RESULTS:
#endif // END: VOICE_BIOMETRICS


			if (gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
			{
				response.returnCode = -3;
			}
			break;

		default:
			break;
		}
	}

	rc = write (gCall[yIntALeg].responseFifoFd, &response,
				sizeof (struct MsgToApp));
	if (rc == -1)
	{
		dynVarLog (zLine, yIntALeg, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Failed to write to response [op=%d, rtc=%d, cs=%d] fifo (%d:%s). [%d, %s] bkpfd(%d)",
					zpResponse->opcode, zpResponse->returnCode, gCall[yIntALeg].callState,
				   gCall[yIntALeg].responseFifoFd, fifoName, errno,
				   strerror (errno), gCall[yIntALeg].responseFifoFdBkp);

		return (-1);
	}

#ifdef VOICE_BIOMETRICS
	if ( response.opcode == DMOP_VERIFY_CONTINUOUS_GET_RESULTS )
	{
		dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling line(%d) fd(%d / %d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,rtc:%d,data:<float> [cs=%d]",
			   zLine, gCall[yIntALeg].responseFifoFd,
			   gCall[yIntALeg].responseFifoFdBkp, rc, fifoName,
			   response.opcode, response.appCallNum, response.appRef,
			   response.appPid, response.returnCode, gCall[yIntALeg].callState);
	}
	else
	{
		if(gCall[zCall].GV_HideDTMF)
		{
			dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling line(%d) fd(%d / %d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,rtc:%d,data:%s [cs=%d]",
			   zLine, gCall[yIntALeg].responseFifoFd,
			   gCall[yIntALeg].responseFifoFdBkp, rc, fifoName,
			   response.opcode, response.appCallNum, response.appRef,
			   response.appPid, response.returnCode, "X", gCall[yIntALeg].callState);
		}
		else
		{
			dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling line(%d) fd(%d / %d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,rtc:%d,data:%s [cs=%d]",
			   zLine, gCall[yIntALeg].responseFifoFd,
			   gCall[yIntALeg].responseFifoFdBkp, rc, fifoName,
			   response.opcode, response.appCallNum, response.appRef,
			   response.appPid, response.returnCode, response.message, gCall[yIntALeg].callState);
		}
	}
#else
	dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling line(%d) fd(%d / %d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,rtc:%d,data:%s [cs=%d]",
			   zLine, gCall[yIntALeg].responseFifoFd,
			   gCall[yIntALeg].responseFifoFdBkp, rc, fifoName,
			   response.opcode, response.appCallNum, response.appRef,
			   response.appPid, response.returnCode, response.message, gCall[yIntALeg].callState);
#endif // END: VOICE_BIOMETRICS
// DJBDEBUG - this is for debugging disconnect issue
//	if ( response.opcode == DMOP_DISCONNECT )
//	{
//
//		sprintf(lPath, "/proc/%d/fd/%d", getpid(), gCall[yIntALeg].responseFifoFd);
//		memset((char *)buf, '\0', sizeof(buf));
//		memset((struct stat *)&(gCall[yIntALeg].DJBStat), '\0', sizeof(struct stat));
//
//	    rc = readlink((const char *)lPath, buf, sizeof(buf));
//		if ((stat(buf, &(gCall[yIntALeg].DJBStat))) == -1)
//		{
//			dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//						"stat(%s) failed. [%d, %s]\n", buf, errno, strerror(errno));
//		}
//		dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//				"[%s, %d] %d = readlink(%s, %s) [%d, %s]",
//				__FILE__, __LINE__, rc, lPath, buf, errno,  strerror (errno));
//
//		dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//				"[%s, %d] DJBStat.st_ino  = %ld", __FILE__, __LINE__, gCall[yIntALeg].DJBStat.st_ino);
//	}
// END DJBDEBUG - this is for debugging disconnect issue

	if (shouldCloseFd == 1)
	{
		arc_close (zCall, mod, __LINE__, &gCall[yIntALeg].responseFifoFd);
	}

	return (0);

}								/*END: int writeGenericResponseToApp */

#ifdef ACU_LINUX

int
sendRequestToSTonesFaxClientSpecialFifo (int zCall, char *mod,
										 struct MsgToDM *request)
{
	int             rc = -1;
	int             yTmpSpecialFifoFd = -1;

	if ((yTmpSpecialFifoFd =
		 arc_open (zCall, mod, __LINE__, gCall[zCall].faxServerSpecialFifo,
				   O_WRONLY | O_NONBLOCK, ARC_TYPE_FIFO)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Failed to open special tones/fax fifo (%s). [%d, %s]  "
				   "Unable to write request.",
				   gCall[zCall].faxServerSpecialFifo, errno,
				   strerror (errno));

		return (-1);
	}

	rc = write (yTmpSpecialFifoFd, (char *) request, sizeof (struct MsgToDM));
	if (rc < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Failed to write to special tones/fax fifo (%s). [%d, %s]",
				   gCall[zCall].faxServerSpecialFifo, errno,
				   strerror (errno));
		return -1;
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "fd(%d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,data:%s",
			   yTmpSpecialFifoFd,
			   rc, gCall[zCall].faxServerSpecialFifo,
			   request->opcode,
			   request->appCallNum,
			   request->appRef, request->appPid, request->data);

	close (yTmpSpecialFifoFd);

	return (0);

}								/*END: int sendRequestToSTonesFaxClientSpecialFifo */

int
sendRequestToSTonesClientSpecialFifo (int zCall, char *mod,
									  struct MsgToDM *request)
{
	int             rc = -1;
	int             yTmpSpecialFifoFd = -1;

	if ((yTmpSpecialFifoFd =
		 arc_open (zCall, mod, __LINE__, gCall[zCall].toneServerSpecialFifo,
				   O_WRONLY | O_NONBLOCK, ARC_TYPE_FIFO)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Failed to open special tones fifo (%s). [%d, %s]  "
				   "Unable to write request.",
				   gCall[zCall].faxServerSpecialFifo, errno,
				   strerror (errno));

		return (-1);
	}

	rc = write (yTmpSpecialFifoFd, (char *) request, sizeof (struct MsgToDM));
	if (rc < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Failed to write to special tones fifo (%s). [%d, %s]",
				   gCall[zCall].faxServerSpecialFifo, errno,
				   strerror (errno));
		return -1;
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "fd(%d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,data:%s",
			   yTmpSpecialFifoFd,
			   rc, gCall[zCall].toneServerSpecialFifo,
			   request->opcode,
			   request->appCallNum,
			   request->appRef, request->appPid, request->data);

	close (yTmpSpecialFifoFd);

	return (0);

}								/*END: int sendRequestToSTonesClientSpecialFifo */

int
sendRequestToSTonesFaxClient (char *mod, struct MsgToDM *request, int zCall)
{
	int             rc = -1;
	int             yIntErrno = 0;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "faxServerSpecialFifo=(%s)",
			   gCall[zCall].faxServerSpecialFifo);

	if (gCall[zCall].faxServerSpecialFifo[0] != '\0')
	{
		return sendRequestToSTonesFaxClientSpecialFifo (zCall, mod, request);
	}

	if (gSTonesFaxFifoFd < 0)
	{
		rc = openChannelToTonesFaxClient (zCall);

		if (rc < 0)
		{
			return (rc);
		}
	}

	rc = write (gSTonesFaxFifoFd, (char *) request, sizeof (struct MsgToDM));
	yIntErrno = errno;
	if (rc < 0)
	{
		if (yIntErrno == 32)	/*Try one more time */
		{
			rc = openChannelToTonesFaxClient (zCall);
			if (rc < 0)
			{
				return (rc);
			}

			rc = write (gSTonesFaxFifoFd, (char *) request,
						sizeof (struct MsgToDM));
			if (rc < 0)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_FILE_IO_ERROR, ERR,
						   "Failed to write to special fax/tones fifo (%s) in 2nd attempt. [%d, %s]",
						   gSTonesFaxFifo, errno, strerror (errno));
				return (-1);
			}
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR, "Failed to write to fifo (%s). [%d, %s]",
					   gSTonesFaxFifo, errno, strerror (errno));

			return (-1);
		}
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "fd(%d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,data:%s",
			   gSTonesFaxFifoFd,
			   rc, gSTonesFaxFifo,
			   request->opcode,
			   request->appCallNum,
			   request->appRef, request->appPid, request->data);

	return (0);

}

int
sendRequestToSTonesClient (int zCall, char *mod, struct MsgToDM *request)
{
	int             rc = -1;

//  int zCall = -1;
	int             yIntErrno = 0;

	if (gSTonesFifoFd < 0)
	{
		rc = openChannelToTonesClient ();

		if (rc < 0)
		{
			return (rc);
		}
	}

	rc = write (gSTonesFifoFd, (char *) request, sizeof (struct MsgToDM));
	yIntErrno = errno;
	if (rc < 0)
	{
		if (yIntErrno == 32)	/*Try one more time */
		{
			rc = openChannelToTonesClient ();
			if (rc < 0)
			{
				return (rc);
			}

			rc = write (gSTonesFifoFd, (char *) request,
						sizeof (struct MsgToDM));
			if (rc < 0)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_FILE_IO_ERROR, ERR,
						   "Failed to write to special fax/tones fifo (%s) in 2nd attempt. [%d, %s]",
						   gSTonesFifo, errno, strerror (errno));
				return (-1);
			}
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR, "Failed to write to fifo (%s). [%d, %s]",
					   gSTonesFifo, errno, strerror (errno));

			return (-1);
		}
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "fd(%d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,data:%s",
			   gSTonesFifoFd,
			   rc, gSTonesFifo,
			   request->opcode,
			   request->appCallNum,
			   request->appRef, request->appPid, request->data);

	return (0);

}

int
sendRequestToConfMgr (char *mod, struct MsgToDM *request)
{
	int             rc = -1;
	int             zCall = -1;
	int             yIntErrno = 0;

	if (gConfMgrFifoFd < 0)
	{
		rc = openChannelToConfMgr ();

		if (rc < 0)
		{
			return (rc);
		}
	}

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "Writing to fifo", gConfMgrFifoFd);
	rc = write (gConfMgrFifoFd, (char *) request, sizeof (struct MsgToDM));
	yIntErrno = errno;
	if (rc < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "Failed to write to fifo",
					  gConfMgrFifoFd);

		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to write to conference mgr fifo (fd=%d:%s). [%d, %s]",
				   gConfMgrFifoFd, gConfMgrFifo, errno, strerror (errno));
		if (yIntErrno == 32)	/*Try one more time */
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "Opening fifo",
						  gConfMgrFifoFd);
			rc = openChannelToConfMgr ();
			if (rc < 0)
			{
				__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "Failed to open fifo",
							  gConfMgrFifoFd);
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_FILE_IO_ERROR, ERR,
						   "Failed to open conference mgr fifo (%d:%s) in 2nd attempt. [%d, %s]",
						   gConfMgrFifoFd, gConfMgrFifo, errno,
						   strerror (errno));
				return (rc);
			}

			rc = write (gConfMgrFifoFd, (char *) request,
						sizeof (struct MsgToDM));
			if (rc < 0)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_FILE_IO_ERROR, ERR,
						   "Failed to write to conference mgr fifo (%d:%s) in 2nd attempt. [%d, %s]",
						   gConfMgrFifoFd, gConfMgrFifo, errno,
						   strerror (errno));

				return (-1);
			}
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR, "Failed to write to fifo (%d:%s). [%d, %s]",
					   gConfMgrFifoFd, gConfMgrFifo, errno, strerror (errno));
			return (-1);
		}
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "fd(%d), %d = write(%s) op:%d,call:%d,ref:%d,pid:%d,data:%s",
			   gConfMgrFifoFd,
			   rc, gConfMgrFifo,
			   request->opcode,
			   request->appCallNum,
			   request->appRef, request->appPid, request->data);

	return (0);

}

#endif
int
sendRequestToDynMgr (int zLine, char *mod, struct MsgToDM *request)
{
	int             rc = -1;
	int             zCall = -1;

	rc = write (gRequestFifoFd, (char *) request, sizeof (struct MsgToDM));
	if (rc < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to write to request fifo (%d:%s).  [%d, %s]",
				   gRequestFifoFd, gRequestFifo, errno, strerror (errno));
	}

	return 0;
}

///This function finds the IP address of zHostName and sets zOut to that address.
int
getIpAddress (char *zHostName, char *zOut, char *zErrMsg)
{
	struct hostent *myHost;
	struct in_addr *addPtr;
	char          **p;
	extern int      h_errno;

	int             err = 0;

	if (!zHostName || !*zHostName)
	{
		sprintf (zErrMsg, "%s",
				 "Empty hostname received.  Unable to get ip address.");

		return (-1);
	}

	myHost = gethostbyname (zHostName);

	if (myHost == NULL)
	{
		sprintf (zErrMsg,
				 "gethostbyname(%s) failed.  errno=%d.  Unable to get the address of (%s)",
				 zHostName, h_errno, zHostName);
		return (-1);
	}

	addPtr = (struct in_addr *) *myHost->h_addr_list;

	sprintf (zOut, "%s", (char *) inet_ntoa (*addPtr));

	return (0);

}								/*END: getIpAddress */

int
util_sleep (int Seconds, int Milliseconds)
{
	static char     mod[] = { "util_sleep" };
	struct timeval  SleepTime;

	int             actualSeconds;
	int             actualMilliseconds;
	int             secondsFromMilli;

	secondsFromMilli = Milliseconds / 1000;
	actualSeconds = Seconds + secondsFromMilli;
	actualMilliseconds = Milliseconds % 1000;

	SleepTime.tv_sec = actualSeconds;
	SleepTime.tv_usec = actualMilliseconds * 1000;

	select (0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &SleepTime);

	return (0);

}								/*END: int util_sleep */

///This function is used to add a DMOP command to the request list.
/**
When a DMOP command is received, if it is not synchronous it needs to be added
to a linked list so that Media Manager can return control back to the
application and play the file and its next available moment.  This function
is what adds that DMOP command to a linked list so that Media Manager can take
care of it later.
*/
int
addToAppRequestList (struct MsgToDM *zpMsgToDM)
{
	static char     mod[] = { "addToAppRequestList" };
	int             zCall = zpMsgToDM->appCallNum;
	RequestList    *pRequest;

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "opcode", zpMsgToDM->opcode);

	if (zCall < gStartPort || zCall > gEndPort)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT, ERR,
				   "Invalid port (%d) received; must be between %d and %d. Unable to add request to list.",
				   zCall, gStartPort, gEndPort);

		return (-1);
	}

	pthread_mutex_lock (&gCall[zCall].gMutexRequest);

	if (gCall[zCall].pFirstRequest == NULL)
	{
		gCall[zCall].pFirstRequest =
			(RequestList *) arc_malloc (zCall, mod, __LINE__,
										sizeof (RequestList));

		if (gCall[zCall].pFirstRequest == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s]  Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexRequest);
			return (-1);
		}

		memcpy (&gCall[zCall].pFirstRequest->msgToDM, zpMsgToDM,
				sizeof (struct MsgToDM));
		gCall[zCall].pFirstRequest->nextp = NULL;
		gCall[zCall].pLastRequest = gCall[zCall].pFirstRequest;
	}
	else
	{
		pRequest =
			(RequestList *) arc_malloc (zCall, mod, __LINE__,
										sizeof (RequestList));

		if (pRequest == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s]  Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexRequest);
			return (-1);
		}

		memcpy (&pRequest->msgToDM, zpMsgToDM, sizeof (struct MsgToDM));

		pRequest->nextp = NULL;
		gCall[zCall].pLastRequest->nextp = pRequest;
		gCall[zCall].pLastRequest = pRequest;
	}

	pthread_mutex_unlock (&gCall[zCall].gMutexRequest);

	return (0);

}								/*END: int addToAppRequestList */

///This function clears the request list.
int
clearAppRequestList (int zCall)
{
	static char     mod[] = { "clearAppRequestList" };
	RequestList    *pRequest;

	if (zCall < 0 || zCall > MAX_PORTS)
	{
		return (0);
	}

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "Pending requests total",
				  gCall[zCall].pendingOutputRequests);

	//gCall[zCall].pendingOutputRequests = 0;

	pthread_mutex_lock (&gCall[zCall].gMutexRequest);

	while (gCall[zCall].pFirstRequest != NULL)
	{
		pRequest = gCall[zCall].pFirstRequest->nextp;
		arc_free (zCall, mod, __LINE__, gCall[zCall].pFirstRequest,
				  sizeof (RequestList));

		free (gCall[zCall].pFirstRequest);

		gCall[zCall].pFirstRequest = pRequest;

	}

	gCall[zCall].pLastRequest = NULL;
	gCall[zCall].pFirstRequest = NULL;

	pthread_mutex_unlock (&gCall[zCall].gMutexRequest);

	return (0);

}								/*END: int clearAppRequestList */

///This function is used to keep track of DMOP_SPEAK commands.
int
addToSpeakList (struct Msg_Speak *zpSpeak, long *zpQueueElementId, int zLine)
{
	static char     mod[] = { "addToSpeakList" };
	SpeakList      *pSpeak;
	int             zCall = zpSpeak->appCallNum;

	if (zCall < gStartPort || zCall > gEndPort)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT, ERR,
				   "Invalid port (%d) received; must be between %d and %d. Unable to add request to list.",
				   zCall, gStartPort, gEndPort);
		return (-1);
	}

	pthread_mutex_lock (&gCall[zCall].gMutexSpeak);

	*zpQueueElementId = zpSpeak->appRef;
	if (gCall[zCall].pFirstSpeak == NULL)
	{
		gCall[zCall].pFirstSpeak =
			(SpeakList *) arc_malloc (zCall, mod, __LINE__,
									  sizeof (SpeakList));

		if (gCall[zCall].pFirstSpeak == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
			return (-1);
		}

		memset (gCall[zCall].pFirstSpeak, 0, sizeof (SpeakList));

		memcpy (&gCall[zCall].pFirstSpeak->msgSpeak, zpSpeak,
				sizeof (struct Msg_Speak));
		gCall[zCall].pFirstSpeak->nextp = NULL;
		gCall[zCall].pLastSpeak = gCall[zCall].pFirstSpeak;
		gCall[zCall].pFirstSpeak->isAudioPresent = 1;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[from %d] pFirstSpeak just created, [0x%u]",  zLine);
		if (gCall[zCall].pFirstSpeak->msgSpeak.opcode == DMOP_PLAYMEDIAAUDIO)
		{
			gCall[zCall].pFirstSpeak->msgPlayMedia.syncAudioVideo = NO;
		}
	}
	else
	{
		pSpeak =
			(SpeakList *) arc_malloc (zCall, mod, __LINE__,
									  sizeof (SpeakList));

		if (pSpeak == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
			return (-1);
		}

		memset (pSpeak, 0, sizeof (SpeakList));

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "", -1);

		memcpy (&pSpeak->msgSpeak, zpSpeak, sizeof (struct Msg_Speak));
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[from %d] added to speakList, [0x%u]",  zLine);
		pSpeak->nextp = NULL;
		gCall[zCall].pLastSpeak->nextp = pSpeak;
		gCall[zCall].pLastSpeak = pSpeak;
		pSpeak->isAudioPresent = 1;
	}

	pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
	return (0);

}								/*END: int addToSpeakList */

///This function is used to keep track of DMOP_SPEAK commands.
int
addToSpeakList (struct Msg_SpeakMrcpTts *zpSpeak, long *zpQueueElementId, int zLine)
{
	static char     mod[] = { "addToSpeakList:mrcpTTS" };
	SpeakList      *pSpeak;
	SpeakList      *tmpSpeak;
	long            lMsgMTtsSpeakId = -1;
	int             zCall = zpSpeak->appCallNum;
	int             i;

	if (zCall < gStartPort || zCall > gEndPort)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT, ERR,
				   "Invalid port (%d) received; must be between %d and %d. Unable to add request to list.",
				   zCall, gStartPort, gEndPort);
		return (-1);
	}

	pthread_mutex_lock (&gCall[zCall].gMutexSpeak);

	lMsgMTtsSpeakId = *zpQueueElementId;

	*zpQueueElementId = zpSpeak->appRef;

	if (gCall[zCall].pFirstSpeak == NULL)
	{
		gCall[zCall].pFirstSpeak =
			(SpeakList *) arc_malloc (zCall, mod, __LINE__,
									  sizeof (SpeakList));

		if (gCall[zCall].pFirstSpeak == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
			return (-1);
		}

		memset (gCall[zCall].pFirstSpeak, 0, sizeof (SpeakList));

		memcpy (&gCall[zCall].pFirstSpeak->msgSpeakTts, zpSpeak,
				sizeof (struct Msg_SpeakMrcpTts));
		memcpy (&gCall[zCall].pFirstSpeak->msgSpeak, zpSpeak,
				sizeof (struct Msg_Speak));
		gCall[zCall].pFirstSpeak->isMTtsPresent = 1;
		gCall[zCall].pFirstSpeak->msgMTtsSpeakId = lMsgMTtsSpeakId;
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Set first SpeakList SpeakId to %ld; resource=(%s). "
				   "id=%ld "
				   "&gCall[%d].pFirstSpeak->msgSpeakTts=%u",
				   lMsgMTtsSpeakId,
				   gCall[zCall].pFirstSpeak->msgSpeakTts.resource,
				   gCall[zCall].pFirstSpeak->msgMTtsSpeakId,
				   zCall, &(gCall[zCall].pFirstSpeak->msgSpeakTts));

		gCall[zCall].pFirstSpeak->nextp = NULL;
		gCall[zCall].pLastSpeak = gCall[zCall].pFirstSpeak;
	}
	else
	{
		pSpeak =
			(SpeakList *) arc_malloc (zCall, mod, __LINE__,
									  sizeof (SpeakList));

		if (pSpeak == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
			return (-1);
		}

		memset (pSpeak, 0, sizeof (SpeakList));

		memcpy (&pSpeak->msgSpeakTts, zpSpeak,
				sizeof (struct Msg_SpeakMrcpTts));
		memcpy (&pSpeak->msgSpeak, zpSpeak, sizeof (struct Msg_Speak));
		pSpeak->nextp = NULL;
		gCall[zCall].pLastSpeak->nextp = pSpeak;
		gCall[zCall].pLastSpeak = pSpeak;

		pSpeak->isMTtsPresent = 1;
		pSpeak->msgMTtsSpeakId = lMsgMTtsSpeakId;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Added SpeakList SpeakId to %ld.", lMsgMTtsSpeakId);

		//  debug
		tmpSpeak = gCall[zCall].pFirstSpeak;
		i = 0;
		while (tmpSpeak != NULL)
		{
			tmpSpeak = tmpSpeak->nextp;
		}
	}

	pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
	return (0);

}								/*END: int addToSpeakList */

///This function is used to keep track of DMOP_SPEAK commands.
int
addToSpeakList (struct Msg_Speak *zpSpeak,
				long *zpQueueElementId, struct Msg_PlayMedia *zpPlayMedia, int zLine)
{
	static char     mod[] = { "addToSpeakList" };
	SpeakList      *pSpeak;
	int             zCall = zpPlayMedia->appCallNum;

	if (zCall < gStartPort || zCall > gEndPort)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT, ERR,
				   "Invalid port (%d) received; must be between %d and %d. Unable to add request to list.",
				   zCall, gStartPort, gEndPort);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "port", zCall);

	pthread_mutex_lock (&gCall[zCall].gMutexSpeak);

	*zpQueueElementId = zpSpeak->appRef;

	if (gCall[zCall].pFirstSpeak == NULL)
	{
		gCall[zCall].pFirstSpeak =
			(SpeakList *) arc_malloc (zCall, mod, __LINE__,
									  sizeof (SpeakList));

		if (gCall[zCall].pFirstSpeak == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
			return (-1);
		}

		memset (gCall[zCall].pFirstSpeak, 0, sizeof (SpeakList));

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "", -1);

		memcpy (&gCall[zCall].pFirstSpeak->msgSpeak, zpSpeak,
				sizeof (struct Msg_Speak));

		//gCall[zCall].pFirstSpeak->msgSpeak.interruptOption  = zpPlayMedia->interruptOption;       

		gCall[zCall].pFirstSpeak->isSpeakDone = NO;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "audioFileName=%s", zpPlayMedia->audioFileName);

		if (zpPlayMedia->audioFileName != NULL &&
			zpPlayMedia->audioFileName[0] != '\0' &&
			strcmp (zpPlayMedia->audioFileName, "NULL"))
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting isAudioPresent to 1.");
			gCall[zCall].pFirstSpeak->isAudioPresent = 1;
		}

		if (zpPlayMedia->sync == PLAY_QUEUE_SYNC)
		{
			memcpy (&gCall[zCall].pFirstSpeak->msgPlayMedia, zpPlayMedia,
					sizeof (struct Msg_PlayMedia));

			gCall[zCall].pFirstSpeak->isAudioPresent = 1;
		}

		gCall[zCall].pFirstSpeak->nextp = NULL;
		gCall[zCall].pLastSpeak = gCall[zCall].pFirstSpeak;
	}
	else
	{
		pSpeak =
			(SpeakList *) arc_malloc (zCall, mod, __LINE__,
									  sizeof (SpeakList));

		if (pSpeak == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
			return (-1);
		}

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "", -1);

		memset (pSpeak, 0, sizeof (SpeakList));

		pSpeak->isSpeakDone = NO;

		memcpy (&pSpeak->msgSpeak, zpSpeak, sizeof (struct Msg_Speak));
		//pSpeak->msgSpeak.interruptOption  = zpPlayMedia->interruptOption;         

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "audioFileName=%s, interruptOption=%d",
				   zpPlayMedia->audioFileName, pSpeak->msgSpeak.interruptOption);

		if (zpPlayMedia->audioFileName != NULL &&
			zpPlayMedia->audioFileName[0] != '\0' &&
			strcmp (zpPlayMedia->audioFileName, "NULL"))
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting isAudioPresent to 1.");
			pSpeak->isAudioPresent = 1;
		}

		if (zpPlayMedia->sync == PLAY_QUEUE_SYNC)
		{
			memcpy (&pSpeak->msgPlayMedia, zpPlayMedia,
					sizeof (struct Msg_PlayMedia));
			pSpeak->isAudioPresent = 1;
		}
		pSpeak->nextp = NULL;
		gCall[zCall].pLastSpeak->nextp = pSpeak;
		gCall[zCall].pLastSpeak = pSpeak;
	}

	pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
	return (0);

}								/*END: int addToSpeakList */

///This function is used to keep track of DMOP_SPEAK commands.
int
addToBkpSpeakList (struct Msg_Speak *zpSpeak,
				   long *zpQueueElementId, struct Msg_PlayMedia *zpPlayMedia)
{
	static char     mod[] = { "addToBkpSpeakList" };
	SpeakList      *pSpeak;
	int             zCall = zpSpeak->appCallNum;

	if (zCall < gStartPort || zCall > gEndPort)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT, ERR,
				   "Invalid port (%d) received; must be between %d and %d. Unable to add request to list.",
				   zCall, gStartPort, gEndPort);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "port", zCall);

	pthread_mutex_lock (&gCall[zCall].gMutexBkpSpeak);

	*zpQueueElementId = zpSpeak->appRef;

	if (gCall[zCall].pFirstBkpSpeak == NULL)
	{
		gCall[zCall].pFirstBkpSpeak =
			(SpeakList *) arc_malloc (zCall, mod, __LINE__,
									  sizeof (SpeakList));

		if (gCall[zCall].pFirstBkpSpeak == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexBkpSpeak);
			return (-1);
		}

		memset (gCall[zCall].pFirstBkpSpeak, 0, sizeof (SpeakList));

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "", -1);

		memcpy (&gCall[zCall].pFirstBkpSpeak->msgSpeak, zpSpeak,
				sizeof (struct Msg_Speak));

		gCall[zCall].pFirstBkpSpeak->isSpeakDone = NO;

		if (zpPlayMedia->audioFileName != NULL &&
			zpPlayMedia->audioFileName[0] != '\0' &&
			strcmp (zpPlayMedia->audioFileName, "NULL"))
		{
			gCall[zCall].pFirstBkpSpeak->isAudioPresent = 1;
		}

		if (zpPlayMedia->sync == PLAY_QUEUE_SYNC)
		{
			memcpy (&gCall[zCall].pFirstBkpSpeak->msgPlayMedia, zpPlayMedia,
					sizeof (struct Msg_PlayMedia));

		}

		gCall[zCall].pFirstBkpSpeak->nextp = NULL;
		gCall[zCall].pLastBkpSpeak = gCall[zCall].pFirstBkpSpeak;
	}
	else
	{
		pSpeak =
			(SpeakList *) arc_malloc (zCall, mod, __LINE__,
									  sizeof (SpeakList));

		if (pSpeak == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexBkpSpeak);
			return (-1);
		}

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "", -1);

		memset (pSpeak, 0, sizeof (SpeakList));

		pSpeak->isSpeakDone = NO;

		memcpy (&pSpeak->msgSpeak, zpSpeak, sizeof (struct Msg_Speak));

		if (zpPlayMedia->audioFileName != NULL &&
			zpPlayMedia->audioFileName[0] != '\0' &&
			strcmp (zpPlayMedia->audioFileName, "NULL"))
		{
			pSpeak->isAudioPresent = 1;
		}

		if (zpPlayMedia->sync == PLAY_QUEUE_SYNC)
		{
			memcpy (&pSpeak->msgPlayMedia, zpPlayMedia,
					sizeof (struct Msg_PlayMedia));
		}
		pSpeak->nextp = NULL;
		gCall[zCall].pLastBkpSpeak->nextp = pSpeak;
		gCall[zCall].pLastBkpSpeak = pSpeak;
	}

	pthread_mutex_unlock (&gCall[zCall].gMutexBkpSpeak);
	return (0);

}								/*END: int addToBkpSpeakList */

int
modifySpeakList (int zCall, int vloopingOption, int aloopingOption)
{
	static char     mod[] = "modifySpeakList";

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "aloopingOption=%d, vloopingOption=%d",
			   aloopingOption, vloopingOption);

	if (aloopingOption != YES && aloopingOption != NO)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Invalid value of aloopingOption, setting it to NO");
		aloopingOption = NO;
	}
	if (vloopingOption != YES && vloopingOption != NO)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Invalid value of vloopingOption, setting it to NO");
		vloopingOption = NO;
	}

	SpeakList      *lpSpeak = gCall[zCall].pFirstSpeak;

	if (lpSpeak == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "lpSpeak is NULL. Returning -1.");
		return -1;
	}

	if (zCall < gStartPort || zCall > gEndPort)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT, ERR,
				   "Invalid port (%d) received; must be between %d and %d. Unable to modify speak list.",
				   zCall, gStartPort, gEndPort);

		return (-1);
	}

	pthread_mutex_lock (&gCall[zCall].gMutexSpeak);

	while (lpSpeak != NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "lpSpeak=%p", lpSpeak);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting lpSpeak->audioLooping to %d", aloopingOption);
		lpSpeak->audioLooping = aloopingOption;
		lpSpeak = lpSpeak->nextp;
	}
	pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);

	return 0;
}

///This function finds out the filename for the current DMOP_SPEAK command.
int
getSpeakFileName (int zCall, SpeakList * pSpeak)
{
	static char     mod[] = { "getSpeakFileName" };
	SpeakList      *pTemp = NULL;

	if (pthread_mutex_trylock (&gCall[zCall].gMutexSpeak) == EBUSY)
	{
		return (0);
	}

	pTemp = gCall[zCall].pFirstSpeak;

	while (pTemp != NULL)
	{
		if (pSpeak->msgSpeak.appRef == pTemp->msgSpeak.appRef)
		{
			sprintf (pSpeak->msgSpeak.file, "%s", pTemp->msgSpeak.file);
			break;
		}

		pTemp = pTemp->nextp;

	}							/*END: while */

	pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);

	return (0);

}								/*END: int getSpeakFileName */

void           *
copyBkpToCurrentQueue (int zCall)
{
	static char     mod[] = { "copyBkpToCurrentQueue" };

	if (pthread_mutex_trylock (&gCall[zCall].gMutexBkpSpeak) == EBUSY)
	{
		return (NULL);
	}

	SpeakList      *pTemp = NULL;

	gCall[zCall].pFirstSpeak = gCall[zCall].pFirstBkpSpeak;
	gCall[zCall].pLastSpeak = gCall[zCall].pLastBkpSpeak;

	gCall[zCall].pFirstBkpSpeak = NULL;
	gCall[zCall].pLastBkpSpeak = NULL;

	pthread_mutex_unlock (&gCall[zCall].gMutexBkpSpeak);

	return (NULL);

}								/*END: void* copyBkpToCurrentQueue */

void           *
copySpeakList (int zCall)
{
	static char     mod[] = { "copySpeakList" };

	SpeakList      *pSpeak = NULL;
	SpeakList      *pTemp = NULL;
	SpeakList      *pPrev = NULL;
	SpeakList      *pFirst = NULL;

	if (pthread_mutex_trylock (&gCall[zCall].gMutexSpeak) == EBUSY)
	{
		return (NULL);
	}

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "port", zCall);

	pTemp = gCall[zCall].pFirstSpeak;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		   "Copying speakList pFirstSpeak=0x%u",  gCall[zCall].pFirstSpeak );
	while (pTemp != NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "pTemp->msgPlayMedia.audioFileName=%s, pTemp->isAudioPresent=%d, "
				   "pTemp->isMTtsPresent=%d, pTemp->msgMTtsSpeakId=%ld",
				   pTemp->msgPlayMedia.audioFileName, pTemp->isAudioPresent,
				   pTemp->isMTtsPresent, pTemp->msgMTtsSpeakId);

		if ((pTemp->isAudioPresent == 1) || (pTemp->isMTtsPresent == 1))
		{

			pSpeak =
				(SpeakList *) arc_malloc (zCall, mod, __LINE__,
										  sizeof (SpeakList));

			memcpy (pSpeak, pTemp, sizeof (SpeakList));

			pSpeak->nextp = NULL;

			if (pPrev != NULL)
			{
				pPrev->nextp = pSpeak;
			}
			else
			{
				pFirst = pSpeak;
			}

			pPrev = pSpeak;

		}

		pTemp = pTemp->nextp;

	}							/*END: while */

	pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);

	return (pFirst);

}								/*END: void* copySpeakList */

int
clearSpeakList (int zCall, int zLine)
{
	char            mod[] = { "clearSpeakList" };
	SpeakList      *pSpeak;
	char			buf[128];

	if (pthread_mutex_trylock (&gCall[zCall].gMutexSpeak) == EBUSY)
	{
		sprintf(buf, "CLEANING SPEAK LIST [called from %d] FAILED", zLine);
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "CLEANING SPEAK LIST FAILED",
					  0);
		return (-1);
	}

	sprintf(buf, "CLEANING SPEAK LIST [called from %d]", zLine);
	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", buf, 0);
//	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//			"DJB:[called from %d] CLEANING SPEAK LIST ", zLine);

	while (gCall[zCall].pFirstSpeak != NULL)
	{
	//	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
	//		"DJB:[called from %d] msgSpeak.list=%d", zLine, gCall[zCall].pFirstSpeak->msgSpeak.list);
		if (gCall[zCall].pFirstSpeak->msgSpeak.list == 2)	// MRCP TTS
		{
			arc_unlink (zCall, mod, gCall[zCall].pFirstSpeak->msgSpeak.key);
	//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
	//		 	"DJB:[called from %d] unlinked (%s)", zLine, gCall[zCall].pFirstSpeak->msgSpeak.key);
		}

		pSpeak = gCall[zCall].pFirstSpeak->nextp;

		arc_free (zCall, mod, __LINE__, gCall[zCall].pFirstSpeak,
				  sizeof (SpeakList));
		free (gCall[zCall].pFirstSpeak);
	//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
	//		 	"DJB:[called from %d] Freed pFirstSpeak (%s)", zLine, gCall[zCall].pFirstSpeak->msgSpeak.key);
		gCall[zCall].pFirstSpeak = pSpeak;
	}

	gCall[zCall].pLastSpeak = NULL;
	gCall[zCall].pFirstSpeak = NULL;

	gCall[zCall].currentSpeak = NULL;
	gCall[zCall].currentPlayMedia = NULL;

	pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);

	return (0);

}								/*END: int clearSpeakList */

int
clearBkpSpeakList (int zCall)
{
	char            mod[] = { "clearBkpSpeakList" };
	SpeakList      *pSpeak;

	if (pthread_mutex_trylock (&gCall[zCall].gMutexBkpSpeak) == EBUSY)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "CLEANING SPEAK LIST FAILED",
					  0);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "CLEANING SPEAK LIST", 0);

	while (gCall[zCall].pFirstBkpSpeak != NULL)
	{
		if (gCall[zCall].pFirstBkpSpeak->msgSpeak.list == 2)	// MRCP TTS
		{
			arc_unlink (zCall, mod,
						gCall[zCall].pFirstBkpSpeak->msgSpeak.key);
		}

		pSpeak = gCall[zCall].pFirstBkpSpeak->nextp;
		arc_free (zCall, mod, __LINE__, gCall[zCall].pFirstBkpSpeak,
				  sizeof (SpeakList));
		free (gCall[zCall].pFirstBkpSpeak);
		gCall[zCall].pFirstBkpSpeak = pSpeak;
	}

	gCall[zCall].pLastBkpSpeak = NULL;
	gCall[zCall].pFirstBkpSpeak = NULL;

	pthread_mutex_unlock (&gCall[zCall].gMutexBkpSpeak);

	return (0);

}								/*END: int clearSpeakList */

int
getWavHeader (int zCall, char *zBuffer, int zSize)
{
	char            mod[] = { "getWavHeader" };
	WavHeaderStruct yWavHeaderStruct;
	WavHeaderStruct *zpwaveHeaderStruct = &yWavHeaderStruct;

	strncpy (zBuffer, gCall[zCall].silenceBuffer, 160);

	yWavHeaderStruct.rLen = zSize - 8;
	yWavHeaderStruct.dLen = yWavHeaderStruct.rLen - 38;

	sprintf (zpwaveHeaderStruct->rId, "RIFF");
	zpwaveHeaderStruct->rLen = zSize - 8;
	sprintf (zpwaveHeaderStruct->wId, "%s", "WAVE");
	sprintf (zpwaveHeaderStruct->fId, "%s", "fmt ");
	zpwaveHeaderStruct->fLen = 18;
	zpwaveHeaderStruct->nFormatTag = 7;
	zpwaveHeaderStruct->nChannels = 1;
	zpwaveHeaderStruct->nSamplesPerSec = 8000L;
	zpwaveHeaderStruct->nAvgBytesPerSec = 8000L;
	zpwaveHeaderStruct->nBlockAlign = 1;
	zpwaveHeaderStruct->nBitsPerSample = 8;
	zpwaveHeaderStruct->extSize = 0;
	zpwaveHeaderStruct->dId[0] = 'd';
	zpwaveHeaderStruct->dId[1] = 'a';
	zpwaveHeaderStruct->dId[2] = 't';
	zpwaveHeaderStruct->dId[3] = 'a';
	zpwaveHeaderStruct->dLen = zSize - 38;

	memcpy (zBuffer, &yWavHeaderStruct, sizeof (WavHeaderStruct));

	return (0);

}								/*END: getWavHeader */

int
writeWavHeaderToFile (int zCall, FILE * infile)
{
	char            mod[] = { "writeWavHeaderToFile" };
	guint32         yIntFileSize;
	WavHeaderStruct yWavHeaderStruct;
	WavHeaderStruct *zpwaveHeaderStruct = &yWavHeaderStruct;

	// if (strstr (gCall[zCall].audioCodecString, "711"))
    // there was a race to read this data so I switched to using the ints 
    // these should not change 
	if ((gCall[zCall].codecType == 0) || (gCall[zCall].codecType == 8))
	{
		;
	}
	else
	{
		return (0);
	}

	if (!infile)
	{
		return (-1);
	}

	yIntFileSize = ftell (infile);

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "File size ", yIntFileSize);

	rewind (infile);

	zpwaveHeaderStruct->rId[0] = 'R';
	zpwaveHeaderStruct->rId[1] = 'I';
	zpwaveHeaderStruct->rId[2] = 'F';
	zpwaveHeaderStruct->rId[3] = 'F';

	zpwaveHeaderStruct->rLen = (yIntFileSize <= 0) ? 46 : yIntFileSize - 8;

	zpwaveHeaderStruct->wId[0] = 'W';
	zpwaveHeaderStruct->wId[1] = 'A';
	zpwaveHeaderStruct->wId[2] = 'V';
	zpwaveHeaderStruct->wId[3] = 'E';

	zpwaveHeaderStruct->fId[0] = 'f';
	zpwaveHeaderStruct->fId[1] = 'm';
	zpwaveHeaderStruct->fId[2] = 't';
	zpwaveHeaderStruct->fId[3] = ' ';

	zpwaveHeaderStruct->fLen = 18;
	zpwaveHeaderStruct->nFormatTag = 0x0007;
	zpwaveHeaderStruct->nChannels = 1;
	zpwaveHeaderStruct->nSamplesPerSec = 8000;
	zpwaveHeaderStruct->nAvgBytesPerSec = 8000;
	zpwaveHeaderStruct->nBlockAlign = 1;
	zpwaveHeaderStruct->nBitsPerSample = 8;
	zpwaveHeaderStruct->extSize = 0;

	zpwaveHeaderStruct->dId[0] = 'd';
	zpwaveHeaderStruct->dId[1] = 'a';
	zpwaveHeaderStruct->dId[2] = 't';
	zpwaveHeaderStruct->dId[3] = 'a';

	zpwaveHeaderStruct->dLen = (yIntFileSize <= 0) ? 0 : yIntFileSize - 46;

	fwrite (zpwaveHeaderStruct->rId, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->rLen, 4, 1, infile);
	fwrite (zpwaveHeaderStruct->wId, 4, 1, infile);
	fwrite (zpwaveHeaderStruct->fId, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->fLen, 4, 1, infile);

	fwrite (&zpwaveHeaderStruct->nFormatTag, 2, 1, infile);
	fwrite (&zpwaveHeaderStruct->nChannels, 2, 1, infile);
	fwrite (&zpwaveHeaderStruct->nSamplesPerSec, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->nAvgBytesPerSec, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->nBlockAlign, 2, 1, infile);
	fwrite (&zpwaveHeaderStruct->nBitsPerSample, 2, 1, infile);
	fwrite (&zpwaveHeaderStruct->extSize, 2, 1, infile);
	fwrite (zpwaveHeaderStruct->dId, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->dLen, 4, 1, infile);

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "rLen ", zpwaveHeaderStruct->rLen);
	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "dLen ", zpwaveHeaderStruct->dLen);

	return (0);

}								/*END: int writeWavHeaderToFile */

int
trimStartOfWavFile (int zCall, char *zFile, time_t t1, time_t t2)
{
	int             yRc;
	int             yIntFread = 0;
	int             yIntFwrite = 0;
	char            mod[] = { "trimStartOfWavFile" };

	char            yStrBackupFile[256];
	char            yStrBackupFile1[256];

	FILE           *fp_write = NULL;
	FILE           *fp_read = NULL;

	struct stat     yStat;

	char           *buf = NULL;

	int             ySecondsToBeTrimmed = (t2 - t1) - 2;
	int             yBytesToBeTrimmed = (ySecondsToBeTrimmed * 8192) - 8192;

	if (t2 <= t1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "No action for ySecondsToBeTrimmed=%d, yBytesToBeTrimmed=%d.",
				   ySecondsToBeTrimmed, yBytesToBeTrimmed);

		return (0);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "ySecondsToBeTrimmed=%d, yBytesToBeTrimmed=%d.",
			   ySecondsToBeTrimmed, yBytesToBeTrimmed);

	sprintf (yStrBackupFile, "%s.bkp", zFile);
	sprintf (yStrBackupFile1, "%s.bkp1", zFile);

	if ( gCall[zCall].googleRecordIsActive == 0 )
	{
		if (ySecondsToBeTrimmed <= 2)
		{
			return (0);
		}
	}
//	else
//	{
//		yBytesToBeTrimmed = yBytesToBeTrimmed;
//	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] Chopping... %d bytes", __LINE__, yBytesToBeTrimmed);
	if ((yRc = stat (zFile, &yStat)) < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to stat file", zFile,
					  errno);
		return (-1);
	}

	if (yStat.st_size < yBytesToBeTrimmed)
	{

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Audio file too small to trim",
					  zFile, yStat.st_size);
		return (-1);
	}

	fp_write = fopen (yStrBackupFile, "w");
	if (fp_write == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to open file",
					  yStrBackupFile, errno);
		return (-1);
	}

	fp_read = fopen (zFile, "r");
	if (fp_read == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to open file", zFile,
					  errno);
		fclose (fp_write);
		return (-1);
	}

	/*Create empty header first */
	if (gCall[zCall].recordUtteranceWithWav == 1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Writing wav header to file (%s).", yStrBackupFile);

		writeWavHeaderToFile (zCall, fp_write);
	}

	buf = (char *) arc_malloc (zCall, mod, __LINE__, yStat.st_size + 1);

	if (buf == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to malloc", "", errno);
		fclose (fp_write);
		fclose (fp_read);
		return (-1);
	}

	memset (buf, 0, yStat.st_size + 1);

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Audio file size", zFile,
				  yStat.st_size);
	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Audio trim size", zFile,
				  (int) yBytesToBeTrimmed);

	if ((yIntFread = fread (buf, 1, yStat.st_size, fp_read)) < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to read from file", zFile,
					  errno);
		fclose (fp_write);
		fclose (fp_read);
		free (buf);
		arc_free (zCall, mod, __LINE__, buf, yStat.st_size + 1);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Read from file", zFile, yIntFread);

	if ((yIntFwrite =
		 fwrite (buf + (int) yBytesToBeTrimmed, 1,
				 (int) (yStat.st_size - yBytesToBeTrimmed), fp_write)) < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to write to file",
					  yStrBackupFile, yIntFwrite);
		fclose (fp_write);
		fclose (fp_read);

		arc_free (zCall, mod, __LINE__, buf, yStat.st_size + 1);
		free (buf);

		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Wrote to file", yStrBackupFile,
				  yIntFwrite);

	/*Create empty header first */
	if (gCall[zCall].recordUtteranceWithWav == 1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Writing wav header to file (%s).", yStrBackupFile);
		writeWavHeaderToFile (zCall, fp_write);
	}

	fclose (fp_write);
	fclose (fp_read);

	arc_free (zCall, mod, __LINE__, buf, yStat.st_size + 1);
	free (buf);

	yRc = rename (yStrBackupFile, zFile);
	if (yRc < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to rename file",
					  yStrBackupFile, errno);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Successfully trimmed", zFile, zCall);

	return (0);

}								/*END: trimStartOfWavFile */

int
trimStartAndEndOfWavFile (int zCall, char *zFile, time_t t1, time_t t2, int numTrailSeconds)
{
	int             yRc;
	int             yIntFread = 0;
	int             yIntFwrite = 0;
	char            mod[] = { "trimStartAndEndOfWavFile" };

	char            yStrBackupFile[256];

	FILE           *fp_write = NULL;
	FILE           *fp_read = NULL;

	struct stat     yStat;

	char           *buf = NULL;

	int             ySecondsToBeTrimmed = (t2 - t1) - 2;
	int             yBytesToBeTrimmed = (ySecondsToBeTrimmed * 8192) - 8192;
	int             yTrailBytesToBeTrimmed = numTrailSeconds * 8192 - 2048;

	if (t2 <= t1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "No action for ySecondsToBeTrimmed=%d, yBytesToBeTrimmed=%d.",
				   ySecondsToBeTrimmed, yBytesToBeTrimmed);

		return (0);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "ySecondsToBeTrimmed=%d, yBytesToBeTrimmed=%d.",
			   ySecondsToBeTrimmed, yBytesToBeTrimmed);

	sprintf (yStrBackupFile, "%s.bkp", zFile);

	if (ySecondsToBeTrimmed <= 2)
	{
		return (0);
	}

	if ((yRc = stat (zFile, &yStat)) < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to stat file", zFile,
					  errno);
		return (-1);
	}

	if (yStat.st_size < yBytesToBeTrimmed)
	{

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Audio file too small to trim",
					  zFile, yStat.st_size);
		return (-1);
	}

	fp_write = fopen (yStrBackupFile, "w");
	if (fp_write == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to open file",
					  yStrBackupFile, errno);
		return (-1);
	}

	fp_read = fopen (zFile, "r");
	if (fp_read == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to open file", zFile,
					  errno);
		fclose (fp_write);
		return (-1);
	}

	writeWavHeaderToFile (zCall, fp_write);

	buf = (char *) arc_malloc (zCall, mod, __LINE__, yStat.st_size + 1);

	if (buf == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to malloc", "", errno);
		fclose (fp_write);
		fclose (fp_read);
		return (-1);
	}

	memset (buf, 0, yStat.st_size + 1);

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Audio file size", zFile,
				  yStat.st_size);
	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Audio trim size", zFile,
				  (int) yBytesToBeTrimmed);

	if ((yIntFread = fread (buf, 1, yStat.st_size, fp_read)) < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to read from file", zFile,
					  errno);
		fclose (fp_write);
		fclose (fp_read);
		free (buf);
		arc_free (zCall, mod, __LINE__, buf, yStat.st_size + 1);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Read from file", zFile, yIntFread);

	if ((yIntFwrite =
		 fwrite (buf + (int) yBytesToBeTrimmed, 1,
				 (int) (yStat.st_size - yBytesToBeTrimmed - yTrailBytesToBeTrimmed), fp_write)) < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to write to file",
					  yStrBackupFile, yIntFwrite);
		fclose (fp_write);
		fclose (fp_read);

		arc_free (zCall, mod, __LINE__, buf, yStat.st_size + 1);
		free (buf);

		return (-1);
	}
	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Wrote to file", yStrBackupFile,
				  yIntFwrite);

	writeWavHeaderToFile (zCall, fp_write);

	fclose (fp_write);
	fclose (fp_read);

	arc_free (zCall, mod, __LINE__, buf, yStat.st_size + 1);
	free (buf);

#if 0
	yRc = rename (yStrBackupFile, zFile);
	if (yRc < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to rename file",
					  yStrBackupFile, errno);
		return (-1);
	}
#endif

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Successfully trimmed", zFile, zCall);

	return (0);

}								/*END: trimStartAndEndOfWavFile */

int
trimEndOfWavFile (int zCall, char *zFile, int zNumSeconds)
{
	int             yRc;
	int             yIntFread = 0;
	int             yIntFwrite = 0;
	char            mod[] = { "trimEndOfWavFile" };

	char            yStrBackupFile[256];

	FILE           *fp_write = NULL;
	FILE           *fp_read = NULL;

	struct stat     yStat;

	char           *buf = NULL;

	int             ySecondsToBeTrimmed = zNumSeconds;
	int             yBytesToBeTrimmed = ySecondsToBeTrimmed * 8192 - 2048;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "ySecondsToBeTrimmed=%d, yBytesToBeTrimmed=%d.",
			   ySecondsToBeTrimmed, yBytesToBeTrimmed);

	sprintf (yStrBackupFile, "%s.bkp", zFile);

	if ((yRc = stat (zFile, &yStat)) < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to stat file", zFile,
					  errno);
		return (-1);
	}

	if (yStat.st_size < yBytesToBeTrimmed)
	{

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Audio file too small to trim",
					  zFile, yStat.st_size);
		return (-1);
	}

	fp_write = fopen (yStrBackupFile, "w");
	if (fp_write == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to open file",
					  yStrBackupFile, errno);
		return (-1);
	}

	fp_read = fopen (zFile, "r");
	if (fp_read == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to open file", zFile,
					  errno);
		fclose (fp_write);
		return (-1);
	}

//	/*Create empty header first */
	writeWavHeaderToFile (zCall, fp_write);

	buf = (char *) arc_malloc (zCall, mod, __LINE__, yStat.st_size + 1);

	if (buf == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to malloc", "", errno);
		fclose (fp_write);
		fclose (fp_read);
		return (-1);
	}

	memset (buf, 0, yStat.st_size + 1);

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Audio file size", zFile,
				  yStat.st_size);
	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Audio trim size", zFile,
				  (int) yBytesToBeTrimmed);

	if ((yIntFread = fread (buf, 1, yStat.st_size, fp_read)) < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to read from file", zFile,
					  errno);
		fclose (fp_write);
		fclose (fp_read);
		free (buf);
		arc_free (zCall, mod, __LINE__, buf, yStat.st_size + 1);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Read from file", zFile, yIntFread);

	if ((yIntFwrite =
		 fwrite (buf, 1, (int) (yStat.st_size - yBytesToBeTrimmed), fp_write)) < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to write to file",
					  yStrBackupFile, yIntFwrite);
		fclose (fp_write);
		fclose (fp_read);

		arc_free (zCall, mod, __LINE__, buf, yStat.st_size + 1);
		free (buf);

		return (-1);
	}
	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Wrote to file", yStrBackupFile,
				  yIntFwrite);

	/*Create empty header first */
	writeWavHeaderToFile (zCall, fp_write);

	fclose (fp_write);
	fclose (fp_read);

	arc_free (zCall, mod, __LINE__, buf, yStat.st_size + 1);
	free (buf);

	yRc = rename (yStrBackupFile, zFile);
	if (yRc < 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Failed to rename file",
					  yStrBackupFile, errno);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Successfully trimmed", zFile, zCall);

	return (0);

}								/*END: trimEndOfWavFile */

#ifdef VOICE_BIOMETRICS
int writePCMWavHeaderToFile (int zCall, FILE * infile)
{
	char            mod[] = { "writePCMWavHeaderToFile" };
	guint32         yIntFileSize;
	WavHeaderStruct yWavHeaderStruct;
	WavHeaderStruct *zpwaveHeaderStruct = &yWavHeaderStruct;

	if (!infile)
	{
		return (-1);
	}

	yIntFileSize = ftell (infile);

	rewind (infile);

	zpwaveHeaderStruct->rId[0] = 'R';
	zpwaveHeaderStruct->rId[1] = 'I';
	zpwaveHeaderStruct->rId[2] = 'F';
	zpwaveHeaderStruct->rId[3] = 'F';

	zpwaveHeaderStruct->rLen = (yIntFileSize <= 0) ? 46 : yIntFileSize - 8;

	zpwaveHeaderStruct->wId[0] = 'W';
	zpwaveHeaderStruct->wId[1] = 'A';
	zpwaveHeaderStruct->wId[2] = 'V';
	zpwaveHeaderStruct->wId[3] = 'E';

	zpwaveHeaderStruct->fId[0] = 'f';
	zpwaveHeaderStruct->fId[1] = 'm';
	zpwaveHeaderStruct->fId[2] = 't';
	zpwaveHeaderStruct->fId[3] = ' ';

	zpwaveHeaderStruct->fLen = 16;
	zpwaveHeaderStruct->nFormatTag = 1;
	zpwaveHeaderStruct->nChannels = 1;
	zpwaveHeaderStruct->nSamplesPerSec = 8000;
	zpwaveHeaderStruct->nAvgBytesPerSec = 16000;
	zpwaveHeaderStruct->nBlockAlign = 2;
	zpwaveHeaderStruct->nBitsPerSample = 16;

	zpwaveHeaderStruct->dId[0] = 'd';
	zpwaveHeaderStruct->dId[1] = 'a';
	zpwaveHeaderStruct->dId[2] = 't';
	zpwaveHeaderStruct->dId[3] = 'a';

	zpwaveHeaderStruct->dLen = (yIntFileSize <= 0) ? 0 : yIntFileSize - 46;

	fwrite (zpwaveHeaderStruct->rId, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->rLen, 4, 1, infile);
	fwrite (zpwaveHeaderStruct->wId, 4, 1, infile);
	fwrite (zpwaveHeaderStruct->fId, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->fLen, 4, 1, infile);

	fwrite (&zpwaveHeaderStruct->nFormatTag, 2, 1, infile);
	fwrite (&zpwaveHeaderStruct->nChannels, 2, 1, infile);
	fwrite (&zpwaveHeaderStruct->nSamplesPerSec, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->nAvgBytesPerSec, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->nBlockAlign, 2, 1, infile);
	fwrite (&zpwaveHeaderStruct->nBitsPerSample, 2, 1, infile);
	fwrite (zpwaveHeaderStruct->dId, 4, 1, infile);
	fwrite (&zpwaveHeaderStruct->dLen, 4, 1, infile);

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "rLen ", zpwaveHeaderStruct->rLen);
	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "dLen ", zpwaveHeaderStruct->dLen);

	return (0);

} /*END: int writePCMWavHeaderToFile */
#endif // END: VOICE_BIOMETRICS

gint
arc_msg_to_buf_withrtp (mblk_t * mp, char *buffer, gint len)
{

char           *buf = buffer;
int             hdr_len = 0;
rtp_header_t   *rtp = NULL;

	rtp = (rtp_header_t *) mp->b_rptr;

//  memcpy (buffer, m->b_rptr, mlen);

gint            rlen = len;
mblk_t         *m, *mprev;
gint            mlen;

	m = mp->b_cont;
	mprev = mp;
	while (m != NULL)
	{
		mlen = m->b_wptr - m->b_rptr;

		memcpy (buffer, m->b_wptr, rlen - mlen);
		hdr_len = rlen - mlen;

		buffer = buffer + (rlen - mlen);

		if (mlen <= rlen)
		{
mblk_t         *consumed = m;

			memcpy (buffer, m->b_rptr, mlen);
			/* go to next mblk_t */
			mprev->b_cont = m->b_cont;
			m = m->b_cont;
			consumed->b_cont = NULL;
			freeb (consumed);
			buffer += mlen;
			rlen -= mlen;
		}
		else
		{						/*if mlen>rlen */
			memcpy (buffer, m->b_rptr, rlen);
			m->b_rptr += rlen;
			return len;
		}
	}
	buffer = buf;

	return len - rlen + hdr_len;

}								/*END: gint arc_msg_to_buf */

int
getBeepTime (char *FileName, int zCall)
{
	char            mod[] = "getBeepTime";
	struct stat     file;
	float           fileSize = 0;
	float           beepSleepTime = 0;
	int             rc;

	if (!stat (FileName, &file))
	{
		fileSize = file.st_size;
		switch (gCall[zCall].codecType)
		{
		case 18:
			beepSleepTime = fileSize * (gBeepSleepDuration / 100);
			break;
		case 8:
		case 96:
        case 101:
        case 120:
        case 127:
		case 3:
		case 0:
		default:
			beepSleepTime =
				((20.0 / 160.0) * fileSize) * (gBeepSleepDuration / 100);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "codecType=%d, beep file size=%f, sleepTime=%f.",
					   gCall[zCall].codecType, fileSize, beepSleepTime);

			break;
		}

		rc = (int) beepSleepTime;

	}

	return rc;
}

int
setLastRecordedRtpTs (char *buffer, int zCall)
{
	char            mod[] = "setLastRecordedRtpTs";
	const unsigned char *l_ptr;

	if (buffer[0] != '\0')
	{
		l_ptr = (const unsigned char *) buffer;
		gCall[zCall].lastRecordedRtpTs =
			(l_ptr[4] << 24) | (l_ptr[5] << 16) | (l_ptr[6] << 8) | l_ptr[7];

	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Last buffer is empty lastRecordedRtpTs = %ld.",
				   gCall[zCall].lastRecordedRtpTs);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "lastRecordedRtpTs = %ld", gCall[zCall].lastRecordedRtpTs);

	return 0;
}

int
modifyTS (char *buffer, int zCall, int zBufferLen)
{
	char            mod[] = "modifyTS";

	unsigned long   timestamp =
		gCall[zCall].lastRecordedRtpTs + gCall[zCall].outTsIncrement;

	gCall[zCall].lastRecordedRtpTs = timestamp;

	{
	const unsigned char *l_ptr;

		l_ptr = (const unsigned char *) buffer;
	unsigned long   lastRecordedRtpTs =
		(l_ptr[4] << 24) | (l_ptr[5] << 16) | (l_ptr[6] << 8) | l_ptr[7];

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Timsestamp before modification locallastRecordedRtpTs=%ld.",
				   lastRecordedRtpTs);

	}

	buffer[4] = (BYTE) (timestamp >> 24);
	buffer[5] = (BYTE) (timestamp >> 16);
	buffer[6] = (BYTE) (timestamp >> 8);
	buffer[7] = (BYTE) (timestamp);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "lastRecordedRtpTs = %ld, out_user_ts=%d, timestamp=%ld",
			   gCall[zCall].lastRecordedRtpTs, gCall[zCall].outTsIncrement,
			   timestamp);

	{
	const unsigned char *l_ptr;

		l_ptr = (const unsigned char *) buffer;
	unsigned long   lastRecordedRtpTs =
		(l_ptr[4] << 24) | (l_ptr[5] << 16) | (l_ptr[6] << 8) | l_ptr[7];

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Timsestamp after modification locallastRecordedRtpTs=%ld",
				   lastRecordedRtpTs);

	}

	return 0;
}

#ifdef ACU_RECORD_TRAILSILENCE

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static void
initPort (int zPort, int zType)
{
	static char     mod[] = "initPort";

	switch (zType)
	{
	case RECORD_CHAN:
		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling setChannel.");

		gACard.chan[zPort].recordingCancelled = 0;
		gACard.chan[zPort].aculabRecordThreadId = -1;

		(void) setChannel (zPort, -1, -1, RECORD_CHAN);
		break;

	default:
		break;
	}
}								// END: initPort()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
get_RX_RTP_Status (int zPort, int zHaltStatus, A_DATA * zaData)
{
	int             rc;
	SM_VMPRX_EVENT_PARMS event_p;
	SM_VMPRX_STATUS_PARMS status_p;
	tSMVMPrxId      vmpx;

	event_p = zaData->chan[zPort].rx_event;
	status_p = zaData->chan[zPort].rx_status;
	vmpx = zaData->chan[zPort].rx_create.vmprx;

	INIT_ACU_SM_STRUCT (&status_p);
	vmpx = zaData->chan[zPort].rx_create.vmprx;
	status_p.vmprx = vmpx;

	if ((rc = sm_vmprx_status (&status_p)) != 0)
	{
		printf ("%s sm_vmprx_status(%d, vmprx=%d) failed.  rc=%d. ",
				__FUNCTION__, "Unable to continue.\n", zPort, (int) vmpx, rc);
		return (-1);
	}

	printf ("sm_vmprx_status(%d, vmprx=%d, status=%d);\n",
			zPort, (int) vmpx, status_p.status);

	return (status_p.status);
#if 0
	switch (zHaltStatus)
	{
	case A_RUNNING:
		if (status_p.status == kSMVMPrxStatusRunning)
		{
			return (1);
		}
		break;
	case A_STOPPED:
		if (status_p.status == kSMVMPrxStatusStopped)
		{
			return (1);
		}
		break;
	case A_DETECTTONE:
		if (status_p.status == kSMVMPrxStatusDetectTone)
		{
			return (1);
		}
		break;
	case A_ENDTONE:
		if (status_p.status == kSMVMPrxStatusEndTone)
		{
			return (1);
		}
		break;
	case A_GOTPORTS:
		if (status_p.status == kSMVMPrxStatusGotPorts)
		{
			return (1);
		}
		break;
	case A_NEWSOURCE:
		if (status_p.status == kSMVMPrxStatusNewSSRC)
		{
			return (1);
		}
		break;
	case A_UNHANDLEDPAYLOAD:
		if (status_p.status == kSMVMPrxStatusUnhandledPayload)
		{
			return (1);
		}
		break;
	}
#endif
	return (0);

}								// END: get_RTP_Status()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
getRtpEventStatus (int zPort, int zHaltStatus, A_DATA * zaData)
{
	// static char              mod[] = "getRtpEventStatus";
	int             rc;
	ACU_INT         i;

	//int                       iTXStarted;
	int             iRX;
	ACU_SDK_WAIT_OBJECT *hEvents[2];
	ACU_OS_BOOL     SignalledEvent[2];

	ACU_SDK_WAIT_OBJECT *p_wo;
	SM_VMPRX_EVENT_PARMS event_p;

	p_wo = zaData->chan[zPort].vmprx_wo;
	event_p = zaData->chan[zPort].rx_event;

	for (i = 0; i < 2; i++)
	{
		hEvents[i] = NULL;
		SignalledEvent[i] = -1;
	}

	hEvents[0] = p_wo;

	iRX = 0;

	while (!iRX)
	{
		rc = acu_os_wait_for_any_wait_object (1, hEvents, SignalledEvent,
											  1000);

		printf
			("%d = acu_os_wait_for_any_wait_object();  SignalledEvent[0]=%d "
			 "SignalledEvent[1] = %d\n", rc, SignalledEvent[0],
			 SignalledEvent[1]);

		if (rc == ERR_ACU_OS_TIMED_OUT)
		{
			printf ("Received ERR_ACU_OS_TIMED_OUT(%d) for %d\n",
					rc, (int) event_p.vmprx);

			continue;
		}

		if (SignalledEvent[0] == ACU_OS_TRUE)
		{
			if ((rc = get_RX_RTP_Status (zPort, zHaltStatus, zaData))
				== zHaltStatus)
			{
				return (0);
			}
			else
			{
				if (rc < 0)
				{
					return (-1);
				}
			}
		}
		// DJB - TODO: Need a timeout here.
	}
	return (0);

}								// END: getRtpEventStatus()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
reserveRTP (int zPort, A_DATA * zaData)
{
	char            mod[] = "reserveRTP";
	int             rc;
	ACU_INT         portsPerModule;
	ACU_INT         moduleNumber = 0;

	// disperse number of ports across modules
	portsPerModule = gMaxAcuPorts / zaData->speechInfo.module_count;

	if ((zPort > 0) && (zPort % portsPerModule == 0))
	{
		moduleNumber++;
	}

	zaData->chan[zPort].rx_create.module =
		zaData->pModule[moduleNumber].module_id;

	// receive
	if ((rc = sm_vmprx_create (&zaData->chan[zPort].rx_create)) != 0)
	{
		zaData->chan[zPort].rx_create.vmprx = (tSMVMPrxId) - 1;

		dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "sm_vmprx_create(%d, module_id=%d) failed.  rc=%d. "
				   "Unable to continue.", zPort,
				   (int) zaData->chan[zPort].rx_create.module, rc);

		return (-1);
	}

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "sm_vmprx_create(%d, module_id=%d) succeeded.  rc=%d, "
			   "zaData->chan[%d].rx_create.vmprx=%d",
			   zPort, (int) zaData->chan[zPort].rx_create.module, rc,
			   zPort, (int) zaData->chan[zPort].rx_create.vmprx);

	INIT_ACU_SM_STRUCT (&zaData->chan[zPort].rx_event);
	zaData->chan[zPort].rx_event.vmprx = zaData->chan[zPort].rx_create.vmprx;

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling sm_vmprx_get_event.", zPort);
	if ((rc = sm_vmprx_get_event (&zaData->chan[zPort].rx_event)) != 0)
	{
//      printf("sm_vmprx_get_event(%d, vmprx=%d) failed.  rc=%d. "
//              "Unable to continue.\n",
//              zPort, (int)zaData->chan[zPort].rx_create.vmprx, rc);
		return (-1);
	}
//  printf("sm_vmprx_get_event(%d; vmprx=%d) succeeded.\n",
//          zPort, (int)zaData->chan[zPort].rx_event.vmprx);

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling acu_os_create_wait_object_from_prosody");

	zaData->chan[zPort].vmprx_wo =
		acu_os_create_wait_object_from_prosody (zaData->chan[zPort].rx_event.
												event);

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling getRtpEventStatus");

	if ((rc = getRtpEventStatus (zPort, kSMVMPrxStatusGotPorts, zaData)) != 0)
	{
		dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "aGetRtpEventStatus() returned failure.  rc=%d", rc);
		return (-1);			// DJB: what to do
	}

	INIT_ACU_SM_STRUCT (&zaData->chan[zPort].rx_port);
	zaData->chan[zPort].rx_port.vmprx = zaData->chan[zPort].rx_create.vmprx;
	zaData->chan[zPort].rx_port.nowait = 1;

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "zPort=%d, sm_vmprx_get_ports", zPort);

	if ((rc = sm_vmprx_get_ports (&zaData->chan[zPort].rx_port)) != 0)
	{
		dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "sm_vmprx_get_ports(%d, vmprx=%d) failed.  rc=%d. Unable to continue.",
				   zPort, (int) zaData->chan[zPort].rx_port.vmprx, rc);
		return (-1);
	}
	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "sm_vmprx_get_ports(%d; vmprx=%d) succeeded. rtpPort=%d",
			   zPort,
			   (int) zaData->chan[zPort].rx_port.vmprx,
			   (int) zaData->chan[zPort].rx_port.RTP_port);

	return (0);
}								// END: reserveRTP()

static int
openCard (int zPort, A_DATA * zaData)
{
	ACU_ERR         rc;

	if ((rc = acu_open_card (&zaData->card)) < 0)	// serial_no filled in
	{
		printf ("acu_open_card(%s) failed.  rc=%d\n",
				zaData->card.serial_no, rc);
		return (-1);
	}

	INIT_ACU_STRUCT (&zaData->cardInfo);
	zaData->cardInfo.card_id = zaData->card.card_id;

	if ((rc = acu_get_card_info (&zaData->cardInfo)) < 0)
	{
		printf ("acu_get_card_info(%d) failed.  rc=%d\n",
				zaData->cardInfo.card_id, rc);
		return (-1);
	}

	if (strstr (zaData->cardInfo.card_desc, "Prosody S"))
	{
		sprintf (zaData->cardInfo.ip_address, "%s", "10.0.10.60");
		//sprintf(zaData->cardInfo.ip_address, "%s", "127.0.0.1");
		printf ("Prosody S: Setting IP address system to (%s)\n",
				zaData->cardInfo.ip_address);
	}
	printf ("Successfully opened aculab card (%s)\n"
			"    card_name=%s,\n"
			"    card_description=%s,\n"
			"    card_id=%d,\n"
			"    hw_version=(%s),\n"
			"    ip_address=(%s),\n"
			"    card_key=(%s),\n",
			zaData->card.serial_no,
			zaData->cardInfo.card_name,
			zaData->cardInfo.card_desc,
			zaData->cardInfo.card_id,
			zaData->cardInfo.hw_version,
			zaData->cardInfo.ip_address, zaData->cardInfo.card_key);

	printf ("    resources_available=");
	if (zaData->cardInfo.resources_available & ACU_RESOURCE_SPEECH)
	{
		printf ("SPEECH");
	}
	if (zaData->cardInfo.resources_available & ACU_RESOURCE_SWITCH)
	{
		printf (", SWITCH");
	}
	if (zaData->cardInfo.resources_available & ACU_RESOURCE_CALL)
	{
		printf (", CALL");
	}
	// printf("\n");

	return (0);

}								// END: openCard()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
openSwitch (int zPort, A_DATA * zaData)
{
	ACU_ERR         rc;

	if (!(zaData->cardInfo.resources_available & ACU_RESOURCE_SWITCH))
	{
		printf ("There are no Switch resources available for card (%s:%d).  "
				"Unable to process.\n",
				zaData->card.serial_no, zaData->card.card_id);
		return (-1);
	}

	/* Open Switch Driver Interface */
	INIT_ACU_STRUCT (&zaData->aswitch);
	zaData->aswitch.card_id = zaData->card.card_id;

	if ((rc = acu_open_switch (&zaData->aswitch)) < 0)
	{
		printf ("sm_open_switch(id=%d) failed.  rc=%d\n",
				zaData->aswitch.card_id, rc);
		return (-1);
	}
	printf ("sm_open_switch(id=%d) succeeded.\n", zaData->aswitch.card_id);

	return (0);
}								// END: openSwitch()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
openSpeech (int zPort, A_DATA * zaData)
{
	ACU_ERR         rc;
	ACU_UINT        i;
	SM_OPEN_MODULE_PARMS openModule;

	if (!(zaData->cardInfo.resources_available & ACU_RESOURCE_SPEECH))
	{
		printf ("There are no Speech resources available for card (%s:%d).  "
				"Unable to process.\n",
				zaData->card.serial_no, zaData->card.card_id);
		return (-1);
	}

	// open speech resource
	INIT_ACU_STRUCT (&zaData->speech);
	zaData->speech.card_id = zaData->card.card_id;

	if ((rc = acu_open_prosody (&zaData->speech)) < 0)	// serial_no filled in
	{
		printf ("acu_open_prosody(id=%d) failed.  rc=%d\n",
				zaData->speech.card_id, rc);
		return (-1);
	}

	INIT_ACU_SM_STRUCT (&zaData->speechInfo);
	zaData->speechInfo.card = zaData->speech.card_id;
	if ((rc = sm_get_card_info (&zaData->speechInfo)) < 0)
	{
		printf ("sm_get_card_info(id=%d) failed.  rc=%d\n",
				zaData->speech.card_id, rc);
		return (-1);
	}
	printf ("sm_get_card_info(id=%d) succeeded; %d modules on card.\n",
			zaData->speech.card_id, zaData->speechInfo.module_count);
	fflush (stdout);

	zaData->pModule =
		(MODULE_DATA *) calloc (zaData->speechInfo.module_count,
								sizeof (MODULE_DATA));

	for (i = 0; i < zaData->speechInfo.module_count; i++)
	{
		INIT_ACU_SM_STRUCT (&openModule);
		openModule.module_ix = i;
		openModule.card_id = zaData->speech.card_id;
		if ((rc = sm_open_module (&openModule)) < 0)
		{
			printf
				("sm_open_module() failed to open for (%d;id=%d).  rc=%d\n",
				 i, zaData->speech.card_id, rc);
			return (-1);
		}
		printf ("sm_open_module() succeeded for (%d;id=%d).\n",
				i, (int) openModule.module_id);

		zaData->pModule[i].module = i;
		zaData->pModule[i].module_id = openModule.module_id;
	}

	return (0);
}								// END: openSpeech()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
getCards (ACU_SNAPSHOT_PARMS * zSnapShotParm)
{
ACU_ERR         result;
ACU_UINT        c;

	result = acu_get_system_snapshot (zSnapShotParm);
	printf ("%d = acu_get_system_snapshot(); count = %d\n",
			result, zSnapShotParm->count);
	fflush (stdout);

	if (result == 0)
	{
		for (c = 0; c < zSnapShotParm->count; c++)
		{
			printf ("Found card %s\n", zSnapShotParm->serial_no[c]);
			fflush (stdout);
		}
		return (0);
	}

	return (-1);

}								// END: getCards()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
aInitialize (int zPort, A_DATA * zaData)
{
	char            mod[] = "aInitialize";
	ACU_SNAPSHOT_PARMS snapshot_parms;
	int             rc;
	int             i;

	INIT_ACU_STRUCT (&zaData->card);
	INIT_ACU_STRUCT (&snapshot_parms);

	TiNGtrace = 3;

	for (i = 0; i < gMaxAcuPorts; i++)
	{
		zaData->chan[i].vmprx_wo = NULL;
		INIT_ACU_SM_STRUCT (&zaData->chan[i].rx_create);
		zaData->chan[i].rx_create.vmprx = (tSMVMPrxId) - 1;
	}

	zaData->speech.card_id = -1;
	zaData->card.card_id = -1;

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling getCards.");
	if ((rc = getCards (&snapshot_parms)) != 0)
	{
		return (-1);
	}

	// open the card
	sprintf (zaData->card.serial_no, "%s", snapshot_parms.serial_no[0]);

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling openCard.");

	if ((rc = openCard (zPort, zaData)) < 0)
	{
		return (-1);
	}

	if (zaData->cardInfo.resources_available & ACU_RESOURCE_SPEECH)
	{
		// open speech resource
		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling openSpeech.");
		if ((rc = openSpeech (zPort, zaData)) < 0)
		{
			return (-1);
		}
	}

	if (zaData->cardInfo.resources_available & ACU_RESOURCE_SWITCH)
	{
		// open switch resource
		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling openSwitch.");
		if ((rc = openSwitch (zPort, zaData)) < 0)
		{
			return (-1);
		}
	}

#if 1
	// reserve rtp
	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling reserveRTP.");

	for (i = 0; i < gMaxAcuPorts; i++)
	{
		if ((rc = reserveRTP (i, zaData)) < 0)
		{
			dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
					   TEL_RESOURCE_NOT_AVAILABLE, ERR,
					   "Failed to reserve %dth Resource.", i);
			return (i);
		}
	}
#endif
	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Returning from aInitialize().");

	return (gMaxAcuPorts);
}								// END: aInitialize()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
doGrunt (int zPort, A_DATA * zaData)
{
	char            mod[] = "doGrunt";
	int             rc;
	int             i;
	int             done;
	ACU_SDK_WAIT_OBJECT *hEvents[2];
	ACU_OS_BOOL     SignalledEvent[2];
	char            tmpString[64];

	INIT_ACU_SM_STRUCT (&zaData->chan[gCall[zPort].zAcuPort].channelSetEvent);
	zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.channel =
		zaData->chan[gCall[zPort].zAcuPort].channelParms.channel;
	zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.event_type =
		kSMEventTypeRecog;
	zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.issue_events =
		kSMChannelSpecificEvent;

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling smd_ev_create().");

	rc = smd_ev_create (&zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
						event,
						zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
						channel,
						zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
						event_type,
						zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
						issue_events);
	if (rc < 0)
	{
		dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "smd_ev_create(channel=%d) failed.  rc=%d",
				   (int) zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
				   channel, rc);
		fflush (stdout);
		return (-1);
	}
	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "smd_ev_create(channedl=%d) succeeded.",
			   (int) zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
			   channel);
	fflush (stdout);

	if ((rc =
		 sm_channel_set_event (&zaData->chan[gCall[zPort].zAcuPort].
							   channelSetEvent)) < 0)
	{
		dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "sm_channel_set_event(channel=%d) failed.  rc=%d",
				   (int) zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
				   channel, rc);
		fflush (stdout);

		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling smd_ev_free(%d)",
				   &zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
				   event);

		if ((rc =
			 smd_ev_free (zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
						  event)) != 0)
		{
			dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
					   TEL_RESOURCE_NOT_AVAILABLE, ERR,
					   "Failed to free event for channnel (%d).  rc=%d",
					   &zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
					   event, rc);
		}

		return (-1);
	}
	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "sm_channel_set_event(channedl=%d) succeeded.",
			   (int) zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
			   channel);
	fflush (stdout);

	// Turn on Call Progress Tone Detection
	INIT_ACU_SM_STRUCT (&zaData->chan[gCall[zPort].zAcuPort].listenFor);
	zaData->chan[gCall[zPort].zAcuPort].listenFor.channel =
		zaData->chan[gCall[zPort].zAcuPort].channelParms.channel;
	zaData->chan[gCall[zPort].zAcuPort].listenFor.tone_detection_mode =
		kSMToneDetectionNone;
//  zaData->chan[gCall[zPort].zAcuPort].listenFor.enable_cptone_recognition = 1;
	zaData->chan[gCall[zPort].zAcuPort].listenFor.enable_cptone_recognition =
		0;
	zaData->chan[gCall[zPort].zAcuPort].listenFor.enable_grunt_detection = 1;
	zaData->chan[gCall[zPort].zAcuPort].listenFor.grunt_latency =
		gCall[zPort].msgRecord.trail_silence * 1000;
//  zaData->chan[gCall[zPort].zAcuPort].listenFor.grunt_threshold = 30;
//  zaData->chan[gCall[zPort].zAcuPort].listenFor.min_noise_level = 30;

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling sm_listen_for zCall=%d", zPort);

	if ((rc =
		 sm_listen_for (&zaData->chan[gCall[zPort].zAcuPort].listenFor)) < 0)
	{
		dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "sm_listen_for(%d) failed.  rc=%d",
				   (int) zaData->chan[gCall[zPort].zAcuPort].listenFor.
				   channel, rc);
		fflush (stdout);

		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling smd_ev_free(%d)",
				   &zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
				   event);

		if ((rc =
			 smd_ev_free (zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
						  event)) != 0)
		{
			dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
					   TEL_RESOURCE_NOT_AVAILABLE, ERR,
					   "Failed to free event for channnel (%d).  rc=%d",
					   &zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
					   event, rc);
		}
		return (-1);
	}
	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "sm_listen_for(%d) succeeded. enable_grunt_detection=%d; grunt_latency=%d",
			   (int) zaData->chan[gCall[zPort].zAcuPort].listenFor.channel,
			   zaData->chan[gCall[zPort].zAcuPort].listenFor.
			   enable_grunt_detection,
			   zaData->chan[gCall[zPort].zAcuPort].listenFor.grunt_latency);
	fflush (stdout);

	// Setup Waitable Objects
	for (i = 0; i < 3; i++)
	{
		hEvents[i] = NULL;
		SignalledEvent[i] = 0;
	}

	hEvents[0] =
		acu_os_create_wait_object_from_prosody (zaData->
												chan[gCall[zPort].zAcuPort].
												channelSetEvent.event);

	done = 0;
	memset ((char *) tmpString, '\0', sizeof (tmpString));
	while (!done && gCall[zPort].recordStarted == 1)
	{
		if (gCall[zPort].stop_aculabRecordThread == 1)
		{
			dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Recording is done exiting doGrunt");
			break;
		}
		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling acu_os_wait_for_any_wait_object");

		rc = acu_os_wait_for_any_wait_object (1, hEvents, SignalledEvent,
											  500);

		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "rc = %d, ERR_ACU_OS_TIMED_OUT=%d", rc,
				   ERR_ACU_OS_TIMED_OUT);

		if (rc == ERR_ACU_OS_TIMED_OUT)
		{
			continue;
		}

		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "SignalledEvent[0] = %d;  ACU_OS_TRUE=%d",
				   SignalledEvent[0], ACU_OS_TRUE);

		// RECOGNITION EVENT 
		if (SignalledEvent[0] == ACU_OS_TRUE)
		{
			INIT_ACU_SM_STRUCT (&zaData->chan[gCall[zPort].zAcuPort].
								recogParms);

			zaData->chan[gCall[zPort].zAcuPort].recogParms.channel =
				zaData->chan[gCall[zPort].zAcuPort].TiNG_ChannelId;

			if ((rc =
				 sm_get_recognised (&zaData->chan[gCall[zPort].zAcuPort].
									recogParms)) < 0)
			{
				dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
						   TEL_RESOURCE_NOT_AVAILABLE, ERR,
						   "sm_get_recognised(%d) failed.  rc=%d",
						   (int) zaData->chan[gCall[zPort].zAcuPort].
						   TiNG_ChannelId, rc);
				dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Calling smd_ev_free(%d)",
						   &zaData->chan[gCall[zPort].zAcuPort].
						   channelSetEvent.event);
				if ((rc =
					 smd_ev_free (zaData->chan[gCall[zPort].zAcuPort].
								  channelSetEvent.event)) != 0)
				{
					dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
							   TEL_RESOURCE_NOT_AVAILABLE, ERR, mod,
							   REPORT_NORMAL, TONES_LOG_BASE, 0,
							   "Failed to free event for channnel (%d).  rc=%d",
							   &zaData->chan[gCall[zPort].zAcuPort].
							   channelSetEvent.event, rc);
				}
				return (-1);
			}
			dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "sm_get_recognised(%d) succeeded.  recogParms.type=%d.",
					   (int) zaData->chan[gCall[zPort].zAcuPort].
					   TiNG_ChannelId,
					   zaData->chan[gCall[zPort].zAcuPort].recogParms.type);

			if (zaData->chan[gCall[zPort].zAcuPort].recogParms.type ==
				kSMRecognisedCPTone)
			{
				switch (zaData->chan[gCall[zPort].zAcuPort].recogParms.param0)
				{
				case 0:
					sprintf (tmpString, "%s", "Unknown");
					break;
				case 1:
					sprintf (tmpString, "%s", "Unobtainable");
					break;
				case 2:
					sprintf (tmpString, "%s", "Busy");
					break;
				case 3:
					sprintf (tmpString, "%s", "Engaged");
					break;
				case 4:
					sprintf (tmpString, "%s", "Ring");
					break;
				case 5:
					sprintf (tmpString, "%s", "SIT");
					break;
				case 6:
					sprintf (tmpString, "%s", "CNG");
					break;
				case 7:
					sprintf (tmpString, "%s", "CED");
					break;
				case 8:
					sprintf (tmpString, "%s", "Dialtone");
					break;
				case 9:
					sprintf (tmpString, "%s", "Reorder");
					break;
				case 10:
					sprintf (tmpString, "%s", "Unknown");
					break;
				case 11:
					sprintf (tmpString, "%s", "Intercept");
					break;
				default:
					sprintf (tmpString, "%s", "Default");
					break;
				}

				dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "CALL PROGESS TONE [%d] = %s",
						   zaData->chan[gCall[zPort].zAcuPort].recogParms.
						   param0, tmpString);
			}
			else if (zaData->chan[gCall[zPort].zAcuPort].recogParms.type ==
					 kSMRecognisedGruntStart)
			{
				dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "GRUNT START received: kSMRecognisedGruntStart = %d",
						   kSMRecognisedGruntStart);
			}
			else if (zaData->chan[gCall[zPort].zAcuPort].recogParms.type ==
					 kSMRecognisedGruntEnd)
			{
				dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "GRUNT END   received: kSMRecognisedGruntEnd = %d TRAIL SILENCE",
						   kSMRecognisedGruntEnd);
				gCall[zPort].trailSilence = 1;
				break;
			}
			else if (zaData->chan[gCall[zPort].zAcuPort].recogParms.type ==
					 kSMRecognisedCatSig)
			{
				if (zaData->chan[gCall[zPort].zAcuPort].recogParms.param1 ==
					0)
				{
					dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "ANSWERING MACHINE ON ");
					done = 1;
				}
				else if (zaData->chan[gCall[zPort].zAcuPort].recogParms.
						 param1 == 1)
				{
					dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "LIVE SPEAKER ON");
					done = 1;
				}
			}
			else if (zaData->chan[gCall[zPort].zAcuPort].recogParms.type ==
					 kSMRecognisedNothing)
			{
				dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Got RecognisedNothing event");
			}
		}
	}

	dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling smd_ev_free(%d)",
			   &zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.event);

	if ((rc =
		 smd_ev_free (zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.
					  event)) != 0)
	{
		dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "Failed to free event for channnel (%d).  rc=%d",
				   &zaData->chan[gCall[zPort].zAcuPort].channelSetEvent.event,
				   rc);
	}

	return (0);

}								// doGrunt

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
terminateRTP (int zPort, A_DATA * zaData)
{
	int             rc;

	// receive
	if (zaData->chan[zPort].rx_create.vmprx <= 0)
	{
		printf ("Invalid VMPrx id (%d).  Unable to perform sm_vmprx_stop() "
				"on channel %d\n",
				(int) zaData->chan[zPort].rx_create.vmprx, zPort);
		return (-1);
	}

	INIT_ACU_SM_STRUCT (&zaData->chan[zPort].rx_stop);
	zaData->chan[zPort].rx_stop.vmprx = zaData->chan[zPort].rx_create.vmprx;

	if ((rc = sm_vmprx_stop (&zaData->chan[zPort].rx_stop)) != 0)
	{
		printf ("sm_vmprx_stop(%d, vmprx=%d) failed.  rc=%d. "
				"Unable to continue.\n",
				zPort, (int) zaData->chan[zPort].rx_stop.vmprx, rc);
		return (-1);
	}
	printf ("sm_vmprx_stop(%d, vmprx=%d) succeeded.\n",
			zPort, (int) zaData->chan[zPort].rx_create.vmprx);

//  if ((rc = getRtpEventStatus(zPort, A_STOPPED, zaData)) != 0)
	if ((rc = getRtpEventStatus (zPort, kSMVMPrxStatusStopped, zaData)) != 0)
	{
		return (-1);			// DJB: what to do
	}

	return (0);
}								// END: terminateRTP

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
disconnectRTP_from_TiNG (int zPort, A_DATA * zaData)
{
	int             rc;

//    INIT_ACU_SM_STRUCT( &zaData->chan[zPort].rx_datafeed );
//  zaData->chan[zPort].channel_datafeed_connect.data_source = kSMNullDatafeedId;
//    if ((rc = sm_vmprx_datafeed_connect( &zaData->chan[zPort].rx_datafeed )) < 0 )
//  {
//      printf("sm_vmprx_datafeed_connect(%d, null) failed.  rc=%d\n",
//              (int)zaData->chan[zPort].rx_datafeed.vmprx, rc);
//      return(-1);
//  }
//  printf("sm_vmprx_datafeed_connect(%d, null) succeeded.  Disconnected\n",
//              (int)zaData->chan[zPort].rx_datafeed.vmprx);

	// INIT_ACU_SM_STRUCT( &zaData->chan[zPort].channel_datafeed_connect );
	zaData->chan[zPort].channel_datafeed_connect.data_source =
		kSMNullDatafeedId;

	if ((rc =
		 sm_channel_datafeed_connect (&zaData->chan[zPort].
									  channel_datafeed_connect)) < 0)
	{
		printf ("sm_channel_datafeed_connect(%d, %d, null) failed.  rc=%d\n",
				(int) zaData->chan[zPort].channel_datafeed_connect.
				data_source,
				(int) zaData->chan[zPort].channel_datafeed_connect.channel,
				rc);
		return (-1);
	}
	printf
		("sm_channel_datafeed_connect(%d, %d, null) succeeded.  Disconnected\n",
		 (int) zaData->chan[zPort].channel_datafeed_connect.data_source,
		 (int) zaData->chan[zPort].channel_datafeed_connect.channel);

	return (0);
}								// END: disconnectRTP_from_TiNG()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
aShutdown (int zPort, A_DATA * zaData)
{
	int             rc;
	int             i;
	ACU_CLOSE_CARD_PARMS cCard;
	ACU_CLOSE_PROSODY_PARMS cProsody;
	SM_CLOSE_MODULE_PARMS cCloseModule;
	ACU_CLOSE_SWITCH_PARMS cCloseSwitch;
	char            serial_no[64];
	ACU_UINT        card_id;

	sprintf (serial_no, "%s", zaData->card.serial_no);
	card_id = zaData->card.card_id;

// Close switch
	if (zaData->cardInfo.resources_available & ACU_RESOURCE_SWITCH)
	{
		INIT_ACU_STRUCT (&cCloseSwitch);
		cCloseSwitch.card_id = zaData->card.card_id;
		if ((rc = acu_close_switch (&cCloseSwitch)) < 0)
		{
			printf ("acu_close_switch(%d) failed.  rc=%d\n",
					cCloseSwitch.card_id, rc);
			return (-1);
		}
		printf ("acu_close_switch(%s:%d) successfully closed.\n", serial_no,
				card_id);
		fflush (stdout);
	}

// Close Speech
	if (zaData->cardInfo.resources_available & ACU_RESOURCE_SPEECH)
	{
		for (i = 0; i < zaData->speechInfo.module_count; i++)
		{
			INIT_ACU_SM_STRUCT (&cCloseModule);
			cCloseModule.module_id = zaData->pModule[i].module_id;
			if ((rc = sm_close_module (&cCloseModule)) < 0)
			{
				printf
					("sm_close_module() failed to close for (%d;id=%d).  rc=%d\n",
					 i, (int) zaData->pModule[i].module_id, rc);
				return (-1);
			}
			printf ("sm_close_module() succeeded for (%d;id=%d).\n",
					i, (int) zaData->pModule[i].module_id);
		}

		if (zaData->pModule)
		{
			free (zaData->pModule);
		}
	}

// Close the card
	INIT_ACU_STRUCT (&cCard);
	cCard.card_id = zaData->card.card_id;
	if ((rc = acu_close_card (&cCard)) < 0)
	{
		printf ("acu_close_card(%d) failed.  rc=%d\n", cCard.card_id, rc);
		return (-1);
	}
	printf ("acu_close_card(%s:%d) successfully closed.\n", serial_no,
			card_id);
	fflush (stdout);

// Close Prosody
	INIT_ACU_STRUCT (&cProsody);
	cProsody.card_id = zaData->card.card_id;
	if ((rc = acu_close_prosody (&cProsody)) < 0)
	{
		printf ("acu_close_prosody(%d) failed.  rc=%d\n",
				cProsody.card_id, rc);
		return (-1);
	}
	printf ("acu_close_prosody(%s:%d) successfully closed.\n", serial_no,
			card_id);
	fflush (stdout);

	return (0);

}								// END: aShutdown()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
configureRTP (int zPort, A_DATA * zaData, A_RTP_DETAILS * zRtpDetails)
{
	int             rc;

	INIT_ACU_SM_STRUCT (&zaData->chan[zPort].rx_codec);

	zaData->chan[zPort].rx_codec.vmprx = zaData->chan[zPort].rx_create.vmprx;
	zaData->chan[zPort].rx_codec.codec = kSMCodecTypeMulaw;
	zaData->chan[zPort].rx_codec.payload_type = 0;

	if ((rc = sm_vmprx_config_codec (&zaData->chan[zPort].rx_codec)) < 0)
	{
		printf ("sm_vmprx_config_codec(%d, vmprx=%d) failed.  rc=%d. "
				"Unable to continue.\n", zPort,
				(int) zaData->chan[zPort].rx_codec.vmprx, rc);
		return (-1);
	}
	printf ("sm_vmprx_config_codec(vmprx=%d) succeeded.\n",
			(int) zaData->chan[zPort].rx_codec.vmprx);

	return (0);
}								// END: configureRTP()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int
connectRTP_to_TiNG (int zPort, A_DATA * zaData)
{
	int             rc;

	INIT_ACU_SM_STRUCT (&zaData->chan[zPort].rx_datafeed);
	zaData->chan[zPort].rx_datafeed.vmprx =
		zaData->chan[zPort].rx_create.vmprx;
	if ((rc = sm_vmprx_get_datafeed (&zaData->chan[zPort].rx_datafeed)) < 0)
	{
		printf ("sm_vmprx_get_datafeed(%d) failed.  rc=%d\n",
				(int) zaData->chan[zPort].rx_datafeed.vmprx, rc);
		return (-1);
	}
	printf ("sm_vmprx_get_datafeed(%d) succeeded.\n",
			(int) zaData->chan[zPort].rx_datafeed.vmprx);

	INIT_ACU_SM_STRUCT (&zaData->chan[zPort].channel_datafeed_connect);
	zaData->chan[zPort].channel_datafeed_connect.data_source =
		zaData->chan[zPort].rx_datafeed.datafeed;
	zaData->chan[zPort].channel_datafeed_connect.channel =
		zaData->chan[zPort].TiNG_ChannelId;
	if ((rc =
		 sm_channel_datafeed_connect (&zaData->chan[zPort].
									  channel_datafeed_connect)) < 0)
	{
		printf ("sm_channel_datafeed_connect(%d, %d) failed.  rc=%d\n",
				(int) zaData->chan[zPort].channel_datafeed_connect.
				data_source,
				(int) zaData->chan[zPort].channel_datafeed_connect.channel,
				rc);
		return (-1);
	}
	printf ("sm_channel_datafeed_connect(%d, %d) succeeded.\n",
			(int) zaData->chan[zPort].channel_datafeed_connect.data_source,
			(int) zaData->chan[zPort].channel_datafeed_connect.channel);

	return (0);
}								// END: connectRTP_to_TiNG()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int
aDestroyResource (int zPort, int zAppPort, int zType)
{
	static char     mod[] = "aDestroyResource";
	int             rc;

	if (zPort < 0 || zPort > gMaxAcuPorts)
	{
		return -1;
	}

	if (zType == RECORD_CHAN)
	{
		if (gACard.chan[zPort].vmprx_wo)
		{
			acu_os_destroy_wait_object (gACard.chan[zPort].vmprx_wo);
		}

		if ((int) gACard.chan[zPort].rx_create.vmprx != -1)
		{
			if ((rc =
				 sm_vmprx_destroy (gACard.chan[zPort].rx_create.vmprx)) != 0)
			{
				dynVarLog (__LINE__, zPort, mod, REPORT_NORMAL,
						   TEL_RESOURCE_NOT_AVAILABLE, ERR,
						   "Port %d, sm_vmprx_destroy(%d) failed.  rc=%d. "
						   "Unable to continue.", zPort,
						   (int) gACard.chan[zPort].rx_create.vmprx, rc);
				return (-1);
			}
			dynVarLog (__LINE__, zAppPort, mod, REPORT_VERBOSE, TEL_BASE,
					   INFO, "Port %d, sm_vmprx_destroy(%d) succeeded.  ",
					   zPort, (int) gACard.chan[zPort].rx_create.vmprx);
		}
		return (0);
	}

	return (0);
}

void           *
aculabRecordThread (void *z)
{
	char            mod[] = "aculabRecordThread";
	int             yRc = 0;
	int             zCall = -1;
	int             acuPort = -1;
	struct MsgToDM *ptrMsgToDM;

	ptrMsgToDM = (struct MsgToDM *) z;

	zCall = ptrMsgToDM->appCallNum;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling doGrunt for zCall=%d", zCall);
	yRc = doGrunt (zCall, &gACard);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling disconnectRTP_from_TiNG");

	yRc = disconnectRTP_from_TiNG (gCall[zCall].zAcuPort, &gACard);

	// Release the allocated channel 
	if ((yRc =
		 sm_channel_release (gACard.chan[gCall[zCall].zAcuPort].channelParms.
							 channel)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "sm_channel_release() failed.  type=%d  TiNG_ChannelId=%d",
				   gACard.chan[gCall[zCall].zAcuPort].channelParms.type,
				   (int) gACard.chan[gCall[zCall].zAcuPort].channelParms.
				   module);
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "sm_channel_released() succeeded. TiNG_ChannelId=%d.",
				   (int) gACard.chan[gCall[zCall].zAcuPort].TiNG_ChannelId);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling terminateRTP zCall=%d", zCall);
	if ((yRc = terminateRTP (gCall[zCall].zAcuPort, &gACard)) < 0)
	{
		//return(-1);
	}

	gACard.chan[gCall[zCall].zAcuPort].aculabRecordThreadId = -1;

	yRc = aDestroyResource (gCall[zCall].zAcuPort, zCall, RECORD_CHAN);

	yRc = reserveRTP (gCall[zCall].zAcuPort, &gACard);
	initPort (gCall[zCall].zAcuPort, RECORD_CHAN);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Exiting from aculabRecordThread.");

	pthread_detach (pthread_self ());
	pthread_exit (NULL);

	//return(yRc);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int
setChannel (int zChannel, int zPortValue, int zAppPid, int zType)
{
	static char     mod[] = "setChannel";

	if (zType == RECORD_CHAN)
	{
		pthread_mutex_lock (&gMutexCPChannel);

		gACard.chan[zChannel].appPort = zPortValue;
		gACard.chan[zChannel].pid = zAppPid;

		if (zPortValue < 0)
		{
			gNumRecordingChansInUse--;
		}
		else
		{
			gNumRecordingChansInUse++;
		}

		pthread_mutex_unlock (&gMutexCPChannel);

		dynVarLog (__LINE__, zPortValue, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Set record channel %d to %d, appPid=%d.", zChannel,
				   zPortValue, zAppPid);

		return (0);
	}
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int
assignFreeChannel (int zAppPort, int zAppPid, int *zSlotNum, int zType)
{
	static char     mod[] = "assignFreeChannel";
	int             i;

	if (zType == RECORD_CHAN)
	{
		pthread_mutex_lock (&gMutexCPChannel);
		*zSlotNum = -1;
		for (i = 0; i < gMaxAcuPorts; i++)
		{
			if (gACard.chan[i].appPort < 0)
			{
				*zSlotNum = i;
				gACard.chan[i].appPort = zAppPort;
				gACard.chan[i].pid = zAppPid;

				pthread_mutex_unlock (&gMutexCPChannel);
				return (0);
			}
		}
		pthread_mutex_unlock (&gMutexCPChannel);
		return (-1);
	}

}								// END: assignFreeChannel()

///This function is used by the inputThread() to recordFiles and send packets to B_LEG during a bridge call.
int
recordFile_X_with_trailsilence (int zCall, struct MsgToDM *zMsgToDM)
{
	int             yRc = 0;
	char            mod[] = { "recordFile_X_with_trailsilence" };
	char            buffer[4096] = "";
	char            header[4096] = "";
	char            yErrMsg[256] = "";
	FILE           *infile;
	char            zFileName[255] = "";
	char            file[256] = "";
	int             createFile;
	int             createFile_X;
	time_t          yCurrentTime;
	time_t          yRecordTime;
	time_t          yLeadSilence;
	double          yBeepTime;
	int             i = 0;
	int             err = 0;
	int             yRtpRecvCount = 0;
	int             streamAvailable = 1;
	int             streamStarted = 0;
	struct MsgToApp response;
	int             yMrcpRc = 0;
	int             yMrcpFailureCount = 0;
	int             startSavingData = NO;
	int             acuRtpPort = -1;
	int             zSlotNum;

	gCall[zCall].trailSilence = 0;
	gCall[zCall].stop_aculabRecordThread = 0;

	// for each request
	//      * config rtp
	//      * connect RTP to TiNG
	//      * listen
	//      * when get a response, sm_get_recognized()
	A_RTP_DETAILS   aRtpDetails;

	yRc = assignFreeChannel (zCall, zMsgToDM->appPid, &zSlotNum, RECORD_CHAN);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Got free AcuLab port = %d", zSlotNum);
	if (yRc < 0 || zSlotNum < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "Failed to assign free Aculab channel yRc=%d, zSlotNum=%d",
				   yRc, zSlotNum);
		return -1;
	}

	gCall[zCall].zAcuPort = zSlotNum;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling setChannel");

	(void) setChannel (zSlotNum, zCall, zMsgToDM->appPid, RECORD_CHAN);

	memset ((A_RTP_DETAILS *) & aRtpDetails, '\0', sizeof (aRtpDetails));
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling configureRTP.");
	yRc = configureRTP (zSlotNum, &gACard, &aRtpDetails);

	INIT_ACU_SM_STRUCT (&gACard.chan[zSlotNum].channelParms);

	gACard.chan[zSlotNum].channelParms.type = kSMChannelTypeInput;
	gACard.chan[zSlotNum].channelParms.module =
		gACard.chan[zSlotNum].rx_create.module;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling sm_channel_alloc_placed.");

	if ((yRc =
		 sm_channel_alloc_placed (&gACard.chan[zSlotNum].channelParms)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "sm_channel_alloc_placed() failed.  type=%d  module_id=%d",
				   gACard.chan[zSlotNum].channelParms.type,
				   (int) gACard.chan[zSlotNum].channelParms.module);

		yRc = terminateRTP (zSlotNum, &gACard);
		yRc = aShutdown (zSlotNum, &gACard);
		return (-1);
	}

	gACard.chan[zSlotNum].TiNG_ChannelId =
		gACard.chan[zSlotNum].channelParms.channel;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "sm_channel_alloc_placed() succeeded. module_id=%d "
			   "TiNG_ChannelId=%d.",
			   (int) gACard.chan[zSlotNum].channelParms.module,
			   (int) gACard.chan[zSlotNum].TiNG_ChannelId);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling connectRTP_to_TiNG");
	yRc = connectRTP_to_TiNG (zSlotNum, &gACard);

#if 0
	yRc = pthread_create (&gACard.chan[zSlotNum].aculabRecordThreadId,
						  &gpthread_attr,
						  aculabRecordThread,
						  (void *) &(gCall[zCall].msgToDM));
	if (yRc != 0)
	{
		gACard.chan[zCall].aculabRecordThreadId = -1;
	}
#endif

	acuRtpPort = gACard.chan[zSlotNum].rx_port.RTP_port;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "acuRtpPort = %d.", acuRtpPort);

/*
	Done Aculab initialization for trail silence
*/

//Create local rtp session to send data to Aculab

	RtpSession     *rtp_session_trailsilence;
	int             acuTs = 0;
	int             acuTime = 0;
	char            aculabServer[256] = "";

	sprintf (aculabServer, "%s", gACard.cardInfo.ip_address);

	rtp_session_trailsilence = rtp_session_new (RTP_SESSION_SENDONLY);
	rtp_session_set_blocking_mode (rtp_session_trailsilence, 0);
	rtp_session_set_ssrc (rtp_session_trailsilence, atoi ("3197096732"));
	rtp_session_set_profile (rtp_session_trailsilence, &av_profile);

	rtp_session_set_remote_addr (rtp_session_trailsilence,
								 aculabServer, acuRtpPort, gHostIf);

	rtp_session_set_payload_type (rtp_session_trailsilence,
								  gCall[zCall].codecType);

//  rtp_profile_set_payload(&av_profile, yPayloadType, &telephone_event);

	response.message[0] = '\0';
	createFile = 1;
	gCall[zCall].recordStarted = 0;

		gCall[zCall].isIFrameDetected = 1;

	getWavHeader (zCall, header, 1000 * 1000);

	memset (yErrMsg, 0, sizeof (yErrMsg));

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "inside recordFile_X", "", zCall);

	int             beepSilenceTime = 0;

	gCall[zCall].receivingSilencePackets = 0;
	gCall[zCall].recordStarted = 1;

	yRtpRecvCount = 0;
	err = 0;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "RECORD START with audioRecordFilename=%s",
			   gCall[zCall].msgRecord.filename);

	if (gCall[zCall].msgRecord.beepFile[0] != 0)
	{
	struct Msg_Speak yBeepMsg;
	long int        yIntQueueElement = -1;

		beepSilenceTime =
			getBeepTime (gCall[zCall].msgRecord.beepFile, zCall);

		memcpy (&(yBeepMsg), &(gCall[zCall].msgRecord),
				sizeof (struct Msg_Record));

		yBeepMsg.opcode = DMOP_SPEAK;
		yBeepMsg.synchronous = ASYNC;
		yBeepMsg.list = 0;
		yBeepMsg.allParties = 0;
		yBeepMsg.key[0] = '\0';
		yBeepMsg.interruptOption = INTERRUPT;

		sprintf (yBeepMsg.file, "%s", gCall[zCall].msgRecord.beepFile);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting isBeepActive = 1.");
		gCall[zCall].isBeepActive = 1;

		__DDNDEBUG__ (DEBUG_MODULE_SR, "Calling addToSpeakList", "", zCall);
		addToAppRequestList ((struct MsgToDM *) &(yBeepMsg), __LINE__);

		gCall[zCall].pendingOutputRequests++;

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "Pending requests total",
					  gCall[zCall].pendingOutputRequests);

	}							/*END: if beep */
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting isBeepActive = 0.");
		gCall[zCall].isBeepActive = 0;
	}

	time (&yRecordTime);
	time (&yCurrentTime);

	struct timeb    tb;
	double          currentMilliSec = 0;

	ftime (&tb);
	currentMilliSec = (tb.time + ((double) tb.millitm) / 1000) * 1000;

	yBeepTime = currentMilliSec + beepSilenceTime;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "beepSilenceTime = %d.", beepSilenceTime);

	yLeadSilence = yCurrentTime + gCall[zCall].msgRecord.lead_silence;
	yRecordTime += gCall[zCall].msgRecord.record_time;

	switch (gCall[zCall].msgRecord.overwrite)
	{
	case APPEND:
		infile = fopen (gCall[zCall].msgRecord.filename, "a+");
		break;

	case NO:
		infile = fopen (gCall[zCall].msgRecord.filename, "w+");
		break;

	case YES:
	default:
		infile = fopen (gCall[zCall].msgRecord.filename, "w");
		break;

	}							/*END: switch */

	if (infile == NULL)
	{
	int             rc;

		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
				   "End of Record; infile in NULL.");

		response.opcode = gCall[zCall].msgRecord.opcode;
		response.appCallNum = gCall[zCall].msgRecord.appCallNum;
		response.appPassword = gCall[zCall].msgRecord.appPassword;
		response.appPid = gCall[zCall].msgRecord.appPid;
		response.appRef = gCall[zCall].msgRecord.appRef;
		response.returnCode = -7;

		sprintf (response.message, "\0");

		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

		gCall[zCall].receivingSilencePackets = 1;

		if (rtp_session_trailsilence != NULL)
		{
			rtp_session_destroy (rtp_session_trailsilence);
		}
		gCall[zCall].stop_aculabRecordThread = 1;
		return (-7);
	}

	switch (gCall[zCall].msgRecord.overwrite)
	{
	case APPEND:
		break;

	case NO:
		fseek (infile, SEEK_END, 0);
		break;

	case YES:
	default:
		fseek (infile, SEEK_SET, 0);
		if (gCall[zCall].recordOption != WITH_RTP)
		{
			writeWavHeaderToFile (zCall, infile);
		}
	}							/*END: switch */

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "RECV NON EMPTY", 0);

	sprintf (response.message, "\0");
	response.opcode = gCall[zCall].msgRecord.opcode;

	err = 0;
	int             have_more = 0;
	gCall[zCall].recordStartedForDisconnect = 1; // MR 5126

	while (1)
	{
		time (&yCurrentTime);

		ftime (&tb);
		currentMilliSec = (tb.time + ((double) tb.millitm) / 1000) * 1000;

		if (!canContinue (mod, zCall, __LINE__))
		{
			yRc = -3;
			break;
		}
		else if (gCall[zCall].pendingInputRequests > 0)
		{
			yRc = ACTION_GET_NEXT_REQUEST;
			break;
		}
		else if (yCurrentTime > yRecordTime)
		{
			response.opcode = gCall[zCall].msgRecord.opcode;
			yRc = ACTION_GET_NEXT_REQUEST;
			break;
		}

#if 1
		if (gCall[zCall].trailSilence == 1)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "trailSilence stopping the recording");
			yRc = ACTION_GET_NEXT_REQUEST;
			break;
		}
#endif

		if (currentMilliSec > yBeepTime)
		{
			startSavingData = YES;
		}

		memcpy (buffer, gCall[zCall].silenceBuffer,
				gCall[zCall].codecBufferSize);

		if (gCall[zCall].isBeepActive == 0)
		{
			if (gCall[zCall].recordOption == WITH_RTP)
			{
				yRtpRecvCount =
					rtp_session_recv_with_ts (gCall[zCall].rtp_session_in,
											  buffer, 0, 0, &have_more, 1,
											  rtp_session_trailsilence);

				if (yRtpRecvCount > 0)
				{
					streamAvailable = 0;
				}
			}
			else
			{
				yRtpRecvCount =
					rtp_session_recv_with_ts (gCall[zCall].rtp_session_in,
											  buffer,
											  gCall[zCall].codecBufferSize,
											  gCall[zCall].in_user_ts,
											  &streamAvailable, 0,
											  rtp_session_trailsilence);
			}
		}
		else
		{
			if (gCall[zCall].recordOption == WITH_RTP)
			{
				yRtpRecvCount =
					rtp_session_recv_with_ts (gCall[zCall].rtp_session_in,
											  buffer, 0, 0, &have_more, 1,
											  NULL);

				if (yRtpRecvCount > 0)
				{
					streamAvailable = 0;
				}
			}
			else
			{

				yRtpRecvCount =
					rtp_session_recv_with_ts (gCall[zCall].rtp_session_in,
											  buffer,
											  gCall[zCall].codecBufferSize,
											  gCall[zCall].in_user_ts,
											  &streamAvailable, 0, NULL);
			}
		}

		err = err + yRtpRecvCount;

		if (err >= gCall[zCall].codecBufferSize &&
			!streamStarted && !streamAvailable)
		{
			yRc = pthread_create (&gACard.chan[zSlotNum].aculabRecordThreadId,
								  &gpthread_attr,
								  aculabRecordThread,
								  (void *) &(gCall[zCall].msgToDM));
			if (yRc != 0)
			{
				gACard.chan[zCall].aculabRecordThreadId = -1;
			}
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting streamStarted to 1.");
			streamStarted = 1;
		}

		if (gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
		{
			gCall[zCall].in_user_ts += (gCall[zCall].inTsIncrement * 3);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Setting dtmfAvailable to 0.");
			gCall[zCall].dtmfAvailable = 0;

			if (gCall[zCall].msgRecord.interrupt_option == INTERRUPT 
				|| gCall[zCall].msgRecord.interrupt_option == FIRST_PARTY_INTERRUPT
				|| gCall[zCall].msgRecord.interrupt_option == 1)
			{
				if (dtmf_tab[gCall[zCall].lastDtmf] == (gCall[zCall].msgRecord.terminate_char) 
					|| gCall[zCall].msgRecord.terminate_char == 32)
				{

//fprintf(stderr, " %s() gCall[zCall].msgRecord.terminate_char==%d", __func__, gCall[zCall].msgRecord.terminate_char);
					gCall[zCall].GV_RecordTermChar = dtmf_tab[gCall[zCall].lastDtmf];

					yRc = ACTION_GET_NEXT_REQUEST;

					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO, "RECORD END. Calling break.");

					break;
				}
			}

			sprintf (response.message, "%c", dtmf_tab[gCall[zCall].lastDtmf]);

			__DDNDEBUG__ (DEBUG_MODULE_FILE, "Sending DTMF to app", "", -1);

			response.opcode = DMOP_GETDTMF;
			response.appCallNum = zCall;
			response.appPassword = gCall[zCall].appPassword;
			response.appPid = gCall[zCall].appPid;
			response.returnCode = 0;

			if (gCall[zCall].responseFifoFd >= 0)
			{

				if (gCall[zCall].dtmfCacheCount > 0)
				{
	int             i = 0;

					for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
					{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

						sprintf (response.message, "%c",
								 dtmf_tab[dtmfToSend]);

						writeGenericResponseToApp (zCall, &response, mod,
												   __LINE__);
					}
					gCall[zCall].dtmfCacheCount = 0;
				}

				sprintf (response.message, "%c",
						 dtmf_tab[gCall[zCall].lastDtmf]);

				yRc =
					writeGenericResponseToApp (zCall, &response, mod,
											   __LINE__);
			}
		}

		if (!streamStarted && yCurrentTime > yLeadSilence)
		{
			yRc = -2;

			sprintf (yErrMsg,
					 "streamStarted(%d), yLeadSilence(%d), yRecordTime(%d) yRc(%d)",
					 streamStarted, yLeadSilence, yRecordTime, yRc);

			__DDNDEBUG__ (DEBUG_MODULE_RTP, "Timed out", yErrMsg, 0);

			break;
		}

		if (gCall[zCall].isBeepActive == 1)
		{
			if ((streamStarted) && (err >= gCall[zCall].codecBufferSize))
			{
				gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
				err = 0;
			}

			yRtpRecvCount = 0;

			rtpSleep (20, &gCall[zCall].lastInRtpTime, __LINE__, zCall);
			continue;
		}

		if (yRtpRecvCount > 0 &&
			gCall[zCall].isIFrameDetected == 1 &&
			gCall[zCall].isBeepActive == 0 && startSavingData == YES)
		{
	int             byteToWrite = yRtpRecvCount;

			if (gCall[zCall].recordOption == WITH_RTP)
			{
	BYTE            l_caLength[2];

				l_caLength[0] = (BYTE) (byteToWrite >> 8);
				l_caLength[1] = (BYTE) byteToWrite;
				i = fwrite (l_caLength, 1, 2, infile);
				if (gCall[zCall].msgRecord.overwrite == APPEND)
				{
					modifyTS (buffer, zCall, byteToWrite);
				}
			}
			else
			{
			}

			i = fwrite (buffer, 1, byteToWrite, infile);
		}
		else if (gCall[zCall].isBeepActive == 0 || startSavingData == NO)
		{
			if (gCall[zCall].isIFrameDetected == 1
				&& gCall[zCall].recordOption != WITH_RTP)
			{
	int             byteToWrite = gCall[zCall].codecBufferSize;

				if (gCall[zCall].recordOption == WITH_RTP)
				{
					byteToWrite += 12;
	BYTE            l_caLength[2];

					l_caLength[0] = (BYTE) (byteToWrite >> 8);
					l_caLength[1] = (BYTE) byteToWrite;
					i = fwrite (l_caLength, 1, 2, infile);
					if (gCall[zCall].msgRecord.overwrite == APPEND)
					{
						modifyTS (buffer, zCall, byteToWrite);
					}
				}

				i = fwrite (gCall[zCall].silenceBuffer, 1, byteToWrite,
							infile);
			}

			gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
			err = 0;
		}
		else if (gCall[zCall].isBeepActive == 1 ||
				 yRtpRecvCount == 0 || startSavingData == NO)
		{
			if (gCall[zCall].isIFrameDetected == 1
				&& gCall[zCall].recordOption != WITH_RTP)
			{
	int             byteToWrite = gCall[zCall].codecBufferSize;

				if (gCall[zCall].recordOption == WITH_RTP)
				{
					byteToWrite += 12;
	BYTE            l_caLength[2];

					l_caLength[0] = (BYTE) (byteToWrite >> 8);
					l_caLength[1] = (BYTE) byteToWrite;
					i = fwrite (l_caLength, 1, 2, infile);
					if (gCall[zCall].msgRecord.overwrite == APPEND)
					{
						modifyTS (buffer, zCall, byteToWrite);
					}
				}

				i = fwrite (gCall[zCall].silenceBuffer, 1, byteToWrite,
							infile);
			}

			gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
			err = 0;
		}

		if ((streamStarted) && (err >= gCall[zCall].codecBufferSize))
		{
			gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
			//gCall[zCall].in_user_ts+=gCall[zCall].codecBufferSize;
			err = 0;
		}

		yRtpRecvCount = 0;

		rtpSleep (20, &gCall[zCall].lastInRtpTime, __LINE__, zCall);

	}							/*while(1) */
	gCall[zCall].recordStartedForDisconnect = 0;        // MR 5126

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Writing header",
				  gCall[zCall].msgRecord.filename, 0);

	if (gCall[zCall].recordOption == WITHOUT_RTP)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Writing wav header to the record file.");

		writeWavHeaderToFile (zCall, infile);
	}
	else
	{
		setLastRecordedRtpTs (buffer, zCall);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "RECORD END.");

	fclose (infile);

	if ( (arcDtmfData[zCall].lead_silence_triggered == 1) ||
	     (gCall[zCall].poundBeforeLeadSilence == 1) )			// BT-226
	{
		infile = fopen (zFileName, "w");
		fclose(infile);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[%d] Zero'd out (%s).", zLine, zFileName);
	}

	int		rc2;
	int		rc;

	arcDtmfData[zCall].lead_silence_triggered = 0;
	gCall[zCall].poundBeforeLeadSilence = 0;

	if ( (yRc != ACTION_NEW_REQUEST_AVAILABLE) &&
	     ( zMsgToDM->opcode != DMOP_SRRECOGNIZE ) )
	{
		int             rc;

		response.opcode = gCall[zCall].msgRecord.opcode;
		response.appCallNum = gCall[zCall].msgRecord.appCallNum;
		response.appPassword = gCall[zCall].msgRecord.appPassword;
		response.appPid = gCall[zCall].msgRecord.appPid;
		response.appRef = gCall[zCall].msgRecord.appRef;
		response.returnCode = (yRc >= 0) ? 0 : yRc;

		sprintf (response.message, "%c", gCall[zCall].GV_RecordTermChar);

		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	gCall[zCall].receivingSilencePackets = 1;

	if (rtp_session_trailsilence != NULL)
	{
		rtp_session_destroy (rtp_session_trailsilence);
	}

	gCall[zCall].stop_aculabRecordThread = 1;
	return (yRc);

}								/*END: int recordFile_X_with_trailsilence */
#endif

// for ulaw and alaw 
// the silence buffer is OK to use 
// later if we use other codecs we will nost likely have to 
// come up with a 16 bit version of the silence buffer 
// to use 

static int
slin16_gen_audio_silence (char *buf, size_t size)
{

	int             samples;
	int             i, j;
	short          *ptr;

	// short values[] = {-1, 1, 0, -1, 0, 1, -1, 0};
	short           values[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	if (!size)
	{
		return -1;
	}

	samples = size / 2;

	memset (buf, 0, size);

	j = 0;

	for (i = 0; i < size; i++, i++)
	{
		ptr = (short *) &buf[i];
		*ptr = values[(j &= 8 - 1)];
		j++;
	}

	return samples;
}

int
arc_do_inband_detection (char *buff, size_t size, int zCall, char *func,
						 int line)
{
	char            mod[] = { "arc_do_inband_detection" };
	int             rc = -1;
	int             samples;
	int             decode = 1;
	int				noDtmf = 0;
	char            silence_buff[320];
    int 		     silenceCounter = 1;
	char			triggerFile[256]="/tmp/cpTrigger.txt";

	if (size <= 0)
	{
		decode = 0;
		size = 160;
		noDtmf = 1;
	}

	if ( access(triggerFile, F_OK) == 0 )
	{
		unlink(triggerFile);
		silenceCounter=0;
	}

	if (gCall[zCall].runToneThread != 0)
	{

		if (gCall[zCall].codecType == 0 || gCall[zCall].codecType == 8)
		{

			//if(size > 0 && (  gCall[zCall].sendCallProgressAudio != 0 || gCall[zCall].recordStarted || gCall[zCall].telephonePayloadType < 0))
			if (size > 0 && gCall[zCall].runToneThread
				&& (gCall[zCall].sendCallProgressAudio != 0
					|| gCall[zCall].recordStarted))
			{

				if (decode == 1)
				{
					samples =
						arc_decode_buff (__LINE__, &gCall[zCall].decodeAudio[AUDIO_IN],
										 buff, size,
										 gCall[zCall].
										 decodedAudioBuff[AUDIO_IN],
										 (int) sizeof (gCall[zCall].
													   decodedAudioBuff
													   [AUDIO_IN]));
				}
				else
				{
					// fprintf(stderr, " %s() using silence packets, detected silence.... \n", __func__);
					slin16_gen_audio_silence (gCall[zCall].
											  decodedAudioBuff[AUDIO_IN],
											  320);
					samples = 160;
				}

				if (samples > 0)
				{
#if 0
                    if ( silenceCounter == 0 )
                    {
						//
						// MR 4645 - This is to insert some audio (silence, noise, etc) for call progress to fake it out
						// and return better results.
						//
						FILE           *fp_read = NULL;
						char           buf[512];
						int				rc2;
						char			file[256]="/tmp/keep/hellohowdy.wav";
	
						memset((char *)buf, '\0', sizeof(buf));
//						dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
//										   DYN_BASE, INFO, "DJB: opening (%s)", file);
						fp_read = fopen (file, "r");
						if (fp_read != NULL)
						{
//							dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
//										   DYN_BASE, INFO, "DJB: opened wav");
							while ((rc2 = fread (buf, 1, 320, fp_read)) == 320)
							{
                        		rc = arc_tones_ringbuff_post (&gCall[zCall].toneDetectionBuff,
                                                  buf, 320);
//								dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
//										   DYN_BASE, INFO, "DJB: posting silence.  silenceCounter=%d", silenceCounter);
							}
//							dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
		//										   DYN_BASE, INFO, "DJB: done with sending silence");
									fclose(fp_read);
								}
								else
								{
		//							dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, DYN_BASE, INFO, "DJB: error opening (%s).  [%d]",
//														file, errno);
						}
						silenceCounter++;
					}
#endif
					rc = arc_tones_ringbuff_post (&gCall[zCall].toneDetectionBuff,
												  gCall[zCall].decodedAudioBuff[AUDIO_IN],
												  samples * 2);
				}
//				else
//				{
//					fprintf (stderr, " %s() samples=%d\n", __func__, samples);
//				}
			}
		}
	}

//DDN 06/24/2011    Inband DTMF Detection
	if (noDtmf == 0 && size > 0 && gCall[zCall].telephonePayloadType == -99 )
	{
		int  detect = 0;

		if (decode == 1)
		{
			samples =
				arc_decode_buff (__LINE__, &gCall[zCall].decodeDtmfAudio[AUDIO_IN],
								 buff, size,
								 gCall[zCall].decodedDtmfAudioBuff[AUDIO_IN],
								 (int) sizeof (gCall[zCall].
											   decodedDtmfAudioBuff
											   [AUDIO_IN]));
		}

		if (samples > 0)
		{
			rc = arc_tones_ringbuff_post (&gCall[zCall].dtmfDetectionBuff,
										  gCall[zCall].
										  decodedDtmfAudioBuff[AUDIO_IN],
										  samples * 2);
		}

		if(rc != -1)
		{
			detect  = arc_tones_ringbuff_detect (&gCall[zCall].dtmfDetectionBuff,
								   ARC_TONES_INBAND_DTMF, 10);
		}

		if (detect & ARC_TONES_INBAND_DTMF)
		{
	struct timeb    currentDtmfTime;
	long            current = 0;
	long            last = 0;

			ftime (&currentDtmfTime);

			current = (1000 * currentDtmfTime.time) + currentDtmfTime.millitm;
			last =
				(1000 * arcDtmfData[zCall].lastTime.time) +
				arcDtmfData[zCall].lastTime.millitm;

			if ((current - last) < 96
				&& (arcDtmfData[zCall].x ==
					gCall[zCall].dtmfDetectionBuff.dtmf[0]))
			{
				if(gCall[zCall].GV_HideDTMF)
				{
					dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						   DYN_BASE, INFO,
						   "SpanDSP: Duplicate DTMF char %c current time(%d) last dtmf time(%d)",
						   'X', current, last);
				}
				else
				{
					dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						   DYN_BASE, INFO,
						   "SpanDSP: Duplicate DTMF char %c current time(%d) last dtmf time(%d)",
						   gCall[zCall].dtmfDetectionBuff.dtmf[0], current,
						   last);
				}
			}
			else if (gCall[zCall].dtmfDetectionBuff.dtmf[0] != '\0')
			{
				int i = 0;

				ftime (&arcDtmfData[zCall].lastTime);
				arcDtmfData[zCall].x = gCall[zCall].dtmfDetectionBuff.dtmf[i];

				for (i = 0; gCall[zCall].dtmfDetectionBuff.dtmf[i] != '\0'; i++)
				{
                    if(gCall[zCall].GV_HideDTMF)
					{
						dynVarLog (__LINE__, zCall, (char *) __func__,
							   REPORT_VERBOSE, DYN_BASE, INFO,
							   "SpanDSP: Inband DTMF char %c", 'X');
					}
					else
					{
						dynVarLog (__LINE__, zCall, (char *) __func__,
							   REPORT_VERBOSE, DYN_BASE, INFO,
							   "SpanDSP: Inband DTMF char %c",
							   gCall[zCall].dtmfDetectionBuff.dtmf[i]);
					}

					if (gCall[zCall].dtmfAvailable)
					{
						int i = gCall[zCall].dtmfCacheCount;

						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, 9005,
								   INFO, "Found unprocessed DTMF in cache.");

						if (i < 0)
							i = 0;

						gCall[zCall].dtmfInCache[i] = gCall[zCall].lastDtmf;

						gCall[zCall].dtmfCacheCount++;
					}

					gCall[zCall].dtmfAvailable = 1;
					gCall[zCall].lastDtmf =
						get_dtmf_position (gCall[zCall].dtmfDetectionBuff.
										   dtmf[i]);

					if (gCall[zCall].callState == CALL_STATE_CALL_BRIDGED &&		// MR 4854
						gCall[zCall].callSubState ==
						CALL_STATE_CALL_MEDIACONNECTED)
					{
						char yStrTmpBuffer[2];

						sprintf (yStrTmpBuffer, "%c",
								 gCall[zCall].dtmfDetectionBuff.dtmf[i]);

						saveBridgeCallPacket (zCall, yStrTmpBuffer, gCall[zCall].dtmfDetectionBuff.dtmf[i], 0);	/* -1 for NO DTMF */
					}			//bridged
				}				//for

				if (gCall[zCall].GV_FlushDtmfBuffer)	//Work around for Level3 fast DTMFs
				{
					arc_tones_ringbuff_free (&gCall[zCall].dtmfDetectionBuff);
					arc_tones_ringbuff_init (&gCall[zCall].dtmfDetectionBuff,
											 ARC_TONES_INBAND_DTMF,
											 gToneThreshMinimum,
											 gToneThreshDifference, 0);;
				}

			}//toneDetectionBuff.dtmf[0]
		}
	}

	return rc;

}/*arc_do_inband_detection */

///This function is used by the inputThread() to recordFiles and send packets to B_LEG during a bridge call.
int
recordFile_X (int zLine, int zCall, struct MsgToDM *zMsgToDM)
{
	int             yRc = 0;
	char            mod[] = { "recordFile_X" };
	char            buffer[4096] = "";
	char            header[4096] = "";
	char            yErrMsg[256] = "";
	FILE           *infile;
	char            zFileName[255] = "";
	char            file[256] = "";
	int             createFile;
	int             createFile_X;
	time_t          yCurrentTime;
	time_t          yRecordTime;
	time_t          yLeadSilence;
	double          yBeepTime;
	int             i = 0;
	int             err = 0;
	int             yRtpRecvCount = 0;
	int             streamAvailable = 1;
	int             streamStarted = 0;
	struct MsgToApp response;
	int             yMrcpRc = 0;
	int             yMrcpFailureCount = 0;
	int             startSavingData = NO;
	int             bytes;
	int             samples;
	char			gsDir[32];

	gCall[zCall].isInbandToneDetectionThreadStarted = 0;
	int             mrcpSilencePacketCounter = 0;

//  static int packetNum = 0;
	response.message[0] = '\0';
	createFile = 1;
	gCall[zCall].recordStarted = 0;
	struct Msg_SRRecognize yMsgRecognize;

	gCall[zCall].isIFrameDetected = 1;

	getWavHeader (zCall, header, 1000 * 1000);

	memset (yErrMsg, 0, sizeof (yErrMsg));

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "inside recordFile_X", "", zCall);

	if (zMsgToDM == NULL) 
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "zMsgToDM == NULL", "", zCall);
		int             packetCount = 0;

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "RECV EMPTY", 0);

		gCall[zCall].receivingSilencePackets = 1;

//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//				   "[%d] DJB: zMsgToDM is NULL.", zLine);
		/* Close GSR UDP Port if it was active */
		if(gCall[zCall].googleUDPFd > -1)
		{
__DDNDEBUG__ (DEBUG_MODULE_SR, "MRCP: ARCGS: Closing googleUDPFd", "", gCall[zCall].googleUDPFd);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		   		"[%d] GSR: Sending 'bye' to client.", zLine);
			sendto(gCall[zCall].googleUDPFd,
					"bye", 
					3, 
					0,
					(struct sockaddr *) &gCall[zCall].google_si, 
					gCall[zCall].google_slen);	

			close(gCall[zCall].googleUDPFd);
			gCall[zCall].googleUDPPort = -1;
			gCall[zCall].googleUDPFd = -1;
			gCall[zCall].speechRec = 0;
		}

//  DING0

		while (1)
		{
			time (&yCurrentTime);


			if (!canContinue (mod, zCall, __LINE__))
			{
				yRc = -3;

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "canContinue is false",
							  0);

				return (yRc);
			}
			else if (gCall[zCall].pendingInputRequests > 0)
			{
				yRc = ACTION_GET_NEXT_REQUEST;
				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "",
							  "pendingInputRequests > 0", 0);
				return (yRc);
			}

			// memset(buffer, 0xff, 4096);
			memcpy (buffer, gSilenceBuffer, 4096);

			//send silence packet to backend after every 50 packets = 1 second
			if (gCall[zCall].rtp_session_mrcp != NULL
				&& mrcpSilencePacketCounter % 50 == 0)
			{
#if 1
				__DDNDEBUG__ (DEBUG_MODULE_SR,
							  "MRCP: sending Silence to MRCP Server", "",
							  zCall);
				yMrcpRc =
					arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
												  gCall[zCall].
												  rtp_session_mrcp,
												  gCall[zCall].silenceBuffer,
												  gCall[zCall].
												  codecBufferSize,
												  gCall[zCall].mrcpTs);
				gCall[zCall].mrcpTs += 160;
#endif
			}

			if(gCall[zCall].googleUDPFd > -1)
			{
				sendto(gCall[zCall].googleUDPFd,
						gCall[zCall].silenceBuffer, 
						gCall[zCall].codecBufferSize, 
						0,
						(struct sockaddr *) &gCall[zCall].google_si, 
						gCall[zCall].google_slen);	
//				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//						"DJB: MRCP: ARCGS: Sent SILENCE audio packet to Google SR Client");
			}

			mrcpSilencePacketCounter++;

			if (
					(
						gCall[zCall].leg == LEG_B &&
						(	gCall[zCall].callState == CALL_STATE_CALL_BRIDGED ||
							gCall[zCall].callSubState == CALL_STATE_CALL_MEDIACONNECTED
						)
					)
					||
					(
						gCall[zCall].leg == LEG_A &&
						(
							gCall[zCall].callState == CALL_STATE_CALL_BRIDGED
						)
					)
				)
			{
				if (gCall[zCall].callSubState != CALL_STATE_CALL_LISTENONLY)
					//&& gCall[gCall[zCall].crossPort].sendingSilencePackets == 1)
				{
					if (gCall[zCall].crossPort > -1 &&
						canContinue (mod, gCall[zCall].crossPort, __LINE__) &&
						gCall[gCall[zCall].crossPort].rtp_session)

					{
						// added this wrapper to test for some thing 
						// and add some logging 

						bytes = arc_rtp_bridge_with_ts(__func__, zCall, __LINE__, 
													  gCall[zCall].rtp_session_in, buffer,
                                                      gCall[zCall].
                                                      codecBufferSize,
                                                      gCall[zCall].in_user_ts,
                                                      &streamAvailable, 0,
                                                      gCall[gCall[zCall].crossPort].rtp_session
													  );

					}
					else
					{

			        bytes =  arc_rtp_session_recv_in(__func__, zCall, __LINE__, 
								gCall[zCall].rtp_session_in, buffer, gCall[zCall].codecBufferSize, &streamAvailable);


					}

					err += bytes;

					arc_do_inband_detection (buffer, bytes, zCall,
											 (char *) __func__, __LINE__);

					//arc_frame_decode_and_post(zCall, buffer, bytes, AUDIO_IN, 0, (char *)__func__, __LINE__);

					if (gCall[zCall].conferenceStarted == 1)
					{
						arc_conference_decode_and_post (zCall, buffer, bytes,
														0, (char *) __func__,
														__LINE__);
					}

					if (gCall[zCall].leg == LEG_A)
					{
						arc_frame_decode_and_post (zCall, buffer, bytes,
												   AUDIO_MIXED, 0,
												   (char *) __func__,
												   __LINE__);
					}
					else
					{
						int  crossPort = gCall[zCall].crossPort;

						if (crossPort > -1 && crossPort < MAX_PORTS)
						{
							if (bytes == 0)
							{
								arc_frame_apply (crossPort, gSilenceBuffer,
												 160, AUDIO_MIXED,
												 ARC_AUDIO_PROCESS_AUDIO_MIX,
												 (char *) __func__, __LINE__);
							}
							else
							{
								arc_frame_apply (crossPort, buffer, 160,
												 AUDIO_MIXED,
												 ARC_AUDIO_PROCESS_AUDIO_MIX,
												 (char *) __func__, __LINE__);
							}
						}
					}

					dtmf_tone_to_ascii (zCall, buffer, &arcDtmfData[zCall],
										PROCESS_DTMF);
				}
			}
#ifdef ACU_LINUX
			else if (gCall[zCall].sendCallProgressAudio == 1 &&
					 gCall[zCall].rtp_session_tone != NULL)
			{
				bytes = rtp_session_recv_with_ts (gCall[zCall].rtp_session_in,
												  buffer,
												  gCall[zCall].
												  codecBufferSize,
												  gCall[zCall].in_user_ts,
												  &streamAvailable, 0,
												  gCall[zCall].
												  rtp_session_tone);

				err += bytes;
				arc_do_inband_detection (buffer, bytes, zCall,
										 (char *) __func__, __LINE__);

				arc_frame_decode_and_post (zCall, buffer, bytes, AUDIO_IN, 0,
										   (char *) __func__, __LINE__);
				arc_frame_decode_and_post (zCall, buffer, bytes, AUDIO_MIXED,
										   0, (char *) __func__, __LINE__);

				arc_conference_decode_and_post (zCall, buffer, bytes, 0,
												(char *) __func__, __LINE__);

			}
			else if (gCall[zCall].conferenceStarted == 1 &&
					 gCall[zCall].rtp_session_conf_send &&
					 gCall[zCall].sendingSilencePackets == 1)
			{
				err =
					err +
					rtp_session_recv_with_ts (gCall[zCall].rtp_session_in,
											  buffer,
											  gCall[zCall].codecBufferSize,
											  gCall[zCall].in_user_ts,
											  &streamAvailable, 0,
											  gCall[zCall].
											  rtp_session_conf_send);

			}
#endif
			else
			{
#if 1
				if (gCall[zCall].resetRtpSession == 1)
				{
					__DDNDEBUG__ (DEBUG_MODULE_RTP,
								  "resetting RTP Session in.", "", -1);
					rtp_session_reset (gCall[zCall].rtp_session_in);
					gCall[zCall].resetRtpSession = 0;
				}
#endif


			    bytes =  arc_rtp_session_recv_in(__func__, zCall, __LINE__, gCall[zCall].rtp_session_in, buffer, gCall[zCall].codecBufferSize, &streamAvailable);



				__DDNDEBUG__ (DEBUG_MODULE_RTP, "Bytes", "", bytes);
				__DDNDEBUG__ (DEBUG_MODULE_RTP, "TS", "",
							  gCall[zCall].in_user_ts);

				err += bytes;
				arc_do_inband_detection (buffer, bytes, zCall, (char *) __func__, __LINE__);

				//arc_frame_decode_and_post(zCall, buffer, bytes, AUDIO_IN, 0, (char *)__func__, __LINE__);
				arc_frame_decode_and_post (zCall, buffer, bytes, AUDIO_MIXED,
										   0, (char *) __func__, __LINE__);

				if (gCall[zCall].conferenceStarted == 1)
				{
					arc_conference_decode_and_post (zCall, buffer, bytes, 0,
													mod, __LINE__);
				}
				dtmf_tone_to_ascii (zCall, buffer, &arcDtmfData[zCall],
									PROCESS_DTMF);
			}

			arc_frame_record_to_file (zCall, AUDIO_MIXED, (char *) __func__,
									  __LINE__);

			if (gCall[zCall].isCalea == CALL_STATE_CALL_LISTENONLY_CALEA &&
				gCall[zCall].sendingSilencePackets == 1 &&
				gCall[gCall[zCall].caleaPort].rtp_session != NULL)
			{
				//gCall[gCall[zCall].caleaPort].out_user_ts+=gCall[gCall[zCall].caleaPort].outTsIncrement;
				arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
											  gCall[gCall[zCall].caleaPort].
											  rtp_session, buffer,
											  gCall[zCall].codecBufferSize,
											  gCall[gCall[zCall].caleaPort].
											  out_user_ts);
			}
			else if (gCall[zCall].callSubState == CALL_STATE_CALL_LISTENONLY
					 && gCall[zCall].sendingSilencePackets == 1
					 && gCall[gCall[zCall].crossPort].rtp_session != NULL)
			{
			    bytes =  arc_rtp_session_recv_in(__func__, zCall, __LINE__, gCall[zCall].rtp_session_in, buffer, gCall[zCall].codecBufferSize, &streamAvailable);
			}
#ifdef VOICE_BIOMETRICS
            if ( gCall[zCall].continuousVerify == CONTINUOUS_VERIFY_ACTIVE )
            {
                // copy
                memcpy(&(gCall[zCall].avb_buffer[gCall[zCall].avb_bCounter]),
                            buffer, gCall[zCall].codecBufferSize);

                gCall[zCall].avb_bCounter += gCall[zCall].codecBufferSize;

//                dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
//                        TEL_BASE, INFO, "[%d] AVB Buffer now at %d; speechsize=%d", zLine,,
//                        gCall[zCall].avb_bCounter, gAvb_SpeechSize);

                if ( gCall[zCall].avb_bCounter >= gAvb_SpeechSize )
                {
                    avb_process_buffer(zCall, gCall[zCall].avb_bCounter);
                    gCall[zCall].avb_bCounter = 0;
                }
                if (gCall[zCall].gUtteranceFileFp != NULL )
                {
                    fwrite (buffer, gCall[zCall].codecBufferSize, 1,
                            gCall[zCall].gUtteranceFileFp);
                }
            }
#endif  // END: VOICE_BIOMETRICS

			if (err >= gCall[zCall].codecBufferSize &&
				!streamStarted && !streamAvailable)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] Setting streamStarted to 1.", zLine);
				//rtp_session_reset(gCall[zCall].rtp_session_in);
				streamStarted = 1;
			}

			if (gCall[zCall].sendingSilencePackets == 1 &&
				gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
			{
				gCall[zCall].in_user_ts += (gCall[zCall].inTsIncrement * 2);

//__DDNDEBUG__(DEBUG_MODULE_CALL, "Calling clearSpeakList", "", -1);
//              clearSpeakList(zCall, __LINE__);

				__DDNDEBUG__ (DEBUG_MODULE_FILE, "Sending DTMF", "", -1);

				sprintf (response.message, "%c",
						 dtmf_tab[gCall[zCall].lastDtmf]);

				response.opcode = DMOP_GETDTMF;
				response.appCallNum = zCall;
				response.appPassword = zCall;
				response.appPid = gCall[zCall].appPid;
				response.appRef = gCall[zCall].msgToDM.appRef;
				response.returnCode = 0;

				/*RG START */
				if (gCall[zCall].leg == LEG_B)
				{
	int             yIntALeg = gCall[zCall].crossPort;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "[%d] Setting dtmfAvailable to 0.", zLine);

					gCall[zCall].dtmfAvailable = 0;

					if (yIntALeg >= 0 && gCall[yIntALeg].responseFifoFd >= 0)
					{
						if (gCall[zCall].dtmfCacheCount > 0)
						{
	int             i = 0;

							for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
							{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

								sprintf (response.message, "%c",
										 dtmf_tab[dtmfToSend]);

                    			if(gCall[zCall].GV_HideDTMF)
								{
									dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, TEL_BASE, INFO,
										   "[%d] Sending DTMF (%c) to %d.", zLine,
										   'X', yIntALeg);
								}
								else
								{
									dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, TEL_BASE, INFO,
										   "[%d] Sending DTMF (%c) to %d.", zLine,
										   dtmf_tab[dtmfToSend], yIntALeg);
								}

								writeGenericResponseToApp (yIntALeg,
														   &response,
														   mod, __LINE__);
							}

							gCall[zCall].dtmfCacheCount = 0;

						}

						sprintf (response.message, "%c",
								 dtmf_tab[gCall[zCall].lastDtmf]);

                    	if(gCall[zCall].GV_HideDTMF)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO, "[%d] Sending DTMF (%c) to %d.", zLine,
								   'X', yIntALeg);
						}
						else
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO, "[%d] Sending DTMF (%c) to %d.", zLine,
								   dtmf_tab[gCall[zCall].lastDtmf], yIntALeg);
						}
					

						writeGenericResponseToApp (yIntALeg, &response, mod,
												   __LINE__);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_INVALID_DATA, WARN,
								   "[%d] Invalid a-leg port (%d) or no response fifo fd (%d).  Unable to send DTMF.", zLine,
								   yIntALeg, gCall[yIntALeg].responseFifoFd);
					}
				}
				else if (gCall[zCall].rtp_session_mrcp == NULL ||
						 (gCall[zCall].callState == CALL_STATE_CALL_BRIDGED ||
						  gCall[zCall].callSubState ==
						  CALL_STATE_CALL_MEDIACONNECTED
						  || gCall[zCall].GV_BridgeRinging == 1))
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "[%d] Setting dtmfAvailable to 0.", zLine);

					gCall[zCall].dtmfAvailable = 0;

					if (gCall[zCall].dtmfCacheCount > 0)
					{
	int             i = 0;

						for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
						{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

							sprintf (response.message, "%c",
									 dtmf_tab[dtmfToSend]);

                    		if(gCall[zCall].GV_HideDTMF)
							{
								dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "[%d] Sending DTMF (%c) to %d.", zLine,
									   'X', zCall);
							}
							else
							{
								dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "[%d] Sending DTMF (%c) to %d.", zLine,
									   dtmf_tab[dtmfToSend], zCall);
							}

							writeGenericResponseToApp (zCall,
													   &response,
													   mod, __LINE__);
						}
						gCall[zCall].dtmfCacheCount = 0;

					}

					sprintf (response.message, "%c",
							 dtmf_tab[gCall[zCall].lastDtmf]);
                    if(gCall[zCall].GV_HideDTMF)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "[%d] Sending DTMF (%c) to %d.", zLine, 'X', zCall);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "[%d] Sending DTMF (%c) to %d.", zLine,
							   dtmf_tab[gCall[zCall].lastDtmf], zCall);
					}

					writeGenericResponseToApp (zCall,
											   &response, mod, __LINE__);
				}
				else 
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "[%d] Setting dtmfAvailable to 0.", zLine);
					gCall[zCall].dtmfAvailable = 0;

					if (gCall[zCall].responseFifoFd >= 0)
					{

						if (gCall[zCall].dtmfCacheCount > 0)
						{
	int             i = 0;

							for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
							{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

								sprintf (response.message, "%c",
										 dtmf_tab[dtmfToSend]);

								writeGenericResponseToApp (zCall,
														   &response,
														   mod, __LINE__);
							}
							gCall[zCall].dtmfCacheCount = 0;

						}

						sprintf (response.message, "%c",
								 dtmf_tab[gCall[zCall].lastDtmf]);

						writeGenericResponseToApp (zCall,
												   &response, mod, __LINE__);
					}
				}
			}

			if (streamStarted && err > 0)
			{
				err = 0;
				yRtpRecvCount = 0;
			}

			if (gCall[zCall].conferenceStarted != 1)
			{
				rtpSleep (20, &gCall[zCall].lastInRtpTime, __LINE__, zCall);
			}

		}						/*while(1) */
	}
	// ding1
	else if (zMsgToDM->opcode == DMOP_SRRECOGNIZEFROMCLIENT)
	{
		FILE           *recFile = NULL;

		gCall[zCall].speechRec = 1;
		memcpy (&yMsgRecognize, zMsgToDM, sizeof (struct Msg_SRRecognize));

		__DDNDEBUG__ (DEBUG_MODULE_SR, "Bargein", "", yMsgRecognize.bargein);

		gCall[zCall].canBargeIn =
			(yMsgRecognize.bargein == YES
			 || yMsgRecognize.bargein == INTERRUPT) ? 1 : 0;

		if (yMsgRecognize.beepFile[0] != 0 &&
			access (yMsgRecognize.beepFile, F_OK) == 0)
		{
	struct Msg_Speak yBeepMsg;
	long int        yIntQueueElement = -1;

			__DDNDEBUG__ (DEBUG_MODULE_SR, "addToSpeakList",
						  yMsgRecognize.beepFile, zCall);

			memcpy (&(yBeepMsg), &(gCall[zCall].msgToDM),
					sizeof (struct Msg_Speak));

			yBeepMsg.opcode = DMOP_SPEAK;
			yBeepMsg.synchronous = PUT_QUEUE_ASYNC;
			yBeepMsg.list = 0;
			yBeepMsg.allParties = 0;
			yBeepMsg.key[0] = '\0';
			yBeepMsg.interruptOption = 0;

			sprintf (yBeepMsg.file, "%s", yMsgRecognize.beepFile);

			addToSpeakList (&(yBeepMsg), &yIntQueueElement, __LINE__);
		}

		if (gCall[zCall].pFirstSpeak == NULL)
		{
	struct MsgToDM  yMsgToDM;

			__DDNDEBUG__ (DEBUG_MODULE_SR,
						  "Sending message to SRClient,  no Prompts", "",
						  zCall);

			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
					   "[%d] Speak List is empty.", zLine);

			memset (&yMsgToDM, 0, sizeof (struct MsgToDM));

			memcpy (&yMsgToDM, &(gCall[zCall].msgToDM),
					sizeof (struct MsgToDM));

			yMsgToDM.opcode = DMOP_SRPROMPTEND;

			sendMsgToSRClient (__LINE__, zCall, &yMsgToDM);

			gCall[zCall].canBargeIn = 1;
		}
		else
		{
	struct Msg_Speak yBeepMsg;
	long int        yIntQueueElement = -1;

			__DDNDEBUG__ (DEBUG_MODULE_SR, "addToSpeakList",
						  "PLAY_QUEUE_ASYNC", zCall);
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "[%d] Speak list is not empty, playing list; interruptOption = %d.", zLine,
					   gCall[zCall].pFirstSpeak->msgSpeak.interruptOption);

			printSpeakList (gCall[zCall].pFirstSpeak);

			if (gCall[zCall].pFirstSpeak->msgSpeak.interruptOption ==
				NONINTERRUPT)
			{
				gCall[zCall].dtmfCacheCount = 0;
				gCall[zCall].dtmfAvailable = 0;
			}

			memcpy (&(yBeepMsg), &(gCall[zCall].msgToDM),
					sizeof (struct Msg_Speak));

			yBeepMsg.opcode = DMOP_SPEAK;
			yBeepMsg.synchronous = PLAY_QUEUE_ASYNC;
			yBeepMsg.list = 0;
			yBeepMsg.allParties = 0;
			yBeepMsg.key[0] = '\0';
			yBeepMsg.interruptOption = 0;

			sprintf (yBeepMsg.file, "%s", "\0");

			__DDNDEBUG__ (DEBUG_MODULE_SR, "calling addToSpeakList", "",
						  zCall);
			addToAppRequestList ((struct MsgToDM *) &(yBeepMsg));

			addToSpeakList (&(yBeepMsg), &yIntQueueElement, __LINE__);

			gCall[zCall].pendingOutputRequests++;
		}

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "Before while", 0);

		if (gCall[zCall].mrcpTime > 0)
		{
	time_t          yTempTime;

			time (&yTempTime);
			gCall[zCall].mrcpTs += (yTempTime - gCall[zCall].mrcpTime) * 8000;
		}

		if (gCall[zCall].GV_RecordUtterance == 1)
		{
			if (gCall[zCall].lastRecUtteranceFile != NULL &&
				gCall[zCall].lastRecUtteranceFile[0] != '\0' &&
				gCall[zCall].gUtteranceFileFp == NULL)
			{
				gCall[zCall].gUtteranceFileFp =
					fopen (gCall[zCall].lastRecUtteranceFile, "w+");
			}

		}

		while (1)
		{
			if (gCall[zCall].resetRtpSession == 1)
			{
				__DDNDEBUG__ (DEBUG_MODULE_RTP, "resetting RTP Session in.",
							  "", -1);
				rtp_session_reset (gCall[zCall].rtp_session_in);
				gCall[zCall].resetRtpSession = 0;
			}

			if (!canContinue (mod, zCall, __LINE__))
			{
				yRc = -3;

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "yRc = -3", -3);

				break;
			}
			else if (gCall[zCall].pendingInputRequests > 0)
			{
				yRc = ACTION_GET_NEXT_REQUEST;

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "",
							  "ACTION_GET_NEXT_REQUEST",
							  ACTION_GET_NEXT_REQUEST);

				break;
			}

			memset (buffer, 0, 4096);


			if (
				(
					(
						gCall[zCall].leg == LEG_B &&
						(	gCall[zCall].callState == CALL_STATE_CALL_BRIDGED ||
							gCall[zCall].callSubState == CALL_STATE_CALL_MEDIACONNECTED
						)
					) ||
					(
						gCall[zCall].leg == LEG_A &&
						(
							gCall[zCall].callState == CALL_STATE_CALL_BRIDGED 
						)
					)
				)
				
					&& gCall[zCall].callSubState != CALL_STATE_CALL_LISTENONLY

				)//if
			{
                 bytes = arc_rtp_bridge_with_ts(__func__, zCall, __LINE__,
                                               gCall[zCall].rtp_session_in, buffer,
                                               gCall[zCall].
                                               codecBufferSize,
                                               gCall[zCall].in_user_ts,
                                               &streamAvailable, 0,
                                               gCall[gCall[zCall].crossPort].rtp_session
                                               );



				err += bytes;
				arc_do_inband_detection (buffer, bytes, zCall,
										 (char *) __func__, __LINE__);

				//arc_frame_decode_and_post(zCall, buffer, bytes, AUDIO_IN, 0, (char *)__func__, __LINE__);
				arc_frame_decode_and_post (zCall, buffer, bytes, AUDIO_MIXED,
										   0, (char *) __func__, __LINE__);

				arc_conference_decode_and_post (zCall, buffer, bytes, 0,
												(char *) __func__, __LINE__);

				dtmf_tone_to_ascii (zCall, buffer, &arcDtmfData[zCall],
									PROCESS_DTMF);
			}
			else if (gCall[zCall].callSubState == CALL_STATE_CALL_LISTENONLY
					 && gCall[zCall].sendingSilencePackets == 1
					 && gCall[gCall[zCall].crossPort].rtp_session != NULL)
			{

                 int bytes = 0;
                 int streamAvailable = 0;
                 int a = zCall;
                 int b = gCall[zCall].crossPort;

                 bytes = arc_rtp_bridge_with_ts(__func__, a, __LINE__,
                                                gCall[a].rtp_session_in, buffer,
                                                gCall[a].codecBufferSize,
                                                gCall[a].in_user_ts,
                                                &streamAvailable, 0,
                                                gCall[b].rtp_session, 1);


				err += bytes;
#if 0
				arc_do_inband_detection (buffer, bytes, zCall,
										 (char *) __func__, __LINE__);
				//arc_frame_decode_and_post(zCall, buffer, bytes, AUDIO_IN, 0, (char *)__func__, __LINE__);
				arc_frame_decode_and_post (zCall, buffer, bytes, AUDIO_MIXED,
										   0, (char *) __func__, __LINE__);

				if (bytes && gCall[zCall].conferenceStarted)
				{
					arc_conference_decode_and_post (zCall, buffer, bytes, 0,
													(char *) __func__,
													__LINE__);
				}
#endif

			}
			else
			{

			    bytes =  arc_rtp_session_recv_in(__func__, zCall, __LINE__, gCall[zCall].rtp_session_in, buffer, gCall[zCall].codecBufferSize, &streamAvailable);
				//bytes += err;
				err += bytes;

				arc_do_inband_detection (buffer, bytes, zCall,
										 (char *) __func__, __LINE__);

				//arc_frame_decode_and_post(zCall, buffer, bytes, AUDIO_IN, 0, (char *)__func__, __LINE__);
				arc_frame_decode_and_post (zCall, buffer, gCall[zCall].codecBufferSize, AUDIO_IN,
										   0, (char *) __func__, __LINE__);
				if (bytes && gCall[zCall].conferenceStarted)
				{
					arc_conference_decode_and_post (zCall, buffer, bytes, 0,
													(char *) __func__,
													__LINE__);
				}

				dtmf_tone_to_ascii (zCall, buffer, &arcDtmfData[zCall],
									PROCESS_DTMF);
			}

#ifdef VOICE_BIOMETRICS
            if ( gCall[zCall].continuousVerify )
            {
                // copy
                memcpy(&(gCall[zCall].avb_buffer[gCall[zCall].avb_bCounter]),
                            buffer, gCall[zCall].codecBufferSize);

                gCall[zCall].avb_bCounter += gCall[zCall].codecBufferSize;

                dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
                          TEL_BASE, INFO, "[%d] AVB Buffer now at %d", zLine, gCall[zCall].avb_bCounter);

                if ( gCall[zCall].avb_bCounter >= gAvb_SpeechSize )
                {
                    avb_process_buffer(zCall, gCall[zCall].avb_bCounter);
                    gCall[zCall].avb_bCounter = 0;
                }
            }
#endif  // END: VOICE_BIOMETRICS

			arc_frame_record_to_file (zCall, AUDIO_MIXED, (char *) __func__,
									  __LINE__);

			if (gCall[zCall].isCalea == CALL_STATE_CALL_LISTENONLY_CALEA &&
				gCall[zCall].sendingSilencePackets == 1)
			{
	int             yIntTmpCaleaPort = gCall[zCall].caleaPort;

				if (yIntTmpCaleaPort >= 0
					&& gCall[yIntTmpCaleaPort].rtp_session != NULL)
				{
					gCall[yIntTmpCaleaPort].out_user_ts +=
						gCall[yIntTmpCaleaPort].outTsIncrement;

					arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
												  gCall[yIntTmpCaleaPort].
												  rtp_session, buffer,
												  gCall[yIntTmpCaleaPort].
												  codecBufferSize,
												  gCall[yIntTmpCaleaPort].
												  out_user_ts);
				}
			}

		//	gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
			if (gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
			{
				if (gCall[zCall].canBargeIn)
				{
	struct MsgToDM  yMsgToDM;

					//gCall[zCall].dtmfAvailable = 0;   

					memset (&yMsgToDM, 0, sizeof (struct MsgToDM));

					memcpy (&yMsgToDM, zMsgToDM, sizeof (struct MsgToDM));

					yMsgToDM.opcode = DMOP_SRPROMPTEND;

					sprintf (yMsgToDM.data, "%c",
							 dtmf_tab[gCall[zCall].lastDtmf]);

					__DDNDEBUG__ (DEBUG_MODULE_SR,
								  "MRCP: Sending DTMF to MRCP Client",
								  yMsgToDM.data, zCall);

                    if(gCall[zCall].GV_HideDTMF)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "[%d] Got DTMF=%c, and GV_RecordTermChar=%c.", zLine,
							   'X', gCall[zCall].GV_RecordTermChar);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "[%d] Got DTMF=%c, and GV_RecordTermChar=%c.", zLine,
							   dtmf_tab[gCall[zCall].lastDtmf],
							   gCall[zCall].GV_RecordTermChar);
					}
#if 0
					if (gCall[zCall].GV_RecordTermChar ==
						dtmf_tab[gCall[zCall].lastDtmf])
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, INFO,
								   "[%d] Got DTMF=%c, and GV_RecordTermChar=%c not sending DTMF to backend", zLine,
								   dtmf_tab[gCall[zCall].lastDtmf],
								   gCall[zCall].GV_RecordTermChar);
						gCall[zCall].dtmfAvailable = 0;

					}
					else
#endif
					{
			     		if ( ( gCall[zCall].GV_LastPlayPostion == 1 ) &&
							 ( gCall[zCall].currentSpeakFd > 0 ) )
						{
							if ( gCall[zCall].currentSpeakSize > 40960 )
							{
								errno=0;
								gCall[zCall].currentPhraseOffset_save = gCall[zCall].currentPhraseOffset;
								if ((gCall[zCall].currentPhraseOffset =
										lseek(gCall[zCall].currentSpeakFd, 0, SEEK_CUR)) < 0)
								{
									gCall[zCall].currentPhraseOffset =
													gCall[zCall].currentPhraseOffset_save;
									dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
											"[%d] lseek() of fd %d failed. Unable to get current ", 
											"offset.  [%d, %s]  Setting to previous value of %d.", zLine,
											gCall[zCall].currentSpeakFd, errno, strerror(errno),
											gCall[zCall].currentPhraseOffset);
								}
								else
								{
				            		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
										"[%d] Current offset for file fd (%d) is %ld. Size=%d.", zLine, 
										gCall[zCall].currentSpeakFd,
										gCall[zCall].currentPhraseOffset,
										gCall[zCall].currentSpeakSize);
								}
							}
							else
							{
				            		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
										"[%d] Current size for file fd (%d) is %d.  Too small to set the offset.", zLine, 
										gCall[zCall].currentSpeakFd,
										gCall[zCall].currentSpeakSize);
							}
						}

						/*Notify the backend about DTMF input */
                    	if(gCall[zCall].GV_HideDTMF)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "[%d] Got DTMF=%c, and GV_RecordTermChar=%c sending DTMF to backend, dtmfCacheCount=%d", zLine,
								   'X',
								   gCall[zCall].GV_RecordTermChar,
								   gCall[zCall].dtmfCacheCount);
						}
						else
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "[%d] Got DTMF=%c, and GV_RecordTermChar=%c sending DTMF to backend, dtmfCacheCount=%d", zLine,
								   dtmf_tab[gCall[zCall].lastDtmf],
								   gCall[zCall].GV_RecordTermChar,
								   gCall[zCall].dtmfCacheCount);
						}

						if (gCall[zCall].dtmfCacheCount > 0)
						{
	int             i = 0;

							for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
							{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

								sprintf (response.message, "%c",
										 dtmf_tab[dtmfToSend]);

                    			if(gCall[zCall].GV_HideDTMF)
								{
									dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, TEL_BASE, INFO,
										   "[%d] Sending DTMF (%c) to backend.", zLine, 'X');
								}
								else
								{
									dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, TEL_BASE, INFO,
										   "[%d] Sending DTMF (%c) to backend.", zLine,
										   dtmf_tab[dtmfToSend]);
								}

								
								if(gCall[zCall].googleUDPFd > -1)
								{
	__DDNDEBUG__ (DEBUG_MODULE_SR, "MRCP: ARCGS: Sending silence to Google SR Client", "", zCall);

									sendto(gCall[zCall].googleUDPFd,
											gCall[zCall].silenceBuffer, 
											gCall[zCall].codecBufferSize, 
											0,
											(struct sockaddr *) &gCall[zCall].google_si, 
											gCall[zCall].google_slen);	
								}

			

								yMrcpRc =
									arc_rtp_session_send_with_ts (mod,
																  __LINE__,
																  zCall,
																  gCall
																  [zCall].
																  rtp_session_mrcp,
																  gCall
																  [zCall].
																  silenceBuffer,
																  gCall
																  [zCall].
																  codecBufferSize,
																  gCall
																  [zCall].
																  mrcpTs);

								gCall[zCall].mrcpTs += 160;
								rtpSleep (20, &gCall[zCall].lastOutRtpTime,
										  __LINE__, zCall);

								yMrcpRc =
									rtp_session_send_dtmf (gCall[zCall].
														   rtp_session_mrcp,
														   dtmf_tab
														   [dtmfToSend],
														   gCall[zCall].
														   mrcpTs);
								gCall[zCall].mrcpTs += 160 + 160 + 160;

							}

							gCall[zCall].dtmfCacheCount = 0;
						}

						/*Send Audio Data to Google*/
						
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   			INFO, "[%d] gCall[zCall].googleUDPFd=%d", __LINE__,
										gCall[zCall].googleUDPFd);
						if(gCall[zCall].googleUDPFd > -1)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   			INFO, "[%d] Sending audio to google client.", zLine);
							sendto(gCall[zCall].googleUDPFd,
									gCall[zCall].silenceBuffer, 
									gCall[zCall].codecBufferSize, 
									0,
									(struct sockaddr *) &gCall[zCall].google_si, 
									gCall[zCall].google_slen);	
						}


						/*Send Audio Data to the back end */
						yMrcpRc =
							arc_rtp_session_send_with_ts (mod, __LINE__,
														  zCall,
														  gCall[zCall].
														  rtp_session_mrcp,
														  gCall[zCall].
														  silenceBuffer,
														  gCall[zCall].
														  codecBufferSize,
														  gCall[zCall].
														  mrcpTs);

						gCall[zCall].mrcpTs += 160;
						rtpSleep (20, &gCall[zCall].lastOutRtpTime, __LINE__,
								  zCall);

						yMrcpRc =
							rtp_session_send_dtmf (gCall[zCall].
												   rtp_session_mrcp,
												   dtmf_tab[gCall[zCall].
															lastDtmf],
												   gCall[zCall].mrcpTs);
						gCall[zCall].mrcpTs += 160 + 160 + 160;

						gCall[zCall].dtmfAvailable = 0;

						/*Notify mrcp client */
						sendMsgToSRClient (__LINE__, zCall, &yMsgToDM);
					}

					/*Exit from current play */
					gCall[zCall].pendingOutputRequests++;
				}
				else
				{
					gCall[zCall].dtmfAvailable = 0;
				}
			}
			else if (gCall[zCall].canBargeIn && err > 0)
			{
				
				if(gCall[zCall].googleUDPFd > -1)
				{

					yMrcpRc = sendto(gCall[zCall].googleUDPFd,
							buffer, 
							gCall[zCall].codecBufferSize, 
							0,
							(struct sockaddr *) &gCall[zCall].google_si, 
							gCall[zCall].google_slen);	

__DDNDEBUG__ (DEBUG_MODULE_SR, "MRCP: ARCGS: Sent audio packet to Google SR Client", "", yMrcpRc);
				}

				yMrcpRc = arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
														gCall[zCall].
														rtp_session_mrcp,
														buffer,
														gCall[zCall].
														codecBufferSize,
														gCall[zCall].mrcpTs);

				gCall[zCall].mrcpTs += 160;

				if (yMrcpRc < 0)
				{

					__DDNDEBUG__ (DEBUG_MODULE_SR,
								  "MRCP: Failed to send RTP to MRCP Server",
								  "", zCall);

					yMrcpFailureCount++;

					if (yMrcpFailureCount > 200)
					{
						break;
					}
				}
				else
				{
					yMrcpFailureCount = 0;

//__DDNDEBUG__(DEBUG_MODULE_SR, yStrTmpLogMsg, "", zCall);
				}

			}
			else
			{
	int             yTmpCounter = 50;

				if (gCall[zCall].canBargeIn && err <= 0)
				{
					yTmpCounter = 1;
				}

				//send silence packet to backend after every 50 packets = 1 second
				if (gCall[zCall].rtp_session_mrcp != NULL
					&& mrcpSilencePacketCounter % yTmpCounter == 0)
				{
					yMrcpRc =
						arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													  gCall[zCall].
													  rtp_session_mrcp,
													  gCall[zCall].
													  silenceBuffer,
													  gCall[zCall].
													  codecBufferSize,
													  gCall[zCall].mrcpTs);

					gCall[zCall].mrcpTs += 160;
				}
				mrcpSilencePacketCounter++;
			}

			if (gCall[zCall].gUtteranceFileFp != NULL && err > 0)
			{
				fwrite (buffer, gCall[zCall].codecBufferSize, 1,
						gCall[zCall].gUtteranceFileFp);
			}

			//rtpSleep(20, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);
			rtpSleep (20, &gCall[zCall].lastInRtpTime, __LINE__, zCall);

		}						/*END: while */

		time (&gCall[zCall].mrcpTime);

		__DDNDEBUG__ (DEBUG_MODULE_SR, "MRCP", "After while", 0);

		gCall[zCall].speechRecFromClient = 0;
		gCall[zCall].speechRec = 0;

#if 0
		if (gCall[zCall].rtp_session_mrcp != NULL)
		{
			rtp_session_destroy (gCall[zCall].rtp_session_mrcp);
			gCall[zCall].rtp_session_mrcp = NULL;
		}
#endif

		if (gCall[zCall].gUtteranceFileFp != NULL)
		{
			if (gCall[zCall].recordUtteranceWithWav == 1)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] Writing wav header to file=%s.", zLine,
						   gCall[zCall].lastRecUtteranceFile);

				writeWavHeaderToFile (zCall, gCall[zCall].gUtteranceFileFp);
			}

			fclose (gCall[zCall].gUtteranceFileFp);
			gCall[zCall].gUtteranceFileFp = NULL;

			/*Trim the start of file */
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "[%d] Calling trimStartOfWavFile utteranceTime1=%ld, utteranceTime2=%ld.", zLine,
					   gCall[zCall].utteranceTime1,
					   gCall[zCall].utteranceTime2);

			trimStartOfWavFile (zCall, gCall[zCall].lastRecUtteranceFile,
								gCall[zCall].utteranceTime1,
								gCall[zCall].utteranceTime2);
		}

		gCall[zCall].GV_RecordUtterance = 0;

		return (yRc);

	}							/*DMOP_SRRECOGNIZEFROMCLIENT */

	/*NOTE: Only DMOP_RECORD can go beyond this point. */
	/*NOTE: Also DMOP_SRRECOGNIZE for google streaming. */

#ifdef ACU_RECORD_TRAILSILENCE

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
	   "[%d] ACU - Inside ACU_RECORD_TRAILSILENCE, gTrailSilenceDetection=%d, trail_silence=%d.", zLine,
	   gTrailSilenceDetection, gCall[zCall].msgRecord.trail_silence);

	if (gTrailSilenceDetection == 1 &&
		gCall[zCall].msgRecord.trail_silence > 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[%d] Calling recordFile_X_with_trailsilence.", zLine);

		yRc = recordFile_X_with_trailsilence (zCall, zMsgToDM);
		if (yRc == -1)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_RECORD_ERROR,
					   WARN,
					   "[%d] Recording with trail silence failed. Continuing regular recording.", zLine);
		}
		else
		{
			return yRc;
		}
	}
#endif

	int             beepSilenceTime = 0;

	gCall[zCall].receivingSilencePackets = 0;
	gCall[zCall].recordStarted = 1;

	yRtpRecvCount = 0;
	err = 0;

	if ( zMsgToDM->opcode == DMOP_SRRECOGNIZE )
	{
//		gCall[zCall].googleStreamingSR = 1;
//		gCall[zCall].speechRec = 1;
		memcpy (&yMsgRecognize,
			&(gCall[zCall].msgToDM), sizeof (struct Msg_SRRecognize));

        gCall[zCall].canBargeIn =
            (yMsgRecognize.bargein == YES
             || yMsgRecognize.bargein == INTERRUPT) ? 1 : 0;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] ARCGS: In recordFile_X() for DMOP_SRRECOGNIZE.", zLine);

		sprintf(gsDir, "%s", "/tmp/.GS");
		if ( (yRc = arc_mkdir(gsDir, 0755)) == 0 )
		{
			sprintf(zFileName, "%s/gsrUtterance.%d.wav", gsDir, zCall);
		}
		else
		{
			sprintf(zFileName, "/tmp/gsrUtterance.%d.wav", zCall);
		}
		sprintf(gCall[zCall].msgRecord.filename, "%s", zFileName);
		sprintf(gCall[zCall].msgRecord.beepFile, "%s", yMsgRecognize.beepFile);
		gCall[zCall].msgRecord.lead_silence = yMsgRecognize.lead_silence / 1000;
	    gCall[zCall].msgRecord.trail_silence = yMsgRecognize.trail_silence / 1000;
		gCall[zCall].msgRecord.record_time = yMsgRecognize.total_time / 1000;
		gCall[zCall].msgRecord.overwrite = YES;
		gCall[zCall].msgRecord.overwrite = YES;

//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//				"DJB: yMsgRecognize.lead_silence:%d  gCall[%d].msgRecord.lead_silence:%d",
//				yMsgRecognize.lead_silence, zCall, gCall[zCall].msgRecord.lead_silence);
	}
	else
	{
		if ( (gCall[zCall].msgToDM.opcode == DMOP_RECORDMEDIA)  &&
		     (gCall[zCall].googleRecord == NO) )
		{
			sprintf (zFileName, "%s", gCall[zCall].msgRecordMedia.audioFileName);
		}
		else
		{
			sprintf (zFileName, "%s", gCall[zCall].msgRecord.filename);
		}
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] RECORD START with audioRecordFilename=(%s); beepFile=(%s).", zLine,
			   zFileName, gCall[zCall].msgRecord.beepFile);

	if (gCall[zCall].msgRecord.beepFile[0] != 0)
	{
	struct Msg_Speak yBeepMsg;
	long int        yIntQueueElement = -1;

		beepSilenceTime =
			getBeepTime (gCall[zCall].msgRecord.beepFile, zCall);

		memcpy (&(yBeepMsg), &(gCall[zCall].msgRecord),
				sizeof (struct Msg_Record));

		yBeepMsg.opcode = DMOP_SPEAK;
		yBeepMsg.synchronous = ASYNC;
		yBeepMsg.list = 0;
		yBeepMsg.allParties = 0;
		yBeepMsg.key[0] = '\0';
		yBeepMsg.interruptOption = INTERRUPT;

		memset (yBeepMsg.file, 0, sizeof (yBeepMsg.file));
		sprintf (yBeepMsg.file, "%s", gCall[zCall].msgRecord.beepFile);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[%d] Setting isBeepActive to 1.", zLine);

		gCall[zCall].isBeepActive = 1;

		__DDNDEBUG__ (DEBUG_MODULE_SR, "Calling addToAppRequestList", "", zCall);
		addToAppRequestList ((struct MsgToDM *) &(yBeepMsg));

		gCall[zCall].pendingOutputRequests++;

		//usleep(200*1000);

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "Pending requests total",
					  gCall[zCall].pendingOutputRequests);

	}							/*END: if beep */
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[%d] Setting isBeepActive to 0.", zLine);

		gCall[zCall].isBeepActive = 0;
	}

	if ( ( zMsgToDM->opcode == DMOP_SRRECOGNIZE ) ||
	     ( gCall[zCall].googleRecordIsActive == 1 ) )
	{
		struct Msg_Speak yBeepMsg;
		long int        yIntQueueElement = -1;

		__DDNDEBUG__ (DEBUG_MODULE_SR, "addToSpeakList",
						  "PLAY_QUEUE_ASYNC", zCall);

		printSpeakList (gCall[zCall].pFirstSpeak);

		if  ( gCall[zCall].pFirstSpeak ) 
		{
			if (gCall[zCall].pFirstSpeak->msgSpeak.interruptOption == NONINTERRUPT)
			{
				gCall[zCall].dtmfCacheCount = 0;
				gCall[zCall].dtmfAvailable = 0;
			}
		}
//		else
//		{
//			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//				   "[%d] DJB: pFirstSpeak is NULL", zLine);
//		}

//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//				   "[%d] DJB: gCall[%d].msgToDM.opcode=%d",
//					zCall, gCall[zCall].msgToDM.opcode);

		memcpy (&(yBeepMsg), &(gCall[zCall].msgToDM),
					sizeof (struct Msg_Speak));

		yBeepMsg.opcode = DMOP_SPEAK;
		yBeepMsg.synchronous = PLAY_QUEUE_ASYNC;
		yBeepMsg.list = 0;
		yBeepMsg.allParties = 0;
		yBeepMsg.key[0] = '\0';
		yBeepMsg.interruptOption = 0;

		sprintf (yBeepMsg.file, "%s", "\0");

		__DDNDEBUG__ (DEBUG_MODULE_SR, "calling addToSpeakList", "",
						  zCall);
		addToAppRequestList ((struct MsgToDM *) &(yBeepMsg));

		addToSpeakList (&(yBeepMsg), &yIntQueueElement, __LINE__);

		gCall[zCall].pendingOutputRequests++;
	}

	time (&yRecordTime);
	time (&yCurrentTime);

	struct timeb    tb;
	double          currentMilliSec = 0;

	ftime (&tb);
	currentMilliSec = (tb.time + ((double) tb.millitm) / 1000) * 1000;

	yBeepTime = currentMilliSec + beepSilenceTime;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] beepSilenceTime = %d", zLine, beepSilenceTime);

	yLeadSilence = yCurrentTime + gCall[zCall].msgRecord.lead_silence;
	yRecordTime += gCall[zCall].msgRecord.record_time;

	switch (gCall[zCall].msgRecord.overwrite)
	{
	case APPEND:
		infile = fopen (zFileName, "a+");
		break;

	case NO:
		infile = fopen (zFileName, "w+");
		break;

	case YES:
	default:
		infile = fopen (zFileName, "w");
		break;
	}							/*END: switch */

	if ( zMsgToDM->opcode != DMOP_SRRECOGNIZE )
	{
		if (infile == NULL)
		{
		int             rc;
	
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_RECORD_ERROR, INFO, "[%d] RECORD END; null infile.", zLine);
	
			response.opcode = gCall[zCall].msgRecord.opcode;
			response.appCallNum = gCall[zCall].msgRecord.appCallNum;
			response.appPassword = gCall[zCall].msgRecord.appPassword;
			response.appPid = gCall[zCall].msgRecord.appPid;
			response.appRef = gCall[zCall].msgRecord.appRef;
			response.returnCode = -7;
	
			sprintf (response.message, "\0");
	
			rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	
			gCall[zCall].receivingSilencePackets = 1;
	
			return (-7);
		}
	}

//	dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_RECORD_ERROR, INFO, 
//			"[%d] DJB: gTrailSilenceDetection=%d,  trail_silence=%d, lead_silence=%d", zLine,
//			gTrailSilenceDetection,
//	     	gCall[zCall].msgRecord.trail_silence,
//			gCall[zCall].msgRecord.lead_silence);
	
	if ( ( gTrailSilenceDetection == 1) &&
	     ( gCall[zCall].msgRecord.trail_silence > 0) ||
	     ( gCall[zCall].msgRecord.lead_silence > 0) )
	{
		if (gCall[zCall].codecType == 0 || gCall[zCall].codecType == 8)
		{
			if (gCall[zCall].codecType == 8)
			{
				ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 0);
			}
			else if (gCall[zCall].codecType == 0)
			{
				ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 1);
			}

			arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_IN],
							  ARC_DECODE_G711, &gCall[zCall].g711parms,
							  (int) sizeof (gCall[zCall].g711parms), 1);
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						   "[%d] gTrailSilenceDetection is set, starting startInbandToneDetectionThread.", zLine);

			if ( gCall[zCall].googleRecord == YES )
			{
				time (&(gCall[zCall].utteranceTime1));
			}

			if ( zMsgToDM->opcode == DMOP_SRRECOGNIZE )
			{
				googleStartInbandToneDetectionThread (zCall, __LINE__, ARC_TONES_BEGINNING_AUDIO_TIMEOUT);
			}
			else
			{
				startInbandToneDetectionThread (zCall, __LINE__, ARC_TONES_BEGINNING_AUDIO_TIMEOUT);
			}
			gCall[zCall].isInbandToneDetectionThreadStarted = 1;
		}
	}

	switch (gCall[zCall].msgRecord.overwrite)
	{
	case APPEND:
		break;

	case NO:
		fseek (infile, SEEK_END, 0);
		break;

	case YES:
	default:
		fseek (infile, SEEK_SET, 0);
		if (gCall[zCall].recordOption != WITH_RTP)
		{
			writeWavHeaderToFile (zCall, infile);
		}
	}							/*END: switch */

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "RECV NON EMPTY", 0);

	sprintf (response.message, "\0");
	response.opcode = gCall[zCall].msgRecord.opcode;

	err = 0;
	int             have_more = 0;

	//rtp_session_reset(gCall[zCall].rtp_session_in);

	mblk_t *myBlk   = NULL;

	/*DDN: 02/22/2010 Initialize dtmfDataHolderFlags for lead and trail silence */

	__DDNDEBUG__ (DEBUG_MODULE_RTP,
				  "Initialized arcDtmfData to process lead and trail silence.",
				  "", zCall);

	arcDtmfData[zCall].lead_silence_triggered = 0;
	arcDtmfData[zCall].trail_silence_triggered = 0;
	arcDtmfData[zCall].lead_silence_processed = 0;
	arcDtmfData[zCall].trail_silence_processed = 0;

	arcDtmfData[zCall].lookForLeadSilence = 1;		/******/
	arcDtmfData[zCall].lookForTrailSilence = 0;

	arcDtmfData[zCall].silence_time = 0;

	arcDtmfData[zCall].lead_silence = gCall[zCall].msgRecord.lead_silence;
	arcDtmfData[zCall].trail_silence = gCall[zCall].msgRecord.trail_silence;
	arcDtmfData[zCall].last_non_silent_time = yTmpApproxTime;

	/*END: DDN: 02/22/2010 Initialize dtmfDataHolderFlags for lead and trail silence */

	if ( zMsgToDM->opcode != DMOP_SRRECOGNIZE )
	{
		gCall[zCall].recordStartedForDisconnect = 1;        // MR 5126
	}

	while (1)
	{
		time (&yCurrentTime);

		ftime (&tb);
		currentMilliSec = (tb.time + ((double) tb.millitm) / 1000) * 1000;

		if (!canContinue (mod, zCall, __LINE__))
		{
			yRc = -3;
			break;
		}
		else if (gCall[zCall].pendingInputRequests > 0)
		{
			//yRc = ACTION_GET_NEXT_REQUEST;
			yRc = ACTION_NEW_REQUEST_AVAILABLE;
			break;
		}
		else if (yCurrentTime > yRecordTime)
		{
			response.opcode = gCall[zCall].msgRecord.opcode;
			yRc = ACTION_GET_NEXT_REQUEST;
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Max record time reached.  Calling break.");
			break;
		}

		if (currentMilliSec > yBeepTime)
		{
			startSavingData = YES;
		}

		memcpy (buffer, gCall[zCall].silenceBuffer,
				gCall[zCall].codecBufferSize);

		//send silence packet to backend after every 50 packets = 1 second
		if (gCall[zCall].rtp_session_mrcp != NULL
			&& mrcpSilencePacketCounter % 50 == 0)
		{
			yMrcpRc = arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													gCall[zCall].
													rtp_session_mrcp,
													gCall[zCall].
													silenceBuffer,
													gCall[zCall].
													codecBufferSize,
													gCall[zCall].mrcpTs);
			gCall[zCall].mrcpTs += 160;
		}

		mrcpSilencePacketCounter++;

		if (gCall[zCall].recordOption == WITH_RTP)
		{
			myBlk = rtp_session_recvm_with_ts(gCall[zCall].rtp_session_in, gCall[zCall].in_user_ts, NULL);

			yRtpRecvCount =
				rtp_session_recv_with_ts (gCall[zCall].rtp_session_in, buffer,
										  0, 0, &have_more, 1, NULL);

			if (yRtpRecvCount > 0)
			{
				streamAvailable = 0;
			}

#if 0
			if (myBlk != NULL)
			{
				yRtpRecvCount = msgdsize (myBlk);
				yRtpRecvCount =
					arc_msg_to_buf_withrtp (myBlk, buffer, yRtpRecvCount);
				freemsg (myBlk);
				myBlk = NULL;
				streamAvailable = 0;
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] yRtpRecvCount=%d", zLine, yRtpRecvCount);

			}
			else
			{
				yRtpRecvCount = 0;
			}
#endif
		}
		else
		{
	char            yStrTmpDebug[256];


			// DING3
            yRtpRecvCount = arc_rtp_session_recv_in(__func__, zCall, __LINE__, gCall[zCall].rtp_session_in, buffer, gCall[zCall].codecBufferSize, &streamAvailable);



			/*DDN_TODO: Detect lead and trail silcense using same routine 02/22/2010 */
			//sprintf(yStrTmpDebug, "rtp session:%p ts=%d recv count=%d", gCall[zCall].rtp_session_in, gCall[zCall].in_user_ts, yRtpRecvCount);

            //__DDNDEBUG__(DEBUG_MODULE_RTP, "Calling arc_do_inband_detection", yStrTmpDebug, zCall);

			arc_do_inband_detection (buffer, yRtpRecvCount, zCall,
									 (char *) __func__, __LINE__);

			arc_frame_decode_and_post (zCall, buffer,
									   gCall[zCall].codecBufferSize, AUDIO_IN,
									   0, (char *) __func__, __LINE__);
			arc_frame_decode_and_post (zCall, buffer,
									   gCall[zCall].codecBufferSize,
									   AUDIO_MIXED, 0, (char *) __func__,
									   __LINE__);

			if ( ( zMsgToDM->opcode == DMOP_SRRECOGNIZE ) &&
			     ( gCall[zCall].googleUDPFd > -1 ) )
			{
				yMrcpRc = sendto(gCall[zCall].googleUDPFd,
										buffer,
										yRtpRecvCount,
										0,
										(struct sockaddr *) &gCall[zCall].google_si,
										gCall[zCall].google_slen);
							
				__DDNDEBUG__ (DEBUG_MODULE_SR,
						"MRCP: ARCGS: Sent audio packet to Google SR Client", "", yMrcpRc);
			}

			if (gCall[zCall].conferenceStarted == 1)
			{
				arc_conference_decode_and_post (zCall, buffer, bytes, 0,
												(char *) __func__, __LINE__);
			}

			if (!gCall[zCall].isBeepActive)
			{
				if (gCall[zCall].telephonePayloadType > -1)
				{
					dtmf_tone_to_ascii (zCall, buffer, &arcDtmfData[zCall],
										PROCESS_RECORD);
				}
				else
				{
					dtmf_tone_to_ascii (zCall, buffer, &arcDtmfData[zCall],
										PROCESS_DTMF_AND_RECORD);
				}
			}

#if 1
			if ( zMsgToDM->opcode != DMOP_SRRECOGNIZE )
			{
				if (arcDtmfData[zCall].lead_silence_triggered)
				{
					response.opcode = gCall[zCall].msgRecord.opcode;
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "[%d] LEAD SILENCE triggered. Returning -2.", zLine);
					__DDNDEBUG__ (DEBUG_MODULE_RTP, "lead_silence_triggered", "",
								  zCall);
					yRc = -2;
					gCall[zCall].runToneThread = 0; 	// BT-173
					break;
				}
			}

			if (arcDtmfData[zCall].trail_silence_triggered)
			{
				response.opcode = gCall[zCall].msgRecord.opcode;
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] TRAIL SILENCE triggered. Returning -2.", zLine);
				__DDNDEBUG__ (DEBUG_MODULE_RTP, "trail_silence_triggered", "",
							  zCall);
				//yRc = -2;
				yRc = 0;			// BT-83
				gCall[zCall].runToneThread = 0; 	// BT-173
				break;
			}
#endif

		}

		err = err + yRtpRecvCount;

		if (err >= gCall[zCall].codecBufferSize &&
			!streamStarted && !streamAvailable)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "[%d] Setting streamStarted to 1.", zLine);

			streamStarted = 1;
		}

		if (gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
		{
			gCall[zCall].in_user_ts += (gCall[zCall].inTsIncrement * 3);

			gCall[zCall].dtmfAvailable = 0;

//			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
//				"[%d] DJB: interrupt_option=%d,  dtmf_tab[%d] = [%c, %d], terminate_char=[%c, %d]", zLine,
//				gCall[zCall].msgRecord.interrupt_option, gCall[zCall].lastDtmf, 
//				dtmf_tab[gCall[zCall].lastDtmf], dtmf_tab[gCall[zCall].lastDtmf],
//				gCall[zCall].msgRecord.terminate_char, gCall[zCall].msgRecord.terminate_char);
					
			if (gCall[zCall].msgRecord.interrupt_option == INTERRUPT 
				|| gCall[zCall].msgRecord.interrupt_option == FIRST_PARTY_INTERRUPT
				|| gCall[zCall].msgRecord.interrupt_option == 1)
			{
				if (dtmf_tab[gCall[zCall].lastDtmf] == (gCall[zCall].msgRecord.terminate_char) 
					|| gCall[zCall].msgRecord.terminate_char == 32)
				{
//fprintf(stderr, " %s() gCall[zCall].msgRecord.terminate_char==%d", __func__, gCall[zCall].msgRecord.terminate_char);

					gCall[zCall].GV_RecordTermChar =
						dtmf_tab[gCall[zCall].lastDtmf];

					yRc = ACTION_GET_NEXT_REQUEST;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "[%d] RECORD END; calling break.", zLine);

					if ( gCall[zCall].GV_LastCallProgressDetail != ARC_TONES_ACTIVE_SPEAKER )	// BT-226
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
							"[%d] Term char (%c) received before lead silence ended. Must zero-out record file.",
							__LINE__, gCall[zCall].msgRecord.terminate_char);
						gCall[zCall].poundBeforeLeadSilence = 1;
					}

					/*Trim last packet if it is inband DTMF. */
					if (gCall[zCall].telephonePayloadType == -99)
					{
						long   file_len = ftell (infile);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   DYN_BASE, INFO,
								   "[%d] Writing silence buffer in the end. ftell=%ld", zLine,
								   file_len);

						ftruncate (fileno (infile), file_len - (160 * 3));
						clearerr (infile);
						fseek (infile, 0, SEEK_END);
					}

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
							   INFO, "[%d] RECORD END, calling break.", zLine);

					break;
				}
			}

			sprintf (response.message, "%c", dtmf_tab[gCall[zCall].lastDtmf]);

			__DDNDEBUG__ (DEBUG_MODULE_FILE, "Sending DTMF to app", "", -1);

			response.opcode = DMOP_GETDTMF;
			response.appCallNum = zCall;
			response.appPassword = gCall[zCall].appPassword;
			response.appPid = gCall[zCall].appPid;
			response.returnCode = 0;

			if (gCall[zCall].responseFifoFd >= 0)
			{

				if (gCall[zCall].dtmfCacheCount > 0)
				{
	int             i = 0;

					for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
					{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

						sprintf (response.message, "%c",
								 dtmf_tab[dtmfToSend]);

						writeGenericResponseToApp (zCall, &response, mod,
												   __LINE__);
					}
					gCall[zCall].dtmfCacheCount = 0;
				}

				sprintf (response.message, "%c",
						 dtmf_tab[gCall[zCall].lastDtmf]);

				yRc =
					writeGenericResponseToApp (zCall, &response, mod,
											   __LINE__);
			}
		}

		if (!streamStarted && yCurrentTime > yLeadSilence)
		{
			yRc = -2;
			response.opcode = gCall[zCall].msgRecord.opcode;

			sprintf (yErrMsg,
					 "streamStarted(%d), yLeadSilence(%d), yRecordTime(%d) yRc(%d)",
					 streamStarted, yLeadSilence, yRecordTime, yRc);

			__DDNDEBUG__ (DEBUG_MODULE_RTP, "Timed out", yErrMsg, 0);

			break;
		}

		if (gCall[zCall].isBeepActive == 1)
		{
			if ((streamStarted) && (err >= gCall[zCall].codecBufferSize))
			{
				//gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
				//gCall[zCall].in_user_ts+=gCall[zCall].codecBufferSize;
				err = 0;
			}

			yRtpRecvCount = 0;

			rtpSleep (20, &gCall[zCall].lastInRtpTime, __LINE__, zCall);
			continue;
		}

		if (yRtpRecvCount > 0 &&
			gCall[zCall].isIFrameDetected == 1 &&
			gCall[zCall].isBeepActive == 0 && startSavingData == YES)
		{
	int             byteToWrite = yRtpRecvCount;

			if (gCall[zCall].recordOption == WITH_RTP)
			{
	BYTE            l_caLength[2];

				l_caLength[0] = (BYTE) (byteToWrite >> 8);
				l_caLength[1] = (BYTE) byteToWrite;
				i = fwrite (l_caLength, 1, 2, infile);
				if (gCall[zCall].msgRecord.overwrite == APPEND)
				{
					modifyTS (buffer, zCall, byteToWrite);
				}
			}
			else
			{
			}
#if 0
			{
	const unsigned char *l_ptr;

				l_ptr = (const unsigned char *) buffer;
	unsigned long   templastRecordedRtpTs =
		(l_ptr[4] << 24) | (l_ptr[5] << 16) | (l_ptr[6] << 8) | l_ptr[7];

				//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
				//              "[%d] templastRecordedRtpTs = %ld", zLine, templastRecordedRtpTs);
			}
#endif

			i = fwrite (buffer, 1, byteToWrite, infile);
		}
		else if (gCall[zCall].isBeepActive == 0 || startSavingData == NO)
		{
			if (gCall[zCall].isIFrameDetected == 1
				&& gCall[zCall].recordOption != WITH_RTP)
			{
	int             byteToWrite = gCall[zCall].codecBufferSize;

				if (gCall[zCall].recordOption == WITH_RTP)
				{
					byteToWrite += 12;
	BYTE            l_caLength[2];

					l_caLength[0] = (BYTE) (byteToWrite >> 8);
					l_caLength[1] = (BYTE) byteToWrite;
					i = fwrite (l_caLength, 1, 2, infile);
					if (gCall[zCall].msgRecord.overwrite == APPEND)
					{
						modifyTS (buffer, zCall, byteToWrite);
					}
				}

#if 0
				//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
				//  "[%d] Saving Silence Data size=%d", zLine, byteToWrite);
				{
	const unsigned char *l_ptr;

					l_ptr =
						(const unsigned char *) gCall[zCall].silenceBuffer;
	unsigned long   templastRecordedRtpTs =
		(l_ptr[4] << 24) | (l_ptr[5] << 16) | (l_ptr[6] << 8) | l_ptr[7];

					//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
					//          "[%d] templastRecordedRtpTs = %ld", zLine, templastRecordedRtpTs);
				}
#endif
				i = fwrite (gCall[zCall].silenceBuffer, 1, byteToWrite,
							infile);
			}

			//gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
			err = 0;
		}
		else if (gCall[zCall].isBeepActive == 1 ||
				 yRtpRecvCount == 0 || startSavingData == NO)
		{

			if (gCall[zCall].isIFrameDetected == 1
				&& gCall[zCall].recordOption != WITH_RTP)
			{
	int             byteToWrite = gCall[zCall].codecBufferSize;

				if (gCall[zCall].recordOption == WITH_RTP)
				{
					byteToWrite += 12;
	BYTE            l_caLength[2];

					l_caLength[0] = (BYTE) (byteToWrite >> 8);
					l_caLength[1] = (BYTE) byteToWrite;
					i = fwrite (l_caLength, 1, 2, infile);
					if (gCall[zCall].msgRecord.overwrite == APPEND)
					{
						modifyTS (buffer, zCall, byteToWrite);
					}
				}

#if 0
				//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
				//  "[%d] Saving Silence Data size=%d", zLine, byteToWrite);
				{
	const unsigned char *l_ptr;

					l_ptr =
						(const unsigned char *) gCall[zCall].silenceBuffer;
	unsigned long   templastRecordedRtpTs =
		(l_ptr[4] << 24) | (l_ptr[5] << 16) | (l_ptr[6] << 8) | l_ptr[7];

					//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
					//          "[%d] templastRecordedRtpTs = %ld", zLine, templastRecordedRtpTs);
				}
#endif

				i = fwrite (gCall[zCall].silenceBuffer, 1, byteToWrite,
							infile);
			}

			gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
			err = 0;
		}
#if 0
		else
		{

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "[%d] not writing to the file", zLine);
		}
#endif

		if ((streamStarted) && (err >= gCall[zCall].codecBufferSize))
		{
			//gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
			//gCall[zCall].in_user_ts+=gCall[zCall].codecBufferSize;
			err = 0;
		}

		yRtpRecvCount = 0;

		rtpSleep (20, &gCall[zCall].lastInRtpTime, __LINE__, zCall);

	}							/*while(1) */
	gCall[zCall].recordStartedForDisconnect = 0;        // MR 5126
//	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//		"DJB: Set recordStartedForDisconnect to %d",
//		gCall[zCall].recordStartedForDisconnect);

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Writing header", zFileName, 0);

	if (gCall[zCall].recordOption == WITHOUT_RTP)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[%d] Writing wave header to the record file (%s).", zLine, zFileName);

		writeWavHeaderToFile (zCall, infile);
	}
	else
	{
		setLastRecordedRtpTs (buffer, zCall);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "[%d] RECORD END.", zLine);

	fclose (infile);

	if ( gCall[zCall].googleRecordIsActive == 1 )
	{
//		(void)trimStartOfWavFile (zCall, zFileName, 
//							gCall[zCall].utteranceTime1,
//							gCall[zCall].utteranceTime2);
		(void)trimEndOfWavFile (zCall, zFileName, gCall[zCall].msgRecord.trail_silence);
//		(void)trimStartAndEndOfWavFile (zCall, zFileName, 
//					gCall[zCall].utteranceTime1,
//					gCall[zCall].utteranceTime2,
//					gCall[zCall].msgRecord.trail_silence);
		if ( sendRequestToGoogleSRFifo(zCall, __LINE__, GSR_RECORD_REQUEST, zFileName ) < 0 )
		{
	        response.returnCode = -1;
	        sprintf(response.message,  "%s", "ARCGS: Failed to make request to google java client.");
			
	        dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, response.message);
			
	        (void)writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	        return(0);
		}
	}

	if ( (arcDtmfData[zCall].lead_silence_triggered == 1) ||
	     (gCall[zCall].poundBeforeLeadSilence == 1) )			// BT-226
	{
		infile = fopen (zFileName, "w");
		fclose(infile);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[%d] Zero'd out (%s).", zLine, zFileName);
	}
	int		rc;

#if 0
	if ( ( ( zMsgToDM->opcode == DMOP_SRRECOGNIZE )	||
		   ( gCall[zCall].googleRecordIsActive == 1 ) ) &&
		 ( access ("/tmp/gsKeepAudio.txt", F_OK) != 0 ) )
	{
		if ( access (zFileName, F_OK) == 0 )
		{
//			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//	   			"[%d] DJB: deleting (%s)", __LINE__, zFileName);
			rc = unlink (zFileName);
		}
	}
#endif
	gCall[zCall].googleRecordIsActive = 0;

	arcDtmfData[zCall].lead_silence_triggered = 0;
	gCall[zCall].poundBeforeLeadSilence = 0;

	if ( (yRc != ACTION_NEW_REQUEST_AVAILABLE) &&
	     ( zMsgToDM->opcode != DMOP_SRRECOGNIZE ) )
	{
	int             rc;

		response.appCallNum = gCall[zCall].msgRecord.appCallNum;
		response.appPassword = gCall[zCall].msgRecord.appPassword;
		response.appPid = gCall[zCall].msgRecord.appPid;
		response.appRef = gCall[zCall].msgRecord.appRef;
		response.returnCode = (yRc >= 0) ? 0 : yRc;

		sprintf (response.message, "%c", gCall[zCall].GV_RecordTermChar);
		if ( response.returnCode == -3 )
		{
			response.opcode = DMOP_DISCONNECT;
		}

		if (gCall[zCall].isInbandToneDetectionThreadStarted == 1)
		{
			stopInbandToneDetectionThread (__LINE__, zCall);

			int tdCounter = 30;				// BT-173

			while ( ( tdCounter-- != 0 ) &&
			        ( gCall[zCall].detectionTID != -1 ) )
			{
				usleep (1000 * 100);
			}
		}

		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	if (yRc == ACTION_NEW_REQUEST_AVAILABLE)
	{
		yRc = ACTION_GET_NEXT_REQUEST;
	}

//	if (gCall[zCall].isInbandToneDetectionThreadStarted == 1)		// BT-173
//	{
//		stopInbandToneDetectionThread (zCall);
//	}

	gCall[zCall].receivingSilencePackets = 1;
//	gCall[zCall].GV_RecordTermChar = '\0'; 		// BT-226
	gCall[zCall].GV_RecordTermChar = ' ';		// BT-226

	return (yRc);

}								/*END: int recordFile_X */

int
recvFile_X (int zCall, struct MsgToDM *zMsgToDM)
{
	int             yRc = 0;
	char            mod[] = { "recvFile_X" };
	char            buffer[4096];
	char            yErrMsg[256];
	FILE           *infile;
	char            zFileName[255];
	char            file[256];
	int             createFile;
	time_t          yCurrentTime;
	time_t          yRecordTime;
	time_t          yLeadSilence;
	int             i = 0;
	int             err = 0;
	int             streamAvailable = 1;
	int             streamStarted = 0;
	struct MsgToApp response;
	int             yMrcpRc = 0;
	int             yMrcpFailureCount = 0;

	memset (yErrMsg, 0, sizeof (yErrMsg));
	response.message[0] = '\0';

	gCall[zCall].receivingSilencePackets = 0;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "FAX RECEIVE START.");

	time (&yRecordTime);
	time (&yCurrentTime);

	infile = fopen (gCall[zCall].msgFax.faxFile, "w");

	if (infile == NULL)
	{
	int             rc;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "FAX RECEIVE END.");

		response.opcode = gCall[zCall].msgFax.opcode;
		response.appCallNum = gCall[zCall].msgFax.appCallNum;
		response.appPassword = gCall[zCall].msgFax.appPassword;
		response.appPid = gCall[zCall].msgFax.appPid;
		response.appRef = gCall[zCall].msgFax.appRef;
		response.returnCode = -7;

		sprintf (response.message, "\0");

		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

		gCall[zCall].receivingSilencePackets = 1;

		return (-7);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "FAX RECEIVE NOT EMPTY", 0);

	sprintf (response.message, "\0");

	response.opcode = gCall[zCall].msgFax.opcode;

	while (1)
	{
		time (&yCurrentTime);

		if (!canContinue (mod, zCall, __LINE__))
		{
			yRc = -3;
			break;
		}
		else if (gCall[zCall].pendingInputRequests > 0)
		{
			yRc = ACTION_GET_NEXT_REQUEST;
			break;
		}
		else if (yCurrentTime > yRecordTime)
		{
			yRc = ACTION_GET_NEXT_REQUEST;
			break;
		}

		memset (buffer, 0, 4096);

		err = rtp_session_recv_with_ts (gCall[zCall].rtp_session_in,
										buffer,
										gCall[zCall].codecBufferSize,
										gCall[zCall].in_user_ts,
										&streamAvailable, 0, NULL);

		if (err >= gCall[zCall].codecBufferSize &&
			!streamStarted && !streamAvailable)
		{
			streamStarted = 1;
		}

		if (!streamStarted && yCurrentTime > yLeadSilence)
		{
			yRc = -2;

			sprintf (yErrMsg,
					 "streamStarted(%d), yLeadSilence(%d), yRecordTime(%d) yRc(%d)",
					 streamStarted, yLeadSilence, yRecordTime, yRc);

			__DDNDEBUG__ (DEBUG_MODULE_RTP, "FAX RECIEVE Timed out", yErrMsg,
						  0);

			break;
		}

		if ((streamStarted) && (err >= gCall[zCall].codecBufferSize))
		{
			i = fwrite (buffer, 1, err, infile);

			gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
			//gCall[zCall].in_user_ts+=gCall[zCall].codecBufferSize;
		}

		rtpSleep (20, &gCall[zCall].lastInRtpTime, __LINE__, zCall);

	}							/*while(1) */

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Writing header", zFileName, 0);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "RECORD END.");

	fclose (infile);

	{
	int             rc;

		response.appCallNum = gCall[zCall].msgFax.appCallNum;
		response.appPassword = gCall[zCall].msgFax.appPassword;
		response.appPid = gCall[zCall].msgFax.appPid;
		response.appRef = gCall[zCall].msgFax.appRef;
		response.returnCode = (yRc >= 0) ? 0 : yRc;

		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	gCall[zCall].receivingSilencePackets = 1;

	return (yRc);

}								/*END: int recvFile_X */

///This function is used by the ouputThread to speak files, using speakFile() as well as send silence packets.
int
speakFile_X (int zLine, int zCall, int zAppRef, struct MsgToDM *zMsgToDM,
			 int *zpTermReason)
{
	int             yRc = 0;
	int             yRtpRc = 0;
	char            mod[] = { "speakFile_X" };
	FILE           *fp;
	char            zFileName[255];
	char            file[256];
	int             yIntRtpFailCount = 0;
	int             silenceFd = -1;

	struct Msg_Speak yTmpMsgSpeak;
	struct Msg_Speak *lpMsgSpeak = &yTmpMsgSpeak;

	struct MsgToApp response;

	char            yTmpBuffer4K[160 * 40 + 1] = "";
	char            buffer[160 * 40 + 1] = "";
	int             yIntRemainingBytes = 0;
	int             yIntTotalSize = 0;
	int             yIntStreamingFinished = 0;
	int             i = 0;
	int             streamAvailable = 1;
	guint32         ts = 0;

	response.message[0] = '\0';

	sprintf (zFileName, "%s", "\0");

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Inside speakFile_X", zLine);

	if (zMsgToDM == NULL)
	{

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "SEND EMPTY", 0);

		yRc = 0;
		yRtpRc = 0;

		memset (yTmpBuffer4K, 0, sizeof (yTmpBuffer4K));
		memset (buffer, 0, sizeof (buffer));
		memset (buffer, 0xff, sizeof (buffer) - 1);

		if (gCall[zCall].codecType == 18)
		{
			memcpy (buffer, gCall[zCall].silenceBuffer, 160);
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "[%d] Opening silenceFile (%s).", zLine,  gCall[zCall].silenceFile);

			silenceFd =
				arc_open (zCall, mod, __LINE__, gCall[zCall].silenceFile,
						  O_RDONLY, ARC_TYPE_FILE);
		}

		gCall[zCall].sendingSilencePackets = 1;

#ifdef ACU_LINUX
		gCall[zCall].conf_in_user_ts = 0;
#endif

		while (1)
		{
			if (!canContinue (mod, zCall, __LINE__))
			{
				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "Can not continue", -3);

				yRc = -3;
				if (silenceFd > -1)
				{
					arc_close (zCall, mod, __LINE__, &silenceFd);
					silenceFd = -1;
				}

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] Returning from speakFile_X", zLine);

				break;
			}

			if (gCall[zCall].isSendRecvFaxActive == 1)
			{
				rtpSleep (200, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);
				continue;
			}

			if (gCall[zCall].waitForPlayEnd == 1)
			{
				gCall[zCall].waitForPlayEnd = 0;

				if (gCall[zCall].responseFifoFd >= 0)
				{
					writeGenericResponseToApp (zCall,
											   &(gCall[zCall].
												 msgPortOperation), mod,
											   __LINE__);
				}
			}

//          dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//              "[%d] DJB: gCall[%d].dtmfAvailable=%d, "
//              "gCall[%d].lastDtmf=%d, "
//              "gCall[%d].recordStarted=%d, "
//              "gCall[%d].speechRecFromClient=%d.", zLine, 
//              zCall, gCall[zCall].dtmfAvailable,
//              zCall, gCall[zCall].lastDtmf,
//              zCall, gCall[zCall].recordStarted,
//              zCall, gCall[zCall].speechRecFromClient);

			if (gCall[zCall].dtmfAvailable == 1 &&
				gCall[zCall].lastDtmf < 16 &&
				gCall[zCall].recordStarted == 0 &&
				!gCall[zCall].speechRecFromClient &&
				gCall[zCall].rtp_session_mrcp == NULL)
			{
				gCall[zCall].in_user_ts += (gCall[zCall].inTsIncrement * 3);
				//gCall[zCall].in_user_ts+=(gCall[zCall].codecBufferSize*3);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] Setting dtmfAvailable to 0.", zLine);
				gCall[zCall].dtmfAvailable = 0;

				sprintf (response.message, "%c",
						 dtmf_tab[gCall[zCall].lastDtmf]);

				__DDNDEBUG__ (DEBUG_MODULE_FILE, "Sending DTMF to app", "",
							  -1);

				response.opcode = DMOP_GETDTMF;
				response.appCallNum = zCall;
				response.appPassword = gCall[zCall].appPassword;
				response.appPid = gCall[zCall].appPid;
				response.returnCode = 0;

				if (gCall[zCall].leg == LEG_B)
				{
	int             yIntALeg = gCall[zCall].crossPort;

					if (yIntALeg >= 0 && gCall[yIntALeg].responseFifoFd >= 0)
					{
						if (gCall[zCall].dtmfCacheCount > 0)
						{
	int             i = 0;

							for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
							{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

								sprintf (response.message, "%c",
										 dtmf_tab[dtmfToSend]);

								writeGenericResponseToApp (zCall, &response,
														   mod, __LINE__);
							}
							gCall[zCall].dtmfCacheCount = 0;
						}
						sprintf (response.message, "%c",
								 dtmf_tab[gCall[zCall].lastDtmf]);
						yRc =
							writeGenericResponseToApp (zCall, &response, mod,
													   __LINE__);
					}
				}
				else if (gCall[zCall].responseFifoFd >= 0)
				{
					if (gCall[zCall].dtmfCacheCount > 0)
					{
	int             i = 0;

						for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
						{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

							sprintf (response.message, "%c",
									 dtmf_tab[dtmfToSend]);

							writeGenericResponseToApp (zCall, &response, mod,
													   __LINE__);
						}
						gCall[zCall].dtmfCacheCount = 0;
					}
					sprintf (response.message, "%c",
							 dtmf_tab[gCall[zCall].lastDtmf]);
					yRc =
						writeGenericResponseToApp (zCall, &response, mod,
												   __LINE__);
					gCall[zCall].dtmfCacheCount = 0;
				}
			}

			if (silenceFd > -1)
			{
				if (yIntRemainingBytes >= gCall[zCall].codecBufferSize)
				{
					memcpy (buffer,
							yTmpBuffer4K + (yIntTotalSize -
											yIntRemainingBytes),
							gCall[zCall].codecBufferSize);

					yIntRemainingBytes -= gCall[zCall].codecBufferSize;

					i = gCall[zCall].codecBufferSize;
				}
				else if (yIntRemainingBytes <= 0)
				{

					if (yIntStreamingFinished == 1)
					{
						lseek (silenceFd, SEEK_SET, 0);
						yIntRemainingBytes = 0;
						yIntTotalSize = 0;
						yIntStreamingFinished = 0;
						i = 0;
						continue;
					}

					yIntTotalSize = read (silenceFd, yTmpBuffer4K, 4096);

					if (yIntTotalSize <= 0)
					{
						lseek (silenceFd, SEEK_SET, 0);
						yIntRemainingBytes = 0;
						yIntTotalSize = 0;
						yIntStreamingFinished = 0;
						i = 0;
						continue;
					}

					yIntRemainingBytes = yIntTotalSize;

					if (yIntTotalSize < 4096)
					{
						yIntStreamingFinished = 1;
					}

					continue;

				}
				else
				{
					memcpy (buffer,
							yTmpBuffer4K + (yIntTotalSize -
											yIntRemainingBytes),
							yIntRemainingBytes);

					i = yIntRemainingBytes;

					yIntRemainingBytes = 0;

					yIntTotalSize = read (silenceFd, yTmpBuffer4K, 4096);

					memcpy (buffer + i, yTmpBuffer4K,
							gCall[zCall].codecBufferSize - i);

					yIntRemainingBytes =
						yIntTotalSize - (gCall[zCall].codecBufferSize -
										 yIntRemainingBytes - i);

					if (yIntTotalSize < 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "[%d] Returning from speakFile_X; yIntTotalSize=%d.", zLine, 
								   yIntTotalSize);
						break;
					}
					if (yIntTotalSize == 0)
					{
						yIntRemainingBytes = 0;
						yIntStreamingFinished = 1;
						continue;
					}

					if (yIntTotalSize < 4096)
					{
						yIntStreamingFinished = 1;
					}
				}
			}

			if (gCall[zCall].pendingOutputRequests > 0)
			{

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "pendingOutputRequests",
							  gCall[zCall].pendingOutputRequests);
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] pendingOutputRequests > 0.", zLine);

				if (silenceFd > -1)
				{
					arc_close (zCall, mod, __LINE__, &silenceFd);
					silenceFd = -1;
				}

				yRc = ACTION_GET_NEXT_REQUEST;

				break;
			}

			if (gCall[zCall].rtp_session != NULL &&
				(gCall[zCall].callState != CALL_STATE_CALL_BRIDGED &&
				 gCall[zCall].callSubState != CALL_STATE_CALL_MEDIACONNECTED))
			{

				if ((gCall[zCall].codecType == 18
					 && gCall[zCall].isG729AnnexB == YES)
					|| (strcmp (gCall[zCall].audioCodecString, "AMR") == 0))
				{
					util_sleep (0, 20);
				}
				else if (gCall[zCall].conferenceStarted == 1)
				{
					yRc = arc_conference_read_and_encode (zCall, buffer,
														  gCall[zCall].
														  codecBufferSize, 0,
														  (char *) __func__,
														  __LINE__);

					if (yRc > 0)
					{

						yRtpRc =
							arc_rtp_session_send_with_ts (mod, __LINE__,
														  zCall,
														  gCall[zCall].
														  rtp_session, buffer,
														  gCall[zCall].
														  codecBufferSize,
														  gCall[zCall].
														  out_user_ts);
						if (yRtpRc == 0)
						{
							//arc_frame_apply(zCall, gSilenceBuffer, 160, AUDIO_MIXED, 
							//  ARC_AUDIO_PROCESS_AUDIO_MIX, (char *)__func__, __LINE__);
						}
						else
						{
							arc_frame_apply (zCall, buffer,
											 gCall[zCall].codecBufferSize,
											 AUDIO_MIXED,
											 ARC_AUDIO_PROCESS_AUDIO_MIX,
											 (char *) __func__, __LINE__);
						}
					}
				}
				else if (gCall[zCall].codecType != 0)
				{

					yRtpRc =
						arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													  gCall[zCall].
													  rtp_session, buffer,
													  gCall[zCall].
													  codecBufferSize,
													  gCall[zCall].
													  out_user_ts);

					//gCall[zCall].out_user_ts+=(gCall[zCall].outTsIncrement);

				}
				else if (gCall[zCall].codecType == 0
						 || gCall[zCall].codecType == 8)
				{
#if 1
	char            silence320[320];

					memset (silence320, 0, 320);

					arc_audio_frame_post (&gCall[zCall].
										  audio_frames[AUDIO_OUT], silence320,
										  320, 0);
					arc_audio_frame_apply (&gCall[zCall].
										   audio_frames[AUDIO_MIXED],
										   silence320, 320,
										   ARC_AUDIO_PROCESS_AUDIO_MIX);

					if (gCall[zCall].GV_CallProgress != 0)
					{
						/*DDN: 20101104 Had to send silence data since some ISP don't send audio back without receiving audio first */
						yRtpRc =
							arc_rtp_session_send_with_ts (mod, __LINE__,
														  zCall,
														  gCall[zCall].
														  rtp_session, buffer,
														  gCall[zCall].
														  codecBufferSize,
														  gCall[zCall].
														  out_user_ts);
					}
#endif
				}
			}
			else if (gCall[zCall].rtp_session != NULL &&
					 (gCall[zCall].callState == CALL_STATE_CALL_BRIDGED ||
					  gCall[zCall].callSubState ==
					  CALL_STATE_CALL_MEDIACONNECTED))
			{
				yRtpRc = 160;
				//BT-134 Send one packet to reset output rtp session
				if (gCall[zCall].restart_rtp_session == 1)
				{
					yRtpRc =
						arc_rtp_session_send_with_ts (mod, __LINE__,
												  zCall,
												  gCall[zCall].
												  rtp_session, buffer,
												  gCall[zCall].
												  codecBufferSize,
												  gCall[zCall].
												  out_user_ts);
				}
			}

			if (yRtpRc < 0)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] yRtpRc=%d", zLine, yRtpRc);

				yIntRtpFailCount++;

				if (yIntRtpFailCount > 10)
				{

					__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "yRtpRc < 0",
								  "Returning -3",
								  gCall[zCall].pendingOutputRequests);

					if (silenceFd > -1)
					{
						arc_close (zCall, mod, __LINE__, &silenceFd);
						silenceFd = -1;
					}

					util_sleep (0, 100);
					yRc = -3;
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "[%d] Returning from speakFile_X; yIntRtpFailCount=%d.", zLine,
							   yIntRtpFailCount);

					break;
				}

				rtpSleep (20, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);
				continue;
			}

			yIntRtpFailCount = 0;

			gCall[zCall].out_user_ts += (gCall[zCall].outTsIncrement);

			if (gCall[zCall].conferenceStarted != 1)
			{
				rtpSleep (20, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);
			}

		}						//while

		gCall[zCall].sendingSilencePackets = 0;

		if (silenceFd > -1)
		{
			arc_close (zCall, mod, __LINE__, &silenceFd);
			silenceFd = -1;
		}

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[%d] Returning from speakFile_X.", zLine);

		return (yRc);

	}							/*END: if(zMsgToDM == NULL) */

#if 0
	fclose (tmpfp);
#endif

	gCall[zCall].firstSpeakStarted++;

	memcpy (&yTmpMsgSpeak, (struct Msg_Speak *) zMsgToDM,
			sizeof (struct Msg_Speak));

	gCall[zCall].currentSpeak = &yTmpMsgSpeak;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] gCall[%d].currentOpcode=%d.", zLine,  zCall,
			   gCall[zCall].currentOpcode);

	if (gCall[zCall].currentOpcode == DMOP_PLAYMEDIAAUDIO)
	{
		response.opcode = DMOP_PLAYMEDIA;
		gCall[zCall].currentOpcode = DMOP_PLAYMEDIA;
	}
	else if (gCall[zCall].currentOpcode == DMOP_SPEAKMRCPTTS)
	{
		response.opcode = DMOP_SPEAKMRCPTTS;
		gCall[zCall].currentOpcode = DMOP_SPEAKMRCPTTS;
	}
	else if (gCall[zCall].currentOpcode != DMOP_SPEAK)
	{
		response.opcode = DMOP_PLAYMEDIA;
	}
	else
	{
		response.opcode = DMOP_SPEAK;
	}

	if (!lpMsgSpeak->file[0])
	{
		switch (lpMsgSpeak->synchronous)
		{
		case SYNC:
		case PLAY_QUEUE_SYNC:
	int             rc;

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearSpeakList", "",
						  -1);
			clearSpeakList (zCall, __LINE__);

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearAppRequestList",
						  "", -1);

			clearAppRequestList (zCall);

			response.appCallNum = lpMsgSpeak->appCallNum;
			response.appPassword = lpMsgSpeak->appPassword;
			response.appPid = lpMsgSpeak->appPid;
			response.appRef = gCall[zCall].currentAudio.appRef;
			response.returnCode = (yRc >= 0) ? 0 : yRc;

			if (gCall[zCall].waitForPlayEnd == 1)
			{
				gCall[zCall].waitForPlayEnd = 0;

				if (gCall[zCall].responseFifoFd >= 0)
				{
					writeGenericResponseToApp (zCall,
											   &(gCall[zCall].
												 msgPortOperation), mod,
											   __LINE__);
				}
			}
			else if (gCall[zCall].responseFifoFd >= 0)
			{
				if (gCall[zCall].msgToDM.opcode == DMOP_TTS_COMPLETE_FAILURE)
				{
					response.returnCode = -1;
				}
				rc = writeGenericResponseToApp (zCall, &response, mod,
												__LINE__);
			}

			break;

		case PLAY_QUEUE_ASYNC:

			if (gCall[zCall].waitForPlayEnd == 1)
			{
				gCall[zCall].waitForPlayEnd = 0;

				if (gCall[zCall].responseFifoFd >= 0)
				{
					writeGenericResponseToApp (zCall,
											   &(gCall[zCall].
												 msgPortOperation), mod,
											   __LINE__);
				}
			}

			if (gCall[zCall].speechRecFromClient == 1)
			{
	struct MsgToDM  yMsgToDM;

				memset (&yMsgToDM, 0, sizeof (struct MsgToDM));

				memcpy (&yMsgToDM, zMsgToDM, sizeof (struct MsgToDM));

				yMsgToDM.opcode = DMOP_SRPROMPTEND;

				sprintf (yMsgToDM.data, "%s", "\0");

				sendMsgToSRClient (__LINE__, zCall, &yMsgToDM);	// ding4

				gCall[zCall].canBargeIn = 1;
			}

#if 0
			if (gCall[zCall].googleStreamingSR == 1)
			{
				if ( (rc = sendRequestToGoogleSRFifo(zCall, __LINE__, GSR_STREAMING_REQUEST, "")) < 0 )
				{
			        response.returnCode = -1;
			        sprintf(response.message,  "%s", "ARCGS: Failed to make request to google java client.");
			
			        dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, response.message);
			
			        rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			        return(0);
				}
				gCall[zCall].canBargeIn = 1;
			}
#endif

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearSpeakList", "",
						  -1);
			clearSpeakList (zCall, __LINE__);

			break;

		default:
			break;
		}

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "[%d] Returning from speakFile_X.", zLine);
		return (0);
	}

	gCall[zCall].sendingSilencePackets = 0;

	if (lpMsgSpeak->opcode == DMOP_SPEAKMRCPTTS)
	{
		gCall[zCall].canBargeIn =
			(lpMsgSpeak->interruptOption == NONINTERRUPT) ? 0 : 1;

		gCall[zCall].keepSpeakingMrcpTTS = 1;
		yRc = processSpeakMrcpTTS (zCall, zMsgToDM, zpTermReason,
								   &response, lpMsgSpeak->interruptOption);
	}
	else if (lpMsgSpeak->list == 0)
	{
		sprintf (zFileName, "%s", lpMsgSpeak->file);

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Playing File", lpMsgSpeak->file,
					  zAppRef);

		if (strncmp (zFileName, "dtmf://", 7) == 0)
		{

			yRc = speakDtmf (gCall[zCall].codecSleep,
							 lpMsgSpeak->interruptOption,
							 zFileName,
							 lpMsgSpeak->synchronous,
							 zCall,
							 zAppRef,
							 gCall[zCall].rtp_session,
							 zpTermReason, &response);

		}
		else
		{

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "[%d] Calling speakFile() with interruptOption=%d.", zLine,
					   lpMsgSpeak->interruptOption);

			gCall[zCall].canBargeIn =
				(lpMsgSpeak->interruptOption == NONINTERRUPT) ? 0 : 1;

			yRc = speakFile (gCall[zCall].codecSleep,
							 lpMsgSpeak->interruptOption,
							 zFileName,
							 lpMsgSpeak->synchronous,
							 zCall,
							 zAppRef,
							 gCall[zCall].rtp_session,
							 zpTermReason, &response);

			clearSpeakList (zCall, __LINE__);
		}

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Done playing File",
					  lpMsgSpeak->file, 0);
	}
	else
	{
		/*DDN: 07012011: Inband DTMF: If it is of type list and list file starts with "dtmf://", it is INBAND DTMF */
		if (strncmp (lpMsgSpeak->file, "dtmf://", 7) == 0)
		{
			fp = fopen (&(lpMsgSpeak->file[7]), "r");
			sprintf (zFileName, lpMsgSpeak->file);	//DDN: 07012011 This is done so that opcode will be set back to DMOP_OUTPUT_DTMF after the list is played.

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
					   "[%d] INBAND DTMF: List file name=%s", zLine,
					   &(lpMsgSpeak->file[7]));

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
						  "OUTPUT_DTMF_INBAND: List file: ",
						  &(lpMsgSpeak->file[7]), zCall);
		}
		else
		{
			fp = fopen (lpMsgSpeak->file, "r");
		}

		if (fp == NULL)
		{
			strcpy (file, lpMsgSpeak->file);
			yRc = -7;
		}
		else
			while (fgets (file, 512, fp) != NULL)
			{
				file[strlen (file) - 1] = '\0';

				if (file[0] == '#')
				{
					continue;
				}

				if (strcmp (file, "") == 0)
				{
					continue;
				}

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Playing File",
							  lpMsgSpeak->file, zAppRef);

				if (strncmp (file, "dtmf://", 7) == 0)
				{

					yRc = speakDtmf (gCall[zCall].codecSleep,
									 lpMsgSpeak->interruptOption,
									 file,
									 lpMsgSpeak->synchronous,
									 zCall,
									 zAppRef,
									 gCall[zCall].rtp_session,
									 zpTermReason, &response);

				}
				else
				{
					yRc = speakFile (gCall[zCall].codecSleep,
									 lpMsgSpeak->interruptOption,
									 file,
									 lpMsgSpeak->synchronous,
									 zCall,
									 zAppRef,
									 gCall[zCall].rtp_session,
									 zpTermReason, &response);

				}

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Done playing File",
							  lpMsgSpeak->file, 0);

				if (yRc != 0)
				{
					break;
				}
			}

		if (fp != NULL)
		{
			fclose (fp);
			fp = NULL;
		}
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] zFileName=(%s), setting isBeepActive to 0.", zLine, zFileName);

	gCall[zCall].isBeepActive = 0;

	switch (lpMsgSpeak->synchronous)
	{
	case PLAY_QUEUE_ASYNC:
	case PLAY_QUEUE_SYNC:
	case ASYNC:
	case SYNC:
		// FAX Message, send this to API
#if 0
		if (lpMsgSpeak->synchronous == SYNC
			&& (strcmp (lpMsgSpeak->key, "faxtone") == 0))
		{
//			fprintf (stderr,
//					 " %s() sending message back to fax API that tone has ended\n",
//					 __func__);
	struct Msg_Fax_PlayTone msgPlayTone;

			memset (&msgPlayTone, 0, sizeof (msgPlayTone));
			memcpy (&msgPlayTone, &(gCall[zCall].msgPortOperation),
					sizeof (msgPlayTone));
			msgPlayTone.opcode = DMOP_FAX_PLAYTONE;
			writeGenericResponseToApp (zCall,
									   (struct MsgToApp *) &msgPlayTone,
									   (char *) __func__, __LINE__);
			gCall[zCall].sentFaxPlaytone = 1;
			break;
		}
#endif
		if (gCall[zCall].waitForPlayEnd == 1)
		{
			gCall[zCall].waitForPlayEnd = 0;

			if ((gCall[zCall].responseFifoFd >= 0) &&
				(lpMsgSpeak->opcode != DMOP_SPEAKMRCPTTS))
			{
				writeGenericResponseToApp (zCall,
										   &(gCall[zCall].msgPortOperation),
										   mod, __LINE__);
			}
		}
		break;

	default:
		break;
	}

	if (gCall[zCall].speechRec == 0)
	{
		switch (lpMsgSpeak->synchronous)
		{
		case PUT_QUEUE:
		case PUT_QUEUE_ASYNC:
			//case ASYNC:

			if (*zpTermReason == TERM_REASON_DEV &&
				canContinue (mod, zCall, __LINE__))
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] TERM_REASON_DEV && canContinue.", zLine);
				break;
			}

			if (*zpTermReason == TERM_REASON_DTMF)
			{
				response.opcode = DMOP_GETDTMF;
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "[%d] Setting dtmfAvailable to 0.", zLine);
				gCall[zCall].dtmfAvailable = 0;
			}

			response.appCallNum = lpMsgSpeak->appCallNum;
			response.appPassword = lpMsgSpeak->appPassword;
			response.appPid = lpMsgSpeak->appPid;
			response.appRef = lpMsgSpeak->appRef;
			response.returnCode = (yRc >= 0) ? 0 : yRc;

			if (gCall[zCall].responseFifoFd >= 0)
			{
				if (gCall[zCall].dtmfCacheCount > 0 &&
					gCall[zCall].rtp_session_mrcp == NULL)
				{
	int             i = 0;
	char            yStrSavedResponse[32];

					sprintf (yStrSavedResponse, "%s", response.message);

					for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
					{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

						sprintf (response.message, "%c",
								 dtmf_tab[dtmfToSend]);

						writeGenericResponseToApp (zCall, &response, mod,
												   __LINE__);
					}
					gCall[zCall].dtmfCacheCount = 0;
					sprintf (response.message, "%s", yStrSavedResponse);
				}

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			}

			break;

		case SYNC:
		case PLAY_QUEUE_SYNC:
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "[%d] SYNC / PLAY_QUEUE_SYNC, termReason(%d).", zLine,
					   *zpTermReason);
			response.returnCode = (yRc >= 0) ? 0 : yRc;
			response.appCallNum = lpMsgSpeak->appCallNum;
			response.appPassword = lpMsgSpeak->appPassword;
			response.appPid = lpMsgSpeak->appPid;
			response.appRef = lpMsgSpeak->appRef;

			if (gCall[zCall].dtmfCacheCount > 0 &&
				gCall[zCall].rtp_session_mrcp == NULL)
			{
	int             i = 0;
	char            yStrSavedResponse[32];

				sprintf (yStrSavedResponse, "%s", response.message);
				response.opcode = DMOP_GETDTMF;
				response.returnCode = 0;

				for (i = 0; i < gCall[zCall].dtmfCacheCount; i++)
				{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

					sprintf (response.message, "%c", dtmf_tab[dtmfToSend]);

					writeGenericResponseToApp (zCall, &response, mod,
											   __LINE__);
				}
				gCall[zCall].dtmfCacheCount = 0;
				sprintf (response.message, "%s", yStrSavedResponse);
			}

			//response.opcode       = lpMsgSpeak->opcode;
			if (*zpTermReason == TERM_REASON_DTMF)
			{
				response.opcode = DMOP_GETDTMF;

				sprintf (response.message, "%c",
						 dtmf_tab[gCall[zCall].lastDtmf]);

				__DDNDEBUG__ (DEBUG_MODULE_FILE, "Sending DTMF",
							  response.message, gCall[zCall].lastDtmf);

			}

			if (strstr (zFileName, "dtmf://") != NULL)
			{
				response.opcode = DMOP_OUTPUTDTMF;

				if (lpMsgSpeak->list == 1)	/*Delete temp file created for INBAND DTMF. */
				{
					if (access (&(lpMsgSpeak->file[7]), F_OK) == 0)
					{
						unlink (&(lpMsgSpeak->file[7]));
					}
				}
			}

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Setting opcode to", "",
						  response.opcode);

			response.appCallNum = lpMsgSpeak->appCallNum;
			//response.opcode           = lpMsgSpeak->opcode;
			response.appPassword = lpMsgSpeak->appPassword;
			response.appPid = lpMsgSpeak->appPid;
			response.appRef = lpMsgSpeak->appRef;

			if (gCall[zCall].leg == LEG_B)
			{
	int             yIntALeg = gCall[zCall].crossPort;

				if (yIntALeg >= 0 && gCall[yIntALeg].responseFifoFd >= 0)
				{
					writeGenericResponseToApp (yIntALeg,
											   &response, mod, __LINE__);
				}
				__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearSpeakList", "",
							  -1);
				clearSpeakList (zCall, __LINE__);

				__DDNDEBUG__ (DEBUG_MODULE_CALL,
							  "Calling clearAppRequestList", "", -1);
				clearAppRequestList (zCall);
			}
			else
			{
				if (gCall[zCall].pFirstSpeak != NULL)
				{
					gCall[zCall].pFirstSpeak->isSpeakDone = YES;
				}

				if (gCall[zCall].responseFifoFd >= 0)
				{
						__DDNDEBUG__ (DEBUG_MODULE_CALL,
									  "Calling clearSpeakList", "", -1);
						clearSpeakList (zCall, __LINE__);

						__DDNDEBUG__ (DEBUG_MODULE_CALL,
									  "Calling clearAppRequestList", "", -1);
						clearAppRequestList (zCall);

						if (response.opcode == DMOP_SPEAKMRCPTTS)
						{
							if (*zpTermReason == TERM_REASON_DTMF)
							{
								sprintf (response.message, "%c^%s",
										 dtmf_tab[gCall[zCall].lastDtmf],
										 ((struct Msg_SpeakMrcpTts *)
										  zMsgToDM)->file);
							}
							else
							{
								if (gCall[zCall].msgToDM.opcode ==
									DMOP_TTS_COMPLETE_TIMEOUT)
								{
									response.returnCode = -2;
								}
								if (gCall[zCall].msgToDM.opcode ==
									DMOP_TTS_COMPLETE_FAILURE)
								{
									response.returnCode = -1;
								}
								sprintf (response.message, "%s",
										 ((struct Msg_SpeakMrcpTts *)
										  zMsgToDM)->file);
							}
						}
	int             respOpCode = response.opcode;
	char            message[256] = "";

						sprintf (message, "%s", response.message);
						if (lpMsgSpeak->opcode == DMOP_SPEAKMRCPTTS &&
							lpMsgSpeak->opcode != response.opcode)
						{
							response.opcode = DMOP_SPEAKMRCPTTS;
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "[%d] Calling writeGenericResponseToApp msg=(%s).", zLine,
									   response.message);
							memset (response.message, 0,
									sizeof (response.message));
							writeGenericResponseToApp (zCall, &response, mod,
													   __LINE__);
						}
						response.opcode = respOpCode;
						// this is a fax tone 
						if ((lpMsgSpeak->key[0] != '\0')
							&& (strcmp (lpMsgSpeak->key, "faxtone") == 0))
						{
							response.opcode = DMOP_FAX_PLAYTONE;
						}
						sprintf (response.message, "%s", message);
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "[%d] Calling writeGenericResponseToApp msg=(%s).", zLine,
								   response.message);
						writeGenericResponseToApp (zCall, &response, mod,
												   __LINE__);
				}
				else
				{
					__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearSpeakList",
								  "", -1);
					clearSpeakList (zCall, __LINE__);

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Calling clearAppRequestList", "", -1);
					clearAppRequestList (zCall);
					if (response.opcode == DMOP_SPEAKMRCPTTS)
					{
						if (*zpTermReason == TERM_REASON_DTMF)
						{
							sprintf (response.message, "%c^%s",
									 dtmf_tab[gCall[zCall].lastDtmf],
									 ((struct Msg_SpeakMrcpTts *) zMsgToDM)->
									 file);
						}
						else
						{
							if (gCall[zCall].msgToDM.opcode ==
								DMOP_TTS_COMPLETE_TIMEOUT)
							{
								response.returnCode = -2;
							}
							if (gCall[zCall].msgToDM.opcode ==
								DMOP_TTS_COMPLETE_FAILURE)
							{
								response.returnCode = -1;
							}
							sprintf (response.message, "%s",
									 ((struct Msg_SpeakMrcpTts *) zMsgToDM)->
									 file);
						}
					}
					writeGenericResponseToApp (zCall,
											   &response, mod, __LINE__);
				}

			}

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearAppRequestList",
						  "", -1);

			break;

		default:
			break;
		}
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "[%d] Returning from speakFile_X().", zLine);

	return (yRc);

}								/*int speakFile_X */

int
sendFile_X (int zCall, int zAppRef, struct MsgToDM *zMsgToDM,
			int *zpTermReason)
{
	int             yRc = 0;
	int             yRtpRc = 0;
	char            mod[] = { "sendFile_X" };
	char            buffer[160];
	char            header[161];
	FILE           *infile;
	FILE           *fp;
	char            zFileName[255];
	char            file[256];
	int             yIntRtpFailCount = 0;

	struct Msg_SendFax yTmpMsgSendFax;
	struct Msg_SendFax *lpMsgSendFax = &yTmpMsgSendFax;

	struct MsgToApp response;

	response.message[0] = '\0';

	sprintf (zFileName, "%s", "\0");

	memset (buffer, 0, 160);

	if (zMsgToDM == NULL)
	{

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "SEND FAX EMPTY", 0);

		yRc = 0;
		yRtpRc = 0;

		gCall[zCall].sendingSilencePackets = 1;

		while (1)
		{
			if (!canContinue (mod, zCall, __LINE__))
			{

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "Can not continue", -3);

				yRc = -3;
				break;
			}

			if (gCall[zCall].pendingOutputRequests > 0)
			{
				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "pendingOutputRequests",
							  gCall[zCall].pendingOutputRequests);
				yRc = ACTION_GET_NEXT_REQUEST;
				break;
			}

			yIntRtpFailCount = 0;

			gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;

#if 0
			arcDynamicSleep (gCall[zCall].codecSleep,
							 &gCall[zCall].m_dwPrevGrabTime_out,
							 &gCall[zCall].m_dwExpectGrabTime_out);
#endif

			rtpSleep (20, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);

		}

		gCall[zCall].sendingSilencePackets = 0;

		return (yRc);

	}

	memcpy (&yTmpMsgSendFax, (struct Msg_SendFax *) zMsgToDM,
			sizeof (struct Msg_SendFax));

	response.opcode = lpMsgSendFax->opcode;

	if (!lpMsgSendFax->faxFile[0])
	{
	int             rc;

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearSpeakList", "", -1);
		clearSpeakList (zCall, __LINE__);

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearAppRequestList", "",
					  -1);

		clearAppRequestList (zCall);

		response.appCallNum = lpMsgSendFax->appCallNum;
		response.appPassword = lpMsgSendFax->appPassword;
		response.appPid = lpMsgSendFax->appPid;
		response.appRef = lpMsgSendFax->appRef;
		response.returnCode = (yRc >= 0) ? 0 : yRc;

		if (gCall[zCall].responseFifoFd >= 0)
		{
			rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		}

		return (0);
	}

	gCall[zCall].sendingSilencePackets = 0;

	fp = fopen (lpMsgSendFax->faxFile, "r");

	if (fp == NULL)
	{
		strcpy (file, lpMsgSendFax->faxFile);
		yRc = -7;
	}
	else
	{
		while (fgets (file, 512, fp) != NULL)
		{
			file[strlen (file) - 1] = '\0';

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Sending File",
						  lpMsgSendFax->faxFile, zAppRef);

			yRc =
				sendFile (20, file, zCall, zAppRef, gCall[zCall].rtp_session,
						  zpTermReason, &response);

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Done sending File",
						  lpMsgSendFax->faxFile, 0);

			if (yRc != 0)
			{
				break;
			}
		}
	}

	if (fp != NULL)
	{
		fclose (fp);
		fp = NULL;
	}

	return (yRc);

}								/*int sendFile_X */

int
changeHeader (int zCall, int fp)
{

	if (gCall[zCall].codecType == 18)
	{
		return (0);
	}

	return (0);

}								/*END: int changeHeader */

int
removeHeader (int zCall, int fp)
{
	char            mod[] = "removeHeader";
	char            yStrBuffer[128];
	int             i;
	int             yIntWavHeaderSize = 0;

	if (!fp)
	{
		return (-1);
	}

	if (gCall[zCall].codecType == 18 || gCall[zCall].codecType == 96)
	{
		return (0);
	}

	i = read (fp, yStrBuffer, 64);

	yIntWavHeaderSize = getWavHeaderLength (yStrBuffer, i);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yIntWavHeaderSize=%d.", yIntWavHeaderSize);

	//fseek(fp, yIntWavHeaderSize, SEEK_SET);
	lseek (fp, yIntWavHeaderSize, SEEK_SET);

	return (0);

}								/*END: int removeHeader */

///This function speaks a file using rtp_session_send_with_ts().
int
speakRtpFile (int sleep_time,
			  int interrupt_option,
			  char *zFileName,
			  int synchronous,
			  int zCall,
			  int zAppRef,
			  RtpSession * zSession,
			  int *zpTermReason, struct MsgToApp *zResponse)
{
	int             yRc = 0;
	int             yRtpRc = 0;
	char            mod[] = { "speakRtpFile" };
	char           *buffer = NULL;
	int             i;
	FILE           *infile;
	char            lRemoteRtpIp[50];
	int             lRemoteRtpPort;
	struct MsgToApp response;
	char            yTmpBuffer4K[4097];
	char           *zQuery;

	guint32         user_ts = 0;
	unsigned char   header[2] = "";
	long            l_iBytesRead;
	long            tempNumOfBytes;
	const unsigned char *l_ptr;
	int             totalLengthSending = 0;
	int             startOfFile = 0;
	int             isMarkerSet = 0;
	unsigned long   l_dwNewRTPTimestamp = 0;
	unsigned long   l_dwDelayTime = 0;
	unsigned long   m_dwLastRTPTimestamp = 0;
	unsigned long   m_dwPrevGrabTime = 0;
	unsigned long   m_dwExpectGrabTime = 0;
	unsigned long   dwPrevSleepTimestamp;
	unsigned long   dwExpectSleepTimestamp;

	response.message[0] = '\0';

// 	gCall[zCall].vplayBackOn = 0;


    arc_rtp_session_set_ssrc(zCall, zSession);

	if (zSession == NULL)
	{
		*zpTermReason = TERM_REASON_DEV;

		return -1;
	}

	if (zFileName == NULL || zFileName[0] == '\0')
	{
		*zpTermReason = TERM_REASON_DEV;
		return -1;
	}

	infile = fopen (zFileName, "rb");

	if (infile == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Failed to open file (%s). [%d, %s] Unable to speak file.",
				   zFileName, errno, strerror (errno));
		fflush (stdout);

		return (-7);
	}

	*zpTermReason = TERM_REASON_DEV;

	dwPrevSleepTimestamp = dwExpectSleepTimestamp = msec_gettimeofday ();
	m_dwExpectGrabTime = m_dwPrevGrabTime = MCoreAPITime ();
	m_dwLastRTPTimestamp = 0;
	startOfFile = 1;

	memset (header, 0, 2);

	yRc = ACTION_WAIT;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Inside speakRtpFile before while");

	while (((i = fread (header, 1, 2, infile)) > 0))
	{
		if (!canContinue (mod, zCall, __LINE__))
		{
			yRc = -3;

			break;
		}
		else if (gCall[zCall].pendingOutputRequests > 0)
		{

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "Pending requests, total",
						  gCall[zCall].pendingOutputRequests);

			*zpTermReason = TERM_REASON_USER_STOP;
			yRc = ACTION_GET_NEXT_REQUEST;

			if (strcmp (gCall[zCall].audioCodecString, "729b") == 0 &&
				gCall[zCall].isG729AnnexB == YES)
			{
				gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Sending silence = %s", g729b_silenceBuffer);
				yRtpRc =
					arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
												  gCall[zCall].rtp_session,
												  g729b_silenceBuffer,
												  sizeof
												  (g729b_silenceBuffer),
												  gCall[zCall].out_user_ts);
			}
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Calling break");
			break;
		}

		if (gCall[zCall].speechRec == 0 &&
			gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
		{
			//gCall[zCall].in_user_ts+=(gCall[zCall].inTsIncrement*3);
			gCall[zCall].in_user_ts += (gCall[zCall].codecBufferSize * 3);
			sprintf (zResponse->message, "\0");

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "DTMF",
						  gCall[zCall].lastDtmf);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Interrupt_option(%d), gCall[zCall].GV_DtmfBargeinDigitsInt(%d)",
					   interrupt_option,
					   gCall[zCall].GV_DtmfBargeinDigitsInt);

			if (interrupt_option != 0 && interrupt_option != NONINTERRUPT &&	/*0: No interrupt */
				((gCall[zCall].GV_DtmfBargeinDigitsInt == 0) ||
				 (gCall[zCall].GV_DtmfBargeinDigitsInt == 1 &&
				  strchr (gCall[zCall].GV_DtmfBargeinDigits,
						  dtmf_tab[gCall[zCall].lastDtmf]) != NULL)))
			{
				sprintf (zResponse->message, "%c",
						 dtmf_tab[gCall[zCall].lastDtmf]);

				if (gCall[zCall].dtmfAvailable == 1)
				{
					//if(gCall[zCall].isBeepActive == 0)
					if (gCall[zCall].rtp_session_mrcp == NULL
						|| gCall[zCall].GV_BridgeRinging == 1)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Setting dtmfAvailable to 0.");
						gCall[zCall].dtmfAvailable = 0;
					}

					if (gCall[zCall].rtp_session != NULL)
					{
#if 0
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "reading audio data of size(%d)",
								   gCall[zCall].codecBufferSize);
						arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													  gCall[zCall].rtp_session,
													  gCall[zCall].
													  silenceBuffer,
													  gCall[zCall].
													  codecBufferSize,
													  gCall[zCall].
													  out_user_ts);
#endif

						//gCall[zCall].out_user_ts+=(gCall[zCall].outTsIncrement);
					}

					*zpTermReason = TERM_REASON_DTMF;
					__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "TERM_REASON_DTMF",
								  zCall);

					yRc = ACTION_GET_NEXT_REQUEST;
					if (strcmp (gCall[zCall].audioCodecString, "729b") == 0 &&
						gCall[zCall].isG729AnnexB == YES)
					{
						gCall[zCall].out_user_ts +=
							gCall[zCall].outTsIncrement;
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO, "Sending silence = (%s)",
								   g729b_silenceBuffer);
						yRtpRc =
							arc_rtp_session_send_with_ts (mod, __LINE__,
														  zCall, gCall[zCall].rtp_session,
														  g729b_silenceBuffer,
														  sizeof
														  (g729b_silenceBuffer),
														  gCall[zCall].
														  out_user_ts);
					}

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Calling break");
					break;
				}
			}
			else
			{
				if (gCall[zCall].rtp_session_mrcp == NULL)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Setting dtmfAvailable to 0.");
					gCall[zCall].dtmfAvailable = 0;
				}
			}

			if (gCall[zCall].rtp_session != NULL)
			{
#if 0
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "reading audio data of size(%d)",
						   gCall[zCall].codecBufferSize);
				arc_rtp_session_send_with_ts (mod, __LINE__, zCall, gCall[zCall].rtp_session,
											  gCall[zCall].silenceBuffer,
											  gCall[zCall].codecBufferSize,
											  gCall[zCall].out_user_ts);

#endif
				//gCall[zCall].out_user_ts+=(gCall[zCall].outTsIncrement);
			}

		}

		if (i != 2)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Failed to read size; i=%d.", i);
			if (strcmp (gCall[zCall].audioCodecString, "729b") == 0 &&
				gCall[zCall].isG729AnnexB == YES)
			{
				gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Sending silence = %s.",
						   g729b_silenceBuffer);
				yRtpRc =
					arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
												  gCall[zCall].rtp_session,
												  g729b_silenceBuffer,
												  sizeof
												  (g729b_silenceBuffer),
												  gCall[zCall].out_user_ts);
			}
			break;
		}
		else
		{
			l_iBytesRead = (header[0] << 8) | header[1];

			if (l_iBytesRead < 12 || l_iBytesRead > 5000)	// RTP header is at least 12 bytes
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Failed to speak; rtp header incorrect (%d).",
						   l_iBytesRead);
				if (strcmp (gCall[zCall].audioCodecString, "729b") == 0
					&& gCall[zCall].isG729AnnexB == YES)
				{
					gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Sending silence = %s",
							   g729b_silenceBuffer);
					yRtpRc =
						arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													  gCall[zCall].rtp_session,
													  g729b_silenceBuffer,
													  sizeof
													  (g729b_silenceBuffer),
													  gCall[zCall].
													  out_user_ts);
				}
				break;
			}

			if (l_iBytesRead <= 0 || l_iBytesRead > 1500)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Failed to speakl_iBytesRead calculated=%d.",
						   l_iBytesRead);
				continue;
			}

			buffer = (char *) arc_malloc (zCall, mod, __LINE__, l_iBytesRead);
			memset (buffer, 0, l_iBytesRead);

			if ((tempNumOfBytes =
				 fread (buffer, 1, l_iBytesRead, infile)) != l_iBytesRead)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
						   "Failed to read size; l_iBytesRead=%d, bytes read=%d.",
						   l_iBytesRead, tempNumOfBytes);
				if (strcmp (gCall[zCall].audioCodecString, "729b") == 0
					&& gCall[zCall].isG729AnnexB == YES)
				{
					gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Sending silence = %s.",
							   g729b_silenceBuffer);
					yRtpRc =
						arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													  gCall[zCall].rtp_session,
													  g729b_silenceBuffer,
													  sizeof
													  (g729b_silenceBuffer),
													  gCall[zCall].
													  out_user_ts);
				}
				break;
			}

			l_ptr = (const unsigned char *) buffer;

			if ((l_ptr[1] & 0x80) != 0)
			{
				isMarkerSet = 1;
			}
			else
			{
				isMarkerSet = 0;
			}

			l_dwNewRTPTimestamp =
				(l_ptr[4] << 24) | (l_ptr[5] << 16) | (l_ptr[6] << 8) |
				l_ptr[7];

#if 0
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "FILE::audio packet with timestamp=(%ld), m_dwLastRTPTimestamp=(%ld)",
					   l_dwNewRTPTimestamp, m_dwLastRTPTimestamp);
#endif
			if (l_dwNewRTPTimestamp < 0)
			{
				l_dwNewRTPTimestamp = m_dwLastRTPTimestamp;
				rtpSleep (20, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);
			}
			else if (m_dwLastRTPTimestamp != 0
					 && l_dwNewRTPTimestamp > m_dwLastRTPTimestamp)
			{
				l_dwDelayTime =
					(l_dwNewRTPTimestamp - m_dwLastRTPTimestamp) / 8;

#if 0
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "FILE::sleeping for time=(%d)",
						   l_dwDelayTime);
#endif
				if (strcmp (gCall[zCall].audioCodecString, "729b") == 0 &&
					gCall[zCall].isG729AnnexB == YES)
				{
					rtpSleep_withbreaks (l_dwDelayTime,
										 &gCall[zCall].lastOutRtpTime, zCall,
										 interrupt_option, __LINE__);
				}
				else if (l_dwDelayTime > 11000)
				{
					rtpSleep (20, &gCall[zCall].lastOutRtpTime, __LINE__,
							  zCall);
				}
				else
				{
					rtpSleep_withbreaks (l_dwDelayTime,
										 &gCall[zCall].lastOutRtpTime, zCall,
										 interrupt_option, __LINE__);
				}
			}
			m_dwLastRTPTimestamp = l_dwNewRTPTimestamp;
		}

#if 0
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "FILE::amr audio packet with timestamp=(%ld)",
				   l_dwNewRTPTimestamp);
#endif

#if 0
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "STREAM:: sending RTP audio data of size(%d)",
				   l_iBytesRead);
#endif

		if (l_iBytesRead > 12)
		{
			gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;

			if (gCall[zCall].rtp_session != NULL)
			{
				if ((strcmp (gCall[zCall].audioCodecString, "AMR") == 0))
				{
					//buffer = buffer +12;
					/* bytes per AMR frame, indexed by mode */
	int             packed_size[16] =
		{ 12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0 };

	char            localBuffer[4096] = "";
	char           *toc_ptr, tocs[4096];
	int             cmr = 0;
	int             mode = 0;
	int             framesize = 0;
	int             count = 0;
	int             dataSize = l_iBytesRead - 12;

					memcpy (localBuffer, buffer + 12, dataSize);

					count++;
					// Read the TOC table
					toc_ptr = tocs;

					do
					{
						*toc_ptr = localBuffer[count];
						count++;
						if (count > dataSize)
						{
							break;
						}
					}
					while (*toc_ptr++ & 0x80);	// handle multiple AMR frames in one RTP payload

					//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					//      "calling Traverse the TOC table, count=%d, cmr=%d", count, cmr);

					// Traverse the TOC table, pulling out the AMR data in individual frames
					toc_ptr = tocs;
					do
					{
	char            sentBuf[128] = "";
	char            buf[128] = "";

						mode = (*toc_ptr >> 3) & 0xf;

						switch (mode)
						{
						case 0:
							framesize = 12;
							break;
						case 1:
							framesize = 13;
							break;
						case 2:
							framesize = 15;
							break;
						case 3:
							framesize = 17;
							break;
						case 4:
							framesize = 19;
							break;
						case 5:
							framesize = 20;
							break;
						case 6:
							framesize = 26;
							break;
						case 7:
							framesize = 31;
							break;
						case 8:
							framesize = 5;
							break;
						default:
							framesize = 0;
							break;
						}

						//framesize = packed_size[mode];
						//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						//  "inside Traverse the TOC table, framesize=%d, mode=%d", framesize, mode);

						if ((count + framesize) > dataSize)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "count=%d, framesize=%d, dataSize=%d, skipping frame");
							break;
						}
						memcpy (buf, localBuffer + count, framesize);
						count = count + framesize;
						//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						//  "inside Traverse the TOC table, count=%d", count);

						sprintf (sentBuf, "%c%c%s", localBuffer[0],
								 (*toc_ptr & 0x7f), buf);
						//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						//  "inside Traverse the TOC table calling arc_rtp_session_send_with_ts, size=%d", framesize + 2);

						yRtpRc =
							arc_rtp_session_send_with_ts (mod, __LINE__,
														  zCall, gCall[zCall].rtp_session,
														  sentBuf,
														  framesize + 2,
														  gCall[zCall].
														  out_user_ts);

						//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						//  "inside Traverse the TOC table, after arc_rtp_session_send_with_ts yRtpRc=%d", yRtpRc);

						if (toc_ptr == NULL
							|| !canContinue (mod, zCall, __LINE__))
						{
							break;
						}

						gCall[zCall].out_user_ts +=
							gCall[zCall].outTsIncrement;
						//util_sleep(0, 20);

					}
					while (*toc_ptr++ & 0x80);

				}
				else
				{

					yRtpRc =
						arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													  gCall[zCall].rtp_session, buffer + 12,
													  l_iBytesRead - 12,
													  gCall[zCall].
													  out_user_ts,
													  isMarkerSet);
				}

			}

			if (yRtpRc <= 0)
			{
				util_sleep (0, 100);
				yRc = ACTION_GET_NEXT_REQUEST;
				break;
			}

		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Failed to speak; rtp header incorrect (%d).",
					   l_iBytesRead);
			break;
		}

		totalLengthSending += l_iBytesRead + 12;
		user_ts += l_dwNewRTPTimestamp;

		if (buffer)
		{
			arc_free (zCall, mod, __LINE__, buffer, l_iBytesRead);
			free (buffer);
			buffer = NULL;
		}

	}

	if (buffer)
	{
		arc_free (zCall, mod, __LINE__, buffer, l_iBytesRead);
		free (buffer);
		buffer = NULL;
	}

	fclose (infile);

	return (yRc);

}								/*END: speakRtpFile */

/*
 * Returns index for this span provided tha name matches
 */

int
arc_conference_create_by_name (int zCall, struct arc_small_conference_t *c, size_t size,
							   char *name)
{
	int             rc = -1;
	int             i;
	arc_small_conference_t *ptr;
	int             found = 0;

	/// find first

	for (i = 0; i < size; i++)
	{

		ptr = &c[i];

		if (arc_small_conference_find (ptr, name) == 1)
		{
			dynVarLog (__LINE__, zCall, "arc_conference_create_by_name",
					   REPORT_VERBOSE, DYN_BASE, INFO,
					   "Found conference with name [%s] and index=%d.", name,
					   i);
			found = 1;
			rc = -1;
			break;
		}
	}

	/// if not create 

	if (!found)
	{

		for (i = 0; i < size; i++)
		{

			ptr = &c[i];

			// DJB DEBUG - uncomment this if (arc_small_conference_find_free (ptr) == 1)
			//if (arc_small_conference_find_free (zCall, ptr) == 1)
			// END DJB DEBUG - uncomment this if (arc_small_conference_find_free (ptr) == 1)
			if (arc_small_conference_find_free (zCall, ptr) == 1)
			{
				arc_small_conference_init (ptr, name);
				rc = i;
				break;
			}
		}
	}

	return rc;
}

int
arc_conference_addref_by_name (struct arc_small_conference_t *c, size_t size,
							   int zCall, char *name)
{

	int             rc = -1;
	int             idx;

	if (!name)
	{
		dynVarLog (__LINE__, zCall, "arc_conference_addref_by_name",
				   REPORT_NORMAL, DYN_BASE, ERR,
				   "Empty name (%s) received. Unable to add user.", name);
		return -1;
	}

	idx = arc_conference_find_by_name (zCall, c, size, name);

	if (idx == -1)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL, DYN_BASE, ERR,
				"Conference with name (%s) not found. Unable to add user.", name);
		return -1;
	}

	// fprintf(stderr, " %s() adding ref %p to conference with idx=%d\n", __func__, &gCall[zCall].audio_frames[CONFERENCE_AUDIO], idx);

	arc_audio_frame_reset (&gCall[zCall].audio_frames[CONFERENCE_AUDIO]);

	gCall[zCall].conf_frame =
		arc_small_conference_add_ref (&c[idx], 0,
									  ARC_SMALL_CONFERENCE_AUDIO_OUT);

	if (gCall[zCall].conf_frame != NULL)
	{
		rc = 0;
	}

	if (rc != -1)
	{
		gCall[zCall].conferenceIdx = idx;
		gCall[zCall].conferenceStarted = 1;
		gCall[zCall].confPacketCount = 0;
		//fprintf(stderr, " %s() conf id = [%s]\n", __func__, gCall[zCall].confID);
	}

	return rc;
}

int
arc_conference_delref_by_name (struct arc_small_conference_t *c, size_t size,
							   int zCall, char *name)
{

	int             rc = -1;
	int             idx;
	int				count;
	static char		mod[]="arc_conference_delref_by_name";
	int				yRc;

	if (!name || (zCall == -1))
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_DETAIL, DYN_BASE,
				   WARN, "Emtyp name (%s). Unable to delete reference by name.", name);
		return -1;
	}

	if (name[0] == '\0')
	{
		return -1;
	}

	idx = arc_conference_find_by_name (zCall, c, size, name);

	if (idx == -1)
	{
		dynVarLog (__LINE__, -1, (char *) __func__, REPORT_DETAIL, DYN_BASE,
			   WARN, "Conference with name [%s] not found.  Unable to delete reference by name.", name);
		memset (gCall[zCall].confID, 0, sizeof (gCall[zCall].confID));
		return -1;
	}

	//fprintf(stderr, " %s() deleting ref %p to conference with idx=%d\n", __func__, &gCall[zCall].audio_frames[CONFERENCE_AUDIO], idx);

	rc = arc_small_conference_del_ref (&c[idx], gCall[zCall].conf_frame,
									   ARC_SMALL_CONFERENCE_AUDIO_OUT);

	if (rc != -1)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_DETAIL, DYN_BASE, WARN,
                "arc_small_conference_del_ref() succesfully deleted name/index of [%s, %d]. rc=%d",
                name, idx, rc);
		gCall[zCall].conferenceIdx = -1;
		//gCall[zCall].conferenceStarted = 0;
		//gCall[zCall].confID[0] = '\0';
	}
	else
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_DETAIL, DYN_BASE, WARN,
                "arc_small_conference_del_ref() returned failure for name/index of [%s, %d]. "
                 "Unable to delete reference by name.", name, idx);
	}

	if (idx != -1)
	{
		count = arc_small_conference_refcount (&c[idx]);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF: conference reference count = %d", count);
		if ( count == 0 )
		{

			rc = arc_conference_delete_by_name (zCall, Conferences, 48,
										gCall[zCall].confID);

			if (rc == -1)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF: failed to delete conference with ID=[%s].",
										gCall[zCall].confID);
			}
			else
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF: Successfully deleted conference with ID=[%s].",
										gCall[zCall].confID);
			}
		}
	}
	memset (gCall[zCall].confID, 0, sizeof (gCall[zCall].confID));

	return rc;
}

int
arc_conference_find_by_name (int zCall, struct arc_small_conference_t *c, size_t size,
							 char *name)
{

	int             rc = -1;
	int             i;
	arc_small_conference_t *ptr = NULL;

	if (!name)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_DETAIL, DYN_BASE,
				   ERR, "Emtyp name (%s). Unable to find reference by name.", name);
	}

	for (i = 0; i < size; i++)
	{

		ptr = &(c[i]);

		if (arc_small_conference_find (ptr, name) == 1)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_DETAIL, DYN_BASE, INFO,
					   "Found conference with name [%s] and index=%d", name, i);
			rc = i;
			break;
		}
	}
	return rc;
}

int
arc_conference_delete_by_name (int zCall, struct arc_small_conference_t *c, size_t size,
							   char *name)
{

	int             rc = -1;
	int             i;
	int             idx = -1;
	int             count;
	arc_small_conference_t *ptr = NULL;

	for (i = 0; i < size; i++)
	{

		ptr = &c[i];

		if (arc_small_conference_find (ptr, name) == 1)
		{
//			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_DETAIL,
//					   DYN_BASE, INFO,
//					   "Found conference with name [%s] and index=%d, attemping to delete",
//					   name, i);
			idx = i;
			break;
		}
	}

	if (idx != -1)
	{
		count = arc_small_conference_refcount (&c[idx]);
		arc_small_conference_free (&c[idx]);
		rc = 0;
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_DETAIL, DYN_BASE,
				   INFO,
				   "Found conference with name [%s] and index=%d, refcount=%d",
				   name, i, count);
	}

	return rc;
}

int
arc_conference_decode_and_post (int zCall, char *buff, size_t bufsize,
								int audio_opts, char *func, int line)
{

	int             rc = -1;
	char            tmp[1024];
	int             idx = -1;
	int             samples = -1;
	int             bytes = -1;

	if (zCall < 0 || zCall >= MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   DYN_BASE, ERR,
				   "Invalid zCall index %d from func %s and line %d, cannot process, returning -1",
				   zCall, func, line);
		return -1;
	}
	if (gCall[zCall].conferenceStarted != 1)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   DYN_BASE, INFO, "Conference is not started yet returning",
				   zCall, func, line);
		return -1;

	}

	if (gCall[zCall].conferenceIdx < 0 || gCall[zCall].conferenceIdx >= 48)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   DYN_BASE, ERR,
				   " conference idx is out of range, returning -1 from func %s and line %d",
				   func, line);
		usleep (20 * 1000);
		return -1;
	}

	if (!buff || !bufsize)
	{
			// djb - maybe uncomment this after load testing is complete
//		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_DETAIL,
//				   DYN_BASE, ERR,
//				   " buff or bufsize is set to zero, cannot continue, called from func %s and line %d",
//				   func, line);
		usleep (20 * 1000);
		return -1;
	}

	arc_clock_sync_wait (clock_sync_in, &gCall[zCall], 20, NULL,
						 ARC_CLOCK_SYNC_WRITE);

	idx = gCall[zCall].conferenceIdx;

	//fprintf(stderr, " %s() buff=%p bufsize=%d calling func=%s lineno=%d\n", __func__, buff, bufsize, __func__, line);

	samples =
		arc_decode_buff (__LINE__, &gCall[zCall].decodeAudio[CONFERENCE_AUDIO], buff,
						 bufsize, tmp, sizeof (tmp));

	//fprintf(stderr, " %s() samples=%d\n", __func__, samples);

	if ((samples * 2) == 320)
	{
		//fprintf(stderr, " %s() idx=%d\n", __func__, idx);
		// bytes = arc_small_conference_post(&Conferences[idx], gCall[zCall].conf_frame, tmp, samples * 2, audio_opts, ARC_SMALL_CONFERENCE_AUDIO_OUT);
		bytes =
			arc_small_conference_post (&Conferences[idx],
									   gCall[zCall].conf_frame, tmp, 320,
									   audio_opts,
									   ARC_SMALL_CONFERENCE_AUDIO_OUT);

#if 0
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   DYN_BASE, INFO,
				   "Posted %d bytes to audio framebuffer[%d] buff=%p bufsize=%d from func %s and line %d conf_frame=%p",
				   samples, CONFERENCE_AUDIO, buff, bufsize, func, line,
				   gCall[zCall].conf_frame);
#endif

		rc = samples;
		gCall[zCall].confPacketCount++;

	}
	else
	{
		fprintf (stderr, " %s() samples * 2 == %d\n", __func__, samples * 2);
	}

	// arc_clock_sync_wait(clock_sync_in, &gCall[zCall], 0, NULL, ARC_CLOCK_SYNC_WRITE);

	return rc;
}

int
arc_conference_read_and_encode (int zCall, char *buff, size_t bufsize,
								int audio_opts, char *func, int line)
{

	int             rc = -1;
	int             idx = -1;
	char            tmp[1024];
	int             samples = 0;
	int             bytes = 0;

//  int count = 1;

//struct timeb tp;
//ftime(&tp);

	if (zCall < 0 || zCall >= MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   DYN_BASE, ERR,
				   " Invalid zCall index %d from func %s and line %d, cannot process, returning -1",
				   zCall, func, line);
		return -1;
	}

	if (gCall[zCall].conferenceIdx < 0 || gCall[zCall].conferenceIdx >= 48)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   DYN_BASE, ERR,
				   " conference idx is out of range, returning -1 from func %s and line %d",
				   func, line);
		return -1;
	}

//fprintf(stderr, "%s Call: %d, millisec:%d\n", __func__, zCall, tp.millitm);

	arc_clock_sync_wait (clock_sync_in, &gCall[zCall], 20, NULL,
						 ARC_CLOCK_SYNC_READ);

	idx = gCall[zCall].conferenceIdx;

//  while(count--)
//  {
	bytes =
		arc_small_conference_read (&Conferences[idx], gCall[zCall].conf_frame,
								   tmp, sizeof (tmp),
								   ARC_SMALL_CONFERENCE_AUDIO_OUT);
	bytes = 1;

#if 0
	dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, DYN_BASE,
			   INFO,
			   " read rc=%d from conference[%d] framebuffer  from func %s and line %d",
			   bytes, idx, func, line);
#endif

	if (bytes > 0)
	{
		samples =
			arc_encode_buff (&gCall[zCall].encodeAudio[CONFERENCE_AUDIO], tmp,
							 320, buff, bufsize);

#if 0
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   DYN_BASE, INFO,
				   " %d samples encoded[%d] from func %s and line %d count %d",
				   samples, CONFERENCE_AUDIO, func, line,
				   gCall[zCall].confPacketCount);
#endif

		rc = bytes;
	}

#if 0
	if (gCall[zCall].confPacketCount <= 0)
	{
		continue;
	}
#endif

	gCall[zCall].confPacketCount--;

//  break;
//  }

	// arc_clock_sync_wait(clock_sync_in, &gCall[zCall], 0, NULL, ARC_CLOCK_SYNC_READ);

	return 320;
}

// utility to post raw audio to frame ringbuffer 

int
arc_frame_decode_and_post (int zCall, char *buff, size_t bufsize, int which,
						   int audio_opts, char *func, int line)
{

	int             rc = -1;
	char            tmp[1024];
	int             samples;
	int             bytes;
	int             use_silence = 0;
	char            mod[] = "arc_frame_decode_and_post";
	char            yStrMsg[128];

	if (zCall < 0 || zCall >= MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, "arc_frame_decode_and_post",
				   REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Invalid port (%d); must be between 0 and %d. "
				   "Unable to decode and post frame.", zCall, MAX_PORTS);
		return rc;
	}

	if (gCall[zCall].codecType != 0 && gCall[zCall].codecType != 8)
	{
		return rc;
	}

	switch (which)
	{
	case AUDIO_MIXED:
		/*A + B Leg recordings */
		if (gCall[zCall].silentRecordFlag == 0)
		{
			return (rc);
		}
		break;

	case AUDIO_IN:
		/*Allow only if call recording and trail silence */
		break;

	case AUDIO_OUT:
		/*Allow only when volume level is other than default */
#if 0							// we have other falgs for this in speakFile
		if (gCall[zCall].GV_Volume == 5 && gCall[zCall].GV_Speed == 0)	/*FOR Joe: Default Speed level is 5 */
		{
			return (rc);
		}

#endif
		break;

	default:
		break;
	}

	if (!buff || !bufsize)
	{
		slin16_gen_audio_silence (tmp, gCall[zCall].codecBufferSize * 2);
		use_silence = 1;
		samples = 160;
	}

	if (which < 0 || which > 2)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_INVALID_PARM, ERR,
				   "Invalid \"which\" received (%d).  Must be between 0 and 2. "
				   "Unable to decode and post frame.", which);
		return rc;
	}

	if (use_silence == 0)
	{
		samples =
			arc_decode_buff (__LINE__, &gCall[zCall].decodeAudio[which], buff, bufsize,
							 tmp, sizeof (tmp));
	}

	if (samples > 0)
	{

//sprintf(yStrMsg, "which=%d, samples=%d", which, samples); __DDNDEBUG__(DEBUG_MODULE_AUDIO, "", yStrMsg, which);

		bytes =
			arc_audio_frame_post (&gCall[zCall].audio_frames[which], tmp,
								  samples * 2, audio_opts);
		rc = bytes;
	}

	return rc;

}								/*END: arc_frame_decode_and_post */

int
arc_frame_encode_with_speed_ctl (int zCall, char *buff, size_t buffsize,
								 int which, char *func, int line, int *more)
{

	int             rc = 0;
	char            tmp[1024];
	int             idx = -1;
	int             bytes = 0;
	int             samples = 0;
	int             count = 0;
	int             havebuff = 0;

	struct timeb    ts;

	if (zCall < 0 || zCall >= MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, "arc_frame_encode_with_speed_ctl",
				   REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Invalid port (%d); must be between 0 and %d. "
				   "Unable to encode with speed control.", zCall, MAX_PORTS);
		return 0;
	}

	if (!buff || !buffsize)
	{
		dynVarLog (__LINE__, zCall, "arc_frame_encode_with_speed_ctl",
				   REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Empty buff (%s) received. Unable to encode with speed control.", buff);
		return 0;
	}

	idx = zCall % 48;

	if (idx < 0 || idx >= 48)
	{
		dynVarLog (__LINE__, zCall, "arc_frame_encode_with_speed_ctl",
				   REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Invalid index (%s). Unable to encode with speed control.", idx);
		return 0;
	}

	if (promptControl[idx] == NULL)
	{
		dynVarLog (__LINE__, zCall, "arc_frame_encode_with_speed_ctl",
				 REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				 "prompt control is not allocated. Unable to encode with speed control.");
		return 0;
	}

	//ftime(&ts);
	//fprintf(stderr, " timestamp %d.%d\n", ts.time, ts.millitm);

	memset (tmp, 0, sizeof (tmp));

	if (promptControl[idx]->is_flushed () == 0)
	{
		bytes =
			arc_audio_frame_read (&gCall[zCall].audio_frames[which], tmp,
								  sizeof (tmp));
		if (bytes)
		{
			//is_all_zeros(tmp, bytes, __LINE__, __func__);
			//fprintf(stderr, " read %d bytes from ringbuffer \n", bytes);
			promptControl[idx]->putBuff (tmp, bytes);
			havebuff++;
		}
	}

	if (havebuff)
	{
		//promptControl[idx].putBuff(tmp, bytes);
		bytes = promptControl[idx]->getBuff (tmp, bytes);
		//fprintf(stderr, "prompt control bytes=%d\n", bytes);
		if (bytes > 0)
		{
			samples =
				arc_encode_buff (&gCall[zCall].encodeAudio[which], tmp, bytes,
								 buff, buffsize);
			//fprintf(stderr, "samples=%d\n", samples);
			//is_all_zeros(buff, samples, __LINE__, __func__);
		}
	}
	else
	{

#if 1
		// fprintf(stderr, " %s() remaining buffer size=%d\n", __func__, promptControl[idx]->getRemaining());
		bytes = promptControl[idx]->getBuff (tmp, buffsize);
		if (bytes > 0)
		{
			samples =
				arc_encode_buff (&gCall[zCall].encodeAudio[which], tmp, bytes,
								 buff, buffsize);
			//is_all_zeros(buff, samples, __LINE__, __func__);
		}
		else
#endif
		if (promptControl[idx]->getRemaining () <= 0)
		{
			samples = -1;
		}
		// fprintf(stderr, " %s() bytes=%d\n", __func__, bytes);
	}

	switch (samples)
	{
	case -1:
		*more = ARC_SPEED_DONE;
		samples = 0;
		break;
	case 0:
		*more = ARC_SPEED_NEED_MORE;
		break;
	default:
		*more = ARC_SPEED_DO_NOTHING;
		break;
	}

	if (samples > 0)
	{
		rc = samples;
	}

	//ftime(&ts);
	//fprintf(stderr, " timestamp %d.%d\n", ts.time, ts.millitm);
	//fprintf(stderr, " %s() rc=%d\n", __func__, rc);
	return rc;
}								/* END: arc_frame_encode_with_speed_control */

int
arc_frame_read_and_encode (int zCall, char *buff, size_t bufsize, int which,
						   char *func, int line)
{

	int             rc = -1;
	char            tmp[1024];
	int             bytes = 0;
	int             samples = 0;

	if (zCall < 0 || zCall >= MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, "arc_frame_read_and_encode",
				   REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Invalid port (%d); must be between 0 and %d. "
				   "Unable to read and encode frame.", zCall, MAX_PORTS);
		return rc;
	}

	if (gCall[zCall].codecType != 0
		&& gCall[zCall].codecType != 8 && gCall[zCall].codecType != 127)
	{
#if 0
		dynVarLog (__LINE__, zCall, "arc_frame_read_and_encode",
				   REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Invalid codecType (%d); must be either 0 or 8. "
				   "Unable to read and encode frame.",
				   gCall[zCall].codecType);
#endif
		return rc;
	}

	if (!buff || !bufsize)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_INVALID_PARM, ERR,
				   "Empty buffer; cannot processs.  Unable to read and encode frame.");
		return rc;
	}

	if (which < 0 || which > 2)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_INVALID_PARM, ERR,
				   "Invalid \"which\" received (%d).  Must be between 0 and 2. "
				   "Unable to read and encode frame.", which);
		return rc;
	}

#if 0

	switch (which)
	{
	case AUDIO_OUT:
		if (gCall[zCall].GV_Volume == 5)
		{
			return rc;
		}
		break;

	default:
		break;
	}

#endif

	bytes =
		arc_audio_frame_read (&gCall[zCall].audio_frames[which], tmp,
							  sizeof (tmp));
	//dynVarLog(__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, DYN_BASE, INFO, " read %d bytes from framebuffer[%d]  from func %s and line %d", bytes, which, func, line);

	if (bytes > 0)
	{
		samples =
			arc_encode_buff (&gCall[zCall].encodeAudio[which], tmp, bytes,
							 buff, bufsize);
		//dynVarLog(__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, DYN_BASE, INFO, " %d samples encoded[%d] from func %s and line %d", samples, which, func, line); 
		rc = bytes;
	}

	// return rc;
	return samples;
}								// END: arc_frame_read_and_encode()

int
arc_frame_reset_with_silence (int zCall, int which, int numframes, char *func,
							  int line)
{

	int             rc = -1;
	int             i;
	char            buff[1024];
	char            mod[] = "arc_frame_reset_with_silence";

	if (zCall < 0 || zCall >= MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, "arc_frame_reset_with_silence",
				   REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Invalid port (%d); must be between 0 and %d. "
				   "Unable to reset frame with silence.", zCall, MAX_PORTS);
		return -1;
	}

	if (gCall[zCall].codecType != 0 && gCall[zCall].codecType != 8)
	{
#if 0
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_INVALID_PARM, ERR,
				   "Invalid codecType (%d); must be either 0,8, or 127. "
				   "Unable to reset frame with silence.",
				   gCall[zCall].codecType);
#endif
		return rc;
	}

	memset (buff, 0, sizeof (buff));

//__DDNDEBUG__(DEBUG_MODULE_AUDIO, "", "" , zCall);

	arc_audio_frame_reset (&gCall[zCall].audio_frames[which]);

	for (i = 0; i < numframes; i++)
	{
		rc = arc_audio_frame_post (&gCall[zCall].audio_frames[which], buff,
								   gCall[zCall].codecBufferSize * 2, 0);
	}

	// fprintf(stderr, " %s() rc = %d\n", __func__, rc);
	return rc;
}								// END: arc_frame_reset_with_silence()

int
arc_frame_record_to_file (int zCall, int what, char *func, int line)
{

	char            mod[] = "arc_frame_record_to_file";

	int             rc;
	char            tmp[1024];
	char            buff[1024];
	int             bytes = 0;
	int             samples = 0;
	int             tries = 3;
	int             i;

	if (gCall[zCall].silentRecordFlag == 1
		&& gCall[zCall].gSilentInRecordFileFp != NULL)
	{
		if (zCall < 0 || zCall >= MAX_PORTS)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
					   DYN_BASE, ERR,
					   " Invalid zCall index %d from func %s and line %d, cannot process, returning -1",
					   zCall, func, line);
			return -1;
		}

		if (gCall[zCall].codecType != 0 && gCall[zCall].codecType != 8)
		{
			dynVarLog (__LINE__, zCall, "arc_frame_apply", REPORT_NORMAL,
					   TEL_INVALID_PARM, ERR,
					   "Invalid port (%d); must be between 0 and %d. "
					   "Unable to apply frame.", zCall, MAX_PORTS);
			return rc;
		}

		if (what < 0 || what > 2)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
					   DYN_BASE, ERR,
					   " Invalid \"what\" index %d from func %s and line %d, cannot process, returning -1",
					   what, func, line);
			return -1;
		}

		while (tries--)
		{
			bytes =
				arc_audio_frame_read (&gCall[zCall].audio_frames[what], tmp,
									  sizeof (tmp));
			if (bytes)
			{
				break;
			}
			usleep (10);
		}

//		dynVarLog(__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, DYN_BASE, INFO, " read %d bytes from framebuffer[%d]  from func %s and line %d", bytes, what, func, line);

		if (bytes > 0)
		{
			samples =
				arc_encode_buff (&gCall[zCall].encodeAudio[what], tmp, bytes,
								 buff, sizeof (buff));
//			dynVarLog(__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, DYN_BASE, INFO, " %d samples encoded[%d] from func %s and line %d",
//												samples, what, func, line);
			rc = bytes;
		}

		if (samples <= 0)
		{
			dynVarLog(__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, DYN_BASE, INFO, "Failed to decode samples for recording, returning -1");
			return -1;
		}

	}

	// app logic from gcall structure

	if (gCall[zCall].silentRecordFlag == 1 &&
		gCall[zCall].gSilentInRecordFileFp != NULL && (samples > 0))
	{
//		dynVarLog(__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, DYN_BASE, INFO, " Writing %d bytes to file.", gCall[zCall].codecBufferSize);

//__DDNDEBUG__(DEBUG_MODULE_AUDIO, "Writting to data record file", "", 160);

		fwrite (buff, gCall[zCall].codecBufferSize, 1,
				gCall[zCall].gSilentInRecordFileFp);

//		sendAudioToSomewhere(buff, gCall[zCall].codecBufferSize, gCall[zCall].gSilentInRecordFileFp);


	}
	else if (gCall[zCall].silentRecordFlag == 0 &&
			 gCall[zCall].gSilentInRecordFileFp != NULL)
	{
		// fprintf(stderr, " flushing buffer to file \n");

		for (i = 0; i < 64; i++)
		{
			bytes =
				arc_audio_frame_read (&gCall[zCall].audio_frames[what], tmp,
									  sizeof (tmp));
			//fprintf(stderr, " %s() bytes= %d, i=%d", __func__, bytes, i);
			if (bytes > 0)
			{
				samples =
					arc_encode_buff (&gCall[zCall].encodeAudio[what], tmp,
									 bytes, buff, sizeof (buff));
				if (samples)
				{
					//fprintf(stderr, " %d additional bytes in buffer, writing to file, i=%d\n", bytes, i);
					fwrite (buff, gCall[zCall].codecBufferSize, 1,
							gCall[zCall].gSilentInRecordFileFp);
				}
			}
			else
			{
				break;
			}
		}
		writeWavHeaderToFile (zCall, gCall[zCall].gSilentInRecordFileFp);
		fclose (gCall[zCall].gSilentInRecordFileFp);
		gCall[zCall].gSilentInRecordFileFp = NULL;

	}

}

void sendAudioToSomewhere(char *buf, int bufSize, FILE *fp)
{
	struct  sockaddr_in serv_addr;
	int                 sockfd;
	int                 servPort;

	static int	firstTime = 1;
	char		host[64]="10.0.0.212";
	int			rc;
    struct protoent	*p;
    int				protocol;
	socklen_t		slen = sizeof(serv_addr);
	int				totalSent;

	servPort = 6789;
	if ( firstTime )
	{
		firstTime = 0;
      if((p = getprotobyname("UDP")) == NULL)
        {
            fprintf(stderr, "%s|%d|getprotobyname failed, [%d,%s]\n",
                __FILE__, __LINE__, errno, strerror(errno));
            return;
        }
        protocol = p->p_proto;

        /* Get socket into TCP/IP */
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, protocol)) < 0)
        {
            fprintf(stderr, "%s|%d|socket failed, [%d,%s]\n",
                __FILE__, __LINE__, errno, strerror(errno));
            return;
        }
		fprintf(stderr, "%s|%d|%d = socket()\n",
                __FILE__, __LINE__, sockfd);

		memset((char *) &serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(servPort);
		inet_aton(host, &serv_addr.sin_addr);
	}
		
	rc = sendto(sockfd, buf, bufSize, 0, (struct sockaddr *)&serv_addr, slen);
	totalSent += rc;

//	shutdown(sockfd, 2);
//	close(sockfd);

    return;
} // END: sendAudioToSomewhere()

int
arc_frame_apply (int zCall, char *buff, size_t bufsize, int which, int op,
				 char *func, int line)
{

	int             rc = -1;
	char            tmp[1024];
	int             samples = 0;

	if (zCall < 0 || zCall >= MAX_PORTS)
	{
		//dynVarLog(__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, DYN_BASE, ERR, " Invalid zCall index %d from func %s and line %d, cannot process, returning -1", zCall, func, line);
		return rc;
	}

	if (gCall[zCall].codecType != 0 && gCall[zCall].codecType != 8)
	{
#if 0
		dynVarLog (__LINE__, zCall, "arc_frame_read_and_encode",
				   REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Invalid codecType (%d); must be either 0 or 8. "
				   "Unable to apply frame.", gCall[zCall].codecType);
#endif
		return rc;
	}

	if (!buff || !bufsize)
	{
		return 0;
	}

	if (which < 0 || which > 2)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_INVALID_PARM, ERR,
				   "Invalid \"which\" received (%d).  Must be between 0 and 2. "
				   "Unable to apply frame.", which);
		return -1;
	}

	samples =
		arc_decode_buff (__LINE__, &gCall[zCall].decodeAudio[which], buff, bufsize, tmp,
						 sizeof (tmp));
	//dynVarLog(__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO, " decoded %d samples from func %s and line %d", samples, func, line);

	if (samples > 0)
	{
		rc = arc_audio_frame_apply (&gCall[zCall].audio_frames[which], tmp,
									samples * 2, op);
	}

	return rc;
}								// END: arc_frame_apply()

///This function speaks a file using rtp_session_send_with_ts().
int
speakFile (int sleep_time,
		   int interrupt_option,
		   char *zFileName,
		   int synchronous,
		   int zCall,
		   int zAppRef,
		   RtpSession * zSession,
		   int *zpTermReason, struct MsgToApp *zResponse)
{
	char            mod[] = { "speakFile" };
	int             yRc = 0;
	int             yRtpRc = 0;
	int             i;
	int				rc;
	int             infile;

	char            lRemoteRtpIp[50];
	int             lRemoteRtpPort;
	struct MsgToApp response;
	char            yTmpBuffer4K[160 * 40 + 1] = "";
	char            buffer[160 * 40 + 1] = "";
	char            leftOverBuffer[160 * 40 + 1] = "";
	char           *zQuery;
	const unsigned char *rtpHeader;
	unsigned long   l_dwNewRTPTimestamp = 0;
	unsigned long   l_dwDelayTime = sleep_time;
	unsigned long   m_dwLastRTPTimestamp = 0;
	int             skipPaused = 0;

	// checks for prompt playback 
	char yStrTmpPlaybackDTMF[10] = "";
	char *pchTerminate = NULL;
	char *pchRewind = NULL;
	char *pchForward = NULL;
	char *pchBackward = NULL;
	char *pchPause = NULL;
	char *pchVolumeUp = NULL;
	char *pchVolumeDown = NULL;
	char *pchSpeedUp = NULL;
	char *pchSpeedDown = NULL;

	// speed up down
	float           speed_lookup[21] = { -50.0, -45.0, -40.0, -35.0,
		-30.0, -25.0, -20.0, -15.0,
		-10.0, -5.0, 0.0, 5.0,
		10.0, 15.0, 20.0, 25.0,
		30.0, 35.0, 40.0, 45.0, 50.0
	};

	// volume up down
	float           volume_lookup[] = { 0.1, 0.2, 0.4, 0.6, 0.8,
		1.0, 1.3, 1.6, 1.9, 2.2, 2.5
	};

	int             speedDoNext = 0;
	float           CurrentSpeed = 0.0;
	float           CurrentVolume = 0.0;
	char            speedBuff[1024];
	int             speedBytes = 0;
	int             outBytes = 0;
	int             speedSleep = 1;
	int             speedReset = 0;
	int             speedSet = 0;

	// 
	int             yIntRemainingBytes = 0;
	int             yIntTotalSize = 0;
	int             yIntStreamingFinished = 0;

    long    rcSeek;
    long    threeSecondsBack = 0;

	// these turn on options for the duration of this thread

	enum audio_processing_options_e
	{
		SPEAKFILE_AUDIO_PROCESS_VOLUME = (1 << 0),
		SPEAKFILE_AUDIO_PROCESS_SPEED = (1 << 1),
		SPEAKFILE_AUDIO_PROCESS_MIXING = (1 << 2)
	};

	int             audio_opts = 0;

	// and for each new prompt

	int             skipForward = 0;
	int             playBackKeyPressed = 0;

	response.message[0] = '\0';
	gCall[zCall].playBackOn = 0;
	gCall[zCall].playBackControl[0] = '\0';

	// 
	// if we enter 
	// playfile  with speed set to something other that
	// 5 start the prompt control 
	//

	memset (speedBuff, 127, sizeof (speedBuff));

	if (gCall[zCall].GV_Speed != 0)
	{

	int             index = zCall % 48;
	int             speed_idx;

		if (speedSet == 0)
		{
			promptControl[index] = new ArcPromptControl;
			promptControl[index]->set ();
			speedSet++;
		}

		if (gCall[zCall].GV_Speed > 10)
		{
			gCall[zCall].GV_Speed = 10;
		}

		if (gCall[zCall].GV_Speed < -10)
		{
			gCall[zCall].GV_Speed = -10;
		}

		speed_idx = (gCall[zCall].GV_Speed + 10) % 21;

		CurrentSpeed = speed_lookup[speed_idx];
		CurrentSpeed = promptControl[index]->setSpeed (CurrentSpeed);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Speed adjusted for port (%d); current speed=%f on playfile entry $SPEED=%d.",
				   zCall, CurrentSpeed, gCall[zCall].GV_Speed);

		audio_opts |= SPEAKFILE_AUDIO_PROCESS_SPEED;
		speedReset = 1;
	}


    arc_rtp_session_set_ssrc(zCall, zSession);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "interrupt_option=%d, FIRST_PARTY_PLAYBACK_CONTROL=%d",
			   interrupt_option, FIRST_PARTY_PLAYBACK_CONTROL);

	if (interrupt_option == FIRST_PARTY_PLAYBACK_CONTROL ||
		interrupt_option == PLAYBACK_CONTROL ||
		interrupt_option == SECOND_PARTY_PLAYBACK_CONTROL)
	{
		gCall[zCall].playBackOn = 1;

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "PLAYBACK CONTROL is ON", "",
					  zCall);

	}
	else
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "PLAYBACK CONTROL is OFF", "",
					  zCall);
	}

    gCall[zCall].currentSpeakFd =
        arc_open (zCall, mod, __LINE__, zFileName, O_RDONLY, ARC_TYPE_FILE);
    gCall[zCall].bytesToBeSkipped = 0;

    dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
               "currentSpeakFd=%d, zFileName=%s", gCall[zCall].currentSpeakFd,
               zFileName);

//Code for dynamically checking rtp header and setting the flag for the stack
	BYTE            l_caLength[2];
	int             num_frames_per_buffer = 30;
	int             samples_per_frame = 8;
	int             bytes_per_sample = 2;
	bool            isRTPHeaderIncluded = false;

	int             l_iCount =
		num_frames_per_buffer * samples_per_frame * bytes_per_sample;

	int             l_iBytesRead =
		read (gCall[zCall].currentSpeakFd, l_caLength, 2);

	if (l_iBytesRead != 2)
	{
		isRTPHeaderIncluded = false;
	}
	else
	{
		l_iBytesRead = (l_caLength[0] << 8) | l_caLength[1];

		if (l_iBytesRead < 12 || l_iBytesRead > l_iCount + 12)	// RTP header is at least 12 bytes
		{
			dynVarLog(__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, WARN,
					   "Audio file doesn't have RTP header.");
			isRTPHeaderIncluded = false;
		}
		else
		{
			isRTPHeaderIncluded = true;
			gCall[zCall].bytesToBeSkipped = 14;
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Audio file has RTP header, bytesToBeSkipped=%d.",
					   gCall[zCall].bytesToBeSkipped);
		}
	}

	rc = lseek (gCall[zCall].currentSpeakFd, 0, SEEK_SET);

	if (isRTPHeaderIncluded == true)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling speakRtpFile");

		speakRtpFile (sleep_time, interrupt_option, zFileName,
					  synchronous, zCall, zAppRef, gCall[zCall].rtp_session, zpTermReason,
					  zResponse);
	}
	else
	{
		memset (yTmpBuffer4K, 0, sizeof (yTmpBuffer4K));

		if (gCall[zCall].currentSpeakFd < 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR,
					   "open(%s) failed. [%d, errno=%d errstr=%s] Unable to speak file(%s).",
					   zFileName, zCall, errno, strerror (errno), zFileName);
			return (-7);
		}

		*zpTermReason = TERM_REASON_DEV;

		if (gCall[zCall].bytesToBeSkipped == 0)
		{
			if (gCall[zCall].speakStarted == 1)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Calling removeHeader.");
				removeHeader (zCall, gCall[zCall].currentSpeakFd);
			}
			else
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Calling changeHeader.");

				//changeHeader(zCall, gCall[zCall].currentSpeakFd);
				removeHeader (zCall, gCall[zCall].currentSpeakFd);
				gCall[zCall].speakStarted = 1;
			}
		}

		//rtp_session_reset(gCall[zCall].rtp_session);
		if (gCall[zCall].GV_Volume != 5)
		{
			arc_frame_reset_with_silence (zCall, AUDIO_OUT, 1,
										  (char *) __func__, __LINE__);
			CurrentVolume =
				arc_audio_frame_adjust_gain (&gCall[zCall].
											 audio_frames[AUDIO_OUT],
											 volume_lookup[gCall[zCall].
														   GV_Volume],
											 ARC_AUDIO_GAIN_ADJUST_EXPLICIT);
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Global Volume setting is set on playfile entry, Value=[%d] Current Volume=%f.",
					   gCall[zCall].GV_Volume, CurrentVolume);
			audio_opts |= SPEAKFILE_AUDIO_PROCESS_VOLUME;
		}
		else
		{
			arc_frame_reset_with_silence (zCall, AUDIO_OUT, 1,
										  (char *) __func__, __LINE__);
			CurrentVolume =
				arc_audio_frame_adjust_gain (&gCall[zCall].
											 audio_frames[AUDIO_OUT], 1.0,
											 ARC_AUDIO_GAIN_ADJUST_EXPLICIT);
			gCall[zCall].GV_Volume = 5;
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Global Volume setting is set to default playfile entry, Value=[%d] Current Volume=%f.",
					   gCall[zCall].GV_Volume, CurrentVolume);
			audio_opts |= SPEAKFILE_AUDIO_PROCESS_VOLUME;
		}
		//arc_audio_frame_reset(&gCall[zCall].audio_frames[AUDIO_OUT]);

		if (gCall[zCall].silentRecordFlag != 0)
		{
			audio_opts |= SPEAKFILE_AUDIO_PROCESS_MIXING;
		}

    	if ( ( gCall[zCall].currentPhraseOffset > 0 ) &&
		     ( gCall[zCall].GV_LastPlayPostion == 1 ) )
	    {
			if ( gCall[zCall].currentSpeakSize > 40960 )
			{
	        	errno=0;
		        if ((rc = lseek(gCall[zCall].currentSpeakFd,
	                             gCall[zCall].currentPhraseOffset, SEEK_SET)) < 0)
		        {
		            dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
		                    "lseek(fd=%d, SEEK_SET) failed.  [%d, %s] "
		                    "Unable to seek to last position.",
   	                 gCall[zCall].currentSpeakFd, errno, strerror(errno));
		        }
		        else
		        {
		            dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		                    "Successfully set current offset of fd (%d) to %ld.",
		                    gCall[zCall].currentSpeakFd,
		                    gCall[zCall].currentPhraseOffset);
		        }
			}
			else
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					"Current size for file fd (%d) is %d.  Too small to set the offset.", 
					gCall[zCall].currentSpeakFd,
					gCall[zCall].currentSpeakSize);
			}
	    }

        gCall[zCall].in_speakfile++;
	
		while (1)
		{


			if (!canContinue (mod, zCall, __LINE__))
			{
				yRc = -3;

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
						   INFO, "Calling break; cannot continue.");

				break;
			}
			else if (gCall[zCall].pendingOutputRequests > 0)
			{

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "",
							  "Pending requests, total",
							  gCall[zCall].pendingOutputRequests);

				*zpTermReason = TERM_REASON_USER_STOP;
				yRc = ACTION_GET_NEXT_REQUEST;

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
						   INFO, "Calling break; pendingOutputRequests = %d", 
							gCall[zCall].pendingOutputRequests );

				if (strcmp (gCall[zCall].audioCodecString, "729b") == 0 &&
					gCall[zCall].isG729AnnexB == YES)
				{
					gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
							   INFO, "Sending silence = %s",
							   g729b_silenceBuffer);

					yRtpRc =
						arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													  gCall[zCall].rtp_session,
													  g729b_silenceBuffer,
													  sizeof
													  (g729b_silenceBuffer),
													  gCall[zCall].
													  out_user_ts);
				}

				break;
			}

			if (gCall[zCall].speechRec == 0 &&
				gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
			{
				//gCall[zCall].in_user_ts+=(gCall[zCall].inTsIncrement*3);

				gCall[zCall].in_user_ts += (gCall[zCall].codecBufferSize * 3);

				sprintf (zResponse->message, "\0");

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "DTMF",
							  gCall[zCall].lastDtmf);

				/* Now check to see if we need to do PLAYBACK_CONTROL */
				if (gCall[zCall].playBackOn == 1 || gCall[zCall].GV_PlaybackAndSpeechRec == 1)
				{
					yStrTmpPlaybackDTMF[0] = '\0';
					pchTerminate = NULL;
					pchRewind = NULL;
					pchForward = NULL;
					pchBackward = NULL;
					pchPause = NULL;
					pchVolumeUp = NULL;
					pchVolumeDown = NULL;
					pchSpeedUp = NULL;
					pchSpeedDown = NULL;

					sprintf (yStrTmpPlaybackDTMF, "%s",
							 gCall[zCall].playBackControl);


					sprintf (gCall[zCall].playBackControl, "%s%c",
							 yStrTmpPlaybackDTMF,
							 dtmf_tab[gCall[zCall].lastDtmf]);

					__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
								  gCall[zCall].playBackControl,
								  gCall[zCall].GV_VolumeUpKey, zCall);

					__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
								  gCall[zCall].playBackControl,
								  gCall[zCall].GV_SkipTerminateKey, zCall);

					pchTerminate =
						gCall[zCall].
						GV_SkipTerminateKey[0] ? strstr (gCall[zCall].
														playBackControl,
														gCall[zCall].  GV_SkipTerminateKey) : NULL;

#if 0
					pchTerminate =
						strstr (gCall[zCall].playBackControl,
								gCall[zCall].GV_SkipTerminateKey);
#endif

					pchRewind = 
						gCall[zCall].GV_SkipRewindKey[0] ? 
						strstr (gCall[zCall].playBackControl,
								gCall[zCall].GV_SkipRewindKey) : NULL;

					pchBackward =
						gCall[zCall].
						GV_SkipBackwardKey[0] ? strstr (gCall[zCall].
														playBackControl,
														gCall[zCall].
														GV_SkipBackwardKey) :
						NULL;

					pchForward =
						gCall[zCall].
						GV_SkipForwardKey[0] ? strstr (gCall[zCall].
													   playBackControl,
													   gCall[zCall].
													   GV_SkipForwardKey) :
						NULL;

					pchPause =
						gCall[zCall].GV_PauseKey[0] ? strstr (gCall[zCall].
															  playBackControl,
															  gCall[zCall].
															  GV_PauseKey) :
						NULL;


					pchVolumeUp =
						gCall[zCall].GV_VolumeUpKey[0] ? 
						strstr (gCall[zCall].playBackControl, gCall[zCall].GV_VolumeUpKey)
						: NULL;


					pchVolumeDown =
						gCall[zCall].
						GV_VolumeDownKey[0] ? strstr (gCall[zCall].
													  playBackControl,
													  gCall[zCall].
													  GV_VolumeDownKey) :
						NULL;

					pchSpeedUp =
						gCall[zCall].GV_SpeedUpKey[0] ? strstr (gCall[zCall].
																playBackControl,
																gCall[zCall].
																GV_SpeedUpKey)
						: NULL;

					pchSpeedDown =
						gCall[zCall].
						GV_SpeedDownKey[0] ? strstr (gCall[zCall].
													 playBackControl,
													 gCall[zCall].
													 GV_SpeedDownKey) : NULL;




					if (pchTerminate != NULL)
					{
						playBackKeyPressed = 1;

						__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "",
									  "SKIP_TERMINATE_KEY pressed", zCall);

						yIntRemainingBytes = 0;
						yIntTotalSize = 0;
						yIntStreamingFinished = 0;
						i = 0;

						if (gCall[zCall].rtp_session != NULL)
						{
							yRc =
								arc_rtp_session_send_with_ts (mod, __LINE__,
															  zCall, gCall[zCall].rtp_session,
															  gCall[zCall].
															  silenceBuffer,
															  gCall[zCall].
															  codecBufferSize,
															  gCall[zCall].
															  out_user_ts);

							if (yRc < 0)
							{
							}

							//gCall[zCall].out_user_ts+=(gCall[zCall].outTsIncrement);
						}

						*zpTermReason = TERM_REASON_DEV;
						yRc = ACTION_GET_NEXT_REQUEST;
						__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "",
									  "TERM_REASON_DEV", zCall);

						gCall[zCall].playBackControl[0] = '\0';

						break;
					}

					/*
						Prompt Control 
					*/

					if (pchBackward != NULL)
					{
						__DDNDEBUG__(DEBUG_MODULE_AUDIO, "", "SKIP_BACKWARD_KEY pressed", zCall);

						//threeSecondsBack = gCall[zCall].playBackIncrement * -3;
						threeSecondsBack = gCall[zCall].playBackIncrement * gCall[zCall].GV_SkipTimeInSeconds * -1;
						dynVarLog( __LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
							"SKIP_BACKWARD_KEY pressed. Backing up %d seconds (%d * %d).",
							 gCall[zCall].GV_SkipTimeInSeconds, gCall[zCall].playBackIncrement,
							 gCall[zCall].GV_SkipTimeInSeconds);
						errno = 0;
						if ((rcSeek = lseek(gCall[zCall].currentSpeakFd, threeSecondsBack, SEEK_CUR)) < 0)
						{
							dynVarLog(__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, WARN,
								"Unable to skip backward.  lseek(%d, %ld, SEEK_CUR) failed. "
								"rc=%ld [%d, %s]",
								gCall[zCall].currentSpeakFd,
								threeSecondsBack, rcSeek, errno, strerror(errno));
						}
						else
						{
							yIntRemainingBytes 		= 0;
							yIntTotalSize 			= 0;
//							yIntStreamingFinished 	= 0;
							i = 0;
						}

						gCall[zCall].playBackControl[0] = '\0';
						gCall[zCall].dtmfAvailable = 0;
					}
					else if (pchRewind != NULL)
					{
						__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "",
									  "SKIP_REWIND_KEY pressed", zCall);

						if ((rcSeek = lseek(gCall[zCall].currentSpeakFd, 0, SEEK_SET)) < 0)
						{
							dynVarLog(__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, WARN,
								"Unable to skip backward.  lseek(%d, %ld, SEEK_SET) failed. "
								"rc=%ld [%d, %s]",
								gCall[zCall].currentSpeakFd,
								0, rcSeek, errno, strerror(errno));
						}
						else
						{
							yIntRemainingBytes 		= 0;
							yIntTotalSize 			= 0;
//							yIntStreamingFinished 	= 0;
							i = 0;
						}

						gCall[zCall].playBackControl[0] = '\0';
						gCall[zCall].dtmfAvailable = 0;
					}
					else if (pchForward != NULL)
					{
	
	__DDNDEBUG__(DEBUG_MODULE_AUDIO, "", "SKIP_FORWARD_KEY pressed", zCall);
						playBackKeyPressed = 1;
	
						// skipForward = 100;			// 100 ~ 2 seconds
						skipForward = 50 *  gCall[zCall].GV_SkipTimeInSeconds;       // 100 ~ 2 seconds

						gCall[zCall].playBackControl[0] = '\0';
						gCall[zCall].dtmfAvailable = 0;
						dynVarLog( __LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
								"SKIP_FORWARD_KEY  pressed.  "
								"Set skipForward to %d ( %d seconds ).", skipForward, gCall[zCall].GV_SkipTimeInSeconds);
					}
					else if (pchPause != NULL)
					{
						playBackKeyPressed = 1;
	__DDNDEBUG__(DEBUG_MODULE_AUDIO, "", "PAUSE_KEY pressed", zCall);
				
						skipPaused = 1;
						dynVarLog( __LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
						"PAUSE_KEY pressed (%s).  "
						"Set skipPaused to %d.", pchPause,skipPaused );

	__DDNDEBUG__(DEBUG_MODULE_AUDIO, "", "RESUME_KEY pressed", zCall);
	
						gCall[zCall].playBackControl[0] = '\0';
	//printf("RGD ::%d::got DTMF setting dtmfAvailable = 0\n", __LINE__);fflush(stdout);
						gCall[zCall].dtmfAvailable = 0;
	
					}

					/*
                        Volume Control
                    */

					else if (pchVolumeUp != NULL)
					{
						float current;

						playBackKeyPressed = 1;
						if (gCall[zCall].GV_Volume < 10)
						{
							gCall[zCall].GV_Volume++;
						}
						else
						{
							gCall[zCall].GV_Volume = 10;
						}
						//volumeChangeCallback((float)gCall[zCall].GV_Volume);
						current =
							arc_audio_frame_adjust_gain (&gCall[zCall].audio_frames[AUDIO_OUT],
														 volume_lookup[gCall[zCall].GV_Volume],
														 ARC_AUDIO_GAIN_ADJUST_EXPLICIT);
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Volume adjusted up. Current volume=%f.",
								   current);
						gCall[zCall].playBackControl[0] = '\0';
						gCall[zCall].dtmfAvailable = 0;
						audio_opts |= SPEAKFILE_AUDIO_PROCESS_VOLUME;
					}
					else if (pchVolumeDown != NULL)
					{
						float current;

						playBackKeyPressed = 1;
						if (gCall[zCall].GV_Volume > 0)
						{
							gCall[zCall].GV_Volume--;
						}
						else
						{
							gCall[zCall].GV_Volume = 0;
						}
						//volumeChangeCallback((float)gCall[zCall].GV_Volume);
						current =
							arc_audio_frame_adjust_gain (&gCall[zCall].audio_frames[AUDIO_OUT],
														 volume_lookup[gCall[zCall].GV_Volume],
														 ARC_AUDIO_GAIN_ADJUST_EXPLICIT);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   DYN_BASE, INFO,
								   "Volume adjusted down. Current volume=%f.",
								   current);

						gCall[zCall].playBackControl[0] = '\0';
						gCall[zCall].dtmfAvailable = 0;
						audio_opts |= SPEAKFILE_AUDIO_PROCESS_VOLUME;
					}


					/*
						Speed Control 
					*/


					else if (pchSpeedUp != NULL)
					{
	int             index = zCall % 48;
	int             speed_idx;

						if (gCall[zCall].GV_Speed > 10)
						{
							gCall[zCall].GV_Speed = 10;
						}

						playBackKeyPressed = 1;

						if (gCall[zCall].GV_Speed < 10)
						{
							gCall[zCall].GV_Speed++;
						}

						if (speedSet == 0)
						{
							promptControl[index] = new ArcPromptControl;
							promptControl[index]->set ();
							speedSet++;
						}

						speed_idx = (gCall[zCall].GV_Speed + 10) % 21;
						CurrentSpeed =
							promptControl[index]->
							setSpeed (speed_lookup[speed_idx]);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Speed adjusted up. Current speed=%f, global speed=%d, speed index=%d.",
								   CurrentSpeed, gCall[zCall].GV_Speed,
								   speed_idx);

						gCall[zCall].playBackControl[0] = '\0';
						gCall[zCall].dtmfAvailable = 0;
						audio_opts |= SPEAKFILE_AUDIO_PROCESS_SPEED;
						speedReset = 1;
					}
					else if (pchSpeedDown != NULL)
					{
	int             index = zCall % 48;
	int             speed_idx;

						playBackKeyPressed = 1;

						if (gCall[zCall].GV_Speed < -10)
						{
							gCall[zCall].GV_Speed = -10;
						}

						// gCall[zCall].GV_Speed--;
						if (gCall[zCall].GV_Speed > -10)
						{
							gCall[zCall].GV_Speed--;
						}

						if (speedSet == 0)
						{
							promptControl[index] = new ArcPromptControl;
							promptControl[index]->set ();
							speedSet++;
						}

						speed_idx = (gCall[zCall].GV_Speed + 10) % 21;
						CurrentSpeed =
							promptControl[index]->
							setSpeed (speed_lookup[speed_idx]);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Speed adjusted down. Current speed=%f, global speed=%d, speed index=%d.",
								   CurrentSpeed, gCall[zCall].GV_Speed,
								   speed_idx);

						gCall[zCall].playBackControl[0] = '\0';
						gCall[zCall].dtmfAvailable = 0;
						audio_opts |= SPEAKFILE_AUDIO_PROCESS_SPEED;
						speedReset = 1;
					}
					else
					{
#if 1
						if (strcmp
							(gCall[zCall].playBackControl,
							 gCall[zCall].GV_SkipBackwardKey) == 0
							|| strcmp (gCall[zCall].playBackControl,
									   gCall[zCall].GV_SkipForwardKey) == 0
							|| strcmp (gCall[zCall].playBackControl,
									   gCall[zCall].GV_PauseKey) == 0
							|| strcmp (gCall[zCall].playBackControl,
									   gCall[zCall].GV_VolumeUpKey) == 0
							|| strcmp (gCall[zCall].playBackControl,
									   gCall[zCall].GV_VolumeDownKey) == 0
							|| strcmp (gCall[zCall].playBackControl,
									   gCall[zCall].GV_SpeedUpKey) == 0
							|| strcmp (gCall[zCall].playBackControl,
									   gCall[zCall].GV_SpeedDownKey) == 0)
#endif
						{
							playBackKeyPressed = 1;
						}
						else /*First key of multiple keys pressed */
						if (gCall[zCall].playBackControl[0] ==
							gCall[zCall].GV_SkipBackwardKey[0] ||
							gCall[zCall].playBackControl[0] ==
							gCall[zCall].GV_SkipForwardKey[0]
							|| gCall[zCall].playBackControl[0] ==
							gCall[zCall].GV_PauseKey[0]
							|| gCall[zCall].playBackControl[0] ==
							gCall[zCall].GV_VolumeUpKey[0]
							|| gCall[zCall].playBackControl[0] ==
							gCall[zCall].GV_VolumeDownKey[0]
							|| gCall[zCall].playBackControl[0] ==
							gCall[zCall].GV_SpeedUpKey[0]
							|| gCall[zCall].playBackControl[0] ==
							gCall[zCall].GV_SpeedDownKey[0]
							|| gCall[zCall].playBackControl[0] ==
							gCall[zCall].GV_SkipTerminateKey[0])
						{
							playBackKeyPressed = 1;
							gCall[zCall].dtmfAvailable = 0;
						}
						else if (gCall[zCall].GV_PlaybackAndSpeechRec == 1)
						{
							playBackKeyPressed = 0;
							gCall[zCall].playBackControl[0] = '\0';

							yStrTmpPlaybackDTMF[0] = '\0';
							pchTerminate = NULL;
							pchRewind = NULL;
							pchForward = NULL;
							pchBackward = NULL;
							pchPause = NULL;
							pchVolumeUp = NULL;
							pchVolumeDown = NULL;
							pchSpeedUp = NULL;
							pchSpeedDown = NULL;
						}
						else
						{
							playBackKeyPressed = 0;
							gCall[zCall].playBackControl[0] = '\0';
							gCall[zCall].dtmfAvailable = 0;
							// not 100% sure shy this was here 
							// speedEnabled = 1;
						}
					}
				} else {
					yStrTmpPlaybackDTMF[0] = '\0';
					pchTerminate = NULL;
					pchRewind = NULL;
					pchForward = NULL;
					pchBackward = NULL;
					pchPause = NULL;
					pchVolumeUp = NULL;
					pchVolumeDown = NULL;
					pchSpeedUp = NULL;
					pchSpeedDown = NULL;
                }

				// end playback control 
				// this check interrupt is done for volume up down purposes 

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					"Interrupt_option(%d), playBackKeyPressed(%d), gCall[%d].GV_DtmfBargeinDigitsInt(%d), "
					"GV_DtmfBargeinDigits=(%s) dtmf_tab[gCall[zCall].lastDtmf]=(%c)",
					interrupt_option, playBackKeyPressed, zCall, gCall[zCall].GV_DtmfBargeinDigitsInt,
					gCall[zCall].GV_DtmfBargeinDigits, dtmf_tab[gCall[zCall].lastDtmf]);

				if (interrupt_option != 0 && interrupt_option != NONINTERRUPT && !playBackKeyPressed &&	/*0: No interrupt */
					((gCall[zCall].GV_DtmfBargeinDigitsInt == 0) ||
					 (gCall[zCall].GV_DtmfBargeinDigitsInt == 1 &&
					  strchr (gCall[zCall].GV_DtmfBargeinDigits,
							  dtmf_tab[gCall[zCall].lastDtmf]) != NULL)))
				{
					sprintf (zResponse->message, "%c",
							 dtmf_tab[gCall[zCall].lastDtmf]);

					if (gCall[zCall].dtmfAvailable == 1)
					{
						if (gCall[zCall].GV_BridgeRinging == 1)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Setting dtmfAvailable to 0.");
							gCall[zCall].dtmfAvailable = 0;
						}
						if (gCall[zCall].isBeepActive == 0)
						{
							//
							// BT-38 - removed #if 0 to allow this code
							//         to be executed. It was #ifdef'd out.
							//
							if (gCall[zCall].rtp_session_mrcp == NULL)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, TEL_BASE, INFO,
										   "Setting dtmfAvailable to 0.");
								gCall[zCall].dtmfAvailable = 0;
							}
						}

						if (strcmp (gCall[zCall].audioCodecString, "729b") ==
							0 && gCall[zCall].isG729AnnexB == YES)
						{
							gCall[zCall].out_user_ts +=
								gCall[zCall].outTsIncrement;
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sending silence = %s.",
									   g729b_silenceBuffer);

							yRtpRc =
								arc_rtp_session_send_with_ts (mod, __LINE__,
															  zCall, gCall[zCall].rtp_session,
															  g729b_silenceBuffer,
															  sizeof
															  (g729b_silenceBuffer),
															  gCall[zCall].
															  out_user_ts);
						}
						else if (gCall[zCall].rtp_session != NULL)
						{
#if 0
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "reading audio data of size(%d)",
									   gCall[zCall].codecBufferSize);
#endif

							yRtpRc =
								arc_rtp_session_send_with_ts (mod, __LINE__,
															  zCall, gCall[zCall].rtp_session,
															  gCall[zCall].
															  silenceBuffer,
															  gCall[zCall].
															  codecBufferSize,
															  gCall[zCall].
															  out_user_ts);
							if (yRtpRc < 0)
							{
							}

						}
						*zpTermReason = TERM_REASON_DTMF;

						yRc = ACTION_GET_NEXT_REQUEST;

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO, "Calling break");
						break;
					}
				}
				else
				{
					if (gCall[zCall].rtp_session_mrcp == NULL ||
						interrupt_option == 0
						|| interrupt_option == NONINTERRUPT
						|| (gCall[zCall].GV_DtmfBargeinDigitsInt == 1
							&& strchr (gCall[zCall].GV_DtmfBargeinDigits,
									   dtmf_tab[gCall[zCall].lastDtmf]) ==
							NULL))
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Setting dtmfAvailable to 0.");
						gCall[zCall].dtmfAvailable = 0;
						gCall[zCall].dtmfCacheCount = 0;
					}
				}

				if (gCall[zCall].rtp_session != NULL)
				{
#if 1
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Sending Silence packet");
#endif
					yRtpRc =
						arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													  gCall[zCall].rtp_session,
													  gCall[zCall].
													  silenceBuffer,
													  gCall[zCall].
													  codecBufferSize,
													  gCall[zCall].
													  out_user_ts);

					if (yRtpRc < 0)
					{
					}

					//gCall[zCall].out_user_ts+=(gCall[zCall].outTsIncrement);
				}

			}
			if(skipPaused)
			{
				char yStrPauseFile[400];
				int count = 1;
				int filesFinished = 0;
			
				char    yTmpPauseBuffer4K[160 * 40 + 1];
				char    pauseBuffer[160 * 40 + 1];
							
				int yRemainingBytes      = 0;
				int yTotalSize           = 0;
				int yStreamingFinished   = 0;
		
				int j = 0;
		
				time_t start;
				time_t end = 0;
				int playSilence = 0;
	
				time(&start);
				time(&end);
	
				while(canContinue(mod, zCall, __LINE__) && skipPaused)
				{
					if((end - start) >= gCall[zCall].GV_MaxPauseTime)
					{
						skipPaused = 0;
						break;
					}
	
					time(&end);
									
	
					if(count % 2 == 1)
					{
						yRemainingBytes = 0;
						yTotalSize = 0;
						yStreamingFinished = 0;
	
						sprintf(yStrPauseFile, "%s/PlaybackPauseGreeting_%d.%s",
							gCall[zCall].GV_PlayBackPauseGreetingDirectory,
							count, gCall[zCall].playBackFileExtension);
	
						dynVarLog( __LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
								"yStrPauseFile=%s", yStrPauseFile);
	
						if(access(yStrPauseFile, F_OK) != 0)
						{
							sprintf(yStrPauseFile, "beep.%s", gCall[zCall].playBackFileExtension);
						}
	
						if(access(yStrPauseFile, F_OK) != 0)
						{
							sprintf(yStrPauseFile, "silence.%s", gCall[zCall].playBackFileExtension);
						}
	
						/*
						sprintf(yStrPauseFile, "%s/PlaybackPauseGreeting_%d.g729a",
							"/home/arc/.ISP/Telecom/Exec",
							count);
						*/
	
						playSilence = 0;
					}
					else
					{
						yRemainingBytes = 0;
						yTotalSize = 0;
						yStreamingFinished = 0;
						playSilence = gCall[zCall].GV_PlayBackBeepInterval*50;
	
						sprintf(yStrPauseFile, "no_file_silence.%s", gCall[zCall].playBackFileExtension);
					}
	
					if(gCall[zCall].pauseFileFd > -1)
					{
						arc_close(zCall, mod, __LINE__, &(gCall[zCall].pauseFileFd));
						gCall[zCall].pauseFileFd = -1;
					}
						
					dynVarLog( __LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
								"Going to open yStrPauseFile=%s", yStrPauseFile);
	
					if(playSilence == 1)
					{
						gCall[zCall].pauseFileFd = -1;
					}
					else
					{
						gCall[zCall].pauseFileFd = arc_open(zCall, mod, __LINE__, 
										yStrPauseFile, O_RDONLY, ARC_TYPE_FILE);	
					}
					
					if( gCall[zCall].pauseFileFd == -1)/*CPU Usage*/
					{
						usleep(20 * 1000);
					}
	
					memset(yTmpPauseBuffer4K, 0, sizeof(yTmpPauseBuffer4K));
	
					while(canContinue(mod, zCall, __LINE__) && skipPaused)
					{
						if(gCall[zCall].pauseFileFd > -1)
						{
							/*
							 *  Play the pause greeting file and the beep until it is done.
							 */
		
							if(gCall[zCall].dtmfAvailable == 1)
							{
								/*
								 *  Check to see if we have gotten the RESUME_KEY.
								 *  If we have then exit this loop and stop playing
								 *  pause greeting and beep.
								 */
		
								char yStrTmpPlaybackDTMF[256];	
									
								sprintf(yStrTmpPlaybackDTMF, "%s", 
									gCall[zCall].playBackControl);
		
								sprintf(gCall[zCall].playBackControl, "%s%c", 
									yStrTmpPlaybackDTMF, dtmf_tab[gCall[zCall].lastDtmf]);
			
								/*
								 *  Don't forget to reset the values of playBackControlBuffer
								 *  and gCall[zCall].dtmfAvailable.
								 */
			
								skipPaused = 0;
	//printf("RGD ::%d::got DTMF setting dtmfAvailable = 0\n", __LINE__);fflush(stdout);
								gCall[zCall].dtmfAvailable = 0;
								gCall[zCall].playBackControl[0] = '\0';
								break;
							}
		
							if(yRemainingBytes >= gCall[zCall].codecBufferSize)
							{
	
								memcpy(pauseBuffer, 
									yTmpPauseBuffer4K + (yTotalSize - yRemainingBytes), gCall[zCall].codecBufferSize);
	
								yRemainingBytes-=gCall[zCall].codecBufferSize;
	
								j = gCall[zCall].codecBufferSize;
							}
							else
							if(yRemainingBytes <= 0)
							{
								if(yStreamingFinished == 1)
								{
									break;
								}
								
								if(playSilence == 0)
								{
									yTotalSize  = read(gCall[zCall].pauseFileFd,  yTmpPauseBuffer4K, 4096);
								}
								else
								{
									yTotalSize = 4096;
								}
								
								if(yTotalSize <= 0)
								{
									break;
								}
		
								yRemainingBytes = yTotalSize;
	
								if(yTotalSize < 4096)
								{
									yStreamingFinished = 1;
								}
	
								usleep(20 * 1000);
								continue;
							}
							else
							{
								memcpy(pauseBuffer, yTmpPauseBuffer4K + (yTotalSize - yRemainingBytes), yRemainingBytes);
		
								j = yRemainingBytes;
		
								yRemainingBytes = 0;
	
								yTotalSize = read(gCall[zCall].pauseFileFd, yTmpPauseBuffer4K, 4096);
		
								memcpy(pauseBuffer+j, yTmpPauseBuffer4K, gCall[zCall].codecBufferSize - j);
		
								yRemainingBytes = yTotalSize - (gCall[zCall].codecBufferSize - yRemainingBytes - j);
									
								if(yTotalSize <= 0)
								{
									//count++;
									//break;
								}
	
								if(yTotalSize < 4096)
								{
									yStreamingFinished = 1;
								}
							}
					
							if(j >= 1)
							{
								gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;
	
								if(gCall[zCall].rtp_session != NULL)
								{
				
									yRtpRc = arc_rtp_session_send_with_ts(	
										mod, __LINE__, zCall,
										gCall[zCall].rtp_session,
										pauseBuffer,
										gCall[zCall].codecBufferSize,
										gCall[zCall].out_user_ts);
									
								}
	
								if (yRtpRc <= 0)
								{
									util_sleep(0, 100);
									yRc = ACTION_GET_NEXT_REQUEST;
									dynVarLog(__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, INFO, 
												"break in speakStream");
									break;
								}
	
							}
							else
							{
								dynVarLog(__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, INFO, 
											"break in speakStream");
								break;
							}
	
	#if 0
							arcDynamicSleep(sleep_time, 
								&gCall[zCall].m_dwPrevGrabTime_out, 
								&gCall[zCall].m_dwExpectGrabTime_out);
	#endif
				
							//rtpSleep(l_dwDelayTime, &gCall[zCall].lastOutRtpTime);
							rtpSleep_withbreaks(l_dwDelayTime, &gCall[zCall].lastOutRtpTime, zCall, interrupt_option, __LINE__);
						}
						else
						{
							/*
							 *  Play the pause greeting file and the beep until it is done.
							 */
		
							if(gCall[zCall].dtmfAvailable == 1)
							{
								/*
								 *  Check to see if we have gotten the RESUME_KEY.
								 *  If we have then exit this loop and stop playing
								 *  pause greeting and beep.
								 */
		
								char yStrTmpPlaybackDTMF[256];	
									
								sprintf(yStrTmpPlaybackDTMF, "%s", 
									gCall[zCall].playBackControl);
		
								sprintf(gCall[zCall].playBackControl, "%s%c", 
									yStrTmpPlaybackDTMF, dtmf_tab[gCall[zCall].lastDtmf]);
			
								/*
								 *  Don't forget to reset the values of playBackControlBuffer
								 *  and gCall[zCall].dtmfAvailable.
								 */
			
								skipPaused = 0;
	//printf("RGD ::%d::got DTMF setting dtmfAvailable = 0\n", __LINE__);fflush(stdout);
								gCall[zCall].dtmfAvailable = 0;
								gCall[zCall].playBackControl[0] = '\0';
								dynVarLog(__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, INFO, 
											"break in speakStream");
								break;
							}
		
								gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;
	
								if(gCall[zCall].rtp_session != NULL)
								{
				
									if(playSilence > 0)
									{
										playSilence --;
	
										yRtpRc = arc_rtp_session_send_with_ts(	
											mod, __LINE__, zCall,
											gCall[zCall].rtp_session,
											gCall[zCall].silenceBuffer,
											gCall[zCall].codecBufferSize,
											gCall[zCall].out_user_ts);
										
										if(playSilence <= 0)
										{
											dynVarLog(__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, INFO, 
												"break in speakStream");
											break;	
										}
									}
									else
									{
										yRtpRc = arc_rtp_session_send_with_ts(	
											mod, __LINE__, zCall,
											gCall[zCall].rtp_session,
											pauseBuffer,
											gCall[zCall].codecBufferSize,
											gCall[zCall].out_user_ts);
									}
								}
	
								if (yRtpRc <= 0)
								{
									util_sleep(0, 100);
									yRc = ACTION_GET_NEXT_REQUEST;
									dynVarLog(__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, INFO, 
										"break in speakStream");
									break;
								}
	
							rtpSleep(20, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);
						}
	
					}/*	
					  *  END: while(canContinue(mod, zCall) && skipPaused) 
					  *  for playing the pause files.
					  */
					
					if(playSilence == 0)
					{	
						count ++;
					}
	
				}/*END: while(canContinue(mod, zCall) && skipPaused)*/
	
			}/*END: if(skipPaused)*/

			if (yIntRemainingBytes >=
				gCall[zCall].codecBufferSize + gCall[zCall].bytesToBeSkipped)
			{
				memcpy (buffer,
						yTmpBuffer4K + (yIntTotalSize - yIntRemainingBytes) +
						gCall[zCall].bytesToBeSkipped,
						gCall[zCall].codecBufferSize);

				yIntRemainingBytes -=
					gCall[zCall].codecBufferSize +
					gCall[zCall].bytesToBeSkipped;

				i = gCall[zCall].codecBufferSize;	//+ gCall[zCall].bytesToBeSkipped;
			}
			else if (yIntRemainingBytes <= 0)
			{
				if (yIntStreamingFinished == 1)
				{

					if (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED)
					{
						// check for more audio buffer 
						;;
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO, "Calling break");
						break;
					}
				}

				yIntTotalSize =
					read (gCall[zCall].currentSpeakFd, yTmpBuffer4K, 4096);

				if (yIntTotalSize <= 19)
				{
					if (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED)
					{
						// check for more audio buffer 
	int             idx = zCall % 48;

						promptControl[idx]->flush ();
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO, "Calling break");
						break;
					}
				}

				yIntRemainingBytes = yIntTotalSize;

				if (yIntTotalSize < 4096)
				{

					if (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED)
					{
						;;
					}
					else
					{
						yIntStreamingFinished = 1;
					}
				}

				if (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED)
				{
					;;
				}
				else
				{
					continue;
				}

			}
			else
			{

				if (yIntRemainingBytes <= gCall[zCall].bytesToBeSkipped)
				{
	int             tempBytesToBeSkipped =
		gCall[zCall].bytesToBeSkipped - yIntRemainingBytes;

					/*
					   if(skipForward)
					   {
					   skipForward = 0;
					   lseek(gCall[zCall].currentSpeakFd,  1000, SEEK_CUR);
					   }
					 */

					yIntTotalSize =
						read (gCall[zCall].currentSpeakFd, yTmpBuffer4K,
							  4096);

					memcpy (buffer, yTmpBuffer4K + tempBytesToBeSkipped,
							gCall[zCall].codecBufferSize);

					yIntRemainingBytes = 0;

					yIntRemainingBytes =
						yIntTotalSize - (gCall[zCall].codecBufferSize +
										 tempBytesToBeSkipped);

					if (yIntTotalSize <= 19)
					{

						if (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED)
						{
							// flush audio buffer 
	int             idx = zCall % 48;

							promptControl[idx]->flush ();
							// fprintf(stderr, " %s(%d) promptControl has been flushed %d samples remaining\n", __func__, __LINE__, promptControl[idx]->getRemaining());

						}
						else
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO, "Calling break");
							break;
						}
					}

					if (yIntTotalSize < 4096)
					{
						yIntStreamingFinished = 1;
					}
				}
				else
				{
					memcpy (buffer,
							yTmpBuffer4K + (yIntTotalSize -
											yIntRemainingBytes) +
							gCall[zCall].bytesToBeSkipped,
							yIntRemainingBytes -
							gCall[zCall].bytesToBeSkipped);

					i = yIntRemainingBytes - gCall[zCall].bytesToBeSkipped;

					yIntRemainingBytes = 0;

					/*
					   if(skipForward)
					   {
					   skipForward = 0;
					   lseek(gCall[zCall].currentSpeakFd,  1000, SEEK_CUR);
					   }
					 */

					yIntTotalSize =
						read (gCall[zCall].currentSpeakFd, yTmpBuffer4K,
							  4096);

					memcpy (buffer + i, yTmpBuffer4K,
							gCall[zCall].codecBufferSize - i);

					yIntRemainingBytes =
						yIntTotalSize - (gCall[zCall].codecBufferSize -
										 yIntRemainingBytes - i);

					if (yIntTotalSize <= 19)
					{
						if (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED)
						{
							;;
						}
						else
						{
							audio_opts = 0;
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO, "Calling break");
							break;
						}
					}

					if (yIntTotalSize < 4096)
					{
						yIntStreamingFinished = 1;
					}
				}
			}

			// fprintf(stderr, " %s() i = %d at top of the loop\n", __func__, i);   

			if ((audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED))
			{
				if (i < 1)
				{
					i = 1;
				}
			}

			if (i >= 1)
			{
				if(skipForward > 0)
				{
					skipForward--;
					continue;
				}

				if ((audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED) == 0)
				{
					gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;
				}
				else
				{
					if (speedSleep == 1)
					{
						gCall[zCall].out_user_ts +=
							gCall[zCall].outTsIncrement;
					}
				}

				if (gCall[zCall].rtp_session != NULL)
				{

					if (audio_opts)
					{
						arc_frame_decode_and_post (zCall, buffer,
												   gCall[zCall].
												   codecBufferSize, AUDIO_OUT,
												   0, (char *) __func__,
												   __LINE__);
					}

					if (audio_opts & SPEAKFILE_AUDIO_PROCESS_VOLUME)
					{
						arc_frame_apply (zCall, buffer,
										 gCall[zCall].codecBufferSize,
										 AUDIO_OUT,
										 ARC_AUDIO_PROCESS_GAIN_CONTROL,
										 (char *) __func__, __LINE__);
					}

					if (audio_opts & SPEAKFILE_AUDIO_PROCESS_MIXING)
					{
						arc_frame_apply (zCall, buffer,
										 gCall[zCall].codecBufferSize,
										 AUDIO_MIXED,
										 ARC_AUDIO_PROCESS_AUDIO_MIX,
										 (char *) __func__, __LINE__);
					}

					if ((audio_opts
						 && (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED) ==
						 0))
					{
						arc_frame_read_and_encode (zCall, buffer,
												   gCall[zCall].
												   codecBufferSize, AUDIO_OUT,
												   (char *) __func__,
												   __LINE__);
					}
					// else is implied here //
					if (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED)
					{

						if (speedBytes < gCall[zCall].codecBufferSize)
						{
	int             speed_rc = 0;

							//speed_rc = arc_frame_encode_with_speed_ctl(zCall, &speedBuff[speedBytes], sizeof(speedBuff) - speedBytes, AUDIO_OUT, (char *)__func__, __LINE__, &speedDoNext);
							speed_rc =
								arc_frame_encode_with_speed_ctl (zCall,
																 &speedBuff
																 [speedBytes],
																 320,
																 AUDIO_OUT,
																 (char *)
																 __func__,
																 __LINE__,
																 &speedDoNext);

							if (speed_rc > 0)
							{
								speedBytes += speed_rc;
							}
							if (speedDoNext == ARC_SPEED_DO_NOTHING)
							{
								;;	// fprintf(stderr, " %s() ARC_SPEED_DO_NOTHING rc=%d\n", __func__, speedBytes);
							}
							else if (speedDoNext == ARC_SPEED_NEED_MORE)
							{
								;;	//fprintf(stderr, " %s() ARC_SPEED_NEED_MORE doing continue rc=%d\n", __func__, speedBytes);
							}
							else if (speedDoNext == ARC_SPEED_HAVE_MORE)
							{
								;;	// fprintf(stderr, " %s() ARC_SPEED_HAVE_MORE rc=%d\n", __func__, speedBytes);
							}
							else if (speedDoNext == ARC_SPEED_DONE)
							{
								audio_opts = 0;
								// fprintf(stderr, " %s() ARC_SPEED_DONE\n", __func__);
								break;
								;;	// fprintf(stderr, " %s() ARC_SPEED_HAVE_MORE rc=%d\n", __func__, speedBytes);
							}
							else
							{
//								fprintf (stderr,
//										 " %s(%d) hit default case statement, speedDoNext=%d calling break\n",
//										 __func__, __LINE__, speedDoNext);
								break;
							}

						}

						if (speedBytes >= gCall[zCall].codecBufferSize)
						{

							yRtpRc =
								arc_rtp_session_send_with_ts (mod, __LINE__,
															  zCall, gCall[zCall].rtp_session,
															  speedBuff,
															  gCall[zCall].
															  codecBufferSize,
															  gCall[zCall].
															  out_user_ts);

#if 0

							if (speedBytes > 160)
							{
//								fprintf (stderr,
//										 " %s() have too much buffer size(%d)\n",
//										 __func__, speedBytes);
							}

#endif

							if (speedBytes > gCall[zCall].codecBufferSize)
							{
	int             len = speedBytes - gCall[zCall].codecBufferSize;

								memmove (speedBuff,
										 &speedBuff[gCall[zCall].
													codecBufferSize], len);
								speedBytes = len;
							}
							else
							{
								speedBytes = 0;
							}
							speedSleep = 1;
						}
						else
						{
							speedSleep = 0;
						}
					}

					if ((audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED) == 0)
					{

						yRtpRc =
							arc_rtp_session_send_with_ts (mod, __LINE__,
														  zCall, gCall[zCall].rtp_session,
														  buffer,
														  gCall[zCall].
														  codecBufferSize,
														  gCall[zCall].
														  out_user_ts);
					}

					if (gCall[zCall].callSubState ==
						CALL_STATE_CALL_LISTENONLY
						&& gCall[gCall[zCall].crossPort].rtp_session != NULL)
					{
						//gCall[gCall[zCall].crossPort].out_user_ts+=gCall[gCall[zCall].crossPort].outTsIncrement;

						arc_rtp_session_send_with_ts (mod, __LINE__,
													  gCall[zCall].crossPort,
													  gCall[gCall[zCall].
															crossPort].
													  rtp_session, buffer,
													  gCall[gCall[zCall].
															crossPort].
													  codecBufferSize,
													  gCall[gCall[zCall].
															crossPort].
													  out_user_ts);
					}

#if 0
					if (gCall[zCall].isCalea ==
						CALL_STATE_CALL_LISTENONLY_CALEA
						&& gCall[gCall[zCall].caleaPort].rtp_session != NULL)
					{
						gCall[gCall[zCall].caleaPort].out_user_ts +=
							gCall[gCall[zCall].caleaPort].outTsIncrement;
						arc_rtp_session_send_with_ts (mod, __LINE__,
													  gCall[zCall].caleaPort,
													  gCall[gCall[zCall].
															caleaPort].
													  rtp_session, buffer,
													  gCall[gCall[zCall].
															caleaPort].
													  codecBufferSize,
													  gCall[gCall[zCall].
															caleaPort].
													  out_user_ts);
					}
#endif

					if (yRtpRc < 0)
					{
					}

				}

				if ((audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED) == 0)
				{
					if (yRtpRc <= 0)
					{
						util_sleep (0, 100);
						yRc = ACTION_GET_NEXT_REQUEST;
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO, "Calling break;  rc=%d.",
								   yRtpRc);
						break;
					}
				}

			}
			else
			{
				if (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED)
				{
					;;
				}
				else
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Calling break");
					break;
				}
			}

			if (audio_opts & SPEAKFILE_AUDIO_PROCESS_SPEED)
			{
				speedSleep ? rtpSleep (sleep_time,
									   &gCall[zCall].lastOutRtpTime, __LINE__,
									   zCall) : 0;
			}
			else
			{
				rtpSleep (sleep_time, &gCall[zCall].lastOutRtpTime, __LINE__,
						  zCall);
			}

		}						//end while
	}							// end else 

	arc_close (zCall, mod, __LINE__, &(gCall[zCall].currentSpeakFd));

	// reset prompt speed control 

	int             idx = zCall % 48;

	if (idx > -1 && idx < 48)
	{
		if (speedReset)
		{
			// promptControl[idx]->reset();
	delete          promptControl[idx];

			promptControl[idx] = NULL;
		}
	}

	gCall[zCall].currentSpeakFd = -1;
	gCall[zCall].currentSpeakSize = -1;

	if (gCall[zCall].pFirstSpeak != NULL)
	{
		gCall[zCall].pFirstSpeak->isSpeakDone = YES;
	}

    gCall[zCall].in_speakfile = 0;
	return (yRc);

}								/*END: speakFile */

///This function sends dtmf using rtp_session_send_dtmf.
int
speakDtmf (int sleep_time,
		   int interrupt_option,
		   char *zFileName,
		   int synchronous,
		   int zCall,
		   int zAppRef,
		   RtpSession * zSession,
		   int *zpTermReason, struct MsgToApp *zResponse)
{
	int             yRc = 0;
	int             yRtpRc = 0;
	char            mod[] = { "speakDtmf" };
	int             i;

	int             infile;

	char            lRemoteRtpIp[50];
	int             lRemoteRtpPort;
	struct MsgToApp response;
	char            yTmpBuffer4K[160 * 40 + 1];
	char            buffer[160 * 40 + 1];
	char            leftOverBuffer[160 * 40 + 1];
	char           *zQuery;

	int             yIntRemainingBytes = 0;
	int             yIntTotalSize = 0;
	int             yIntStreamingFinished = 0;

	int             skipForward = 0;

	response.message[0] = '\0';
	if (zSession == NULL)
	{
		return (-3);
	}

	*zpTermReason = TERM_REASON_DEV;
	yRc = 0;

	__DDNDEBUG__ (DEBUG_MODULE_RTP, "OUTPUT DTMF <data:port>", zFileName,
				  zCall);

	if (strlen (zFileName) < 8)
	{
		__DDNDEBUG__ (DEBUG_MODULE_RTP,
					  "CANCELLED OUTPUT DTMF , short data, <data:port>",
					  zFileName, zCall);
		return (0);
	}

	for (i = 7; i < strlen (zFileName); i++)
	{
		if (!canContinue (mod, zCall, __LINE__))
		{
			yRc = -3;

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "Can not continue", 0);
			break;
		}
		else if (gCall[zCall].pendingOutputRequests > 0)
		{

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "",
						  "Pending requests, stoping OUTPUTDTMF",
						  gCall[zCall].pendingOutputRequests);

			*zpTermReason = TERM_REASON_USER_STOP;
			yRc = ACTION_GET_NEXT_REQUEST;
			break;
		}

		if (gCall[zCall].speechRec == 0 &&
			gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
		{
			gCall[zCall].in_user_ts += (gCall[zCall].inTsIncrement * 3);
			//gCall[zCall].in_user_ts+=(gCall[zCall].codecBufferSize*3);
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting dtmfAvailable to 0.");
			gCall[zCall].dtmfAvailable = 0;
		}

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Sending DTMF", zFileName, i);

		if (zFileName[i] == ',')
		{
			sleep (1);
			continue;
		}

		rtp_session_send_dtmf (gCall[zCall].rtp_session,
							   zFileName[i], gCall[zCall].out_user_ts);

		gCall[zCall].out_user_ts += (gCall[zCall].outTsIncrement * 3);

		util_sleep (0, sleep_time);

		yRtpRc = arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
											   gCall[zCall].rtp_session,
											   gCall[zCall].silenceBuffer,
											   gCall[zCall].codecBufferSize,
											   gCall[zCall].out_user_ts);

		gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;

		if (yRtpRc <= 0)
		{
			util_sleep (0, 100);
			yRc = ACTION_GET_NEXT_REQUEST;
			break;
		}

		util_sleep (0, sleep_time);

	}							/*END: for */

	return (yRc);

}								/*END: speakDtmf */

int
sendFile (int sleep_time,
		  char *zFileName,
		  int zCall,
		  int zAppRef,
		  RtpSession * zSession,
		  int *zpTermReason, struct MsgToApp *zResponse)
{
	int             yRc = 0;
	int             yRtpRc = 0;
	char            mod[] = { "sendFile" };
	char            buffer[160];
	int             i;
	FILE           *infile;
	char            lRemoteRtpIp[50];
	int             lRemoteRtpPort;
	struct MsgToApp response;
	char            yTmpBuffer4K[4097];
	char           *zQuery;

	response.message[0] = '\0';
	infile = fopen (zFileName, "rb");

	if (infile == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Failed to open send fax file (%s). [%d, %s] Unable to send fax.",
				   zFileName, errno, strerror (errno));
		return (-7);
	}

	*zpTermReason = TERM_REASON_DEV;

	while ((i = fread (buffer, 1, gCall[zCall].codecBufferSize, infile)) > 0)
	{
		if (!canContinue (mod, zCall, __LINE__))
		{
			yRc = -3;

			break;
		}
		else if (gCall[zCall].pendingOutputRequests > 0)
		{

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "Pending requests, total",
						  gCall[zCall].pendingOutputRequests);

			*zpTermReason = TERM_REASON_USER_STOP;
			yRc = ACTION_GET_NEXT_REQUEST;

			break;
		}

		if (i >= 1)
		{
			gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;

			if (gCall[zCall].rtp_session != NULL)
			{
				yRtpRc =
					arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
												  gCall[zCall].rtp_session, buffer, i,
												  gCall[zCall].out_user_ts);
			}

			if (yRtpRc <= 0)
			{
				util_sleep (0, 100);
				yRc = ACTION_GET_NEXT_REQUEST;
				break;
			}

		}
		else
		{
			break;
		}

		util_sleep (0, sleep_time);
	}

	fclose (infile);

	return (yRc);

}								/*END: sendFile */

///This function gives the inputThread its next action.
int
getNextInputAction (int zCall,
					struct InputAction *zAction,
					struct InputAction *zCurrentAction, char *mod)
{

	if (!canContinue (mod, zCall, __LINE__))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PendingInputRequests=%d; setting action to ACTION_EXIT.",
				   gCall[zCall].pendingInputRequests);

		zAction->actionId = ACTION_EXIT;
		return (0);
	}

	if (gCall[zCall].rtp_session_in == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PendingInputRequests=%d; setting action to ACTION_EXIT.",
				   gCall[zCall].pendingInputRequests);

		zAction->actionId = ACTION_EXIT;
		return (0);
	}

	if (gCall[zCall].pendingInputRequests <= 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PendingInputRequests=%d; setting action to ACTION_WAIT.",
				   gCall[zCall].pendingInputRequests);

		zAction->actionId = ACTION_WAIT;
	}
	else if (zCurrentAction == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PendingInputRequests=%d; setting action to ACTION_CONTINUE.",
				   gCall[zCall].pendingInputRequests);

		zAction->actionId = ACTION_CONTINUE;
		zAction->request = gCall[zCall].pLastRequest;
	}
	else if (zCurrentAction->request == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PendingInputRequests=%d; setting action to ACTION_CONTINUE.",
				   gCall[zCall].pendingInputRequests);

		zAction->actionId = ACTION_CONTINUE;
		zAction->request = gCall[zCall].pLastRequest;
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PendingInputRequests=%d; setting action to ACTION_CONTINUE.",
				   gCall[zCall].pendingInputRequests);
		zAction->actionId = ACTION_CONTINUE;
		zAction->request = zCurrentAction->request->nextp;
	}

	return (0);

}								/*int getNextInputAction */

///This function gives the outputThread its next action.
int
getNextOutputAction (int zCall,
					 struct OutputAction *zAction,
					 struct OutputAction *zCurrentAction, char *mod)
{

	if (!canContinue (mod, zCall, __LINE__))
	{
		zAction->actionId = ACTION_EXIT;
		return (0);
	}

	if (gCall[zCall].rtp_session == NULL)
	{
		zAction->actionId = ACTION_EXIT;
		return (0);
	}
	else if (gCall[zCall].restart_rtp_session == 1)
	{
		gCall[zCall].restart_rtp_session = 0;
		zAction->actionId = ACTION_RESTART_OUTPUT_SESSION;
		return (0);
	}

	if (gCall[zCall].pendingOutputRequests <= 0)
	{
		zAction->actionId = ACTION_WAIT;
	}
	else if (zCurrentAction == NULL)
	{
		zAction->actionId = ACTION_CONTINUE;
		zAction->request = gCall[zCall].pLastRequest;
	}
	else if (zCurrentAction->request == NULL)
	{
		zAction->actionId = ACTION_CONTINUE;
		zAction->request = gCall[zCall].pLastRequest;
	}
	else
	{
		zAction->actionId = ACTION_CONTINUE;
		zAction->request = zCurrentAction->request->nextp;
	}

	return (0);

}								/*END: int getNextOutputAction */

int
getNextSpeakDetails (int zCall,
					 struct Msg_Speak *zNewDetails,
					 struct Msg_Speak *zCurrentDetails, char *mod)
{
	return (0);

}								/*END: int getNextSpeakDetails */

///Callback routine for paylaod change
void
recv_payload_change_cb (RtpSession * session, void *user_data)
{
	int            *zpCall = (int *) user_data;
	int             zCall = -1;
	char            mod[] = { "recv_payload_change" };
	char            yMsg[100];

	zCall = *zpCall;

	sprintf (yMsg, "%p", user_data);

	__DDNDEBUG__ (DEBUG_MODULE_RTP, "Receiving payload_change event w/ data ",
				  yMsg, -1);

	return;

}								/*END: void recv_payload_change */

///Callback routine for timestamp jump
//void recv_tsjump_cb(RtpSession *session, gint type, void* user_data)
void
recv_tsjump_cb (RtpSession * session, ...)
{
	//int *zpCall = (int *)user_data;
	//int zCall = *zpCall;
	int            *zpCall;
	int             zCall;
	char            mod[] = { "recv_tsjump_cb" };
	mblk_t         *myBlk = NULL;
	gint            type, *zpType;

	va_list         ap;

	va_start (ap, session);

	zpType = va_arg (ap, gint *);

	type = *zpType;

	zpCall = va_arg (ap, int *);

	zCall = *zpCall;

	va_end (ap);

#if 1
	__DDNDEBUG__ (DEBUG_MODULE_RTP, "Received timestamp jump w/ data", "",
				  (guint32) type);
#endif

	if (gCall[zCall].callState == CALL_STATE_CALL_BRIDGED ||
		gCall[zCall].callSubState == CALL_STATE_CALL_MEDIACONNECTED ||
		gCall[zCall].receivingSilencePackets == 0)
	{
		// what is type here ?
		gCall[zCall].in_user_ts = (guint32) type;
		// old code gCall[zCall].in_user_ts += (guint32) *((int *)(type));
	}

	if (gCall[zCall].codecType == 0)
	{
	char            yStrTmpLogData[256];


		sprintf (yStrTmpLogData, "get_recv_ts=%d in_user_ts=%d",
				 rtp_session_get_current_recv_ts (gCall[zCall].
												  rtp_session_in),
				 gCall[zCall].in_user_ts);

		__DDNDEBUG__ (DEBUG_MODULE_RTP, yStrTmpLogData, "stack_ts",
					  (guint32) type);

		gCall[zCall].in_user_ts = (guint32) type;

		//gCall[zCall].in_user_ts = rtp_session_get_current_recv_ts(gCall[zCall].rtp_session_in);
	}

#if 0
	if (gCall[zCall].runToneThread)
	{
		gCall[zCall].resetRtpSession = 1;
	}
#endif

}								/*END: recv_tsjump_cb */

void
recv_tsjump_cb_tts (RtpSession * session, ...)
{
	//int *zpCall = (int *)user_data;
	//int zCall = *zpCall;
	int            *zpCall;
	int             zCall;
	char            mod[] = { "recv_tsjump_cb_tts" };
	mblk_t         *myBlk = NULL;
	gint            type, *zpType;

	va_list         ap;

	va_start (ap, session);

	zpType = va_arg (ap, gint *);

	type = *zpType;

	zpCall = va_arg (ap, int *);

	zCall = *zpCall;

	va_end (ap);

#if 1
	__DDNDEBUG__ (DEBUG_MODULE_RTP, "Received TTS timestamp jump w/ data", "",
				  (guint32) type);
#endif

	if (gCall[zCall].callState == CALL_STATE_CALL_BRIDGED ||
		gCall[zCall].callSubState == CALL_STATE_CALL_MEDIACONNECTED ||
		gCall[zCall].receivingSilencePackets == 0)
	{
		// what is type here ?
		//gCall[zCall].in_user_ts = (guint32)type;
		// old code gCall[zCall].in_user_ts += (guint32) *((int *)(type));
	}

	if (gCall[zCall].codecType == 0)
	{
	char            yStrTmpLogData[256];

		gCall[zCall].tts_ts = (guint32) type;

		sprintf (yStrTmpLogData, "get_recv_ts=%d tts_ts=%d",
				 rtp_session_get_current_recv_ts (gCall[zCall].
												  rtp_session_mrcp_tts_recv),
				 gCall[zCall].tts_ts);

		__DDNDEBUG__ (DEBUG_MODULE_RTP, yStrTmpLogData, "stack_ts",
					  (guint32) type);

		//gCall[zCall].in_user_ts = rtp_session_get_current_recv_ts(gCall[zCall].rtp_session_in);
	}

}								/*END: recv_tsjump_cb */

///Callback routine for telephony event packet (not the entire event)
void
recv_tevp_cb (RtpSession * session, ...)
{
	int            *zpCall;
	int             zCall;
	char            mod[] = { "recv_tevp_cb" };

	va_list         ap;

	va_start (ap, session);

	mblk_t         *myBlk = NULL;	/*Entire packet */
	mblk_t         *mp = NULL;	/*Data part */
	telephone_event_t *myEvent = NULL;	/*RFC 2833 event structure */
	gint            event = -1;	/*RFC 2833 event value */
	rtp_header_t   *myRtp = NULL;	/*RTP part */

	// myBlk = (mblk_t *)type;

	myBlk = va_arg (ap, mblk_t *);
	zpCall = va_arg (ap, int *);

	zCall = *zpCall;

	va_end (ap);

	//
	// if SIP signaled digits is enabled 
	// do not do rfc 2833 events for this RTP session 
	//
	if (gSipSignaledDigits)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "gSipSignaledDigits is not set returning");
		return;
	}

	if (myBlk == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "myBlk is not set returning");
		return;
	}

	myRtp = (rtp_header_t *) (myBlk->b_rptr);

	if (myRtp == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "myRtp is not set returning");
		return;
	}

	mp = myBlk->b_cont;
	if (mp == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "mp is not set returning");
		return;
	}

	myEvent = (telephone_event_t *) mp->b_rptr;
	if (myEvent == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "myEvent is not set returning");
		return;
	}

	event = myEvent->event;
    // MR 4596 - if (event < 0 || event > 15)
	if (event < 0 || event > 11)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
				   "Invalid event(%d); character(%c).  Returning.", event, dtmf_tab[event]);
		return;
	}

	if (zCall > -1 && zCall < MAX_PORTS)
	{
		if (gCall[zCall].isSendRecvFaxActive == 1)
		{
			return;
		}
	struct timeb    tb;
	double          currentMilliSec = 0;

		ftime (&tb);

		currentMilliSec = (tb.time + ((double) tb.millitm) / 1000) * 1000;

		if (gCall[zCall].lastDtmfTimestamp != myRtp->timestamp)
		{

			if(gCall[zCall].GV_HideDTMF)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Received DTMF (%c) session(%p) ts(%i).", 'X',
					   session, myRtp->timestamp);
			}
			else
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Received DTMF (%c) session(%p) ts(%i).",
					   dtmf_tab[event], session, myRtp->timestamp);
			}

			__DDNDEBUG__ (DEBUG_MODULE_RTP, "Received dtmf event w/ data",
						  "dtmf", dtmf_tab[event]);

			/*Save old DTMF if unprocesssed. */
			if (gCall[zCall].dtmfAvailable)
			{
	int             i = gCall[zCall].dtmfCacheCount;

				dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_DTMF_DATA,
						   INFO, "Found unprocessed DTMF in cache.");

				if (i < 0)
					i = 0;

				gCall[zCall].dtmfInCache[i] = gCall[zCall].lastDtmf;

				gCall[zCall].dtmfCacheCount++;
			}

			gCall[zCall].dtmfAvailable = 1;
			gCall[zCall].lastDtmf = event;

			if (gCall[zCall].callState == CALL_STATE_CALL_BRIDGED &&
				gCall[zCall].callSubState == CALL_STATE_CALL_MEDIACONNECTED)
			{
	char            yStrTmpBuffer[2];

				sprintf (yStrTmpBuffer, "%c\0", dtmf_tab[event]);

				saveBridgeCallPacket (zCall, yStrTmpBuffer, dtmf_tab[event], 0);	/* -1 for NO DTMF */
			}
			util_u_sleep (20 * 1000);

		}
		else
		{

			if(gCall[zCall].GV_HideDTMF)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Ignoring duplicate DTMF (%c) ts(%i).",
					   'X', myRtp->timestamp);
			}
			else
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Ignoring duplicate DTMF (%c) ts(%i).",
					   dtmf_tab[event], myRtp->timestamp);
			}

		}

		gCall[zCall].lastDtmfMilliSec = currentMilliSec;

		gCall[zCall].lastDtmfTimestamp = myRtp->timestamp;
	}
	else
	{
		if(gCall[zCall].GV_HideDTMF)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "Failed to get port number, ignoring DTMF (%c) ts(%i) zCall(%d)",
				   'X',myRtp->timestamp, zCall);
		}
		else
		{
			dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "Failed to get port number, ignoring DTMF (%c) ts(%i) zCall(%d)",
				   dtmf_tab[event], myRtp->timestamp, zCall);
		}
	}

	return;

}								/*END: recv_tevp_cb */

///Callback routine for RFC2833 events
void
recv_tev_cb (RtpSession * session, gint type, void *user_data)
{
	int            *zpCall = (int *) user_data;
	int             zCall = *zpCall;
	char            mod[] = { "recv_tev_cb" };

	if (type < 16)
	{
		if(gCall[zCall].GV_HideDTMF)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Received DTMF (%c).", 'X');
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Received DTMF (%c).", dtmf_tab[type]);
		}

		if (zCall > -1 && zCall < MAX_PORTS)
		{
	struct timeb    tb;
	double          currentMilliSec = 0;

			ftime (&tb);

			currentMilliSec = (tb.time + ((double) tb.millitm) / 1000) * 1000;

			if ((currentMilliSec - gCall[zCall].lastDtmfMilliSec) >
				gInterDigitDelay)
			{

				__DDNDEBUG__ (DEBUG_MODULE_RTP, "Received dtmf event w/ data",
							  "", dtmf_tab[type]);

				gCall[zCall].dtmfAvailable = 1;
				gCall[zCall].lastDtmf = type;
			}

			gCall[zCall].lastDtmfMilliSec = currentMilliSec;
		}
	}

}								/*END: recv_tev_cb */

///This function is a thread that is fired when the call starts for caller input.
/**
The inputThread is fired whenever a call is successfully established by Call
Manager.  Its purpose is to listen to the remote RTP port and grab packets from
it.
*/
void           *
inputThread (void *z)
{
	char            mod[] = { "inputThread" };
	int             rc;
	int             zCall = -1;
	FILE           *fp;
	char            file[256];
	char            message[256];
	char            buffer[256];
	int             i, streamAvailable;

	char            lRemoteRtpIp[50];
	int             lRemoteRtpPort;
    int 			freeDTMFStructs = 0;

	gTotalInputThreads++;

	struct InputAction yInputAction;

	struct MsgToDM *ptrMsgToDM;
	struct Msg_Speak msg;

	struct MsgToDM  yTmpCopyMsgToDM;
	int             yTmpCopyActionId;

	ptrMsgToDM = (struct MsgToDM *) z;
	zCall = ptrMsgToDM->appCallNum;

	gCall[zCall].isInbandToneDetectionThreadStarted = 0;

	sprintf (lRemoteRtpIp, "%s", gCall[zCall].remoteRtpIp);

	lRemoteRtpPort = gCall[zCall].remoteRtpPort;

	if (gCall[zCall].rtp_session_in == NULL)
	{
		gCall[zCall].inputThreadId = 0;

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
	}

	__DDNDEBUG__ (DEBUG_MODULE_RTP, "resetting RTP Session in.", "", -1);
	rtp_session_reset (gCall[zCall].rtp_session_in);
    arc_rtp_session_set_ssrc(zCall, gCall[zCall].rtp_session);

	gCall[zCall].in_user_ts = 0;

	rc = ACTION_WAIT;

//Empty out the buffer before new call
#if 0
	for (i = 0; i < 200; i++)
	{

		rtp_session_recv_with_ts (gCall[zCall].rtp_session_in,
								  buffer,
								  gCall[zCall].codecBufferSize,
								  gCall[zCall].in_user_ts,
								  &streamAvailable, 0, NULL);

		gCall[zCall].in_user_ts += 160;
	}
#endif

	// only g711 decodeing and inband for now 
	// the flag we are using here is should be exclusive 
	// of using the tones client 

	if (gCall[zCall].telephonePayloadType == -99)	//DDN added telephonyPayloadType check 06/24/2011
	{
		initializeDtmfToneDetectionBuffers (zCall);
        freeDTMFStructs++;
	}

	while (1)
	{

		if (rc != ACTION_GET_NEXT_REQUEST)
		{
			usleep (100 * 1000);
		}

		//lock
		pthread_mutex_lock (&gCall[zCall].gMutexRequest);

		yInputAction.request = NULL;

		rc = getNextInputAction (zCall, &yInputAction, NULL, mod);

		if (yInputAction.request != NULL)
		{
			memcpy (&yTmpCopyMsgToDM, &(yInputAction.request->msgToDM),
					sizeof (struct MsgToDM));
		}
		//unlock
		pthread_mutex_unlock (&gCall[zCall].gMutexRequest);

		gCall[zCall].m_dwExpectGrabTime_in =
			gCall[zCall].m_dwPrevGrabTime_in = mCoreAPITime ();

		if (yInputAction.actionId == ACTION_EXIT)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "PendingInputRequests=%d; actionId=(ACTION_EXIT:%d), opcode=%d.",
					   gCall[zCall].pendingInputRequests, ACTION_EXIT, yTmpCopyMsgToDM.opcode);

			gCall[zCall].pendingInputRequests = 0;

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "ACTION_EXIT", "", zCall);

			break;
		}
		else if (yInputAction.actionId == ACTION_WAIT)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "PendingInputRequests=%d; actionId=(ACTION_WAIT:%d), opcode=%d.",
					   gCall[zCall].pendingInputRequests, ACTION_WAIT, yTmpCopyMsgToDM.opcode);

			gCall[zCall].pendingInputRequests = 0;
			rc = recordFile_X (__LINE__, zCall, NULL);
			if (rc < 0)
			{

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "recordFile_X returned", "", rc);
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "recordFile_X returned %d", rc);
				break;
			}
			continue;
		}
		else if (yInputAction.request == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "PendingInputRequests=%d; request == NULL.",
					   gCall[zCall].pendingInputRequests);

			gCall[zCall].pendingInputRequests = 0;
			rc = recordFile_X (__LINE__, zCall, NULL);
			if (rc < 0)
			{

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "recordFile_X returned", "",
							  rc);
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "recordFile_X returned %d", rc);

				break;
			}
			continue;
		}
		else if (yInputAction.actionId == ACTION_CONTINUE)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "PendingInputRequests=%d; actionId=(ACTION_CONTINUE:%d); "
					   "opcode=%d;  DMOP_SRRECOGNIZEFROMCLIENT=%d.",
					   gCall[zCall].pendingInputRequests,
					   ACTION_CONTINUE, yTmpCopyMsgToDM.opcode,
					   DMOP_SRRECOGNIZEFROMCLIENT);
			gCall[zCall].pendingInputRequests = 0;

			//rtp_session_reset(gCall[zCall].rtp_session_in);

			switch (yTmpCopyMsgToDM.opcode)
			{
			case DMOP_SRRECOGNIZEFROMCLIENT:		// MS_SR
			case DMOP_SRRECOGNIZE:					// GOOGLE_SR

				__DDNDEBUG__ (DEBUG_MODULE_SR, "START REOGNITION", "", 0);

				//if(!gCall[zCall].conferenceStarted)
				//{
				rtp_session_reset (gCall[zCall].rtp_session_in);
				//}

				gCall[zCall].in_user_ts = 0;

				rc = recordFile_X (__LINE__, zCall, &(yTmpCopyMsgToDM));

				__DDNDEBUG__ (DEBUG_MODULE_SR, "END REOGNITION", "", 0);

				if (rc == ACTION_GET_NEXT_REQUEST)
				{
					continue;
				}

				break;

			case DMOP_RECORD:
			case DMOP_RECORDMEDIA:
				if ( gCall[zCall].googleRecord == YES )
				{
					gCall[zCall].googleRecordIsActive = 1;
				}

				rtp_session_reset (gCall[zCall].rtp_session_in);
				//gCall[zCall].in_user_ts = 0;

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "recordFile_X for port", "",
							  yTmpCopyMsgToDM.appCallNum);

				rc = recordFile_X (__LINE__, zCall, &(yTmpCopyMsgToDM));

				if (rc == ACTION_GET_NEXT_REQUEST)
				{
					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Calling clearAppRequestList", "", -1);

					clearAppRequestList (zCall);
					continue;
				}

				if (rc != -2)
				{

					__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "recordFile_X returned",
								  "", rc);
					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Calling clearAppRequestList", "", -1);

					clearAppRequestList (zCall);

				}
				else
				{

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Calling clearAppRequestList", "", -1);

					clearAppRequestList (zCall);
					__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
								  "recordFile_X returned, which is a timeout",
								  "", rc);

					continue;
				}

				break;

			case DMOP_RECVFAX:

				rtp_session_reset (gCall[zCall].rtp_session_in);

				rc = recvFile_X (zCall, &(yTmpCopyMsgToDM));

				if (rc == ACTION_GET_NEXT_REQUEST)
				{
					continue;
				}

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "recvFile_X returned", "",
							  rc);

				break;

			case DMOP_STOPACTIVITY:
				rc = recordFile_X (__LINE__, zCall, NULL);
				break;

			default:
				rc = recordFile_X (__LINE__, zCall, NULL);
				break;

			}					/*END: switch */

		}						/*END: ACTION_CONTINUE */

		if (rc < 0)
		{
			break;
		}

	}							/*END: while(1) */

	/*Flush the buffer */

	for (i = 0; i < 200; i++)
	{

		rtp_session_recv_with_ts (gCall[zCall].rtp_session_in,
								  buffer,
								  gCall[zCall].codecBufferSize,
								  gCall[zCall].in_user_ts,
								  &streamAvailable, 0, NULL);

		gCall[zCall].in_user_ts += 160;
	}

	/*Cleanup inband tone detector struture */
	arcDtmfData[zCall].silence_time = -1;
	arcDtmfData[zCall].last = DSIL;
	arcDtmfData[zCall].x = DSIL;
	arcDtmfData[zCall].MFmode = 0;

	arcDtmfData[zCall].lookForLeadSilence = 0;
	arcDtmfData[zCall].lookForTrailSilence = 0;

	arcDtmfData[zCall].lead_silence_triggered = 0;
	arcDtmfData[zCall].trail_silence_triggered = 0;

	arcDtmfData[zCall].lead_silence_processed = 0;
	arcDtmfData[zCall].trail_silence_processed = 0;

	ftime (&arcDtmfData[zCall].lastTime);
	/*END; Cleanup inband tone detector struture */

	if (gCall[zCall].rtp_session_in != NULL)
	{

#if 0
		rtp_session_signal_disconnect_by_callback
			(gCall[zCall].rtp_session_in,
			 "telephone-event", (RtpCallback) recv_tev_cb);

		rtp_session_signal_disconnect_by_callback
			(gCall[zCall].rtp_session_in,
			 "telephone-event_packet", (RtpCallback) recv_tevp_cb);

		rtp_session_signal_disconnect_by_callback
			(gCall[zCall].rtp_session_in,
			 "payload_type_changed", (RtpCallback) recv_payload_change_cb);
#endif

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Destroying rtp_session_in", "Call",
					  zCall);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling arc_rtp_session_destroy(); rtp_session_in socket=%d.",
				   gCall[zCall].rtp_session_in->rtp.socket);

		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_in);
	}

	if (gCall[zCall].gSilentInRecordFileFp != NULL)
	{
		gCall[zCall].silentRecordFlag = 0;
		arc_frame_record_to_file (zCall, AUDIO_MIXED, (char *) __func__,
								  __LINE__);
		//writeWavHeaderToFile(zCall, gCall[zCall].gSilentInRecordFileFp);
		//fclose(gCall[zCall].gSilentInRecordFileFp);
		//gCall[zCall].gSilentInRecordFileFp = NULL;
	}

	gTotalInputThreads--;

	if (gCall[zCall].isInbandToneDetectionThreadStarted == 1)
	{
		stopInbandToneDetectionThread (__LINE__, zCall);
	}

    // this seems to be the source of the core 
    // maybe a race for freeing this resource for inband dtmf 
    // JS 9/21/2015 
    // I Addded a flag local to this thread to prevent clearPort from setting this before 
    // the thread exits 
	// DDN 04/02/2016 -99 for no rfc 2833 as opposed to older -1
    
	// DDN 06/26/2011
	if (gCall[zCall].telephonePayloadType == -99 && freeDTMFStructs)	//DDN added telephonyPayloadType check 06/24/2011
	{
		arc_tones_ringbuff_free (&gCall[zCall].dtmfDetectionBuff);
	}

	arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_IN]);
	// arc_audio_frame_reset(&gCall[zCall].audio_frames[AUDIO_MIXED]);

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "EXIT inputThread", "Total threads",
				  gTotalInputThreads + gTotalOutputThreads);

	if (gCall[zCall].outputThreadId > 0)
	{
		gCall[zCall].inputThreadId = 0;
		gCall[zCall].cleanerThread = gCall[zCall].outputThreadId;
	}
	else if (gCall[zCall].cleanerThread == gCall[zCall].inputThreadId)
	{
		gCall[zCall].clear = 1;
		if (gCall[zCall].leg == LEG_B || gCall[zCall].thirdParty == 1)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 0",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 0);
		}
		else
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 1",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 1);
		}
		gCall[zCall].inputThreadId = 0;
		gCall[zCall].cleanerThread = 0;
	}

	gCall[zCall].inputThreadId = 0;

	pthread_detach (pthread_self ());
	pthread_exit (NULL);

	return (0);

}/*END: void * inputThread */

int
printSpeakList (SpeakList * zpList)
{
int             count = 0;
char            mod[] = "printSpeakList";

	while (zpList != NULL)
	{
		count++;

		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "count = %d, zpList->msgSpeak.file=%s, zpList->msgSpeak.appRef=%d, "
				   "opcode=%d interruptOption=%d",
				   count, zpList->msgSpeak.file, zpList->msgSpeak.appRef,
				   zpList->msgSpeak.opcode, zpList->msgSpeak.interruptOption);
		zpList = zpList->nextp;
	}
}

///This function is a thread that is fired when the call starts for output to the caller.
/**
The outputThread is fired whenever a call is successfully established by Call
Manager.  Its purpose is to send packets to the caller as well as any bridged
callers. 
*/
void           *
outputThread (void *z)
{
	char            mod[] = { "outputThread" };
	int             rc;
	int             zCall = -1;
	int             yTermReason = TERM_REASON_DEV;
	FILE           *fp;
	char            file[256];
	char            message[256];
	struct stat		speakStat;

	struct OutputAction yOutputAction;

	struct MsgToDM *ptrMsgToDM;
	struct Msg_Speak msg;

	int             packetCounter = 0;

	int				restart_output_thread = 0;

	ptrMsgToDM = (struct MsgToDM *) z;

	zCall = ptrMsgToDM->appCallNum;

	gCall[zCall].m_dwExpectGrabTime_out = gCall[zCall].m_dwPrevGrabTime_out =
		mCoreAPITime ();

	if (gCall[zCall].rtp_session == NULL)
	{
		gCall[zCall].outputThreadId = 0;

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
	}

	gTotalOutputThreads++;

	while (1)
	{


		gCall[zCall].sendingSilencePackets = 1;

		usleep (10 * 1000);

		memset (&yOutputAction, 0, sizeof (OutputAction));

		rc = getNextOutputAction (zCall, &yOutputAction, NULL, mod);

		gCall[zCall].m_dwExpectGrabTime_out =
			gCall[zCall].m_dwPrevGrabTime_out = mCoreAPITime ();

		if (yOutputAction.actionId == ACTION_EXIT)
		{

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "PendingOutputRequests=%d; actionId=(ACTION_EXIT:%d).",
					   gCall[zCall].pendingInputRequests, ACTION_EXIT);

			gCall[zCall].pendingOutputRequests = 0;

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "ACTION_EXIT", "", zCall);

			gCall[zCall].sendingSilencePackets = 1;
			break;
		}
		else if (yOutputAction.actionId == ACTION_RESTART_OUTPUT_SESSION)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "PendingOutputRequests=%d; actionId=(ACTION_RESTART_OUTPUT_SESSION:%d).",
					   gCall[zCall].pendingInputRequests, ACTION_RESTART_OUTPUT_SESSION);

			//gCall[zCall].pendingOutputRequests = 0;

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "ACTION_RESTART_OUTPUT_SESSION", "", zCall);

#if 0
			gCall[zCall].sendingSilencePackets = 1;

			restart_output_thread = 1;

			break;
#endif 

			restartOutputSession(zCall);
			continue;

		}
		else if (yOutputAction.actionId == ACTION_WAIT)
		{

			gCall[zCall].sendingSilencePackets = 1;

			rc = speakFile_X (__LINE__, zCall, -1, NULL, &yTermReason);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Returned from speakFile_X(). rc=%d ACTION_GET_NEXT_REQUEST=%d",
					rc, ACTION_GET_NEXT_REQUEST);

			if (rc == ACTION_GET_NEXT_REQUEST)
			{
				continue;
			}

			break;
		}
		else if (yOutputAction.request == NULL)
		{
			gCall[zCall].sendingSilencePackets = 1;
			gCall[zCall].pendingOutputRequests = 0;

			rc = speakFile_X (__LINE__, zCall, -1, NULL, &yTermReason);
			if (rc == ACTION_GET_NEXT_REQUEST)
			{
				continue;
			}
			break;
		}
		else if (yOutputAction.actionId == ACTION_CONTINUE)
		{

			gCall[zCall].sendingSilencePackets = 0;

			gCall[zCall].pendingOutputRequests = 0;

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Pending requests", "total",
						  gCall[zCall].pendingOutputRequests);

			switch (yOutputAction.request->msgToDM.opcode)
			{
			case DMOP_OUTPUTDTMF:
				{
	struct Msg_Speak yTmpMsgSpeak;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "OUTPUT DTMF START");

					memcpy (&yTmpMsgSpeak,
							(struct Msg_Speak *) &(yOutputAction.request->
												   msgToDM),
							sizeof (struct Msg_Speak));

					rc = speakFile_X (__LINE__, zCall,
									  -1,
									  &(yOutputAction.request->msgToDM),
									  &yTermReason);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Returned from speakFile_X().");
					break;
				}

			case DMOP_SPEAK:
			case DMOP_PLAYMEDIA:
			case DMOP_PLAYMEDIAVIDEO:
			case DMOP_PLAYMEDIAAUDIO:
			case DMOP_SPEAKMRCPTTS:
				{
	size_t          sizeOfSpeak;
	struct Msg_SpeakMrcpTts yTmpMsgSpeak;
	SpeakList      *pSpeakList = NULL;

					if (yOutputAction.request->msgToDM.opcode ==
						DMOP_SPEAKMRCPTTS)
					{
						sizeOfSpeak = sizeof (struct Msg_SpeakMrcpTts);
					}
					else
					{
						sizeOfSpeak = sizeof (struct Msg_Speak);
					}

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "PLAY START");

					if ( ( gCall[zCall].googleStreamingSR == 1 ) ||
					     ( gCall[zCall].googleRecordIsActive == 1 ) )
					{
						gCall[zCall].googlePromptIsPlaying = 1;
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
							"googlePromptIsPlaying set to %d", gCall[zCall].googlePromptIsPlaying);
					}

					memcpy (&yTmpMsgSpeak,
							(struct Msg_Speak *) &(yOutputAction.request->
												   msgToDM), sizeOfSpeak);


					__DDNDEBUG__ (DEBUG_MODULE_MEMORY,
								  "Calling copySpeakList", "", -1);

					pSpeakList = (SpeakList *) copySpeakList (zCall);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		   "pFirstSpeak - just copied to pSpeakList=0x%u",  pSpeakList);

	SpeakList      *lpSpeakListToDelete = pSpeakList;

					if (yOutputAction.request->msgToDM.opcode ==
						DMOP_PLAYMEDIA
//						|| yOutputAction.request->msgToDM.opcode ==
//						DMOP_PLAYMEDIAVIDEO
						|| yOutputAction.request->msgToDM.opcode ==
						DMOP_PLAYMEDIAAUDIO)
					{
						if (pSpeakList != NULL)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "opcode=%d, interruptOption=%d.",
									   yOutputAction.request->msgToDM.opcode,
									   pSpeakList->msgSpeak.interruptOption);
							memcpy (&yTmpMsgSpeak, &pSpeakList->msgSpeak,
									sizeOfSpeak);
						}
						else
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "pSpeakList == NULL; nothing to play. Writing response back to app.");

	struct MsgToApp response;

							response.opcode =
								yOutputAction.request->msgToDM.opcode;
							response.appRef =
								yOutputAction.request->msgToDM.appRef;
							response.appCallNum = zCall;
							response.appPassword = gCall[zCall].appPassword;
							response.appPid = gCall[zCall].appPid;
							response.returnCode = 0;

							if (gCall[zCall].responseFifoFd >= 0)
							{
	int             yRc =
		writeGenericResponseToApp (zCall, &response, mod, __LINE__);
							}
							break;
						}
					}
					else
					{
						memcpy (&yTmpMsgSpeak,
								(struct Msg_Speak *) &(yOutputAction.request->
													   msgToDM), sizeOfSpeak);
					}

					if (yTmpMsgSpeak.synchronous == SYNC ||
						yTmpMsgSpeak.synchronous == ASYNC)
					{
						if (yTmpMsgSpeak.opcode != DMOP_SPEAK &&
							pSpeakList != NULL)
						{
							do
							{
								gCall[zCall].currentAudio.appRef =
									yOutputAction.request->msgToDM.appRef;
								gCall[zCall].currentAudio.isReadyToPlay = YES;

								if ( gCall[zCall].GV_LastPlayPostion == 1 )
								{
									if ((stat(yTmpMsgSpeak.file, &speakStat)) == -1)
									{
										dynVarLog (__LINE__, __LINE__, mod, REPORT_VERBOSE, TEL_BASE, INFO,
												"stat(%s) failed. [%d, %s]",
												yTmpMsgSpeak.file, errno, strerror(errno));
										gCall[zCall].currentSpeakSize = -1;
									}
									else
									{
										gCall[zCall].currentSpeakSize = speakStat.st_size;
									}
								}
	
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, TEL_BASE, INFO,
										   "Calling speakFile_X(); yTmpMsgSpeak.file=%s, size=%d, interrupt=%d.",
										   yTmpMsgSpeak.file,
										   gCall[zCall].currentSpeakSize,
										   yTmpMsgSpeak.interruptOption);

								if (gCall[zCall].dtmfAvailable == 0)
								{
									rc = speakFile_X (__LINE__, zCall,
													  yOutputAction.request->
													  msgToDM.appRef,
													  (MsgToDM *) &
													  yTmpMsgSpeak,
													  &yTermReason);

									dynVarLog (__LINE__, zCall, mod,
											   REPORT_VERBOSE, TEL_BASE, INFO,
											   "Returned from speakFile_X().");
								}
								else if (gCall[zCall].rtp_session_mrcp ==
										 NULL)
								{
	struct MsgToApp response;

									yTermReason = TERM_REASON_DTMF;

									dynVarLog (__LINE__, zCall, mod,
											   REPORT_VERBOSE, TEL_BASE, INFO,
											   "Setting dtmfAvailable to 0.");
									gCall[zCall].dtmfAvailable = 0;

									sprintf (response.message, "%c",
											 dtmf_tab[gCall[zCall].lastDtmf]);

									__DDNDEBUG__ (DEBUG_MODULE_FILE,
												  "Sending DTMF to app", "",
												  -1);

									response.opcode = DMOP_GETDTMF;
									response.appRef =
										gCall[zCall].msgToDM.appRef;
									response.appCallNum = zCall;
									response.appPassword =
										gCall[zCall].appPassword;
									response.appPid = gCall[zCall].appPid;
									response.returnCode = 0;

									if (gCall[zCall].responseFifoFd >= 0)
									{
	int             yRc =
		writeGenericResponseToApp (zCall, &response, mod, __LINE__);
									}
								}

								gCall[zCall].currentAudio.appRef = -1;
								gCall[zCall].currentAudio.isReadyToPlay = NO;

								if (yTermReason == TERM_REASON_DTMF)
								{
									__DDNDEBUG__ (DEBUG_MODULE_CALL,
												  "Calling clearSpeakList",
												  "", -1);
									clearSpeakList (zCall, __LINE__);

									__DDNDEBUG__ (DEBUG_MODULE_CALL,
												  "Calling clearAppRequestList",
												  "", -1);

									clearAppRequestList (zCall);
									break;
								}
								if (!canContinue (mod, zCall, __LINE__))
								{
									break;
								}

								while (1)
								{
									if (gCall[zCall].pFirstSpeak == NULL)
									{

										if (gCall[zCall].dtmfAvailable == 1 &&
											gCall[zCall].rtp_session_mrcp ==
											NULL)
										{
											dynVarLog (__LINE__, zCall, mod,
													   REPORT_VERBOSE,
													   TEL_BASE, INFO,
													   "Got DTMF; done waiting.");

											dynVarLog (__LINE__, zCall, mod,
													   REPORT_VERBOSE,
													   TEL_BASE, INFO,
													   "Setting dtmfAvailable to 0.");
											gCall[zCall].dtmfAvailable = 0;

	struct MsgToApp response;

											response.message[0] = '\0';

											__DDNDEBUG__ (DEBUG_MODULE_FILE,
														  "Sending DTMF to app",
														  "", -1);

											response.opcode = DMOP_GETDTMF;
											response.appRef =
												gCall[zCall].msgToDM.appRef;
											response.appCallNum = zCall;
											response.appPassword =
												gCall[zCall].appPassword;
											response.appPid =
												gCall[zCall].appPid;
											response.returnCode = 0;

											if (gCall[zCall].responseFifoFd >=
												0)
											{
												if (gCall[zCall].
													dtmfCacheCount > 0)
												{
	int             i = 0;

													for (i = 0;
														 i <
														 gCall[zCall].
														 dtmfCacheCount; i++)
													{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

														sprintf (response.
																 message,
																 "%c",
																 dtmf_tab
																 [dtmfToSend]);

														writeGenericResponseToApp
															(zCall, &response,
															 mod, __LINE__);
													}
													gCall[zCall].
														dtmfCacheCount = 0;
												}

												sprintf (response.message,
														 "%c",
														 dtmf_tab[gCall
																  [zCall].
																  lastDtmf]);

												writeGenericResponseToApp
													(zCall, &response, mod,
													 __LINE__);
												//int yRc = writeGenericResponseToApp(zCall, &response, mod, __LINE__);
											}
										}

										dynVarLog (__LINE__, zCall, mod,
												   REPORT_VERBOSE, TEL_BASE,
												   INFO, "Done waiting.");
										break;
									}

									if (gCall[zCall].dtmfAvailable == 1 &&
										gCall[zCall].rtp_session_mrcp == NULL)
									{
										dynVarLog (__LINE__, zCall, mod,
												   REPORT_VERBOSE, TEL_BASE,
												   INFO,
												   "Got DTMF. Done waiting.");

										dynVarLog (__LINE__, zCall, mod,
												   REPORT_VERBOSE, TEL_BASE,
												   INFO,
												   "Setting dtmfAvailable to 0.");
										gCall[zCall].dtmfAvailable = 0;

	struct MsgToApp response;

										response.message[0] = '\0';

										__DDNDEBUG__ (DEBUG_MODULE_FILE,
													  "Sending DTMF to app",
													  "", -1);

										response.opcode = DMOP_GETDTMF;
										response.appRef =
											gCall[zCall].msgToDM.appRef;
										response.appCallNum = zCall;
										response.appPassword =
											gCall[zCall].appPassword;
										response.appPid = gCall[zCall].appPid;
										response.returnCode = 0;

										if (gCall[zCall].responseFifoFd >= 0)
										{
											if (gCall[zCall].dtmfCacheCount >
												0)
											{
	int             i = 0;

												for (i = 0;
													 i <
													 gCall[zCall].
													 dtmfCacheCount; i++)
												{
	char            dtmfToSend = gCall[zCall].dtmfInCache[i];

													sprintf (response.message,
															 "%c",
															 dtmf_tab
															 [dtmfToSend]);

													writeGenericResponseToApp
														(zCall, &response,
														 mod, __LINE__);
												}
												gCall[zCall].dtmfCacheCount =
													0;
											}

											sprintf (response.message, "%c",
													 dtmf_tab[gCall[zCall].
															  lastDtmf]);

											writeGenericResponseToApp (zCall,
																	   &response,
																	   mod,
																	   __LINE__);
											//int yRc = writeGenericResponseToApp(zCall, &response, mod, __LINE__);
										}

										break;
									}

									{
										dynVarLog (__LINE__, zCall, mod,
												   REPORT_VERBOSE, TEL_BASE,
												   INFO,
												   "Done waiting; calling clearSpeakList().");

										__DDNDEBUG__ (DEBUG_MODULE_CALL,
													  "Calling clearSpeakList",
													  "", -1);
										clearSpeakList (zCall, __LINE__);

										__DDNDEBUG__ (DEBUG_MODULE_CALL,
													  "Calling clearAppRequestList",
													  "", -1);

										clearAppRequestList (zCall);

										break;
									}

									if (!canContinue (mod, zCall, __LINE__))
									{
										dynVarLog (__LINE__, zCall, mod,
												   REPORT_VERBOSE, TEL_BASE,
												   INFO, "Done waiting.");
										break;
									}
#if 1
									dynVarLog (__LINE__, zCall, mod,
											   REPORT_VERBOSE, TEL_BASE, INFO,
											   "Waiting for corresponding video to end...");
#endif
									usleep (50 * 1000);
								}

							}
							while (pSpeakList->msgPlayMedia.audioLooping ==
								   YES);
						}
						else
						{
							gCall[zCall].currentAudio.appRef =
								yOutputAction.request->msgToDM.appRef;
							gCall[zCall].currentAudio.isReadyToPlay = YES;

							rc = speakFile_X (__LINE__, zCall,
											  yOutputAction.request->msgToDM.
											  appRef,
											  (MsgToDM *) & yTmpMsgSpeak,
											  &yTermReason);

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Returned from speakFile_X() rc=%d.",
									   rc);

							gCall[zCall].currentAudio.appRef = -1;
							gCall[zCall].currentAudio.isReadyToPlay = NO;

							if (yTermReason == TERM_REASON_DTMF)
							{
								__DDNDEBUG__ (DEBUG_MODULE_CALL,
											  "Calling clearSpeakList", "",
											  -1);
								clearSpeakList (zCall, __LINE__);

								__DDNDEBUG__ (DEBUG_MODULE_CALL,
											  "Calling clearAppRequestList",
											  "", -1);

								clearAppRequestList (zCall);
							}

						}

						if (rc == ACTION_GET_NEXT_REQUEST)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO, "PLAY END.");

							while (lpSpeakListToDelete != NULL)
							{
	SpeakList      *p1 = lpSpeakListToDelete->nextp;

								arc_free (zCall, mod, __LINE__,
										  lpSpeakListToDelete,
										  sizeof (SpeakList));
								free (lpSpeakListToDelete);

								lpSpeakListToDelete = p1;
							}

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Setting gCall[zCall].isBeepActive to 0.");

							gCall[zCall].isBeepActive = 0;
							continue;
						}
					}
					else
					{
	int             yEmptyFileNameCounter = 0;
	int             ySavedAppRef = 0;
	int             yCurrentAppRef = 0;
	int             yStreamCounter = 0;
	char            yStrLastStreamFile[512];

						//SpeakList *pSpeakList = gCall[zCall].pFirstSpeak;
						__DDNDEBUG__ (DEBUG_MODULE_MEMORY,
									  "Calling copySpeakList", "", -1);
	SpeakList      *pSpeakList = (SpeakList *) copySpeakList (zCall);
	SpeakList      *pSpeakListToDelete = pSpeakList;

						//printSpeakList(pSpeakList);

						__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
									  "PLAY_QUEUE_SYNC | ASYNC", "", zCall);

						if (pSpeakList == NULL)
						{
							;
							__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
										  "PLAY_QUEUE_SYNC | ASYNC",
										  "SpeakList NULL", zCall);
						}

						while (pSpeakList != NULL)
						{
							if (!canContinue (mod, zCall, __LINE__))
							{
								break;
							}
							yCurrentAppRef = pSpeakList->msgSpeak.appRef;

							if (ySavedAppRef == 0)
							{
								ySavedAppRef = pSpeakList->msgSpeak.appRef;
							}

							//pSpeakList->msgSpeak.appRef = yTmpMsgSpeak.appRef;

							if (pSpeakList->msgSpeak.file[0])
							{
								if (strcmp (pSpeakList->msgSpeak.file, "NULL")
									== 0)
								{
									yEmptyFileNameCounter++;
								}
								else
								{
									yEmptyFileNameCounter = 0;
								}
							}
							else
							{
								yEmptyFileNameCounter++;
							}

							if (pSpeakList->msgSpeak.interruptOption !=
								FIRST_PARTY_PLAYBACK_CONTROL
								&& pSpeakList->msgSpeak.interruptOption !=
								PLAYBACK_CONTROL
								&& pSpeakList->msgSpeak.interruptOption !=
								SECOND_PARTY_PLAYBACK_CONTROL)
							{
								//pSpeakList->msgSpeak.interruptOption = yTmpMsgSpeak.interruptOption;
							}

							yTermReason = TERM_REASON_DEV;

							//gCall[zCall].currentAudio.appRef = pSpeakList->msgSpeak.appRef;
							gCall[zCall].currentAudio.appRef = yCurrentAppRef;
							gCall[zCall].currentAudio.isReadyToPlay = YES;

							gCall[zCall].syncAV = -1;
							gCall[zCall].appRefToBePlayed = -1;

							if ( gCall[zCall].GV_LastPlayPostion == 1 )
							{
								if ((stat(pSpeakList->msgSpeak.file, &speakStat)) == -1)
								{
									dynVarLog (__LINE__, __LINE__, mod, REPORT_VERBOSE, TEL_BASE, INFO,
												"stat(%s) failed. [%d, %s]\n", pSpeakList->msgSpeak.file, errno, strerror(errno));
									gCall[zCall].currentSpeakSize = -1;
								}
								else
								{
									gCall[zCall].currentSpeakSize = speakStat.st_size;
								}
							}
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Calling speakFile_X(); appRef=%d, audioFileName=%s, size=%d, "
									   "opcode=%d, interruptOption=%d.",
									   pSpeakList->msgSpeak.appRef,
									   pSpeakList->msgSpeak.file,
									   gCall[zCall].currentSpeakSize,
									   pSpeakList->msgSpeak.opcode,
									   pSpeakList->msgSpeak.interruptOption);

							if (pSpeakList->isMTtsPresent)
							{
								rc = speakFile_X (__LINE__, zCall,
												  yCurrentAppRef,
												  (struct MsgToDM *)
												  &(pSpeakList->msgSpeakTts),
												  &yTermReason);
							}
							else
							{
								rc = speakFile_X (__LINE__, zCall,
												  yCurrentAppRef,
												  (struct MsgToDM *)
												  &(pSpeakList->msgSpeak),
												  &yTermReason);
							}
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Returned from speakFile_X().");

							if (gCall[zCall].waitForPlayEnd == 1)
							{
								gCall[zCall].waitForPlayEnd = 0;
								if (gCall[zCall].responseFifoFd >= 0)
								{
									writeGenericResponseToApp (zCall,
															   &(gCall[zCall].
																 msgPortOperation),
															   mod, __LINE__);
								}
							}

							gCall[zCall].currentAudio.appRef = -1;
							gCall[zCall].currentAudio.isReadyToPlay = NO;
							__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
										  "Got TERM REASON", "", yTermReason);

							if (yTermReason == TERM_REASON_DTMF)
							{
								__DDNDEBUG__ (DEBUG_MODULE_CALL,
											  "Calling clearSpeakList", "",
											  -1);
								clearSpeakList (zCall, __LINE__);
								break;
							}

							if ((!pSpeakList->msgSpeak.file[0] ||
								 strcmp (pSpeakList->msgSpeak.file,
										 "NULL") == 0)
								&& pSpeakList->msgSpeak.synchronous ==
								PUT_QUEUE_ASYNC)
							{
								if (!canContinue (mod, zCall, __LINE__))
								{
									break;
								}

								if (yEmptyFileNameCounter > 1000)
								{
									yEmptyFileNameCounter = 0;
									//if(pSpeakList->msgPlayMedia.audioLooping == YES)
									//{
									//  addLoopFile(pSpeakList);
									//}
									pSpeakList = pSpeakList->nextp;
									continue;
								}

								pSpeakList->msgSpeak.appRef = ySavedAppRef;

								getSpeakFileName (zCall, pSpeakList);

								__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
											  "Got file name",
											  pSpeakList->msgSpeak.file, 0);

								/*Work around for MR 1583 */
								if (yStreamCounter > 0
									&& strcmp (yStrLastStreamFile,
											   pSpeakList->msgSpeak.file) ==
									0)
								{
									dynVarLog (__LINE__, zCall, mod,
											   REPORT_VERBOSE, TEL_BASE, INFO,
											   "Skipping; second identical streaming in the same queue.");

									pSpeakList = pSpeakList->nextp;

									if (pSpeakList != NULL)
									{
										ySavedAppRef =
											pSpeakList->msgSpeak.appRef;
									}

									continue;

								}	/*END: if */

								usleep (20 * 1000);

								continue;
							}

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "pSpeakList->msgPlayMedia.audioLooping=%d, pSpeakList->audioLooping=%d.",
									   pSpeakList->msgPlayMedia.audioLooping,
									   pSpeakList->audioLooping);

							if (pSpeakList->msgPlayMedia.audioLooping == YES)
							{
								addLoopFile (mod, pSpeakList);
							}

							pSpeakList = pSpeakList->nextp;

							if (pSpeakList != NULL)
							{
								ySavedAppRef = pSpeakList->msgSpeak.appRef;
							}
						}

						while (pSpeakListToDelete != NULL)
						{
	SpeakList      *p1 = pSpeakListToDelete->nextp;

							arc_free (zCall, mod, __LINE__,
									  pSpeakListToDelete, sizeof (SpeakList));
							free (pSpeakListToDelete);

							pSpeakListToDelete = p1;
						}
					}
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Setting isBeepActive to 0.");
					gCall[zCall].isBeepActive = 0;
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "PLAY END");
					dynVarLog (__LINE__, zCall, mod, REPORT_EVENT, TEL_BASE, INFO, // AT-2	
								"%s:PROMPT END", gCall[zCall].callCDR);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						"googleStreamingSR=%d  googleRecordIsActive=%d googleBarginDidOccur=%d",
						gCall[zCall].googleStreamingSR, gCall[zCall].googleRecordIsActive,
						gCall[zCall].googleBarginDidOccur);

					if ( ( gCall[zCall].googleStreamingSR == 1 ) ||
					     ( gCall[zCall].googleRecordIsActive == 1 ) )
					{
						int leadS;
						gCall[zCall].googlePromptIsPlaying = 0;

						//
						// for some reason, the detection has 3 seconds ahead
						//
						leadS = (gCall[zCall].msgRecord.lead_silence - 3) * 4000;
//						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//							"DJB: leads=%d msgRecord.lead_silence=%d.",
//							leadS, gCall[zCall].msgRecord.lead_silence);


						arc_tones_increment_leading_samples(&gCall[zCall].toneDetectionBuff, leadS);
//						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//							"DJB: googlePromptIsPlaying set to %d. leading_samples updated to  %d. ",
//							gCall[zCall].googlePromptIsPlaying, 
//							gCall[zCall].toneDetectionBuff.leading_samples);
							//gCall[zCall].toneDetectionBuff.leading_samples, leadS * 4000);
					}

					if ( ( gCall[zCall].googleStreamingSR == 1 ) &&
				   	     ( gCall[zCall].googleBarginDidOccur == 0 ) )
					{
						if (gCall[zCall].isInbandToneDetectionThreadStarted == 1)
						{
							stopInbandToneDetectionThread (__LINE__, zCall);
				
							int tdCounter = 30;				// BT-173
				
							while ( ( tdCounter-- != 0 ) &&
							        ( gCall[zCall].detectionTID != -1 ) )
							{
								usleep (1000 * 100);
							}
						}
						if ( gCall[zCall].detectionTID != -1 )
						{
							gCall[zCall].googleSRResponse.returnCode = -1;
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						   		"GSR: Sending 'bye' to client.");
							sendto(gCall[zCall].googleUDPFd,
									"bye", 
									3, 
									0,
									(struct sockaddr *) &gCall[zCall].google_si, 
									gCall[zCall].google_slen);	
				
							close(gCall[zCall].googleUDPFd);
							gCall[zCall].googleUDPPort = -1;
							gCall[zCall].googleUDPFd = -1;
							gCall[zCall].speechRec = 0;
						}
						else
						{
							gCall[zCall].googleSRResponse.returnCode = 0;
							startInbandToneDetectionThread (zCall, __LINE__, ARC_TONES_BEGINNING_AUDIO_TIMEOUT);
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
									"Restarted silence detection for GSR.");
						}

						memset((char *)gCall[zCall].googleSRResponse.message, '\0',
								sizeof(gCall[zCall].googleSRResponse.message));
						(void)writeGenericResponseToApp (zCall, &gCall[zCall].googleSRResponse, mod, __LINE__);
					}

					while (lpSpeakListToDelete != NULL)
					{
	SpeakList      *p1 = lpSpeakListToDelete->nextp;

						arc_free (zCall, mod, __LINE__, lpSpeakListToDelete,
								  sizeof (SpeakList));
						free (lpSpeakListToDelete);

						lpSpeakListToDelete = p1;
					}

					break;

				}				/*END: case: DMOP_SPEAK */

			case DMOP_FAX_PLAYTONE:
				if (gSendFaxTone == 0)
				{
	struct MsgToApp msg;

					memcpy (&msg, &gCall[zCall].msgToDM, sizeof (msg));
					msg.opcode = DMOP_FAX_PLAYTONE;
					writeGenericResponseToApp (zCall, &msg, mod, __LINE__);
				}
				break;

			case DMOP_FAX_STOPTONE:
				if (gCall[zCall].sentFaxPlaytone == 0)
				{
	struct Msg_Fax_PlayTone faxPlayTone;

					memset (&faxPlayTone, 0, sizeof (faxPlayTone));
					memcpy (&faxPlayTone, &(gCall[zCall].msgToDM),
							sizeof (faxPlayTone));

					faxPlayTone.opcode = DMOP_FAX_PLAYTONE;
					writeGenericResponseToApp (zCall,
											   (struct MsgToApp *)
											   &faxPlayTone, mod, __LINE__);
					gCall[zCall].sentFaxPlaytone = 1;
				}
				break;

			case DMOP_SENDFAX:
				{
#if 0
	struct Msg_SendFax yTmpMsgFax;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "FAX START");

					memcpy (&yTmpMsgFax,
							(struct Msg_SendFax *) &(yOutputAction.request->
													 msgToDM),
							sizeof (struct Msg_SendFax));

	int             yEmptyFileNameCounter = 0;
	int             ySavedAppRef = 0;
	int             yCurrentAppRef = 0;

					yCurrentAppRef = yTmpMsgFax.appRef;

					yTermReason = TERM_REASON_DEV;

					rc = sendFile_X (zCall, yCurrentAppRef,
									 (struct MsgToDM *) &(yTmpMsgFax),
									 &yTermReason);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "FAX  END");
#endif

					break;

				}				/*END: case: DMOP_SENDFAX */

			case DMOP_STOPACTIVITY:
				{
					break;
				}

			default:
				{
					__DDNDEBUG__ (DEBUG_MODULE_RTP,
								  "action id = ACTION_CONTINUE none of the cases are matched",
								  "", -1);
				}

			}					/*END: switch */

		}						/*END: if ACTION_CONTINUE */

	}							/*END: while(1) */

	if (gCall[zCall].rtp_session != NULL)
	{

		__DDNDEBUG__ (DEBUG_MODULE_RTP, "Destroying rtp_session_out", "Call",
					  zCall);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling arc_rtp_session_destroy(); rtp_session socket=%d.",
				   gCall[zCall].rtp_session->rtp.socket);
		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session);
	}

	if (gCall[zCall].inputThreadId > 0)
	{
		usleep (100 * 1000);
	}

	if (gCall[zCall].inputThreadId > 0)
	{
		gCall[zCall].outputThreadId = 0;
		gCall[zCall].cleanerThread = gCall[zCall].inputThreadId;
	}
	else if (gCall[zCall].cleanerThread == gCall[zCall].outputThreadId)
	{
		gCall[zCall].clear = 1;
		if (gCall[zCall].leg == LEG_B || gCall[zCall].thirdParty == 1)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 0",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 0);
		}
		else
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 1",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 1);
		}
		gCall[zCall].outputThreadId = 0;
		gCall[zCall].cleanerThread = 0;
	}

	gCall[zCall].sendingSilencePackets = 1;
	gCall[zCall].outputThreadId = 0;

	gTotalOutputThreads--;

	if (gCall[zCall].gSilentOutRecordFileFp != NULL)
	{
		writeWavHeaderToFile (zCall, gCall[zCall].gSilentOutRecordFileFp);
		fclose (gCall[zCall].gSilentOutRecordFileFp);
		gCall[zCall].gSilentOutRecordFileFp = NULL;
	}

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "EXIT outputThread", "Total threads",
				  gTotalInputThreads + gTotalOutputThreads);

	arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_OUT]);

	if (gCall[zCall].conferenceIdx != -1)
	{
		arc_conference_delref_by_name (Conferences, 48, zCall,
									   gCall[zCall].confID);
	}

	pthread_detach (pthread_self ());

	pthread_exit (NULL);

	return (0);

}/*END: void * outputThread */

void
sigterm (int x)
{
	char            mod[] = { "sigterm" };
	int             zCall = -1;

	gCanExit = 1;
	gCanReadShmData = 0;

// __DDNDEBUG__(DEBUG_MODULE_ALWAYS, "", "SIGNAL", x);
	dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
			   "Received signal %d.", x);

	if (access (".mrcpTTSRtpDetails", F_OK) == 0)
	{
		unlink (".mrcpTTSRtpDetails");
	}

	return;

}								/*END: voide sigterm */

void
sigpipe (int x)
{
	char            mod[] = { "sigpipe" };
	int             zCall = -1;

	//gCanExit = 1;
	//gCanReadShmData = 0;

// __DDNDEBUG__(DEBUG_MODULE_ALWAYS, "", "SIGNAL", x);
	dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
			   "Received signal %d.", x);

	return;
}								/*END: int sigpipe */

void
sigsegv (int x)
{
	char            mod[] = { "sigsegv" };
	int             zCall = -1;

// __DDNDEBUG__(DEBUG_MODULE_ALWAYS, "", "SIGNAL", x);

	dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "Received signal %d.", x);
	gCanExit = 1;
	gCanReadShmData = 0;

	return;

}								/*END: void sigsegv */

int
cleanPlayQueueList (int zCall)
{
	char            mod[] = { "cleanPlayQueueList" };

	SpeakList      *pSpeak;

	if (zCall < gStartPort || zCall > gEndPort)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT, ERR,
				   "Invalid port number received (%d). Unable to clear play list.",
				   zCall);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "CLEANING PLAY LIST", 0);

	while (gCall[zCall].pPlayQueueList != NULL)
	{
		pSpeak = gCall[zCall].pPlayQueueList->nextp;
		arc_free (zCall, mod, __LINE__, gCall[zCall].pPlayQueueList,
				  sizeof (SpeakList));
		free (gCall[zCall].pPlayQueueList);
		gCall[zCall].pPlayQueueList = pSpeak;
	}

	gCall[zCall].pPlayQueueList = NULL;
	return (0);

}								/*END: int cleanPlayQueueList */


/*

 This is only called at exit for each port
 it's here to free up any unfreed memory to remove 
 false positives for memory leaks, or highlight real ones 
 Joe S. 

*/


int 
clearPortAtExit(int zCall, const char *callingFunc, int opts)
{

    int rc =0;

     return 0;

    if(zCall < gStartPort || zCall > gEndPort){
      return -1;
    }

    // free audio resources that are perisitent across calling 
    
    arc_audio_frame_free (&gCall[zCall].audio_frames[AUDIO_IN]);
    arc_audio_frame_free (&gCall[zCall].audio_frames[AUDIO_OUT]);
    arc_audio_frame_free (&gCall[zCall].audio_frames[AUDIO_MIXED]);
    arc_audio_frame_free (&gCall[zCall].audio_frames[CONFERENCE_AUDIO]);


    return rc;
}

/* end clearPortAtExit */

void 
clearPortLock(int zCall){

  if(zCall < gStartPort || zCall > gEndPort){
      return;
  }

  pthread_mutex_lock(&ClearPortLock);
}


void 
clearPortUnlock(int zCall){

 if(zCall < gStartPort || zCall > gEndPort){
      return;
 }
 pthread_mutex_unlock(&ClearPortLock);
}


///This function sets the call specific variables back to default and closes the fifos this call was using.
int
clearPort (int zCall, char *mod, int canClearStreamData,
		   int canDeleteResponseFifo)
{
	int             j = 0;

	SpeakList      *pSpeak;

   clearPortLock(zCall);

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "clearPort", "", zCall);

	if (gCall[zCall].responseFifo[0])
	{
		sprintf (gCall[zCall].lastResponseFifo, "%s",
				 gCall[zCall].responseFifo);
	}

	memset (gCall[zCall].lastInData, 160, 0);

	if (canDeleteResponseFifo == 1)
	{
		if (gCall[zCall].responseFifoFd >= 0)
		{
			arc_close (zCall, mod, __LINE__, &(gCall[zCall].responseFifoFd));

			/*ArcIPResp will unlink this FIFO once app is gone */
			//arc_unlink(zCall, mod, gCall[zCall].responseFifo);

			gCall[zCall].responseFifoFd = -1;
		}

	}							/*END: if */

	arc_fax_clone_globals_to_gCall (zCall);

	if (gCall[zCall].leg != LEG_A || gCall[zCall].thirdParty == 1)
	{
		gCall[zCall].responseFifoFd = -1;
	}

	gCall[zCall].responseFifo[0] = 0;

	gCall[zCall].leg = LEG_A;
	gCall[zCall].crossPort = -1;
    gCall[zCall].in_speakfile = 0;

	gCall[zCall].lastReadPointer = 0;
	gCall[zCall].lastWritePointer = 0;

	gCall[zCall].gUtteranceFile[0] = 0;
	gCall[zCall].mrcpServer[0] = 0;
	gCall[zCall].mrcpRtpPort = 0;
	gCall[zCall].mrcpRtcpPort = 0;
	gCall[zCall].canBargeIn = 0;

	gCall[zCall].googleStreamingSR = 0;
	gCall[zCall].googlePromptIsPlaying = 0;
	gCall[zCall].googleBarginDidOccur = 0;
	gCall[zCall].speechRecFromClient = 0;
	gCall[zCall].speechRec = 0;
	gCall[zCall].parent_idx = -1;

	gCall[zCall].waitForPlayEnd = 0;
#ifdef VOICE_BIOMETRICS
    gCall[zCall].continuousVerify   = CONTINUOUS_VERIFY_INACTIVE;
    gCall[zCall].avb_bCounter   = 0;
#endif  // END: VOICE_BIOMETRICS


	/*RTP Sleep */
	gCall[zCall].lastOutRtpTime.millitm = 0;
	gCall[zCall].lastInRtpTime.millitm = 0;

	gCall[zCall].lastOutRtpTime.time = 0;
	gCall[zCall].lastInRtpTime.time = 0;

	gCall[zCall].outboundRetCode = 0;
	/*GLOBAL Integers */
	gCall[zCall].GV_CallProgress = 0;
	gCall[zCall].GV_SRDtmfOnly = 0;
	gCall[zCall].GV_MaxPauseTime = 60;
	gCall[zCall].GV_PlayBackBeepInterval = 5;
	gCall[zCall].GV_KillAppGracePeriod = 5;
	gCall[zCall].GV_RecordTermChar = ' ';
    gCall[zCall].GV_HideDTMF = 0;


	/*Inband Tone Detection */
	gCall[zCall].toneDetect = 0;
	gCall[zCall].toneDetected = 0;
	gCall[zCall].detectionTID = -1;
    gCall[zCall].telephonePayloadType = -1;
    gCall[zCall].runToneThread = 0;

	 /**/
		/*GLOBAL Strings */
		gCall[zCall].GV_DtmfBargeinDigits[0] = '\0';
	gCall[zCall].GV_PlayBackPauseGreetingDirectory[0] = '\0';
	gCall[zCall].GV_CallingParty[0] = '\0';
	gCall[zCall].GV_CustData1[0] = '\0';
	gCall[zCall].GV_CustData2[0] = '\0';
	gCall[zCall].GV_DtmfBargeinDigitsInt = 0;

	sprintf(gCall[zCall].GV_SkipTerminateKey,	GV_SkipTerminateKey);
	sprintf(gCall[zCall].GV_SkipRewindKey,		GV_SkipRewindKey);
	sprintf(gCall[zCall].GV_SkipForwardKey,		GV_SkipForwardKey);
	sprintf(gCall[zCall].GV_SkipBackwardKey,	GV_SkipBackwardKey);
	sprintf(gCall[zCall].GV_PauseKey,			GV_PauseKey);
	sprintf(gCall[zCall].GV_VolumeUpKey,		GV_VolumeUpKey);
	sprintf(gCall[zCall].GV_VolumeDownKey,		GV_VolumeDownKey);
	sprintf(gCall[zCall].GV_SpeedUpKey,			GV_SpeedUpKey);
	sprintf(gCall[zCall].GV_SpeedDownKey,		GV_SpeedDownKey);
	sprintf(gCall[zCall].GV_ResumeKey,			GV_ResumeKey);

	gCall[zCall].GV_Volume = 5;
	gCall[zCall].GV_Speed = 0;
	gCall[zCall].GV_OutboundCallParm[0] = '\0';
	gCall[zCall].GV_InboundCallParm[0] = '\0';
    gCall[zCall].GV_FaxState = 0;

	gCall[zCall].crossPort = -1;
	gCall[zCall].rtpSendGap = 160;
	gCall[zCall].sendingSilencePackets = 1;
	gCall[zCall].dtmfAvailable = 0;

	gCall[zCall].conferenceIdx = 0;

	gCall[zCall].dtmfCacheCount = 0;
	gCall[zCall].dtmfInCache[0] = -1;

	gCall[zCall].speakStarted = 0;
	gCall[zCall].firstSpeakStarted = 0;

	//gCall[zCall].rtp_session  = NULL;
	//gCall[zCall].rtp_session_in   = NULL;

	gCall[zCall].lastDtmfTimestamp = 0;
	gCall[zCall].in_user_ts = 0;
	gCall[zCall].out_user_ts = 0;
	gCall[zCall].inTsIncrement = 160;
	gCall[zCall].outTsIncrement = 160;

	gCall[zCall].lastDtmf = 16;

	gCall[zCall].callNum = zCall;
	setCallState (zCall, CALL_STATE_IDLE, __LINE__);
	gCall[zCall].stopSpeaking = 0;
	gCall[zCall].lastTimeThreadId = 0;
	//gCall[zCall].outputThreadId   = 0;
	gCall[zCall].permanentlyReserved = 0;
	gCall[zCall].appPassword = 0;
	gCall[zCall].appPid = -1;

	gCall[zCall].lastError = CALL_NO_ERROR;

	arc_close (zCall, mod, __LINE__, &gCall[zCall].srClientFifoFd);
#if 0
	gCall[zCall].srClientFifoFd = -1;
#endif

	gCall[zCall].ttsClientFifoFd = -1;

	for (j = 0; j < MAX_THREADS_PER_CALL; j++)
	{
		gCall[zCall].threadInfo[j].appRef = -1;
		gCall[zCall].threadInfo[j].threadId = 0;
	}

	gCall[zCall].ttsRtpPort = (zCall * 2 + LOCAL_STARTING_RTP_PORT) + 700;
	gCall[zCall].ttsRtcpPort = (zCall * 2 + LOCAL_STARTING_RTP_PORT) + 701;
	gCall[zCall].ttsMrcpFileFd = -1;
	gCall[zCall].keepSpeakingMrcpTTS = 0;
	gCall[zCall].finishTTSSpeakAndExit = 0;
	gCall[zCall].ttsRequestReceived = 0;

#ifdef SR_MRCP
	gCall[zCall].pFirstRtpMrcpData = NULL;
	gCall[zCall].pLastRtpMrcpData = NULL;

	if (gCall[zCall].gUtteranceFileFp != NULL)
	{
		if (gCall[zCall].recordUtteranceWithWav == 1)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Writing wav header to file (%s).",
					   gCall[zCall].lastRecUtteranceFile);
			writeWavHeaderToFile (zCall, gCall[zCall].gUtteranceFileFp);
		}
		fclose (gCall[zCall].gUtteranceFileFp);
		gCall[zCall].gUtteranceFileFp = NULL;

		/*Trim the start of file */
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling trimStartOfWavFile(); utteranceTime1=%ld, utteranceTime2=%ld.",
				   gCall[zCall].utteranceTime1, gCall[zCall].utteranceTime2);
		trimStartOfWavFile (zCall, gCall[zCall].lastRecUtteranceFile,
							gCall[zCall].utteranceTime1,
							gCall[zCall].utteranceTime2);
	}
	gCall[zCall].gUtteranceFileFp = NULL;
	gCall[zCall].recordUtteranceWithWav = 1;
	gCall[zCall].utteranceTime1 = 0;
	gCall[zCall].utteranceTime2 = 0;

	memset (gCall[zCall].gUtteranceFile, 0,
			sizeof (gCall[zCall].gUtteranceFile));

	if (gCall[zCall].rtp_session_mrcp != NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling arc_rtp_session_destroy(); rtp_session_mrcp socket=%d.",
				   gCall[zCall].rtp_session_mrcp->rtp.socket);

		rtp_session_destroy (gCall[zCall].rtp_session_mrcp);
		gCall[zCall].rtp_session_mrcp = NULL;
	}
	gCall[zCall].mrcpTs = 0;
	gCall[zCall].mrcpTime = 0;

	//gCall[zCall].rtp_session_mrcp = NULL;
#endif

	pthread_mutex_lock (&gCall[zCall].gMutexSpeak);
	while (gCall[zCall].pFirstSpeak != NULL)
	{
		if (gCall[zCall].pFirstSpeak->msgSpeak.list == 2)	// MRCP TTS
		{
			arc_unlink (zCall, mod, gCall[zCall].pFirstSpeak->msgSpeak.key);
		}

		pSpeak = gCall[zCall].pFirstSpeak->nextp;
		arc_free (zCall, mod, __LINE__, gCall[zCall].pFirstSpeak,
				  sizeof (SpeakList));
		free (gCall[zCall].pFirstSpeak);
		gCall[zCall].pFirstSpeak = pSpeak;
	}
	pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);

	gCall[zCall].pFirstSpeak = NULL;
	gCall[zCall].pLastSpeak = NULL;
	gCall[zCall].pendingInputRequests = 0;
	gCall[zCall].pendingOutputRequests = 0;

	pthread_mutex_lock (&gCall[zCall].gMutexBkpSpeak);
	while (gCall[zCall].pFirstBkpSpeak != NULL)
	{
		if (gCall[zCall].pFirstBkpSpeak->msgSpeak.list == 2)	// MRCP TTS
		{
			arc_unlink (zCall, mod,
						gCall[zCall].pFirstBkpSpeak->msgSpeak.key);
		}

		pSpeak = gCall[zCall].pFirstBkpSpeak->nextp;
		arc_free (zCall, mod, __LINE__, gCall[zCall].pFirstBkpSpeak,
				  sizeof (SpeakList));
		free (gCall[zCall].pFirstBkpSpeak);
		gCall[zCall].pFirstBkpSpeak = pSpeak;
	}
	pthread_mutex_unlock (&gCall[zCall].gMutexBkpSpeak);

	gCall[zCall].pFirstBkpSpeak = NULL;
	gCall[zCall].pLastBkpSpeak = NULL;

	clearAppRequestList (zCall);

	gCall[zCall].pendingInputRequests = 0;
	gCall[zCall].pendingOutputRequests = 0;

	gCall[zCall].speakProcessWriteFifoFd = -1;
	gCall[zCall].speakProcessReadFifoFd = -1;

	memset (&(gCall[zCall].msgPortOperation), 0, sizeof (struct MsgToApp));

//RG Test for record
	gCall[zCall].isRecordOn = 0;
	gCall[zCall].last_serial_number = 0;

	gCall[zCall].currentSpeak = NULL;
	gCall[zCall].currentPlayMedia = NULL;

	memset (&gCall[zCall].call_type, 0, 32);
	gCall[zCall].bytesToBeSkipped = 0;
	gCall[zCall].recordStarted = 0;
	gCall[zCall].recordStartedForDisconnect = 0;        // MR 5126
	gCall[zCall].currentAudio.appRef = -1;
	gCall[zCall].currentAudio.isReadyToPlay = NO;
	gCall[zCall].audioLooping = SPECIFIC;
	gCall[zCall].isBeepActive = 0;
	memset (gCall[zCall].audioCodecString, 0, 50);
	gCall[zCall].isCallAnswered = NO;
	gCall[zCall].isCallInitiated = NO;
	generateAudioSsrc (zCall);
	gCall[zCall].isIFrameDetected = 0;
	memset (gCall[zCall].rdn, 0, 32);
	memset (gCall[zCall].ocn, 0, 32);
	gCall[zCall].full_duplex = SENDRECV;
	gCall[zCall].recordOption = WITHOUT_RTP;
	gCall[zCall].syncAV = -1;
	gCall[zCall].appRefToBePlayed = -1;
	gCall[zCall].clearPlayList = 1;

	//gCall[zCall].pPlayQueueList = NULL;
	gCall[zCall].isG729AnnexB = NO;
	gCall[zCall].lastRecordedRtpTs = 0;
	//gCall[zCall].outboundSocket = -1;
	gCall[zCall].lastStopActivityAppRef = -1;
	gCall[zCall].attachedSRClient = 0;

	cleanPlayQueueList (zCall);

	if (gCall[zCall].ttsFd != -1)
	{
		arc_close (zCall, mod, __LINE__, &gCall[zCall].ttsFd);
	}
	gCall[zCall].ttsFd = -1;

	gCall[zCall].GV_RecordUtterance = 0;
	gCall[zCall].recordUtteranceWithWav = 1;
	memset (gCall[zCall].lastRecUtteranceFile, 0, 128);
	memset (gCall[zCall].lastRecUtteranceType, 0, 128);

	gCall[zCall].callState = CALL_STATE_IDLE;
	gCall[zCall].callSubState = CALL_STATE_IDLE;

	if (gCall[zCall].gSilentInRecordFileFp != NULL)
	{
		gCall[zCall].silentRecordFlag = 0;
		//arc_frame_record_to_file(zCall, AUDIO_MIXED, (char *)__func__, __LINE__);
		//writeWavHeaderToFile(zCall, gCall[zCall].gSilentInRecordFileFp);
		//fclose(gCall[zCall].gSilentInRecordFileFp);
		//gCall[zCall].gSilentInRecordFileFp = NULL;
	}

	if (gCall[zCall].gSilentOutRecordFileFp != NULL)
	{
		writeWavHeaderToFile (zCall, gCall[zCall].gSilentOutRecordFileFp);
		fclose (gCall[zCall].gSilentOutRecordFileFp);
		gCall[zCall].gSilentOutRecordFileFp = NULL;
	}

	memset (gCall[zCall].silentInRecordFileName, 0,
			sizeof (gCall[zCall].silentInRecordFileName));
	memset (gCall[zCall].silentOutRecordFileName, 0,
			sizeof (gCall[zCall].silentOutRecordFileName));

	gCall[zCall].silentRecordFlag = 0;

    //arc_audio_frame_init (&gCall[zCall].audio_frames[AUDIO_OUT]);

	arc_audio_frame_reset (&gCall[zCall].audio_frames[CONFERENCE_AUDIO]);

    // experimental 

    arc_audio_frame_free (&gCall[zCall].audio_frames[AUDIO_OUT]);
    arc_audio_frame_free (&gCall[zCall].audio_frames[AUDIO_IN]);
    arc_audio_frame_free (&gCall[zCall].audio_frames[AUDIO_MIXED]);
    arc_audio_frame_free (&gCall[zCall].audio_frames[CONFERENCE_AUDIO]);

    arc_audio_frame_init (&gCall[zCall].audio_frames[AUDIO_OUT], 320, ARC_AUDIO_FRAME_SLIN_16, 1);
    arc_audio_frame_init (&gCall[zCall].audio_frames[AUDIO_IN], 320, ARC_AUDIO_FRAME_SLIN_16, 1);
    arc_audio_frame_init (&gCall[zCall].audio_frames[AUDIO_MIXED], 320, ARC_AUDIO_FRAME_SLIN_16, 1);
    arc_audio_frame_init (&gCall[zCall].audio_frames[CONFERENCE_AUDIO], 320, ARC_AUDIO_FRAME_SLIN_16, 1);

    // to try and remedy a mem leak ^^^


    // added to try and get mem leak 
#if 0
//	fprintf(stderr, " %s(%d) freeing decoder resources called from %s\n", __func__, zCall, mod);
    arc_encoder_free(&gCall[zCall].encodeAudio[AUDIO_OUT]);
    arc_encoder_free(&gCall[zCall].encodeAudio[AUDIO_IN]);
    arc_encoder_free(&gCall[zCall].encodeAudio[AUDIO_MIXED]);
    arc_decoder_free(&gCall[zCall].decodeAudio[AUDIO_OUT]);
    arc_decoder_free(&gCall[zCall].decodeAudio[AUDIO_IN]);
    arc_decoder_free(&gCall[zCall].decodeAudio[AUDIO_MIXED]);
#endif 


#ifdef ACU_RECORD_TRAILSILENCE
	//gCall[zCall].zAcuPort = -1;
#endif
	for (int i = 0; i < MAX_CONF_PORTS; i++)
	{
		gCall[zCall].conf_ports[i] = -1;
	}
	gCall[zCall].confUserCount = 0;

	gCall[zCall].conferenceStarted = 0;

#ifdef ACU_LINUX
	if (gCall[zCall].isFaxReserverResourceCalled == 1)
	{
	struct MsgToDM  yReleaseSendFaxResource;
		memcpy (&yReleaseSendFaxResource, &(gCall[zCall].msgToDM),
				sizeof (struct MsgToDM));
		yReleaseSendFaxResource.opcode = DMOP_FAX_RELEASE_RESOURCE;
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling sendRequestToSTonesFaxClient() with DMOP_FAX_RELEASE_RESOURCE.");

	int             rc =
		sendRequestToSTonesFaxClient (mod,
									  (struct MsgToDM *)
									  &yReleaseSendFaxResource, zCall);
		gCall[zCall].isFaxReserverResourceCalled = 0;
	}
	gCall[zCall].isSendRecvFaxActive = 0;

#endif

	gCall[zCall].isCalea = -1;
	gCall[zCall].GV_BridgeRinging = 0;
	gCall[zCall].GV_FlushDtmfBuffer = 0;
	gCall[zCall].GV_LastPlayPostion = 0;
	gCall[zCall].GV_SkipTimeInSeconds = 2;

	gCall[zCall].thirdParty = 0;

	gCall[zCall].GV_Volume = 5;

	memset (&gCall[zCall].answerCallResp, 0,
			sizeof (gCall[zCall].answerCallResp));
	gCall[zCall].isItAnswerCall = 0;

	gCall[zCall].confPacketCount = 0;

	if (gCall[zCall].rtp_session_mrcp_tts_recv != NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_RTP,
					  "Destroying rtp_session_mrcp_tts_recv", "Call", zCall);
		arc_rtp_session_destroy (zCall,
								 &gCall[zCall].rtp_session_mrcp_tts_recv);
		//gCall[zCall].rtp_session_mrcp_tts_recv = NULL;
	}

	gCall[zCall].tts_ts = 0;

	gCall[zCall].currentPhraseOffset = -1;
	gCall[zCall].currentPhraseOffset_save = -1;
	gCall[zCall].currentSpeakSize = -1;

	gCall[zCall].poundBeforeLeadSilence = 0; // BT-226

	//Close Google udpport
	if(gCall[zCall].googleUDPFd > -1)
	{
		close(gCall[zCall].googleUDPFd);
		gCall[zCall].googleUDPFd = -1;
		gCall[zCall].googleUDPPort = -1;
	}
	memset (gCall[zCall].callCDR, 0, sizeof (gCall[zCall].callCDR));
    clearPortUnlock(zCall);

	gCall[zCall].googleRecord = NO;
	gCall[zCall].googleRecordIsActive = 0;
	memset (&gCall[zCall].googleSRResponse, 0, sizeof(gCall[zCall].googleSRResponse));

	return (0);

}								/*END: int clearPort */

/* JOES removed else if's   */

void           *
inbandToneDetectionThread (void *args)
{

	char			mod[]="inbandToneDetectionThread";
	int             zCall = -1;
	int             detect;
	char            result[256] = "test";
	time_t          timeo;
	int             seconds;
	struct Msg_CallProgress yMsgCallProgress;
	time_t          tb;
	time_t			gsLeadTimer;
	time_t			currentTime;

	zCall = (long) args;

	int             yOrigOpcode = gCall[zCall].msgToDM.opcode;

	int				toneDetectTimeout = 10; //Default value... will be changed to 2 once SPEAKER is detected.

	yMsgCallProgress.opcode = gCall[zCall].msgToDM.opcode;
	yMsgCallProgress.appCallNum = gCall[zCall].msgToDM.appCallNum;
	yMsgCallProgress.appPid = gCall[zCall].msgToDM.appPid;
	yMsgCallProgress.appRef = gCall[zCall].msgToDM.appRef;
	yMsgCallProgress.appPassword = gCall[zCall].msgToDM.appPassword;
	yMsgCallProgress.opcode = DMOP_INITIATECALL;
	yMsgCallProgress.origOpcode = DMOP_INITIATECALL;

	time (&timeo);

	if (gCall[zCall].runToneThread == 0)
	{
		dynVarLog (__LINE__, zCall, "inbandToneDetectionThread", REPORT_VERBOSE,
			   TEL_BASE, INFO, "Inband thread TID=%d was delayed and stop is already issued. Exiting",
			   gCall[zCall].detectionTID);
	}
	else
	{

	dynVarLog (__LINE__, zCall, "inbandToneDetectionThread", REPORT_VERBOSE,
			   TEL_BASE, INFO, "Inband thread TID=%d started.",
			   gCall[zCall].detectionTID);
	}

	//gCall[zCall].runToneThread = 1;
	gCall[zCall].GV_LastCallProgressDetail = 0;
	gCall[zCall].toneDetected = 0;

	if ( gCall[zCall].googleStreamingSR == 1 )
	{
		time(&gsLeadTimer);
	}
	while (gCall[zCall].runToneThread == 1)
	{
//		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
//						TEL_BASE, INFO, "DJB: beginning of loop.");
		// we will delay human/answering machine until the call is answered 
		if ( gCall[zCall].googlePromptIsPlaying == 1 )
		{
			time(&currentTime);
			if ( currentTime - gsLeadTimer >= 1 )
			{
				
				arc_tones_increment_leading_samples(&gCall[zCall].toneDetectionBuff, 4000);
//				dynVarLog (__LINE__, zCall, (char *) __func__,
//					   REPORT_VERBOSE, TEL_BASE, INFO,
//						"DJBTONES: toneDetectionBuff.leading_samples updated to %d.",
//						gCall[zCall].toneDetectionBuff.leading_samples);
				time(&gsLeadTimer);
			}
		}
			

		if (gCall[zCall].isCallAnswered || gCall[zCall].isCallInitiated)
		{
			detect =
				arc_tones_ringbuff_detect (&gCall[zCall].toneDetectionBuff,
										   gCall[zCall].toneDetect, toneDetectTimeout);
		}
		else
		{
	int             flags =
		gCall[zCall].toneDetect ^ (ARC_TONES_HUMAN | ARC_TONES_RECORDING);
			//int flags = gCall[zCall].toneDetect ^ (ARC_TONES_INBAND_DTMF);

			detect =
				arc_tones_ringbuff_detect (&gCall[zCall].toneDetectionBuff,
										   flags, toneDetectTimeout);
		}

		gCall[zCall].toneDetected |= detect;

		if (detect == ARC_TONES_NONE)
		{
			;;
		}

		if (gCall[zCall].GV_CallProgress != 0)
		{
			if (detect & ARC_TONES_HUMAN)
			{
				// do something and remove it from the mask once detected 
				// these have to be exclusive of other detected tones
				// so it is best to check against the already detecetd tone
				if (gCall[zCall].toneDetected == ARC_TONES_HUMAN)
				{

					dynVarLog (__LINE__, zCall, (char *) __func__,
							   REPORT_VERBOSE, TEL_BASE, INFO,
							   "ARC_TONES_HUMAN; arc_tones_get_result_string() returned (%s).",
							   arc_tones_get_result_string (&gCall[zCall].
															toneDetectionBuff,
															ARC_TONES_HUMAN,
															result,
															sizeof (result)));

					sprintf (yMsgCallProgress.nameValue, "callProgress=%d",
							 152);
					memcpy (&gCall[zCall].msgToDM, &yMsgCallProgress,
							sizeof (struct MsgToDM));
					process_DMOP_END_CALL_PROGRESS (zCall);
				}
				gCall[zCall].toneDetect &= (~ARC_TONES_HUMAN);

				stopInbandToneDetectionThread (__LINE__, zCall);
				// end human detection 
			}

			if (detect & ARC_TONES_RECORDING)
			{
				// do something and remove it from the mask once detected 
				// these have to be exclusive of other detected tones
				// so it is best to check against the already detecetd tone
				if (gCall[zCall].toneDetected == ARC_TONES_RECORDING)
				{

					dynVarLog (__LINE__, zCall, (char *) __func__,
							   REPORT_VERBOSE, TEL_BASE, INFO,
							   "ARC_TONES_RECORDING; arc_tones_get_result_string() returned (%s).",
							   arc_tones_get_result_string (&gCall[zCall].
															toneDetectionBuff,
															ARC_TONES_RECORDING,
															result,
															sizeof (result)));

					sprintf (yMsgCallProgress.nameValue, "callProgress=%d",
							 153);
					memcpy (&gCall[zCall].msgToDM, &yMsgCallProgress,
							sizeof (struct MsgToDM));
					process_DMOP_END_CALL_PROGRESS (zCall);
				}
				gCall[zCall].toneDetect &= (~ARC_TONES_HUMAN);
				stopInbandToneDetectionThread (__LINE__, zCall);
				// end human detection 
			}

			if (detect & ARC_TONES_FAST_BUSY)
			{

				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						   TEL_BASE, INFO,
						   "ARC_TONES_FAST_BUSY; arc_tones_get_result_string() returned (%s).",
						   arc_tones_get_result_string (&gCall[zCall].
														toneDetectionBuff,
														ARC_TONES_FAST_BUSY,
														result,
														sizeof (result)));

				sprintf (yMsgCallProgress.nameValue, "callProgress=%d", 52);
				memcpy (&gCall[zCall].msgToDM, &yMsgCallProgress,
						sizeof (struct MsgToDM));
				process_DMOP_END_CALL_PROGRESS (zCall);
				stopInbandToneDetectionThread (__LINE__, zCall);
			}

			if (detect & ARC_TONES_DIAL_TONE)
			{

				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						   TEL_BASE, INFO,
						   "ARC_TONES_DIAL_TONE; arc_tones_get_result_string() returned (%s).",
						   arc_tones_get_result_string (&gCall[zCall].
														toneDetectionBuff,
														ARC_TONES_DIAL_TONE,
														result,
														sizeof (result)));

				sprintf (yMsgCallProgress.nameValue, "callProgress=%d", 155);
				memcpy (&gCall[zCall].msgToDM, &yMsgCallProgress,
						sizeof (struct MsgToDM));
				process_DMOP_END_CALL_PROGRESS (zCall);
				stopInbandToneDetectionThread (__LINE__, zCall);
			}

			if (detect & ARC_TONES_RINGBACK)
			{

				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						   TEL_BASE, INFO,
						   "ARC_TONES_RINGBACK; arc_tones_get_result_string() returned (%s).",
						   arc_tones_get_result_string (&gCall[zCall].
														toneDetectionBuff,
														ARC_TONES_RINGBACK,
														result,
														sizeof (result)));

				sprintf (yMsgCallProgress.nameValue, "callProgress=%d", 156);
				memcpy (&gCall[zCall].msgToDM, &yMsgCallProgress,
						sizeof (struct MsgToDM));
				process_DMOP_END_CALL_PROGRESS (zCall);
				stopInbandToneDetectionThread (__LINE__, zCall);
			}

	        if (detect & ARC_TONES_SIT_TONE)
			{

				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						   TEL_BASE, INFO,
						   "ARC_TONES_SIT_TONE; arc_tones_get_result_string() returned (%s).",
						   arc_tones_get_result_string (&gCall[zCall].
														toneDetectionBuff,
														ARC_TONES_RINGBACK,
														result,
														sizeof (result)));

				sprintf (yMsgCallProgress.nameValue, "callProgress=%d", 156);
				memcpy (&gCall[zCall].msgToDM, &yMsgCallProgress,
						sizeof (struct MsgToDM));
				process_DMOP_END_CALL_PROGRESS (zCall);
				stopInbandToneDetectionThread (__LINE__, zCall);
			}

		}						/*gCall[zCall].GV_CallProgress != 0) */

		if (detect & ARC_TONES_FAX_TONE)
		{

			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   TEL_BASE, INFO,
					   "ARC_TONES_FAX_TONE; arc_tones_get_result_string() returned (%s).",
					   arc_tones_get_result_string (&gCall[zCall].
													toneDetectionBuff,
													ARC_TONES_FAX_TONE,
													result, sizeof (result)));

			sprintf (yMsgCallProgress.nameValue, "callProgress=%d", 151);
			memcpy (&gCall[zCall].msgToDM, &yMsgCallProgress,
					sizeof (struct MsgToDM));
			process_DMOP_END_CALL_PROGRESS (zCall);
			stopInbandToneDetectionThread (__LINE__, zCall);
		}

		if (detect & ARC_TONES_FAX_CED)
		{

			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   TEL_BASE, INFO,
					   "ARC_TONES_FAX_CED; arc_tones_get_result_string() returned (%s).",
					   arc_tones_get_result_string (&gCall[zCall].
													toneDetectionBuff,
													ARC_TONES_FAX_CED,
													result, sizeof (result)));

			sprintf (yMsgCallProgress.nameValue, "callProgress=%d", 151);
			memcpy (&gCall[zCall].msgToDM, &yMsgCallProgress,
					sizeof (struct MsgToDM));
			process_DMOP_END_CALL_PROGRESS (zCall);
			stopInbandToneDetectionThread (__LINE__, zCall);
		}

#if 0
		if (detect & ARC_TONES_INBAND_DTMF)
		{
			// use the dtmf dispatch routine 

			if (gCall[zCall].toneDetectionBuff.dtmf[0] != '\0')
			{
	int             i;

				for (i = 0; gCall[zCall].toneDetectionBuff.dtmf[i] != '\0';
					 i++)
				{
					dynVarLog (__LINE__, zCall, (char *) __func__,
							   REPORT_VERBOSE, TEL_BASE, INFO,
							   "SpanDSP: Inband DTMF char %c",
							   gCall[zCall].toneDetectionBuff.dtmf[i]);
					// disptach dtmf char here 
				}
			}
		}
#endif
		if (detect & ARC_TONES_BEGINNING_AUDIO_TIMEOUT)
		{

			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   TEL_BASE, INFO,
					   "ARC_TONES_BEGINNING_AUDIO_TIMEOUT; arc_tones_get_result_string() returned (%s).",
					   arc_tones_get_result_string (&gCall[zCall].
													toneDetectionBuff,
													ARC_TONES_BEGINNING_AUDIO_TIMEOUT,
													result, sizeof (result)));

			gCall[zCall].GV_LastCallProgressDetail = ARC_TONES_BEGINNING_AUDIO_TIMEOUT;
			arcDtmfData[zCall].lead_silence_triggered = 1;

			// GS - need to ignore this here as the prompt is playing.  It is activated 
			//      by a PROMPT END and the lead-silence timer is in the recordFile_X()
			//
			if(gCall[zCall].googleUDPFd > -1)
			{
	__DDNDEBUG__ (DEBUG_MODULE_SR, "MRCP: ARCGS: Closing googleUDPFd", "", gCall[zCall].googleUDPFd);
	
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   		"GSR: Sending 'bye' to client.");
				sendto(gCall[zCall].googleUDPFd,
						"bye", 
						3, 
						0,
						(struct sockaddr *) &gCall[zCall].google_si, 
						gCall[zCall].google_slen);	
	
				close(gCall[zCall].googleUDPFd);
				gCall[zCall].googleUDPPort = -1;
				gCall[zCall].googleUDPFd = -1;
				gCall[zCall].speechRec = 0;
			}
			if ( gCall[zCall].googleStreamingSR == 1 )
			{
				gCall[zCall].runToneThread = 0;
			}
		}

		if (detect & ARC_TONES_ACTIVE_SPEAKER){
			time (&tb);
			gCall[zCall].utteranceTime2 = tb;

			toneDetectTimeout = 2;

			if((gCall[zCall].GV_LastCallProgressDetail & ARC_TONES_ACTIVE_SPEAKER) == 0)
			{
				// then set it and log only once per session  
				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						TEL_BASE, INFO,
						"ARC_TONES_ACTIVE_SPEAKER; arc_tones_get_result_string() returned (%s).",
						arc_tones_get_result_string (&gCall[zCall].
						toneDetectionBuff,
						ARC_TONES_TRAILING_AUDIO_TIMEOUT,
						result, sizeof (result)));
				gCall[zCall].GV_LastCallProgressDetail = ARC_TONES_ACTIVE_SPEAKER;
				if ( gCall[zCall].googleStreamingSR == 1 )
				{
					gCall[zCall].googleBarginDidOccur = 1;
					(void)googleStopActivity(zCall);
					gCall[zCall].googleSRResponse.returnCode = 0;
					memset((char *)gCall[zCall].googleSRResponse.message, '\0', sizeof(gCall[zCall].googleSRResponse.message));
					(void)writeGenericResponseToApp (zCall, &gCall[zCall].googleSRResponse, mod, __LINE__);
				}
			}
        }

		if (detect & ARC_TONES_TRAILING_AUDIO_TIMEOUT)
		{

			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   TEL_BASE, INFO,
					   "ARC_TONES_TRAILING_AUDIO_TIMEOUT; arc_tones_get_result_string() returned (%s).",
					   arc_tones_get_result_string (&gCall[zCall].
													toneDetectionBuff,
													ARC_TONES_TRAILING_AUDIO_TIMEOUT,
													result, sizeof (result)));

			gCall[zCall].GV_LastCallProgressDetail = ARC_TONES_TRAILING_AUDIO_TIMEOUT;
			arcDtmfData[zCall].trail_silence_triggered = 1;
		}

		// if (detect & ARC_TONES_PACKET_TIMEOUT && gCall[zCall].toneDetected == ARC_TONES_PACKET_TIMEOUT)
		if (detect & ARC_TONES_PACKET_TIMEOUT)
		{
	        struct voice_energy_level_t el;

			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   TEL_BASE, INFO,
					   "ARC_TONES_PACKET_TIMEOUT: Timeout (%d sec) waiting for RTP packets from remote end.", 
						toneDetectTimeout);

			arc_tones_get_human_energy_results (&gCall[zCall].
												toneDetectionBuff, &el);

			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   TEL_BASE, INFO,
					   "Voice energy details a0=%f a1=%f a2=%f a3=%f a4=%f peak_rms=%f total_rms=%f samples=%d clips=%d.",
					   el.a0, el.a1, el.a2, el.a3, el.a4, el.peak_rms,
					   el.total_rms, el.samples, el.clips);

			sprintf (yMsgCallProgress.nameValue, "callProgress=%d", 0);

#if 1 // COOMENTING_THIS_OUT_BUT_LEAVING_THE_PRINTF

			if (gCall[zCall].GV_CallProgress != 0)
			{
				memcpy (&gCall[zCall].msgToDM, &yMsgCallProgress,
						sizeof (struct MsgToDM));
				process_DMOP_END_CALL_PROGRESS (zCall);
				stopInbandToneDetectionThread (__LINE__, zCall);
			} else {
                // added to catch runaway tone detection thread 
				stopInbandToneDetectionThread (__LINE__, zCall);
            }

			gCall[zCall].GV_LastCallProgressDetail = ARC_TONES_PACKET_TIMEOUT;

			arcDtmfData[zCall].trail_silence_triggered = 1;
			break;
#endif 
		}

	}							// end while 

	arc_tones_ringbuff_free (&gCall[zCall].toneDetectionBuff);

	gCall[zCall].toneDetect = 0;
	gCall[zCall].toneDetected = 0;
	gCall[zCall].runToneThread = 0;

	dynVarLog (__LINE__, zCall, "inbandToneDetectionThread", REPORT_VERBOSE,
			   TEL_BASE, INFO, "Inband thread exiting.  TID=%d",
			   gCall[zCall].detectionTID);

	gCall[zCall].detectionTID = -1;

	return NULL;

}//END: inbandToneDetectionThread()

#if 1
int
initializeDtmfToneDetectionBuffers (int zCall)
{
	int             rc = -1;
	char            mod[] = "initializeDtmfToneDetectionBuffers";

	dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, TEL_BASE,
			   INFO, "No RFC 2833 was present in SDP.");

	arc_decoder_init (&gCall[zCall].decodeDtmfAudio[AUDIO_IN],
					  ARC_DECODE_G711, &gCall[zCall].g711parms,
					  (int) sizeof (gCall[zCall].g711parms), 1);

	arc_tones_ringbuff_init (&gCall[zCall].dtmfDetectionBuff,
							 ARC_TONES_INBAND_DTMF, gToneThreshMinimum,
							 gToneThreshDifference, 0);

}/*END: initializeDtmfToneDetectionBuffers */

#endif

int
startInbandToneDetectionThread (int zCall, int zLine, int mode)
{

	int             rc = -1;

	// these will be taken from the config 
	int             max_samples = 8000 * 3;

	// end 
	pthread_attr_t  attr;

	pthread_attr_init (&attr);
	pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

	if (zCall < 0 || zCall > MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_INVALID_PORT, ERR,
				   "[%d] Invalid port number received (%d). Unable to start inband tone detection.",
				   zLine, zCall);
		return -1;
	}

	if (access ("/tmp/reloadConfigForInband", F_OK) == 0)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   TEL_BASE, INFO, "[%d] Reloading config file for InbandAudioDetection tuning.", zLine);
		loadConfigFile (NULL, NULL);

   		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO, 
			"[%d] ToneThreshold set to (%f)", zLine, gToneThreshMinimum);
   		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] ToneThresholdDifference set to (%f)", zLine, gToneThreshDifference);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] AnsweringDetectionNoiseThresh set to (%f)", zLine, gAnsweringDetectionNoiseThresh);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] AnsweringMachineThresh set to (%d)", zLine, gAnsweringMachineThresh);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] AnsweringTrailSilenceTrigger set to (%d)", zLine, gAnsweringTrailSilenceTrigger);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] AnsweringOverallTimeout set to (%d)", zLine, gAnsweringOverallTimeout);
   		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] TrailSilenceNoiseThresh set to (%f)", zLine, gToneDetectionNoiseThresh);
   		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO, 
			"[%d] AnsweringLeadingAudioTrim set to (%d)", zLine, gAnsweringLeadingAudioTrim);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,  
			"[%d] HumanAnsweringThresh set to (%f)", zLine, gHumanAnsweringThresh);
	}

	if ((gCall[zCall].detectionTID != -1) && (pthread_kill(gCall[zCall].detectionTID, 0) == 0))
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_PTHREAD_ALREADY_RUNNING, ERR,
				   "[%d] Detection thread already running.  TID=%d; returning failure. ", zLine, 
				   gCall[zCall].detectionTID);
		return -1;
	}

	gCall[zCall].toneDetected = 0;
	gCall[zCall].toneDetect = mode;

	if ( gCall[zCall].googleStreamingSR  == 1 )
	{
		rc = arc_tones_ringbuff_init (&gCall[zCall].toneDetectionBuff,
								  gCall[zCall].toneDetect, gToneThreshMinimum,
								  gToneThreshDifference, 0);  // not threaded 
	}
	else
	{
		rc = arc_tones_ringbuff_init (&gCall[zCall].toneDetectionBuff,
								  gCall[zCall].toneDetect, gToneThreshMinimum,
								  gToneThreshDifference, 1);
	}

	if (gToneDebug)
	{
		arc_tones_set_result_string (&gCall[zCall].toneDetectionBuff,
									 gCall[zCall].toneDetect);
	}

	if (mode & ARC_TONES_HUMAN)
	{
		arc_tones_set_human_thresh (&gCall[zCall].toneDetectionBuff,
									gToneDetectionNoiseThresh,
									gHumanAnsweringThresh,
									gAnsweringMachineThresh, max_samples);
		arc_tones_set_answering_thresh (&gCall[zCall].toneDetectionBuff,
										gAnsweringDetectionNoiseThresh,
										gAnsweringOverallTimeout,
										gAnsweringTrailSilenceTrigger,
										gAnsweringMachineThresh,
										gAnsweringLeadingAudioTrim);
	}

	if (mode & ARC_TONES_BEGINNING_AUDIO_TIMEOUT)
	{
		arc_tones_set_silence_params (&gCall[zCall].toneDetectionBuff,
									  gToneDetectionNoiseThresh,
									  gCall[zCall].msgRecord.lead_silence * 4000,
									  gCall[zCall].msgRecord.trail_silence * 4000);
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, TEL_BASE, INFO,
				"Set lead silence to %d; trail silence to %d.",
				gCall[zCall].msgRecord.lead_silence,
				gCall[zCall].msgRecord.trail_silence);
					
	}

	if (mode & ARC_TONES_ACTIVE_SPEAKER )
	{
		arc_tones_set_human_thresh (&gCall[zCall].toneDetectionBuff,
									gToneDetectionNoiseThresh,
									gHumanAnsweringThresh,
									gAnsweringMachineThresh, max_samples);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
            "Set active-speaker.");
	}

	gCall[zCall].runToneThread = 1;

	// 
	rc = pthread_create (&gCall[zCall].detectionTID, &attr,
						 inbandToneDetectionThread, (void *) zCall);

	if (rc != 0)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_PTHREAD_ERROR, ERR,
				   "[%d] pthread_create() failed. rc=%d. [%d, %s] "
				   "Unable to create inband detection thread. Returning failure.", zLine, 
				   rc, errno, strerror (errno));
		gCall[zCall].detectionTID = -1;
		rc = -1;
	}
	else
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   TEL_BASE, INFO,
				   "[%d] Started inband detection thread;  TID=[%d]", zLine, 
				   gCall[zCall].detectionTID);
	}

	return rc;
}

int
stopInbandToneDetectionThread (int zLine, int zCall)
{
	int             rc = 0;

	if (gCall[zCall].detectionTID == -1)
	{
		return -1;
	}

	gCall[zCall].runToneThread = 0;
	dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, TEL_BASE,
			INFO, "[%d] Stopping inband detection thread.  runToneThread set to %d",
			zLine, gCall[zCall].runToneThread);


	return rc;
}

int
googleStartInbandToneDetectionThread (int zCall, int zLine, int mode)
{
	int             rc = -1;

	// these will be taken from the config 
	int             max_samples = 8000 * 3;

	// end 
	pthread_attr_t  attr;

	pthread_attr_init (&attr);
	pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

	if (zCall < 0 || zCall > MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_INVALID_PORT, ERR,
				   "[%d] Invalid port number received (%d). Unable to start inband tone detection.",
				   zLine, zCall);
		return -1;
	}

	if (access ("/tmp/reloadConfigForInband", F_OK) == 0)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   TEL_BASE, INFO, "[%d] Reloading config file for InbandAudioDetection tuning.", zLine);
		loadConfigFile (NULL, NULL);

   		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO, 
			"[%d] ToneThreshold set to (%f)", zLine, gToneThreshMinimum);
   		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] ToneThresholdDifference set to (%f)", zLine, gToneThreshDifference);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] AnsweringDetectionNoiseThresh set to (%f)", zLine, gAnsweringDetectionNoiseThresh);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] AnsweringMachineThresh set to (%d)", zLine, gAnsweringMachineThresh);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] AnsweringTrailSilenceTrigger set to (%d)", zLine, gAnsweringTrailSilenceTrigger);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] AnsweringOverallTimeout set to (%d)", zLine, gAnsweringOverallTimeout);
   		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			"[%d] TrailSilenceNoiseThresh set to (%f)", zLine, gToneDetectionNoiseThresh);
   		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO, 
			"[%d] AnsweringLeadingAudioTrim set to (%d)", zLine, gAnsweringLeadingAudioTrim);
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,  
			"[%d] HumanAnsweringThresh set to (%f)", zLine, gHumanAnsweringThresh);
	}

	if ((gCall[zCall].detectionTID != -1) && (pthread_kill(gCall[zCall].detectionTID, 0) == 0))
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_PTHREAD_ALREADY_RUNNING, ERR,
				   "[%d] Detection thread already running.  TID=%d; returning failure. ", zLine, 
				   gCall[zCall].detectionTID);
		return -1;
	}

	gCall[zCall].toneDetected = 0;
	gCall[zCall].toneDetect = mode;

	if ( gCall[zCall].googleStreamingSR  == 1 )
	{
		rc = arc_tones_ringbuff_init (&gCall[zCall].toneDetectionBuff,
								  gCall[zCall].toneDetect, gToneThreshMinimum,
								  gToneThreshDifference, 0);  // not threaded 
	}
	else
	{
		rc = arc_tones_ringbuff_init (&gCall[zCall].toneDetectionBuff,
								  gCall[zCall].toneDetect, gToneThreshMinimum,
								  gToneThreshDifference, 1);
	}

	if (gToneDebug)
	{
		arc_tones_set_result_string (&gCall[zCall].toneDetectionBuff,
									 gCall[zCall].toneDetect);
	}

	if (mode & ARC_TONES_HUMAN)
	{
		arc_tones_set_human_thresh (&gCall[zCall].toneDetectionBuff,
									gToneDetectionNoiseThresh,
									gHumanAnsweringThresh,
									gAnsweringMachineThresh, max_samples);
		arc_tones_set_answering_thresh (&gCall[zCall].toneDetectionBuff,
										gAnsweringDetectionNoiseThresh,
										gAnsweringOverallTimeout,
										gAnsweringTrailSilenceTrigger,
										gAnsweringMachineThresh,
										gAnsweringLeadingAudioTrim);
	}

	if (mode & ARC_TONES_BEGINNING_AUDIO_TIMEOUT)
	{
		arc_tones_set_silence_params (&gCall[zCall].toneDetectionBuff,
									  gToneDetectionNoiseThresh,
									  (gCall[zCall].msgRecord.lead_silence + 30) * 4000,
									  gCall[zCall].msgRecord.trail_silence * 4000);
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, TEL_BASE, INFO,
				"Set google lead silence and trail silence.",
				gCall[zCall].msgRecord.trail_silence);
	}

	if (mode & ARC_TONES_ACTIVE_SPEAKER )
	{
		arc_tones_set_human_thresh (&gCall[zCall].toneDetectionBuff,
									gToneDetectionNoiseThresh,
									gHumanAnsweringThresh,
									gAnsweringMachineThresh, max_samples);
	}

	gCall[zCall].runToneThread = 1;

	// 
	rc = pthread_create (&gCall[zCall].detectionTID, &attr,
						 inbandToneDetectionThread, (void *) zCall);

	if (rc != 0)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_PTHREAD_ERROR, ERR,
				   "[%d] pthread_create() failed. rc=%d. [%d, %s] "
				   "Unable to create inband detection thread. Returning failure.", zLine, 
				   rc, errno, strerror (errno));
		gCall[zCall].detectionTID = -1;
		rc = -1;
	}
	else
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   TEL_BASE, INFO,
				   "[%d] Started inband detection thread;  TID=[%d]", zLine, 
				   gCall[zCall].detectionTID);
	}

	return rc;
} // END: googleStartInbandToneDetectionThread ()

/*
  FAX thread sends and recieves 
  for the duration of the fax 
  so it has it's own thread 

  --recv data 
  --decode 
  --process buff 
  --encode 
  --send data

   for audio faxes the call must be set to g711au 
   otherwise forget it it's not worth it 
*/

//
//  This is the FAX structure with all of the options necessary 
//  for setting up a FAX session.
//
//   struct arc_fax_session_parms_t
//   {
//      char filename[256];        must be set 
//      char dirpath[256];         not used at this point, if we ever use a spool 
//      char stationid[100];       user set 
//      char headernfo[100];       user set 
//      int type;                  sending/recieving 
//      int ecm;                   zero or one 
//      int debug;                 sets logging 
//      int use_t38;               flag to turn on t38 
//      int t38_fax_version;       i understand this is 0,1,2,3 
//      int t38_max_buffer;        default is 1024 
//      int t38_max_datagram;      default is 1500 typically the udp mtu 
//      int t38_max_bitrate;       not sure 
//      int t38_fax_rate_mgmt;     not sure yet  
//      int t38_fax_transport;     ??  0 = UDPTL 1 = RTP  ?? 
//      int t38_udp_ec;            rtp or udptl 
//   };
//
//

#define FAX_STATE_SET(state, val) do {state |= val;} while(0)
#define FAX_STATE_UNSET(state, val) do {state &= (~val);} while(0)
#define FAX_STATE_HAS(state, val) (state & val)

void           *faxThread (void *args);

static int      cleanupFaxResources (int zCall, int dmop, int return_code);

int
startFaxThread (int zCall, void *msg, int mode)
{

	enum state_e
	{
		START = 0,
		GOT_SEND_FAX = (1 << 0),
		GOT_RECV_FAX = (1 << 1),
		SENT_T38_LOCAL_DETAILS = (1 << 2),
		GOT_T38_REMOTE_DETAILS = (1 << 3),
		SETUP_T38 = (1 << 5),
		SET_T38_REMOTE_DETAILS = (1 << 6),
		SET_FILE_NAME = (1 << 7),
		// must be last
		FAX_READY = (1 << 15)
	};

	int             rc = 0;
	pthread_attr_t  attr;
	int             idx = -1;
	struct Msg_RecvFax *lpGetFax = (struct Msg_RecvFax *) msg;
	struct Msg_SendFax *lpSendFax = (struct Msg_SendFax *) msg;
	struct MsgToDM *msg_to_dm = (struct MsgToDM *) msg;

	// i send this
	struct Msg_Fax_Proceed fax_response;

	// i get  this 
	struct MsgToDM  fax_details;

	// then I can continue 

	int             startport = gUdptlStartPort;
	int             endport = gUdptlEndPort;

	//fprintf(stderr, " fax state=0x%x\n", gCall[zCall].GV_FaxState);

	if (zCall < 0 || zCall > MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_INVALID_PORT, ERR,
				   "Invalid port number received (%d). Unable to process fax request.",
				   zCall);
		return -1;
	}

	idx = zCall % 48;

#if 0
	if (idx == -1)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   TEL_INVALID_PORT, ERR,
				   "Fax index is out of range, cannot continue",);
		return -1;
	}
#endif
	if (msg == NULL)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   FAX_INVALID_DATA, ERR,
				   "Empty fax message received; unable to process fax request.");
		return -1;
	}

	if (gCall[zCall].GV_FaxState == START)
	{
		memset (&FaxParms[idx], 0, sizeof (FaxParms[idx]));
		snprintf (FaxParms[idx].stationid, sizeof (FaxParms[idx].stationid),
				  "%s", gCall[zCall].GV_T30FaxStationId);
		snprintf (FaxParms[idx].headernfo, sizeof (FaxParms[idx].headernfo),
				  "%s", gCall[zCall].GV_T30HeaderInfo);
		FaxParms[idx].ecm = gCall[zCall].GV_T30ErrorCorrection;
		FaxParms[idx].debug = gCall[zCall].GV_FaxDebug;
		//fprintf(stderr, " %s() t38 max bitrate set to [%d]\n", __func__, gT38MaxBitRate);
		FaxParms[idx].t38_max_bitrate = gT38MaxBitRate;
		FaxParms[idx].t38_fax_rate_mgmt = gT38FaxRateManagement;
		FaxParms[idx].use_t38 = gCall[zCall].GV_T38Enabled;
		FaxParms[idx].t38_fax_version = gCall[zCall].GV_T38FaxVersion;
		FaxParms[idx].t38_max_buffer = gT38FaxMaxBuffer;
		FaxParms[idx].t38_fax_transport = gCall[zCall].GV_T38Transport;
	}

	switch (msg_to_dm->opcode)
	{
	case DMOP_SENDFAX:
		//fprintf(stderr, " %s() got SENDFAX\n", __func__);
		FAX_STATE_SET (gCall[zCall].GV_FaxState, GOT_SEND_FAX);
		break;
	case DMOP_RECVFAX:
		// fprintf(stderr, " %s() got RECVFAX\n", __func__);
		FAX_STATE_SET (gCall[zCall].GV_FaxState, GOT_RECV_FAX);
		break;
	case DMOP_FAX_SEND_SDPINFO:
		//fprintf(stderr, " %s() got FAX_SDP_INFO\n", __func__);
		memset (&fax_details, 0, sizeof (fax_details));
		memcpy (&fax_details, msg_to_dm, sizeof (struct MsgToDM));
		FAX_STATE_SET (gCall[zCall].GV_FaxState, GOT_T38_REMOTE_DETAILS);
		break;
	default:
//		fprintf (stderr, " %s() got unhandled opcode=%d\n", __func__,
//				 msg_to_dm->opcode);
		break;
	}

	memset (&fax_response, 0, sizeof (fax_response));
	memcpy (&fax_response, msg, sizeof (struct MsgToDM));

	// put both input thread and output thread to sleep
	// start fax sthread
	if (gCall[zCall].isSendRecvFaxActive == 1)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   FAX_RESOURCE_NOT_AVAILABLE, ERR,
				   "Fax flag is set to active, fax process already running on this port.  Returning failure.");
		return -1;
	}

	if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_SEND_FAX) ||
		FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_RECV_FAX))
	{
		// parse out file name from nameValue 
		if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, SET_FILE_NAME) == 0)
		{
			if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_SEND_FAX))
			{
				FaxParms[idx].type = ARC_FAX_SESSION_OUTBOUND;
				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						   FAX_BASE, INFO, "Sending Fax(TX); file=(%s).",
						   lpSendFax->faxFile);
				// parse it out here 
				snprintf (FaxParms[idx].filename,
						  sizeof (FaxParms[idx].filename), "%s",
						  lpSendFax->faxFile);
			}
			if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_RECV_FAX))
			{
				FaxParms[idx].type = ARC_FAX_SESSION_INBOUND;
				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						   FAX_BASE, INFO, " Recieving Fax(RX); file=(%s).",
						   lpGetFax->faxFile);
				// parse it out here
				snprintf (FaxParms[idx].filename,
						  sizeof (FaxParms[idx].filename), "%s",
						  lpGetFax->faxFile);
			}
			FAX_STATE_SET (gCall[zCall].GV_FaxState, SET_FILE_NAME);
		}
	}

	// for audio fax only 

	if (FaxParms[idx].use_t38 == 0)
	{
		switch (gCall[zCall].codecType)
		{
		case 0:
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   FAX_BASE, INFO,
					   "Using G.711u for audio fax transmission.");
			break;
		case 8:
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   FAX_BASE, INFO,
					   "Using G.711a for audio fax transmission.");
			break;
		default:
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
					   FAX_INVALID_DATA, ERR,
					   "Invalid codec (%d). Audio codec must be either g711u(%d) or g711a(%d), "
					   "Unable to process fax.", gCall[zCall].codecType, 0,
					   8);
			return -1;
			break;
		}

		FAX_STATE_SET (gCall[zCall].GV_FaxState, FAX_READY);

		// djb - log message - is a new thread really being started?
//      dynVarLog(__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, FAX_BASE, INFO,
//            "Audio fax has been selected, starting thread [state=%d]\n",
//           gCall[zCall].GV_FaxState);
	}

	// end audio fax
	// open t.38 udptl sock if set

	if (FaxParms[idx].use_t38 != 0)
	{
	int             stat = -1;
	int             i;
	int             local_fd = -1;
	int             remote_fd = -1;
	char            local[100] = "";
	char            remote[100] = "";
	unsigned short  lport = 0;
	unsigned short  rport = 0;

		// i had to add this becuase we might have to 
		// handle the invite form the gateway 

		if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, SETUP_T38) == 0)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   FAX_BASE, INFO,
					   "Setting up udptl transmission for T.38 Fax.");

			FaxUdptlRx[idx] = arc_udptl_rx_alloc ();
			FaxUdptlTx[idx] = arc_udptl_tx_alloc ();
			// 
			arc_udptl_rx_init (FaxUdptlRx[idx]);
			arc_udptl_tx_init (FaxUdptlTx[idx]);

	int             af_type = PF_INET;

			if (gEnableIPv6Support != 0)
			{
				af_type = PF_INET6;
			}

			local_fd = arc_udptl_rx_open (FaxUdptlRx[idx], 1, af_type);
			if (local_fd == -1)
			{
				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
						   FAX_FILE_IO_ERROR, ERR,
						   "Failed to open udptl RX socket.");
				return -1;
			}

			remote_fd = arc_udptl_tx_open (FaxUdptlTx[idx], 1, af_type);
			if (remote_fd == -1)
			{
				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
						   FAX_FILE_IO_ERROR, ERR,
						   " Failed to open udptl TX socket");
				return -1;
			}
			//
			for (i = startport; i < endport; i++)
			{
				arc_udptl_rx_setsockinfo (FaxUdptlRx[idx], gHostIp, i,
										  ARC_UDPTL_LOCAL);
				stat = arc_udptl_rx_bind (FaxUdptlRx[idx]);
				// arc_udptl_tx_setsockinfo(FaxUdptlTx[idx], "127.0.0.1", 12345, ARC_UDPTL_REMOTE);
				// arc_udptl_rx_clone_symmetric(FaxUdptlRx[idx], FaxUdptlTx[idx]);
				if (stat == 0)
				{
					break;
				}
			}

			if (stat == -1)
			{
				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
						   FAX_INITIALIZATION_ERROR, ERR,
						   "Error binding udptl socket on host witn ip address (%s:%d--%d), "
						   "exiting FAX thread", gHostIp, startport, endport);
				arc_udptl_rx_close (FaxUdptlRx[idx]);
				arc_udptl_tx_close (FaxUdptlTx[idx]);
				arc_udptl_rx_free (FaxUdptlRx[idx]);
				arc_udptl_tx_free (FaxUdptlTx[idx]);
				FaxUdptlRx[idx] = NULL;
				FaxUdptlTx[idx] = NULL;
				arc_fax_session_free (FaxSession[idx]);
				FaxSession[idx] = NULL;
				if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_SEND_FAX))
				{
					cleanupFaxResources (zCall, DMOP_SENDFAX, -1);
				}
				else
				{
					cleanupFaxResources (zCall, DMOP_RECVFAX, -1);
				}
				return -1;
			}
			arc_udptl_rx_set_packet_interval (FaxUdptlRx[idx],
											  gT38PacketInterval);
			arc_udptl_tx_set_packet_interval (FaxUdptlTx[idx],
											  gT38PacketInterval);
			//
			FAX_STATE_SET (gCall[zCall].GV_FaxState, SETUP_T38);
		}
		// final leg of fax details 
		if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_T38_REMOTE_DETAILS))
		{
			//fprintf(stderr, " got remote FAX details\n");

			snprintf (T38Parms[idx].remote_ip,
					  sizeof (T38Parms[idx].remote_ip), "%s", "None");
			T38Parms[idx].remote_port = 0;

			// get remote t38 details 
			// ip and port from pic message 
			//
	char           *ptr = NULL;
	char            buff[20];
	int             len;

			// (destIP=10.0.10.11&destFaxPort=11152
			// fprintf(stderr, " %s() fax_details.data=[%s]\n", __func__, fax_details.data);
			ptr = strstr ((char *) fax_details.data, "destIP=");

			if (ptr != NULL)
			{
				ptr += strlen ("destIP=");
				len = strcspn (ptr, "& ");
				if (len && (len < sizeof (T38Parms[idx].remote_ip)))
				{
					snprintf (T38Parms[idx].remote_ip, len + 1, "%s", ptr);
				}
			}

			ptr = strstr ((char *) fax_details.data, "destFaxPort=");

			if (ptr != NULL)
			{
				ptr += strlen ("destFaxPort=");
				len = strcspn (ptr, " \r\n");
				if (len && (len < sizeof (buff)))
				{
					snprintf (buff, len + 1, "%s", ptr);
					T38Parms[idx].remote_port = atoi (buff);
				}
			}

			if ((strcasecmp (T38Parms[idx].remote_ip, "none") == 0)
				&& (T38Parms[idx].remote_port == 0))
			{
				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
						   FAX_TRANSMISSION_FAILURE, ERR,
						   "Recieved empty remote SDP details; unable to continue. Exiting fax thread.");
				if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_SEND_FAX))
				{
					cleanupFaxResources (zCall, DMOP_SENDFAX, -1);
				}
				else
				{
					cleanupFaxResources (zCall, DMOP_RECVFAX, -1);
				}
				return -1;
			}

			arc_udptl_tx_setsockinfo (FaxUdptlTx[idx],
									  T38Parms[idx].remote_ip,
									  T38Parms[idx].remote_port,
									  ARC_UDPTL_REMOTE);
			arc_udptl_rx_setsockinfo (FaxUdptlRx[idx],
									  T38Parms[idx].remote_ip,
									  T38Parms[idx].remote_port,
									  ARC_UDPTL_REMOTE);
			// arc_udptl_rx_setsockinfo(FaxUdptlRx[idx], T38Parms[idx].local_ip, T38Parms[idx].local_port, ARC_UDPTL_LOCAL);
			arc_udptl_tx_clone_symmetric (FaxUdptlTx[idx], FaxUdptlRx[idx]);

			// arc_udptl_tx_clone_symmetric(FaxUdptlTx[idx], FaxUdptlRx[idx]);

			arc_udptl_tx_getsockinfo (FaxUdptlTx[idx], local, sizeof (local),
									  &lport, ARC_UDPTL_LOCAL);
			arc_udptl_tx_getsockinfo (FaxUdptlTx[idx], remote, sizeof (local),
									  &rport, ARC_UDPTL_REMOTE);

			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   FAX_BASE, INFO,
					   "udptl TX socket, local_fd=%d local=%s, "
					   "lport=%d remote_fd=%d remote=%s rport=%d.", local_fd,
					   local, lport, remote_fd, remote, rport);

			arc_udptl_rx_getsockinfo (FaxUdptlRx[idx], local, sizeof (local),
									  &lport, ARC_UDPTL_LOCAL);
			arc_udptl_rx_getsockinfo (FaxUdptlRx[idx], remote, sizeof (local),
									  &rport, ARC_UDPTL_REMOTE);
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   FAX_BASE, INFO,
					   "udptl RX socket, local=%s, lport=%d remote=%s rport=%d",
					   local, lport, remote, rport);

			//
			// set udptl redundnacy parms 
			// arc_udptl_tx_set_ecm (FaxUdptlTx[idx], gT38ErrorCorrectionMode, gT38ErrorCorrectionSpan, gT38ErrorCorrectionEntries);
			arc_udptl_tx_set_ecm (FaxUdptlTx[idx],
								  gCall[zCall].GV_T38ErrorCorrectionMode,
								  gCall[zCall].GV_T38ErrorCorrectionSpan,
								  gCall[zCall].GV_T38ErrorCorrectionEntries);
			if (FaxParms[idx].debug)
			{
				arc_udptl_tx_set_debug (FaxUdptlTx[idx]);
				arc_udptl_rx_set_debug (FaxUdptlRx[idx]);
			}

			FAX_STATE_UNSET (gCall[zCall].GV_FaxState,
							 GOT_T38_REMOTE_DETAILS);
			FAX_STATE_SET (gCall[zCall].GV_FaxState, SET_T38_REMOTE_DETAILS);
		}						// end got remote details 

		if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_SEND_FAX) ||
			FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_RECV_FAX))
		{
			if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, SETUP_T38) &&
				(FAX_STATE_HAS
				 (gCall[zCall].GV_FaxState, SENT_T38_LOCAL_DETAILS) == 0))
			{
				dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
						   FAX_BASE, INFO,
						   "T38 fax session is ready, starting fax thread.");
				arc_udptl_rx_getsockinfo (FaxUdptlRx[idx], local,
										  sizeof (local), &lport,
										  ARC_UDPTL_LOCAL);

				snprintf (T38Parms[idx].local_ip,
						  sizeof (T38Parms[idx].local_ip), "%s", local);
				T38Parms[idx].local_port = lport;

				fax_response.opcode = DMOP_FAX_PROCEED;
				snprintf (fax_response.nameValue,
						  sizeof (fax_response.nameValue),
						  "ip=%s&faxMediaPort=%d", local, lport);
				sendRequestToDynMgr (__LINE__, (char *) __func__,
									 (struct MsgToDM *) &fax_response);
				FAX_STATE_SET (gCall[zCall].GV_FaxState,
							   SENT_T38_LOCAL_DETAILS);
			}
		}
	}							// end t38 

	if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_SEND_FAX) ||
		FAX_STATE_HAS (gCall[zCall].GV_FaxState, GOT_RECV_FAX))
	{
		if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, SETUP_T38) &&
			FAX_STATE_HAS (gCall[zCall].GV_FaxState, SET_T38_REMOTE_DETAILS))
		{
			FAX_STATE_SET (gCall[zCall].GV_FaxState, FAX_READY);
		}
	}

	//fprintf(stderr, " fax state=0x%x\n", gCall[zCall].GV_FaxState);
	// fax is all set now start it 

	if (FAX_STATE_HAS (gCall[zCall].GV_FaxState, FAX_READY))
	{
		FaxSession[idx] = arc_fax_session_alloc ();
		if (FaxSession[idx] == NULL)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
					   FAX_RESOURCE_NOT_AVAILABLE, ERR,
					   "Failed to allocate fax session storage. Returning failure.");
			return -1;
		}

		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   FAX_BASE, INFO,
				   "Fax state is ready. Starting FAX thread.");
		if (gCall[zCall].GV_FaxDebug)
		{
			arc_fax_session_set_debug (&FaxParms[idx]);
		}

		if (FaxParms[idx].use_t38 > 0)
		{
			arc_fax_session_t38_set_tx_handlers (FaxSession[idx],
												 (void *)
												 arc_fax_session_t38_udptl_tx,
												 (void *) FaxUdptlTx[idx]);
			arc_fax_session_t38_set_rx_handlers (FaxSession[idx],
												 (void *)
												 arc_fax_session_t38_udptl_rx,
												 (void *) FaxUdptlRx[idx]);
		}

		rc = arc_fax_session_init (FaxSession[idx], &FaxParms[idx], 0);

		if (FaxParms[idx].use_t38 > 0)
		{
			arc_fax_session_packet_interval (FaxSession[idx],
											 gT38PacketInterval);
			arc_udptl_rx_set_spandsp_cb (FaxUdptlRx[idx], T38_CORE_RX,
										 arc_fax_session_get_t38_state
										 (FaxSession[idx]));
		}

		if (rc != 0)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
					   FAX_INITIALIZATION_ERROR, ERR,
					   "Fax session failed to initialize.  rc=%d", rc);
			arc_fax_session_free (FaxSession[idx]);
			FaxSession[idx] = NULL;
			return -1;
		}

		pthread_attr_init (&attr);
		pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

		gCall[zCall].isSendRecvFaxActive = 1;

		rc = pthread_create (&gCall[zCall].FaxThreadTid, &attr, faxThread,
							 (void *) zCall);
		if (rc != 0)
		{
			dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL,
					   TEL_PTHREAD_ERROR, ERR,
					   "pthread_create() failed. rc=%d. [%s, %d] "
					   "Unable to create fax thread.", rc, errno,
					   strerror (errno));
			// back out 
			arc_fax_session_free (FaxSession[idx]);
			FaxSession[zCall] = NULL;
			memset (&FaxParms[idx], 0, sizeof (arc_fax_session_t));
			gCall[zCall].isSendRecvFaxActive = 0;
		}
		else
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   FAX_BASE, INFO, "Successfully started fax thread.");
			rc = 0;
		}
	}
	// end fax ready

	return rc;
}								// END: startFaxThread()

static int
cleanupFaxResources (int zCall, int dmop, int return_code)
{
	int             rc = 0;
	int             idx;
	struct MsgToApp response;

	if (dmop != -1)
	{
		memset (&response, 0, sizeof (struct MsgToApp));
		memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
		response.opcode = dmop;
		response.returnCode = return_code;
		rc = writeGenericResponseToApp (zCall, &response, (char *) __func__,
										__LINE__);
	}

	idx = zCall % SPAN_WIDTH;

	if (idx > -1 && idx < 48)
	{
		memset (&FaxParms[idx], 0, sizeof (arc_fax_session_t));
		arc_udptl_rx_close (FaxUdptlRx[idx]);
		arc_udptl_tx_close (FaxUdptlTx[idx]);
		arc_udptl_rx_free (FaxUdptlRx[idx]);
		arc_udptl_tx_free (FaxUdptlTx[idx]);
		FaxUdptlRx[idx] = NULL;
		FaxUdptlTx[idx] = NULL;
		arc_fax_session_free (FaxSession[idx]);
		FaxSession[idx] = NULL;
	}

	gCall[zCall].isSendRecvFaxActive = 0;

	return rc;
}

int
stopFaxThread (int zCall)
{

	int             idx;

	if (gCall[zCall].isSendRecvFaxActive == 1)
	{
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
				   FAX_BASE, INFO,
				   "Stopping FAX thread and freeing resources.");
		gCall[zCall].isSendRecvFaxActive = 0;
	}

	return 0;
}

void           *
faxThread (void *args)
{

	char            mod[] = "faxThread";
	int             zCall = (uint64_t) args;
	char            buff[1024];	//  send and recv buff 
	char            rawbuff[1024];	//  decoded audio
	int             bytes_read;
	int             bytes_sent = 0;
	int             bytes_processed;
	int             state;
	int             have_more;
	char           *ptr;
	char            fax_results[256] = "None";
	struct MsgToApp response;
	int             idx = -1;
	int             audio_state;

	int             counter = 500;

	struct timeval  start_time;

	enum state_e
	{
		AUDIO_INIT,
		AUDIO_STARTED,
		T38_STARTED
	};

	if (zCall > -1 && zCall < MAX_PORTS)
	{
		idx = zCall % 48;
	}

	memset (&response, 0, sizeof (response));

	audio_state = AUDIO_INIT;

	usleep (100000);

	while (gCall[zCall].isSendRecvFaxActive == 1)
	{

#if 0
		if (gCall[zCall].isSendRecvFaxActive == 0)
		{
			if (--counter <= 0)
				break;

			usleep (100);
		}
#endif

		if (FaxParms[idx].use_t38 == 0)
		{
			// in this case we need RTP and to decode the audio before processing 
			bytes_read =
				rtp_session_recv_with_ts (gCall[zCall].rtp_session_in, buff,
										  gCall[zCall].codecBufferSize,
										  gCall[zCall].in_user_ts, &have_more,
										  0, NULL);

			if (bytes_read > 0)
			{
				bytes_read =
					arc_decode_buff (__LINE__, &gCall[zCall].decodeAudio[AUDIO_IN],
									 buff, bytes_read, rawbuff,
									 sizeof (rawbuff));
				gCall[zCall].in_user_ts += gCall[zCall].inTsIncrement;
			}
			else
			{
				memset (rawbuff, 0, sizeof (rawbuff));
				bytes_read = 160;
			}

			if (bytes_read > 0)
			{
				bytes_processed =
					arc_fax_session_process_buff (FaxSession[idx], rawbuff,
												  bytes_read * 2, 0);
				if (bytes_processed > 0)
				{
					audio_state = AUDIO_STARTED;
					bytes_sent =
						arc_encode_buff (&gCall[zCall].encodeAudio[AUDIO_IN],
										 rawbuff, bytes_read * 2, buff,
										 sizeof (buff));
					if (bytes_sent > 0)
					{
						bytes_sent =
							arc_rtp_session_send_with_ts (mod, __LINE__,
														  zCall,
														  gCall[zCall].
														  rtp_session, buff,
														  gCall[zCall].
														  codecBufferSize,
														  gCall[zCall].
														  out_user_ts);
					}
				}
			}
			else
			{
				// send silence in the beginning until we get some data to send 
				if (audio_state == AUDIO_INIT)
				{
					bytes_sent =
						arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
													  gCall[zCall].
													  rtp_session,
													  gCall[zCall].
													  silenceBuffer,
													  gCall[zCall].
													  codecBufferSize,
													  gCall[zCall].
													  out_user_ts);
				}
			}
			if (bytes_sent > 0)
			{
				rtpSleep (20, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);
				gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;
				bytes_sent = 0;
			}
			else
			{
				bytes_sent =
					arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
												  gCall[zCall].rtp_session,
												  gCall[zCall].silenceBuffer,
												  gCall[zCall].
												  codecBufferSize,
												  gCall[zCall].out_user_ts);
				rtpSleep (20, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);
			}

		}
		else if (FaxParms[idx].use_t38 == 1)
		{

	char            buff[320] = "";

			// fax session for t38 
			bytes_processed =
				arc_fax_session_process_buff (FaxSession[idx], buff,
											  sizeof (buff), 0);
			//dynVarLog(__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, TEL_BASE, INFO, 
			//         "T38 Fax Bytes Processed=%d", bytes_processed);
		}
		else
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
					   FAX_RESOURCE_NOT_AVAILABLE, ERR,
					   "Fax type is not set, aborting.");
			gCall[zCall].isSendRecvFaxActive = 0;
			break;
		}

		state = arc_fax_session_check_state (FaxSession[idx]);

		if (state)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   FAX_BASE, INFO, "Fax state=%d, exiting fax thread.",
					   state);
			gCall[zCall].isSendRecvFaxActive = 0;
			break;
		}
	}							// end while 

	// destroy fax session 
	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	if (arc_fax_session_get_resultstr
		(FaxSession[zCall], fax_results, sizeof (fax_results)) == 0)
	{

		//dynVarLog(__LINE__, zCall, (char *) __func__, REPORT_NORMAL, FAX_BASE, INFO,
		//      "Fax result=[%s]\n", fax_results);

	char           *ptr;
	int             len;

		ptr = strstr (fax_results, "pages=");
		if (ptr != NULL)
		{
			len = strcspn (ptr, "=\r\n");
			if (len)
			{
				ptr += len + 1;
				len = strcspn (ptr, " \r\n");
				if (len)
				{
					snprintf (response.message, len + 1, "%s", ptr);
				}
			}
		}
	}
	else
	{
		snprintf (response.message, sizeof (response.message), "%s", "-1");
		;;
	}
	arc_fax_session_close (FaxSession[idx]);
	arc_fax_session_free (FaxSession[idx]);
	FaxSession[idx] = NULL;
	//

	switch (FaxParms[idx].type)
	{

	case ARC_FAX_SESSION_INBOUND:
		response.opcode = DMOP_RECVFAX;
		break;
	case ARC_FAX_SESSION_OUTBOUND:
		response.opcode = DMOP_SENDFAX;
		break;
	default:
//		fprintf (stderr, " %s() hit default case statement, lineno=%d\n",
//				 __func__);
		break;
	}

	response.vendorErrorCode = 0;
	response.alternateRetCode = 0;
	// sprintf(response.message, "%d", yMsgFaxComplete.numPagesReceived);

	switch (state)
	{
	case ARC_FAX_SESSION_STATE_START:
		response.returnCode = -1;
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   FAX_TRANSMISSION_FAILURE, ERR,
				   " FAX transmission failure, result codes=[%s]",
				   fax_results);
		break;
	case ARC_FAX_SESSION_T30_AUDIO_START:
		response.returnCode = -1;
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   FAX_TRANSMISSION_FAILURE, ERR,
				   " Audio FAX transmission failure, result codes=[%s]",
				   fax_results);
		break;
	case ARC_FAX_SESSION_T38_START:
		response.returnCode = -1;
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   FAX_TRANSMISSION_FAILURE, ERR,
				   " T38 FAX transmission failure, result codes=[%s]",
				   fax_results);
		break;
	case ARC_FAX_SESSION_OVERALL_TIMEOUT:
		response.returnCode = -1;
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   FAX_TRANSMISSION_FAILURE, ERR,
				   " FAX overall timeout exceeded, result codes=[%s]",
				   fax_results);
		break;
	case ARC_FAX_SESSION_PACKET_TIMEOUT:
		response.returnCode = -1;
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   FAX_TRANSMISSION_FAILURE, ERR,
				   " FAX packet timeout exceeded, result codes=[%s]",
				   fax_results);
		break;
	case ARC_FAX_SESSION_SUCCESS:
		response.returnCode = 0;
		if (response.opcode == DMOP_RECVFAX)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   FAX_BASE, INFO,
					   " Successfully received fax.  rc=%d. [%s]",
					   response.returnCode, fax_results);
		}
		else
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE,
					   FAX_BASE, INFO, " Successfully sent fax.  rc=%d",
					   response.returnCode);
		}
		break;
	case ARC_FAX_SESSION_FAILURE:
		response.returnCode = -1;
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   FAX_TRANSMISSION_FAILURE, ERR,
				   " FAX session failure result codes=[%s]", fax_results);
		break;
	case ARC_FAX_SESSION_END:
		response.returnCode = -1;
		dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
				   FAX_TRANSMISSION_FAILURE, ERR,
				   " Received ARC_FAX_SESSION_END (%d). Fax session failured. rc=%d. [%s]",
				   ARC_FAX_SESSION_END, response.returnCode, fax_results);
		break;
	default:
		response.returnCode = -3;
		break;
	}

	if (FaxUdptlTx[idx] != NULL)
	{
		arc_udptl_tx_close (FaxUdptlTx[idx]);
		arc_udptl_tx_free (FaxUdptlTx[idx]);
		FaxUdptlTx[idx] = NULL;
	}

	if (FaxUdptlRx[idx] != NULL)
	{
		arc_udptl_rx_close (FaxUdptlRx[idx]);
		arc_udptl_rx_free (FaxUdptlRx[idx]);
		FaxUdptlRx[idx] = NULL;
	}

	writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_DIAGNOSE, FAX_BASE,
			   WARN, "Exiting fax thread; fax active flag=%d, last state=%d.",
			   gCall[zCall].isSendRecvFaxActive, state);

	return NULL;
}

int 
restartOutputSession(int zCall)
{
	char            mod[] = "restartOutputSession";
	int             yRc = 0;

	if(gCall[zCall].rtp_session != NULL)
	{
		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session);
		gCall[zCall].rtp_session = NULL;
	}

	if (gCall[zCall].telephonePayloadType < 96
		&& gCall[zCall].telephonePayloadType > -1)
	{
		gCall[zCall].telephonePayloadType = 96;
	}
	else
	if (gCall[zCall].telephonePayloadType == -1)
	{
		gCall[zCall].telephonePayloadType = -99;
	}

	gCall[zCall].rtp_session = rtp_session_new (RTP_SESSION_SENDONLY);

	if (gCall[zCall].rtp_session == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR, ERR,
				   "rtp_session_new() failed for outbound thread. ");
		return -1;
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling rtp_session_set_remote_addr() with rtp_session=%p, remoteRtpIp=%s, remoteRtpPort=%d.",
			   gCall[zCall].rtp_session,
			   gCall[zCall].remoteRtpIp, gCall[zCall].remoteRtpPort);

	yRc = rtp_session_set_remote_addr (gCall[zCall].rtp_session,
									   gCall[zCall].remoteRtpIp,
									   gCall[zCall].remoteRtpPort, gHostIf);


	if (gSymmetricRtp == 1 && gCall[zCall].rtp_session_in != NULL)
	{
		gCall[zCall].outboundSocket = gCall[zCall].rtp_session->rtp.socket;
		gCall[zCall].rtp_session->rtp.socket =
			gCall[zCall].rtp_session_in->rtp.socket;
	}

	rtp_profile_clear_all (&av_profile_array_out[zCall]);

	if (yRc < 0)
	{
		if (gCall[zCall].rtp_session != NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR,
					   ERR,
					   "Failed to set remote address rtp_session for outbound thread. yRc=%d, socket=%d. "
					   "Unable to create outbound thread.", yRc,
					   gCall[zCall].rtp_session->rtp.socket);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
					   "Calling arc_rtp_session_destroy rtp_session socket=%d.",
					   gCall[zCall].rtp_session->rtp.socket);

			arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session);

		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR,
					   ERR,
					   "Failed to set remote address rtp_session for outbound thread; yRc=%d. "
					   "Unable to create outbound thread.", yRc);
		}

		return -1;
	}

	rtp_profile_clear_all (&av_profile_array_out[zCall]);

	switch (gCall[zCall].codecType)
	{
	case 8:

		sprintf (gCall[zCall].audioCodecString, "%s", "711r");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "wav");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "wav");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 8, &pcma8000);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 8);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;
		break;

	case 9:

		sprintf (gCall[zCall].audioCodecString, "%s", "722");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "g722");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "g722");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 9, &g722_8000);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 9);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;

		gCall[zCall].audio_mode = ARC_AUDIO_PROCESS_GAIN_CONTROL;

		ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 0);
		arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_OUT],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[AUDIO_OUT],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);

		//arc_audio_frame_init(&gCall[zCall].audio_frames[AUDIO_OUT], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_OUT]);

		ARC_AUDIO_GAIN_PARMS_INIT (gCall[zCall].audio_gain_parms[AUDIO_OUT],
								   1.0, .25, 2.5, .90, 0);
		ARC_AUDIO_MIX_PARMS_INIT (gCall[zCall].audio_mix_parms[AUDIO_OUT],
								  1.0, 1.0, 1.0);

		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[AUDIO_OUT],
								ARC_AUDIO_PROCESS_GAIN_CONTROL,
								&gCall[zCall].audio_gain_parms[AUDIO_OUT]);

		arc_audio_frame_adjust_gain (&gCall[zCall].audio_frames[AUDIO_OUT],
									 .0, ARC_AUDIO_GAIN_ADJUST_DOWN);

		break;

	case 3:
		sprintf (gCall[zCall].audioCodecString, "%s", "GSM");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "gsm");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "gsm");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 3, &gsm);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 3);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;
		break;

	case 96:
		sprintf (gCall[zCall].audioCodecString, "%s", "AMR");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "amr");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "amr");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 96, &amr);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 96);

		gCall[zCall].codecBufferSize = 14;
		gCall[zCall].codecSleep = 160;

		gCall[zCall].outTsIncrement = 160;
		break;

	case 18:
		if (gCall[zCall].isG729AnnexB == NO)
		{
			sprintf (gCall[zCall].audioCodecString, "%s", "729a");
			sprintf (gCall[zCall].silenceFile, "silence.%s", "g729a");
			sprintf (gCall[zCall].playBackFileExtension, "%s", "g729a");
		}
		else
		{
			sprintf (gCall[zCall].audioCodecString, "%s", "729b");
			sprintf (gCall[zCall].silenceFile, "silence.%s", "g729b");
			sprintf (gCall[zCall].playBackFileExtension, "%s", "g729b");
		}

		rtp_profile_set_payload (&av_profile_array_out[zCall], 18,
								 &g729_8000);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 18);

		gCall[zCall].codecBufferSize = 20;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;
		break;

	case 0:
	default:

		sprintf (gCall[zCall].audioCodecString, "%s", "711r");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "wav");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "wav");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 0, &pcmu8000);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 0);
    	arc_rtp_session_set_ssrc(zCall, gCall[zCall].rtp_session);		// BT-89

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;

		ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 1);

		arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_OUT],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[AUDIO_OUT],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);

		//arc_audio_frame_init(&gCall[zCall].audio_frames[AUDIO_OUT], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_OUT]);

		gCall[zCall].audio_mode = ARC_AUDIO_PROCESS_GAIN_CONTROL;

		ARC_AUDIO_GAIN_PARMS_INIT (gCall[zCall].audio_gain_parms[AUDIO_OUT],
								   1.0, .25, 2.5, .90, 0);
		ARC_AUDIO_MIX_PARMS_INIT (gCall[zCall].audio_mix_parms[AUDIO_OUT],
								  1.0, 1.0, 1.0);

		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[AUDIO_OUT],
								ARC_AUDIO_PROCESS_GAIN_CONTROL,
								&gCall[zCall].audio_gain_parms[AUDIO_OUT]);

	float           current;

		current =
			arc_audio_frame_adjust_gain (&gCall[zCall].
										 audio_frames[AUDIO_OUT], .01,
										 ARC_AUDIO_GAIN_ADJUST_DOWN);
		break;
	}

	gCall[zCall].playBackIncrement =
			gCall[zCall].codecBufferSize * 1000 / gCall[zCall].codecSleep ;

    dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO, 
        "playBackIncrement is set to %d ( %d * 1000 / %d ).",
            gCall[zCall].playBackIncrement, gCall[zCall].codecBufferSize,
            gCall[zCall].codecSleep);


	/*Fill silence buffer */
	if (gCall[zCall].codecType == 18)
	{
	int             yIntSilenceFileFd = -1;

		if (yIntSilenceFileFd != -1)
		{
			read (yIntSilenceFileFd, gCall[zCall].silenceBuffer, 199);
			arc_close (zCall, mod, __LINE__, &yIntSilenceFileFd);
			yIntSilenceFileFd = -1;
		}
		else
		{
	int             i = 0;

	char            yTmpSilenceSequence[10] =
		{ 0x78, 0x52, 0x80, 0xa0, 0x00, 0xfa, 0xd1, 0x00, 0x00, 0x56 };

			memset (gCall[zCall].silenceBuffer, 0,
					sizeof (gCall[zCall].silenceBuffer));

			for (i = 0; i < 20; i++)
			{
				memcpy (gCall[zCall].silenceBuffer + (i * 10),
						yTmpSilenceSequence, 10);
			}
		}
	}
	else if (gCall[zCall].codecType == 0)
	{
		memcpy (gCall[zCall].silenceBuffer, gSilenceBuffer, 200);
	}
	else
	{
		memset (gCall[zCall].silenceBuffer, 0xff, 200);
	}

	if (gCall[zCall].telephonePayloadType > -1)
	{
		rtp_profile_set_payload (&av_profile_array_out[zCall],
								 gCall[zCall].telephonePayloadType,
								 &telephone_event);
	}

	rtp_session_set_profile (gCall[zCall].rtp_session,
							 &av_profile_array_out[zCall]);

	if (gCall[zCall].crossPort > -1)
	{
	int             yAleg = gCall[zCall].crossPort;

		if (gCall[yAleg].rtp_session_in != NULL)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL,
						  "Bridge Call resetting rtp session for call num",
						  "", yAleg);
			//rtp_session_reset(gCall[yAleg].rtp_session_in);
			gCall[yAleg].resetRtpSession = 1;
		}
	}

	//gCall[zCall].out_user_ts  = rtp_session_get_current_send_ts(gCall[zCall].rtp_session);
	gCall[zCall].out_user_ts = 0;

#if 0
	/*Start output thread */
	if (gCall[zCall].outputThreadId == 0)
	{
		yRc = pthread_create (&gCall[zCall].outputThreadId,
							  &gpthread_attr,
							  outputThread, (void *) &(gCall[zCall].msgToDM));
		if (yRc != 0)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
					   TEL_PTHREAD_ERROR, ERR,
					   "pthread_create(%d) failed. rc=%d. [%d, %s] "
					   "Unable to create outbound thread.",
					   gCall[zCall].outputThreadId, yRc, errno,
					   strerror (errno));
			/* Free the thread slot just gotten */
			gCall[zCall].outputThreadId = 0;
		}
	}

#endif

}//restartOutputSession
int
startOutputThread (int zCall)
{
	char            mod[] = "startOutputThread";
	int             yRc = 0;
	char            message[MAX_LOG_MSG_LENGTH];
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;
	int             localFd = -1;

	response.message[0] = '\0';

	if (gCall[zCall].outputThreadId > 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR,
				   ERR,
				   "Failed to process DMOP_RTPDETAILS. Active thread(s) found outputThread(%d).",
				   gCall[zCall].inputThreadId, gCall[zCall].outputThreadId);

		return 0;
	}

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Remote rtp:[%s:<%d>], local rtp:[%d], telephone payload type:[%d], codec Type:[%d].",
			   gCall[zCall].remoteRtpIp,
			   gCall[zCall].remoteRtpPort,
			   gCall[zCall].localSdpPort,
			   gCall[zCall].telephonePayloadType, gCall[zCall].codecType);

	if (gCall[zCall].telephonePayloadType < 96
		&& gCall[zCall].telephonePayloadType > -1)
	{
		gCall[zCall].telephonePayloadType = 96;
	}
	else
	if (gCall[zCall].telephonePayloadType == -1)
	{
		gCall[zCall].telephonePayloadType = -99;
	}

	gCall[zCall].rtp_session = rtp_session_new (RTP_SESSION_SENDONLY);

	if (gCall[zCall].rtp_session == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR, ERR,
				   "rtp_session_new() failed for outbound thread.  Unable to create outbound thread.");
		return 0;
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling rtp_session_set_remote_addr() with rtp_session=%p, remoteRtpIp=%s, remoteRtpPort=%d.",
			   gCall[zCall].rtp_session,
			   gCall[zCall].remoteRtpIp, gCall[zCall].remoteRtpPort);

	yRc = rtp_session_set_remote_addr (gCall[zCall].rtp_session,
									   gCall[zCall].remoteRtpIp,
									   gCall[zCall].remoteRtpPort, gHostIf);

	__DDNDEBUG__ (DEBUG_MODULE_RTP,
				  "Created remote rtp session address for outbound thread.",
				  "", gCall[zCall].rtp_session->rtp.socket);

#if 1
	if (gSymmetricRtp == 1 && gCall[zCall].rtp_session_in != NULL)
	{
		gCall[zCall].outboundSocket = gCall[zCall].rtp_session->rtp.socket;
		gCall[zCall].rtp_session->rtp.socket =
			gCall[zCall].rtp_session_in->rtp.socket;
	}
#endif

	if (yRc < 0)
	{
		if (gCall[zCall].rtp_session != NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR,
					   ERR,
					   "Failed to set remote address rtp_session for outbound thread. yRc=%d, socket=%d. "
					   "Unable to create outbound thread.", yRc,
					   gCall[zCall].rtp_session->rtp.socket);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
					   "Calling arc_rtp_session_destroy rtp_session socket=%d.",
					   gCall[zCall].rtp_session->rtp.socket);

			arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session);

		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR,
					   ERR,
					   "Failed to set remote address rtp_session for outbound thread; yRc=%d. "
					   "Unable to create outbound thread.", yRc);
		}

		return 0;
	}

	rtp_profile_clear_all (&av_profile_array_out[zCall]);

	switch (gCall[zCall].codecType)
	{
	case 8:

		sprintf (gCall[zCall].audioCodecString, "%s", "711r");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "wav");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "wav");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 8, &pcma8000);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 8);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;
		break;

	case 9:

		sprintf (gCall[zCall].audioCodecString, "%s", "722");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "g722");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "g722");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 9, &g722_8000);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 9);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;

		gCall[zCall].audio_mode = ARC_AUDIO_PROCESS_GAIN_CONTROL;

		ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 0);
		arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_OUT],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[AUDIO_OUT],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);

		//arc_audio_frame_init(&gCall[zCall].audio_frames[AUDIO_OUT], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_OUT]);

		ARC_AUDIO_GAIN_PARMS_INIT (gCall[zCall].audio_gain_parms[AUDIO_OUT],
								   1.0, .25, 2.5, .90, 0);
		ARC_AUDIO_MIX_PARMS_INIT (gCall[zCall].audio_mix_parms[AUDIO_OUT],
								  1.0, 1.0, 1.0);

		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[AUDIO_OUT],
								ARC_AUDIO_PROCESS_GAIN_CONTROL,
								&gCall[zCall].audio_gain_parms[AUDIO_OUT]);

		arc_audio_frame_adjust_gain (&gCall[zCall].audio_frames[AUDIO_OUT],
									 .0, ARC_AUDIO_GAIN_ADJUST_DOWN);

		break;

	case 3:
		sprintf (gCall[zCall].audioCodecString, "%s", "GSM");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "gsm");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "gsm");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 3, &gsm);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 3);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;
		break;

	case 96:
		sprintf (gCall[zCall].audioCodecString, "%s", "AMR");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "amr");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "amr");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 96, &amr);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 96);

		gCall[zCall].codecBufferSize = 14;
		gCall[zCall].codecSleep = 160;

		gCall[zCall].outTsIncrement = 160;
		break;

	case 18:
		if (gCall[zCall].isG729AnnexB == NO)
		{
			sprintf (gCall[zCall].audioCodecString, "%s", "729a");
			sprintf (gCall[zCall].silenceFile, "silence.%s", "g729a");
			sprintf (gCall[zCall].playBackFileExtension, "%s", "g729a");
		}
		else
		{
			sprintf (gCall[zCall].audioCodecString, "%s", "729b");
			sprintf (gCall[zCall].silenceFile, "silence.%s", "g729b");
			sprintf (gCall[zCall].playBackFileExtension, "%s", "g729b");
		}

		rtp_profile_set_payload (&av_profile_array_out[zCall], 18,
								 &g729_8000);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 18);

		gCall[zCall].codecBufferSize = 20;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;
		break;

	case 0:
	default:

		sprintf (gCall[zCall].audioCodecString, "%s", "711r");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "wav");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "wav");

		rtp_profile_set_payload (&av_profile_array_out[zCall], 0, &pcmu8000);
		rtp_session_set_payload_type (gCall[zCall].rtp_session, 0);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].outTsIncrement = 160;

		ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 1);

		arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_OUT],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[AUDIO_OUT],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);

		//arc_audio_frame_init(&gCall[zCall].audio_frames[AUDIO_OUT], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_OUT]);

		gCall[zCall].audio_mode = ARC_AUDIO_PROCESS_GAIN_CONTROL;

		ARC_AUDIO_GAIN_PARMS_INIT (gCall[zCall].audio_gain_parms[AUDIO_OUT],
								   1.0, .25, 2.5, .90, 0);
		ARC_AUDIO_MIX_PARMS_INIT (gCall[zCall].audio_mix_parms[AUDIO_OUT],
								  1.0, 1.0, 1.0);

		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[AUDIO_OUT],
								ARC_AUDIO_PROCESS_GAIN_CONTROL,
								&gCall[zCall].audio_gain_parms[AUDIO_OUT]);

	float           current;

		current =
			arc_audio_frame_adjust_gain (&gCall[zCall].
										 audio_frames[AUDIO_OUT], .01,
										 ARC_AUDIO_GAIN_ADJUST_DOWN);
		break;
	}
	gCall[zCall].playBackIncrement =
			gCall[zCall].codecBufferSize * 1000 / gCall[zCall].codecSleep ;
    dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO, 
        "playBackIncrement is set to %d ( %d * 1000 / %d ).",
            gCall[zCall].playBackIncrement, gCall[zCall].codecBufferSize,
            gCall[zCall].codecSleep);


	/*Fill silence buffer */
	if (gCall[zCall].codecType == 18)
	{
	int             yIntSilenceFileFd = -1;

		if (yIntSilenceFileFd != -1)
		{
			read (yIntSilenceFileFd, gCall[zCall].silenceBuffer, 199);
			arc_close (zCall, mod, __LINE__, &yIntSilenceFileFd);
			yIntSilenceFileFd = -1;
		}
		else
		{
	int             i = 0;

	char            yTmpSilenceSequence[10] =
		{ 0x78, 0x52, 0x80, 0xa0, 0x00, 0xfa, 0xd1, 0x00, 0x00, 0x56 };

			memset (gCall[zCall].silenceBuffer, 0,
					sizeof (gCall[zCall].silenceBuffer));

			for (i = 0; i < 20; i++)
			{
				memcpy (gCall[zCall].silenceBuffer + (i * 10),
						yTmpSilenceSequence, 10);
			}
		}
	}
	else if (gCall[zCall].codecType == 0)
	{
		memcpy (gCall[zCall].silenceBuffer, gSilenceBuffer, 200);
	}
	else
	{
		memset (gCall[zCall].silenceBuffer, 0xff, 200);
	}

	if (gCall[zCall].telephonePayloadType > -1)
	{
		rtp_profile_set_payload (&av_profile_array_out[zCall],
								 gCall[zCall].telephonePayloadType,
								 &telephone_event);
	}

	rtp_session_set_profile (gCall[zCall].rtp_session,
							 &av_profile_array_out[zCall]);

	if (gCall[zCall].crossPort > -1)
	{
	int             yAleg = gCall[zCall].crossPort;

		if (gCall[yAleg].rtp_session_in != NULL)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL,
						  "Bridge Call resetting rtp session for call num",
						  "", yAleg);
			//rtp_session_reset(gCall[yAleg].rtp_session_in);
			gCall[yAleg].resetRtpSession = 1;
		}
	}

	//gCall[zCall].out_user_ts  = rtp_session_get_current_send_ts(gCall[zCall].rtp_session);
	gCall[zCall].out_user_ts = 0;

	/*Start output thread */
	if (gCall[zCall].outputThreadId == 0)
	{
		yRc = pthread_create (&gCall[zCall].outputThreadId,
							  &gpthread_attr,
							  outputThread, (void *) &(gCall[zCall].msgToDM));
		if (yRc != 0)
		{
			dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_NORMAL,
					   TEL_PTHREAD_ERROR, ERR,
					   "pthread_create(%d) failed. rc=%d. [%d, %s] "
					   "Unable to create outbound thread.",
					   gCall[zCall].outputThreadId, yRc, errno,
					   strerror (errno));
			/* Free the thread slot just gotten */
			gCall[zCall].outputThreadId = 0;
		}
	}

	return yRc;

}/*END: int startOutputThread */

int
startInputThread (int zCall)
{
	char            mod[] = "startInputThread";
	int             yRc = 0;
	char            message[MAX_LOG_MSG_LENGTH];
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;

	response.message[0] = '\0';

	gCall[zCall].lastStopActivityAppRef = -1;

	if (gCall[zCall].inputThreadId > 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR,
				   ERR,
				   "Failed to process DMOP_RTPDETAILS. Active thread(s) found inputThread(%d), outputThreadId(%d).",
				   gCall[zCall].inputThreadId, gCall[zCall].outputThreadId);

		return -1;
	}

	//gCall[zCall].localSdpPort = LOCAL_STARTING_RTP_PORT +(zCall*2);

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Remote rtp:[%s:%d] local rtp:[%d] telephone payload type:[%d] codec Type:[%d].",
			   gCall[zCall].remoteRtpIp,
			   gCall[zCall].remoteRtpPort,
			   gCall[zCall].localSdpPort,
			   gCall[zCall].telephonePayloadType, gCall[zCall].codecType);

	if (gCall[zCall].telephonePayloadType < 96
		&& gCall[zCall].telephonePayloadType > -1)
	{
		gCall[zCall].telephonePayloadType = 96;
	}
	else
	if (gCall[zCall].telephonePayloadType == -1)
	{
		gCall[zCall].telephonePayloadType = -99;
	}

	if (gCall[zCall].inputThreadId > 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR,
				   ERR,
				   "Failed to process DMOP_RTPDETAILS. Active thread(s) found inputThread(%d), outputThreadId(%d).",
				   gCall[zCall].inputThreadId, gCall[zCall].outputThreadId);

		return -1;
	}

#if 0
	if (gCall[zCall].rtp_session_in != NULL)
	{
		rtp_session_destroy (gCall[zCall].rtp_session_in);
		gCall[zCall].rtp_session_in = NULL;
		gCall[zCall].in_user_ts = 0;
	}
#endif

	if (gCall[zCall].rtp_session_in == NULL)
	{
		dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE,
				   TEL_RTP_ERROR, WARN,
				   "rtp_session_in is NULL; creating a new rtp_session_in.");

		gCall[zCall].rtp_session_in = rtp_session_new (RTP_SESSION_RECVONLY);

		if (gCall[zCall].rtp_session_in == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR,
					   ERR,
					   "rtp_session_new() failed for inbound thread.  Unable to create inbound thread.");

			if (gCall[zCall].rtp_session != NULL)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Calling arc_rtp_session_destroy rtp_session(); socket=%d.",
						   gCall[zCall].rtp_session->rtp.socket);

				arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session);
			}

			return -1;
		}

		dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE,
				   INFO,
				   "Calling rtp_session_set_local_addr() with localSdpPort=%d.",
				   gCall[zCall].localSdpPort);
		yRc =
			rtp_session_set_local_addr (gCall[zCall].rtp_session_in, gHostIp,
										gCall[zCall].localSdpPort, gHostIf);

		if (yRc == -1)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR,
					   ERR, "Failed to set local address to %d.",
					   gCall[zCall].localSdpPort);

			if (gCall[zCall].rtp_session != NULL)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Calling arc_rtp_session_destroy(); rtp_session socket=%d.",
						   gCall[zCall].rtp_session->rtp.socket);

				arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session);
			}

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Calling arc_rtp_session_destroy with rtp_session_in; socket=%d.",
					   gCall[zCall].rtp_session_in->rtp.socket);

			rtp_session_destroy (gCall[zCall].rtp_session_in);	/*Destroy all the way */

			gCall[zCall].rtp_session_in = NULL;

			return -1;
		}

		rtp_session_signal_connect (gCall[zCall].rtp_session_in,
									"telephone-event_packet",
									(RtpCallback) recv_tevp_cb,
									&(gCall[zCall].callNum));

#if 1
		/*Callback for timestamp jump */
		rtp_session_signal_connect (gCall[zCall].rtp_session_in,
									"timestamp_jump",
									(RtpCallback) recv_tsjump_cb,
									&(gCall[zCall].callNum));

		 rtp_session_signal_connect(gCall[zCall].rtp_session_in,
					"ssrc_changed",
					(RtpCallback)rtp_session_reset,
					NULL);
#endif

	}

	rtp_profile_clear_all (&av_profile_array_in[zCall]);

	switch (gCall[zCall].codecType)
	{
	case 8:

		sprintf (gCall[zCall].audioCodecString, "%s", "711r");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "wav");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "wav");

		rtp_profile_set_payload (&av_profile_array_in[zCall], 8, &pcma8000);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].inTsIncrement = 160;
		gCall[zCall].outTsIncrement = 160;

		ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 0);
		arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_IN],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[AUDIO_IN],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		//arc_audio_frame_init(&gCall[zCall].audio_frames[AUDIO_IN], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_IN]);
		// mixed audio frames 
		arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_MIXED],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[AUDIO_MIXED],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		//arc_audio_frame_init(&gCall[zCall].audio_frames[AUDIO_MIXED], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_MIXED]);
		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[AUDIO_MIXED],
								ARC_AUDIO_PROCESS_AUDIO_MIX,
								&gCall[zCall].audio_mix_parms[AUDIO_OUT]);
		// conferencing audio
		// add audio options for mixing and silince

		arc_decoder_init (&gCall[zCall].decodeAudio[CONFERENCE_AUDIO],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[CONFERENCE_AUDIO],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);

		//arc_audio_frame_init(&gCall[zCall].audio_frames[CONFERENCE_AUDIO], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 0);
		arc_audio_frame_reset (&gCall[zCall].audio_frames[CONFERENCE_AUDIO]);

		ARC_AUDIO_MIX_PARMS_INIT (gCall[zCall].
								  audio_mix_parms[CONFERENCE_AUDIO], 1.0, .25,
								  2.5);
		ARC_AUDIO_SILENCE_PARMS_INIT (gCall[zCall].sgen[CONFERENCE_AUDIO],
									  320);

		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[CONFERENCE_AUDIO],
								ARC_AUDIO_PROCESS_AUDIO_MIX,
								&gCall[zCall].
								audio_mix_parms[CONFERENCE_AUDIO]);
		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[CONFERENCE_AUDIO],
								ARC_AUDIO_PROCESS_GEN_SILENCE,
								&gCall[zCall].sgen[CONFERENCE_AUDIO]);

		// end conference audio

		break;

	case 3:
		sprintf (gCall[zCall].audioCodecString, "%s", "GSM");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "gsm");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "gsm");

		rtp_profile_set_payload (&av_profile_array_in[zCall], 3, &gsm);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].inTsIncrement = 160;
		gCall[zCall].outTsIncrement = 160;

		break;

	case 96:
		sprintf (gCall[zCall].audioCodecString, "%s", "AMR");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "amr");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "amr");

		rtp_profile_set_payload (&av_profile_array_in[zCall], 96, &amr);

		gCall[zCall].codecBufferSize = 14;
		gCall[zCall].codecSleep = 160;

		gCall[zCall].inTsIncrement = 160;
		gCall[zCall].outTsIncrement = 160;

		break;

	case 18:
		if (gCall[zCall].isG729AnnexB == NO)
		{
			sprintf (gCall[zCall].audioCodecString, "%s", "729a");
			sprintf (gCall[zCall].silenceFile, "silence.%s", "729a");
			sprintf (gCall[zCall].playBackFileExtension, "%s", "g729a");
		}
		else
		{
			sprintf (gCall[zCall].audioCodecString, "%s", "729b");
			sprintf (gCall[zCall].silenceFile, "silence.%s", "729b");
			sprintf (gCall[zCall].playBackFileExtension, "%s", "g729b");
		}

		rtp_profile_set_payload (&av_profile_array_in[zCall], 18, &g729_8000);

		rtp_session_set_payload_type (gCall[zCall].rtp_session_in, 18);

		gCall[zCall].codecBufferSize = 20;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].inTsIncrement = 160;
		gCall[zCall].outTsIncrement = 160;

		break;

	case 0:
	default:
		sprintf (gCall[zCall].audioCodecString, "%s", "711r");
		sprintf (gCall[zCall].silenceFile, "silence.%s", "wav");
		sprintf (gCall[zCall].playBackFileExtension, "%s", "wav");

		rtp_profile_set_payload (&av_profile_array_in[zCall], 0, &pcmu8000);

		gCall[zCall].codecBufferSize = 160;
		gCall[zCall].codecSleep = 20;

		gCall[zCall].inTsIncrement = 160;
		gCall[zCall].outTsIncrement = 160;

		ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 1);

		arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_IN],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[AUDIO_IN],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		//arc_audio_frame_init(&gCall[zCall].audio_frames[AUDIO_IN], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 1); 
		arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_IN]);
		// mixed audio frames
		arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_MIXED],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[AUDIO_MIXED],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		//arc_audio_frame_init(&gCall[zCall].audio_frames[AUDIO_MIXED], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_reset (&gCall[zCall].audio_frames[AUDIO_MIXED]);
		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[AUDIO_MIXED],
								ARC_AUDIO_PROCESS_AUDIO_MIX,
								&gCall[zCall].audio_mix_parms[AUDIO_OUT]);

		// conferencing audio
		// add audio options for mixing and silince

		arc_decoder_init (&gCall[zCall].decodeAudio[CONFERENCE_AUDIO],
						  ARC_DECODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);
		arc_encoder_init (&gCall[zCall].encodeAudio[CONFERENCE_AUDIO],
						  ARC_ENCODE_G711, &gCall[zCall].g711parms,
						  sizeof (gCall[zCall].g711parms), 1);

		//arc_audio_frame_init(&gCall[zCall].audio_frames[CONFERENCE_AUDIO], gCall[zCall].codecBufferSize * 2, ARC_AUDIO_FRAME_SLIN_16, 0);
		arc_audio_frame_reset (&gCall[zCall].audio_frames[CONFERENCE_AUDIO]);

		ARC_AUDIO_MIX_PARMS_INIT (gCall[zCall].
								  audio_mix_parms[CONFERENCE_AUDIO], 1.0, .25,
								  2.5);
		ARC_AUDIO_SILENCE_PARMS_INIT (gCall[zCall].sgen[CONFERENCE_AUDIO],
									  320);

		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[CONFERENCE_AUDIO],
								ARC_AUDIO_PROCESS_AUDIO_MIX,
								&gCall[zCall].
								audio_mix_parms[CONFERENCE_AUDIO]);
		arc_audio_frame_add_cb (&gCall[zCall].audio_frames[CONFERENCE_AUDIO],
								ARC_AUDIO_PROCESS_GEN_SILENCE,
								&gCall[zCall].sgen[CONFERENCE_AUDIO]);

		// end conference audio

		break;
	}
	gCall[zCall].playBackIncrement =
			gCall[zCall].codecBufferSize * 1000 / gCall[zCall].codecSleep ;
    dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
        "playBackIncrement is set to %d ( %d * 1000 / %d ).",
            gCall[zCall].playBackIncrement, gCall[zCall].codecBufferSize,
            gCall[zCall].codecSleep);


	/*Fill silence buffer */
	if (gCall[zCall].codecType == 18)
	{

	int             yIntSilenceFileFd = -1;

		if (yIntSilenceFileFd != -1)
		{
			read (yIntSilenceFileFd, gCall[zCall].silenceBuffer, 160);
			arc_close (zCall, mod, __LINE__, &yIntSilenceFileFd);
			yIntSilenceFileFd = -1;
		}
		else
		{
	int             i = 0;

	char            yTmpSilenceSequence[10] =
		{ 0x78, 0x52, 0x80, 0xa0, 0x00, 0xfa, 0xd1, 0x00, 0x00, 0x56 };

			memset (gCall[zCall].silenceBuffer, 0,
					sizeof (gCall[zCall].silenceBuffer));

			for (i = 0; i < 16; i++)
			{
				memcpy (gCall[zCall].silenceBuffer + (i * 10),
						yTmpSilenceSequence, 10);
			}
		}
	}
	else if (gCall[zCall].codecType == 0)
	{
		memcpy (gCall[zCall].silenceBuffer, gSilenceBuffer, 160);
	}
	else
	{
		memset (gCall[zCall].silenceBuffer, 0xff, 160);
	}

	if (gCall[zCall].telephonePayloadType > -1)
	{
		rtp_profile_set_payload (&av_profile_array_in[zCall],
								 gCall[zCall].telephonePayloadType,
								 &telephone_event);
	}

	rtp_session_set_profile (gCall[zCall].rtp_session_in,
							 &av_profile_array_in[zCall]);

	if (gCall[zCall].crossPort > -1)
	{
	int             yAleg = gCall[zCall].crossPort;

		if (gCall[yAleg].rtp_session_in != NULL)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL,
						  "Bridge Call resetting rtp session for call num",
						  "", yAleg);
			//rtp_session_reset(gCall[yAleg].rtp_session_in);
			gCall[yAleg].resetRtpSession = 1;
		}
	}

//  gCall[zCall].in_user_ts     = rtp_session_get_current_recv_ts(gCall[zCall].rtp_session_in);
	gCall[zCall].in_user_ts = 0;

	// MR 4602
	if (gCall[zCall].leg == LEG_B && gCall[zCall].crossPort >= 0)
	{
		int   yALeg = gCall[zCall].crossPort;
		gCall[zCall].GV_CallProgress = gCall[yALeg].GV_CallProgress;
	}
	// END: MR 4602

	if (gCall[zCall].GV_CallProgress != 0)
	{

	int             toneDetectionFlags =
		ARC_TONES_FAST_BUSY | ARC_TONES_HUMAN | ARC_TONES_FAX_TONE | ARC_TONES_FAX_CED | ARC_TONES_SIT_TONE;

		gCall[zCall].sendCallProgressAudio = 1;

		if (gCall[zCall].codecType == 0 || gCall[zCall].codecType == 8)
		{
			if (gCall[zCall].codecType == 8)
			{
				ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 0);
			}
			else if (gCall[zCall].codecType == 0)
			{
				ARC_G711_PARMS_INIT (gCall[zCall].g711parms, 1);
			}

			arc_decoder_init (&gCall[zCall].decodeAudio[AUDIO_IN],
							  ARC_DECODE_G711, &gCall[zCall].g711parms,
							  (int) sizeof (gCall[zCall].g711parms), 1);

			gCall[zCall].resetRtpSession = 1;

			startInbandToneDetectionThread (zCall, __LINE__, toneDetectionFlags);

			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, INFO,
					   "Started inbandToneDetectionThread.");
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
					   "Directed to use inband tones detection, but codec does not match 0 or 8 [ulaw or alaw]codecType=%d.",
					   gCall[zCall].codecType);
		}
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CallProgress not set.");
	}

	/*Start input thread */
	if (gCall[zCall].inputThreadId == 0)
	{
	int             rc = -1;

		rc = pthread_create (&gCall[zCall].inputThreadId,
							 &gpthread_attr,
							 inputThread, (void *) &(gCall[zCall].msgToDM));
		if (rc != 0)
		{
			gCall[zCall].inputThreadId = 0;
		}
	}

	return yRc;

}/*END: int startInputThread */

int
process_DMOP_RESTART_OUTPUT_THREAD(int zCall)
{
	int		yRc = 0;
	char	mod[] = { "process_DMOP_RESTART_OUTPUT_THREAD" };

	char            message[MAX_LOG_MSG_LENGTH];

	int yIntRemoteRtpPort = 0;		// BT-89
	char	yStrRemoteRtpIp[50];

	struct MsgToDM  msg = gCall[zCall].msgToDM;

	sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
			 msg.opcode, msg.appCallNum,
			 msg.appPid, msg.appRef, msg.appPassword);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing %s.", message);

	// BT-89
	sscanf (gCall[zCall].msgToDM.data,
			"%d^%d^%d^%d^%d^%d^%s",
			&(gCall[zCall].leg),
			&(yIntRemoteRtpPort),
			&(gCall[zCall].localSdpPort),
			&(gCall[zCall].telephonePayloadType),
			&(gCall[zCall].full_duplex),
			&(gCall[zCall].codecType),
			yStrRemoteRtpIp);

	if(yIntRemoteRtpPort == gCall[zCall].remoteRtpPort 
		&& !strcmp(yStrRemoteRtpIp, gCall[zCall].remoteRtpIp))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "BT-89 Keeping old output rtp session as remote IP and Port are same.");

		return 0;
	}

	gCall[zCall].remoteRtpPort = yIntRemoteRtpPort;
	sprintf(gCall[zCall].remoteRtpIp, "%s", yStrRemoteRtpIp);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Commencing the steps to restart output rtp session.");

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Got data=%s, gCall[zCall].full_duplex=%d, gCall[zCall].isG729AnnexB=%d, YES=%d, NO=%d.",
			   gCall[zCall].msgToDM.data, gCall[zCall].full_duplex,
			   gCall[zCall].isG729AnnexB, YES, NO);

	if (gCall[zCall].telephonePayloadType == -1)
	{
		gCall[zCall].telephonePayloadType = -99;
	}

	//Set the flag, so the output rtp session isn restartted
	gCall[zCall].restart_rtp_session = 1;

	return (yRc);

}/* process_DMOP_RESTART_OUTPUT_THREAD*/

///ArcSipCallMgr sends this message once it has media details available
int
process_DMOP_RTPDETAILS (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_RTPDETAILS" };
	char            message[MAX_LOG_MSG_LENGTH];
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;

	response.message[0] = '\0';

	gCall[zCall].yDropTime = 0;

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearSpeakList", "", -1);
	clearSpeakList (zCall, __LINE__);

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearAppRequestList", "", -1);

	clearAppRequestList (zCall);

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Crossport", "", gCall[zCall].crossPort);

	sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
			 msg.opcode, msg.appCallNum,
			 msg.appPid, msg.appRef, msg.appPassword);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing %s.", message);

	sscanf (gCall[zCall].msgToDM.data,
			"%d^%d^%d^%d^%d^%d^%s",
			&(gCall[zCall].leg),
			&(gCall[zCall].remoteRtpPort),
			&(gCall[zCall].localSdpPort),
			&(gCall[zCall].telephonePayloadType),
			&(gCall[zCall].full_duplex),
			&(gCall[zCall].codecType), gCall[zCall].remoteRtpIp);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Got data=%s, gCall[zCall].full_duplex=%d, gCall[zCall].isG729AnnexB=%d, YES=%d, NO=%d.",
			   gCall[zCall].msgToDM.data, gCall[zCall].full_duplex,
			   gCall[zCall].isG729AnnexB, YES, NO);

	if (gCall[zCall].telephonePayloadType == -1)
	{
		gCall[zCall].telephonePayloadType = -99;
	}
#ifdef  ACU_LINUX

/*
	Send a message to ArcSipToneClient so that it will start listening audio on RTP port
*/
	char            yStrFileCallProgressInfo[256];

	sprintf (yStrFileCallProgressInfo, "%s/callProgressInfo.%d",
			 gCacheDirectory, zCall);

	if (access (yStrFileCallProgressInfo, F_OK) == 0)
	{
	struct Msg_CallProgress yMsgCallProgress;

		yMsgCallProgress.opcode = DMOP_START_CALL_PROGRESS;
		yMsgCallProgress.origOpcode = DMOP_INITIATECALL;
		yMsgCallProgress.appCallNum = zCall;
		yMsgCallProgress.appPid = msg.appPid;
		yMsgCallProgress.appRef = msg.appRef;
		yMsgCallProgress.appPassword = msg.appPassword;

		sprintf (yMsgCallProgress.nameValue, "codec=%d&payload=%d",
				 gCall[zCall].codecType, gCall[zCall].telephonePayloadType);

		unlink (yStrFileCallProgressInfo);

		yRc =
			sendRequestToSTonesClient (zCall, mod,
									   (struct MsgToDM *) &yMsgCallProgress);

		if (yRc >= 0)
		{
			/*
			 *  Do nothing.    
			 *  As soon as DMOP_START_CALL_PROGRESS_RESPONSE is received, 
			 *  RTP data will be pumped to Tones Client from input thread. 
			 */
		}
		else
		{
			/*If can not connect to tones client, send human answer back to app. */
	struct MsgToApp response;

			response.opcode = DMOP_INITIATECALL;
			response.appCallNum = zCall;
			response.appPid = msg.appPid;
			response.appRef = msg.appRef;
			response.appPassword = msg.appPassword;
			sprintf (response.message, "%d", 152);

			writeGenericResponseToApp (zCall, &response, mod, __LINE__);

		}
	}

#endif

	switch (gCall[zCall].full_duplex)
	{
	case SENDONLY:
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling startOutputThread().");
		yRc = startOutputThread (zCall);
#ifdef ACU_LINUX
		//   *Aculab start call progress
		// Make call to start aculab thread 
		// configure/connect the rtp to TiNG and begin listening
#endif
		break;
	case RECVONLY:
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling startOutputThread().");
		yRc = startInputThread (zCall);
#ifdef ACU_LINUX
		//   *Aculab start call progress
		// Make call to start aculab thread 
		// configure/connect the rtp to TiNG and begin listening
#endif
		break;
	case INACTIVE:
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Got INACTIVE media.");
		return 0;
		break;
	case SENDRECV:
	default:
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling startInputThread() and startOutputThread().");

		yRc = startInputThread (zCall);
		if (yRc == 0)
		{
			yRc = startOutputThread (zCall);
		}
		break;
	}

	return yRc;

}								/*END: int process_DMOP_RTPDETAILS */

int
process_DMOP_FAX_PROCEED (int zCall)
{
	int             yRc = 0;
	int             yResponseRetCode = 0;
	char            mod[] = "process_DMOP_FAX_PROCEED";
	char            message[MAX_LOG_MSG_LENGTH];
	struct Msg_Fax_Proceed yMsgFaxProceed;
	char            srBuf[64];

	memcpy (&yMsgFaxProceed, &gCall[zCall].msgToDM,
			sizeof (struct Msg_Fax_Proceed));

	sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,nameVal:%s}",
			 yMsgFaxProceed.opcode, yMsgFaxProceed.appCallNum,
			 yMsgFaxProceed.appPid, yMsgFaxProceed.appRef,
			 yMsgFaxProceed.appPassword, yMsgFaxProceed.nameValue);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
			   "Processing %s.", message);

	if (yMsgFaxProceed.sendRecvOp == DMOP_RECVFAX)
	{
		sprintf (srBuf, "%s", "receive");
	}
	else
	{
		sprintf (srBuf, "%s", "send");
	}

	gCall[zCall].faxServerSpecialFifo[0] = 0;

#if 0
	sscanf (yMsgFaxProceed.nameValue,
			"faxMediaPort=%d", &(gCall[zCall].faxRtpPort));
#endif

	sscanf (yMsgFaxProceed.nameValue,
			"specialFifo=%[^&]&faxMediaPort=%d",
			gCall[zCall].faxServerSpecialFifo, &(gCall[zCall].faxRtpPort));

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
			   "Got nameValue=%s, faxRtpPort=%d, faxServerSpecialFifo=%s.",
			   yMsgFaxProceed.nameValue, gCall[zCall].faxRtpPort,
			   gCall[zCall].faxServerSpecialFifo);

	if (yResponseRetCode != 0)
	{
	struct MsgToApp response;

		memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
		response.returnCode = yResponseRetCode;
		response.vendorErrorCode = 0;
		response.alternateRetCode = 0;

		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, FAX_GENERAL_FAILURE,
				   WARN, "Fax %s returned failure (-1).", srBuf);

		sprintf (response.message, "%d", zCall);

		writeGenericResponseToApp (zCall, &response, mod, __LINE__);

		return (0);
	}

	if (yMsgFaxProceed.sendRecvOp == DMOP_RECVFAX)
	{
		// signal far end to to send the fax to the IP/port
	}

	return (0);

}								// END: process_DMOP_FAX_PROCEED()

int
process_DMOP_FAX_COMPLETE (int zCall)
{
	int             yRc = 0;
	int             yResponseRetCode = -1;
	char            mod[] = "process_DMOP_FAX_COMPLETE";
	char            message[MAX_LOG_MSG_LENGTH] = "";
	struct Msg_Fax_Complete yMsgFaxComplete;
	char            srBuf[64] = "";
	struct MsgToApp response;

	memcpy (&yMsgFaxComplete, &gCall[zCall].msgToDM,
			sizeof (struct Msg_Fax_Complete));

	sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,rc:%d}",
			 yMsgFaxComplete.opcode, yMsgFaxComplete.appCallNum,
			 yMsgFaxComplete.appPid, yMsgFaxComplete.appRef,
			 yMsgFaxComplete.appPassword, yMsgFaxComplete.rc);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
			   "Processing %s.", message);

	gCall[zCall].faxServerSpecialFifo[0] = 0;

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	if (yMsgFaxComplete.sendRecvOp == DMOP_RECVFAX)
	{
		response.opcode = DMOP_RECVFAX;
		sprintf (srBuf, "%s", "receive");
	}
	else
	{
		response.opcode = DMOP_SENDFAX;
		sprintf (srBuf, "%s", "send");
	}

	response.returnCode = yMsgFaxComplete.rc;
	response.vendorErrorCode = 0;
	response.alternateRetCode = 0;
	sprintf (response.message, "%d", yMsgFaxComplete.numPagesReceived);

	writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	return (0);

}								// END: process_DMOP_FAX_COMPLETE()

/*
 *	This routine will, based on information sent by tones client, 
 *	create an RTP session to send incoming data to tones client 
 */
int
process_DMOP_START_CALL_PROGRESS_RESPONSE (int zCall)
{
	int             yRc = 0;
	int             yResponseRetCode = -1;
	char            mod[] = { "process_DMOP_START_CALL_PROGRESS_RESPONSE" };
	char            message[MAX_LOG_MSG_LENGTH];
	struct Msg_CallProgress yMsgCallProgress;
	memcpy (&yMsgCallProgress, &gCall[zCall].msgToDM,
			sizeof (struct Msg_CallProgress));

	sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,nameVal:%s}",
			 yMsgCallProgress.opcode, yMsgCallProgress.appCallNum,
			 yMsgCallProgress.appPid, yMsgCallProgress.appRef,
			 yMsgCallProgress.appPassword, yMsgCallProgress.nameValue);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
			   "Processing %s.", message);

	gCall[zCall].toneServerSpecialFifo[0] = 0;

	sscanf (yMsgCallProgress.nameValue,
			"specialFifo=%[^&]&IP=%[^&]&rtpPort=%d&rc=%d",
			gCall[zCall].toneServerSpecialFifo,
			gCall[zCall].toneServer,
			&(gCall[zCall].toneRtpPort), &yResponseRetCode);

	if (gCall[zCall].rtp_session_tone != NULL)
	{
		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_tone);
	}

	gCall[zCall].rtp_session_tone = NULL;
	gCall[zCall].toneTs = 0;

	if (yResponseRetCode != 0)
	{
	struct MsgToApp response;

		memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Tone server returned failure (-1). Not doing call progress.");

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Sending call progress retcode(%d) to app.", 21);

		response.opcode = DMOP_INITIATECALL;
		response.returnCode = 21;
		sprintf (response.message, "%d", zCall);

		writeGenericResponseToApp (zCall, &response, mod, __LINE__);

		return (0);
	}

	gCall[zCall].rtp_session_tone = rtp_session_new (RTP_SESSION_SENDONLY);
	//rtp_session_set_scheduling_mode(gCall[zCall].rtp_session_tone, 0);
	rtp_session_set_blocking_mode (gCall[zCall].rtp_session_tone, 0);
	//rtp_session_set_ssrc (gCall[zCall].rtp_session_tone, atoi("3197096732") + zCall); //ssrcDelta++ );
	//rtp_session_set_ssrc (gCall[zCall].rtp_session_tone, atoi("3197096732") + ssrcDelta++ );
	rtp_session_set_ssrc (gCall[zCall].rtp_session_tone, atoi ("3197096732"));
	rtp_session_set_profile (gCall[zCall].rtp_session_tone, &av_profile);

	//rtp_session_set_blocking_mode(gCall[zCall].rtp_session_tone, 1);

	rtp_session_set_remote_addr (gCall[zCall].rtp_session_tone,
								 gCall[zCall].toneServer,
								 gCall[zCall].toneRtpPort, gHostIf);

	rtp_session_set_payload_type (gCall[zCall].rtp_session_tone,
								  gCall[zCall].codecType);

	if (gCall[zCall].telephonePayloadType > -1)
	{
		rtp_profile_set_payload (&av_profile,
								 gCall[zCall].telephonePayloadType,
								 &telephone_event);
	}

	gCall[zCall].sendCallProgressAudio = 1;

	if (access ("/tmp/.stopCallProgress", F_OK) == 0)
	{
		gCall[zCall].sendCallProgressAudio = 0;
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Tone server %s port:%d session=%p. fifo:%s.",
			   gCall[zCall].toneServer,
			   gCall[zCall].toneRtpPort,
			   gCall[zCall].rtp_session_tone,
			   gCall[zCall].toneServerSpecialFifo);

	return (0);

}								/*END: int process_DMOP_START_CALL_PROGRESS_RESPONSE */

/*
 *	This routine will reset sendCallProgressAudio flag and 
 *	make the inputThread stop sending audio to aculab.
 *	It will also send the result of call progress back to application.
 */
int
process_DMOP_END_CALL_PROGRESS (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_END_CALL_PROGRESS" };
	char            message[MAX_LOG_MSG_LENGTH] = "";
	int             retCode = -21;

	struct Msg_CallProgress yMsgCallProgress;
	memcpy (&yMsgCallProgress, &gCall[zCall].msgToDM,
			sizeof (struct Msg_CallProgress));

	struct MsgToApp response;


	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

    // added JOES to deal with a bug wherethe media manager was sending the wrong appPid
    // at INIT TELECOM we store this from the app that created the call 
    // Wed Oct 28 09:18:52 EDT 2015

	yMsgCallProgress.appPid = gCall[zCall].appPid;

	if (!canContinue (mod, zCall, __LINE__))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Not processing (%s). Call was disconnected.", message);

		return (0);
	}
	else if (gCall[zCall].sendCallProgressAudio == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Not processing (%s); lendCallProgressAudio flag was reset.",
				   message);
		return (0);
	}

	gCall[zCall].sendCallProgressAudio = 0;

	sscanf (yMsgCallProgress.nameValue, "callProgress=%d", &retCode);

	response.opcode = yMsgCallProgress.origOpcode;
	response.returnCode = retCode;
	sprintf (response.message, "%d", zCall);

	gCall[zCall].outboundRetCode = retCode;

	if (gCall[zCall].leg == LEG_B)
	{
	int             legA = gCall[zCall].crossPort;

		if (legA > -1)
		{
			gCall[legA].outboundRetCode = retCode;
		}
	}

	if (gCall[zCall].isItAnswerCall == 1)
	{
		response.opcode = DMOP_ANSWERCALL;
		memcpy (&response, &gCall[zCall].answerCallResp,
				sizeof (struct MsgToApp));
	}

// MR 4655
	if ( gCall[zCall].callState == CALL_STATE_CALL_INITIATE_ISSUED )
	{
		sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,message:%s}",
			 response.opcode, response.appCallNum,
			 response.appPid, response.appRef,
			 response.appPassword, response.message);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Call has not yet been answered; Not processing (%s).", message);
		gCall[zCall].sendSavedInitiateCall = 1;
		memcpy (&gCall[zCall].respMsgInitiateCallToApp, &response, sizeof (struct MsgToApp));
		return(0);
	}
// END: MR 4655

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Sending call progress retcode(%d) to app.", retCode);

	writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	// stopInbandToneDetectionThread(__LINE__, zCall);

	return (0);

}								/*END: int process_DMOP_END_CALL_PROGRESS */

///This function is called when Media Manager receives a DMOP_DISCONNECT.
int
process_DMOP_DISCONNECT (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_DISCONNECT" };
	char            message[MAX_LOG_MSG_LENGTH];
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;

	response.message[0] = '\0';
	int             signalApp = 1;
	char            blastDialFile1[256];
	char            blastDialFile2[256];

	sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
			 msg.opcode, msg.appCallNum,
			 msg.appPid, msg.appRef, msg.appPassword);

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing %s. leg:%s, crossPort:%d.",
			   message,
			   (gCall[zCall].leg == LEG_B) ? "B" : "A",
			   gCall[zCall].crossPort);

	if(gCall[zCall].googleUDPFd > -1)
	{
__DDNDEBUG__ (DEBUG_MODULE_SR, "MRCP: ARCGS: Closing googleUDPFd", "", gCall[zCall].googleUDPFd);
		
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
   			"GSR: Sending 'hangup' to client to stop processing");
		sendto(gCall[zCall].googleUDPFd,
					"hangup", 
					6, 
					0,
					(struct sockaddr *) &gCall[zCall].google_si, 
					gCall[zCall].google_slen);	

		close(gCall[zCall].googleUDPFd);
		gCall[zCall].googleUDPPort = -1;
		gCall[zCall].googleUDPFd = -1;
		gCall[zCall].speechRec = 0;
	}

	if (msg.data[0] != '\0')
	{
		if (!strcmp (msg.data, "false"))
		{
			signalApp = 0;
		}

        if ( atoi(msg.data) == CALL_STATE_CALL_TRANSFER_MESSAGE_ACCEPTED )		// MR 4802
        {
            response.alternateRetCode = DMOP_TRANSFERCALL;
            dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
                "Set response.alternateRetCode to %d (DMOP_TRANSFERCALL).", response.alternateRetCode);
        }
	}

	response.opcode = DMOP_DISCONNECT;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appCallNum;

	if (gCall[zCall].isSendRecvFaxActive != 0)
	{
		dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_DIAGNOSE, FAX_BASE,
				   INFO, "Stopping fax thread activity on call disconnect.");
		stopFaxThread (zCall);
	}

	if (gCall[zCall].leg == LEG_B)
	{
	int             yALeg = gCall[zCall].crossPort;

		if (yALeg >= 0 || yALeg < MAX_PORTS)
		{
			if (gCall[yALeg].callState == CALL_STATE_CALL_BRIDGED ||
				gCall[yALeg].callSubState == CALL_STATE_CALL_MEDIACONNECTED ||
				gCall[yALeg].callSubState == CALL_STATE_CALL_LISTENONLY)
			{
				if (gCall[yALeg].crossPort == zCall)
				{
					setCallState (yALeg, CALL_STATE_IDLE, __LINE__);
					setCallSubState (yALeg, CALL_STATE_IDLE);
				}
			}
		}

		response.opcode = DMOP_DISCONNECT;
		response.returnCode = -3;
		response.appPassword = yALeg;

		sprintf (blastDialFile1, "%s.%d", BLASTDIAL_RESULT_FILE_BASE, zCall);
		sprintf (blastDialFile2, "%s.%d", BLASTDIAL_RESULT_FILE_BASE, yALeg);

		dynVarLog (__LINE__, yALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Checking for existance of files (%s) (%s).",
				   blastDialFile1, blastDialFile2);

		if (access (blastDialFile1, R_OK) == 0)
		{
			sprintf (response.message, "%s", blastDialFile1);
			if (access (blastDialFile2, R_OK) == 0)
			{
				unlink (blastDialFile2);
			}
		}
		else if (access (blastDialFile2, R_OK) == 0)
		{
			sprintf (response.message, "%s", blastDialFile2);
		}

		if (yALeg >= 0)
		{
			dynVarLog (__LINE__, yALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Calling writeGenericResponseToApp leg:%s, crossPort:%d.",
					   (gCall[yALeg].leg == LEG_B) ? "B" : "A",
					   gCall[yALeg].crossPort);
			if ( (signalApp) && (! gCall[zCall].recordStartedForDisconnect) )           // MR 5126
			{
				writeGenericResponseToApp (yALeg, &response, mod, __LINE__);
			}
		}
		else
		{
			dynVarLog (__LINE__, yALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Calling writeGenericResponseToApp with B Leg %d since no A Leg found.",
					   zCall);
			if ( (signalApp) && (! gCall[zCall].recordStartedForDisconnect) )           // MR 5126
			{
				writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			}
		}

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 0", "LEG=",
					  gCall[zCall].leg);
		clearPort (zCall, mod, 1, 0);
		//return(0);
	}
	else
	{
		sprintf (blastDialFile1, "%s.%d", BLASTDIAL_RESULT_FILE_BASE,
				 msg.appCallNum);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Checking for existance of file (%s).", blastDialFile1);
		if (access (blastDialFile1, R_OK) == 0)
		{
			sprintf (response.message, "%s", blastDialFile1);
		}
		
		if ( (signalApp) && (! gCall[zCall].recordStartedForDisconnect) )           // MR 5126
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
					   "sendCallProgressAudio=%d, isCallInitiated=%d, appRef=%d, lastMsgToDM.appRef=%d",
					   gCall[zCall].sendCallProgressAudio,
					   gCall[zCall].isCallInitiated, response.appRef,
					   gCall[zCall].lastMsgToDM.appRef);

			if ( (gCall[zCall].sendCallProgressAudio == 1) &&
			     (gCall[zCall].isCallInitiated == YES) &&
			     (! gCall[zCall].recordStartedForDisconnect) )      // MR 5126
			{
				response.opcode = DMOP_INITIATECALL;
				response.appRef = gCall[zCall].lastMsgToDM.appRef;
				response.returnCode = -3;
				writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			}
			if (! gCall[zCall].recordStartedForDisconnect)      // MR 5126
			{
				response.opcode = DMOP_DISCONNECT;
				response.appRef = msg.appRef;
				response.returnCode = 0;
	
				writeGenericResponseToApp (zCall, &response, mod, __LINE__);
				// writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			}
		}
	}

	time (&gCall[zCall].yDropTime);

	if ((gMrcpTTS_Enabled) && (gCall[zCall].ttsRequestReceived))
	{
	ARC_TTS_REQUEST_SINGLE_DM ttsRequest;

		memset ((ARC_TTS_REQUEST_SINGLE_DM *) & ttsRequest, '\0',
				sizeof (ttsRequest));
		ttsRequest.speakMsgToDM.opcode = DMOP_DISCONNECT;
		ttsRequest.speakMsgToDM.appRef = gCall[zCall].msgToDM.appRef;
		sprintf (ttsRequest.port, "%d", zCall);
		sprintf (ttsRequest.pid, "%d", gCall[zCall].appPid);
		sprintf (ttsRequest.resultFifo, "%s", "notUsed");
		sprintf (ttsRequest.string, "%s", "notUsed");

		gCall[zCall].keepSpeakingMrcpTTS = 0;
		(void) sendMsgToTTSClient (zCall, &ttsRequest);
		gCall[zCall].ttsRequestReceived = 0;
	}

#ifdef VOICE_BIOMETRICS
    if (gCall[zCall].continuousVerify == CONTINUOUS_VERIFY_ACTIVE)
    {
        closeVoiceID(zCall);
        gCall[zCall].continuousVerify = CONTINUOUS_VERIFY_INACTIVE;
        gCall[zCall].avb_bCounter   = 0;
    }
#endif  // END: VOICE_BIOMETRICS


#ifdef ACU_LINUX

	if (gCall[zCall].sendCallProgressAudio == 1)
	{
	struct Msg_CallProgress yMsgCallProgress;

		yMsgCallProgress.opcode = DMOP_CANCEL_CALL_PROGRESS;
		yMsgCallProgress.origOpcode = DMOP_INITIATECALL;
		yMsgCallProgress.appCallNum = zCall;
		yMsgCallProgress.appPid = msg.appPid;
		yMsgCallProgress.appRef = msg.appRef;
		yMsgCallProgress.appPassword = msg.appPassword;

		gCall[zCall].sendCallProgressAudio = 0;

		sendRequestToSTonesClientSpecialFifo (zCall, mod,
											  (struct MsgToDM *)
											  &yMsgCallProgress);

	}

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "isFaxReserverResourceCalled=%d, isSendRecvFaxActive=%d",
			   gCall[zCall].isFaxReserverResourceCalled,
			   gCall[zCall].isSendRecvFaxActive);

	if (gCall[zCall].isFaxReserverResourceCalled == 1 ||
		gCall[zCall].isSendRecvFaxActive == 1)
	{
	struct MsgToDM  yMsgToDM;

		yMsgToDM.opcode = DMOP_FAX_CANCEL;
		yMsgToDM.appCallNum = zCall;
		yMsgToDM.appPid = msg.appPid;
		yMsgToDM.appRef = msg.appRef;
		yMsgToDM.appPassword = msg.appPassword;

		dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, FAX_BASE,
				   INFO, "Sending DMOP_FAX_CANCEL to TonesFaxClient");
	int             rc =
		sendRequestToSTonesFaxClient (mod, (struct MsgToDM *) &(yMsgToDM),
									  zCall);
	}

#endif

	if (gCall[zCall].conferenceStarted == 1
		&& gCall[zCall].conferenceIdx != -1)
	{
#ifdef ACU_LINUX
		MSG_CONF_REMOVE_USER yTmpConfRemoveUser;

		yTmpConfRemoveUser.opcode = DMOP_CONFERENCE_REMOVE_USER;
		yTmpConfRemoveUser.appCallNum = zCall;
		yTmpConfRemoveUser.appPid = msg.appPid;
		yTmpConfRemoveUser.appRef = msg.appRef;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF:RemoveUser confID=%s.", gCall[zCall].confID);

		sprintf (yTmpConfRemoveUser.confId, "%s", gCall[zCall].confID);

		yRc =
			sendRequestToConfMgr (mod,
								  (struct MsgToDM *) &(yTmpConfRemoveUser));
#else
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF:Deleting conference by name; confID=(%s).", gCall[zCall].confID);
		arc_clock_sync_broadcast (clock_sync_in, &gCall[zCall],
								  ARC_CLOCK_SYNC_READ);
		arc_clock_sync_broadcast (clock_sync_in, &gCall[zCall],
								  ARC_CLOCK_SYNC_WRITE);
		yRc =
			arc_conference_delref_by_name (Conferences, 48, zCall,
										   gCall[zCall].confID);
		arc_clock_sync_del (clock_sync_in, &gCall[zCall]);
		arc_clock_sync_del (clock_sync_in, &gCall[zCall]);



#endif
		gCall[zCall].conferenceStarted = 0;
		gCall[zCall].lastOutRtpTime.time = 0;
	}

	if (gCall[zCall].outputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].outputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else if (gCall[zCall].inputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].inputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else
	{
		gCall[zCall].clear = 1;
		gCall[zCall].cleanerThread = 0;
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 0", "LEG=",
					  gCall[zCall].leg);
		clearPort (zCall, mod, 1, 0);
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}

	stopFaxThread (zCall);

	return yRc;

}								/*END: int process_DMOP_DISCONNECT */

///This function is called when Media Manager receives a DMOP_ANSWERCALL.
int
process_DMOP_ANSWERCALL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_ANSWERCALL" };
	char            message[MAX_LOG_MSG_LENGTH];
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;
	int             yIntRetcode = 0;

	response.message[0] = '\0';
	sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
			 msg.opcode, msg.appCallNum,
			 msg.appPid, msg.appRef, msg.appPassword);

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing %s.", message);

	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;
	gCall[zCall].isCallAnswered = YES;
	gCall[zCall].isItAnswerCall = 1;

	sprintf (response.message, "\0");

	if (gCall[zCall].rtp_session == NULL)
	{
		response.returnCode = -1;
		sprintf (response.message, "Media Failed, no out rtp");
	} else {
      arc_rtp_session_set_ssrc(zCall, gCall[zCall].rtp_session);
    }

	if (gCall[zCall].rtp_session_in == NULL)
	{
		response.returnCode = -1;
		sprintf (response.message, "Media Failed, no in rtp");
	}

	if (gCall[zCall].responseFifoFd >= 0)
	{

		if (response.returnCode > -1)
		{
			sprintf (response.message, "AUDIO_CODEC=%s",
					 gCall[zCall].audioCodecString);
		}

		if (gCall[zCall].GV_CallProgress != 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Call Progress is %d; not sending response to app.",
					   gCall[zCall].GV_CallProgress);

			memcpy (&(gCall[zCall].answerCallResp), &response,
					sizeof (struct MsgToApp));

		}
		else
		{
			writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		}

	}

	return yRc;

}								/*END: int process_DMOP_ANSWERCALL */

///This function is called when Media Manager receives a DMOP_APPDIED.
int
process_DMOP_APPDIED (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_APPDIED" };

	char            fifoName[256];

	sprintf (fifoName, "%s", gCall[zCall].responseFifo);

	//if(access(fifoName, F_OK) == 0)
	{
		if (gCall[zCall].responseFifoFd >= 0)
		{
			arc_close (zCall, mod, __LINE__, &(gCall[zCall].responseFifoFd));
		}

		arc_unlink (zCall, mod, fifoName);
//		dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO, 
//				"DJB: unlink (%s)", fifoName);
		gCall[zCall].responseFifo[0] = 0;
	}

	gCall[zCall].responseFifoFd = -1;

	if (gCall[zCall].outputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].outputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else if (gCall[zCall].inputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].inputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else
	{
		gCall[zCall].clear = 1;
		gCall[zCall].cleanerThread = 0;
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 1", "LEG=",
					  gCall[zCall].leg);
		clearPort (zCall, mod, 1, 1);
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}

	return yRc;

}								/*END: int process_DMOP_APPDIED */

///This function is called when Media Manager receives a DMOP_CANTFIREAPP.
int
process_DMOP_CANTFIREAPP (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_CANTFIREAPP" };

	if (gCall[zCall].outputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].outputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else if (gCall[zCall].inputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].inputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else
	{
		gCall[zCall].clear = 1;
		gCall[zCall].cleanerThread = 0;
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 1", "LEG=",
					  gCall[zCall].leg);
		clearPort (zCall, mod, 1, 1);
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}

	return yRc;

}								/*END: int process_DMOP_CANTFIREAPP */

///This function is called when Media Manager receives a DMOP_DROPCALL.
int
process_DMOP_DROPCALL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_DROPCALL" };
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;

	response.opcode = DMOP_DISCONNECT;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;

	sprintf (response.message, "\0");

	if ((gMrcpTTS_Enabled) && (gCall[zCall].ttsRequestReceived))
	{
	ARC_TTS_REQUEST_SINGLE_DM ttsRequest;

		memset ((ARC_TTS_REQUEST_SINGLE_DM *) & ttsRequest, '\0',
				sizeof (ttsRequest));
		ttsRequest.speakMsgToDM.opcode = DMOP_DISCONNECT;
		ttsRequest.speakMsgToDM.appRef = gCall[zCall].msgToDM.appRef;
		sprintf (ttsRequest.port, "%d", zCall);
		sprintf (ttsRequest.pid, "%d", gCall[zCall].appPid);
		sprintf (ttsRequest.resultFifo, "%s", "notUsed");
		sprintf (ttsRequest.string, "%s", "notUsed");

		gCall[zCall].keepSpeakingMrcpTTS = 0;
		(void) sendMsgToTTSClient (zCall, &ttsRequest);
	}
	writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	time (&gCall[zCall].yDropTime);

	int             yALeg = gCall[zCall].crossPort;

	if (gCall[zCall].leg != LEG_A)
	{
		if (yALeg >= 0 || yALeg < MAX_PORTS)
		{
			if (gCall[yALeg].callState == CALL_STATE_CALL_BRIDGED ||
				gCall[yALeg].callSubState == CALL_STATE_CALL_MEDIACONNECTED)
			{
				if (gCall[yALeg].crossPort == zCall &&
					gCall[yALeg].callSubState != CALL_STATE_CALL_LISTENONLY)
				{
					setCallState (yALeg, CALL_STATE_IDLE, __LINE__);
					setCallSubState (yALeg, CALL_STATE_IDLE);
				}
			}
		}
	}

	if (gCall[zCall].conferenceStarted == 1
		&& gCall[zCall].conferenceIdx != -1)
	{
	MSG_CONF_REMOVE_USER yTmpConfRemoveUser;

		yTmpConfRemoveUser.opcode = DMOP_CONFERENCE_REMOVE_USER;
		yTmpConfRemoveUser.appCallNum = zCall;
		yTmpConfRemoveUser.appPid = msg.appPid;
		yTmpConfRemoveUser.appRef = msg.appRef;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "CONF:RemoveUser confID=%s, portnum=%d.",
				   gCall[zCall].confID, zCall);

		sprintf (yTmpConfRemoveUser.confId, "%s", gCall[zCall].confID);

#ifdef ACU_LINUX
		yRc =
			sendRequestToConfMgr (mod,
								  (struct MsgToDM *) &(yTmpConfRemoveUser));
#else
		arc_clock_sync_broadcast (clock_sync_in, &gCall[zCall],
								  ARC_CLOCK_SYNC_READ);
		arc_clock_sync_broadcast (clock_sync_in, &gCall[zCall],
								  ARC_CLOCK_SYNC_WRITE);
		yRc =
			arc_conference_delref_by_name (Conferences, 48, zCall,
										   gCall[zCall].confID);
		arc_clock_sync_del (clock_sync_in, &gCall[zCall]);
		arc_clock_sync_del (clock_sync_in, &gCall[zCall]);
#endif
		gCall[zCall].conferenceStarted = 0;
		gCall[zCall].lastOutRtpTime.time = 0;
	}

	if (gCall[zCall].outputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].outputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else if (gCall[zCall].inputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].inputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else
	{
		gCall[zCall].clear = 1;
		gCall[zCall].cleanerThread = 0;
		if (gCall[zCall].leg == LEG_B || gCall[zCall].thirdParty == 1)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 0",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 0);
		}
		else
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 1",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 1);
		}
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}

#ifdef VOICE_BIOMETRICS
    if (gCall[zCall].continuousVerify == CONTINUOUS_VERIFY_ACTIVE)
    {
        closeVoiceID(zCall);
        gCall[zCall].continuousVerify = CONTINUOUS_VERIFY_INACTIVE;
        gCall[zCall].avb_bCounter   = 0;
    }
#endif  // END: VOICE_BIOMETRICS

	return yRc;

}								/*END: int process_DMOP_DROPCALL */

///This function is called when Media Manager receives a DMOP_DROPCALL.
int
process_DMOP_REJECTCALL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_REJECTCALL" };
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;

	response.opcode = DMOP_DISCONNECT;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;

	sprintf (response.message, "\0");

	if ((gMrcpTTS_Enabled) && (gCall[zCall].ttsRequestReceived))
	{
	ARC_TTS_REQUEST_SINGLE_DM ttsRequest;

		memset ((ARC_TTS_REQUEST_SINGLE_DM *) & ttsRequest, '\0',
				sizeof (ttsRequest));
		ttsRequest.speakMsgToDM.opcode = DMOP_DISCONNECT;
		ttsRequest.speakMsgToDM.appRef = gCall[zCall].msgToDM.appRef;
		sprintf (ttsRequest.port, "%d", zCall);
		sprintf (ttsRequest.pid, "%d", gCall[zCall].appPid);
		sprintf (ttsRequest.resultFifo, "%s", "notUsed");
		sprintf (ttsRequest.string, "%s", "notUsed");

		gCall[zCall].keepSpeakingMrcpTTS = 0;
		(void) sendMsgToTTSClient (zCall, &ttsRequest);
	}
	//writeGenericResponseToApp(zCall, &response, mod, __LINE__);

	time (&gCall[zCall].yDropTime);

	int             yALeg = gCall[zCall].crossPort;

	if (yALeg >= 0 || yALeg < MAX_PORTS)
	{
		if (gCall[yALeg].callState == CALL_STATE_CALL_BRIDGED ||
			gCall[yALeg].callSubState == CALL_STATE_CALL_MEDIACONNECTED ||
			gCall[yALeg].callSubState == CALL_STATE_CALL_LISTENONLY)
		{
			if (gCall[yALeg].crossPort == zCall)
			{
				setCallState (yALeg, CALL_STATE_CALL_CLOSED, __LINE__);
			}
		}
	}

	if (gCall[zCall].outputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].outputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else if (gCall[zCall].inputThreadId > 0)
	{
		gCall[zCall].cleanerThread = gCall[zCall].inputThreadId;
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}
	else
	{
		gCall[zCall].clear = 1;
		gCall[zCall].cleanerThread = 0;
		if (gCall[zCall].leg == LEG_B || gCall[zCall].thirdParty == 1)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 0",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 0);
		}
		else
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 1",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 1);
		}
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
	}

	return yRc;

}								/*END: int process_DMOP_REJECTCALL */

///This function is a thread that is fired when there is a hung outputThread.
/**
Sometimes our oRTP stack makes the outputThread hang, if another process or
application is using the same socket it needs to send packets.  In that case
we fire the initTelecomThread() which cancels that thread and continues with
initializing Telecom.
*/
void           *
initTelecomThread (void *z)
{
	char            mod[] = { "initTelecomThread" };
	int             zCall = ((struct MsgToDM *) z)->appCallNum;
	struct MsgToApp response;
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct Msg_InitTelecom *ptrMsgInit =
		(struct Msg_InitTelecom *) &(gCall[zCall].msgToDM);

	int             yIntCount = 0;
	int             yRc = -1;

	response.message[0] = '\0';

	pthread_t       inputThreadId = gCall[zCall].inputThreadId;
	pthread_t       outputThreadId = gCall[zCall].outputThreadId;

	while (yIntCount < 20)
	{
		if (inputThreadId > 0 || outputThreadId > 0)
		{
			yIntCount++;
			usleep (1000 * 10);
			continue;
		}
	}

	if (inputThreadId > 0)
	{

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Cancelling input thread", "",
					  inputThreadId);

		yRc = pthread_cancel (inputThreadId);
		usleep (1000 * 10);

		if (gCall[zCall].cleanerThread == inputThreadId
			&& gCall[zCall].inputThreadId != 0)
		{
			gCall[zCall].clear = 1;
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 0",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 0);
			gCall[zCall].inputThreadId = 0;
			gCall[zCall].cleanerThread = 0;
		}

		if (gCall[zCall].rtp_session_in != NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
					   "Calling arc_rtp_session_destroy rtp_session_in socket=%d.",
					   gCall[zCall].rtp_session_in->rtp.socket);

			arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_in);
		}

		gCall[zCall].inputThreadId = 0;
	}

	if (outputThreadId > 0)
	{

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Cancelling output thread", "",
					  outputThreadId);

		yRc = pthread_cancel (outputThreadId);
		usleep (1000 * 10);

		if (yRc < 0)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL,
						  "Cancelling output thread failed.", "",
						  outputThreadId);
		}

		if (gCall[zCall].cleanerThread == outputThreadId
			&& gCall[zCall].outputThreadId != 0)
		{
			gCall[zCall].clear = 1;
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 0",
						  "LEG=", gCall[zCall].leg);
			clearPort (zCall, mod, 1, 0);
			gCall[zCall].outputThreadId = 0;
			gCall[zCall].cleanerThread = 0;
		}

		if (gCall[zCall].rtp_session != NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Calling arc_rtp_session_destroy(); rtp_session socket=%d.",
					   gCall[zCall].rtp_session->rtp.socket);

			arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session);
		}

		gCall[zCall].outputThreadId = 0;

		if (gCall[zCall].currentSpeakFd > -1)
		{
			arc_close (zCall, mod, __LINE__, &(gCall[zCall].currentSpeakFd));
		}
	}

	if (gCall[zCall].responseFifo[0])
	{
		if (gCall[zCall].responseFifoFd >= 0)
		{
			arc_close (zCall, mod, __LINE__, &(gCall[zCall].responseFifoFd));
			gCall[zCall].responseFifoFd = -1;
		}
		arc_unlink (zCall, mod, gCall[zCall].responseFifo);
//		dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO, 
//							"DJB: unlink (%s)", gCall[zCall].responseFifo);
		gCall[zCall].responseFifo[0] = 0;
	}

	if (gCall[zCall].clear == 0)
	{
		gCall[zCall].cleanerThread = 0;
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 1", "LEG=",
					  gCall[zCall].leg);
		clearPort (zCall, mod, 0, 1);
	}

	gCall[zCall].clear = 1;

	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;

	sprintf (response.message, "\0");

	gCall[zCall].appPid = msg.appPid;
	gCall[zCall].appPassword = msg.appPassword;
	gCall[zCall].leg = LEG_A;
	sprintf (gCall[zCall].responseFifo, "%s", ptrMsgInit->responseFifo);

	/*Read call info from callInfo.port file */
	{
	char            yStrFileCallInfo[100];
	char            line[256];
	FILE           *yFpCallInfo = NULL;
	int             i = 0;
	char            lhs[128];
	char            rhs[128];

		sprintf (yStrFileCallInfo, "%s/callInfo.%d", gCacheDirectory, zCall);

		yFpCallInfo = fopen (yStrFileCallInfo, "r");
		if (yFpCallInfo == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR, "Failed to open call info file (%s). [%d, %s] ",
					   yStrFileCallInfo, errno, strerror (errno));
		}
		else
		{

			sprintf (gCall[zCall].ani, "%s", "\0");
			sprintf (gCall[zCall].dnis, "%s", "\0");

			while (fgets (line, sizeof (line) - 1, yFpCallInfo))
			{
				/*  Strip \n from the line if it exists */
				if (line[(int) strlen (line) - 1] == '\n')
				{
					line[(int) strlen (line) - 1] = '\0';
				}

				sscanf (line, "%[^=]=%s", lhs, rhs);

//              dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//                             "line=%s", line); 
				if ((lhs != NULL) && (rhs != NULL) && (lhs[0] != 0)
					&& (rhs[0] != 0))
				{
					if (strcmp (lhs, "ANI") == 0)
					{
						sprintf (gCall[zCall].ani, "%s", rhs);
					}
					else if (strcmp (lhs, "DNIS") == 0)
					{
						sprintf (gCall[zCall].dnis, "%s", rhs);
					}
					else if (strcmp (lhs, "OCN") == 0)
					{
						sprintf (gCall[zCall].ocn, "%s", rhs);
					}
					else if (strcmp (lhs, "RDN") == 0)
					{
						sprintf (gCall[zCall].rdn, "%s", rhs);
					}
					else if (strcmp (lhs, "CALL_TYPE") == 0)
					{
						sprintf (gCall[zCall].call_type, "%s", rhs);
					}
					else if (strcmp (lhs, "AUDIO_CODEC") == 0)
					{
						sprintf (gCall[zCall].audioCodecString, "%s", rhs);
					}
					else if (strcmp (lhs, "ANNEXB") == 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "ANNEXB=%s, gCall[zCall].audioCodecString=%s.",
								   rhs, gCall[zCall].audioCodecString);
						if (strstr (gCall[zCall].audioCodecString, "729") !=
							NULL)
						{
	int             annexb = atoi (rhs);

							gCall[zCall].isG729AnnexB = annexb;

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO, "ANNEXB=%d, YES=%d",
									   annexb, YES);
							if (annexb == YES)
							{
								sprintf (gCall[zCall].audioCodecString,
										 "729b");
							}
							else
							{
								sprintf (gCall[zCall].audioCodecString,
										 "729a");
							}
						}
					}
				}

			}					/*while fgets */

			fclose (yFpCallInfo);

			arc_unlink (zCall, mod, yStrFileCallInfo);

		}						/*else */

	}							/*END: read call info */

	setCallState (zCall, CALL_STATE_IDLE, __LINE__);

	writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	pthread_detach (pthread_self ());

	pthread_exit (NULL);

	return (0);

}								/*END: void* initTelecomThread */

///This function is called when Media Manager receives a DMOP_INITTELECOM.
int
process_DMOP_INITTELECOM (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_INITTELECOM" };
	char            message[MAX_LOG_MSG_LENGTH];
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;

	struct Msg_InitTelecom *ptrMsgInit =
		(struct Msg_InitTelecom *) &(gCall[zCall].msgToDM);

	response.message[0] = '\0';
	sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d}",
			 msg.opcode, msg.appCallNum,
			 msg.appPid, msg.appRef, msg.appPassword);

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing %s.", message);

	time (&gCall[zCall].yInitTime);

	if (gCall[zCall].inputThreadId > 0 || gCall[zCall].outputThreadId > 0)
	{
#if 0
		yRc = pthread_create (&gCall[zCall].initTelecomThreadId,
							  &gpthread_attr,
							  initTelecomThread,
							  (void *) &(gCall[zCall].msgToDM));
		if (yRc != 0)
		{
			;
		}
#endif
		dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_NORMAL, TEL_BASE,
				   ERR,
				   "Failed to process DMOP_INITTELECOM. "
				   "Active threads(input:%d, output:%d) found.",
				   gCall[zCall].inputThreadId, gCall[zCall].outputThreadId);

		if (gCall[zCall].responseFifo[0] || gCall[zCall].responseFifoFd >= 0)
		{
			if (gCall[zCall].responseFifoFd >= 0)
			{
				arc_close (zCall, mod, __LINE__,
						   &(gCall[zCall].responseFifoFd));
				gCall[zCall].responseFifoFd = -1;
			}

			gCall[zCall].responseFifo[0] = 0;
		}

		response.opcode = msg.opcode;
		response.appCallNum = msg.appCallNum;
		response.appPid = msg.appPid;
		response.appRef = msg.appRef;
		response.returnCode = -1;
		response.appPassword = msg.appPassword;

		sprintf (response.message, "Threads Active");
		sprintf (gCall[zCall].responseFifo, "%s", ptrMsgInit->responseFifo);
		writeGenericResponseToApp (zCall, &response, mod, __LINE__);

		return (0);
	}

	if (gCall[zCall].responseFifo[0] || gCall[zCall].responseFifoFd >= 0)
	{
		if (gCall[zCall].responseFifoFd >= 0)
		{
			arc_close (zCall, mod, __LINE__, &(gCall[zCall].responseFifoFd));
			gCall[zCall].responseFifoFd = -1;
		}
		//arc_unlink(zCall, mod, gCall[zCall].responseFifo);
		gCall[zCall].responseFifo[0] = 0;
	}

	if (gCall[zCall].clear == 0)
	{
		gCall[zCall].cleanerThread = 0;
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 1", "LEG=",
					  gCall[zCall].leg);
		clearPort (zCall, mod, 0, 1);
	}

	gCall[zCall].clear = 1;
	stopInbandToneDetectionThread (__LINE__, zCall);

	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;

	sprintf (response.message, "\0");
	sprintf (response.message, "%d", getppid ());

	gCall[zCall].appPid = msg.appPid;
	gCall[zCall].appPassword = msg.appPassword;
	gCall[zCall].leg = LEG_A;

	gCall[zCall].GV_Speed = 0;

	sprintf (gCall[zCall].responseFifo, "%s", ptrMsgInit->responseFifo);

	/*Read call info from callInfo.port file */
	{
	char            yStrFileCallInfo[100];
	char            line[256];
	FILE           *yFpCallInfo = NULL;
	int             i = 0;
	char            lhs[128];
	char            rhs[128];

		gCall[zCall].ani[0] = 0;
		gCall[zCall].dnis[0] = 0;

		sprintf (yStrFileCallInfo, "callInfo.%d", zCall);

		yFpCallInfo = fopen (yStrFileCallInfo, "r");
		if (yFpCallInfo == NULL)
		{
			dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_NORMAL,
					   TEL_FILE_IO_ERROR, ERR,
					   "Failed to read call info from (%s). [%d, %s]",
					   yStrFileCallInfo, errno, strerror (errno));
		}
		else
		{
			while (fgets (line, sizeof (line) - 1, yFpCallInfo))
			{
				/*  Strip \n from the line if it exists */
				if (line[(int) strlen (line) - 1] == '\n')
				{
					line[(int) strlen (line) - 1] = '\0';
				}

				lhs[0] = 0;
				rhs[0] = 0;

				sscanf (line, "%[^=]=%s", lhs, rhs);

				if ((lhs != NULL) && (rhs != NULL) && (lhs[0] != 0)
					&& (rhs[0] != 0))
				{
					if (strcmp (lhs, "ANI") == 0)
					{
						sprintf (gCall[zCall].ani, "%s", rhs);
					}
					else if (strcmp (lhs, "DNIS") == 0)
					{
						sprintf (gCall[zCall].dnis, "%s", rhs);
					}
					else if (strcmp (lhs, "OCN") == 0)
					{
						sprintf (gCall[zCall].ocn, "%s", rhs);
					}
					else if (strcmp (lhs, "RDN") == 0)
					{
						sprintf (gCall[zCall].rdn, "%s", rhs);
					}
					else if (strcmp (lhs, "CALL_TYPE") == 0)
					{
						sprintf (gCall[zCall].call_type, "%s", rhs);
					}
					else if (strcmp (lhs, "AUDIO_CODEC") == 0)
					{
						sprintf (gCall[zCall].audioCodecString, "%s", rhs);
					}
					else if (strcmp (lhs, "ANNEXB") == 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "ANNEXB=%s, gCall[zCall].audioCodecString=(%s)",
								   rhs, gCall[zCall].audioCodecString);
						if (strstr (gCall[zCall].audioCodecString, "729") !=
							NULL)
						{
	int             annexb = atoi (rhs);

							gCall[zCall].isG729AnnexB = annexb;
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO, "ANNEXB=%d, YES=%d",
									   annexb, YES);
							if (annexb == YES)
							{
								sprintf (gCall[zCall].audioCodecString,
										 "729b");
							}
							else
							{
								sprintf (gCall[zCall].audioCodecString,
										 "729a");
							}
						}
					}
				}

			}					/*while fgets */

			fclose (yFpCallInfo);

			arc_unlink (zCall, mod, yStrFileCallInfo);

		}						/*else */

	}							/*END: read call info */

	setCallState (zCall, CALL_STATE_IDLE, __LINE__);

	writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	return (yRc);

}								/*END: int process_DMOP_INITTELECOM */

int
process_DMOP_MEDIACONNECT (int zCall)
{
	int             yRc = 0;
	char            mod[] = "process_DMOP_MEDIACONNECT";
	struct MsgToApp response;
	struct Msg_InitiateCall msg;

	gCall[zCall].GV_BridgeRinging = 0;

	response.message[0] = '\0';
	memcpy (&msg, &(gCall[zCall].msgToDM), sizeof (struct Msg_InitiateCall));

	if (gCall[zCall].sendingSilencePackets != 1)
	{
		clearBkpSpeakList (zCall);
		gCall[zCall].pendingOutputRequests = 1;
	}

	gCall[msg.appCallNum2].yDropTime = 0;
	gCall[msg.appCallNum2].yInitTime = 0;

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "AppCallNum1", "", msg.appCallNum1);
	__DDNDEBUG__ (DEBUG_MODULE_CALL, "AppCallNum2", "", msg.appCallNum2);

	gCall[msg.appCallNum1].crossPort = msg.appCallNum2;
	gCall[msg.appCallNum2].crossPort = msg.appCallNum1;

	gCall[msg.appCallNum2].appPid = msg.appPid;
	gCall[msg.appCallNum2].appPassword = msg.appCallNum2;

	setCallSubState (msg.appCallNum1, CALL_STATE_CALL_MEDIACONNECTED);
	setCallSubState (gCall[msg.appCallNum1].crossPort,
					 CALL_STATE_CALL_MEDIACONNECTED);

	return (0);

}								/*END: int process_DMOP_MEDIACONNECT */

///This function is called when Media Manager receives a DMOP_INITIATECALL, it is only used for bridge calls.
int
process_DMOP_INITIATECALL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_INITIATECALL" };
	struct MsgToApp response;
	struct Msg_InitiateCall msg;
	char			message[512];

	response.message[0] = '\0';
	memcpy (&msg, &(gCall[zCall].msgToDM), sizeof (struct Msg_InitiateCall));

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "AppCallNum1", "", msg.appCallNum1);
	__DDNDEBUG__ (DEBUG_MODULE_CALL, "AppCallNum2", "", msg.appCallNum2);

	gCall[zCall].clear = 1;
	gCall[zCall].isCallInitiated = YES;

	dynVarLog (__LINE__, msg.appCallNum1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "data=(%s), appCallNum2=(%d).",
			   gCall[zCall].msgToDM.data, msg.appCallNum2);

	if (msg.appCallNum2 > -1 && msg.appCallNum2 < MAX_PORTS)
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearPort with 0", "LEG=",
					  gCall[zCall].leg);
		clearPort (msg.appCallNum2, mod, 1, 0);

		gCall[msg.appCallNum2].yDropTime = 0;
		gCall[msg.appCallNum2].yInitTime = 0;

		gCall[msg.appCallNum1].crossPort = msg.appCallNum2;
		gCall[msg.appCallNum2].crossPort = msg.appCallNum1;

		gCall[msg.appCallNum2].appPid = msg.appPid;
		gCall[msg.appCallNum2].appPassword = msg.appCallNum2;
	}
	else
	{
		if (gCall[zCall].msgToDM.data != NULL &&
			gCall[zCall].msgToDM.data[0] != '\0')
		{
			snprintf (gCall[zCall].ani, sizeof(gCall[zCall].ani), "%s", gCall[zCall].msgToDM.data);
		}
	}

// MR 4655
	if ( gCall[zCall].sendSavedInitiateCall == 1)
	{
		memcpy (&response, &gCall[zCall].respMsgInitiateCallToApp, sizeof (struct MsgToApp));
		sprintf(message, "op:%d,c#:%d,pid:%d,ref:%d,pw:%d", 
				response.opcode, 
				response.appCallNum, 
				response.appPid, 
				response.appRef, 
				response.appPassword);

		gCall[zCall].sendSavedInitiateCall = 0;
		dynVarLog (__LINE__, msg.appCallNum1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Send previous msg (%s) back to app.", message);
		writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		gCall[zCall].pendingOutputRequests++;
		return (0);
	}
	else if ( msg.appCallNum2 >= 0 ) 
	{ 
		if ( gCall[msg.appCallNum2].sendSavedInitiateCall == 1)
		{
			memcpy (&response, &gCall[msg.appCallNum2].respMsgInitiateCallToApp, sizeof (struct MsgToApp));
			gCall[msg.appCallNum2].sendSavedInitiateCall = 0;
			sprintf(message, "op:%d,c#:%d,pid:%d,ref:%d,pw:%d", 
				response.opcode, 
				response.appCallNum, 
				response.appPid, 
				response.appRef, 
				response.appPassword);
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Send previous bleg msg (%s) back to app.", message);
			writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			gCall[zCall].pendingOutputRequests++;
			return (0);
		}
	}
// END: MR 4655

	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;
	//sprintf(response.message, "%d", msg.appCallNum2);
	
	if (msg.appCallNum2 >= 0)
	{
		sprintf (response.message, "CALL=%d^AUDIO_CODEC=%s",
				 msg.appCallNum2, gCall[zCall].audioCodecString);
		sprintf (gCall[msg.appCallNum2].responseFifo, "%s",
				 gCall[zCall].responseFifo);
	}
	else
	{
		sprintf (response.message, "CALL=%d^AUDIO_CODEC=%s",
				 zCall, gCall[zCall].audioCodecString);
	}
	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			   "response msg set to (op:%d,c#:%d,ref:%d,pid:%d,msg:%s",
				response.opcode,
				response.appCallNum,
				response.appRef,
				response.appPid,
				response.message);

#if 0
// MR 4602
	if (gCall[zCall].GV_CallProgress == 4 && msg.appCallNum2 >= 0)
	{
		return (0);
	}
// END: MR 4602
#endif

	if (  gCall[zCall].GV_CallProgress != 4 || msg.appCallNum2 >= 0)
	{
		int bleg = msg.appCallNum2;

		writeGenericResponseToApp (zCall, &response, mod, __LINE__);


		if ( ( bleg >= 0 ) && 
		     ( gCall[bleg].callState == CALL_STATE_CALL_INITIATE_ISSUED || gCall[bleg].callState == CALL_STATE_IDLE )  )
		{
			;
		}
		else
		{ 
			gCall[zCall].pendingOutputRequests++;
		}

	}

	return (0);

}								/*END: int process_DMOP_INITIATECALL */

///This function is called when Media Manager receives a DMOP_LISTENCALL.
int
process_DMOP_LISTENCALL_INTERNAL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_LISTENCALL_INTERNAL" };
	struct MsgToApp response;
	struct Msg_ListenCall msg;

	response.message[0] = '\0';
	memcpy (&msg, &(gCall[zCall].msgToDM), sizeof (struct Msg_ListenCall));

	int             yBLeg = atoi (msg.phoneNumber);

	gCall[yBLeg].caleaPort = msg.appCallNum1;

	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;
	sprintf (response.message, "%s", msg.phoneNumber);

	writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	//setCallState(yBLeg, CALL_STATE_CALL_BRIDGED_CALEA, __LINE__);

	//setCallSubState(yBLeg, CALL_STATE_CALL_LISTENONLY_CALEA);
	gCall[yBLeg].isCalea = CALL_STATE_CALL_LISTENONLY_CALEA;

	return (0);

}								/*END: int process_DMOP_LISTENCALL_INTERNAL */

///This function is called when Media Manager receives a DMOP_LISTENCALL.
int
process_DMOP_LISTENCALL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_LISTENCALL" };
	struct MsgToApp response;
	struct Msg_InitiateCall msg;

	response.message[0] = '\0';
	memcpy (&msg, &(gCall[zCall].msgToDM), sizeof (struct Msg_ListenCall));

	if (msg.informat == INTERNAL_PORT)
	{
		return (process_DMOP_LISTENCALL_INTERNAL (zCall));
	}

	gCall[msg.appCallNum1].crossPort = msg.appCallNum2;
	gCall[msg.appCallNum2].crossPort = msg.appCallNum1;

	int             yBLeg = gCall[zCall].crossPort;

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "AppCallNum1", "", msg.appCallNum);
	__DDNDEBUG__ (DEBUG_MODULE_CALL, "AppCallNum2", "",
				  gCall[zCall].crossPort);

	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;
	sprintf (response.message, "%d", gCall[zCall].crossPort);

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yBLeg=%d", yBLeg);

	if (yBLeg >= 0)
	{
		gCall[yBLeg].leg = LEG_B;
		gCall[yBLeg].crossPort = zCall;

		sprintf (gCall[yBLeg].responseFifo, "%s", gCall[zCall].responseFifo);
		gCall[yBLeg].responseFifoFd = gCall[zCall].responseFifoFd;

		writeGenericResponseToApp (zCall, &response, mod, __LINE__);

		setCallState (zCall, CALL_STATE_CALL_BRIDGED, __LINE__);
		setCallSubState (zCall, CALL_STATE_CALL_LISTENONLY);

		setCallState (yBLeg, CALL_STATE_IDLE, __LINE__);

		gCall[yBLeg].yDropTime = 0;
		gCall[yBLeg].yInitTime = 0;
	}

	return (0);

}								/*END: int process_DMOP_LISTENCALL */

///This function is called when Media Manager receives a DMOP_BRIDGECONNECT.
int
process_DMOP_BRIDGECONNECT (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_BRIDGECONNECT" };
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;

	gCall[zCall].GV_BridgeRinging = 0;

	if (gCall[zCall].sendingSilencePackets != 1)
	{
		clearBkpSpeakList (zCall);
		gCall[zCall].pendingOutputRequests = 1;
	}

	int             yBLeg = gCall[zCall].crossPort;

	gCall[yBLeg].yDropTime = 0;
	gCall[yBLeg].yInitTime = 0;

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "AppCallNum1", "", msg.appCallNum);
	__DDNDEBUG__ (DEBUG_MODULE_CALL, "AppCallNum2", "", yBLeg);

	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;
	sprintf (response.message, "%d", yBLeg);

	if (yBLeg >= 0)
	{
		char yStrTmpBuffer[2] = "\0";
		gCall[yBLeg].leg = LEG_B;

		saveBridgeCallPacket (zCall, yStrTmpBuffer, -1, 0);	/* -1 for NO DTMF */

		sprintf (gCall[yBLeg].responseFifo, "%s", gCall[zCall].responseFifo);

		writeGenericResponseToApp (zCall, &response, mod, __LINE__);

		gCall[zCall].lastReadPointer = 0;
		gCall[zCall].lastWritePointer = 0;

		gCall[yBLeg].lastReadPointer = 0;
		gCall[yBLeg].lastWritePointer = 0;

		memset (gCall[zCall].bridgeCallData, 0,
				sizeof (struct BridgeCallData) * BRIDGE_DATA_ARRAY_SIZE);

		if (yBLeg > -1 && yBLeg < MAX_PORTS)
		{
			memset (gCall[yBLeg].bridgeCallData, 0,
					sizeof (struct BridgeCallData) * BRIDGE_DATA_ARRAY_SIZE);
		}

		setCallState (zCall, CALL_STATE_CALL_BRIDGED, __LINE__);
		setCallState (yBLeg, CALL_STATE_CALL_BRIDGED, __LINE__);
		gCallSetCrossRef (gCall, MAX_PORTS, zCall, &gCall[yBLeg], yBLeg);
		gCallSetCrossRef (gCall, MAX_PORTS, yBLeg, &gCall[zCall], zCall);
	}

	return (0);

}								/*END: int process_DMOP_BRIDGECONNECT */

#define ZERO_ALL_READ_WRITE_POINTERS(idx) \
do { \
   gCall[idx].lastReadPointer = 0; \
   gCall[idx].lastWritePointer = 0; \
} \
while (0)\
\

int
process_DMOP_BRIDGE_THIRD_LEG (int zCall)
{

	int             rc = -1;
	int             yRc = 0;
	char            mod[] = { "process_DMOP_BRIDGE_THIRD_LEG" };
	struct Msg_BridgeThirdLeg *msg =
		(struct Msg_BridgeThirdLeg *) &(gCall[zCall].msgToDM);
	struct MsgToApp response;
	int             legs[3] = { -1, -1, -1 };
	enum legs_e
	{ A = 0, B, C };

	gCall[zCall].yDropTime = 0;

	// sanity check on the zCall index coming from the calling app
	if (msg->appCallNum < 0 || msg->appCallNum > MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   " C-LEG zCall index not in range; zCall=%d, returning -1",
				   zCall);
		return -1;
	}

	if (msg->appCallNum1 >= 0 && msg->appCallNum1 < MAX_PORTS)
	{
		legs[A] = msg->appCallNum1;
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   " C-LEG zCall index not in range; zCall=%d, returning -1",
				   msg->appCallNum);
		return -1;
	}

	if (msg->appCallNum2 >= 0 && msg->appCallNum2 < MAX_PORTS)
	{
		legs[B] = msg->appCallNum2;
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   " C-LEG b-leg crossport not in range; bleg=%d, returning -1",
				   msg->appCallNum1);
		return -1;
	}

	if (msg->appCallNum >= 0 && msg->appCallNum < MAX_PORTS)
	{
		legs[C] = msg->appCallNum3;
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   " C-LEG c-leg crossport not in range; cleg=%d, returning -1",
				   msg->appCallNum2);
		return -1;
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			   " C-LEG call legs = a=%d b=%d/c=%d ", legs[A], legs[B],
			   legs[C]);

	sprintf (gCall[legs[B]].responseFifo, "%s", gCall[zCall].responseFifo);
	sprintf (gCall[legs[C]].responseFifo, "%s", gCall[zCall].responseFifo);

	gCall[legs[B]].responseFifoFd = gCall[zCall].responseFifoFd;
	gCall[legs[C]].responseFifoFd = gCall[zCall].responseFifoFd;

	setCallState (legs[A], CALL_STATE_IDLE, __LINE__);
	setCallSubState (legs[A], CALL_STATE_IDLE);
	setCallState (legs[B], CALL_STATE_CALL_BRIDGED, __LINE__);
	setCallState (legs[C], CALL_STATE_CALL_BRIDGED, __LINE__);

	ZERO_ALL_READ_WRITE_POINTERS (legs[C]);
	gCall[legs[B]].crossPort = legs[C];
	ZERO_ALL_READ_WRITE_POINTERS (legs[B]);
	gCall[legs[C]].crossPort = legs[B];

	//rtp_session_reset(gCall[legs[C]].rtp_session_in);
	//rtp_session_reset(gCall[legs[B]].rtp_session_in);
	gCall[legs[C]].resetRtpSession = 1;
	gCall[legs[B]].resetRtpSession = 1;

	if (msg->appCallNum != msg->appCallNum1)
	{
		gCall[msg->appCallNum1].thirdParty = 1;
	}

	if (msg->appCallNum != msg->appCallNum2)
	{
		gCall[msg->appCallNum2].thirdParty = 1;
	}

	if (msg->appCallNum != msg->appCallNum3)
	{
		gCall[msg->appCallNum3].thirdParty = 1;
	}

	response.opcode = msg->opcode;
	response.appCallNum = msg->appCallNum;
	response.appPid = msg->appPid;
	response.appRef = msg->appRef;
	response.returnCode = 0;
	response.appPassword = msg->appPassword;

	sprintf (response.message, "^%d^%d^%d", legs[A], legs[B], legs[C]);

	writeGenericResponseToApp (msg->appCallNum, &response, mod, __LINE__);

#if 0
	/*If set to 99; b or c leg were disconnected */
	if (msg->whichOne == 99)
	{
		response.appRef = msg->appRef - 1;
		response.opcode = DMOP_BRIDGECONNECT;
		response.returnCode = -10;
		writeGenericResponseToApp (msg->appCallNum, &response, mod, __LINE__);
	}
#endif

	return rc;
}

///This function is called when Media Manager receives a DMOP_OUTPUTDTMF.
int
process_DMOP_OUTPUTDTMF (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_OUTPUTDTMF" };
	struct MsgToDM  msg = gCall[zCall].msgToDM;

	struct Msg_Speak yBeepMsg;
	long int        yIntQueueElement = -1;
	struct Msg_OutputDTMF yMsgOutputDtmf;

	memcpy (&(yBeepMsg), &(gCall[zCall].msgToDM), sizeof (struct Msg_Speak));
	memcpy (&(yMsgOutputDtmf), &(gCall[zCall].msgToDM),
			sizeof (struct Msg_OutputDTMF));

	yBeepMsg.opcode = DMOP_OUTPUTDTMF;
	yBeepMsg.synchronous = SYNC;
	yBeepMsg.list = 0;
	yBeepMsg.allParties = 0;
	yBeepMsg.key[0] = '\0';
	yBeepMsg.interruptOption = 0;

	sprintf (yBeepMsg.file, "dtmf://%s", yMsgOutputDtmf.dtmf_string);

	if (gSipOutputDtmfMethod == OUTPUT_DTMF_INBAND)
	{
	char            yStrInbandList[256];
	char            yStrInbandListData[256] = "\0";
	FILE           *fp = NULL;

		sprintf (yStrInbandList, ".inband.%d", zCall);

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "OUTPUT_DTMF_INBAND",
					  yStrInbandList, zCall);

		if ((fp = fopen (yStrInbandList, "w+")) == NULL)
		{
	MsgToApp        response;

	struct MsgToDM  msg = gCall[zCall].msgToDM;

			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, DYN_BASE, ERR,
					   "Failed to open file (%s), [%d, %s].",
					   yStrInbandList, errno, strerror (errno));

			response.opcode = DMOP_OUTPUTDTMF;
			response.appCallNum = msg.appCallNum;
			response.appPid = msg.appPid;
			response.appRef = msg.appRef;
			response.returnCode = -7;
			response.appPassword = msg.appPassword;

			sprintf (response.message, "\0");

			yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

			return (yRc);
		}

		for (int i = 0; yMsgOutputDtmf.dtmf_string[i]; i++)
		{
			yStrInbandListData[0] = 0;

			switch (yMsgOutputDtmf.dtmf_string[i])
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case 'A':
			case 'B':
			case 'C':
			case 'D':
				sprintf (yStrInbandListData, "%s/%s/inband_%c.wav",
						 gSystemPhraseRoot, gSystemLanguage,
						 yMsgOutputDtmf.dtmf_string[i]);
				break;
			case ',':
				sprintf (yStrInbandListData, "%s/%s/inband_comma.wav",
						 gSystemPhraseRoot, gSystemLanguage,
						 yMsgOutputDtmf.dtmf_string[i]);
				break;
			case '*':
				sprintf (yStrInbandListData, "%s/%s/inband_star.wav",
						 gSystemPhraseRoot, gSystemLanguage,
						 yMsgOutputDtmf.dtmf_string[i]);
				break;
			case '#':
				sprintf (yStrInbandListData, "%s/%s/inband_pound.wav",
						 gSystemPhraseRoot, gSystemLanguage,
						 yMsgOutputDtmf.dtmf_string[i]);
				break;
			default:

				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, DYN_BASE, ERR,
						   "INBAND DTMF: Failed to open system inband tone file for (%c). Ignoring.",
						   yMsgOutputDtmf.dtmf_string[i]);
				break;
			}

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
						  "OUTPUT_DTMF_INBAND: Adding file",
						  yStrInbandListData, zCall);

			if (access (yStrInbandListData, R_OK) != 0)
			{
				/*All tone files should be present */
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, DYN_BASE, ERR,
						   "INBAND DTMF: Failed to open system inband tone file (%s), [%d, %s].",
						   yStrInbandListData, errno, strerror (errno));

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
							  "OUTPUT_DTMF_INBAND: Open file failed",
							  yStrInbandListData, zCall);

			}

			fprintf (fp, "%s\n", yStrInbandListData);

		}						/*END: for */

		fclose (fp);

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
					  "Creating list for OUTPUT_DTMF_INBAND", yStrInbandList,
					  zCall);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "INBAND DTMF: Creating list for OUTPUT_DTMF_INBAND (%s)",
				   yStrInbandList);

		yBeepMsg.list = 1;

		sprintf (yBeepMsg.file, "dtmf://%s", yStrInbandList);
	}
	else
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "OUTPUT_DTMF_RFC_2833", "", zCall);
	}

	__DDNDEBUG__ (DEBUG_MODULE_SR, "Calling addToAppRequestList", "", zCall);

	addToAppRequestList ((struct MsgToDM *) &(yBeepMsg));

	gCall[zCall].pendingOutputRequests++;

	return (0);

}								/*END: int process_DMOP_OUTPUTDTMF */

int
process_DMOP_CONFERENCESTART (int zCall)
{
	int             yRc = -1;
	int             idx = -1;
	char            mod[] = "process_DMOP_CONFERENCESTART";
	int             send_response = 0;

	MSG_CONF_START *yTmpConferenceStart =
		(MSG_CONF_START *) & (gCall[zCall].msgToDM);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing process_DMOP_CONFERENCESTART for ID (%s).",
			   yTmpConferenceStart->confId);

	/*Step 1: Allow only one conf */
	if (gCall[zCall].conferenceStarted == 1)
	{
	MsgToApp        response;

	struct MsgToDM  msg = gCall[zCall].msgToDM;

		response.opcode = DMOP_CONFERENCE_START;
		response.appCallNum = msg.appCallNum;
		response.appPid = msg.appPid;
		response.appRef = msg.appRef;
		response.returnCode = -1;
		response.appPassword = msg.appPassword;

		sprintf (response.message, "error.conference.duplicateconference\0");

		yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

		return (yRc);
	}

#ifdef ACU_LINUX
	/*Step 1: Send it to Conf Mgr */
	yRc = sendRequestToConfMgr (mod, &(gCall[zCall].msgToDM));
#else
	idx =
		arc_conference_create_by_name (zCall, Conferences, 48,
									   yTmpConferenceStart->confId);

	if (idx == -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   " %s() failed to create conference for ID (%s)", __func__,
				   yTmpConferenceStart->confId);
		// fprintf(stderr, " %s() failed x to create conference with id[%s]\n", __func__, yTmpConferenceStart->confId);
		yRc = idx;
	}
	else
	{
		yRc = 0;
		send_response++;
	}
#endif

	if (yRc < 0)
	{
	MsgToApp        response;

		response.opcode = DMOP_CONFERENCE_START;
		response.appCallNum = zCall;
		response.appPid = yTmpConferenceStart->appPid;
		response.appRef = yTmpConferenceStart->appRef;
		response.returnCode = -1;
		response.appPassword = yTmpConferenceStart->appPassword;
		sprintf (response.message, "error.conference.duplicateconference");
		yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	if (send_response)
	{
	MsgToApp        response;

		response.opcode = DMOP_CONFERENCE_START;
		response.appCallNum = zCall;
		response.appPid = yTmpConferenceStart->appPid;
		response.appRef = yTmpConferenceStart->appRef;
		response.returnCode = 0;
		// response.appPassword  = yTmpConferenceStart->appPasswd;
		sprintf (response.message, "200 OK");
		yRc =
			writeGenericResponseToApp (zCall, &response, (char *) __func__,
									   __LINE__);
	}

	return yRc;
}

int
process_DMOP_INSERTDTMF (int zCall)
{
	int             yRc = 0;
	char            mod[] = "process_DMOP_INSERTDTMF";
	MsgToApp        response;

	MSG_INSERT_DTMF *yTmpInsertDtmf =
		(MSG_INSERT_DTMF *) & (gCall[zCall].msgToDM);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing process_DMOP_INSERTDTMF for ID (%s), portNum=%d",
			   yTmpInsertDtmf->confId, yTmpInsertDtmf->portNum);

	if (yTmpInsertDtmf->portNum < 0 && yTmpInsertDtmf->portNum > MAX_PORTS)
	{
		yRc = -1;
	}
	else
	{
		if (!canContinue (mod, yTmpInsertDtmf->portNum, __LINE__))
		{
			yRc = -1;
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting dtmfAvailable portNum=%d dtmf=%d.",
					   yTmpInsertDtmf->portNum, yTmpInsertDtmf->dtmf);
			gCall[yTmpInsertDtmf->portNum].dtmfAvailable = 1;
			gCall[yTmpInsertDtmf->portNum].lastDtmf =
				atoi (yTmpInsertDtmf->dtmf);
		}
	}

	if (yTmpInsertDtmf->sendResp == 1)
	{

		response.opcode = DMOP_INSERTDTMF;
		response.appPid = yTmpInsertDtmf->appPid;
		response.appRef = yTmpInsertDtmf->appRef;
		response.returnCode = yRc;
		response.appPassword = yTmpInsertDtmf->appPassword;

		sprintf (response.message, "\0");

		/*Step 1: Send the response back to the app */
		yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	return yRc;
}

int
process_DMOP_CONFERENCEADDUSER (int zCall)
{
	int             yRc = -1;
	char            mod[] = "process_DMOP_CONFERENCEADDUSER";
	int             send_response = 0;

	MSG_CONF_ADD_USER *yTmpConfAddUser =
		(MSG_CONF_ADD_USER *) & (gCall[zCall].msgToDM);

	if (gCall[zCall].conferenceStarted == 1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF: User is already added not doing anything.");
	MsgToApp        response;

		response.opcode = DMOP_CONFERENCE_ADD_USER;
		response.appCallNum = zCall;
		response.appPid = yTmpConfAddUser->appPid;
		response.appRef = yTmpConfAddUser->appRef;
		response.returnCode = -1;
		response.appPassword = yTmpConfAddUser->appPasswd;
		sprintf (response.message, "error.conference.duplicateuser");
		yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		return yRc;
	}

	sprintf (gCall[zCall].confID, "%s", yTmpConfAddUser->confId);
#if 0
	yTmpConfAddUser->rtpPort = LOCAL_STARTING_CONFERENCE_RTP + (2 * zCall);
#endif
	yTmpConfAddUser->rtpPort = gCall[zCall].remoteRtpPort;
	sprintf (yTmpConfAddUser->remoteIP, "%s", gCall[zCall].remoteRtpIp);

	gCall[zCall].confRtpLocalPort = yTmpConfAddUser->rtpPort;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF ADD USER: sending local port=%d and beepFile=%s, isBeep=%d, confId=%s.",
			   yTmpConfAddUser->rtpPort, yTmpConfAddUser->beepFile,
			   yTmpConfAddUser->isBeep, yTmpConfAddUser->confId);

	if (yTmpConfAddUser->isBeep == NO)
	{
		memset (yTmpConfAddUser->beepFile, 0,
				sizeof (yTmpConfAddUser->beepFile));
	}

	switch (yTmpConfAddUser->duplex)
	{
	case RECVONLY:
		gCall[zCall].sendConfData = NO;
		gCall[zCall].recvConfData = YES;
		break;
	case SENDONLY:
		gCall[zCall].sendConfData = YES;
		gCall[zCall].recvConfData = NO;
		break;
	case SENDRECV:
	default:
		gCall[zCall].recvConfData = YES;
		gCall[zCall].sendConfData = YES;
		break;
	}

#ifdef ACU_LINUX
	/*Step 1: Send it to Conf Mgr */
	yRc = sendRequestToConfMgr (mod, &(gCall[zCall].msgToDM));

#else
	yRc =
		arc_conference_addref_by_name (Conferences, 48, zCall,
									   gCall[zCall].confID);
	arc_clock_sync_add (clock_sync_in, &gCall[zCall], ARC_CLOCK_SYNC_READ);
	arc_clock_sync_add (clock_sync_in, &gCall[zCall], ARC_CLOCK_SYNC_WRITE);

	if (yRc == -1)
	{
		;;						// fprintf(stderr, " %s() failed to addref for conference with id [%s]\n", __func__, gCall[zCall].confID);
	}
	else
	{
		send_response++;
	}
	// send phony message to app back to app 
#endif
	if (yRc < 0)
	{
	MsgToApp        response;

		response.opcode = DMOP_CONFERENCE_ADD_USER;
		response.appCallNum = zCall;
		response.appPid = yTmpConfAddUser->appPid;
		response.appRef = yTmpConfAddUser->appRef;
		response.returnCode = -1;
		response.appPassword = yTmpConfAddUser->appPasswd;
		sprintf (response.message, "Failed to Add User.");
		yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	if (send_response)
	{
	MsgToApp        response;

		response.opcode = DMOP_CONFERENCE_ADD_USER;
		response.appCallNum = zCall;
		response.appPid = yTmpConfAddUser->appPid;
		response.appRef = yTmpConfAddUser->appRef;
		response.returnCode = 0;
		response.appPassword = yTmpConfAddUser->appPasswd;
		sprintf (response.message, "200 OK");
		yRc =
			writeGenericResponseToApp (zCall, &response, (char *) __func__,
									   __LINE__);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF:AddUser confID=%s, portnum=%d.", gCall[zCall].confID,
			   zCall);

	return yRc;
}

int
process_DMOP_CONFERENCEREMOVEUSER (int zCall)
{
	int             yRc = -1;
	int             send_response = 0;
	char           *mod = (char *) __func__;
	MsgToApp        response;

	MSG_CONF_REMOVE_USER *yTmpConfRemoveUser =
		(MSG_CONF_REMOVE_USER *) & (gCall[zCall].msgToDM);

	memset((MsgToApp *)&response, '\0', sizeof(response));

	if (gCall[zCall].conferenceStarted == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF: User is already removed not doing anything.");

		response.opcode = DMOP_CONFERENCE_REMOVE_USER;
		response.appCallNum = zCall;
		response.appPid = yTmpConfRemoveUser->appPid;
		response.appRef = yTmpConfRemoveUser->appRef;
		response.returnCode = -1;
		response.appPassword = yTmpConfRemoveUser->appPasswd;
		sprintf (response.message, "error.conference.invaliduser");
		yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		return 0;
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: Calling arc_conference_delref_by_name() with confID [%s].",
			   gCall[zCall].confID);

	gCall[zCall].conferenceStarted = 0;
	gCall[zCall].lastOutRtpTime.time = 0;

#ifdef ACU_LINUX
	/*Step 1: Send it to Conf Mgr */
	yRc = sendRequestToConfMgr (mod, &(gCall[zCall].msgToDM));
    if ( yRc == -1 )
    {
        sprintf (response.message,
                 "Failed to send request to ConfMgr, check if ArcConferenceMgr is running.");
    }   
#else
	arc_clock_sync_broadcast (clock_sync_in, &gCall[zCall],
							  ARC_CLOCK_SYNC_READ);
	arc_clock_sync_broadcast (clock_sync_in, &gCall[zCall],
							  ARC_CLOCK_SYNC_WRITE);
	yRc =
		arc_conference_delref_by_name (Conferences, 48, zCall,
									   gCall[zCall].confID);
	arc_clock_sync_del (clock_sync_in, &gCall[zCall]);
	arc_clock_sync_del (clock_sync_in, &gCall[zCall]);
	if (yRc == -1)
	{
        sprintf (response.message,
                   "CONF: failed to delete ref from conference with ID=[%s].",
                   gCall[zCall].confID, zCall);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF: failed to delete ref from conference with ID=[%s].",
				   gCall[zCall].confID, zCall);
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF: deleted ref from conference with ID=[%s], yRc=%d",
				   gCall[zCall].confID, yRc);
		send_response++;
	}
#endif

	if (yRc < 0)
	{
		response.opcode = DMOP_CONFERENCE_REMOVE_USER;
		response.appCallNum = zCall;
		response.appPid = yTmpConfRemoveUser->appPid;
		response.appRef = yTmpConfRemoveUser->appRef;
		response.returnCode = -1;
		response.appPassword = yTmpConfRemoveUser->appPasswd;
		sprintf (response.message,
				 "Failed to send request to ConfMgr, check if ArcConferenceMgr is running.");
		yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: Setting conferenceStarted to 0.");
	gCall[zCall].conferenceStarted = 0;
	gCall[zCall].conferenceIdx = -1;
	//gCall[zCall].confID[0] = '\0';

	if (send_response)
	{
		response.opcode = DMOP_CONFERENCE_REMOVE_USER;
		response.appCallNum = zCall;
		response.appPid = yTmpConfRemoveUser->appPid;
		response.appRef = yTmpConfRemoveUser->appRef;
		response.returnCode = 0;
		response.appPassword = yTmpConfRemoveUser->appPasswd;
		sprintf (response.message, "200 OK");
		yRc =
			writeGenericResponseToApp (zCall, &response, (char *) __func__,
									   __LINE__);
	}

	return yRc;
}

int
process_DMOP_CONFERENCESTOP (int zCall)
{
	int             yRc = -1;
	int             rc = -1;
	char            mod[] = "";
	int             send_response = 0;
	MSG_CONF_STOP  *yTmpConferenceStop =
		(MSG_CONF_STOP *) & (gCall[zCall].msgToDM);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: calling arc_conference_delete_by_name with ID=[%s].",
			   gCall[zCall].confID);

#ifdef ACU_LINUX
	/*Step 1: Send it to Conf Mgr */
	yRc = sendRequestToConfMgr (mod, &(gCall[zCall].msgToDM));
#else
	yRc =
		arc_conference_delete_by_name (zCall, Conferences, 48,
									   yTmpConferenceStop->confId);

	if (yRc == -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "CONF: failed to delete conference with ID=[%s].",
				   yTmpConferenceStop->confId);
	}
	else
	{
		send_response++;
	}
#endif

	if (yRc < 0)
	{
	MsgToApp        response;

		response.opcode = DMOP_CONFERENCE_STOP;
		response.appCallNum = zCall;
		response.appPid = yTmpConferenceStop->appPid;
		response.appRef = yTmpConferenceStop->appRef;
		response.returnCode = -1;
		response.appPassword = yTmpConferenceStop->appPassword;
		sprintf (response.message, "error.conference.invalidconference");
		yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	gCall[zCall].conferenceStarted = 0;

	if (strcmp (yTmpConferenceStop->confId, gCall[zCall].confID) == 0)
	{
		memset (gCall[zCall].confID, 0, sizeof (gCall[zCall].confID));
		gCall[zCall].confUserCount = 0;

		for (int i = 0; i < MAX_CONF_PORTS; i++)
		{
			gCall[zCall].conf_ports[i] = -1;
		}
	}

	if (send_response)
	{
	MsgToApp        response;

		response.opcode = DMOP_CONFERENCE_STOP;
		response.appCallNum = zCall;
		response.appPid = yTmpConferenceStop->appPid;
		response.appRef = yTmpConferenceStop->appRef;
		response.returnCode = 0;
		// response.appPassword  = yTmpConferenceStop->appPasswd;
		sprintf (response.message, "200 OK");
		yRc =
			writeGenericResponseToApp (zCall, &response, (char *) __func__,
									   __LINE__);
	}

	return yRc;

}								/* END: int process_DMOP_CONFERENCESTOP */

int
process_DMOP_CONFERENCE_PLAYAUDIO (int zCall)
{
	int             yRc = -1;
	char            mod[] = "process_DMOP_CONFERENCE_PLAYAUDIO";

	MSG_CONF_PLAY_AUDIO *yTmpConferencePlayAudio =
		(MSG_CONF_PLAY_AUDIO *) & (gCall[zCall].msgToDM);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing process_DMOP_CONFERENCE_PLAYAUDIO for ID (%s).",
			   yTmpConferencePlayAudio->confId);

	/*Step 1: Send it to Conf Mgr */

#ifdef ACU_LINUX
	yRc = sendRequestToConfMgr (mod, &(gCall[zCall].msgToDM));
//#else
//	fprintf (stderr, " %s() not implemented \n", __func__);
#endif

	if (yRc < 0)
	{
	MsgToApp        response;

		response.opcode = DMOP_CONFERENCE_PLAY_AUDIO;
		response.appCallNum = zCall;
		response.appPid = yTmpConferencePlayAudio->appPid;
		response.appRef = yTmpConferencePlayAudio->appRef;
		response.returnCode = -1;
		response.appPassword = yTmpConferencePlayAudio->appPassword;
		sprintf (response.message,
				 "Failed to send request to ConfMgr, check if ArcConferenceMgr is running.");
		yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	return yRc;

}								/*END: int process_DMOP_CONFERENCE_PLAYAUDIO */

int
process_DMOP_CONFERENCESTART_RESPONSE (int zCall)
{
	int             yRc = -1;
	char            mod[] = "process_DMOP_CONFERENCESTART_RESPONSE";
	MsgToApp        response;
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	MSG_CONF_START_RESPONSE *yTmpConferenceStartResponse =
		(MSG_CONF_START_RESPONSE *) & (gCall[zCall].msgToDM);

	response.opcode = DMOP_CONFERENCE_START;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = yTmpConferenceStartResponse->resultCode;
	response.appPassword = msg.appPassword;

	sprintf (response.message, "\0");
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: resultCode for MSG_CONF_START_RESPONSE=%d.",
			   yTmpConferenceStartResponse->resultCode);

	if (yTmpConferenceStartResponse->resultCode == -1)
	{
		sprintf (response.message, "error.conference.duplicateconference\0");
	}

	/*Step 1: Send the response back to the app */
	yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	return yRc;

}								/*END: int process_DMOP_CONFERENCESTART_RESPONSE */

int
process_DMOP_CONFERENCEADDUSER_RESPONSE (int zCall)
{
	int             yRc = -1;
	char            mod[] = "process_DMOP_CONFERENCEADDUSER_RESPONSE";
	MsgToApp        response;
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	MSG_CONF_ADD_USER_RESPONSE *yTmpConfAddUserResponse =
		(MSG_CONF_ADD_USER_RESPONSE *) & (gCall[zCall].msgToDM);

	response.opcode = DMOP_CONFERENCE_ADD_USER;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = yTmpConfAddUserResponse->resultCode;
	response.appPassword = msg.appPassword;

	sprintf (response.message, "\0");

//  sprintf(gCall[zCall].confID, "%s", yTmpConfAddUserResponse->confId);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: confID=%s, portnum=%d, resultCode=%d.",
			   gCall[zCall].confID, zCall,
			   yTmpConfAddUserResponse->resultCode);

	if (yTmpConfAddUserResponse->resultCode >= 0)
	{

		for (int i = 0; i < MAX_PORTS; i++)
		{
			if (strcmp (gCall[zCall].confID, gCall[i].confID) == 0)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "CONF: confID=%s, portnum=%d, confUserCount=%d.",
						   gCall[i].confID, i, gCall[i].confUserCount);
				if (gCall[i].confUserCount < MAX_CONF_PORTS)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "CONF: confID=%s, portnum=%d, confUserCount=%d.",
							   gCall[i].confID, i, gCall[i].confUserCount);
					gCall[i].conf_ports[gCall[i].confUserCount] = zCall;
					gCall[i].confUserCount++;
					gCall[zCall].confUserCount = gCall[i].confUserCount;
				}
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "CONF: confID=%s, portnum=%d, confUserCount=%d.",
						   gCall[i].confID, i, gCall[i].confUserCount);
			}
		}

		sprintf (response.message, "%d", gCall[zCall].confUserCount);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: resultCode for MSG_CONF_ADD_USER_RESPONSE =%d.",
			   yTmpConfAddUserResponse->resultCode);

	if (yTmpConfAddUserResponse->resultCode == -1)
	{
		sprintf (response.message, "%s",
				 "error.conference.invalidconference");
	}

	/*Step 1: Send the response back to the app */
	yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	if (yTmpConfAddUserResponse->resultCode < 0)
	{
		gCall[zCall].conferenceStarted = 0;
		return (yRc);
	}

	/*If add user was successful, start sending/receiving the RTP data */

	//1 Save remote rtp port
	gCall[zCall].confRtpRemotePort = yTmpConfAddUserResponse->rtpPort;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: confRtpRemotePort=%d.", gCall[zCall].confRtpRemotePort);

	if (gCall[zCall].rtp_session_conf_send)
	{
		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_conf_send);
		gCall[zCall].rtp_session_conf_send = NULL;
	}

	gCall[zCall].rtp_session_conf_send =
		rtp_session_new (RTP_SESSION_SENDONLY);

	if (!gCall[zCall].rtp_session_conf_send)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE,
				   TEL_RTP_ERROR,
				   "CONF: Failed to create outbound conf rtp ");

		gCall[zCall].conferenceStarted = 0;
		return (-1);
	}

	rtp_session_set_blocking_mode (gCall[zCall].rtp_session_conf_send, 0);

	rtp_session_set_ssrc (gCall[zCall].rtp_session_conf_send,
						  atoi ("3197096732"));

	rtp_session_set_profile (gCall[zCall].rtp_session_conf_send, &av_profile);

	rtp_session_set_remote_addr (gCall[zCall].rtp_session_conf_send, gHostIp,
								 gCall[zCall].confRtpRemotePort, gHostIf);

	rtp_session_set_payload_type (gCall[zCall].rtp_session_conf_send,
								  gCall[zCall].codecType);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: Created outbound conf rtp session for (%s:%d).",
			   gHostIp, gCall[zCall].confRtpRemotePort);
#if 0
	if (!gCall[zCall].rtp_session_conf_recv)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
				   "CONF: Resuing inbound conf rtp session.");

		gCall[zCall].rtp_session_conf_recv =
			rtp_session_new (RTP_SESSION_RECVONLY);

		rtp_session_set_blocking_mode (gCall[zCall].rtp_session_conf_recv, 0);

		dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_DETAIL, TEL_BASE,
				   INFO,
				   "Calling rtp_session_set_local_addr with localSdpPort=%d.",
				   gCall[zCall].confRtpLocalPort);

		yRc = rtp_session_set_local_addr (gCall[zCall].rtp_session_conf_recv,
										  gHostIp,
										  gCall[zCall].confRtpLocalPort);

		if (yRc == -1)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
					   "Failed to set local address to %d.",
					   gCall[zCall].confRtpLocalPort);

			if (gCall[zCall].rtp_session_conf_recv != NULL)
			{
			}

			rtp_session_destroy (gCall[zCall].rtp_session_conf_recv);	/*Destroy all the way */

			gCall[zCall].rtp_session_conf_recv = NULL;

			gCall[zCall].conferenceStarted = 0;
			return -1;
		}

//      rtp_session_set_payload_type(gCall[zCall].rtp_session_conf_recv,
//          yPayloadType);

		rtp_session_set_profile (gCall[zCall].rtp_session_conf_recv,
								 &av_profile_array_in[zCall]);

	}
#endif

	if (!gCall[zCall].rtp_session_conf_send)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
				   "CONF: Failed to create inbound conf rtp. "
				   "Destroying outbound session.");

		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_conf_send);

		gCall[zCall].rtp_session_conf_send = NULL;

		return (-1);
	}

	yRc =
		rtp_session_set_local_addr (gCall[zCall].rtp_session_conf_send,
									gHostIp, gCall[zCall].confRtpLocalPort,
									gHostIf);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Setting conferenceStarted to 1.");
	gCall[zCall].conferenceStarted = 1;

	return yRc;

}								/*END:  int process_DMOP_CONFERENCEADDUSER_RESPONSE */

int
process_DMOP_CONFERENCEREMOVEUSER_RESPONSE (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";
	MsgToApp        response;
	struct MsgToDM  msg = gCall[zCall].msgToDM;

	MSG_CONF_REMOVE_USER_RESPONSE *yTmpConfRemoveUserResponse =
		(MSG_CONF_REMOVE_USER_RESPONSE *) & (gCall[zCall].msgToDM);

	response.opcode = DMOP_CONFERENCE_REMOVE_USER;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = yTmpConfRemoveUserResponse->resultCode;
	response.appPassword = msg.appPassword;

	sprintf (response.message, "\0");
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: resultCode=%d.", response.returnCode);

	for (int i = 0; i < MAX_PORTS; i++)
	{
		if (strcmp (gCall[zCall].confID, gCall[i].confID) == 0)
		{
			if (gCall[i].confUserCount < MAX_CONF_PORTS)
			{
				gCall[i].conf_ports[gCall[i].confUserCount] = -1;
				gCall[i].confUserCount--;
				if (gCall[i].confUserCount < 0)
				{
					gCall[i].confUserCount = 0;
				}
			}
		}
	}

	gCall[zCall].confUserCount = 0;

	memset (gCall[zCall].confID, 0, sizeof (gCall[zCall].confID));
	sprintf (response.message, "%d", gCall[zCall].confUserCount);

	/*Step 1: Send the response back to the app */
	yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	if (gCall[zCall].rtp_session_conf_send)
	{
		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_conf_send);
		gCall[zCall].rtp_session_conf_send = NULL;
	}

	if (gCall[zCall].rtp_session_conf_recv)
	{
		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_conf_recv);
		gCall[zCall].rtp_session_conf_recv = NULL;
	}

	return yRc;

}								/*END: int process_DMOP_CONFERENCEREMOVEUSER_RESPONSE */

int
process_DMOP_CONFERENCESTOP_RESPONSE (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";
	MsgToApp        response;
	struct MsgToDM  msg = gCall[zCall].msgToDM;

	MSG_CONF_STOP_RESPONSE *yTmpConferenceStopResponse =
		(MSG_CONF_STOP_RESPONSE *) & (gCall[zCall].msgToDM);

	response.opcode = DMOP_CONFERENCE_STOP;
	response.appCallNum = msg.appCallNum;
	response.appPid = gCall[zCall].appPid;
	response.appRef = msg.appRef;
	response.returnCode = yTmpConferenceStopResponse->resultCode;
	response.appPassword = msg.appPassword;

	sprintf (response.message, "\0");
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: resultCode=%d.", response.returnCode);

	/*Step 1: Send the response back to the app */
	yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	gCall[zCall].conferenceStarted = 0;

	if (gCall[zCall].rtp_session_conf_send)
	{
		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_conf_send);
		gCall[zCall].rtp_session_conf_send = NULL;
	}

	if (gCall[zCall].rtp_session_conf_recv)
	{
		arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_conf_recv);
		gCall[zCall].rtp_session_conf_recv = NULL;
	}

	return yRc;

}								/*END: int process_DMOP_CONFERENCESTOP_RESPONSE */

int
process_DMOP_CONFERENCE_PLAYAUDIO_RESPONSE (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";
	MsgToApp        response;
	struct MsgToDM  msg = gCall[zCall].msgToDM;

	MSG_CONF_PLAYAUDIO_RESPONSE *yTmpConferencePlayAudioResponse =
		(MSG_CONF_PLAYAUDIO_RESPONSE *) & (gCall[zCall].msgToDM);

	response.opcode = DMOP_CONFERENCE_PLAY_AUDIO;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = yTmpConferencePlayAudioResponse->resultCode;
	response.appPassword = msg.appPassword;

	sprintf (response.message, "\0");
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "CONF: resultCode=%d.", response.returnCode);

	/*Step 1: Send the response back to the app */
	yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	return yRc;

}								/*END: int process_DMOP_CONFERENCE_PLAYAUDIO_RESPONSE */

int
getQueueStatus (int zCall)
{
	char            mod[] = "getQueueStatus";
	int             yRc = 0;
	int             isAudioPresent = 0;

	if (gCall[zCall].pFirstSpeak != NULL)
	{
	SpeakList      *pTemp = NULL;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "pFirstSpeak is Not NULL, addr=0x%u",  gCall[zCall].pFirstSpeak );

		yRc = 1;

		if (pthread_mutex_trylock (&gCall[zCall].gMutexSpeak) == EBUSY)
		{
			return (0);
		}

		pTemp = gCall[zCall].pFirstSpeak;

		while (pTemp != NULL)
		{
			if (pTemp->isAudioPresent == 1 || pTemp->isMTtsPresent == 1)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "pTemp->msgSpeak.file=(%s)",
						   pTemp->msgSpeak.file);
				isAudioPresent = 1;
			}

			pTemp = pTemp->nextp;

		}						/*END: while */

		pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);

	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "pFirstSpeak is NULL");
	}

	if (gCall[zCall].pFirstBkpSpeak != NULL)
	{
	SpeakList      *pTemp = NULL;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "pFirstBkpSpeak is Not NULL");
		yRc = 1;

		if (pthread_mutex_trylock (&gCall[zCall].gMutexBkpSpeak) == EBUSY)
		{
			return (0);
		}

		pTemp = gCall[zCall].pFirstBkpSpeak;

		while (pTemp != NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "pTemp->isAudioPresent=%d.", pTemp->isAudioPresent);

			if (pTemp->isAudioPresent == 1 || pTemp->isMTtsPresent == 1)

			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "pTemp->msgSpeak.file=(%s)",
						   pTemp->msgSpeak.file);
				isAudioPresent = 1;
			}

			pTemp = pTemp->nextp;

		}						/*END: while */

		pthread_mutex_unlock (&gCall[zCall].gMutexBkpSpeak);

	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "pBkpFirstSpeak is NULL");
	}

	if (isAudioPresent == 0 && yRc == 3)
	{
		yRc = 2;
	}

	return yRc;

}								/*END: int getQueueStatus(int zCall) */

///This function is called when Media Manager receives a DMOP_PORTOPERATION.
int
process_DMOP_PORTOPERATION (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_PORTOPERATION" };

	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct Msg_PortOperation yMsgPortOperation;
	struct MsgToApp response;
	int             yIntSendReply = 0;

	memcpy (&yMsgPortOperation,
			&(gCall[zCall].msgToDM), sizeof (struct Msg_PortOperation));

	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;
	sprintf (response.message, "\0");

	switch (yMsgPortOperation.operation)
	{
	case STOP_ACTIVITY:
	case STOP_FAX_TONE_ACTIVITY:
		{
			__DDNDEBUG__ (DEBUG_MODULE_SR, "calling addToAppRequestList", "",
						  yMsgPortOperation.callNum);
			addToAppRequestList (&(gCall[zCall].msgToDM));

			if ( ( yMsgPortOperation.operation == STOP_ACTIVITY ) &&
			     ( gCall[yMsgPortOperation.callNum].GV_LastPlayPostion == 1 ) )
			{
				errno=0;


				gCall[yMsgPortOperation.callNum].currentPhraseOffset_save = gCall[yMsgPortOperation.callNum].currentPhraseOffset;
				if (  gCall[yMsgPortOperation.callNum].currentSpeakFd > 0 ) 
				{
					if ((gCall[yMsgPortOperation.callNum].currentPhraseOffset =
									lseek(gCall[yMsgPortOperation.callNum].currentSpeakFd, 0, SEEK_CUR)) < 0)
					{
							gCall[yMsgPortOperation.callNum].currentPhraseOffset =
											gCall[yMsgPortOperation.callNum].currentPhraseOffset_save;
							dynVarLog (__LINE__, yMsgPortOperation.callNum, mod, REPORT_NORMAL, TEL_BASE, ERR,
									"lseek() of fd %d failed. Unable to get current "
									"offset.  [%d, %s]  Setting to previous value of %d.",
								gCall[yMsgPortOperation.callNum].currentSpeakFd, errno, strerror(errno),
								gCall[yMsgPortOperation.callNum].currentPhraseOffset);
					}
					else
					{
	            		dynVarLog (__LINE__, yMsgPortOperation.callNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
							"Current offset for file fd (%d) is %ld.", 
							gCall[yMsgPortOperation.callNum].currentSpeakFd,
							gCall[yMsgPortOperation.callNum].currentPhraseOffset);
					}
				}
			}
			dynVarLog (__LINE__, yMsgPortOperation.callNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Got STOP_ACTIVITY, sendingSilencePackets=%d, pendingOutputRequests=%d.",
					   gCall[yMsgPortOperation.callNum].sendingSilencePackets,
					   gCall[yMsgPortOperation.callNum].pendingOutputRequests);

			//if(gCall[yMsgPortOperation.callNum].sendingSilencePackets == 1)
			{
				clearSpeakList (yMsgPortOperation.callNum, __LINE__);
			}

			clearBkpSpeakList (yMsgPortOperation.callNum);
			gCall[yMsgPortOperation.callNum].pendingOutputRequests = 1;
			//gCall[yMsgPortOperation.callNum].pendingInputRequests++;
			sprintf (response.message, "0");
			if (yMsgPortOperation.operation == STOP_FAX_TONE_ACTIVITY)
			{
				yIntSendReply = 0;
			}
			else
			{
				yIntSendReply = 1;
			}
		}
		break;

	case GET_CONNECTION_STATUS:
		{
			sprintf (response.message, "%d",
					 isCallActive (mod, yMsgPortOperation.callNum, __LINE__));

			yIntSendReply = 1;
		}
		break;

	case CLEAR_DTMF:
		{
			dynVarLog (__LINE__, yMsgPortOperation.callNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting dtmfAvailable to 0.");
			gCall[yMsgPortOperation.callNum].dtmfAvailable = 0;
			yIntSendReply = 1;
		}
		break;

	case GET_FUNCTIONAL_STATUS:
		{
			sprintf (response.message, "%d", getFunctionalStatus (yMsgPortOperation.callNum));

			yIntSendReply = 1;
		}
		break;

	case WAIT_FOR_PLAY_END:
		{
			if (gCall[yMsgPortOperation.callNum].sendingSilencePackets == 1)
			{
				yIntSendReply = 1;
				gCall[yMsgPortOperation.callNum].waitForPlayEnd = 0;
			}
			else
			{
				memcpy (&(gCall[yMsgPortOperation.callNum].msgPortOperation),
						&response, sizeof (struct MsgToApp));

				yIntSendReply = 0;

				gCall[yMsgPortOperation.callNum].waitForPlayEnd = 1;
			}

		}
		break;

	case GET_QUEUE_STATUS:
		{
	int             yRc = 0;

			yRc = getQueueStatus (yMsgPortOperation.callNum);

			sprintf (response.message, "%d", yRc);

			yIntSendReply = 1;
		}
		break;

	default:
		response.returnCode = -1;

		sprintf (response.message,
				 "Invalid port operation(%d)", yMsgPortOperation);

	}							/*END: switch */

	if (yIntSendReply == 1)
	{
		writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	}

	return (yRc);

}								/*END: int process_DMOP_PORTOPERATION */


int googleStopActivity(int zCall)
{
	int		yRc = 0;
	char 	mod[] = { "googleStopActivity" };

	errno=0;

	gCall[zCall].currentPhraseOffset_save = gCall[zCall].currentPhraseOffset;
	if (  gCall[zCall].currentSpeakFd > 0 ) 
	{
		if ((gCall[zCall].currentPhraseOffset =
						lseek(gCall[zCall].currentSpeakFd, 0, SEEK_CUR)) < 0)
		{
				gCall[zCall].currentPhraseOffset =
								gCall[zCall].currentPhraseOffset_save;
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
						"lseek() of fd %d failed. Unable to get current "
						"offset.  [%d, %s]  Setting to previous value of %d.",
					gCall[zCall].currentSpeakFd, errno, strerror(errno),
					gCall[zCall].currentPhraseOffset);
		}
		else
		{
          		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				"Current offset for file fd (%d) is %ld.", 
				gCall[zCall].currentSpeakFd,
				gCall[zCall].currentPhraseOffset);
		}
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Got STOP_ACTIVITY, sendingSilencePackets=%d, pendingOutputRequests=%d.",
					   gCall[zCall].sendingSilencePackets,
					   gCall[zCall].pendingOutputRequests);

	clearSpeakList (zCall, __LINE__);

	clearBkpSpeakList (zCall);
	gCall[zCall].pendingOutputRequests = 1;

	return (0);

} // END: googleStopActivity()

///This function is called when Media Manager receives a DMOP_SETGLOBAL.
int
process_DMOP_SETGLOBAL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_SETGLOBAL" };

	struct MsgToApp response;

	struct Msg_SetGlobal yMsgSetGlobal;

	sprintf (response.message, "\0");

	memcpy (&yMsgSetGlobal,
			&(gCall[zCall].msgToDM), sizeof (struct Msg_SetGlobal));

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "%s [%s=%d]", mod, yMsgSetGlobal.name, yMsgSetGlobal.value);

	response.returnCode = 0;

	if (strcmp (yMsgSetGlobal.name, "$KILL_APP_GRACE_PERIOD") == 0)
	{
		gCall[zCall].GV_KillAppGracePeriod = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$PLAY_BACK_BEEP_INTERVAL") == 0)
	{
		gCall[zCall].GV_PlayBackBeepInterval = (yMsgSetGlobal.value);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "After setting GV_PlayBackBeepInterval to %d",
				   gCall[zCall].GV_PlayBackBeepInterval);
	}
	else if (strcmp (yMsgSetGlobal.name, "$MAX_PAUSE_TIME") == 0)
	{
		gCall[zCall].GV_MaxPauseTime = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$SR_DTMF_ONLY") == 0)
	{
		gCall[zCall].GV_SRDtmfOnly = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$CALL_PROGRESS") == 0)
	{
		gCall[zCall].GV_CallProgress = (yMsgSetGlobal.value);
	}
    else if (strcmp (yMsgSetGlobal.name, "$CALL_PROGRESS") == 0)
    {
        gCall[zCall].GV_CallProgress = (yMsgSetGlobal.value);
    }
    else if (strcmp (yMsgSetGlobal.name, "$PLAYBACK_AND_SPEECHREC") == 0)
    {
        gCall[zCall].GV_PlaybackAndSpeechRec = (yMsgSetGlobal.value);
    }
	else if (strcmp (yMsgSetGlobal.name, "$T38_FAX_VERSION") == 0)
	{
		gCall[zCall].GV_T38FaxVersion = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$T30_ERROR_CORRECTION") == 0)
	{
		gCall[zCall].GV_T30ErrorCorrection = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$FAX_VERSION") == 0)
	{
		gCall[zCall].GV_FaxDebug = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$T38_ENABLED") == 0)
	{
		gCall[zCall].GV_T38Enabled = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$T38_ERROR_CORRECTION_MODE") == 0)
	{
		gCall[zCall].GV_T38ErrorCorrectionMode = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$T38_ERROR_CORRECTION_SPAN") == 0)
	{
		gCall[zCall].GV_T38ErrorCorrectionSpan = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$T38_ERROR_CORRECTION_ENTRIES") ==
			 0)
	{
		gCall[zCall].GV_T38ErrorCorrectionEntries = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$RECORD_TERM_CHAR") == 0)
	{

		gCall[zCall].GV_RecordTermChar = (yMsgSetGlobal.value);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "After setting GV_RecordTermChar to %d.",
				   gCall[zCall].GV_RecordTermChar);
	}
	else if (strcmp (yMsgSetGlobal.name, "$INTERNAL_BRIDGE_RINGING") == 0)
	{

		gCall[zCall].GV_BridgeRinging = (yMsgSetGlobal.value);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "After setting GV_BridgeRinging to %d.",
				   gCall[zCall].GV_BridgeRinging);
		return yRc;
	}
	else if (strcmp (yMsgSetGlobal.name, "$VOLUME") == 0)
	{
		gCall[zCall].GV_Volume = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$SPEED") == 0)
	{
		gCall[zCall].GV_Speed = (yMsgSetGlobal.value);
	}
    else if (strcmp (yMsgSetGlobal.name, "$HIDE_DTMF") == 0)
    {
        gCall[zCall].GV_HideDTMF = (yMsgSetGlobal.value);
    }
	else if (strcmp (yMsgSetGlobal.name, "$FLUSH_DTMF_BUFFER") == 0)
	{
		gCall[zCall].GV_FlushDtmfBuffer = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$LAST_PLAY_POSITION") == 0)
	{
		gCall[zCall].GV_LastPlayPostion = (yMsgSetGlobal.value);
		if ( gCall[zCall].GV_LastPlayPostion == 0 ) 
		{
			gCall[zCall].currentPhraseOffset = -1;
			gCall[zCall].currentPhraseOffset_save = -1;
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Resetting current phrase offset to beginning.");
		}
	}
	else if (strcmp (yMsgSetGlobal.name, "$SKIP_TIME_IN_SECONDS") == 0)
	{
		if ( (yMsgSetGlobal.value) <= 0 )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_DATA_NOT_FOUND, WARN,
				"Invalid $SKIP_TIME_IN_SECONDS value (%d). Must be greater than 0.  GV_SkipTimeInSeconds will remain unchanged at %d.",
				(yMsgSetGlobal.value), gCall[zCall].GV_SkipTimeInSeconds);
		}
		else
		{
			gCall[zCall].GV_SkipTimeInSeconds = (yMsgSetGlobal.value);
		}
	}
	else if (strcmp (yMsgSetGlobal.name, "$GOOGLE_RECORD") == 0)
	{
		gCall[zCall].googleRecord = (yMsgSetGlobal.value);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Global var name (%s)=(%d).  [YES=%d NO=%d]",
				yMsgSetGlobal.name, gCall[zCall].googleRecord, YES, NO);
	}
	else
	{
#if 0
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
				   "Failed set global var, invalid name (%s).",
				   yMsgSetGlobal.name);

		response.returnCode = -1;
#endif
		response.returnCode = 0;
	}

	response.message[0] = 0;

	yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	return yRc;

}								/*END: int process_DMOP_SETGLOBAL */


///This function is called when Media Manager receives a DMOP_SETDTMF.
int process_DMOP_SETDTMF(int zCall)
{
    int yRc = 0;
    char mod[] = {"process_DMOP_SETDTMF"};

   struct timeb tb;
   double currentMilliSec = 0;

   ftime(&tb);
   currentMilliSec = ( tb.time + ((double)tb.millitm)/1000 ) * 1000;

   /* 
     is there a dtmf already from 2833 ? 
     -- added 
           Tue Sep 30 09:40:44 EDT 2014 by joe S. 
     to deal with duplicates from sip info DTMF 
   */

   if(gCall[zCall].lastDtmfMilliSec < currentMilliSec)
   {
    gCall[zCall].dtmfAvailable  = 1;
    gCall[zCall].lastDtmf       = atoi(gCall[zCall].msgToDM.data);
    gCall[zCall].lastDtmfMilliSec = currentMilliSec;
    // gCall[zCall].lastDtmfTimestamp = currentMilliSec * ;
   } else {
       dynVarLog(__LINE__, zCall, mod, REPORT_DIAGNOSE, 9005, INFO,
                        "Ignoring duplicate DTMF (dtmfval=%d) ts(double=%lf)",
                            atoi(gCall[zCall].msgToDM.data), currentMilliSec);
   }

   return(0);

}/*END: int process_DMOP_SETDTMF*/



#if 0 // old code delete after some testing 
int
process_DMOP_SETDTMF (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_SETDTMF" };

	/*
      OK little bug here that prevents SIP info from working
    */

//    fprintf(stderr, " %s() dtmf_data=%s\n", __func__, gCall[zCall].msgToDM.data);

	gCall[zCall].dtmfAvailable = 1;
	gCall[zCall].lastDtmf = atoi (gCall[zCall].msgToDM.data);

	return (0);

}								/*END: int process_DMOP_SETDTMF */

#endif 

///This function is called when Media Manager receives a DMOP_GETGLOBAL.
int
process_DMOP_GETGLOBAL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_GETGLOBAL" };

	struct MsgToApp response;

	struct Msg_GetGlobal yMsgGetGlobal;

	sprintf (response.message, "\0");
	memcpy (&yMsgGetGlobal,
			&(gCall[zCall].msgToDM), sizeof (struct Msg_GetGlobal));

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	response.returnCode = 0;

	if (strcmp (yMsgGetGlobal.name, "$OUTBOUND_RET_CODE") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].outboundRetCode);
	}
	else if (strcmp (yMsgGetGlobal.name, "$KILL_APP_GRACE_PERIOD") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_KillAppGracePeriod);
	}
	else if (strcmp (yMsgGetGlobal.name, "$PLAY_BACK_BEEP_INTERVAL") == 0)
	{
		sprintf (response.message, "%d",
				 gCall[zCall].GV_PlayBackBeepInterval);
	}
	else if (strcmp (yMsgGetGlobal.name, "$RECORD_TERM_CHAR") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_RecordTermChar);
	}
	else if (strcmp (yMsgGetGlobal.name, "$MAX_PAUSE_TIME") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_MaxPauseTime);
	}
	else if (strcmp (yMsgGetGlobal.name, "$BRIDGE_PORT") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].crossPort);
	}
	else if (strcmp (yMsgGetGlobal.name, "$SR_DTMF_ONLY") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_SRDtmfOnly);
	}
	else if (strcmp (yMsgGetGlobal.name, "$CALL_PROGRESS") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_CallProgress);
	}
	else if (strcmp (yMsgGetGlobal.name, "$CALL_PROGRESS_DETAIL") == 0)
	{
//fprintf(stderr, " gCall[zCall].GV_LastCallProgressDetail=%d\n", gCall[zCall].GV_LastCallProgressDetail);
		sprintf (response.message, "%d", gCall[zCall].GV_LastCallProgressDetail);
	}
	else if (strcmp (yMsgGetGlobal.name, "$DTMF_AVAILABLE") == 0)
	{
        if(gCall[zCall].dtmfAvailable && (gCall[zCall].lastDtmf >= 0 || gCall[zCall].lastDtmf < 16)){
		  sprintf (response.message, "%d", gCall[zCall].lastDtmf);
//fprintf(stderr, " %s() DTMF_AVAILABLE dtmf char=%d\n", __func__, dtmfTab[gCall[zCall].lastDtmf]);
        } else {
//fprintf(stderr, " %s() DTMF_AVAILABLE not available\n", __func__);
		  sprintf (response.message, "%d", -1);
        }
	}
	else if (strcmp (yMsgGetGlobal.name, "$VOLUME") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_Volume);
	}
	else if (strcmp (yMsgGetGlobal.name, "$SPEED") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_Speed);
	}
	else if (strcmp (yMsgGetGlobal.name, "$FAX_DEBUG") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_FaxDebug);
	}
	else if (strcmp (yMsgGetGlobal.name, "$T30_ERROR_CORRECTION") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_T30ErrorCorrection);
	}
	else if (strcmp (yMsgGetGlobal.name, "$T38_ENABLED") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_T38Enabled);
	}
	else if (strcmp (yMsgGetGlobal.name, "$T38_FAX_VERSION") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_T38FaxVersion);
	}
	else if (strcmp (yMsgGetGlobal.name, "$T38_ERROR_CORRECTION_MODE") == 0)
	{
		sprintf (response.message, "%d",
				 gCall[zCall].GV_T38ErrorCorrectionMode);
	}
	else if (strcmp (yMsgGetGlobal.name, "$T38_ERROR_CORRECTION_SPAN") == 0)
	{
		sprintf (response.message, "%d",
				 gCall[zCall].GV_T38ErrorCorrectionSpan);
	}
	else if (strcmp (yMsgGetGlobal.name, "$T38_ERROR_CORRECTION_ENTRIES") ==
			 0)
	{
		sprintf (response.message, "%d",
				 gCall[zCall].GV_T38ErrorCorrectionEntries);
	}
	else if (strcmp (yMsgGetGlobal.name, "$AUDIO_SAMPLING_RATE") == 0)
	{

		switch (gCall[zCall].codecType)
		{
		case 0:
		case 8:
			sprintf (response.message, "%d", 8000);
			break;

		case 18:
			sprintf (response.message, "%d", 1000);
			break;

		default:
			sprintf (response.message, "%d", 8000);
			break;
		}
	}
	else if (strcmp (yMsgGetGlobal.name, "$LAST_PLAY_POSITION") == 0)
	{
		sprintf (response.message, "%d",
				 gCall[zCall].GV_LastPlayPostion);
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Failed to recognize global var name (%s) for call=%d.",
				   yMsgGetGlobal.name, zCall);

		response.returnCode = -1;
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Global var name (%s)=(%s).  rc=%d", yMsgGetGlobal.name,
			   response.message, response.returnCode);


	yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	return yRc;

}								/*END: process_DMOP_GETGLOBAL */

///This function is called when Media Manager receives a DMOP_SETGLOBALSTRING.
int
process_DMOP_SETGLOBALSTRING (int zCall)
{
	char            mod[] = { "process_DMOP_SETGLOBALSTRING" };
	int             yRc = 0;
	struct Msg_SetGlobalString yMsgSetGlobalString;
	struct MsgToApp response;

	sprintf (response.message, "\0");
	memcpy (&yMsgSetGlobalString,
			&(gCall[zCall].msgToDM), sizeof (struct Msg_SetGlobalString));

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	response.returnCode = 0;

	memset (response.message, 0, sizeof (response.message));

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Got (%s) with value (%s).",
			   yMsgSetGlobalString.name, yMsgSetGlobalString.value);

	if (strcmp (yMsgSetGlobalString.name, "$CALLING_PARTY") == 0)
	{
		sprintf (gCall[zCall].GV_CallingParty, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$CUST_DATA1") == 0)
	{
		sprintf (gCall[zCall].GV_CustData1, "%s", yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$CUST_DATA2") == 0)
	{
		sprintf (gCall[zCall].GV_CustData2, "%s", yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SKIP_TERMINATE_KEY") == 0)
	{
		sprintf (gCall[zCall].GV_SkipTerminateKey, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SKIP_BACKWARD_KEY") == 0)
	{

		sprintf (gCall[zCall].GV_SkipBackwardKey, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SKIP_FORWARD_KEY") == 0)
	{

		sprintf (gCall[zCall].GV_SkipForwardKey, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SKIP_REWIND_KEY") == 0)
	{

		sprintf (gCall[zCall].GV_SkipRewindKey, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$OUTBOUND_CALL_PARM") == 0)
	{
	FILE           *fp;
	char            yStrFileName[128];

		sprintf (yStrFileName, "%s", yMsgSetGlobalString.value);

		if ((fp = fopen (yStrFileName, "r")) == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR,
					   "Failed to open file (%s), [%d, %s] Unable to set $OUTBOUND_CALL_PARM.",
					   yStrFileName, errno, strerror (errno));
			response.returnCode = -1;
		}
		else
		{

			fscanf (fp, "%s", gCall[zCall].GV_OutboundCallParm);
			fclose (fp);

			arc_unlink (zCall, mod, yStrFileName);
		}
	}
	else if (strcmp (yMsgSetGlobalString.name, "$PAUSE_KEY") == 0)
	{
		sprintf (gCall[zCall].GV_PauseKey, "%s", yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$RESUME_KEY") == 0)
	{
		sprintf (gCall[zCall].GV_ResumeKey, "%s", yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$VOLUME_UP_KEY") == 0)
	{

		sprintf (gCall[zCall].GV_VolumeUpKey, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$VOLUME_DOWN_KEY") == 0)
	{
		sprintf (gCall[zCall].GV_VolumeDownKey, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SPEED_UP_KEY") == 0)
	{
		sprintf (gCall[zCall].GV_SpeedUpKey, "%s", yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SPEED_DOWN_KEY") == 0)
	{
		sprintf (gCall[zCall].GV_SpeedDownKey, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$DTMF_BARGEIN_DIGITS") == 0)
	{

		sprintf (gCall[zCall].GV_DtmfBargeinDigits, "%s",
				 yMsgSetGlobalString.value);

		if (yMsgSetGlobalString.value[0])
		{
			gCall[zCall].GV_DtmfBargeinDigitsInt = 1;
		}
		else
		{
			gCall[zCall].GV_DtmfBargeinDigitsInt = 0;
		}
	}
	else if (strcmp (yMsgSetGlobalString.name, "$PLAYBACK_PAUSE_GREETING_DIR")
			 == 0)
	{
		sprintf (gCall[zCall].GV_PlayBackPauseGreetingDirectory, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp
			 (yMsgSetGlobalString.name,
			  "$PLAYBACK_PAUSE_GREETING_DIRECTORY") == 0)
	{
		sprintf (gCall[zCall].GV_PlayBackPauseGreetingDirectory, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$PREFERRED_CODEC") == 0)
	{
		sprintf (gCall[zCall].GV_PreferredCodec, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$RECORD_UTTERANCE") == 0)
	{
		gCall[zCall].GV_RecordUtterance = 1;

		sprintf (gCall[zCall].lastRecUtteranceFile, "%s",
				 yMsgSetGlobalString.value);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Got RECORD_UTTERANCE setting GV_RecordUtterance to 1 and lastRecUtteranceFile(%s).",
				   gCall[zCall].lastRecUtteranceFile);

	}
	else if (strcmp (yMsgSetGlobalString.name, "$RECORD_UTTERANCETYPE") == 0)
	{

		sprintf (gCall[zCall].lastRecUtteranceType, "%s",
				 yMsgSetGlobalString.value);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Got RECORD_UTTERANCETYPE setting lastRecUtteranceType to (%s).",
				   gCall[zCall].lastRecUtteranceType);

		if (strcmp (gCall[zCall].lastRecUtteranceType, "audio/x-wav") == 0
			|| strcmp (gCall[zCall].lastRecUtteranceType, "audio/wav") == 0)
		{
			gCall[zCall].recordUtteranceWithWav = 1;
		}
		else
		{
			gCall[zCall].recordUtteranceWithWav = 0;
		}

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Got RECORD_UTTERANCETYPE setting recordUtteranceWithWav to %d.",
				   gCall[zCall].recordUtteranceWithWav);

	}
	else if (strcmp (yMsgSetGlobalString.name, "$STOP_RECORD_CALL") == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Got RECORD_CALL_STOP setting silentRecordFlag to 0.");
		gCall[zCall].silentRecordFlag = 0;
		//arc_frame_record_to_file(zCall, AUDIO_MIXED, (char *)__func__, __LINE__);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$T30_FAX_STATION_ID") == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
				   "Setting T30_FAX_STATION_ID to [%s].",
				   yMsgSetGlobalString.value);
		snprintf (gCall[zCall].GV_T30FaxStationId,
				  sizeof (gCall[zCall].GV_T30FaxStationId), "%s",
				  yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$T30_HEADER_INFO") == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
				   "Setting T30_HEADER_INFO to [%s].",
				   yMsgSetGlobalString.value);
		snprintf (gCall[zCall].GV_T30HeaderInfo,
				  sizeof (gCall[zCall].GV_T30HeaderInfo), "%s",
				  yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$T38_TRANSPORT") == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
				   "Setting T30_HEADER_INFO to [%s] ",
				   yMsgSetGlobalString.value);
		snprintf (gCall[zCall].GV_T30HeaderInfo,
				  sizeof (gCall[zCall].GV_T30HeaderInfo), "%s",
				  yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$RECORD_CALL") == 0)
	{
		sprintf (gCall[zCall].silentInRecordFileName, "%s",
				 yMsgSetGlobalString.value);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Got RECORD_CALL setting silentRecordFlag to 1.");

		gCall[zCall].silentRecordFlag = 1;

		if (gCall[zCall].gSilentInRecordFileFp != NULL)
		{
			fclose (gCall[zCall].gSilentInRecordFileFp);
			gCall[zCall].gSilentInRecordFileFp = NULL;
		}

		if (gCall[zCall].gSilentInRecordFileFp == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Opening silentInRecordFileName=(%s).",
					   gCall[zCall].silentInRecordFileName);

			gCall[zCall].gSilentInRecordFileFp =
				fopen (gCall[zCall].silentInRecordFileName, "w+");
			if (gCall[zCall].gSilentInRecordFileFp == NULL)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
						   "Failed to open recordFile=(%s), [%d, %s]  Unable to record call.",
						   gCall[zCall].silentInRecordFileName, errno,
						   strerror (errno));
				gCall[zCall].silentRecordFlag = 0;
			}
			else
			{
				/*Update the wav header */
				writeWavHeaderToFile (zCall,
									  gCall[zCall].gSilentInRecordFileFp);
			}
		}

#if 0
		sprintf (gCall[zCall].silentOutRecordFileName, "%s_outbound",
				 yMsgSetGlobalString.value);
		gCall[zCall].gSilentOutRecordFileFp =
			fopen (gCall[zCall].silentOutRecordFileName, "w+");
#endif
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Got RECORD_CALL setting silentRecordFlag to 1.");

		gCall[zCall].silentRecordFlag = 1;

	}
	else if (strcmp (yMsgSetGlobalString.name, "$CALL_CDR") == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
				   "Setting callCDR to [%s].",
				   yMsgSetGlobalString.value);
		snprintf (gCall[zCall].callCDR,
				  sizeof (gCall[zCall].callCDR), "%s",
				  yMsgSetGlobalString.value);
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Failed to recognize global string (%s) for call=%d.",
				   yMsgSetGlobalString.name, zCall);
		response.returnCode = -1;
	}

	yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	return (0);

}								/*END: int process_DMOP_SETGLOBALSTRING */

///This function is called when Media Manager receives a DMOP_GETGLOBALSTRING.
int
process_DMOP_GETGLOBALSTRING (int zCall)
{
	char            mod[] = { "process_DMOP_GETGLOBALSTRING" };
	int             yRc = 0;
	struct Msg_GetGlobalString yMsgGetGlobalString;
	struct MsgToApp response;

	sprintf (response.message, "\0");

	memcpy (&yMsgGetGlobalString,
			&(gCall[zCall].msgToDM), sizeof (struct Msg_GetGlobalString));

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	response.returnCode = 0;

	memset (response.message, 0, sizeof (response.message));

	if (strcmp (yMsgGetGlobalString.name, "$SKIP_TERMINATE_KEY") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_SkipTerminateKey);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$SKIP_BACKWARD_KEY") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_SkipRewindKey);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$PAUSE_KEY") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_PauseKey);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$RESUME_KEY") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_ResumeKey);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$CUST_DATA1") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_CustData1);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$CUST_DATA2") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_CustData2);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$DTMF_BARGEIN_DIGITS") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_DtmfBargeinDigits);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$T30_FAX_STATION_ID") == 0)
	{
		sprintf (response.message, "%s", gCall[zCall].GV_T30FaxStationId);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$T30_HEADER_INFO") == 0)
	{
		sprintf (response.message, "%s", gCall[zCall].GV_T30HeaderInfo);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$T38_TRANSPORT") == 0)
	{
		sprintf (response.message, "%s", gCall[zCall].GV_T38Transport);
	}
	else

	 if (strcmp (yMsgGetGlobalString.name, "$AUDIO_CODEC") == 0)
	{
		sprintf (response.message, "%s", gCall[zCall].audioCodecString);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$OUTBOUND_CALL_PARM") == 0)
	{
	FILE           *fp;
	char            yStrFileName[128];
	char            yTempBuffer[512];

		sprintf (yStrFileName, "%s/outboundCallParm.%d", DEFAULT_FILE_DIR,
				 gCall[zCall].appPid);

		if ((fp = fopen (yStrFileName, "w+")) == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR,
					   "Failed to open file (%s), [%d, %s] Unable to get $OUTBOUND_CALL_PARM.",
					   yStrFileName, errno, strerror (errno));

			response.returnCode = -1;
		}
		else
		{

	int             yIntTmpSamplingRate = 8000;

			switch (gCall[zCall].codecType)
			{
			case 18:
				yIntTmpSamplingRate = 1000;
				break;

			default:
				yIntTmpSamplingRate = 8000;
			}

			memset (gCall[zCall].GV_OutboundCallParm, 0,
					sizeof (gCall[zCall].GV_InboundCallParm));

			sprintf (gCall[zCall].GV_OutboundCallParm,
					 "ANI=%s"
					 "&DNIS=%s"
					 "&AUDIO_CODEC=%s"
					 "&FRAME_RATE=%d"
					 "&CALL_TYPE=%s"
					 "&AUDIO_SAMPLING_RATE=%d",
					 gCall[zCall].ani,
					 gCall[zCall].dnis,
					 gCall[zCall].audioCodecString,
					 -1,
					 gCall[zCall].call_type, yIntTmpSamplingRate);

			memset (yTempBuffer, 0, sizeof (yTempBuffer));
			sprintf (yTempBuffer, "%s", gCall[zCall].GV_OutboundCallParm);

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Writing OUTBOUND_CALL_PARM",
						  gCall[zCall].GV_OutboundCallParm, zCall);

			fprintf (fp, "%s", yTempBuffer);

			fclose (fp);

			sprintf (response.message, "%s", yStrFileName);

//          fprintf(fp, "%s", gCall[zCall].GV_OutboundCallParm);
//          fclose(fp);
		}

	}
	else if (strcmp (yMsgGetGlobalString.name, "$INBOUND_CALL_PARM") == 0)
	{
	FILE           *fp;
	char            yStrFileName[128];
	char            yTempBuffer[512];

		yTempBuffer[0] = '\0';

		sprintf (yStrFileName, "%s/inboundCallParm.%d", DEFAULT_FILE_DIR,
				 gCall[zCall].appPid);

		if ((fp = fopen (yStrFileName, "w+")) == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
					   "Failed to open file (%s). [%d, %s] Unable to get $INBOUND_CALL_PARM.",
					   yStrFileName, errno, strerror (errno));

			response.returnCode = -1;
		}
		else
		{
	int             yIntTmpSamplingRate = 8000;

			switch (gCall[zCall].codecType)
			{
			case 18:
				yIntTmpSamplingRate = 1000;
				break;

			default:
				yIntTmpSamplingRate = 8000;
			}

			memset (gCall[zCall].GV_InboundCallParm, 0,
					sizeof (gCall[zCall].GV_InboundCallParm));

#if 1
			if ( gCall[zCall].isCallAnswered == YES)
			{
				memset (gCall[zCall].call_type, 0, 50);
				sprintf (gCall[zCall].call_type, "Voice-Only");
			}
#endif

			sprintf (gCall[zCall].GV_InboundCallParm,
					 "ANI=%s"
					 "&DNIS=%s"
					 "&OCN=%s"
					 "&RDN=%s"
					 "&AUDIO_CODEC=%s"
					 "&FRAME_RATE=%d"
					 "&CALL_TYPE=%s"
					 "&AUDIO_SAMPLING_RATE=%d",
					 gCall[zCall].ani,
					 gCall[zCall].dnis,
					 gCall[zCall].ocn,
					 gCall[zCall].rdn,
					 gCall[zCall].audioCodecString,
					 -1,
					 gCall[zCall].call_type, yIntTmpSamplingRate);

			memset (yTempBuffer, 0, sizeof (yTempBuffer));
			sprintf (yTempBuffer, "%s", gCall[zCall].GV_InboundCallParm);

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Writing INBOUND_CALL_PARM",
						  gCall[zCall].GV_InboundCallParm, zCall);
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Writing INBOUND_CALL_PARAM = (%s).", yTempBuffer);

			fprintf (fp, "%s", yTempBuffer);

			fclose (fp);

			sprintf (response.message, "%s", yStrFileName);
		}

	}
	else if (strcmp (yMsgGetGlobalString.name, "$CONF_CHANNEL") == 0)
	{
	char            tempChar[64] = "";

		for (int i = 0; i < MAX_PORTS; i++)
		{
			if (strcmp (gCall[zCall].confID, gCall[i].confID) == 0)
			{
	char            temp[4] = "";

				sprintf (temp, "%s^%d_", gCall[i].ani, i);
				strcat (tempChar, temp);
			}
		}
		sprintf (response.message, "%s", tempChar);
	}
#ifdef VOICE_BIOMETRICS
    else if (strcmp (yMsgGetGlobalString.name, "$AVB_CONTSPEECH_RESULT") == 0)
    {

        if ( gCall[zCall].continuousVerify == CONTINUOUS_VERIFY_INACTIVE )
        {
            memcpy (&(response),
                &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
            response.returnCode = -1;
        }
//      dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM, ERR,
//              "DJB: gCall[%d].msg_avbMsgToApp.returnCode to %d; "
//              "msg_avbMsgToApp.score=%f; " 
//              "msg_avbMsgToApp.indThreshold=%f; " 
//              "msg_avbMsgToApp.confidence=%f; ",
//              zCall,
//              gCall[zCall].msg_avbMsgToApp.returnCode,
//              gCall[zCall].msg_avbMsgToApp.score,
//              gCall[zCall].msg_avbMsgToApp.indThreshold,
//              gCall[zCall].msg_avbMsgToApp.confidence);
        if ( gCall[zCall].msg_avbMsgToApp.returnCode == TEL_AVB_NO_RESULTS )
        {
            memcpy (&(gCall[zCall].msg_avbMsgToApp),
                &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
            gCall[zCall].msg_avbMsgToApp.returnCode = TEL_AVB_NO_RESULTS;
        }
        else
        {
            memcpy (&(gCall[zCall].msg_avbMsgToApp),
                &(gCall[zCall].msgToDM), sizeof (int)*5);
        }
        memcpy (&response, &(gCall[zCall].msg_avbMsgToApp),
                    sizeof (struct MsgToApp));
    }
#endif // END: VOICE_BIOMETRICS

	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Failed to recognize global string (%s) for call=%d.",
				   yMsgGetGlobalString.name, zCall);

		response.returnCode = -1;
	}

	yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	return (yRc);

}								/*END: int process_DMOP_GETGLOBALSTRING */

///This function is called when Media Manager receives a DMOP_SPEECHDETECTED.
int
process_DMOP_SPEECHDETECTED (int zCall)
{
	char            mod[] = "process_DMOP_SPEECHDETECTED";
	struct Msg_SpeechDetected msg;
	struct Msg_SpeechDetected *pmsg = &msg;

	time_t          tb;

	time (&tb);

	gCall[zCall].utteranceTime2 = tb;

	pmsg = (struct Msg_SpeechDetected *) (&gCall[zCall].msgToDM);

	//gCall[zCall].utteranceTime2 = pmsg->milliSecs;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Got process_DMOP_SPEECHDETECTED setting utteranceTime2(%ld), "
			   "milliSecs=%d.", gCall[zCall].utteranceTime2, pmsg->milliSecs);

	return (0);
}

int openGoogleUdpPort(int telport, int udpport)
{
	char    mod[] = "openGoogleUdpPort";

	if((telport < gStartPort) ||  (telport > gEndPort))
	{
		return -1;
	}

	int s, i;
	gCall[telport].google_slen=sizeof(gCall[telport].google_si);

	
	//Close existing udpport, if left open somehow
	if(gCall[telport].googleUDPFd > -1)
	{
		close(gCall[telport].googleUDPFd);
		gCall[telport].googleUDPFd = -1;
		gCall[telport].googleUDPPort = -1;
	}

	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		dynVarLog (__LINE__, telport, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
			"Failed to open UDP Port to Google SR Client. [%d:%s]", 
			errno, strerror (errno));

		gCall[telport].googleUDPPort = -1;
		gCall[telport].googleUDPFd = -1;
		return (-1);
	}

	memset((char *) &gCall[telport].google_si, 0, sizeof(gCall[telport].google_si));
	gCall[telport].google_si.sin_family = AF_INET;
	gCall[telport].google_si.sin_port = htons(udpport);

	if (inet_aton("127.0.0.1" , &gCall[telport].google_si.sin_addr) == 0) 
	{
		dynVarLog (__LINE__, telport, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
			"inet_atno failed while opening UDP Port %d to Google SR Client. [%d:%s]", 
			udpport, errno, strerror (errno));

		close(s);

		gCall[telport].googleUDPPort = -1;
		gCall[telport].googleUDPFd = -1;

		return (-1);
		
	}

	
	gCall[telport].googleUDPPort = udpport;
	gCall[telport].googleUDPFd = s;

	dynVarLog (__LINE__, telport, mod, REPORT_VERBOSE, TEL_BASE, INFO,
   		"GSR: Opened UDP for telport(%d) udpoport(%d) fd(%d)", telport, udpport, s);
	return (0);

}//END: int openGoogleUdpPort

void *googleReaderThread(void *z)
{
	char	mod[] = "googleReaderThread";
	int yRc = 0;
	static int count = 0;

	//1 Create the request struct
	typedef struct
	{ 
		guint32 opcode;
		guint32 mmid;
		guint32 telport;
		guint32 udpport;
		guint32 other;
		char	data[128];

	} _GSR;

	_GSR gsr;

	//2. Create and open /tmp/ArcGSRResponseFifo.<mgr id> for response details
	sprintf (gGoogleSRResponseFifo, "%s/%s.%d", gFifoDir, GOOGLE_RESPONSE_FIFO, gDynMgrId);

	if (mknod (gGoogleSRResponseFifo, S_IFIFO | PERMS, 0) < 0 && errno != EEXIST)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
			"ARCGS: Failed to create response fifo (%s). [%d, %s] "
			"Unable to communicate with Google SR Client.",
				gGoogleSRResponseFifo, errno, strerror (errno));

		close(gGoogleSRRequestFifoFd);
		gGoogleSRRequestFifoFd = -1;

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (0);
	}

	if ((gGoogleSRResponseFifoFd = open (gGoogleSRResponseFifo, O_CREAT | O_RDWR)) < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
			"ARCGS: Failed to open response fifo (%s). [%d, %s] "
			"Unable to communicate with Google SR Client.",
				gGoogleSRResponseFifo, errno, strerror (errno));

		close(gGoogleSRRequestFifoFd);
		gGoogleSRRequestFifoFd = -1;

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return(0);
	}

	dynVarLog (__LINE__, gsr.telport, mod, REPORT_VERBOSE, TEL_BASE, INFO,
   		"ARCGS: googleReaderThread: Listening on GoogleSRResponseFifo(%d)", 
		gGoogleSRResponseFifoFd);

	//Now read responses arriving on gGoogleSRResponseFifo
	count = 0;
	while(gCanExit == 0)
	{
		gsr.opcode 	= -1;
		gsr.mmid 	= gDynMgrId;
		gsr.telport = -1;
		gsr.udpport = -1;


		yRc = read (gGoogleSRResponseFifoFd, &gsr, sizeof (_GSR));
//		dynVarLog (__LINE__, gsr.telport, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//  				"DJB: %d = read()  gsr.opcode = %d", yRc, gsr.opcode);
		if (yRc == -1)
		{
			if (errno == EAGAIN)
			{
				sleep(1);
				continue;
			}

			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR, "ARCGS: Failed to read request from (%s). [%d, %s]",
					   gGoogleSRResponseFifo, errno, strerror (errno));

			sleep(1);
			continue;
		}

		if((gsr.telport < gStartPort) ||  (gsr.telport > gEndPort))
		{
			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
			   ERR, "ARCGS: Failed to process request from (%s). "
				"GSR Message from Java Client has invalid or out of range port %d",
			   gGoogleSRResponseFifo, gsr.telport);

			continue;
		}

		switch(gsr.opcode)
		{
			case 2:
				dynVarLog (__LINE__, gsr.telport, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   		"ARCGS: Got GSR_START_RESPONSE with telport(%d) udpoport(%d)", 
					gsr.telport, gsr.udpport);

				gCall[gsr.telport].googleUDPPort = gsr.udpport;

				//Open this udpport to write speech data
				yRc = openGoogleUdpPort(gsr.telport, gsr.udpport);

				// DJB TODO: if google_sr, 
				//              send Msg_SRRecognize result back to app
				//
				if ( ! gCall[gsr.telport].googleStreamingSR == 1 )
				{
					gCall[gsr.telport].googleSRResponse.returnCode = 0;
					memset((char *)gCall[gsr.telport].googleSRResponse.message, '\0', sizeof(gCall[gsr.telport].googleSRResponse.message));
					yRc = writeGenericResponseToApp (gsr.telport, &gCall[gsr.telport].googleSRResponse, mod, __LINE__);
				}
				break;

			case 3:

				//Close this udpport
				if(gCall[gsr.telport].googleUDPFd > -1 && gsr.udpport == 1)
				{
					int zCall = gsr.telport;
__DDNDEBUG__ (DEBUG_MODULE_SR, "MRCP: ARCGS: Closing googleUDPFd", "", gCall[zCall].googleUDPFd);

					dynVarLog (__LINE__, gsr.telport, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   			"ARCGS: Got FINAL GSROP_RESULT for telport(%d) data(%s). Closing UDP(%d)", 
						gsr.telport, gsr.data, gCall[gsr.telport].googleUDPPort);

					close(gCall[gsr.telport].googleUDPFd);
					gCall[gsr.telport].googleUDPFd = -1;
					gCall[gsr.telport].googleUDPPort = -1;

				}
				else
				{
					dynVarLog (__LINE__, gsr.telport, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   			"ARCGS: Got intermediate GSROP_RESULT for telport(%d) data(%s).", 
						gsr.telport, gsr.data);
				}
				break;

			default:
				break;
		}//switch


	} // while

	pthread_detach (pthread_self ());
	pthread_exit (NULL);
	return(0);

} //End: void googleReaderThread

int startGoogleReaderThread()
{
	char	mod[] = "startGoogleReader";
	int yRc = 0;

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "gGoogleSR_Enabled, starting the thread.");

	yRc = pthread_attr_init (&gGoogleReaderThreadAttr);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
			"Failed to start google thread. pthread_attr_init() failed. rc=%d. [%d, %s] ",
			yRc, errno, strerror (errno));

		return (-1);
	}

	yRc =
		pthread_attr_setdetachstate (&gGoogleReaderThreadAttr,
									 PTHREAD_CREATE_DETACHED);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "Failed to start google thread. pthread_attr_setdetachstate() failed. rc=%d. [%d, %s] ",
				   yRc, errno, strerror (errno));
		return (-1);
	}
	yRc =
		pthread_create (&gGoogleReaderThreadId, &gGoogleReaderThreadAttr,
						googleReaderThread, NULL);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "Shutting down: pthread_create() failed. rc=%d. [%s, %d] "
				   "Unable to create shared memory reader thread. Exiting.",
				   yRc, errno, strerror (errno));

		return (-1);
	}

} /*END: startReaderThread */

int initializeGoogleSRFifo(int zCall)
{
	char            mod[] = { "initializeGoogleCom" };
	//1. Open /tmp/ArcGSRRequestFifo for writing general messages to JAVA client

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
  		"ARCGS: Inside initializeGoogleSRFifo");

	if(gGoogleSRRequestFifoFd > -1)
	{
		return (0);
	}

	sprintf (gGoogleSRRequestFifo, "%s/%s", gFifoDir, GOOGLE_REQUEST_FIFO);

	if ((gGoogleSRRequestFifoFd =
	 	arc_open (zCall, mod, __LINE__, gGoogleSRRequestFifo, O_WRONLY,
			   ARC_TYPE_FIFO)) < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
			"ARCGS: Failed to open request fifo (%s). [%d, %s] "
			"Unable to communicate with Google SR Client.",
			gRequestFifo, errno, strerror (errno));

		return (-1);
	}

	return 0;

}//int initializeGoogleSRFifo

int sendRequestToGoogleSRFifo(int zCall, int zLine, int gsopcode, char *recordedFile)
{
	char	mod[] = { "sendRequestToGoogleSRFifo" };
	int 	yRc = 0;

	pthread_mutex_lock (&gCall[zCall].gMutexSentGoogleRequest);
	if(gGoogleSRRequestFifoFd <= -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
	   		"ARCGS: [%d] Calling initializeGoogleSRFifo", zLine);

		yRc = initializeGoogleSRFifo(zCall);
		if(yRc < 0)
		{
			pthread_mutex_unlock (&gCall[zCall].gMutexSentGoogleRequest);
			return yRc;
		}
	}

	//1 Create the request struct
	typedef struct
	{ 
		guint32 opcode;
		guint32 mmid;
		guint32 telport;
		guint32 rectime;
		guint32 trailtime;
		char	data[128];

	} _GSR;

	_GSR gsr;

	memset((_GSR *)&gsr, '\0', sizeof(gsr));
	gsr.opcode 	= gsopcode;
	gsr.mmid 	= gDynMgrId;
	gsr.telport = zCall;
	gsr.rectime = 30;
	gsr.trailtime = 10;

	if ( gsopcode == GSR_RECORD_REQUEST )
	{
		sprintf(gsr.data, "%s", recordedFile);
	}

	yRc = write (gGoogleSRRequestFifoFd, &gsr, sizeof (_GSR));
	
	pthread_mutex_unlock (&gCall[zCall].gMutexSentGoogleRequest);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"ARCGS: [%d] Sent %d bytes to GoogleSRRequestFifo fd(%d) for opcode(%d) data(%s)", zLine,
		yRc, gGoogleSRRequestFifoFd, gsopcode, recordedFile);


	return (yRc);
}//int sendRequestToGoogleSRFifo

// This is for google streaming only - no MS
int process_DMOP_SRRECOGNIZE(int zCall) 
{
	static char		mod[]="process_DMOP_SRRECOGNIZE";
	int				rc;
	struct MsgToApp			response;


	memset((MsgToApp *)&response, '\0', sizeof(MsgToApp));
	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
	memcpy (&gCall[zCall].googleSRResponse, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	gCall[zCall].googleStreamingSR = 1;
	gCall[zCall].speechRec = 1;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "lastRecUtteranceFile=(%s).",
			   gCall[zCall].lastRecUtteranceFile);

	if (gCall[zCall].lastRecUtteranceFile[0] != 0)
	{

		time_t          tb;

		time (&tb);

		if (gCall[zCall].gUtteranceFileFp == NULL)
		{
			gCall[zCall].gUtteranceFileFp =
				fopen (gCall[zCall].lastRecUtteranceFile, "w");
		}

		if (gCall[zCall].gUtteranceFileFp == NULL)
		{
			gCall[zCall].lastRecUtteranceFile[0] = 0;
		}
		else
		{
			if (gCall[zCall].recordUtteranceWithWav == 1)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Writing wav header to file (%s).",
						   gCall[zCall].lastRecUtteranceFile);
				writeWavHeaderToFile (zCall, gCall[zCall].gUtteranceFileFp);
			}
		}

		gCall[zCall].utteranceTime1 = tb;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting utteranceTime1 to %ld.",
				   gCall[zCall].utteranceTime1);
	}

	/*Step 4: If any phrases are queued, play them, else send SR_PROPMTEND to client */

	__DDNDEBUG__ (DEBUG_MODULE_SR, "calling addToAppRequestList", "", zCall);
	clearAppRequestList (zCall);
	addToAppRequestList (&(gCall[zCall].msgToDM));

	__DDNDEBUG__ (DEBUG_MODULE_SR, "Setting pendingInputRequests to 1.", "",
				  zCall);
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Setting pendingInputRequests to 1.");
	gCall[zCall].pendingInputRequests = 1;

	if (gCall[zCall].googleStreamingSR == 1)
	{
		if ( (rc = sendRequestToGoogleSRFifo(zCall, __LINE__, GSR_STREAMING_REQUEST, "" )) < 0 )
		{
	        response.returnCode = -1;
	        sprintf(response.message,  "%s", "ARCGS: Failed to make request to google java client.");
	
	        dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, response.message);
	
	        rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
	        return(0);
		}
	}

	return(0);
} // END: process_DMOP_SRRECOGNIZE()

///This function is called when Media Manager receives a DMOP_SRRECOGNIZEFROMCLIENT.
int
process_DMOP_SRRECOGNIZEFROMCLIENT (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_SRRECOGNIZEFROMCLIENT" };
	char            yStrDtmf[10];
	int             yPayloadType;
	int             yCodecType;
	static int      ssrcDelta = 0;

	char            mrcpServer[256];
	int             mrcpRtpPort = -1;
	int             mrcpRtcpPort = -1;

	memset (yStrDtmf, 0, sizeof (yStrDtmf));

	/*Step 1: Save the SR structure */
	memcpy (&(gCall[zCall].msgRecognizeFromClient),
			&(gCall[zCall].msgToDM), sizeof (struct Msg_SRRecognize));

	gCall[zCall].attachedSRClient = gCall[zCall].msgToDM.appPassword;

#if 0
	/*Step 2: Get resource info from saved SR structure */
	//memset(gCall[zCall].mrcpServer,       0, sizeof(gCall[zCall].mrcpServer));
	//memset(gCall[zCall].gUtteranceFile,0, sizeof(gCall[zCall].gUtteranceFile));
	//gCall[zCall].mrcpRtpPort = -1;
	//gCall[zCall].mrcpRtcpPort = -1;
#endif

	/*Step 2: Get resource info from saved SR structure */
	memset (mrcpServer, 0, sizeof (mrcpServer));
	mrcpRtpPort = -1;
	mrcpRtcpPort = -1;

	__DDNDEBUG__ (DEBUG_MODULE_SR, "SR Resource",
				  gCall[zCall].msgRecognizeFromClient.resource, 0);

	sscanf (gCall[zCall].msgRecognizeFromClient.resource,
			"%[^|]|%d|%d|%d|%d|%[^|]|%s",
			mrcpServer,
			&mrcpRtpPort,
			&mrcpRtcpPort,
			&yPayloadType, &yCodecType,
			gCall[zCall].gUtteranceFile, yStrDtmf);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "MRCP::Setting yPayloadType to %d.", yPayloadType);

	__DDNDEBUG__ (DEBUG_MODULE_SR, "MRCP RTP IP and PORT", mrcpServer,
				  mrcpRtpPort);
	__DDNDEBUG__ (DEBUG_MODULE_SR, "MRCP UTTERANCE FILE",
				  gCall[zCall].gUtteranceFile, zCall);
	__DDNDEBUG__ (DEBUG_MODULE_SR, "Payload type", "", yPayloadType);
	__DDNDEBUG__ (DEBUG_MODULE_SR, "Codec type", "", yCodecType);

	/*Step 3: Open RTP session based on resource info */

	if (gCall[zCall].rtp_session_mrcp != NULL)
	{

		if (strcmp (mrcpServer, gCall[zCall].mrcpServer) == 0 &&
			mrcpRtpPort == gCall[zCall].mrcpRtpPort)
		{
			//rtp_session_reset(gCall[zCall].rtp_session_mrcp);
		}
		else
		{
			__DDNDEBUG__ (DEBUG_MODULE_SR, "Closing last mrcp session",
						  "Call", zCall);

			sprintf (gCall[zCall].mrcpServer, "%s", mrcpServer);
			gCall[zCall].mrcpRtpPort = mrcpRtpPort;
			gCall[zCall].mrcpRtcpPort = mrcpRtcpPort;

			rtp_session_destroy (gCall[zCall].rtp_session_mrcp);
			gCall[zCall].rtp_session_mrcp = NULL;
			gCall[zCall].mrcpTs = 0;
			gCall[zCall].mrcpTime = 0;
			gCall[zCall].rtp_session_mrcp =
				rtp_session_new (RTP_SESSION_SENDONLY);
			rtp_session_set_blocking_mode (gCall[zCall].rtp_session_mrcp, 0);
			rtp_session_set_ssrc (gCall[zCall].rtp_session_mrcp,
								  atoi ("3197096732"));

			rtp_session_set_remote_addr (gCall[zCall].rtp_session_mrcp,
										 gCall[zCall].mrcpServer,
										 gCall[zCall].mrcpRtpPort, gHostIf);

			rtp_session_set_payload_type (gCall[zCall].rtp_session_mrcp,
										  yCodecType);

			rtp_profile_clear_payload (&av_profile, 96);
			rtp_profile_clear_payload (&av_profile, 101);
			rtp_profile_clear_payload (&av_profile, 120);
			rtp_profile_clear_payload (&av_profile, 127);

			rtp_profile_set_payload (&av_profile, yPayloadType,
									 &telephone_event);
			rtp_session_set_profile (gCall[zCall].rtp_session_mrcp,
									 &av_profile);

		}

	}
	else
	{
		sprintf (gCall[zCall].mrcpServer, "%s", mrcpServer);
		gCall[zCall].mrcpRtpPort = mrcpRtpPort;
		gCall[zCall].mrcpRtcpPort = mrcpRtcpPort;
		gCall[zCall].mrcpTs = 0;
		gCall[zCall].mrcpTime = 0;

		gCall[zCall].rtp_session_mrcp =
			rtp_session_new (RTP_SESSION_SENDONLY);
		rtp_session_set_blocking_mode (gCall[zCall].rtp_session_mrcp, 0);
		rtp_session_set_ssrc (gCall[zCall].rtp_session_mrcp,
							  atoi ("3197096732"));

		rtp_session_set_remote_addr (gCall[zCall].rtp_session_mrcp,
									 gCall[zCall].mrcpServer,
									 gCall[zCall].mrcpRtpPort, gHostIf);

		rtp_session_set_payload_type (gCall[zCall].rtp_session_mrcp,
									  yCodecType);

		rtp_profile_clear_payload (&av_profile, 96);
		rtp_profile_clear_payload (&av_profile, 101);
		rtp_profile_clear_payload (&av_profile, 120);
		rtp_profile_clear_payload (&av_profile, 127);

		rtp_profile_set_payload (&av_profile, yPayloadType, &telephone_event);
		rtp_session_set_profile (gCall[zCall].rtp_session_mrcp, &av_profile);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "MRCP::Setting yPayloadType to %d.", yPayloadType);

	}

	gCall[zCall].speechRecFromClient = 1;
	gCall[zCall].speechRec = 1;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "lastRecUtteranceFile=(%s).",
			   gCall[zCall].lastRecUtteranceFile);

	if (gCall[zCall].lastRecUtteranceFile[0] != 0)
	{

	time_t          tb;

		time (&tb);

		if (gCall[zCall].gUtteranceFileFp == NULL)
		{
			gCall[zCall].gUtteranceFileFp =
				fopen (gCall[zCall].lastRecUtteranceFile, "w");
		}

		if (gCall[zCall].gUtteranceFileFp == NULL)
		{
			gCall[zCall].lastRecUtteranceFile[0] = 0;
		}
		else
		{
			if (gCall[zCall].recordUtteranceWithWav == 1)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Writing wav header to file (%s).",
						   gCall[zCall].lastRecUtteranceFile);
				writeWavHeaderToFile (zCall, gCall[zCall].gUtteranceFileFp);
			}
		}

		gCall[zCall].utteranceTime1 = tb;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting utteranceTime1 to %ld.",
				   gCall[zCall].utteranceTime1);
	}

	/*Step 4: If any phrases are queued, play them, else send SR_PROPMTEND to client */

#if 0
	if (gCall[zCall].msgRecognizeFromClient.beepFile[0] != '\0' &&
		gCall[zCall].pFirstSpeak == NULL)
	{
		;						//DDN_TODO: add beep file to queue as PUT_QUEUE
	}
#endif

	__DDNDEBUG__ (DEBUG_MODULE_SR, "calling addToAppRequestList", "", zCall);
	clearAppRequestList (zCall);
	addToAppRequestList (&(gCall[zCall].msgToDM));

	__DDNDEBUG__ (DEBUG_MODULE_SR, "Setting pendingInputRequests to 1.", "",
				  zCall);
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Setting pendingInputRequests to 1.");
	gCall[zCall].pendingInputRequests = 1;
	gCall[zCall].speechRecFromClient = 1;

	/*Step 5: Start sending the rtp data to mrcp server */

	/*Step 6: Check if it is a Google SR */
	if(gCall[zCall].msgRecognizeFromClient.tokenType == SR_GOOGLE)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "ARCGS: Calling sendRequestToGoogleSRFifo");

		int yGoogleRc = sendRequestToGoogleSRFifo(zCall, __LINE__, GSR_STREAMING_REQUEST, "");
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "ARCGS: NOT Calling sendRequestToGoogleSRFifo");
	}

	return (yRc);

}/*END: int process_DMOP_SRRECOGNIZEFROMCLIENT */
#ifdef VOICE_BIOMETRICS

extern "C"
{   

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int process_DMOP_VERIFY_CONTINUOUS_SETUP (int zCall)
{
	static char mod[] = "process_DMOP_VERIFY_CONTINUOUS_SETUP";
	int			rc;
	char		errMsg[1024];
	char		tmpMsg[1024];
	char		pcmWavefile[256];

	struct Msg_ContinuousSpeech yMsgContinuousSpeech;
	struct Msg_AVBMsgToApp		yAvbMsg;
	struct MsgToApp				response;

	memset((struct MsgToApp *)&response, '\0', sizeof( struct MsgToApp ));

	memcpy (&yMsgContinuousSpeech,
			&(gCall[zCall].msgToDM), sizeof (struct Msg_ContinuousSpeech));

	//memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
	memcpy (&yAvbMsg, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	yAvbMsg.returnCode = 0;
	yAvbMsg.vendorErrorCode = 0;
	yAvbMsg.score			= 0.0;
	yAvbMsg.indThreshold	= 0.0;
	yAvbMsg.confidence		= 0.0;
	memset((char *)yAvbMsg.errMsg, '\0', sizeof(yAvbMsg.errMsg));

	gCall[zCall].msg_avbMsgToApp.returnCode = TEL_AVB_NO_RESULTS;
	response.returnCode = -1;

	gCall[zCall].pv_voiceprint = NULL;
	gCall[zCall].pv_VoiceID_conObj = NULL;
	gCall[zCall].avb_readyFlag		= 0;
	gCall[zCall].avb_score			= 0.0;
	gCall[zCall].avb_indThreshold	= 0.0;
	gCall[zCall].avb_confidence		= 0.0;

	gCall[zCall].fp_pcmWavefile = NULL;
	if ( yMsgContinuousSpeech.Keep_PCM_Files == 1 )
	{
		time_t		DateTime;
		struct tm	*PT_Time;
		struct tm	tempTM;
		char		LogLine[64];

        DateTime = time(NULL);      /* Obtain local date & time */
        PT_Time = localtime_r(&DateTime, &tempTM);  /* Convert to structure */
        (void)strftime(LogLine, 132, "%m%d%y-%H%M%S", PT_Time);

		sprintf(pcmWavefile, "%s/%s_VerifyCont_%d_%s.wav",
					gAvb_PCM_PhraseDir, yMsgContinuousSpeech.pin, zCall, LogLine);
		gCall[zCall].fp_pcmWavefile = fopen (pcmWavefile, "w");
		if (gCall[zCall].fp_pcmWavefile == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
				"Unable to open pcm utterance file (%s).  Continuing.",
				pcmWavefile);
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				"Successfully opened (%s) for pcm utterance.", pcmWavefile);
		}
	}

	ARC_G711_PARMS_INIT (gCall[zCall].avb_decode_parms, 1);
	rc = arc_decoder_init (&(gCall[zCall].avb_decode_adc),
			ARC_DECODE_G711, 
			&(gCall[zCall].avb_decode_parms),
			sizeof (gCall[zCall].avb_decode_parms), 1);
	if ( rc == -1 )
	{
		yAvbMsg.returnCode = -1;
		yAvbMsg.vendorErrorCode = -1;
		sprintf(tmpMsg, "%s", "Unable to initialize decoder. ");
		sprintf(yAvbMsg.errMsg,  "[%.*s]", 214, tmpMsg);
		
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, tmpMsg);
		memcpy (&response, &yAvbMsg,  sizeof (struct MsgToApp));

		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		return(0);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"%d=arc_decoder_init(), size = %d", rc, sizeof (gCall[zCall].avb_decode_parms));

	gCall[zCall].pv_voiceprint = VoiceID_allocate_voiceprint(yMsgContinuousSpeech.pin,
										&yMsgContinuousSpeech.ui_voiceprint_size, errMsg);
	if(gCall[zCall].pv_voiceprint == NULL)
	{
		yAvbMsg.returnCode = -1;
		yAvbMsg.vendorErrorCode = -1;
		sprintf(tmpMsg, "Unable to allocate voiceprint; unable to retrieve it.  [%s]", errMsg);
		sprintf(yAvbMsg.errMsg,  "[%.*s]", 214, tmpMsg);
		
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, tmpMsg);
		memcpy (&response, &yAvbMsg,  sizeof (struct MsgToApp));

		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		return(0);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "VoiceID_allocate_voiceprint() succeeded.");
	if ( yMsgContinuousSpeech.ui_voiceprint_size > 0 )
	{
		FILE			*pF;
		char			errorMsg[256] = "";
		int				rc;
		unsigned 		readSize;

		readSize = yMsgContinuousSpeech.ui_voiceprint_size;
		if ( access(yMsgContinuousSpeech.file, F_OK))
		{
			yAvbMsg.returnCode = -1;
			yAvbMsg.vendorErrorCode = -1;
			sprintf(tmpMsg, "Unable to access (%s). [%d, %s]", 
					yMsgContinuousSpeech.file, errno, strerror(errno));
			sprintf(yAvbMsg.errMsg,  "[%.*s]", 214, tmpMsg);
		
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, tmpMsg);
			memcpy (&response, &yAvbMsg,  sizeof (struct MsgToApp));

			rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			return(0);
		}

		if((pF = fopen(yMsgContinuousSpeech.file, "rb")) == NULL)
		{
			yAvbMsg.returnCode = -1;
			yAvbMsg.vendorErrorCode = -1;
			sprintf(tmpMsg, "Error opening (%s) to read.  Unable to retrieve voice print. "
						"[%d, %s] Cannot perform continuous speech operation.",
						yMsgContinuousSpeech.file, errno, strerror(errno));
			sprintf(yAvbMsg.errMsg,  "[%.*s]", 214, tmpMsg);
		
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, tmpMsg);
			memcpy (&response, &yAvbMsg,  sizeof (struct MsgToApp));

			rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			return(0);
		}
	
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				"Successfully opened (%s).  "
				"Attempting to read %d bytes from it.",
				yMsgContinuousSpeech.file, readSize);
		errno=0;
		if((rc=fread(gCall[zCall].pv_voiceprint, 1, readSize, pF)) != readSize)
		{
			yAvbMsg.returnCode = -1;
			yAvbMsg.vendorErrorCode = -1;
			sprintf(tmpMsg, "Failed to read voiceprint file (%s). rc=%d. [%d, %s]",
								yMsgContinuousSpeech.file, rc, errno, strerror(errno));
			sprintf(yAvbMsg.errMsg,  "[%.*s]", 214, tmpMsg);
		
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, tmpMsg);
			memcpy (&response, &yAvbMsg,  sizeof (struct MsgToApp));

			fclose(pF);
			rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			return(0);
		}
		fclose(pF);
		if (gCall[zCall].lastRecUtteranceFile[0] != 0)
		{
			time_t          tb;
			time (&tb);
	
			if (gCall[zCall].gUtteranceFileFp == NULL)
			{
				gCall[zCall].gUtteranceFileFp =
					fopen (gCall[zCall].lastRecUtteranceFile, "w");
			}
	
			if (gCall[zCall].gUtteranceFileFp == NULL)
			{
				gCall[zCall].lastRecUtteranceFile[0] = 0;
			}
			else
			{
				if (gCall[zCall].recordUtteranceWithWav == 1)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Writing wav header to file (%s).",
							   gCall[zCall].lastRecUtteranceFile);
					writeWavHeaderToFile (zCall, gCall[zCall].gUtteranceFileFp);
				}
			}
	
			gCall[zCall].utteranceTime1 = tb;
	
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting utteranceTime1 to %ld.",
					   gCall[zCall].utteranceTime1);
		}
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"Successfully read voice print file (%s). rc=%d. Unlinking it.",	
			yMsgContinuousSpeech.file, rc);
		unlink(yMsgContinuousSpeech.file);
	}

	/* Create VoiceID container object */
	if( (gCall[zCall].pv_VoiceID_conObj = VoiceID_initialize(errMsg)) == NULL)
	{
		yAvbMsg.returnCode = -1;
		yAvbMsg.vendorErrorCode = -1;
		sprintf(tmpMsg, "VoiceID_initialize failed:[%s] Cannot perform "
						"continuous speech operation.", errMsg);
		sprintf(yAvbMsg.errMsg,  "[%.*s]", 214, tmpMsg);
		
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR, ERR,
				"VoiceID_initialize failed:[%s] Cannot perform "
				"continuous speech operation.", errMsg);
		closeVoiceID(zCall);
		memcpy (&response, &yAvbMsg,  sizeof (struct MsgToApp));
		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		return(0);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "VoiceID_initialize() succeeded.");
	
	if((rc = VoiceID_set_voiceprint(gCall[zCall].pv_voiceprint, yMsgContinuousSpeech.pin,
									 gCall[zCall].pv_VoiceID_conObj, errMsg)))
	{
		yAvbMsg.returnCode = -1;
		yAvbMsg.vendorErrorCode = rc;
		sprintf(tmpMsg, "VoiceID_set_voiceprint() failed, [%d, %s].  "
			"Unable to copy voiceprint into container and perform continuous "
			"verification.", rc, errMsg);
		sprintf(yAvbMsg.errMsg,  "[%.*s]", 214, tmpMsg);
		
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, tmpMsg);
		memcpy (&response, &yAvbMsg,  sizeof (struct MsgToApp));
		closeVoiceID(zCall);
		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		return 1;
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "VoiceID_set_voiceprint(%s) succeeded.",
					yMsgContinuousSpeech.pin);
	sprintf(gCall[zCall].avb_pin, "%s", yMsgContinuousSpeech.pin);
	gCall[zCall].continuousVerify	= CONTINUOUS_VERIFY_ACTIVE; 
	gCall[zCall].avb_bCounter	= 0;

	// now send the results back
	memcpy (&response, &yAvbMsg,  sizeof (struct MsgToApp));
	(void)writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	return(0);
} // END: process_DMOP_VERIFY_CONTINUOUS_SETUP()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int process_DMOP_VERIFY_CONTINUOUS_GET_RESULTS (int zCall)
{
	static char mod[] = "process_DMOP_VERIFY_CONTINUOUS_GET_RESULTS";
	int			rc;
	char		errMsg[512];

	struct MsgToApp				response;

	memset((struct MsgToApp *)&response, '\0', sizeof( struct MsgToApp )); 
	if ( gCall[zCall].continuousVerify == CONTINUOUS_VERIFY_INACTIVE )
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM, ERR,
			"There is currently no continuous voice biometrics being "
			"performed.  Unable to retrieve score.");

		memcpy (&(response),
			&(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
		response.returnCode = -1;
		rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);
		return(0);
	}

	gCall[zCall].msg_avbMsgToApp.score        = gCall[zCall].avb_score;
	gCall[zCall].msg_avbMsgToApp.confidence   = gCall[zCall].avb_confidence;
	gCall[zCall].msg_avbMsgToApp.indThreshold = gCall[zCall].avb_indThreshold;
	
	dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				"gCall[%d].msg_avbMsgToApp.returnCode to %d; [%f, %f, %f]",
				zCall,
				gCall[zCall].msg_avbMsgToApp.returnCode,
				gCall[zCall].avb_score,
				gCall[zCall].avb_indThreshold,
				gCall[zCall].avb_confidence);

	if ( gCall[zCall].msg_avbMsgToApp.returnCode == TEL_AVB_NO_RESULTS )
	{
		memcpy (&(gCall[zCall].msg_avbMsgToApp),
				&(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
		gCall[zCall].msg_avbMsgToApp.score        = gCall[zCall].avb_score;
		gCall[zCall].msg_avbMsgToApp.confidence   = gCall[zCall].avb_confidence;
		gCall[zCall].msg_avbMsgToApp.indThreshold = gCall[zCall].avb_indThreshold;

		gCall[zCall].msg_avbMsgToApp.returnCode = TEL_AVB_NO_RESULTS;
	}
	else
	{
		memcpy (&(gCall[zCall].msg_avbMsgToApp),
			&(gCall[zCall].msgToDM), sizeof (int)*5);
	}
	memcpy (&response, &(gCall[zCall].msg_avbMsgToApp),
					sizeof (struct MsgToApp));

	rc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

	if ( gCall[zCall].msg_avbMsgToApp.returnCode == 0 )
	{
		gCall[zCall].continuousVerify = CONTINUOUS_VERIFY_INACTIVE;
		gCall[zCall].avb_bCounter	= 0;
		memset((Msg_AVBMsgToApp *)&(gCall[zCall].msg_avbMsgToApp), '\0',
				sizeof(gCall[zCall].msg_avbMsgToApp));
	}
					
	return(0);
} // END: process_DMOP_VERIFY_CONTINUOUS_GET_RESULTS()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void avb_process_buffer(int zCall, int bufferSize)
{
	static char	mod[]="avb_process_buffer";
	int		i;
	int		rc;
	char	errMsg[1024];
	char	tmpMsg[1024];
					
	rc = arc_decode_buff (__LINE__, &(gCall[zCall].avb_decode_adc), gCall[zCall].avb_buffer, bufferSize,
					(char *)gCall[zCall].avb_bufferPCM, 8000);
//					(char *)gCall[zCall].avb_bufferPCM, bufferSize*2);
//	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "%d = arc_decode_buff(%d, %d)",
//				rc, bufferSize, 8000);

	gCall[zCall].avb_bCounter = 0;
	rc = VoiceID_ContinuousVerify(
			&gCall[zCall].avb_readyFlag,
			&gCall[zCall].avb_score,
			&gCall[zCall].avb_confidence,
			&gCall[zCall].avb_indThreshold,
			gCall[zCall].pv_VoiceID_conObj,
			gCall[zCall].avb_bufferPCM,
			bufferSize,
			gCall[zCall].avb_pin,
			errMsg);

	if ( rc != 0 )
	{
		sprintf(tmpMsg, 
				"VoiceID_ContinuousVerify() failed to verify pin (%s).  [%d, %s]",
				 gCall[zCall].avb_pin, rc, errMsg);
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, AVB_VERIFY_ERROR, ERR, tmpMsg);
		sprintf(errMsg, "%.*s", 214, tmpMsg);
		
		stop_avb_processsing(zCall, -1, rc, errMsg);
		gCall[zCall].continuousVerify = CONTINUOUS_VERIFY_ACTIVE_COMPLETE;
		return;
	}

	if (gCall[zCall].fp_pcmWavefile != NULL)
	{
		if ( (rc = fwrite(gCall[zCall].avb_bufferPCM, sizeof(short),
                            bufferSize, gCall[zCall].fp_pcmWavefile) ) != bufferSize)
		{
			writePCMWavHeaderToFile (zCall, gCall[zCall].fp_pcmWavefile );
			fclose(gCall[zCall].fp_pcmWavefile);
//			gCall[zCall].avb_bufferPCM = NULL;
			return;
		}

	}

	gCall[zCall].msg_avbMsgToApp.score			= gCall[zCall].avb_score;
	gCall[zCall].msg_avbMsgToApp.confidence		= gCall[zCall].avb_confidence;
	gCall[zCall].msg_avbMsgToApp.indThreshold	= gCall[zCall].avb_indThreshold;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"%d=VoiceID_ContinuousVerify(%s) readyFlag=%d; [%f, %f, %f]",
		rc, gCall[zCall].avb_pin, gCall[zCall].avb_readyFlag, gCall[zCall].avb_score,
		gCall[zCall].avb_confidence, gCall[zCall].avb_indThreshold);

	if ( gCall[zCall].avb_readyFlag )
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				"Verification Complete.");
		gCall[zCall].continuousVerify = CONTINUOUS_VERIFY_ACTIVE_COMPLETE;
		stop_avb_processsing(zCall, 0, rc, "");
	}

} // END: avb_process_buffer()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void stop_avb_processsing(int zCall, int zReturnCode, int zVendorCode,
											char *zErrMsg)
{
	static char	mod[]="stop_avb_processsing";
	int        	rc;
	char		errMsg[512] = "";
	int			j;

	struct MsgToApp				response;
	struct Msg_AVBMsgToApp		avbResponse;

	gCall[zCall].avb_bCounter = 0;

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
	memcpy (&(gCall[zCall].msg_avbMsgToApp),
					&(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	gCall[zCall].msg_avbMsgToApp.returnCode				= zReturnCode;
	gCall[zCall].msg_avbMsgToApp.vendorErrorCode		= zVendorCode;
	sprintf(gCall[zCall].msg_avbMsgToApp.errMsg, "%s", zErrMsg);
	
	gCall[zCall].msg_avbMsgToApp.score			= gCall[zCall].avb_score;
	gCall[zCall].msg_avbMsgToApp.confidence		= gCall[zCall].avb_confidence;
	gCall[zCall].msg_avbMsgToApp.indThreshold	= gCall[zCall].avb_indThreshold;

//	dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM, ERR,
//				"DJB: Set gCall[%d].msg_avbMsgToApp.returnCode to %d; "
//				"msg_avbMsgToApp.score=%f; " 
//				"msg_avbMsgToApp.indThreshold=%f; " 
//				"msg_avbMsgToApp.confidence=%f; ",
//				zCall,
//				gCall[zCall].msg_avbMsgToApp.returnCode,
//				gCall[zCall].msg_avbMsgToApp.score,
//				gCall[zCall].msg_avbMsgToApp.indThreshold,
//				gCall[zCall].msg_avbMsgToApp.confidence);
//
	closeVoiceID(zCall);
	 arc_decoder_free(&(gCall[zCall].avb_decode_adc));
	dynVarLog (__LINE__, zCall, "stop_avb_processsing", REPORT_VERBOSE, TEL_BASE, INFO,
			"Called arc_decoder_free()");

	writePCMWavHeaderToFile (zCall, gCall[zCall].fp_pcmWavefile );
	fclose(gCall[zCall].fp_pcmWavefile);
//	gCall[zCall].avb_bufferPCM = NULL;
} // END: stop_avb_processsing()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static void closeVoiceID(int zCall)
{
	static char	mod[]="closeVoiceID";
	int        	rc;
	char		errMsg[512] = "";

	if ( gCall[zCall].pv_VoiceID_conObj ) 
	{
		if((rc=VoiceID_close(gCall[zCall].pv_VoiceID_conObj, errMsg)) != 0 )
		{	
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					"VoiceIDClose() failed.  [%d, %s]", rc, errMsg);
		}
		else
		{	
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					"VoiceIDClose() succeeded.  [%d, %s]", rc, errMsg);
		}
	}
	
	if ( gCall[zCall].pv_voiceprint )
	{
		errMsg[0] = '\0'; 
		if((rc=VoiceID_release_voiceprint(gCall[zCall].pv_voiceprint, errMsg)) != 0 )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					"VoiceID_release_voiceprint() failed.  [%d, %s]", rc, errMsg);
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					"VoiceID_release_voiceprint() succeeded.  [%d, %s]", rc, errMsg);
		}
	}

} // END: closeVoiceID()

} // extern C


#endif // END: VOICE_BIOMETRICS


///This function is called by the readAndProcessShmData to actually process the DMOP command.
int
processAppRequest (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "processAppRequest" };

	switch (gCall[zCall].msgToDM.opcode)
	{
	case DMOP_START_CALL_PROGRESS_RESPONSE:
		return process_DMOP_START_CALL_PROGRESS_RESPONSE (zCall);
		break;
	case DMOP_END_CALL_PROGRESS:
		return process_DMOP_END_CALL_PROGRESS (zCall);
		break;

	case DMOP_CONFERENCE_START:
		{
			return (process_DMOP_CONFERENCESTART (zCall));
		}
		break;

	case DMOP_INSERTDTMF:
		{
			return (process_DMOP_INSERTDTMF (zCall));
		}
		break;

	case DMOP_CONFERENCE_ADD_USER:
		{
			return (process_DMOP_CONFERENCEADDUSER (zCall));
		}
		break;

	case DMOP_CONFERENCE_REMOVE_USER:
		{
			return (process_DMOP_CONFERENCEREMOVEUSER (zCall));
		}
		break;

	case DMOP_CONFERENCE_STOP:
		{
			return (process_DMOP_CONFERENCESTOP (zCall));
		}
		break;

	case DMOP_CONFERENCE_PLAY_AUDIO:
		{
			return (process_DMOP_CONFERENCE_PLAYAUDIO (zCall));
		}
		break;

	case DMOP_CONFERENCE_START_RESPONSE:
		{
			return (process_DMOP_CONFERENCESTART_RESPONSE (zCall));
		}
		break;

	case DMOP_CONFERENCE_ADD_USER_RESPONSE:
		{
			return (process_DMOP_CONFERENCEADDUSER_RESPONSE (zCall));
		}
		break;

	case DMOP_CONFERENCE_REMOVE_USER_RESPONSE:
		{
			return (process_DMOP_CONFERENCEREMOVEUSER_RESPONSE (zCall));
		}
		break;

	case DMOP_CONFERENCE_STOP_RESPONSE:
		{
			return (process_DMOP_CONFERENCESTOP_RESPONSE (zCall));
		}
		break;

	case DMOP_CONFERENCE_PLAY_AUDIO_RESPONSE:
		{
			return (process_DMOP_CONFERENCE_PLAYAUDIO_RESPONSE (zCall));
		}
		break;

//#ifdef CALEA
	case DMOP_STARTRECORDSILENTLY:
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
					   "Processing DMOP_STARTRECORDSILENTLY.");

	struct Msg_StartRecordSilently *lpRecord =
		(struct Msg_StartRecordSilently *) &(gCall[zCall].msgToDM);
			sprintf (gCall[zCall].silentInRecordFileName, "%s",
					 lpRecord->inFilename);

			if (!gCall[zCall].gSilentInRecordFileFp)
			{
				gCall[zCall].gSilentInRecordFileFp =
					fopen (gCall[zCall].silentInRecordFileName, "w+");
			}

			if (gCall[zCall].gSilentInRecordFileFp)
			{
				/*Update the wav header */
				writeWavHeaderToFile (zCall,
									  gCall[zCall].gSilentInRecordFileFp);
			}

			sprintf (gCall[zCall].silentOutRecordFileName, "%s",
					 lpRecord->outFilename);
			gCall[zCall].gSilentOutRecordFileFp =
				fopen (gCall[zCall].silentOutRecordFileName, "w+");

			if (gCall[zCall].gSilentOutRecordFileFp)
			{
				/*Update the wav header */
				writeWavHeaderToFile (zCall,
									  gCall[zCall].gSilentOutRecordFileFp);
			}

			gCall[zCall].silentRecordFlag = 1;
		}
		break;
	case DMOP_STOPRECORDSILENTLY:
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Processing DMOP_STOPRECORDSILENTLY.");

			gCall[zCall].silentRecordFlag = 0;

			if (gCall[zCall].gSilentInRecordFileFp != NULL)
			{
				//arc_frame_record_to_file(zCall, AUDIO_MIXED, (char *)__func__, __LINE__);
				//writeWavHeaderToFile(zCall, gCall[zCall].gSilentInRecordFileFp);
				//fclose(gCall[zCall].gSilentInRecordFileFp);
				//gCall[zCall].gSilentInRecordFileFp = NULL;
			}

			if (gCall[zCall].gSilentOutRecordFileFp != NULL)
			{
				//arc_frame_record_to_file(zCall, AUDIO_MIXED, (char *)__func__, __LINE__);
				//writeWavHeaderToFile(zCall, gCall[zCall].gSilentOutRecordFileFp);
				//fclose(gCall[zCall].gSilentOutRecordFileFp);
				//gCall[zCall].gSilentOutRecordFileFp = NULL;
			}
		}
		break;

//#endif

	case DMOP_SETDTMF:
		return (process_DMOP_SETDTMF (zCall));
		break;

	case DMOP_BRIDGECONNECT:
		return (process_DMOP_BRIDGECONNECT (zCall));
		break;

	case DMOP_BRIDGE_THIRD_LEG:
		return (process_DMOP_BRIDGE_THIRD_LEG (zCall));
		break;

	case DMOP_INITIATECALL:
		return (process_DMOP_INITIATECALL (zCall));
		break;

	case DMOP_LISTENCALL:
		return (process_DMOP_LISTENCALL (zCall));
		break;

	case DMOP_MEDIACONNECT:
		return (process_DMOP_MEDIACONNECT (zCall));
		break;

	case DMOP_SETGLOBAL:
		return (process_DMOP_SETGLOBAL (zCall));
		break;

	case DMOP_GETGLOBAL:
		return (process_DMOP_GETGLOBAL (zCall));
		break;

	case DMOP_SETGLOBALSTRING:
		return (process_DMOP_SETGLOBALSTRING (zCall));
		break;

	case DMOP_GETGLOBALSTRING:
		return (process_DMOP_GETGLOBALSTRING (zCall));
		break;

	case DMOP_DISCONNECT:
		//setCallState(zCall, CALL_STATE_CALL_CLOSED, __LINE__);
		return (process_DMOP_DISCONNECT (zCall));
		break;

	case DMOP_RESTART_OUTPUT_THREAD:
		return (process_DMOP_RESTART_OUTPUT_THREAD (zCall));
		break;


	case DMOP_RTPDETAILS:
		return (process_DMOP_RTPDETAILS (zCall));
		break;

	case DMOP_EXITALL:
		gCanReadShmData = 0;
		gCanExit = 1;
		break;

	case DMOP_ANSWERCALL:
		return (process_DMOP_ANSWERCALL (zCall));
		break;

	case DMOP_INITTELECOM:
		return (process_DMOP_INITTELECOM (zCall));
		break;

	case DMOP_OUTPUTDTMF:
		return (process_DMOP_OUTPUTDTMF (zCall));
		break;

	case DMOP_RECORD:
		{
	struct Msg_Record *lpRecord =
		(struct Msg_Record *) &(gCall[zCall].msgToDM);
			memcpy (&(gCall[zCall].msgRecord), lpRecord,
					sizeof (struct Msg_Record));
			gCall[zCall].isIFrameDetected = 1;

			__DDNDEBUG__ (DEBUG_MODULE_SR, "Calling addToAppRequestList", "",
						  zCall);
			addToAppRequestList (&(gCall[zCall].msgToDM));
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting pendingInputRequests to 1.");
			gCall[zCall].pendingInputRequests++;

			if (gCall[zCall].inputThreadId <= 0)
			{
	int             rc = -1;

				rc = pthread_create (&gCall[zCall].inputThreadId,
									 &gpthread_attr,
									 inputThread,
									 (void *) &(gCall[zCall].msgToDM));
				if (rc != 0)
				{
					/* Free the thread slot just gotten */
					gCall[zCall].inputThreadId = 0;
				}
			}
		}

		break;

	case DMOP_RECORDMEDIA:
		{
	struct Msg_RecordMedia yTmpRecordMedia;
	struct Msg_RecordMedia *lpRecordMedia = &yTmpRecordMedia;

			gCall[zCall].isIFrameDetected = 0;

			readRecordMediaFile (lpRecordMedia, &(gCall[zCall].msgToDM));

			dynVarLog (__LINE__, 0, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						"lpRecordMedia->terminateChar=%d, GV_RecordTermChar=%d",
						lpRecordMedia->terminateChar, gCall[zCall].GV_RecordTermChar);

			if ( ( ( lpRecordMedia->terminateChar == 0 ) ||				// BT-226
			       ( lpRecordMedia->terminateChar == 32 ) ) && 
			     ( ( gCall[zCall].GV_RecordTermChar != 0 ) ||
			       ( gCall[zCall].GV_RecordTermChar != 32 ) ) )
			{
				lpRecordMedia->terminateChar = gCall[zCall].GV_RecordTermChar;
			}

			switch (lpRecordMedia->audioCompression)
			{
			case COMP_WAV:
			case COMP_MVP:
			case COMP_711:
			case COMP_729A:
			case COMP_729B:
			case COMP_729:
			case COMP_WAV_H263R_QCIF:
			case COMP_711_H263R_QCIF:
			case COMP_729_H263R_QCIF:
			case COMP_729A_H263R_QCIF:
			case COMP_729B_H263R_QCIF:
			case COMP_WAV_H263R_CIF:
			case COMP_711_H263R_CIF:
			case COMP_729_H263R_CIF:
			case COMP_729A_H263R_CIF:
			case COMP_729B_H263R_CIF:
				gCall[zCall].recordOption = WITHOUT_RTP;
				break;
			default:
				gCall[zCall].recordOption = WITH_RTP;
				break;

			}

			lpRecordMedia->appPid = gCall[zCall].msgToDM.appPid;
			lpRecordMedia->appPassword = gCall[zCall].msgToDM.appPassword;
			lpRecordMedia->appCallNum = gCall[zCall].msgToDM.appCallNum;
			lpRecordMedia->appRef = gCall[zCall].msgToDM.appRef;
			lpRecordMedia->opcode = gCall[zCall].msgToDM.opcode;

			memset (&(gCall[zCall].msgRecordMedia), 0,
					sizeof (struct Msg_RecordMedia));

			memcpy (&(gCall[zCall].msgRecordMedia), lpRecordMedia,
					sizeof (struct Msg_RecordMedia));

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Got DMOP_RECORDMEDIA, audioFileName=%s, terminateChar=[%c,%d].",
					   gCall[zCall].msgRecordMedia.audioFileName,
						gCall[zCall].msgRecordMedia.terminateChar,
						gCall[zCall].msgRecordMedia.terminateChar);

			if (gCall[zCall].msgRecordMedia.audioFileName != NULL &&
					 gCall[zCall].msgRecordMedia.audioFileName[0] != '\0')
			{

				__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "Creating msgRecordMedia",
							  gCall[zCall].msgRecordMedia.audioFileName,
							  zCall);

				gCall[zCall].msgRecord.opcode = lpRecordMedia->opcode;
				gCall[zCall].msgRecord.appCallNum = lpRecordMedia->appCallNum;
				gCall[zCall].msgRecord.appPid = lpRecordMedia->appPid;
				gCall[zCall].msgRecord.appRef = lpRecordMedia->appRef;
				gCall[zCall].msgRecord.appPassword =
					lpRecordMedia->appPassword;
				gCall[zCall].msgRecord.party = lpRecordMedia->party;
				//gCall[zCall].msgRecord.allParties = lpRecordMedia->allParties;    
				memcpy (gCall[zCall].msgRecord.filename,
						lpRecordMedia->audioFileName, 100);
				gCall[zCall].msgRecord.record_time =
					lpRecordMedia->recordTime;
				gCall[zCall].msgRecord.compression =
					lpRecordMedia->audioCompression;
				gCall[zCall].msgRecord.overwrite =
					lpRecordMedia->audioOverwrite;
				gCall[zCall].msgRecord.lead_silence =
					lpRecordMedia->lead_silence;


				gCall[zCall].msgRecord.trail_silence =
					lpRecordMedia->trail_silence;
				memcpy (gCall[zCall].msgRecord.beepFile,
						lpRecordMedia->beepFile, 96);
				gCall[zCall].msgRecord.interrupt_option =
					lpRecordMedia->interruptOption;
				gCall[zCall].msgRecord.terminate_char =
					lpRecordMedia->terminateChar;
				gCall[zCall].msgRecord.synchronous = lpRecordMedia->sync;

				gCall[zCall].isIFrameDetected = 1;

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "beepFileName=%s, beepFile=%s.",
						   gCall[zCall].msgRecord.beepFile,
						   lpRecordMedia->beepFile);

				if (gCall[zCall].msgRecord.beepFile[0] != '\0')
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Setting isBeepActive to 1.");
					gCall[zCall].isBeepActive = 1;
				}
				else
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Setting isBeepActive to 0.");
					gCall[zCall].isBeepActive = 0;
				}

				__DDNDEBUG__ (DEBUG_MODULE_SR, "Calling addToAppRequestList", "",
							  zCall);
				addToAppRequestList (&(gCall[zCall].msgToDM));

				gCall[zCall].pendingInputRequests++;

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "pendingInputRequests = %d.",
						   gCall[zCall].pendingInputRequests);

				if (gCall[zCall].inputThreadId <= 0)
				{
	int             rc = -1;

					rc = pthread_create (&gCall[zCall].inputThreadId,
										 &gpthread_attr,
										 inputThread,
										 (void *) &(gCall[zCall].msgToDM));
					if (rc != 0)
					{
						/* Free the thread slot just gotten */
						gCall[zCall].inputThreadId = 0;
					}
				}

			}
		}

		break;

	case DMOP_FAX_PLAYTONE:
		if (gSendFaxTone == 0)
		{
	struct MsgToApp msg;

			memcpy (&msg, &gCall[zCall].msgToDM, sizeof (msg));
			msg.opcode = DMOP_FAX_PLAYTONE;
			writeGenericResponseToApp (zCall, &msg, mod, __LINE__);
		}

		break;

	case DMOP_FAX_STOPTONE:

		if (gCall[zCall].sentFaxPlaytone == 0)
		{
	struct Msg_Fax_PlayTone faxPlayTone;

			memset (&faxPlayTone, 0, sizeof (faxPlayTone));
			memcpy (&faxPlayTone, &(gCall[zCall].msgToDM),
					sizeof (faxPlayTone));
			faxPlayTone.opcode = DMOP_FAX_PLAYTONE;
			writeGenericResponseToApp (zCall,
									   (struct MsgToApp *) &faxPlayTone, mod,
									   __LINE__);
			gCall[zCall].sentFaxPlaytone = 1;
		}
		break;

	case DMOP_RECVFAX:
		{
	struct Msg_RecvFax *lpGetFax =
		(struct Msg_RecvFax *) &(gCall[zCall].msgToDM);

			sprintf (lpGetFax->nameValue, "destPort=%d&destIP=%s",
					 gCall[zCall].remoteRtpPort, gCall[zCall].remoteRtpIp);

			memcpy (&(gCall[zCall].msgFax), lpGetFax,
					sizeof (struct MsgToDM));

			time (&gCall[zCall].msgFax.timeStamp);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Sending nameValue=(%s)", lpGetFax->nameValue);

#ifdef ACU_LINUX
			yRc =
				sendRequestToSTonesFaxClient (mod,
											  (struct MsgToDM *)
											  &(gCall[zCall].msgFax), zCall);
#else
	int             idx = zCall % 48;

			if (idx > -1 && idx < 48)
			{
				yRc =
					startFaxThread (zCall, lpGetFax, ARC_FAX_SESSION_INBOUND);
			}
#endif
			if (yRc < 0)
			{
	struct MsgToApp response;

				response.appCallNum = zCall;
				response.opcode = lpGetFax->opcode;
				response.appPassword = lpGetFax->appPassword;
				response.appPid = lpGetFax->appPid;
				response.appRef = lpGetFax->appRef;
				response.returnCode = yRc;

				sprintf (response.message, "%s", "Failed to Start FAX rc=%d",
						 yRc);

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);
				break;
			}
			// gCall[zCall].isSendRecvFaxActive = 1;

			__DDNDEBUG__ (DEBUG_MODULE_SR, "calling addToAppRequestList", "",
						  zCall);
			addToAppRequestList (&(gCall[zCall].msgToDM));

#if 0							// don't need this; aculab client does it. - djb
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
					   "Setting pendingInputRequests = 1");
			gCall[zCall].pendingInputRequests++;

			if (gCall[zCall].inputThreadId <= 0)
			{
	int             rc = -1;

				rc = pthread_create (&gCall[zCall].inputThreadId,
									 &gpthread_attr, inputThread,
									 (void *) &(gCall[zCall].msgToDM));

				if (rc != 0)
				{
					/* Free the thread slot just gotten */
					gCall[zCall].inputThreadId = 0;
				}

			}
#endif

			break;

		}						/*END: case: DMOP_RECVFAX */
	case DMOP_SENDFAX:
		{
	struct Msg_SendFax *lpSendFax =
		(struct Msg_SendFax *) &(gCall[zCall].msgToDM);

#if 0
			sprintf (lpSendFax->nameValue,
					 "destPort=%d&destIP=%s&localPort=%d",
					 gCall[zCall].remoteRtpPort, gCall[zCall].remoteRtpIp,
					 gCall[zCall].localSdpPort);
#endif
			sprintf (lpSendFax->nameValue, "destPort=%d&destIP=%s",
					 gCall[zCall].remoteRtpPort, gCall[zCall].remoteRtpIp);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "lpSendFax->faxFile=(%s) nameValue=(%s)",
					   lpSendFax->faxFile, lpSendFax->nameValue);

//          long lQueueElementId = -1;
			memcpy (&(gCall[zCall].msgSendFax), lpSendFax,
					sizeof (struct Msg_SendFax));
			time (&gCall[zCall].msgSendFax.timeStamp);
#ifdef ACU_LINUX
			yRc =
				sendRequestToSTonesFaxClient (mod,
											  (struct MsgToDM *)
											  &(gCall[zCall].msgSendFax),
											  zCall);
#else
	int             idx = zCall % 48;

			if (idx > -1 && idx < 48)
			{
				yRc =
					startFaxThread (zCall, lpSendFax,
									ARC_FAX_SESSION_OUTBOUND);
			}
#endif
			if (yRc < 0)
			{
	struct MsgToApp response;

				response.appCallNum = zCall;
				response.opcode = lpSendFax->opcode;
				response.appPassword = lpSendFax->appPassword;
				response.appPid = lpSendFax->appPid;
				response.appRef = lpSendFax->appRef;
				response.returnCode = yRc;

				//sprintf(response.message, "%s", "Failed to send request to Stones Client");
				sprintf (response.message, "%s", "Fax request failed.");

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);
				break;
			}

			// gCall[zCall].isSendRecvFaxActive = 1;

			if (yRc >= 0)
			{
				/*
				 *  Do nothing.    
				 *  As soon as DMOP_START_CALL_PROGRESS_RESPONSE is received, 
				 *  RTP data will be pumped to Tones Client from input thread. 
				 */
			}

			if (gCall[zCall].outputThreadId <= 0)
			{
	struct MsgToApp response;

				response.appCallNum = zCall;
				response.opcode = DMOP_SENDFAX;
				response.appPassword = lpSendFax->appPassword;
				response.appPid = lpSendFax->appPid;
				response.appRef = lpSendFax->appRef;
				response.returnCode = -1;

				sprintf (response.message, "%s", "No media available");

				__DDNDEBUG__ (DEBUG_MODULE_CALL,
							  "Output thread was not running",
							  "Returning -1 <No media available>", -1);

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);

				return (0);
			}

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearAppRequestList",
						  "", -1);

			clearAppRequestList (zCall);

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "After Calling clearSpeakList",
						  "", -1);

			__DDNDEBUG__ (DEBUG_MODULE_SR, "Calling addToAppRequestList", "",
						  zCall);
			addToAppRequestList (&(gCall[zCall].msgToDM));

			gCall[zCall].pendingOutputRequests++;

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "", -1);

			break;

		}						/*END:DMOP_SENDFAX */

	case DMOP_FAX_PROCEED:
		(void) process_DMOP_FAX_PROCEED (zCall);
		break;
	case DMOP_FAX_COMPLETE:
		(void) process_DMOP_FAX_COMPLETE (zCall);
		gCall[zCall].isSendRecvFaxActive = 0;
		break;

	case DMOP_FAX_SEND_SDPINFO:

#ifdef ACU_LINUX

		yRc =
			sendRequestToSTonesFaxClient (mod, &gCall[zCall].msgToDM, zCall);
		if (yRc < 0)
		{
	struct MsgToApp response;

			response.appCallNum = zCall;
			response.opcode = gCall[zCall].msgToDM.opcode;
			response.appPassword = gCall[zCall].msgToDM.appPassword;
			response.appPid = gCall[zCall].msgToDM.appPid;
			response.appRef = gCall[zCall].msgToDM.appRef;
			response.returnCode = yRc;

			sprintf (response.message, "%s",
					 "Failed to send request to Stones Client");

			writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			break;
		}
#else
		yRc = startFaxThread (zCall, &gCall[zCall].msgToDM, -1);
#endif
		break;

	case DMOP_FAX_RESERVE_RESOURCE:
		{

			if (gCall[zCall].sendCallProgressAudio == 1)
			{
	struct Msg_CallProgress yMsgCallProgress;

				yMsgCallProgress.opcode = DMOP_CANCEL_CALL_PROGRESS;
				yMsgCallProgress.origOpcode = DMOP_INITIATECALL;
				yMsgCallProgress.appCallNum = zCall;
				yMsgCallProgress.appPid = gCall[zCall].msgToDM.appPid;
				yMsgCallProgress.appRef = gCall[zCall].msgToDM.appRef;
				yMsgCallProgress.appPassword =
					gCall[zCall].msgToDM.appPassword;

				gCall[zCall].sendCallProgressAudio = 0;

#ifdef ACU_LINUX
				sendRequestToSTonesClientSpecialFifo (zCall, mod,
													  (struct MsgToDM *)
													  &yMsgCallProgress);
#endif
				//Now send Answer Call response to App
	struct MsgToApp response;

				response.appCallNum = zCall;
				response.opcode = DMOP_INITIATECALL;
				response.appPassword = gCall[zCall].msgToDM.appPassword;
				response.appPid = gCall[zCall].msgToDM.appPid;
				response.appRef = gCall[zCall].msgToDM.appRef;
				response.returnCode = 151;
				sprintf (response.message, "%d", zCall);

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);

			}
	struct Msg_FaxReserveResource *lpFaxReserveResource =
		(struct Msg_FaxReserveResource *) &(gCall[zCall].msgToDM);

			sprintf (lpFaxReserveResource->data, "destPort=%d&destIP=%s",
					 gCall[zCall].remoteRtpPort, gCall[zCall].remoteRtpIp);

			time (&lpFaxReserveResource->timeStamp);
			gCall[zCall].isFaxReserverResourceCalled = 1;
#ifdef ACU_LINUX
			yRc =
				sendRequestToSTonesFaxClient (mod,
											  (struct MsgToDM *)
											  lpFaxReserveResource, zCall);
#endif

			if (yRc < 0)
			{
	struct MsgToApp response;

				response.appCallNum = zCall;
				response.opcode = gCall[zCall].msgToDM.opcode;
				response.appPassword = gCall[zCall].msgToDM.appPassword;
				response.appPid = gCall[zCall].msgToDM.appPid;
				response.appRef = gCall[zCall].msgToDM.appRef;
				response.returnCode = yRc;

				sprintf (response.message, "%s",
						 "Failed to send request to Stones Client");

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);
				break;
			}
		}
		break;

	case DMOP_FAX_RELEASE_RESOURCE:
		sprintf (gCall[zCall].msgToDM.data, "destPort=%d&destIP=%s",
				 gCall[zCall].remoteRtpPort, gCall[zCall].remoteRtpIp);
#ifdef ACU_LINUX
		yRc =
			sendRequestToSTonesFaxClient (mod, &gCall[zCall].msgToDM, zCall);
#endif
		if (yRc < 0)
		{
	struct MsgToApp response;

			response.appCallNum = zCall;
			response.opcode = gCall[zCall].msgToDM.opcode;
			response.appPassword = gCall[zCall].msgToDM.appPassword;
			response.appPid = gCall[zCall].msgToDM.appPid;
			response.appRef = gCall[zCall].msgToDM.appRef;
			response.returnCode = yRc;

			sprintf (response.message, "%s",
					 "Failed to send request to Stones Client");

			writeGenericResponseToApp (zCall, &response, mod, __LINE__);
			break;
		}
		break;

	case DMOP_PLAYMEDIA:
//	case DMOP_PLAYMEDIAVIDEO:
//  case DMOP_PLAYMEDIAAUDIO:
	{
	struct Msg_PlayMedia *lpPlay = NULL;
	long            lQueueElementId = -1;
	int             type = 1;
	struct Msg_PlayMedia yTmpMediaInfo;
	int		yIntPlayMediaRetcode = -1;

			//if (gCall[zCall].msgToDM.opcode == DMOP_PLAYMEDIAVIDEO ||
			if (gCall[zCall].msgToDM.opcode == DMOP_PLAYMEDIAAUDIO)
			{
	struct Msg_Speak yTmpMsgSpeak;

				memcpy (&yTmpMsgSpeak,
						(struct Msg_Speak *) &(gCall[zCall].msgToDM),
						sizeof (struct Msg_Speak));

				yTmpMediaInfo.opcode = DMOP_PLAYMEDIA;
				yTmpMediaInfo.appCallNum = gCall[zCall].msgToDM.appCallNum;
				yTmpMediaInfo.appPid = gCall[zCall].msgToDM.appPid;
				yTmpMediaInfo.appRef = gCall[zCall].msgToDM.appRef;
				yTmpMediaInfo.appPassword = gCall[zCall].msgToDM.appPassword;
				yTmpMediaInfo.party = yTmpMsgSpeak.allParties;
				yTmpMediaInfo.interruptOption = yTmpMsgSpeak.interruptOption;
				yTmpMediaInfo.sync = yTmpMsgSpeak.synchronous;
				yTmpMediaInfo.audioinformat = PHRASE_FILE;
				yTmpMediaInfo.audiooutformat = PHRASE_FILE;
				yTmpMediaInfo.audioLooping = NO;
				yTmpMediaInfo.addOnCurrentPlay = YES;
				yTmpMediaInfo.isReadyToPlay = NO;

				sprintf (yTmpMediaInfo.audioFileName, "%s", "NULL");
				sprintf (yTmpMediaInfo.key, "%s", yTmpMsgSpeak.key);

				lpPlay = &yTmpMediaInfo;
			}
			else
			{
				yIntPlayMediaRetcode = readPlayMediaFile (&yTmpMediaInfo, &(gCall[zCall].msgToDM),
								   zCall);

				lpPlay = &yTmpMediaInfo;
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "interruptOpt=%d, data=(%s)",
						   lpPlay->interruptOption, gCall[zCall].msgToDM.data);

				lpPlay->appPassword = gCall[zCall].msgToDM.appPassword;
				lpPlay->appPid = gCall[zCall].msgToDM.appPid;
				lpPlay->appRef = gCall[zCall].msgToDM.appRef;
				lpPlay->appCallNum = gCall[zCall].msgToDM.appCallNum;
			}

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "appRef=%d, lastStopActivityAppRef=%d",
					   lpPlay->appRef, gCall[zCall].lastStopActivityAppRef);

			if (lpPlay->appRef <= gCall[zCall].lastStopActivityAppRef)
			{
				break;
			}

			if (gCall[zCall].msgToDM.opcode != DMOP_PLAYMEDIAVIDEO)
			{
				gCall[zCall].currentOpcode = gCall[zCall].msgToDM.opcode;
			}
			if (gCall[zCall].outputThreadId <= 0 || yIntPlayMediaRetcode < 0)
			{
				struct MsgToApp response;

				response.appCallNum = zCall;
				response.opcode = DMOP_PLAYMEDIA;
				response.appPassword = lpPlay->appPassword;
				response.appPid = lpPlay->appPid;
				response.appRef = lpPlay->appRef;
				response.returnCode = -1;

				if(gCall[zCall].outputThreadId <= 0)
				{
					sprintf (response.message, "%s", "No audio media available");

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
							  "Output thread was not running",
							  "Returning -1 <No audio media available>", -1);
				}
				else
				{
					sprintf (response.message, "%s", "Faild to read media file. ");

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
							  "Failed to read media file.",
							  "Returning -1. ", -1);
				}

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);

				return (0);
			}

			if (lpPlay->sync == PUT_QUEUE_ASYNC &&
				lpPlay->key != NULL && lpPlay->key[0] != '\0')
			{
				if (lpPlay->audioFileName != NULL &&
					lpPlay->audioFileName[0] != '\0' &&
					strcmp (lpPlay->audioFileName, "NULL") != 0)
				{
	SpeakList      *pSpeakList;

	long            lQueueId;
	char            lBuf[256];
	int             keyMatchFound = 0;
	char            tempExt[30];
	char            tempKey[30];

					sscanf (lpPlay->key, "%[^_]_%[^=]", tempKey, tempExt);
	int             key = atoi (tempKey);

					//sscanf(lpPlay->key, "%ld^%s", &lQueueId, lBuf);

					__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
								  "PUT_QUEUE_ASYNC:FILE and KEY",
								  lpPlay->audioFileName, lQueueId);

					pthread_mutex_lock (&gCall[zCall].gMutexSpeak);

					pSpeakList = gCall[zCall].pFirstSpeak;

					while (pSpeakList != NULL)
					{
						if (key == (long) pSpeakList->msgSpeak.appRef)
						{
							sprintf (pSpeakList->msgSpeak.file, "%s",
									 lpPlay->audioFileName);

							__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
										  "PUT_QUEUE_ASYNC:FILE ASSIGNED",
										  lpPlay->audioFileName, 0);
							keyMatchFound = 1;
							break;
						}

						pSpeakList = pSpeakList->nextp;
					}

					pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
					if (keyMatchFound == 0)
					{
						pthread_mutex_lock (&gCall[zCall].gMutexBkpSpeak);

						pSpeakList = gCall[zCall].pFirstBkpSpeak;

						while (pSpeakList != NULL)
						{
							if (key == (long) pSpeakList->msgSpeak.appRef)
							{
								sprintf (pSpeakList->msgSpeak.file, "%s",
										 lpPlay->audioFileName);

								__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
											  "PUT_QUEUE_ASYNC:FILE ASSIGNED",
											  lpPlay->audioFileName, 0);

								break;
							}

							pSpeakList = pSpeakList->nextp;
						}

						pthread_mutex_unlock (&gCall[zCall].gMutexBkpSpeak);
						break;
					}

				}
				break;
			}

			if ((lpPlay->sync == PLAY_QUEUE_SYNC)
				|| (lpPlay->sync == PLAY_QUEUE_ASYNC))
			{
	int             isBkpQueue = 0;
	int             isAlreadyAddedToSpeakList = 0;

				if (gCall[zCall].pFirstBkpSpeak != NULL)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
							   INFO,
							   "Got PLAY_QUEUE Bkp Queue is not null copying bkp queue to current queue");

					if (lpPlay->audioFileName != NULL &&
						lpPlay->audioFileName[0] == '1')
					{
						__DDNDEBUG__ (DEBUG_MODULE_SR,
									  "Calling addToAppRequestList", "", zCall);
						addToAppRequestList (&(gCall[zCall].msgToDM));
						gCall[zCall].pendingOutputRequests = 1;
					}
					clearSpeakList (zCall, __LINE__);
					copyBkpToCurrentQueue (zCall);
					isBkpQueue = 0;

				}
 
				if (lpPlay->audioFileName != NULL &&
					lpPlay->audioFileName[0] == '1')
				{
	long            lQueueElementId = -1;
	struct Msg_Speak yTmpMsgSpeak;
	struct Msg_Speak *lpSpeak = &(yTmpMsgSpeak);

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Calling clearAppRequestList", "", -1);

					clearAppRequestList (zCall);
					lpPlay->audioFileName[0] = '\0';
					memcpy (lpSpeak,
							(struct Msg_Speak *) &(gCall[zCall].msgToDM),
							sizeof (struct Msg_Speak));
					sprintf (lpSpeak->file, "%s", lpPlay->audioFileName);
					lpSpeak->synchronous = lpPlay->sync;
					lpSpeak->list = 0;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "lpSpeak->file=%s, lpSpeak->appref=%d, lpSpeak->opcode=%d.",
							   lpSpeak->file, lpSpeak->appRef,
							   lpSpeak->opcode);

					gCall[zCall].audioLooping = lpPlay->audioLooping;
					if (lpPlay->audioLooping != SPECIFIC)
					{
						gCall[zCall].audioLooping = lpPlay->audioLooping;
					}

					__DDNDEBUG__ (DEBUG_MODULE_SR, "calling addToAppRequestList",
								  "", zCall);
					addToAppRequestList (&(gCall[zCall].msgToDM));
					addToSpeakList (lpSpeak, &lQueueElementId, lpPlay, __LINE__);

					isAlreadyAddedToSpeakList = 1;

					gCall[zCall].pendingOutputRequests++;

				}
			}

			if ((lpPlay->sync == ASYNC_QUEUE) ||
				(lpPlay->sync == ASYNC) ||
				lpPlay->sync == PUT_QUEUE ||
				lpPlay->sync == PUT_QUEUE_ASYNC ||
				lpPlay->sync == PLAY_QUEUE_ASYNC)
			{
	struct MsgToApp response;

				memcpy (&response, lpPlay, sizeof (struct MsgToApp));

				response.opcode = lpPlay->opcode;
				response.appCallNum = lpPlay->appCallNum;
				response.appPid = lpPlay->appPid;
				response.appRef = lpPlay->appRef;
				response.appPassword = lpPlay->appPassword;
				response.returnCode = 0;
				strcpy (response.message, "TEL_PlayMedia worked");

				if (lpPlay->sync == PUT_QUEUE_ASYNC && type == 2)
				{
					sprintf (response.message, "%d_audio;",
							 lpPlay->appRef);
					yRc =
						writeGenericResponseToApp (lpPlay->appCallNum,
												   &response, mod, __LINE__);
				}
				else if (lpPlay->sync == PUT_QUEUE_ASYNC)
				{
					sprintf (response.message, "%d_audio", lpPlay->appRef);
					yRc =
						writeGenericResponseToApp (lpPlay->appCallNum,
												   &response, mod, __LINE__);

				}
				else
				{
					yRc =
						writeGenericResponseToApp (lpPlay->appCallNum,
												   &response, mod, __LINE__);
				}

			}

			if (lpPlay->sync == PUT_QUEUE || lpPlay->sync == PUT_QUEUE_ASYNC)
			{
	long            lQueueElementId = -1;
	struct Msg_Speak yTmpMsgSpeak;
	struct Msg_Speak *lpSpeak = &(yTmpMsgSpeak);

				lpSpeak->opcode = lpPlay->opcode;
				lpSpeak->appCallNum = lpPlay->appCallNum;
				lpSpeak->appPid = lpPlay->appPid;
				lpSpeak->appRef = lpPlay->appRef;
				lpSpeak->appPassword = lpPlay->appPassword;	/* Unique # generated by the DM */
				lpSpeak->list = 0;	/* 0=no list, 1= file is a list file */
				lpSpeak->allParties = lpPlay->party;	/* 0=No, 1=Yes */
				lpSpeak->interruptOption = lpPlay->interruptOption;	/* Not used by DM, only APIs */
				lpSpeak->synchronous = lpPlay->sync;	/* 0=No, 1=Yes */
				lpSpeak->deleteFile = 0;	/* 0=No, 1=Yes */
				sprintf (lpSpeak->key, "%s", lpPlay->key);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "audioFileName=%s, interruptOption=%d.",
						   lpPlay->audioFileName, 
						   lpSpeak->interruptOption);

				sprintf (lpSpeak->file, "%s", lpPlay->audioFileName);

				if (lpPlay->audioFileName != NULL &&
						 lpPlay->audioFileName[0] != '\0' &&
						 //          (strcmp( lpPlay->audioFileName, "NULL") != 0)&&
						 lpPlay->addOnCurrentPlay == NO &&
						 gCall[zCall].sendingSilencePackets == 0)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Adding to backup queue.");
					addToBkpSpeakList (lpSpeak, &lQueueElementId, lpPlay);
				}
				else
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Adding to current queue.");
					addToSpeakList (lpSpeak, &lQueueElementId,
									&yTmpMediaInfo, __LINE__);
				}
			}

			if ((lpPlay->sync == SYNC) || (lpPlay->sync == ASYNC))
			{
				if (lpPlay->audioFileName != NULL &&
					lpPlay->audioFileName[0] != '\0' &&
					strstr (lpPlay->audioFileName, "NULL") == NULL)
				{
	long            lQueueElementId = -1;
	struct Msg_Speak yTmpMsgSpeak;
	struct Msg_Speak *lpSpeak = &(yTmpMsgSpeak);

					memcpy (lpSpeak,
							(struct Msg_Speak *) &(gCall[zCall].msgToDM),
							sizeof (struct Msg_Speak));

					//interrupt
					sprintf (lpSpeak->file, "%s", lpPlay->audioFileName);
					lpSpeak->synchronous = lpPlay->sync;
					lpSpeak->list = 0;

					/*Cleanup the existing queue */
					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Calling clearAppRequestList", "", -1);
					clearAppRequestList (zCall);

					__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearSpeakList",
								  "", -1);
					clearSpeakList (zCall, __LINE__);

						__DDNDEBUG__ (DEBUG_MODULE_SR,
									  "calling addToSpeakList", "", zCall);
						addToAppRequestList (&(gCall[zCall].msgToDM));
						addToSpeakList (lpSpeak, &lQueueElementId, lpPlay, __LINE__);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Increamenting pendingOutputRequests.");

					gCall[zCall].pendingOutputRequests++;

					__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "", -1);

				}
			}
		}						/*END:DMOP_PLAYMEDIA */

		break;

	case DMOP_SPEAK:
	case DMOP_PLAYMEDIAAUDIO:
		{
	struct Msg_Speak *lpSpeak = (struct Msg_Speak *) &(gCall[zCall].msgToDM);
	long            lQueueElementId = -1;

			if (gCall[zCall].msgToDM.opcode != DMOP_PLAYMEDIAAUDIO)
			{
				gCall[zCall].currentOpcode = gCall[zCall].msgToDM.opcode;
				gCall[zCall].msgToDM.opcode = DMOP_SPEAK;
			}

			gCall[zCall].currentOpcode = DMOP_PLAYMEDIAAUDIO;	// we should never get DMOP_SPEAK

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "file=%s, key=%s, appRef=%d, lastStopActivityAppRef=%d.",
					   lpSpeak->file,
					   lpSpeak->key,
					   lpSpeak->appRef, gCall[zCall].lastStopActivityAppRef);

			if (lpSpeak->appRef <= gCall[zCall].lastStopActivityAppRef)
			{
	struct MsgToApp response;

				sprintf (response.message, "\0");
				response.appCallNum = zCall;
				response.opcode = DMOP_PLAYMEDIA;
				response.appPassword = lpSpeak->appPassword;
				response.appPid = lpSpeak->appPid;
				response.appRef = lpSpeak->appRef;
				response.returnCode = 0;

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);

				return (0);
			}

//          printSpeakList(gCall[zCall].pFirstSpeak);

			if (gCall[zCall].outputThreadId <= 0)
			{
	struct MsgToApp response;

				sprintf (response.message, "\0");
				response.appCallNum = zCall;
				response.opcode = DMOP_PLAYMEDIA;
				response.appPassword = lpSpeak->appPassword;
				response.appPid = lpSpeak->appPid;
				response.appRef = lpSpeak->appRef;
				response.returnCode = -1;

				sprintf (response.message, "%s", "No media available");

				__DDNDEBUG__ (DEBUG_MODULE_CALL,
							  "Output thread was not running",
							  "Returning -1 <No media available>", -1);

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);

				return (0);
			}

			if (lpSpeak->synchronous == PUT_QUEUE_ASYNC &&
				lpSpeak->key != NULL && lpSpeak->key[0] != '\0')
			{
				if (lpSpeak->file[0] != '\0')
				{

	SpeakList      *pSpeakList;
	long            lQueueId;
	char            lBuf[256];

					sscanf (lpSpeak->key, "%ld^%s", &lQueueId, lBuf);

					__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
								  "PUT_QUEUE_ASYNC:FILE and KEY",
								  lpSpeak->file, lQueueId);

					pthread_mutex_lock (&gCall[zCall].gMutexSpeak);

					pSpeakList = gCall[zCall].pFirstSpeak;

					while (pSpeakList != NULL)
					{
						if (lQueueId == (long) pSpeakList->msgSpeak.appRef)
						{
							sprintf (pSpeakList->msgSpeak.file, "%s",
									 lpSpeak->file);

							__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
										  "PUT_QUEUE_ASYNC:FILE ASSIGNED",
										  lpSpeak->file, 0);

							break;
						}

						pSpeakList = pSpeakList->nextp;
					}

					pthread_mutex_unlock (&gCall[zCall].gMutexSpeak);
					break;
				}
			}

			if ((lpSpeak->synchronous == ASYNC_QUEUE) ||
				(lpSpeak->synchronous == ASYNC) ||
				lpSpeak->synchronous == PUT_QUEUE ||
				lpSpeak->synchronous == PUT_QUEUE_ASYNC ||
				lpSpeak->synchronous == PLAY_QUEUE_ASYNC)
			{
	struct MsgToApp response;

				memcpy (&response, lpSpeak, sizeof (struct MsgToApp));

				response.opcode = lpSpeak->opcode;
				response.appCallNum = lpSpeak->appCallNum;
				response.appPid = lpSpeak->appPid;
				response.appRef = lpSpeak->appRef;
				response.appPassword = lpSpeak->appPassword;
				response.returnCode = 0;
				strcpy (response.message, "TEL_Speak worked");

				if (lpSpeak->synchronous == PUT_QUEUE_ASYNC)
				{
					sprintf (response.message, "%d^0", lpSpeak->appRef);
					yRc =
						writeGenericResponseToApp (lpSpeak->appCallNum,
												   &response, mod, __LINE__);
				}
				else
				{
					if (lpSpeak->synchronous == ASYNC
						&& (strcmp (lpSpeak->key, "faxtone") == 0))
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "App ref is 999999; not responding back to application.");
					}
					else
						yRc =
							writeGenericResponseToApp (lpSpeak->appCallNum,
													   &response, mod,
													   __LINE__);
				}

			}

			/*Cleanup the existing queue */
			if ((lpSpeak->synchronous == SYNC)
				|| (lpSpeak->synchronous == ASYNC))
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "lpSpeak->synchronous=%d, clearing clearAppRequestList.",
						   lpSpeak->synchronous);
				__DDNDEBUG__ (DEBUG_MODULE_CALL,
							  "Calling clearAppRequestList", "", -1);
				clearAppRequestList (zCall);

				__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearSpeakList", "",
							  -1);
				clearSpeakList (zCall, __LINE__);
			}

			if ((lpSpeak->synchronous == PLAY_QUEUE_SYNC)
				|| (lpSpeak->synchronous == PLAY_QUEUE_ASYNC))
			{

				__DDNDEBUG__ (DEBUG_MODULE_CALL,
							  "Calling clearAppRequestList", "", -1);

				clearAppRequestList (zCall);
				lpSpeak->file[0] = '\0';
			}

			if (lpSpeak->synchronous == PUT_QUEUE
				|| lpSpeak->synchronous == PUT_QUEUE_ASYNC)
			{
				addToSpeakList (lpSpeak, &lQueueElementId, __LINE__);
			}
			else if (lpSpeak->synchronous == PLAY_QUEUE_SYNC ||
					 lpSpeak->synchronous == PLAY_QUEUE_ASYNC ||
					 lpSpeak->synchronous == SYNC ||
					 lpSpeak->synchronous == ASYNC)
			{
				if (lpSpeak->synchronous == PLAY_QUEUE_ASYNC ||
					lpSpeak->synchronous == PLAY_QUEUE_SYNC)
				{
					if (gCall[zCall].pFirstSpeak == NULL)
					{
	struct MsgToApp response;

						sprintf (response.message, "\0");
						response.appCallNum = zCall;
						response.opcode = DMOP_SPEAK;
						response.appPassword = lpSpeak->appPassword;
						response.appPid = lpSpeak->appPid;
						response.appRef = lpSpeak->appRef;
						response.returnCode = -1;

						writeGenericResponseToApp (zCall, &response, mod,
												   __LINE__);

						lpSpeak->file[0] = '\0';

						break;
					}

					lpSpeak->file[0] = '\0';
				}

				__DDNDEBUG__ (DEBUG_MODULE_CALL, "calling addToSpeakList", "",
							  zCall);
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "lpSpeak->file=%s, lpSpeak->synchronous=%d, calling addToAppRequestList.",
						   lpSpeak->file, lpSpeak->synchronous);

				addToAppRequestList (&(gCall[zCall].msgToDM));
				addToSpeakList (lpSpeak, &lQueueElementId, __LINE__);

				gCall[zCall].pendingOutputRequests++;
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "lpSpeak->synchronous=%d, pendingOutputRequests=%d.",
						   lpSpeak->synchronous,
						   gCall[zCall].pendingOutputRequests);

				__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "", -1);
			}

			if (lpSpeak->synchronous == PLAY_QUEUE_ASYNC ||
				lpSpeak->synchronous == PLAY_QUEUE_SYNC)
			{
				if (gCall[zCall].pFirstSpeak == NULL)
				{
	struct MsgToApp response;

					sprintf (response.message, "\0");
					response.appCallNum = zCall;
					response.opcode = DMOP_SPEAK;
					response.appPassword = lpSpeak->appPassword;
					response.appPid = lpSpeak->appPid;
					response.appRef = lpSpeak->appRef;
					response.returnCode = -1;

					writeGenericResponseToApp (zCall, &response, mod,
											   __LINE__);
				}

				lpSpeak->file[0] = '\0';
			}
		}

		break;
	case DMOP_SPEAKMRCPTTS:
		{
	long            lQueueElementId = -1;
	int             yPayloadType = -1;
	int             yCodecType = -1;
	char            mrcpServer[124] = "";
	int             mrcpRtpPort = -1;
	int             mrcpRtcpPort = -1;
	long            lMsgMTtsSpeakId = -1;
	struct Msg_SpeakMrcpTts *lpSpeak =
		(struct Msg_SpeakMrcpTts *) &(gCall[zCall].msgToDM);

			gCall[zCall].currentOpcode = DMOP_SPEAKMRCPTTS;

			if (gCall[zCall].outputThreadId <= 0)
			{
	struct MsgToApp response;

				response.appCallNum = zCall;
				response.opcode = gCall[zCall].currentOpcode;
				response.appPassword = lpSpeak->appPassword;
				response.appPid = lpSpeak->appPid;
				response.appRef = lpSpeak->appRef;
				response.returnCode = -1;

				sprintf (response.message, "%s", "No media available");

				__DDNDEBUG__ (DEBUG_MODULE_CALL,
							  "Output thread was not running",
							  "Returning -1 <No media available>", -1);

				writeGenericResponseToApp (zCall, &response, mod, __LINE__);

//Send Disconnect to TTS Client.
	ARC_TTS_REQUEST_SINGLE_DM ttsRequest;

				memset ((ARC_TTS_REQUEST_SINGLE_DM *) & ttsRequest, '\0',
						sizeof (ttsRequest));
				ttsRequest.speakMsgToDM.opcode = DMOP_DISCONNECT;
				ttsRequest.speakMsgToDM.appRef = gCall[zCall].msgToDM.appRef;
				sprintf (ttsRequest.port, "%d", zCall);
				sprintf (ttsRequest.pid, "%d", gCall[zCall].appPid);
				sprintf (ttsRequest.resultFifo, "%s", "notUsed");
				sprintf (ttsRequest.string, "%s", "notUsed");

				gCall[zCall].keepSpeakingMrcpTTS = 0;
				(void) sendMsgToTTSClient (zCall, &ttsRequest);
				gCall[zCall].ttsRequestReceived = 0;

				return (0);
			}
			gCall[zCall].ttsRequestReceived = 1;

			if ((lpSpeak->synchronous == PUT_QUEUE) ||
				(lpSpeak->synchronous == PUT_QUEUE_ASYNC))
			{
				sscanf (lpSpeak->resource,
						"%[^|]|%d|%d|%d|%d|%ld",
						mrcpServer,
						&mrcpRtpPort,
						&mrcpRtcpPort,
						&yPayloadType, &yCodecType, &lMsgMTtsSpeakId);

				lQueueElementId = lMsgMTtsSpeakId;
//              dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
//                  "DJB: Before addToSpeakList.  lpSpeak->resource=(%s)",
//                  lpSpeak->resource);

				addToSpeakList (lpSpeak, &lQueueElementId, __LINE__);
			}

			if (lpSpeak->synchronous == PLAY_QUEUE_SYNC ||
				lpSpeak->synchronous == PLAY_QUEUE_ASYNC ||
				lpSpeak->synchronous == SYNC || lpSpeak->synchronous == ASYNC)
			{
				sscanf (lpSpeak->resource,
						"%[^|]|%d|%d|%d|%d|%ld",
						mrcpServer,
						&mrcpRtpPort,
						&mrcpRtcpPort,
						&yPayloadType, &yCodecType, &lMsgMTtsSpeakId);

				lQueueElementId = lMsgMTtsSpeakId;

				addToAppRequestList (&(gCall[zCall].msgToDM));
				addToSpeakList (lpSpeak, &lQueueElementId, __LINE__);

				gCall[zCall].pendingOutputRequests++;
			}

			if ((lpSpeak->synchronous == ASYNC_QUEUE) ||
				(lpSpeak->synchronous == ASYNC) ||
				lpSpeak->synchronous == PUT_QUEUE ||
				lpSpeak->synchronous == PUT_QUEUE_ASYNC ||
				lpSpeak->synchronous == PLAY_QUEUE_ASYNC)
			{
	struct MsgToApp response;

				memcpy (&response, lpSpeak, sizeof (struct MsgToApp));

				response.opcode = lpSpeak->opcode;
				response.appCallNum = lpSpeak->appCallNum;
				response.appPid = lpSpeak->appPid;
				response.appRef = lpSpeak->appRef;
				response.appPassword = lpSpeak->appPassword;
				response.returnCode = 0;
				strcpy (response.message, "TEL_Speak worked");

				if (lpSpeak->synchronous == PUT_QUEUE_ASYNC)
				{
					sprintf (response.message, "%d^0", lpSpeak->appRef);
					yRc =
						writeGenericResponseToApp (lpSpeak->appCallNum,
												   &response, mod, __LINE__);
				}
				else
				{
					yRc =
						writeGenericResponseToApp (lpSpeak->appCallNum,
												   &response, mod, __LINE__);
				}

			}
		}
		break;
#ifdef SR_MRCP

	case DMOP_SPEECHDETECTED:
		return process_DMOP_SPEECHDETECTED (zCall);
		break;

	case DMOP_SRRECOGNIZE:
		if (!canContinue (mod, zCall, __LINE__))
		{
			break;
		}
		return process_DMOP_SRRECOGNIZE (zCall);
		break;

	case DMOP_SRRECOGNIZEFROMCLIENT:
		if (!canContinue (mod, zCall, __LINE__))
		{
			break;
		}
		return process_DMOP_SRRECOGNIZEFROMCLIENT (zCall);
		break;
#endif

	case DMOP_CANTFIREAPP:
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);
		return process_DMOP_CANTFIREAPP (zCall);
		break;

	case DMOP_PORTOPERATION:
		return process_DMOP_PORTOPERATION (zCall);
		break;

	case STOP_ACTIVITY:
		if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
			|| gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
			|| gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
		{
	struct MsgToApp response;

			sprintf (response.message, "\0");
	struct Msg_AnswerCall msg;

			msg = *(struct Msg_AnswerCall *) &(gCall[zCall].msgToDM);

			zCall = msg.appCallNum;

			gCall[zCall].callState = CALL_STATE_IDLE;

			response.returnCode = -3;
			response.opcode = DMOP_DISCONNECT;
			response.appCallNum = msg.appCallNum;
			response.appPid = msg.appPid;
			response.appRef = msg.appRef;
			response.appPassword = msg.appPassword;

			yRc = writeGenericResponseToApp (zCall, &response, mod, __LINE__);

			arc_close (zCall, mod, __LINE__, &(gCall[zCall].responseFifoFd));
			gCall[zCall].responseFifoFd = -1;
			//arc_unlink(zCall, mod, gCall[zCall].responseFifo);
			gCall[zCall].responseFifo[0] = 0;
			gCall[zCall].cid = -1;
			gCall[zCall].did = -1;

		}
		gCall[zCall].lastStopActivityAppRef = gCall[zCall].msgToDM.appRef;

//Comment the input request for glenayre streaming 
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting pendingInputRequests to 1.");

		gCall[zCall].pendingInputRequests = 1;

		clearBkpSpeakList (zCall);
		clearSpeakList (zCall, __LINE__);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling clearPlayList.");
		// clearPlayList (zCall);

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling clearAppRequestList", "",
					  -1);

		clearAppRequestList (zCall);

		gCall[zCall].pendingOutputRequests = 1;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "pendingOutputRequests=%d",
				   gCall[zCall].pendingOutputRequests);
		break;

	case DMOP_DROPCALL:
		time (&gCall[zCall].yDropTime);
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);

		yRc = process_DMOP_DROPCALL (zCall);
		break;
	case DMOP_REJECTCALL:
		time (&gCall[zCall].yDropTime);
		setCallState (zCall, CALL_STATE_CALL_RELEASED, __LINE__);

		yRc = process_DMOP_REJECTCALL (zCall);
		break;

	case DMOP_APPDIED:
		if (gCall[zCall].callState != CALL_STATE_CALL_CLOSED &&
			gCall[zCall].callState != CALL_STATE_CALL_CANCELLED &&
			gCall[zCall].callState != CALL_STATE_CALL_RELEASED)
		{
			time (&gCall[zCall].yDropTime);
			yRc = process_DMOP_APPDIED (zCall);
		}
		break;

	case DMOP_B_LEG_DISCONNECTED:
		if (gCall[zCall].crossPort > -1)
		{
	int             yBLeg = gCall[zCall].crossPort;

			time (&gCall[yBLeg].yDropTime);
			setCallState (yBLeg, CALL_STATE_CALL_RELEASED, __LINE__);
		}
		break;

	case DMOP_TTS_COMPLETE_TIMEOUT:
	case DMOP_TTS_COMPLETE_FAILURE:
		gCall[zCall].keepSpeakingMrcpTTS = 0;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Received %d: "
				   "Set gCall[%d].keepSpeakingMrcpTTS to %d.",
				   gCall[zCall].msgToDM.opcode, zCall,
				   gCall[zCall].keepSpeakingMrcpTTS);
		break;
	case DMOP_TTS_COMPLETE_SUCCESS:
		gCall[zCall].finishTTSSpeakAndExit = 1;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Received %d: "
				   "Set gCall[%d].finishTTSSpeakAndExit to %d.",
				   gCall[zCall].msgToDM.opcode, zCall,
				   gCall[zCall].finishTTSSpeakAndExit);
		break;
// MR 4655
		case DMOP_CALL_INITIATED:
		struct Msg_InitiateCall msg;

		msg = *(struct Msg_InitiateCall *) &(gCall[zCall].msgToDM);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Received DMOP_CALL_INITIATED(%d:%d)",
					msg.opcode, msg.informat);

		if ( msg.informat == 1 )
		{
			gCall[zCall].prevCallState = gCall[zCall].callState;
			setCallState (msg.appCallNum, CALL_STATE_CALL_INITIATE_ISSUED, __LINE__);
		}
		else
		{
			setCallState (msg.appCallNum, gCall[zCall].prevCallState, __LINE__);
			gCall[zCall].prevCallState = CALL_STATE_IDLE;
		}

		break;
// END: MR 4655

#ifdef VOICE_BIOMETRICS
    case DMOP_VERIFY_CONTINUOUS_SETUP:
        return process_DMOP_VERIFY_CONTINUOUS_SETUP(zCall);
        break;
    case DMOP_VERIFY_CONTINUOUS_GET_RESULTS:
        return process_DMOP_VERIFY_CONTINUOUS_GET_RESULTS (zCall);
        break;
#endif // VOICE_BIOMETRICS


	default:
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_OPCODE,
				   ERR,
				   "Failed to find request, Unknown request opcode (%d) received from pid=%d.",
				   gCall[zCall].msgToDM.opcode, gCall[zCall].msgToDM.appPid);
		break;

	}							/* switch */

	return (0);

}								/*END: processAppRequests */

///The following function returns us a string representation for the opcode we receive on the request fifo.
int
getOpcodeString (int zOpcode, char *zOut)
{
	if (!zOut)
	{
		return 0;
	}

	switch (zOpcode)
	{
	case DMOP_SETDTMF:
		sprintf (zOut, "%s", "DMOP_SETDTMF");
		break;

	case DMOP_BRIDGECONNECT:
		sprintf (zOut, "%s", "DMOP_BRIDGECONNECT");
		break;

	case DMOP_BRIDGE_THIRD_LEG:
		sprintf (zOut, "%s", "DMOP_BRIDGE_THIRD_PARTY");
		break;

	case DMOP_INITIATECALL:
		sprintf (zOut, "%s", "DMOP_INITIATECALL");
		break;

	case DMOP_LISTENCALL:
		sprintf (zOut, "%s", "DMOP_LISTENCALL");
		break;

	case DMOP_SETGLOBAL:
		sprintf (zOut, "%s", "DMOP_SETGLOBAL");
		break;

	case DMOP_GETGLOBAL:
		sprintf (zOut, "%s", "DMOP_GETGLOBAL");
		break;

	case DMOP_SETGLOBALSTRING:
		sprintf (zOut, "%s", "DMOP_SETGLOBALSTRING");
		break;

	case DMOP_GETGLOBALSTRING:
		sprintf (zOut, "%s", "DMOP_GETGLOBALSTRING");
		break;

	case DMOP_DISCONNECT:
		sprintf (zOut, "%s", "DMOP_DISCONNECT");
		break;

	case DMOP_RTPDETAILS:
		sprintf (zOut, "%s", "DMOP_RTPDETAILS");
		break;

	case DMOP_MEDIACONNECT:
		sprintf (zOut, "%s", "DMOP_MEDIACONNECT");
		break;

	case DMOP_VIDEODETAILS:
		sprintf (zOut, "%s", "DMOP_VIDEODETAILS");
		break;

	case DMOP_EXITALL:
		sprintf (zOut, "%s", "DMOP_EXITALL");
		break;

	case DMOP_ANSWERCALL:
		sprintf (zOut, "%s", "DMOP_ANSWERCALL");
		break;

	case DMOP_INITTELECOM:
		sprintf (zOut, "%s", "DMOP_INITTELECOM");
		break;

	case DMOP_OUTPUTDTMF:
		sprintf (zOut, "%s", "DMOP_OUTPUTDTMF");
		break;

	case DMOP_RECORD:
		sprintf (zOut, "%s", "DMOP_RECORD");
		break;

	case DMOP_RECVFAX:
		sprintf (zOut, "%s", "DMOP_RECVFAX");
		break;

	case DMOP_SPEAK:
		sprintf (zOut, "%s", "DMOP_SPEAK");
		break;

	case DMOP_SENDFAX:
		sprintf (zOut, "%s", "DMOP_SENDFAX");
		break;

	case DMOP_SRRECOGNIZE:
		sprintf (zOut, "%s", "DMOP_SRRECOGNIZE");
		break;

	case DMOP_SRRECOGNIZEFROMCLIENT:
		sprintf (zOut, "%s", "DMOP_SRRECOGNIZEFROMCLIENT");
		break;

	case DMOP_SPEECHDETECTED:
		sprintf (zOut, "%s", "DMOP_SPEECHDETECTED");
		break;

	case DMOP_CANTFIREAPP:
		sprintf (zOut, "%s", "DMOP_CANTFIREAPP");
		break;

	case DMOP_PORTOPERATION:
		sprintf (zOut, "%s", "DMOP_PORTOPERATION");
		break;

	case STOP_ACTIVITY:
		sprintf (zOut, "%s", "STOP_ACTIVITY");
		break;

	case DMOP_DROPCALL:
		sprintf (zOut, "%s", "DMOP_DROPCALL");
		break;

	case DMOP_REJECTCALL:
		sprintf (zOut, "%s", "DMOP_REJECTCALL");
		break;

	case DMOP_APPDIED:
		sprintf (zOut, "%s", "DMOP_APPDIED");
		break;

	case DMOP_PLAYMEDIA:
		sprintf (zOut, "%s", "DMOP_PLAYMEDIA");
		break;

	case DMOP_RECORDMEDIA:
		sprintf (zOut, "%s", "DMOP_RECORDMEDIA");
		break;

	case DMOP_PLAYMEDIAVIDEO:
		sprintf (zOut, "%s", "DMOP_PLAYMEDIAVIDEO");
		break;

	case DMOP_PLAYMEDIAAUDIO:
		sprintf (zOut, "%s", "DMOP_PLAYMEDIAAUDIO");
		break;

	case DMOP_SPEAKMRCPTTS:
		sprintf (zOut, "%s", "DMOP_SPEAKMRCPTTS");
		break;

	case DMOP_TTS_COMPLETE_SUCCESS:
		sprintf (zOut, "%s", "DMOP_TTS_COMPLETE_SUCCESS");
		break;

	case DMOP_TTS_COMPLETE_FAILURE:
		sprintf (zOut, "%s", "DMOP_TTS_COMPLETE_FAILURE");
		break;

	case DMOP_TTS_COMPLETE_TIMEOUT:
		sprintf (zOut, "%s", "DMOP_TTS_COMPLETE_TIMEOUT");
		break;

	case DMOP_END_CALL_PROGRESS:
		sprintf (zOut, "%s", "DMOP_END_CALL_PROGRESS");
		break;

	case DMOP_START_CALL_PROGRESS_RESPONSE:
		sprintf (zOut, "%s", "DMOP_START_CALL_PROGRESS_RESPONSE");
		break;

	case DMOP_CONFERENCE_START:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_START");
		break;

	case DMOP_INSERTDTMF:
		sprintf (zOut, "%s", "DMOP_INSERTDTMF");
		break;

	case DMOP_CONFERENCE_ADD_USER:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_ADD_USER");
		break;

	case DMOP_CONFERENCE_REMOVE_USER:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_REMOVE_USER");
		break;

	case DMOP_CONFERENCE_STOP:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_STOP");
		break;

	case DMOP_CONFERENCE_START_RESPONSE:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_START_RESPONSE");
		break;

	case DMOP_CONFERENCE_ADD_USER_RESPONSE:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_ADD_USER_RESPONSE");
		break;

	case DMOP_CONFERENCE_REMOVE_USER_RESPONSE:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_REMOVE_USER_RESPONSE");
		break;

	case DMOP_CONFERENCE_STOP_RESPONSE:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_STOP_RESPONSE");
		break;

	case DMOP_FAX_PROCEED:
		sprintf (zOut, "%s", "DMOP_FAX_PROCEED");
		break;

	case DMOP_FAX_COMPLETE:
		sprintf (zOut, "%s", "DMOP_FAX_COMPLETE");
		break;

	case DMOP_FAX_SEND_SDPINFO:
		sprintf (zOut, "%s", "DMOP_FAX_SEND_SDPINFO");
		break;

	case DMOP_FAX_PLAYTONE:
		sprintf (zOut, "%s", "DMOP_FAX_PLAYTONE");
		break;

	case DMOP_FAX_STOPTONE:
		sprintf (zOut, "%s", "DMOP_FAX_STOPTONE");
		break;

	case DMOP_FAX_RESERVE_RESOURCE:
		sprintf (zOut, "%s", "DMOP_FAX_RESERVE_RESOURCE");
		break;
	case DMOP_FAX_RELEASE_RESOURCE:
		sprintf (zOut, "%s", "DMOP_FAX_RELEASE_RESOURCE");
		break;

//#ifdef CALEA
	case DMOP_STARTRECORDSILENTLY:
		sprintf (zOut, "%s", "DMOP_STARTRECORDSILENTLY");
		break;

	case DMOP_STOPRECORDSILENTLY:
		sprintf (zOut, "%s", "DMOP_STOPRECORDSILENTLY");
		break;

//#endif

#ifdef VOICE_BIOMETRICS
    case DMOP_VERIFY_CONTINUOUS_SETUP:
        sprintf (zOut, "%s", "DMOP_VERIFY_CONTINUOUS_SETUP");
        break;

    case DMOP_VERIFY_CONTINUOUS_GET_RESULTS:
        sprintf (zOut, "%s", "DMOP_VERIFY_CONTINUOUS_GET_RESULTS");
        break;
#endif // END: VOICE_BIOMETRICS

// MR 4655
	case DMOP_CALL_INITIATED:
		sprintf (zOut, "%s", "DMOP_CALL_INITIATED");
		break;
// END: MR 4655

	case DMOP_B_LEG_DISCONNECTED:
		sprintf (zOut, "%s", "DMOP_B_LEG_DISCONNECTED");
		break;


	case DMOP_RESTART_OUTPUT_THREAD:
		sprintf (zOut, "%s", "DMOP_RESTART_OUTPUT_THREAD");
		break;

	default:
		sprintf (zOut, "%s", "UNKNOWN");
	}

	return (0);

}								/*int getOpcodeString */



///This function reads the shared memory btwn Call Manager and Media Manager and then processes the DMOP commands received.
void           *
readAndProcessShmData (void *z)
{
	int             yRc = -1;
	char            mod[] = { "readAndProcessShmData" };
	int             zCall = -1;
	long            yTmpReadCount;
	struct MsgToDM  yMsgToDM;
	struct MsgToDM  request;
	char            yStrOpcode[64];
	char            yStrTmp[256];
	char            yErrMsg[256];
	char            yStrParentDirectory[256];

	int             yIntParentDirCounter = 0;

	int             yRfc2833Payload = 96;
	int             i = 0;

	time_t          yLastHeartBeat;

	static int      tid = syscall (224);

	time (&yLastHeartBeat);

	sprintf (gIpCfg, "%s/Telecom/Tables/.TEL.cfg", gIspBase);

	sprintf (yStrParentDirectory, "/proc/%d", gParentPid);

	/*RFC 2833 Payload type */
	sprintf (yStrTmp, "%s", "\0");

	yRc =
		get_name_value ("", "SIP_RFC2833Payload", "96", yStrTmp, 5, gIpCfg,
						yErrMsg);
	if (yStrTmp[0])
	{
		yRfc2833Payload = atoi (yStrTmp);
	}

	__DDNDEBUG__ (DEBUG_MODULE_FILE, "SIP_RFC2833Payload", yStrTmp, -1);

	/*END: RFC 2833 Payload type */

	/*Initialize the default profile */
	rtp_profile_set_payload (&av_profile, yRfc2833Payload, &telephone_event);
	rtp_profile_set_payload (&av_profile, 0, &pcmu8000);

	for (i = gStartPort; i <= gEndPort; i++)
	{
		rtp_profile_set_payload (&av_profile_array_in[i], yRfc2833Payload,
								 &telephone_event);
		rtp_profile_set_payload (&av_profile_array_in[i], 0, &pcmu8000);
		rtp_profile_set_payload (&av_profile_array_in[i], 8, &pcma8000);

		rtp_profile_set_payload (&av_profile_array_out[i], yRfc2833Payload,
								 &telephone_event);
		rtp_profile_set_payload (&av_profile_array_out[i], 0, &pcmu8000);
		rtp_profile_set_payload (&av_profile_array_out[i], 8, &pcma8000);

		//rtp_profile_set_payload(&av_mrcp_profile_array_sr[i] ,  yRfc2833Payload,&telephone_event);
		//rtp_profile_set_payload(&av_mrcp_profile_array_sr[i],   0,              &pcmu8000);
	}
	/*END: Initialize the default profile */

	// begin - MR 5071
	time (&yTmpApproxTime);

	yLastHeartBeat = yTmpApproxTime;

	request.opcode = DMOP_HEARTBEAT;
	sendRequestToDynMgr (__LINE__, mod, &request);
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Sending initial HEARTBEAT to the call manager.");

	while (gCanReadShmData)
	{

		yIntParentDirCounter++;

		/*Exit if Parent process (ArcIPDynMgr) is not around */

		if (yIntParentDirCounter % 10 == 0)
		{
			yIntParentDirCounter = 0;

			if (access (yStrParentDirectory, F_OK) != 0)
			{
				dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PID_ERROR,
						   ERR, "Failed to find parent pid (%d). Exiting",
						   gParentPid);

				gCanExit = 1;
				gCanReadShmData = 0;

				break;
			}
		}

		/*DDN: DO NOT CHANGE OR MOVE yTmpApproxTime.  THIS IS A GLOBAL VARIABLE AND IS BEING USED BY OTHER THREADS */
		time (&yTmpApproxTime);

		yTmpReadCount = gEncodedShmBase->msgCounter[gLastShmUsed].count;

//      dynVarLog(__LINE__, -1, mod, REPORT_DETAIL, TEL_INVALID_PORT, INFO, 
//              "DJB: yTmpApproxTime(%d) - yLastHeartBeat(%d) = %d",
//              yTmpApproxTime, yLastHeartBeat, yTmpApproxTime - yLastHeartBeat);
		if (yTmpApproxTime - yLastHeartBeat >= 3)
		{

			yLastHeartBeat = yTmpApproxTime;

			request.opcode = DMOP_HEARTBEAT;

			sendRequestToDynMgr (__LINE__, mod, &request);
		}

		if (yTmpReadCount <= gShmReadCount)
		{
			util_u_sleep (20 * 1000);
			continue;
		}

		__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "Last ShmUsed", "", gLastShmUsed);

		memset (&yMsgToDM, 0, sizeof (struct MsgToDM));

		memcpy (&yMsgToDM,
				&(gEncodedShmBase->msgCounter[gLastShmUsed].msgToDM),
				sizeof (struct MsgToDM));

		zCall = yMsgToDM.appCallNum;

		if (zCall < 0 || zCall > MAX_PORTS)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_INVALID_PORT,
					   INFO,
					   "Failed to process app request, invalid port(%d).",
					   zCall);

			gLastShmUsed++;

			if (gLastShmUsed >= (MAX_PORTS * 4))
			{
				gLastShmUsed = 0;
			}

			gShmReadCount = yTmpReadCount;

			util_u_sleep (100);
			continue;
		}

		if (yMsgToDM.opcode == DMOP_PLAYMEDIA ||
			yMsgToDM.opcode == DMOP_PLAYMEDIAAUDIO ||
			yMsgToDM.opcode == DMOP_PLAYMEDIAVIDEO ||
			yMsgToDM.opcode == DMOP_SPEAK)
		{
	size_t          sizeOfSpeak;
	struct Msg_Speak yTmpMsgSpeak;

			memcpy (&yTmpMsgSpeak,
					(struct Msg_Speak *) &yMsgToDM,
					sizeof (struct Msg_Speak));

			if (yTmpMsgSpeak.synchronous == SYNC &&
				yMsgToDM.appRef < gCall[zCall].msgToDM.appRef)
			{
               
		        getOpcodeString (yMsgToDM.opcode, yStrOpcode);
				dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
						   INFO,
						   "Processing op:%d(%s),call:%d,ref:%d,pid:%d:Current last appRef=%d, ignoring request",
						   yMsgToDM.opcode, yStrOpcode, yMsgToDM.appCallNum,
						   yMsgToDM.appRef, yMsgToDM.appPid,
						   gCall[zCall].msgToDM.appRef);

				gLastShmUsed++;

				if (gLastShmUsed >= (MAX_PORTS * 4))
				{
					gLastShmUsed = 0;
				}

				gShmReadCount = yTmpReadCount;

				util_u_sleep (100);
				// Wed Mar 25 07:52:47 EDT 2015 JOE S 
				JSPRINTF(stderr, " before memcpy\n");
		        memcpy (&gCall[zCall].msgToDM, &(yMsgToDM), sizeof (struct MsgToDM));
			    processAppRequest (zCall);
				JSPRINTF(stderr, " after process\n");
				continue;
			}

		}

		memcpy (&gCall[zCall].msgToDM, &(yMsgToDM), sizeof (struct MsgToDM));

		sprintf (yStrOpcode, "%s", "UNKNOWN");

		getOpcodeString (yMsgToDM.opcode, yStrOpcode);

		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
				   "Processing op:%d(%s),call:%d,ref:%d,pid:%d",
				   yMsgToDM.opcode,
				   yStrOpcode,
				   yMsgToDM.appCallNum, yMsgToDM.appRef, yMsgToDM.appPid);

		if (yMsgToDM.opcode == DMOP_EXITALL)
		{
			gCanReadShmData = 0;
			gCanExit = 1;
			break;
		}

		if (yMsgToDM.opcode > 0)
		{
			yRc = processAppRequest (zCall);
			if (yRc != 0)
			{
				;				//??
			}
		}

		gLastShmUsed++;

		if (gLastShmUsed >= (MAX_PORTS * 4))
		{
			gLastShmUsed = 0;
		}

		gShmReadCount = yTmpReadCount;

		util_u_sleep (100);

	}							/*END: while(gCanReadShmData) */

	__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "Exiting thread readAndProcessShmData",
				  "", gLastShmUsed);

	/*BGN: Detach the shared memory */
	shmdt (gShmBase);
	/*END: Detach the shared memory */

	/*Exit */
	pthread_detach (pthread_self ());
	pthread_exit (NULL);

	return (0);

}								/*END: void * readAndProcessShmData */


///This function is used to bind Media Manager to the last processor on a multi-cpu system.
/**
During testing we found that audio quality was breaking up and this was due
to the fact that Media Manager was sleeping incorrectly after sending an rtp
packet.  The reason for this was that Media Manager was shfting from cpu to cpu
and was sleeping for longer than the time it was supposed to.  In order to fix
this we bind Media Manager to the last cpu on the system using the this
function.
*/
int
bindMediaMgr ()
{
int             yRc = -1;
char            mod[] = "bindMediaMgr";
int             zCall = -1;

cpu_set_t       mask;
int             yIntCpuUsed = -1;
struct utsname  systemInfo;
int             yIntTotalCpus = 0;

	/*Find version information for this OS */
	yRc = uname (&systemInfo);
	if (yRc != 0)
	{
		return -1;
	}
	else
	{
	}
	/*END:Find version information for this OS */

	/*Set CPU Affinity */
	if (strstr (systemInfo.release, "2.4.21") != NULL)
	{
		//if(yRc = sched_getaffinity(getpid(), &mask) < 0)
		if (yRc = sched_getaffinity (getpid (), sizeof (mask), &mask) < 0)
		{
			return -1;
		}
		else
		{
		}

		for (int s = 0; s < 30; s++)
		{
			if (!CPU_ISSET (s, &mask))
			{
				yIntCpuUsed = --s;
				yIntTotalCpus = yIntCpuUsed + 1;
				break;
			}
		}

		CPU_ZERO (&mask);

		CPU_SET ((gDynMgrId + 1) % yIntTotalCpus, &mask);

#if 0
		/*Media Mgrs with start port < 144 should be attached to the last CPU */
		if (gStartPort < 144)
		{
			CPU_SET (yIntCpuUsed, &mask);
		}

		/*Media Mgrs with start port >= 144 should be attached to the second last CPU */
		if (gStartPort >= 144)
		{
			if (yIntCpuUsed >= 1)
			{
				CPU_SET (yIntCpuUsed - 1, &mask);
			}
			else				/*If there is only one CPU */
			{
				CPU_SET (yIntCpuUsed, &mask);
			}
		}
#endif

		//if(yRc = sched_setaffinity(getpid(), &mask) < 0)
		if (yRc = sched_setaffinity (getpid (), sizeof (mask), &mask) < 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
					   TEL_AFFINITY_ERROR, ERR,
					   "Failed to set Media Manager(%d) affinity to CPU (%d).",
					   gDynMgrId, (gDynMgrId + 1) % yIntTotalCpus);
			return -1;
		}

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Set Media Manager(%d) affinity to CPU (%d).",
				   gDynMgrId, (gDynMgrId + 1) % yIntTotalCpus);

	}
	/*END:Set CPU Affinity */

	return 0;

}								/*END: int bindMediaMgr() */

#ifdef  ACU_LINUX

int
openChannelToTonesClient ()
{
char            mod[] = { "openChannelToTonesClient" };
int             zCall = -1;

	gSTonesFifoFd = -1;

	//sprintf (gRequestFifo, "%s/%s", gFifoDir, REQUEST_FIFO);
	sprintf (gSTonesFifo, "%s/%s", gFifoDir, STONES_CP_FIFO);

	if ((gSTonesFifoFd =
		 arc_open (zCall, mod, __LINE__, gSTonesFifo, O_WRONLY | O_NONBLOCK,
				   ARC_TYPE_FIFO)) < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to open stones fifo (%s). [%d, %s] Unable to communicate with aculab client.",
				   gSTonesFifo, errno, strerror (errno));

		return (-1);
	}

	return (0);

}								/*END: int openChannelToTonesClient */

int
openChannelToTonesFaxClient (int zCall)
{
	char            mod[] = { "openChannelToTonesClient" };

	gSTonesFifoFd = -1;

	sprintf (gSTonesFaxFifo, "%s/%s", gFifoDir, STONES_FAX_FIFO);

	if ((gSTonesFaxFifoFd =
		 arc_open (zCall, mod, __LINE__, gSTonesFaxFifo,
				   O_WRONLY | O_NONBLOCK, ARC_TYPE_FIFO)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to open stones fifo (%s). [%d, %s] Unable to communicate with aculab client.",
				   gSTonesFaxFifo, errno, strerror (errno));

		return (-1);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Successfully opened stones fifo (%s) gSTonesFaxFifoFd=%d.",
			   gSTonesFaxFifo, gSTonesFaxFifoFd);

	return (0);

}								/*END: int openChannelToTonesFaxClient */

int
openChannelToConfMgr ()
{
char            mod[] = { "openChannelToConfMgr" };
int             zCall = -1;

	gConfMgrFifoFd = -1;

	sprintf (gConfMgrFifo, "%s/%s", gFifoDir, CONF_MGR_FIFO);

	if ((gConfMgrFifoFd =
		 arc_open (zCall, mod, __LINE__, gConfMgrFifo, O_WRONLY | O_NONBLOCK,
				   ARC_TYPE_FIFO)) < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to open conf fifo (%s). [%d, %s] Unable to communicate with conference manager.",
				   gConfMgrFifo, errno, strerror (errno));

		return (-1);
	}

	return (0);

}								/*END: int openChannelToConfMgr */

#endif

int
openChannelToDynMgr ()
{
char            mod[] = { "openChannelToDynMgr" };
int             zCall = -1;

	//sprintf (gRequestFifo, "%s/%s", gFifoDir, REQUEST_FIFO);
	sprintf (gRequestFifo, "%s/%s.%d", gFifoDir, REQUEST_FIFO, gDynMgrId);

	if ((gRequestFifoFd =
		 arc_open (zCall, mod, __LINE__, gRequestFifo, O_WRONLY,
				   ARC_TYPE_FIFO)) < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to open request fifo (%s). [%d, %s] Unable to communicate with application(s).",
				   gRequestFifo, errno, strerror (errno));

		return (-1);
	}

	return (0);

}								/*END: int openChannelToDynMgr */

int
setRtpPort (int zCall)
{
	char            mod[] = "setRtpPort";
	int             isSdpPortSet = false;
	static int      currentlocalSdpPort =
		LOCAL_STARTING_RTP_PORT + (zCall * 2);
	int             yRc;

	gCall[zCall].localSdpPort = currentlocalSdpPort;

	if (gCall[zCall].rtp_session_in == NULL)
	{
		gCall[zCall].rtp_session_in = rtp_session_new (RTP_SESSION_RECVONLY);

		if (gCall[zCall].rtp_session_in == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR,
					   ERR,
					   "rtp_session_new() failed.  Unable to create rtp_session.");

			if (gCall[zCall].rtp_session != NULL)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Calling arc_rtp_session_destroy(); rtp_session socket=%d.",
						   gCall[zCall].rtp_session->rtp.socket);

				arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session);
			}

			return -1;
		}

		while (1)
		{

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting local audio port address to %d.",
					   gCall[zCall].localSdpPort);

			yRc = rtp_session_set_local_addr (gCall[zCall].rtp_session_in,
											  gHostIp,
											  gCall[zCall].localSdpPort,
											  gHostIf);

			if (yRc == -1)
			{
				gCall[zCall].localSdpPort += 2;
			}
			else
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Successfully set audio port address to %d.",
						   gCall[zCall].localSdpPort);
				break;
			}
		}

		rtp_session_signal_connect (gCall[zCall].rtp_session_in,
									"telephone-event_packet",
									(RtpCallback) recv_tevp_cb,
									&(gCall[zCall].callNum));

#if 1
		/*Callback for timestamp jump */
		rtp_session_signal_connect (gCall[zCall].rtp_session_in,
									"timestamp_jump",
									(RtpCallback) recv_tsjump_cb,
									&(gCall[zCall].callNum));

		__DDNDEBUG__ (DEBUG_MODULE_RTP,
				  "Calling rtp_session_reset", "rtp_session(1)", zCall);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Calling rtp_session_reset(1).");
		 rtp_session_signal_connect(gCall[zCall].rtp_session_in,		// MR 5038
						"ssrc_changed",
						(RtpCallback)rtp_session_reset,
						NULL);
#endif

	}

	currentlocalSdpPort = gCall[zCall].localSdpPort + 2;

}

int
setTtsRtpPort (int zCall, FILE *fp)
{
	char            mod[] = "setTtsRtpPort";
	static int      currentlocalSdpPort = LOCAL_STARTING_TTS_RTP_PORT + (zCall * 2);
	int             yRc;

	gCall[zCall].localTtsRtpPort = currentlocalSdpPort;

	if (gCall[zCall].rtp_session_mrcp_tts_recv == NULL)
	{
		gCall[zCall].rtp_session_mrcp_tts_recv = rtp_session_new (RTP_SESSION_RECVONLY);

		if (gCall[zCall].rtp_session_mrcp_tts_recv == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR, ERR,
					   "rtp_session_new() failed.  Unable to create rtp_session.");

			if (gCall[zCall].rtp_session_mrcp_tts_recv != NULL)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						   "Calling arc_rtp_session_destroy(); rtp_session socket=%d.",
						   gCall[zCall].rtp_session_mrcp_tts_recv->rtp.socket);

				arc_rtp_session_destroy (zCall, &gCall[zCall].rtp_session_mrcp_tts_recv);
			}

			return -1;
		}

		while (1)
		{

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting local tts audio port address to %d.",
					   gCall[zCall].localTtsRtpPort);

			yRc = rtp_session_set_local_addr (gCall[zCall].rtp_session_mrcp_tts_recv, gHostIp,
											  gCall[zCall].localTtsRtpPort, gHostIf);

			if (yRc == -1)
			{
				gCall[zCall].localTtsRtpPort += 2;
			}
			else
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Successfully set tts audio port address to %d.",
						   gCall[zCall].localTtsRtpPort);
				fprintf(fp, "%d|%d\n", zCall, gCall[zCall].localTtsRtpPort);
				break;
			}
		}

		rtp_session_signal_connect (gCall[zCall].rtp_session_mrcp_tts_recv,
									"telephone-event_packet",
									(RtpCallback) recv_tevp_cb,
									&(gCall[zCall].callNum));

#if 1
		/*Callback for timestamp jump */
		rtp_session_signal_connect (gCall[zCall].rtp_session_mrcp_tts_recv,
									"timestamp_jump",
									(RtpCallback) recv_tsjump_cb,
									&(gCall[zCall].callNum));

#endif

	}

	currentlocalSdpPort = gCall[zCall].localTtsRtpPort + 2;

} // END: setTtsRtpPort()

void
sendMediaDetailsToDM ()
{
char            mod[] = "sendMediaDetailsToDM";
FILE           *rtpFile = NULL;
char            path[128] = "";
char            buf[128] = "";
struct MsgToDM  request;

	sprintf (path, "portDetail.%d", gDynMgrId);
	rtpFile = fopen (path, "w+");

	if (rtpFile != NULL)
	{
		for (int i = gStartPort; i <= gEndPort; i++)
		{
					 
			sprintf (buf, "%d_%d\n", i, gCall[i].localSdpPort);
			fwrite (buf, (int) strlen (buf), 1, rtpFile);
		}

		fclose (rtpFile);
	}

	request.appCallNum = gStartPort;
	request.opcode = DMOP_RTPPORTDETAIL;
	request.appPid = 0;
	request.appPassword = 0;

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Sending DMOP_RTPPORTDETAIL to Call Manager.");

	sendRequestToDynMgr (__LINE__, mod, &request);

}								/*END: void sendMediaDetailsToDM */

#ifdef ACU_RECORD_TRAILSILENCE

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static void
sTonesLog (char *zFile, int zLine, int zCallNum, char *zpMod,
		   int mode, int msgId, int msgType, char *msgFormat, ...)
{
	va_list         ap;
	char            m[2048];
	char            m1[2048];
	char            type[32];
	int             rc;
	char            lPortStr[10];

	switch (msgType)
	{
	case 0:
		sprintf (type, "%s", "ERR: ");
		break;
	case 1:
		sprintf (type, "%s", "WRN: ");
		break;
	case 2:
		sprintf (type, "%s", "INF: ");
		break;
	default:
		sprintf (type, "%s", "");
		break;
	}

	memset ((char *) m1, '\0', sizeof (m1));
	va_start (ap, msgFormat);
	vsprintf (m1, msgFormat, ap);
	va_end (ap);

	sprintf (m, "%s[%s:%d:pid-%d] %s", type, zFile, zLine, getpid (), m1);

	sprintf (lPortStr, "%d", zCallNum);

	LogARCMsg (zpMod, mode, lPortStr, "SRC", gProgramName, msgId, m);

}								// END: sTonesLog()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int
tingLogPrint (const char *format, va_list arglist)
{
	static char     mod[] = "tingLogPrint";
	static int      EnterOnce = 0;
	static FILE    *fp;
	char           *pIspBase;
	char            logFile[256];
	char            time_str[80];
	struct tm      *tm;
	struct timeval  tp;
	struct timezone tzp;
	char            logMsg[512];

	if (!EnterOnce)
	{
		pIspBase = (char *) getenv ("ISPBASE");
		if (pIspBase == NULL)
		{
//          sTonesLog(__FILE__, __LINE__, DEF_PORT, mod, REPORT_DETAIL,
//              TONES_LOG_BASE, WARN,
//              "ISPBASE environment variable not set. "
//              "Unable to perform TiNG log message.  ");
			return (0);
		}
		sprintf (logFile, "%s/LOG/TiNG_pid%d.log", pIspBase, getpid ());

		if (!EnterOnce)
		{
			fp = fopen (logFile, "w+");
			sTonesLog (__FILE__, __LINE__, DEF_PORT, mod, REPORT_VERBOSE,
					   TONES_LOG_BASE, INFO,
					   "Successfully fopened logFile (%s). ", logFile);
			EnterOnce = 1;
		}
	}

	gettimeofday (&tp, &tzp);
	tm = localtime (&tp.tv_sec);
	sprintf (time_str, "%02d:%02d:%04d %02d:%02d:%02d:%03d",
			 tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900, tm->tm_hour,
			 tm->tm_min, tm->tm_sec, tp.tv_usec / 1000);

	acu_os_lock_critical_section (LOG_CS);
	fprintf (fp, "%s|", time_str);
	fflush (fp);
	vfprintf (fp, format, arglist);
	fflush (fp);
	acu_os_unlock_critical_section (LOG_CS);

	return 0;
}								// END: tingLogPrint()

#endif

int
main (int argc, char *argv[])
{
	int             yRc;
	char            mod[] = "main";

	int             i, j, fd;
	int             zCall = -1;
	char            yStrSR[64] = "";
	char            yErrMsg[256];
	char            ringEventTime[17];
	FILE           *ttsMrcpDetail_fp;

	struct sigaction sig_act, sig_oact;

    arc_dbg_open(argv[0]);

    pthread_mutex_init(&ClearPortLock, NULL);

	nice (-10);
	setuid (getuid ());

	clock_sync_in = arc_clock_sync_init (NULL, 20);

	gParentPid = getppid ();

	srand (getpid ());

	sprintf (gProgramName, "%s", argv[0]);
	/* Determine base pathname. */
	ispbase = (char *) getenv ("ISPBASE");
	if (ispbase == NULL)
	{
		sleep (EXIT_SLEEP);
		return (-1);
	}

// DJB debug
	yRc = mcheck (NULL);
// end DJB debug

	sprintf (gIspBase, "%s", ispbase);
	/*END: Determine base pathname. */

	sprintf (gIspBase, "%s", ispbase);
	sprintf (gGlobalCfg, "%s/Global/.Global.cfg", gIspBase);
	sprintf (gFifoDir, "%s", "\0");

	yRc = get_name_value ("", FIFO_DIR_NAME_IN_NAME_VALUE_PAIR,
						  DEFAULT_FIFO_DIR, gFifoDir, sizeof (gFifoDir),
						  gGlobalCfg, yErrMsg);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DETAIL,
				   TEL_CONFIG_VALUE_NOT_FOUND, WARN,
				   "Failed to find '%s' in (%s). Defaulting to (%s).",
				   FIFO_DIR_NAME_IN_NAME_VALUE_PAIR, gGlobalCfg, gFifoDir);
	}

	sprintf (gHostName, "%s", 0);
	gethostname (gHostName, 255);

	/*Load the config file */
	loadConfigFile (NULL, NULL);

#ifdef VOICE_BIOMETRICS
	readAVBCfg();
#endif  // END: VOICE_BIOMETRICS


	/*END: Load the config file */

	if (!gHostIp[0] || strcmp (gHostIp, "0.0.0.0") == 0)
	{
		yRc = getIpAddress (gHostName, gHostIp, yErrMsg);
		if (yRc < 0)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_NETWORK_ERROR,
					   ERR, yErrMsg);
			sleep (EXIT_SLEEP);
			return (-1);
		}
	}

	gCanExit = 0;
	gCanReadShmData = 1;

	gTotalOutputThreads = 0;
	gTotalInputThreads = 0;

	/* set death of child function */
	sig_act.sa_handler = NULL;
	sig_act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
	if (sigaction (SIGCHLD, &sig_act, &sig_oact) != 0)
	{
		sprintf (yErrMsg,
				 "sigaction(SIGCHLD, SA_NOCLDWAIT): system call failed. [%d, %s]",
				 errno, strerror (errno));

		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEDIAMGR_SHUTDOWN,
				   INFO, "Shutting down: %s", yErrMsg);

		exit (-1);
	}

	if (signal (SIGCHLD, SIG_IGN) == SIG_ERR)
	{
		sprintf (yErrMsg,
				 "signal(SIGCHLD, SIG_IGN): system call failed. [%d, %s]",
				 errno, strerror (errno));

		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEDIAMGR_SHUTDOWN,
				   INFO, "Shutting down: %s", yErrMsg);

		exit (-1);
	}
	/*END: set death of child function */

	pthread_mutex_init (&gMutexDebug, NULL);
	pthread_mutex_init (&gMutexLog, NULL);
	pthread_mutex_init (&gOpenFdLock, NULL);

	/*Fill silence buffer */
	fd = arc_open (zCall, mod, __LINE__, "silence_4096.64p", O_RDONLY,
				   ARC_TYPE_FILE);
	if (fd != -1)
	{
		gIntSilenceBufferSize = read (fd, gSilenceBuffer, 1600);
		arc_close (zCall, mod, __LINE__, &fd);
		fd = -1;
	}
	/*END: Fill silence buffer */

	/*Set the signal handlers for segv and pipe */
	signal (SIGPIPE, sigpipe);
	signal (SIGTERM, sigterm);
	/*END: Set the signal handlers for segv and pipe */

	// clear conferences

	memset (Conferences, 0, sizeof (Conferences));

	int             confno;

	// confno = arc_conference_create_by_name (zCall, Conferences, 48, "RAHUL");
	//fprintf(stderr, " %s() \"RAHUL\" test conference = %d\n", __func__, confno);

	//confno = arc_conference_create_by_name(zCall, Conferences, 48, "JOE");
	//fprintf(stderr, " %s() \"JOE\" test conference = %d\n", __func__, confno);

	/*Set the version info */
	if (argc == 2 && strcmp (argv[1], "-v") == 0)
	{
#ifdef SR_SPEECHWORKS
		sprintf (yStrSR, "%s", "SpeechWorks");
#elif SR_MRCP
		sprintf (yStrSR, "%s", "MRCP");
#endif
		fprintf (stdout,
				 "ARC Media Manager for SIP Telecom Services.(%s).\n"
				 "Compiled on %s %s with %s Speech Recognition.\n",
				 argv[0], __DATE__, __TIME__, yStrSR);

#ifdef ACU_LINUX
		fprintf (stdout, " Aculab Compatible.\n");
#endif

#ifdef ACU_RECORD_TRAILSILENCE
		fprintf (stdout, " Aculab Trailsilence Enabled.\n");
#endif

//		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEDIAMGR_SHUTDOWN,
//				   INFO, "Shutting down: version display only.");
		exit (-1);
	}
	/*END: Set the version info */

	maxNumberOfCalls = MAX_NUM_CALLS;

	sprintf (gHostName, "%s", 0);
	gethostname (gHostName, 255);

#if 0
	yRc = getIpAddress (gHostName, gHostIp, yErrMsg);
	if (yRc < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_BASE, ERR, yErrMsg);
		sleep (EXIT_SLEEP);
		return (-1);
	}
#endif

	parse_arguments (mod, argc, argv);

	if ((gStartPort < 0) ||
		(gEndPort < 0) ||
		(gDynMgrId < 0) ||
		(gStartPort > gEndPort) ||
		(gStartPort > MAX_PORTS) ||
		(gEndPort > MAX_PORTS) || (gDynMgrId > (MAX_PORTS / 48)))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Shutting down: Failed to start Media Manager, wrong parameter(s)."
				   "Start Port (%d)"
				   "End Port (%d)"
				   "Media Manager instance (%d)",
				   gStartPort, gEndPort, gDynMgrId);

		exit (-1);
	}
	else
	{

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Started Media Manager, with parameters."
				   "Start Port (%d)"
				   "End Port (%d)"
				   "Media Manager instance (%d)",
				   gStartPort, gEndPort, gDynMgrId);
	}

#if 0

	/*Bind Media Mgr with last CPU */
	yRc = bindMediaMgr ();
	if (yRc != 0)
	{
	}
	/*END: Bind Media Mgr with last CPU */
#endif

	/*Initialize IPC between CallMgr and MediaMgr */
#if 0
	yRc = removeSharedMem (-1);
	if (yRc == -1)
	{
		sleep (EXIT_SLEEP);
		return (-1);
	}
#endif

	yRc = createSharedMem (-1);
	if (yRc == -1)
	{
		sleep (EXIT_SLEEP);
		return (-1);
	}

	yRc = attachSharedMem (-1);
	if (yRc == -1)
	{
		sleep (EXIT_SLEEP);
		return (-1);
	}
	/*END: Initialize IPC between CallMgr and MediaMgr */

	ortp_init ();

	if (isModuleEnabled (DEBUG_MODULE_STACK))
	{
		ortp_set_debug_file ("ARC_SIP_SDM", stdout);
	}
	else
	{
		ortp_set_debug_file ("ARC_SIP_SDM", NULL);
	}

#ifdef ACU_RECORD_TRAILSILENCE

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "ACU: if - gTrailSilenceDetection = %d", gTrailSilenceDetection);

	if (gTrailSilenceDetection == 1)
	{
//Initialize Aculab
	char           *pIspBase;
	char            ipCfg[256];
	char            errMsg[512];
	char            str[256];
	char           *p;
	int             rc;

		pIspBase = (char *) getenv ("ISPBASE");
		if (pIspBase == NULL)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_DATA_NOT_FOUND,
					   WARN,
					   "ISPBASE environment variable not set. "
					   "Unable to get Debug_Modules (%s) from (%s).  "
					   "Defaulting to off.", "Debug_Modules", ipCfg);
			return (0);
		}

		sprintf (ipCfg, "%s/Telecom/Tables/.TEL.cfg", pIspBase);
		memset ((char *) errMsg, '\0', sizeof (errMsg));
		memset ((char *) str, '\0', sizeof (str));

		if ((rc = get_name_value ("IP", "Debug_Modules", "",
								  str, sizeof (str), ipCfg, errMsg)) == -1)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_DATA_NOT_FOUND,
					   WARN,
					   "Unable to get Debug_Modules (%s) from (%s).  "
					   "Defaulting to off.", "Debug_Modules", ipCfg);
		}

		p = strstr (str, "TING");
		if (p != NULL)
		{
			sTonesLog (__FILE__, __LINE__, DEF_PORT, mod, REPORT_VERBOSE,
					   TONES_LOG_BASE, INFO, "TiNG tracing is enabled.");
			TiNGtrace = 9;
			TiNG_showtrace = &tingLogPrint;
			LOG_CS = acu_os_create_critical_section ();
		}
		else
		{
			sTonesLog (__FILE__, __LINE__, DEF_PORT, mod, REPORT_VERBOSE,
					   TONES_LOG_BASE, INFO, "TiNG tracing is disabled.");
		}

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "gMaxAcuPorts=%d.", gMaxAcuPorts);

		for (int i = 0; i < gMaxAcuPorts; i++)
		{
			initPort (i, RECORD_CHAN);
		}

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling aInitialize().");
		if ((yRc = aInitialize (DEF_PORT, &gACard)) != gMaxAcuPorts)
		{
			if (yRc > 0)
			{
				//gAculabTrialSilenceEnabled = 1;
				gTrailSilenceDetection = 1;
				gMaxAcuPorts = yRc;
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Only %d resources available; setting gMaxAcuPorts to %d.",
						   yRc, gMaxAcuPorts);
			}
			else
			{
				//gAculabTrialSilenceEnabled = 0;
				gTrailSilenceDetection = 0;
			}
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "aInitialize() returned %d.", yRc);

			gTrailSilenceDetection = 1;
		}
	}
#else
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "ACU: else - gTrailSilenceDetection = %d", gTrailSilenceDetection);
#endif

	ortp_scheduler_init();

	/*END: ORTP init */

	for (i = 0; i < MAX_PORTS; i++)
	{
		arcDtmfData[i].MFmode = 0;
		arcDtmfData[i].last = DSIL;
		arcDtmfData[i].x = DSIL;
		arcDtmfData[i].result[0] = 0;
	}

	ttsMrcpDetail_fp = fopen (".mrcpTTSRtpDetails", "a");

	for (i = gStartPort; i <= gEndPort; i++)
	{

		arc_fax_clone_globals_to_gCall (i);

		gCall[i].thirdParty = 0;

		memset (gCall[i].bridgeCallData, 0,
				sizeof (struct BridgeCallData) * BRIDGE_DATA_ARRAY_SIZE);
		gCall[i].lastReadPointer = 0;
		gCall[i].lastWritePointer = 0;

		gCall[i].lastDtmf = 16;
		gCall[i].lastDtmfMilliSec = 0;
		gCall[i].lastDtmfTimestamp = 0;
		gCall[i].codecBufferSize = 160;
		gCall[i].codecSleep = 20;
		gCall[i].parent_idx = -1;

		memset (gCall[i].lastInData, 0, 160);

		gCall[i].audioCodecString[0] = '\0';

		gCall[i].leg = LEG_A;
		gCall[i].crossPort = -1;
		gCall[i].in_speakfile = 0;
		gCall[i].responseFifoFd = -1;
		gCall[i].lastResponseFifo[0] = 0;

		gCall[i].clear = 1;

		/*RTP Sleep */
		gCall[i].lastOutRtpTime.time = 0;
		gCall[i].lastInRtpTime.time = 0;

		gCall[i].last_serial_number = 0;

		pthread_mutex_init (&(gCall[i].gMutexRequest), NULL);
		pthread_mutex_init (&(gCall[i].gMutexSpeak), NULL);

		gCall[i].pFirstRequest = NULL;
		gCall[i].pLastRequest = NULL;

		gCall[i].pFirstSpeak = NULL;
		gCall[i].pLastSpeak = NULL;

		gCall[i].pFirstBkpSpeak = NULL;
		gCall[i].pLastBkpSpeak = NULL;

		gCall[i].gUtteranceFile[0] = 0;
		gCall[i].mrcpServer[0] = 0;
		gCall[i].mrcpRtpPort = 0;
		gCall[i].mrcpRtcpPort = 0;
		gCall[i].canBargeIn = 0;

		gCall[i].googleStreamingSR = 0;
		gCall[i].googlePromptIsPlaying = 0;
		gCall[i].googleBarginDidOccur = 0;
		gCall[i].speechRecFromClient = 0;
		gCall[i].speechRec = 0;
		gCall[i].waitForPlayEnd = 0;
		gCall[i].attachedSRClient = 0;

//		gCall[i].vstreamFd = -1;
//		gCall[i].vstreamFifo[0] = 0;
//		gCall[i].vstreamOccuredAtleastOnce = 0;
//
//		gCall[i].vlastBufferFd = -1;
		/*END:Streaming */
#ifdef VOICE_BIOMETRICS
        gCall[zCall].continuousVerify = CONTINUOUS_VERIFY_INACTIVE;
        gCall[zCall].avb_bCounter   = 0;
//      gCall[zCall].avb_bufferPCM = NULL;
#endif  // END: VOICE_BIOMETRICS

		gCall[i].outboundRetCode = 0;
		gCall[i].GV_CallProgress = 0;
		gCall[i].GV_LastCallProgressDetail = 0;
		gCall[i].GV_SRDtmfOnly = 0;
		gCall[i].GV_MaxPauseTime = 60;
		gCall[i].GV_PlayBackBeepInterval = 5;
		gCall[i].GV_KillAppGracePeriod = 5;
		gCall[i].GV_RecordTermChar = ' ';
        gCall[i].GV_HideDTMF = 0;

		gCall[i].GV_DtmfBargeinDigits[0] = '\0';
		gCall[i].GV_PlayBackPauseGreetingDirectory[0] = '\0';
		gCall[i].GV_CallingParty[0] = '\0';
		gCall[i].GV_CustData1[0] = '\0';
		gCall[i].GV_CustData2[0] = '\0';
		gCall[i].GV_DtmfBargeinDigitsInt = 0;
		gCall[i].playBackOn = 0;
//		gCall[i].vplayBackOn = 0;


	sprintf(gCall[i].GV_SkipTerminateKey,	GV_SkipTerminateKey);
	sprintf(gCall[i].GV_SkipRewindKey,		GV_SkipRewindKey);
	sprintf(gCall[i].GV_SkipForwardKey,		GV_SkipForwardKey);
	sprintf(gCall[i].GV_SkipBackwardKey,	GV_SkipBackwardKey);
	sprintf(gCall[i].GV_PauseKey,			GV_PauseKey);
	sprintf(gCall[i].GV_VolumeUpKey,		GV_VolumeUpKey);
	sprintf(gCall[i].GV_VolumeDownKey,		GV_VolumeDownKey);
	sprintf(gCall[i].GV_SpeedUpKey,			GV_SpeedUpKey);
	sprintf(gCall[i].GV_SpeedDownKey,		GV_SpeedDownKey);
	sprintf(gCall[i].GV_ResumeKey,			GV_ResumeKey);

		//gCall[i].GV_SkipTerminateKey[0] = '#';
		//gCall[i].GV_SkipRewindKey[0] = '\0';
		//gCall[i].GV_SkipForwardKey[0] = '\0';
		//gCall[i].GV_SkipBackwardKey[0] = '\0';
		//gCall[i].GV_PauseKey[0] = '\0';
		//gCall[i].GV_VolumeUpKey[0] = '0';
		//gCall[i].GV_VolumeDownKey[0] = '1';
		//gCall[i].GV_SpeedUpKey[0] = '\0';
		//gCall[i].GV_SpeedDownKey[0] = '\0';


		gCall[i].GV_Volume = 5;
		gCall[i].GV_Speed = 0;

		gCall[i].GV_OutboundCallParm[0] = '\0';
		gCall[i].GV_InboundCallParm[0] = '\0';

		gCall[i].crossPort = -1;
		gCall[i].rtpSendGap = 160;
		gCall[i].sendingSilencePackets = 1;
		gCall[i].dtmfAvailable = 0;
		gCall[i].speakStarted = 0;
		gCall[i].firstSpeakStarted = 0;

		gCall[i].rtp_session = NULL;
		gCall[i].rtp_session_in = NULL;


#ifdef ACU_LINUX
		gCall[i].rtp_session_conf_send = NULL;
		gCall[i].rtp_session_conf_recv = NULL;
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting conferenceStarted to 0.");
#endif

		gCall[i].conferenceStarted = 0;
		gCall[i].in_user_ts = 0;
		gCall[i].out_user_ts = 0;
		gCall[i].inTsIncrement = 160;
		gCall[i].outTsIncrement = 160;

		gCall[i].callNum = i;
		gCall[i].callState = CALL_STATE_IDLE;
		gCall[i].callSubState = CALL_STATE_IDLE;
		gCall[i].stopSpeaking = 0;
		gCall[i].lastTimeThreadId = 0;
		gCall[i].outputThreadId = 0;
		gCall[i].cleanerThread = 0;
		gCall[i].permanentlyReserved = 0;
		gCall[i].appPassword = 0;
		gCall[i].appPid = -1;

		gCall[i].lastError = CALL_NO_ERROR;

		gCall[i].srClientFifoFd = -1;
		gCall[i].srClientFifoName[0] = 0;
		gCall[i].ttsClientFifoFd = -1;

		for (j = 0; j < MAX_THREADS_PER_CALL; j++)
		{
			gCall[i].threadInfo[j].appRef = -1;
			gCall[i].threadInfo[j].threadId = 0;
		}

		gCall[i].ttsRtpPort = (i * 2 + LOCAL_STARTING_RTP_PORT) + 700;
		gCall[i].ttsRtcpPort = (i * 2 + LOCAL_STARTING_RTP_PORT) + 701;
		gCall[i].ttsMrcpFileFd = -1;
		gCall[i].keepSpeakingMrcpTTS = 0;
		gCall[i].finishTTSSpeakAndExit = 0;
		gCall[i].ttsRequestReceived = 0;

#ifdef SR_MRCP
		gCall[i].pFirstRtpMrcpData = NULL;
		gCall[i].pLastRtpMrcpData = NULL;

		gCall[i].gUtteranceFileFp = NULL;
		memset (gCall[i].gUtteranceFile, 0, sizeof (gCall[i].gUtteranceFile));

		gCall[i].rtp_session_mrcp = NULL;
		gCall[i].mrcpTs = 0;
		gCall[i].mrcpTime = 0;
#endif

		gCall[i].pendingInputRequests = 0;

		gCall[i].pendingOutputRequests = 0;

		gCall[i].speakProcessWriteFifoFd = -1;
		gCall[i].speakProcessReadFifoFd = -1;

		memset (&gCall[i].call_type, 0, 32);

		gCall[i].recordStarted = 0;
		gCall[i].recordStartedForDisconnect = 0;            // MR 5126

		gCall[i].currentAudio.appRef = -1;
		gCall[i].currentAudio.isReadyToPlay = NO;
		gCall[i].audioLooping = SPECIFIC;

		gCall[i].isBeepActive = 0;
		gCall[i].isCallAnswered = NO;
		gCall[i].isCallInitiated = NO;
		generateAudioSsrc (i);
		gCall[i].isIFrameDetected = 0;
		memset (gCall[i].rdn, 0, 32);
		memset (gCall[i].rdn, 0, 32);
		gCall[i].full_duplex = SENDRECV;
		gCall[i].yDropTime = 0;
		gCall[i].yInitTime = 0;
		gCall[i].recordOption = WITHOUT_RTP;
		gCall[i].syncAV = -1;
		gCall[i].appRefToBePlayed = -1;

		gCall[i].inputThreadId = 0;
		gCall[i].outputThreadId = 0;
		gCall[i].clearPlayList = 1;
		gCall[i].isG729AnnexB = NO;
		gCall[i].lastRecordedRtpTs = 0;
		gCall[i].ttsFd = -1;
		gCall[i].outboundSocket = -1;
		gCall[i].pPlayQueueList = NULL;
		gCall[i].lastStopActivityAppRef = -1;

		gCall[i].GV_RecordUtterance = 0;
		gCall[i].recordUtteranceWithWav = 1;
		memset (gCall[i].lastRecUtteranceFile, 0, 128);
		memset (gCall[i].lastRecUtteranceType, 0, 128);

		setRtpPort (i);
		setTtsRtpPort (i, ttsMrcpDetail_fp);

		gCall[i].detectionTID = -1;
		gCall[i].runToneThread = 0;
		gCall[i].toneDetect = 0;
		gCall[i].toneDetected = 0;

		// this is inited once
        // and possibly again in clear port 

		arc_audio_frame_init (&gCall[i].audio_frames[AUDIO_OUT], 320,
							  ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_init (&gCall[i].audio_frames[AUDIO_IN], 320,
							  ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_init (&gCall[i].audio_frames[AUDIO_MIXED], 320,
							  ARC_AUDIO_FRAME_SLIN_16, 1);
		arc_audio_frame_init (&gCall[i].audio_frames[CONFERENCE_AUDIO], 320,
							  ARC_AUDIO_FRAME_SLIN_16, 0);

		// but reset many times 

		gCall[i].GV_FaxState = 0;

		gCall[i].sendCallProgressAudio = 0;
		gCall[i].rtp_session_tone = NULL;
		gCall[i].toneTs = 0;
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting conferenceStarted to 0.");
		gCall[i].conferenceStarted = 0;
		for (int count = 0; count < MAX_CONF_PORTS; count++)
		{
			gCall[i].conf_ports[count] = -1;
		}
		gCall[i].confUserCount = 0;
		memset (gCall[i].confID, 0, sizeof (gCall[i].confID));

		gCall[i].isFaxReserverResourceCalled = 0;
		gCall[i].isSendRecvFaxActive = 0;
		gCall[i].sentFaxPlaytone = 0;

		gCall[i].silentRecordFlag = 0;
		gCall[i].gSilentInRecordFileFp = NULL;
		gCall[i].gSilentOutRecordFileFp = NULL;

#ifdef ACU_RECORD_TRAILSILENCE
		gCall[i].zAcuPort = -1;
#endif

		gCall[i].isCalea = -1;
		gCall[i].GV_BridgeRinging = 0;
		gCall[i].GV_FlushDtmfBuffer = 0;
		gCall[i].GV_LastPlayPostion = 0;
		gCall[i].GV_SkipTimeInSeconds = 2;

		memset (&gCall[i].answerCallResp, 0,
				sizeof (gCall[i].answerCallResp));

		gCall[i].isItAnswerCall = 0;
		gCall[i].confPacketCount = 0;

		gCall[i].tts_ts = 0;

		gCall[i].currentPhraseOffset = -1;
		gCall[i].currentPhraseOffset_save = -1;
		gCall[i].currentSpeakSize = -1;

// MR 4655
		gCall[i].sendSavedInitiateCall = 0;
// END: MR 4655
#ifdef VOICE_BIOMETRICS
//      gCall[i].avb_bufferPCM = (short *)malloc(MAX_AVB_DATA + 1);
//      dynVarLog (__LINE__, i, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//              "DJB: alloc'd %d to avb_bufferPCM.[%p] ",
//              MAX_AVB_DATA + 1, gCall[i].avb_bufferPCM);
#endif // END: VOICE_BIOMETRICS

		gCall[i].poundBeforeLeadSilence = 0; // BT-226

		//Google
		gCall[i].googleUDPFd = -1;
		gCall[i].googleUDPPort = -1;
		gCall[i].googleRecord = NO;
		gCall[i].googleRecordIsActive = 0;
		memset (&gCall[i].googleSRResponse, 0, sizeof(gCall[i].googleSRResponse));
		memset (gCall[i].callCDR, 0, sizeof (gCall[i].callCDR));
	}
	fclose (ttsMrcpDetail_fp);

#if 0
	/*ORTP init */
	ortp_init ();

	if (isModuleEnabled (DEBUG_MODULE_STACK))
	{
		ortp_set_debug_file ("ARC_SIP_SDM", stdout);
	}
	else
	{
		ortp_set_debug_file ("ARC_SIP_SDM", NULL);
	}
	//ortp_scheduler_init();
	/*END: ORTP init */

	for (i = 0; i < MAX_PORTS; i++)
	{
	char            yProfileName[20];

		sprintf (yProfileName, "rtp_profile_%d", i);

		gRtpProfiles[i].rtpProfile = rtp_profile_new (yProfileName);
	}
#endif

	/*Open Connection to RequestFifo */
	yRc = openChannelToDynMgr ();
	if (yRc != 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEDIAMGR_SHUTDOWN,
				   INFO, "Shutting down: Unable to open channel to dynmgr.");
		// Error logged in routine.
		sleep (EXIT_SLEEP);
		exit (-1);
	}
	/*END: Open Connection to RequestFifo */

#ifdef  ACU_LINUX
	/*Open Connection to Tones Client */
	yRc = openChannelToTonesClient ();
//    if (yRc != 0)
//   {
	// Error logged in routine.

//        dynVarLog(__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
//           "Failed to open request fifo to Tones Client. yRc=%d.",
//          yRc);

	//sleep(EXIT_SLEEP);
	//exit(-1);
//  }
	/*END: Open Connection to Tones Client */
#endif

	sendMediaDetailsToDM ();

	/*Google Thread init*/

	/*ShmReaderThread init */
	yRc = pthread_attr_init (&gShmReaderThreadAttr);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "Shutting down: pthread_attr_init() failed. rc=%d. [%d, %s] "
				   "Unable to initialize thread. Exiting.",
				   yRc, errno, strerror (errno));
		sleep (EXIT_SLEEP);
		exit (-1);
	}

	yRc =
		pthread_attr_setdetachstate (&gShmReaderThreadAttr,
									 PTHREAD_CREATE_DETACHED);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "Shutting down: pthread_attr_setdetachstate() failed. rc=%d. [%d, %s] "
				   "Unable to initialize thread. Exiting.",
				   yRc, errno, strerror (errno));
		sleep (EXIT_SLEEP);
		exit (-1);
	}
	yRc =
		pthread_create (&gShmReaderThreadId, &gShmReaderThreadAttr,
						readAndProcessShmData, NULL);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "Shutting down: pthread_create() failed. rc=%d. [%s, %d] "
				   "Unable to create shared memory reader thread. Exiting.",
				   yRc, errno, strerror (errno));
		sleep (EXIT_SLEEP);
		exit (-1);
	}
	/*END: ShmReaderThread init */

	/*Genereal purpose thread attribute */
	yRc = pthread_attr_init (&gpthread_attr);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "Shutting down: pthread_attr_init() failed. rc=%d. [%d, %s] "
				   "Unable to initialize thread. Exiting.",
				   yRc, errno, strerror (errno));
		sleep (EXIT_SLEEP);
		exit (-1);
	}

	yRc =
		pthread_attr_setdetachstate (&gpthread_attr, PTHREAD_CREATE_DETACHED);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "Shutting down: pthread_attr_setdetachstate() failed. rc=%d. [%d, %s] "
				   "Unable to initialize thread. Exiting.",
				   yRc, errno, strerror (errno));
		sleep (EXIT_SLEEP);
		exit (-1);
	}
	/*END: General purpose thread attribute */

	/*Google SR Response Reader Thread*/
	if(gGoogleSR_Enabled == 1)
	{
		startGoogleReaderThread();
	}

	while (!gCanExit)
	{
		sleep (1);
	}

	arc_clock_sync_free (clock_sync_in);
	// arc_clock_sync_free(clock_sync_out);

	/*App cleanup */
	/*1: Shm Cleanup */
	yRc = removeSharedMem (-1);
	/*END: App cleanup */

	for (i = gStartPort; i <= gEndPort; i++){
      clearPortAtExit(i, __func__, 0);
    }

    arc_dbg_close();
    return 0;

}								/*END: main */

int
addToPlayList (struct Msg_PlayMedia *zpPlay, long *zpQueueElementId)
{
	char            mod[] = { "addToPlayList" };
	PlayList       *pPlay;
	int             zCall = zpPlay->appCallNum;

	if (zCall < 0 || zCall > MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT,
				   INFO,
				   "Invalid port (%d) received.  Unable to add request to play list. "
				   "Returning -1.", zCall);
		return (-1);
	}

	pthread_mutex_lock (&gCall[zCall].gMutexPlay);

	*zpQueueElementId = zpPlay->appRef;
	if (gCall[zCall].pFirstPlay == NULL)
	{
		gCall[zCall].pFirstPlay =
			(PlayList *) arc_malloc (zCall, mod, __LINE__, sizeof (PlayList));

		if (gCall[zCall].pFirstPlay == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to play list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexPlay);
			return (-1);
		}

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "", -1);

		memcpy (&gCall[zCall].pFirstPlay->msgPlay, zpPlay,
				sizeof (struct Msg_PlayMedia));
		gCall[zCall].pFirstPlay->nextp = NULL;
		gCall[zCall].pLastPlay = gCall[zCall].pFirstPlay;
	}
	else
	{
		pPlay =
			(PlayList *) arc_malloc (zCall, mod, __LINE__, sizeof (PlayList));

		if (pPlay == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR,
					   ERR,
					   "Failed to allocate memory. [%d, %s] Unable to add request to play list.",
					   errno, strerror (errno));
			pthread_mutex_unlock (&gCall[zCall].gMutexPlay);
			return (-1);
		}

		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "", -1);

		memcpy (&pPlay->msgPlay, zpPlay, sizeof (struct Msg_PlayMedia));
		pPlay->nextp = NULL;
		gCall[zCall].pLastPlay->nextp = pPlay;
		gCall[zCall].pLastPlay = pPlay;
	}

	pthread_mutex_unlock (&gCall[zCall].gMutexPlay);
	return (0);

}								/*END: int addToPlayList */

int
getPlayFileName (int zCall, PlayList * pPlay)
{
	char            mod[] = { "getPlayFileName" };
	PlayList       *pTemp = NULL;

	if (pthread_mutex_trylock (&gCall[zCall].gMutexPlay) == EBUSY)
	{
		return (0);
	}

	pTemp = gCall[zCall].pFirstPlay;

	while (pTemp != NULL)
	{
		if (pPlay->msgPlay.appRef == pTemp->msgPlay.appRef)
		{
			sprintf (pPlay->msgPlay.videoFileName, "%s",
					 pTemp->msgPlay.videoFileName);
			break;
		}

		pTemp = pTemp->nextp;

	}							/*END: while */

	pthread_mutex_unlock (&gCall[zCall].gMutexPlay);

	return (0);

}								/*END: int getPlayFileName */

int
clearPlayList (int zCall)
{
	char            mod[] = { "clearPlayList" };

	PlayList       *pPlay;
	SpeakList      *pSpeak;

	if (zCall < gStartPort || zCall > gEndPort)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT,
				   WARN,
				   "Invalid port (%d) received.  Unable to clea play list. "
				   "Returning -1.", zCall);
		return (-1);
	}

	if (pthread_mutex_trylock (&gCall[zCall].gMutexPlay) == EBUSY)
	{
		__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "CLEANING PLAY LIST FAILED", 0);
		return (-1);
	}

	__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "CLEANING PLAY LIST", 0);

	while (gCall[zCall].pFirstPlay != NULL)
	{
		pPlay = gCall[zCall].pFirstPlay->nextp;
		arc_free (zCall, mod, __LINE__, gCall[zCall].pFirstPlay,
				  sizeof (PlayList));
		free (gCall[zCall].pFirstPlay);
		gCall[zCall].pFirstPlay = pPlay;
	}

	gCall[zCall].pLastPlay = NULL;
	gCall[zCall].pFirstPlay = NULL;

	pthread_mutex_unlock (&gCall[zCall].gMutexPlay);

	return (0);

}								/*END: int clearPlayList */

unsigned long
msec_gettimeofday ()
{
unsigned long   t = 0;
struct timeval  tm;
int             st;

	st = gettimeofday (&tm, NULL);
	if (!st)
	{							//success
		t = (unsigned long) (tm.tv_sec * 1000 + tm.tv_usec / 1000);	// in millisec
	}
	else
	{
		t = 0;
	}
	// for Linux RedHat 6.0, it is unknown why t was only 0.1 nanosec.
	return t;
}

unsigned long
MCoreAPITime ()
{
unsigned long   t = 0;
struct timeval  tm;
int             st;

	st = gettimeofday (&tm, NULL);
	if (!st)
	{							//success
		t = (unsigned long) (tm.tv_sec * 1000 + tm.tv_usec / 1000);	// in millisec
	}
	else
	{
		t = 0;
	}
	// for Linux RedHat 6.0, it is unknown why t was only 0.1 nanosec.
	return t;
}

void
ArcDynamicSleep (unsigned long dwMilsecs, unsigned long *dwPrevSleepTimestamp,
				 unsigned long *dwExpectSleepTimestamp)
{
	unsigned long   l_dwCurrTime;
	unsigned long   l_dwDiffTime, l_dwDelayTime = 0, l_dwSleepTime;	// for periodic timer fine-tuning
	unsigned long   dwMilsecs2;

	if (dwMilsecs == 0)
	{
		return;
	}

	dwMilsecs2 = (dwMilsecs << 2) & 0x3fff;
	l_dwCurrTime = MCoreAPITime ();
	*dwExpectSleepTimestamp += dwMilsecs;
	if ((*dwPrevSleepTimestamp - l_dwCurrTime) < dwMilsecs2)
		// check in case l_dwCurrTime runs slower than dwPrevSleepTimestamp
	{
		l_dwDiffTime = 0;
	}
	else
	{
		l_dwDiffTime =
			l_dwCurrTime >=
			*dwPrevSleepTimestamp ? l_dwCurrTime -
			*dwPrevSleepTimestamp : (MAX_DWORD - *dwPrevSleepTimestamp + 1) +
			l_dwCurrTime;
	}
	if (l_dwDiffTime < dwMilsecs)
	{
		// hasn't missed the timeshot, delay.
		// if time difference is 0, we don't do MSleep() as we don't want to relinquish process.
		// However, if l_dwSleepTime is 0 due to time adjustment, we MSleep() to give way.
		l_dwSleepTime = dwMilsecs - l_dwDiffTime;	// l_dwSleepTime at this point always > 0.
		// adjust sleep time
		if (l_dwCurrTime >= *dwExpectSleepTimestamp)	// we ignore the case of max value loopover as it is not critical
		{
			// we may lag behind schedule
			l_dwSleepTime--;
			if ((l_dwCurrTime ^ *dwExpectSleepTimestamp) && l_dwSleepTime)
			{
				l_dwSleepTime--;
			}
			if (l_dwDelayTime > l_dwSleepTime)	// always l_dwSleepTime >= 0
			{
				if (l_dwSleepTime)
				{
					l_dwSleepTime--;
				}
			}
			if (l_dwDelayTime > 0)
			{
				if (l_dwSleepTime)
					l_dwSleepTime--;
				l_dwDelayTime--;
			}
			if (l_dwDelayTime > l_dwSleepTime)	// try to shorten l_dwSleepTime once more.  always l_dwSleepTime >= 0
			{
				if (l_dwSleepTime)
				{
					l_dwSleepTime--;
				}
			}
		}
		else
		{
			// we run too quickly, dynamically reduce l_dwDelayTime
			l_dwDelayTime >>= 1;
			l_dwSleepTime++;	// add extra 1msec delay
		}
	}
	else
	{
		l_dwSleepTime = 0;
	}

	if (l_dwSleepTime > 0)
	{
		//printf("going to sleep for %ld sec \n",l_dwSleepTime);
		MSleep (l_dwSleepTime);

#if defined(_DEBUG)
		l_dwDiffTime = MCoreAPITime () - l_dwCurrTime;
		if ((l_dwDiffTime > l_dwSleepTime * 2) && (l_dwSleepTime >= 10))
		{
		}
#endif // _DEBUG
	}
	else
	{
#if defined(_DEBUG)
		if (l_dwDiffTime > dwMilsecs2)
		{
		}
#endif // _DEBUG
	}

	*dwPrevSleepTimestamp = l_dwCurrTime + l_dwSleepTime;
}

int
playFile (int sleep_time,
		  int interrupt_option,
		  char *zFileName,
		  int synchronous,
		  int zCall,
		  int zAppRef,
		  RtpSession * zSession,
		  int *zpTermReason, struct MsgToApp *zResponse)
{
	int             yRc = 0;
	int             yRtpRc = 0;
	char            mod[] = { "playFile" };
	char           *buffer = NULL;
	int             i;
	FILE           *infile;
	char            lRemoteRtpIp[50];
	int             lRemoteRtpPort;
	struct MsgToApp response;
	char            yTmpBuffer4K[4097];
	char           *zQuery;

	guint32         user_ts = 0;
	unsigned char   header[2] = "";
	long            l_iBytesRead;
	long            tempNumOfBytes;
	const unsigned char *l_ptr;
	int             totalLengthSending = 0;
	int             startOfFile = 0;
	int             isMarkerSet = 0;
	unsigned long   l_dwNewRTPTimestamp = 0;
	unsigned long   l_dwDelayTime = 0;
	unsigned long   m_dwLastRTPTimestamp = 0;
	unsigned long   m_dwPrevGrabTime = 0;
	unsigned long   m_dwExpectGrabTime = 0;
	unsigned long   dwPrevSleepTimestamp;
	unsigned long   dwExpectSleepTimestamp;

//	gCall[zCall].vplayBackOn = 0;

	sprintf (response.message, "\0");
	if (zSession == NULL)
	{
		*zpTermReason = TERM_REASON_DEV;

		return -1;
	}

	if (zFileName == NULL || zFileName[0] == '\0')
	{
		*zpTermReason = TERM_REASON_DEV;
		return -1;
	}

	infile = fopen (zFileName, "rb");

	if (infile == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Failed to open file (%s). [%d, %s] Unable to play file.",
				   zFileName, errno, strerror (errno));
		return (-7);
	}

	*zpTermReason = TERM_REASON_DEV;

	dwPrevSleepTimestamp = dwExpectSleepTimestamp = msec_gettimeofday ();
	m_dwExpectGrabTime = m_dwPrevGrabTime = MCoreAPITime ();
	m_dwLastRTPTimestamp = 0;
	startOfFile = 1;

	memset (header, 0, 2);

	yRc = ACTION_WAIT;

	arc_frame_reset_with_silence (zCall, AUDIO_OUT, 1, (char *) __func__,
								  __LINE__);

	while (((i = fread (header, 1, 2, infile)) > 0))
	{
		if (!canContinue (mod, zCall, __LINE__))
		{
			yRc = -3;

			break;
		}

	unsigned int    l_dwDelayTime = (unsigned int) (1000.0 / 15);	// ms

		if (i != 2)
		{
			break;
		}
		else
		{
			l_iBytesRead = (header[0] << 8) | header[1];

			if (l_iBytesRead < 12 || l_iBytesRead > 5000)	// RTP header is at least 12 bytes
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_DIAGNOSE,
						   TEL_RTP_ERROR, WARN,
						   "RTP Header not found; l_iBytesRead=%d.",
						   l_iBytesRead);
				break;
			}

			if (l_iBytesRead <= 0 || l_iBytesRead > 1500)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_DIAGNOSE,
						   TEL_RTP_ERROR, WARN,
						   "RTP Header not found; l_iBytesRead=%d.",
						   l_iBytesRead);
				continue;
			}

			buffer = (char *) arc_malloc (zCall, mod, __LINE__, l_iBytesRead);
			memset (buffer, 0, l_iBytesRead);

			if ((tempNumOfBytes =
				 fread (buffer, 1, l_iBytesRead, infile)) != l_iBytesRead)
			{
				break;
			}

			l_ptr = (const unsigned char *) buffer;

			if ((l_ptr[1] & 0x80) != 0)
			{
				isMarkerSet = 0;
			}
			else
			{
				isMarkerSet = 1;
			}

			l_dwNewRTPTimestamp =
				(l_ptr[4] << 24) | (l_ptr[5] << 16) | (l_ptr[6] << 8) |
				l_ptr[7];

			if (l_dwNewRTPTimestamp < 0)
			{
				ArcDynamicSleep (20, &m_dwPrevGrabTime, &m_dwExpectGrabTime);
				l_dwNewRTPTimestamp = m_dwLastRTPTimestamp;
			}
			else if (m_dwLastRTPTimestamp != 0
					 && l_dwNewRTPTimestamp > m_dwLastRTPTimestamp)
			{
				l_dwDelayTime =
					(l_dwNewRTPTimestamp - m_dwLastRTPTimestamp) / 90;
				ArcDynamicSleep (l_dwDelayTime, &m_dwPrevGrabTime,
								 &m_dwExpectGrabTime);

				l_dwDelayTime = l_dwNewRTPTimestamp - m_dwLastRTPTimestamp;
			}
			else
			{
				//ArcDynamicSleep(l_dwDelayTime, &m_dwPrevGrabTime, &m_dwExpectGrabTime);
				l_dwDelayTime = 0;
				//dynVarLog(__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
				//  "After sleeping for 0, m_dwPrevGrabTime=%ld, m_dwExpectGrabTime=%ld\n", 
				//  m_dwPrevGrabTime, m_dwExpectGrabTime); 
			}
			m_dwLastRTPTimestamp = l_dwNewRTPTimestamp;
		}

		gCall[zCall].last_serial_number++;

		totalLengthSending += l_iBytesRead + 12;
		user_ts += l_dwNewRTPTimestamp;

	const BYTE     *l_ptr = (const BYTE *) buffer;

		if ((l_ptr[1] & 0x80) != 0)
		{
			if (interrupt_option != 0 && interrupt_option != NONINTERRUPT &&	/*0: No interrupt */
				((gCall[zCall].GV_DtmfBargeinDigitsInt == 0) ||
				 (gCall[zCall].GV_DtmfBargeinDigitsInt == 1 &&
				  strchr (gCall[zCall].GV_DtmfBargeinDigits,
						  dtmf_tab[gCall[zCall].lastDtmf]) != NULL)))
			{
				sprintf (zResponse->message, "%c",
						 dtmf_tab[gCall[zCall].lastDtmf]);

				if ( gCall[zCall].dtmfAvailable == 1)
				{
					*zpTermReason = TERM_REASON_DEV;

					yRc = ACTION_GET_NEXT_REQUEST;

					break;
				}
			}
		}

		if (buffer)
		{
			arc_free (zCall, mod, __LINE__, buffer, l_iBytesRead);
			free (buffer);
			buffer = NULL;
		}

	}

	if (buffer)
	{
		arc_free (zCall, mod, __LINE__, buffer, l_iBytesRead);
		free (buffer);
		buffer = NULL;
	}

	fclose (infile);

	return (yRc);

}								/*END: playFile */

int
processSpeakMrcpTTS (int zCall, MsgToDM * zMsgToDM, int *zpTermReason,
					 struct MsgToApp *zResponse, int interrupt_option)
{
	static char     mod[] = "processSpeakMrcpTTS";
	int             yPayloadType = -1;
	int             yCodecType = -1;
	char            mrcpServer[256] = "";
	int             mrcpRtpPort = -1;
	int             mrcpRtcpPort = -1;
	long            speakId;
	char            buffer[256];
	gint            have_more;
	int             rc;
	int             rc1;
	int             i;

	//guint32           ts=0;
	ARC_TTS_REQUEST_SINGLE_DM ttsRequest;
	int             rc2;
	struct Msg_SpeakMrcpTts lSpeakMrcpTts;

	int             sessionStarted = 0;

	*zpTermReason = TERM_REASON_DEV;


    arc_rtp_session_set_ssrc(zCall, gCall[zCall].rtp_session);

    gCall[zCall].in_speakfile++;

	memcpy (&(lSpeakMrcpTts), zMsgToDM, sizeof (struct Msg_SpeakMrcpTts));

	sscanf (lSpeakMrcpTts.resource,
			"%[^|]|%d|%d|%d|%d|%ld",
			mrcpServer,
			&mrcpRtpPort,
			&mrcpRtcpPort, &yPayloadType, &yCodecType, &speakId);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
			   "Processing  DMOP_SPEAKMRCPTTS:(%d).  "
			   "mrcpServer(%s) mrcpRtpPort(%d) mrcpRtcpPort(%d) "
			   "yPayloadType(%d) yCodecType(%d) speakId(%ld) deleteFile(%d).",
			   DMOP_SPEAKMRCPTTS, mrcpServer, mrcpRtpPort, mrcpRtcpPort,
			   yPayloadType, yCodecType, speakId, lSpeakMrcpTts.deleteFile);

#if 0
	if (lSpeakMrcpTts.deleteFile == 0)
	{
		if (gCall[zCall].ttsMrcpFileFd <= 0)
		{
			if ((gCall[zCall].ttsMrcpFileFd =
				 open (lSpeakMrcpTts.file, O_CREAT | O_WRONLY, 0666)) == -1)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   WARN,
						   "Unable to open/create file (%s). fd=%d. [%d, %s]."
						   " Session recording is not possible.",
						   lSpeakMrcpTts.file, gCall[zCall].ttsMrcpFileFd,
						   errno, strerror (errno));
				gCall[zCall].ttsMrcpFileFd = -1;
				return (-1);
			}
		}
	}
#endif

	if (speakId > 0)
	{
		memset ((ARC_TTS_REQUEST_SINGLE_DM *) & ttsRequest,
				'\0', sizeof (ttsRequest));

		ttsRequest.speakMsgToDM.opcode = DMOP_START_TTS;
		ttsRequest.speakMsgToDM.appRef = gCall[zCall].msgToDM.appRef;
		sprintf (ttsRequest.port, "%d", zCall);
		sprintf (ttsRequest.pid, "%d", gCall[zCall].appPid);
		sprintf (ttsRequest.resultFifo, "%s", "notUsed");
		sprintf (ttsRequest.string, "%ld", speakId);

		(void) sendMsgToTTSClient (zCall, &ttsRequest);
	}

	memset ((char *) buffer, '\0', sizeof (buffer));

	ttsRequest.speakMsgToDM.opcode = DMOP_HALT_TTS_BACKEND;

	if (gCall[zCall].rtp_session_mrcp_tts_recv == NULL)
	{

		__DDNDEBUG__ (DEBUG_MODULE_RTP, "Creating rtp_session_mrcp_tts_recv",
					  "Call", zCall);
		if ((gCall[zCall].rtp_session_mrcp_tts_recv =
			 rtp_session_new (RTP_SESSION_RECVONLY)) == NULL)
		{
			__DDNDEBUG__ (DEBUG_MODULE_RTP, "Failed to create RTP Session",
						  "rtp_session_mrcp_tts_recv", zCall);

			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR,
					   ERR,
					   "rtp_session_new() failed to create rtp_session for TTS Speak. "
					   "Unable to speak tts.");

			//CORE rtp_session_destroy(gCall[zCall].rtp_session_mrcp_tts_recv);

			//gCall [zCall].rtp_session_mrcp_tts_recv = NULL;

			(void) sendMsgToTTSClient (zCall, &ttsRequest);

			return (0);
		}

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Successfully created rtp_session for TTS Speak.");

		//rtp_session_set_scheduling_mode(gCall[zCall].rtp_session_mrcp_tts_recv, 0);
		rtp_session_set_blocking_mode (gCall[zCall].rtp_session_mrcp_tts_recv,
									   0);

		__DDNDEBUG__ (DEBUG_MODULE_RTP, "Setting Local Address to",
					  "rtp_session_mrcp_tts_recv", mrcpRtpPort);
		rc = rtp_session_set_local_addr (gCall[zCall].
										 rtp_session_mrcp_tts_recv, gHostIp,
										 mrcpRtpPort, gHostIf);
		if (rc < 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_RTP_ERROR,
					   ERR,
					   "rtp_session_set_local_addr() failed.  "
					   "Unable to set local address to (%s:%d).  rc=%d. Unable to "
					   "receive TTS audio.", gHostIp, mrcpRtpPort, rc);

			__DDNDEBUG__ (DEBUG_MODULE_RTP, "Destroying RTP Session",
						  "rtp_session_mrcp_tts_recv", mrcpRtpPort);
			rtp_session_destroy (gCall[zCall].rtp_session_mrcp_tts_recv);

			gCall[zCall].rtp_session_mrcp_tts_recv = NULL;

			(void) sendMsgToTTSClient (zCall, &ttsRequest);

			return (0);
		}

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Successfully set rtp_session_set_local_addr(%s, %d)",
				   gHostIp, mrcpRtpPort);

		rtp_session_set_payload_type (gCall[zCall].rtp_session_mrcp_tts_recv,
									  yPayloadType);

		rtp_session_set_profile (gCall[zCall].rtp_session_mrcp_tts_recv,
								 &av_profile_array_in[zCall]);

		/*Callback for timestamp jump */
		rtp_session_signal_connect (gCall[zCall].rtp_session_mrcp_tts_recv,
									"timestamp_jump",
									(RtpCallback) recv_tsjump_cb_tts,
									&(gCall[zCall].callNum));
	}
	else
	{
	int             errorCount = 0;

		for (int i = 0; 
				i < 50 && gCall[zCall].keepSpeakingMrcpTTS && gCall[zCall].rtp_session_mrcp_tts_recv; 
				i++)
		{
			//rc = rtp_session_recv_with_ts(gCall[zCall].rtp_session_mrcp_tts_recv,
			//  buffer, 160, gCall[zCall].tts_ts, &have_more, 0, (RtpSession *) NULL);
			rc = rtp_session_recv_with_ts (gCall[zCall].
										   rtp_session_mrcp_tts_recv, buffer,
										   0, 0, &have_more, 1, NULL);
			if (rc > 0)
			{
				gCall[zCall].tts_ts += gCall[zCall].outTsIncrement;
			}
			else
			{
				errorCount++;
				if (errorCount > 5)
				{
					break;
				}
			}
		}
		//rtp_session_destroy(gCall[zCall].rtp_session_mrcp_tts_recv);
		//gCall[zCall].rtp_session_mrcp_tts_recv = NULL;
		arc_rtp_session_destroy (zCall,
								 &gCall[zCall].rtp_session_mrcp_tts_recv);
	}
#if 0
	else
	{
		rtp_session_reset (gCall[zCall].rtp_session_mrcp_tts_recv);
	}
#endif

	gCall[zCall].sendingSilencePackets = 0;

	//ts = 0;//gCall[zCall].out_user_ts;
	rc2 = 0;
	i = 0;

	// arc_frame_reset_with_silence(zCall, AUDIO_OUT, 16, (char *)__func__, __LINE__);
	//arc_frame_reset_with_silence(zCall, AUDIO_IN, 1, (char *)__func__, __LINE__);

#if 0
	char            silence320[320];

	memset (silence320, 0, 320);
	for (int i = 0; i < 10; i++)
	{
		arc_audio_frame_post (&gCall[zCall].audio_frames[AUDIO_MIXED],
							  silence320, 320, 0);
	}
#endif

	int             packetCount = 0;
	int             ttsZeroSizeCount = 0;

	arc_frame_reset_with_silence (zCall, AUDIO_OUT, 16, (char *) __func__,
								  __LINE__);

	while (gCall[zCall].keepSpeakingMrcpTTS == 1)
	{
		if (!canContinue (mod, zCall, __LINE__))
		{
			rc2 = -3;
			break;
		}

		if (gCall[zCall].pendingOutputRequests > 0)
		{

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "",
						  "Pending requests while in tts, total",
						  gCall[zCall].pendingOutputRequests);

			gCall[zCall].keepSpeakingMrcpTTS = 0;
			*zpTermReason = TERM_REASON_USER_STOP;
			rc = ACTION_GET_NEXT_REQUEST;

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "pendingOutputRequests=%d, zpTermReason=%d, TERM_REASON_USER_STOP=%d.",
					   gCall[zCall].pendingOutputRequests, *zpTermReason, TERM_REASON_USER_STOP);

			memset ((ARC_TTS_REQUEST_SINGLE_DM *) & ttsRequest, '\0',
					sizeof (ttsRequest));

			ttsRequest.speakMsgToDM.opcode = DMOP_HALT_TTS_BACKEND;
			ttsRequest.speakMsgToDM.appRef = gCall[zCall].msgToDM.appRef;
			sprintf (ttsRequest.port, "%d", zCall);
			sprintf (ttsRequest.pid, "%d", gCall[zCall].appPid);
			sprintf (ttsRequest.resultFifo, "%s", "notUsed");
			sprintf (ttsRequest.string, "%s", "notUsed");

			(void) sendMsgToTTSClient (zCall, &ttsRequest);
			break;
		}

		if (gCall[zCall].speechRec == 0 &&
			gCall[zCall].dtmfAvailable == 1 && gCall[zCall].lastDtmf < 16)
		{
			//gCall[zCall].in_user_ts+=(gCall[zCall].inTsIncrement*3);
			gCall[zCall].in_user_ts += (gCall[zCall].codecBufferSize * 3);
			sprintf (zResponse->message, "\0");

			__DDNDEBUG__ (DEBUG_MODULE_AUDIO, "", "DTMF",
						  gCall[zCall].lastDtmf);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "interrupt_option = %d, gCall[zCall].GV_DtmfBargeinDigitsInt=%d.",
					   interrupt_option,
					   gCall[zCall].GV_DtmfBargeinDigitsInt);

			if (interrupt_option != 0 && interrupt_option != NONINTERRUPT &&	/*0: No interrupt */
				((gCall[zCall].GV_DtmfBargeinDigitsInt == 0) ||
				 (gCall[zCall].GV_DtmfBargeinDigitsInt == 1 &&
				  strchr (gCall[zCall].GV_DtmfBargeinDigits,
						  dtmf_tab[gCall[zCall].lastDtmf]) != NULL)))
			{
				sprintf (zResponse->message, "%c",
						 dtmf_tab[gCall[zCall].lastDtmf]);

				if (gCall[zCall].dtmfAvailable == 1)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Setting dtmfAvailable to 0.");
					gCall[zCall].dtmfAvailable = 0;

					*zpTermReason = TERM_REASON_DTMF;

					rc = ACTION_GET_NEXT_REQUEST;
					gCall[zCall].keepSpeakingMrcpTTS = 0;
					memset ((ARC_TTS_REQUEST_SINGLE_DM *) & ttsRequest, '\0',
							sizeof (ttsRequest));
					ttsRequest.speakMsgToDM.appRef =
						gCall[zCall].msgToDM.appRef;
					ttsRequest.speakMsgToDM.opcode = DMOP_HALT_TTS_BACKEND;
					sprintf (ttsRequest.port, "%d", zCall);
					sprintf (ttsRequest.pid, "%d", gCall[zCall].appPid);
					sprintf (ttsRequest.resultFifo, "%s", "notUsed");
					sprintf (ttsRequest.string, "%s", "notUsed");

					(void) sendMsgToTTSClient (zCall, &ttsRequest);

					break;
				}
			}
			else if (interrupt_option == NONINTERRUPT)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "interrupt_option is NONINTERRUPT; setting dtmfAvailable to 0.");
				gCall[zCall].dtmfAvailable = 0;
			}
		}

		if (!canContinue (mod, zCall, __LINE__))
		{
			rc2 = -3;
			break;
		}

		//rc = rtp_session_recv_with_ts(gCall[zCall].rtp_session_mrcp_tts_recv,
		//      buffer, 160, gCall[zCall].tts_ts, &have_more, 0, (RtpSession *) NULL);
		rc = rtp_session_recv_with_ts (gCall[zCall].rtp_session_mrcp_tts_recv,
									   buffer, 0, 0, &have_more, 1, NULL);

		if (rc > 0)
		{
			packetCount++;
#if 0
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Got TTS packet <%d> of size<%d> packet data=<%hhx %hhx %hhx %hhx>.",
					   packetCount, rc,
					   buffer[12], buffer[13], buffer[14], buffer[15]);
#endif
		}

		if (gCall[zCall].finishTTSSpeakAndExit == 1)
		{
			if (rc <= 0)
			{
				if (ttsZeroSizeCount > 1)
				{
					gCall[zCall].keepSpeakingMrcpTTS = 0;
					gCall[zCall].finishTTSSpeakAndExit = 0;
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "finishTTSSpeakAndExit == 1, recieved packet of size %d getting next tts.",
							   rc);
				}
				ttsZeroSizeCount++;
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "finishTTSSpeakAndExit == 1, recieved packet of size %d ttsZeroSizeCount=%d.",
						   rc, ttsZeroSizeCount);
			}
			else
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "finishTTSSpeakAndExit == 1, Playing TTS packet of size=%d.",
						   rc);
			}
		}

// TTSBUFFER

#if 0
		if (gCall[zCall].ttsMrcpFileFd > 0 && lSpeakMrcpTts.deleteFile == 0)
		{
			rc1 = write (gCall[zCall].ttsMrcpFileFd, buffer, rc);
		}
#endif

		if (rc > 0 && gCall[zCall].rtp_session != NULL)
		{
			if (sessionStarted == 0)
			{
				sessionStarted++;
			}

			arc_frame_decode_and_post (zCall, buffer + 12, 160, AUDIO_OUT, 0,
									   (char *) __func__, __LINE__);

			if (rc == 0)
			{
				arc_frame_apply (zCall, gSilenceBuffer, 160, AUDIO_MIXED,
								 ARC_AUDIO_PROCESS_AUDIO_MIX,
								 (char *) __func__, __LINE__);
			}
			else
			{
				arc_frame_apply (zCall, buffer + 12, 160, AUDIO_MIXED,
								 ARC_AUDIO_PROCESS_AUDIO_MIX,
								 (char *) __func__, __LINE__);
			}
			if (gCall[zCall].GV_Volume != 5)
			{
				arc_frame_apply (zCall, buffer + 12, 160, AUDIO_OUT,
								 ARC_AUDIO_PROCESS_GAIN_CONTROL,
								 (char *) __func__, __LINE__);
				arc_frame_read_and_encode (zCall, buffer + 12, 160, AUDIO_OUT,
										   (char *) __func__, __LINE__);
			}
			// debugging tts issue with recording 

			rc1 = arc_rtp_session_send_with_ts (mod, __LINE__, zCall,
												gCall[zCall].rtp_session,
												buffer + 12,
												rc - 12,
												gCall[zCall].out_user_ts);

			gCall[zCall].out_user_ts += gCall[zCall].outTsIncrement;

			if (gCall[zCall].callSubState == CALL_STATE_CALL_LISTENONLY &&
				gCall[zCall].crossPort >= 0 &&
				gCall[gCall[zCall].crossPort].rtp_session != NULL)
			{
				rc1 =
					arc_rtp_session_send_with_ts (mod, __LINE__,
												  gCall[zCall].crossPort,
												  gCall[gCall[zCall].
														crossPort].
												  rtp_session, buffer + 12,
												  rc - 12,
												  gCall[gCall[zCall].
														crossPort].
												  out_user_ts);

				//gCall[gCall[zCall].crossPort].out_user_ts+=gCall[gCall[zCall].crossPort].outTsIncrement;
			}

		}
		else
		{
			// putting silence data into record buffers

			if (sessionStarted == 0)
			{
				arc_frame_decode_and_post (zCall, gCall[zCall].silenceBuffer,
										   gCall[zCall].codecBufferSize,
										   AUDIO_OUT, 0, (char *) __func__,
										   __LINE__);
				arc_frame_apply (zCall, gCall[zCall].silenceBuffer,
								 gCall[zCall].codecBufferSize, AUDIO_MIXED,
								 ARC_AUDIO_PROCESS_AUDIO_MIX,
								 (char *) __func__, __LINE__);
			}

		}

		if (rc > 0)
		{
			gCall[zCall].tts_ts += gCall[zCall].outTsIncrement;
		}

		rtpSleep (20, &gCall[zCall].lastOutRtpTime, __LINE__, zCall);

	}							/*END: while */

	//gCall[zCall].out_user_ts = ts;

	gCall[zCall].sendingSilencePackets = 1;
	gCall[zCall].finishTTSSpeakAndExit = 0;

#if 0
	if (lSpeakMrcpTts.deleteFile == 0 && gCall[zCall].ttsMrcpFileFd > 0)
	{
		close (gCall[zCall].ttsMrcpFileFd);
	}
	gCall[zCall].ttsMrcpFileFd = -1;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			   "Closing rtp_session for TTS Speak.  zpTermReason=%d, rc2=%d",
			   *zpTermReason, rc2);

	if (gCall[zCall].rtp_session_mrcp_tts_recv != NULL)
	{
		rtp_session_destroy (gCall[zCall].rtp_session_mrcp_tts_recv);
		gCall[zCall].rtp_session_mrcp_tts_recv = NULL;
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			   "Closed rtp_session for TTS Speak.  zpTermReason=%d, rc2=%d",
			   *zpTermReason, rc2);
#endif

    gCall[zCall].in_speakfile = 0;


	return (rc2);

}								// END: processSpeakMrcpTTS()

// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
#if !defined (MEMCHECK)
#define new DEBUG_NEW
#endif // !MEMCHECK
#undef THIS_FILE
static char     THIS_FILE[] = __FILE__;
#endif

#define MIN_BLOCK 32

CMBuffer::CMBuffer (int sz)
{
	char            mod[] = "CMBuffer::CMBuffer";

	if (sz > 0)
	{
		base = new MOCTET[sz];

#if 0
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "new MOCTET(%p) with size=%d", base, sz);
#endif
	}
	else
	{
		base = NULL;
	}

	max_data_size = sz;
	cur_ptr = base;

#ifdef _DEBUG
	isARtpPacket = FALSE;
#endif //_DEBUG
}

CMBuffer::~CMBuffer ()
{
	Free ();
}

CMBuffer::CMBuffer (const CMBuffer & p_other)
{
	max_data_size = 0;
#ifdef _DEBUG
	isARtpPacket = FALSE;
#endif //_DEBUG
	base = NULL;
	cur_ptr = NULL;
	operator= (p_other);
}

CMBuffer & CMBuffer::operator = (const CMBuffer & p_other)
{
	char
		mod[] = "CMBuffer::operator";

	if ((&p_other != this) && (p_other.base != NULL))
	{
		max_data_size = p_other.max_data_size;

		if (base != NULL)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Free MOCTET(%p) with size=%d.", base, max_data_size);
			delete[]base;
		}

		base = new MOCTET[max_data_size];
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "New MOCTET(%p) with size=%d.", base, max_data_size);
		memcpy (base, p_other.base, max_data_size);
		cur_ptr = base + p_other.DataCount ();

#ifdef _DEBUG
		isARtpPacket = p_other.isARtpPacket;
#endif //_DEBUG

	}
	return *this;
}

void
CMBuffer::Free ()
{
char            mod[] = "CMBuffer::Free";

	if (base)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Free MOCTET(%p) with size=%d.", base, max_data_size);
		delete[]base;
		base = NULL;
	}

	cur_ptr = base;
	max_data_size = 0;
}

MBOOL
CMBuffer::DataMakeSpace (int n)
{
	char            mod[] = "CMBuffer::DataMakeSpace";
	int             inc;
	int             sp = MaxDataSize () - DataCount ();

#if defined(_DEBUG)
	assert ((int) (sp >= 0));
#endif

	if (n <= sp)
		return TRUE;
	inc = n - sp;

	MOCTET         *b;
	int             new_size = max_data_size + MAX (MIN_BLOCK, inc);
	int             count = DataCount ();

	b = new MOCTET[new_size];
	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "New MOCTET(%p) with size=%d.", b, new_size);
	if (!b)
	{
		return FALSE;
	}

	if (base)
	{
		memcpy (b, base, count * sizeof (MOCTET));

		cur_ptr = &b[count];
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Free MOCTET(%p) with size=%d.", base, sizeof (base));
		delete[]base;
		base = b;
	}
	else
	{
		cur_ptr = base = b;
	}
	max_data_size = new_size;

	return TRUE;
}

MBOOL
CMBuffer::DataAppend (const MOCTET * b, int n)
{
	if (!b)
		return FALSE;

	if (!DataMakeSpace (n))
		return FALSE;

	memmove (cur_ptr, b, n * sizeof (MOCTET));
	cur_ptr += n;

	return TRUE;
}

// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
#if !defined (MEMCHECK)
#define new DEBUG_NEW
#endif // !MEMCHECK
#undef THIS_FILE
static char     THIS_FILE[] = __FILE__;
#endif

CMObjectPtrQueue::CMObjectPtrQueue ()
{
	queue_first = queue_last = NULL;
	queue_length = 0;
}

CMObjectPtrQueue::CMObjectPtrQueue (const CMObjectPtrQueue & from)
{
	queue_first = queue_last = NULL;
	queue_length = 0;
	Append (from);
}

// CMObjectPtrQueue::Insert(): insert object to top of queue
int
CMObjectPtrQueue::Insert (CMObject * o)
{
	if (!o)
		return -1;

	if (queue_first)
		o->next = queue_first;
	else
		queue_last = o;
	queue_first = o;
	return queue_length++;
}

int
CMObjectPtrQueue::Add (CMObject * o)
{
	if (!o)
		return -1;

	if (queue_last)
		queue_last->next = o;
	else
		queue_first = o;
	queue_last = o;
	o->next = NULL;
	return queue_length++;
}

// Remove - will remove the element indexed by i. 
CMObject       *
CMObjectPtrQueue::Remove (int i)
{
	// ensure i is valid            
	if ((i < 0) || (i >= Length ()) || (queue_first == NULL))
		return NULL;

CMObject       *r, *l = NULL;
int             j;

	for (j = 0, r = queue_first; (r != NULL) && (j < Length ());
		 j++, r = r->next)
	{
		if (j == i)
		{
			if (l)
			{					// not first in queue
				l->next = r->next;
			}
			else
			{
				queue_first = r->next;
			}
			if (r == queue_last)
			{
				queue_last = l;
			}
			queue_length--;
			return r;
		}
		l = r;
	}
	return NULL;
}

// Remove - will remove the object i. 
CMObject       *
CMObjectPtrQueue::Remove (CMObject * o)
{
	// ensure o is valid            
	if ((o == NULL) || (queue_first == NULL))
		return NULL;

CMObject       *r, *l = NULL;

	for (r = queue_first; r != NULL; r = r->next)
	{
		if (r == o)
		{
			if (l)
			{					// not first in queue
				l->next = r->next;
			}
			else
			{
				queue_first = r->next;
			}
			if (r == queue_last)
			{
				queue_last = l;
			}
			queue_length--;
			return r;
		}
		l = r;
	}
	return NULL;
}

// Append - will 
void
CMObjectPtrQueue::Append (const CMObjectPtrQueue & from)
{
	for (CMObjectPtr l_object = from; l_object; ++l_object)
		AddCopy (*l_object);
}

// operator[] - returns the element indexed by i
CMObject       *
CMObjectPtrQueue::operator [] (int i)
	 const
	 {
		 // ensure i is valid
		 if ((i < 0) || (i >= Length ()))
			 return
				 NULL;

	CMObject       *
		r;
	int
		j;
		 for (j = 0, r = queue_first; ((r != NULL) && (j < Length ()));
			  j++, r = r->next)
		 {
			 if (j == i)
				 return r;
		 }
		 return
			 NULL;
	 }

bool CMObjectPtrQueue::Member (CMObject * o) const
{
	if (o == NULL)
		return false;

	for (CMObject * r = queue_first; r != NULL; r = r->next)
	{
		if (r == o)
			return true;
	}
	return false;
}

void
CMObjectPtrQueue::Clear (bool deleteObjects)
{
char            mod[] = "CMObjectPtrQueue::Clear";

	while (queue_first != NULL)
	{
CMObject       *l_object = Remove (0);

		if (deleteObjects)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Free l_object(%p) with size=%d.", l_object,
					   sizeof (l_object));
delete          l_object;
		}
	}
}

///////////////////////////////// Object related functions ///////////////////////////////
// Object Names
//   IMPORTANT: shall map component list in mobject.h
static const char *gs_ObjectName[] = {	// MMEDIA_UNSUPPORTED=-1,
	"Null",						// MMEDIA_NULLDATA,
	"Non Standard",				// MMEDIA_NONSTANDARD,
	"Unknown Object",			// MUNKNOWN_OBJECT,

	// Comm devices
	"Modem",					// MMODEM,
	"Internet (TCP/IP)",		// MTCP,
	"Internet (UDP)",			// MUDP,
	"Real Time Protocol",		// MRTP,
	"Real-time Data Exchange(RTDX)",	// MRTDX,
	"ISDN",						// MISDN,
	"UART",						// MUART,

	// System standards
	"ITU H.320",				// MH320,
	"ITU H.323",				// MH323,
	"ITU H.324",				// MH324,
	"ITU H.324I",				// MH324I,
	"Media Gateway",			// MGATEWAY,

	// Multiplexers
	"ITU H.221",				// MH221,
	"ITU H.223",				// MH223,
	"ITU H.225.0",				// MH2250,

	// Call signaling
	"Q.931",					// MQ931,
	"V.140",					// MV140,

	// Command and control
	"ITU H.242",				// MH242,
	"ITU H.242 Tx",				// MH242TX,
	"ITU H.242 Rx",				// MH242RX,
	"ITU H.243",				// MH243,
	"ITU H.245",				// MH245,
	"H.245/H.242 Transcoder",	// MH245H242XC,

	// Video codecs
	"ITU H.261",				// MH261,
	"ITU H.262",				// MH262,
	"ITU H.263",				// MH263,
	"Non Standard Video",		// MVIDEO_NONSTANDARD,
	"IS11172",					// MVIDEO_IS11172,
	"ISO/IEC MPEG4-Video",		// MMPEG4VIDEO, 
	"H.263/H.261 Transcoder",	// MH263H261XC,
	"H.261/H.263 Transcoder",	// MH261H263XC,

	// Audio codecs
	"Non Standard Audio",		// MAUDIO_NONSTANDARD, 
	"ITU G.723.1",				// MG7231, 
	"ITU G.711",				// MG711, -- MG711 centralises ALAW and ULAW with 56K and 64K rate 
	"ITU G.711 aLaw 64k",		// MG711ALAW64K, 
	"ITU G.711 aLaw 56k",		// MG711ALAW56K, 
	"ITU G.711 uLaw 64k",		// MG711ULAW64K, 
	"ITU G.711 uLaw 56k",		// MG711ULAW56K,
	"ITU G.722 64k",			// MG722_64K,
	"ITU G.722 56k",			// MG722_56K,
	"ITU G.722 48k",			// MG722_48K, 
	"ITU G.728",				// MG728,
	"ITU G.729",				// MG729,
	"ITU G.729A",				// MG729_ANNEXA, 
	"IS11172",					// MAUDIO_IS11172,
	"IS13818",					// MAUDIO_IS13818,
	"ITU G.729 Annex A",		// MG729_ANNEXASS,
	"ITU G.729 Annex B",		// MG729_ANNEXAWB,
	"ITU G.723.1 Annex C",		// MG7231_ANNEXC,
	"ETSI GSM 06.10",			// MGSM_FULL_RATE,
	"GSM Full Rate",			// MGSM_HALF_RATE,
	"GSM Half Rate",			// MGSM_ENHANCED_RATE,
	"ETSI GSM-AMR",				// MGSMAMR,
	"G.729 Extention",			// MG729_EXT,
	"G.723.1<->G.711 Transcoder",	// MG7231G711XC,
	"G.711<->G.723.1 Transcoder",	// MG711G7231XC,

	// Data
	"Non Standard Data",		// MDATA_DAC_NONSTANDARD,
	"DSVDCNTRL",				// MDATA_DAC_DSVDCNTRL,
	"ITU T.120",				// MDATA_DAC_T120,
	"DSM_CC",					// MDATA_DAC_DSM_CC,
	"USER_DATA",				// MDATA_DAC_USER_DATA,
	"T.84",						// MDATA_DAC_T84,
	"T.434",					// MDATA_DAC_T434,
	"H.224",					// MDATA_DAC_H224,
	"NLPD",						// MDATA_DAC_NLPD,
	"H.222 Data Partitioning",	// MDATA_DAC_H222_DATA_PARTITIONING, 

	// Encryption
	"Non Standard Encryption",	// MENCRYPTION_NONSTANDARD
	"H.233",					// MH233_ENCRYPTION,
	"Media Processor",			// MMEDIAPROCESSOR,
	"User",						// MUSER_OBJECT,
	"Software",					// MSW_OBJECT,
	"Hardware",					// MHW_OBJECT,
	"Connection",				// MCONNECTION_OBJECT,

	// Channel
	"Channel",					// MCHANNEL,

	// System
	"H.32X App",				// MH32XAPP,
	"H.320 App",				// MH320APP,
	"H.323 App",				// MH323APP,
	"H.324 App",				// MH324APP,

	// Media devices
	"Unknown Media",			// MUNKNOWN_MEDIA,
	"Video Capture",			// MVIDEOCAPTURE,
	"YUV422 File",				// MYUV422FILE,
	"YUV422 Memory",			// MYUV422MEM,
	"Microsoft Windows Display",	// MMSWINDOW,
	"Microsoft Multimedia Audio",	// MMSAUDIO,
	"PCM Audio File",			// MAUDIOFILE,
	"Microsoft Direct Sound",	// MDIRECTSOUND,
	"XWindows Display",			// MXDISPLAY,
	"Meteor Video Capture",		// MMETEORCAP, 
	"MSMC200H263",				// MSMC200H263,
	"MSMC200G7231",				// MSMC200G7231,
	"MAPOLLOH263",				// MAPOLLOH263,
	"MAPOLLOG7231",				// MAPOLLOG7231,
	"Video Bitstream File",		// MVIDEOSTREAMFILE,
	"Audio Bitstream File",		// MAUDIOSTREAMFILE,

	// Additional comm devices
	"Data Pump (offline monitoring)",	// MDATAPUMP
	"E1 Card",					// ME1
	"Queue Pump (online monitoring)",	// MQUEUEPUMP
	"Pre-opened",				// MPREOPENED

	"Default Audio",			// MDEFAULT_AUDIO,
	"Default Video",			// MDEFAULT_VIDEO,
	"Default Data",				// MDEFAULT_DATA,

	// User Input
	"Non Standard User Input",	// MUSERINPUT_NONSTANDARD,
	"Basic String User Input",	// MUSERINPUT_BASICSTRING,
	"IA5 String User Input",	// MUSERINPUT_IA5STRING,
	"General String User Input",	// MUSERINPUT_GENERALSTRING,
	"DTMF User Input",			// MUSERINPUT_DTMF,
	"Hook Flash User Input",	// MUSERINPUT_HOOKFLASH,
	"Extended Alphanumeric User Input",	// MUSERINPUT_EXTENDEDALPHANUMERIC,
	"Encrypted Basic String User Input",	// MUSERINPUT_ENCRYPTEDBASICSTRING,
	"Encrypted IA5 String User Input",	// MUSERINPUT_ENCRYPTEDIA5STRING,
	"Encrypted General String User Input",	// MUSERINPUT_ENCRYPTEDGENERALSTRING,
	"Secure DTMF User Input",	// MUSERINPUT_SECUREDTMF,

	// Additional media device
	"3GPP Media File",			// M3GPPFILE

	// ADD HERE: Other object additions

	// End of list
	""							// MOBJECT_END
};

static const char gs_ObjectNoName[] = "arc default object";

M_EXPORT const char *
MObjectName (int object_id)
{
	const char     *l_pcName = NULL;

	if (object_id < 0
		|| object_id >=
		(int) (sizeof (gs_ObjectName) / sizeof (gs_ObjectName[0])))
	{
		l_pcName = gs_ObjectName[MUNKNOWN_OBJECT];
	}
	else
	{
		l_pcName = gs_ObjectName[object_id];
	}

	if (l_pcName == NULL || *l_pcName == '\0')
	{
		l_pcName = gs_ObjectNoName;
	}

	return l_pcName;
}

M_EXPORT MOBJECTID
MObjectFromName (const char *p_pcName)
{
	if (p_pcName != NULL)
	{
		for (int i = 0;
			 i < (int) (sizeof (gs_ObjectName) / sizeof (gs_ObjectName[0]));
			 i++)
		{
			if (strcmp (p_pcName, gs_ObjectName[i]) == 0)
			{
				return (MOBJECTID) i;
			}
		}
	}

	return MUNKNOWN_OBJECT;
}

// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
#if !defined (MEMCHECK)
#define new DEBUG_NEW
#endif // !MEMCHECK
#undef THIS_FILE
static char     THIS_FILE[] = __FILE__;
#endif

#define MIN_BLOCK 32

CMPacket::CMPacket (int sz):
CMBuffer (sz)
{
	// Note part of the construction is done in CMBuffer.
	error = 0;
	buffer = NULL;
	buffer_count = 0;
	max_payload_size = 0;
}

CMPacket::~CMPacket ()
{
	Free ();
}

MOCTET         *
CMPacket::Buffer () const
{
	if (buffer == NULL)
	{
	}
	else
	{
	}
	return buffer != NULL ? buffer : base;
}

int
CMPacket::BufferCount () const
{
	return buffer != NULL ? buffer_count : DataCount ();
}

void
CMPacket::BufferCount (int n)
{
	if (buffer != NULL)
	{
		buffer_count = n;
	}
	else
	{
		DataCount (n);
	}
}

MBOOL
CMPacket::DataMakeSpace (int n)
{
	// For derived class overloading
	return CMBuffer::DataMakeSpace (n);
}

void
CMPacket::Free ()
{
char            mod[] = "CMPacket::Free";

	if (buffer)
	{

		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Free buffer(%p) with size=%d.", buffer, sizeof (buffer));
		delete[]buffer;
		buffer = NULL;
	}

	CMBuffer::Free ();
	error = 0;
}

void
CMPacket::SetHeaders (void * /*hdr */ , void * /*plhdr */ )
{
}

void
CMPacket::GetHeaders (void * /*hdr */ , void * /*plhdr */ )
{
}

void
CMPacket::ConfigPayload (int /*ts */ )
{
}

void
CMPacket::Packetise (int /*ts */ )
{
}

int
CMPacket::Segmentalise (int /*inbytes */ )
{
	return 0;
}

// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
#if !defined (MEMCHECK)
#define new DEBUG_NEW
#endif // !MEMCHECK
#undef THIS_FILE
static char     THIS_FILE[] = __FILE__;
#endif

//#define _LITTLE_ENDIAN 

#define MIN_BLOCK 32

// Text string of RTP payload type
static const struct
{
	unsigned int    l_uiRtpPayloadTypeIndex;
	const char     *l_pccRtpPayloadTypeName;
} gs_RtpPayloadType[] =
{
	{
	RTP_PCMU_PAYLOAD_TYPE, "PCMU"},
	{
	RTP_PCMA_PAYLOAD_TYPE, "PCMA"},
	{
	RTP_G722_PAYLOAD_TYPE, "G.722"},
	{
	RTP_G723_PAYLOAD_TYPE, "G.723"},
	{
	RTP_G728_PAYLOAD_TYPE, "G.728"},
	{
	RTP_G729_PAYLOAD_TYPE, "G.729"},
	{
	RTP_GSMAMR_PAYLOAD_TYPE, "GSMAMR"},
	{
	RTP_H261_PAYLOAD_TYPE, "H.261"},
	{
	RTP_H263_PAYLOAD_TYPE, "H.263"},
	{
	RTP_MP4VIDEO_PAYLOAD_TYPE, "MP4VIDEO"}
};

// Note: fixed header extension (X always set to 0) is not supported in this version
// Note: max data packet size includes the sizes of RTP fixed header and payload header

CMRTPPacket::CMRTPPacket (int sz):
CMPacket (sz)
{
	char            mod[] = "CMRTPPacket::CMRTPPacket";

	memset (&hdr, 0, sizeof (hdr));
	memset (&plhdr, 0, sizeof (plhdr));

	if (sz > 0)
	{
		buffer = new MOCTET[sz];
#if 0
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "new MOCTET(%p) with size=%d", buffer, sz);
#endif
	}
	else
	{
		buffer = NULL;
	}
	buffer_count = 0;

	szFixedHdr = sizeof (hdr);
	szPayloadHdr = 0;

#ifdef _DEBUG
	isARtpPacket = TRUE;
#endif //_DEBUG
}

CMRTPPacket::~CMRTPPacket ()	// we do not free buffer memory
{
}

MBOOL
CMRTPPacket::DataMakeSpace (int n)
{
	char            mod[] = "CMRTPPacket::DataMakeSpace";
	int             inc;
	int             sp = MaxDataSize () - DataCount ();

	if (n <= sp)
	{
		return TRUE;
	}
	inc = n - sp;

	if (!CMPacket::DataMakeSpace (n))
	{
		return FALSE;			// memory allocation error
	}

	MOCTET         *b;
	int             new_size = max_data_size + max (MIN_BLOCK, inc);
	int             count = BufferCount ();

	b = new MOCTET[new_size];
	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "New MOCTET(%p) with size=%d.", b, sizeof (b));
	if (!b)
	{
		return FALSE;
	}

	if (buffer)
	{
		memcpy (b, buffer, count * sizeof (MOCTET));
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Free buffer(%p) with size=%d.", buffer, sizeof (buffer));
		delete[]buffer;
	}
	buffer = b;
	return TRUE;
}

void
CMRTPPacket::SetHeaders (void *header, void *plheader)
{
	MRTPHEADER     *ph = (MRTPHEADER *) header;
	int             headerLen =
		sizeof (MRTPHEADER) + (ph->cc - 1) * sizeof (UINT32);
// copy headers
	memcpy (&hdr, ph, headerLen);
	memcpy (&plhdr, plheader, sizeof (MRTPPAYLOADHEADER));
}

void
CMRTPPacket::GetHeaders (void *header, void *plheader)
{
	// copy headers
	memcpy (header, &hdr, szFixedHdr);
	memcpy (plheader, &plhdr, sizeof (MRTPPAYLOADHEADER));
}

void
CMRTPPacket::ConfigPayload (int ts)
{
	// determine RTP fixed and payload header sizes
	switch (media_type)
	{
#if defined(INCLUDE_G711)
	case MG711:
	case MG711ALAW64K:
	case MG711ALAW56K:
	case MG711ULAW64K:
	case MG711ULAW56K:
		// determine fixed header size
		szFixedHdr = sizeof (MRTPHEADER) + (hdr.cc - 1) * sizeof (UINT32);
		// determine payload header size
		szPayloadHdr = RTP_G711_HEADER_SIZE;
		break;
#endif // INCLUDE_G711
#if defined(INCLUDE_G7231)
	case MG7231:
	case MG7231_ANNEXC:
		// determine fixed header size
		szFixedHdr = sizeof (MRTPHEADER) + (hdr.cc - 1) * sizeof (UINT32);
		// determine payload header size
		szPayloadHdr = RTP_G723_HEADER_SIZE;
		break;
#endif // INCLUDE_G7231
#if defined(INCLUDE_GSMAMR)
	case MGSMAMR:
		// determine fixed header size
		szFixedHdr = sizeof (MRTPHEADER) + (hdr.cc - 1) * sizeof (UINT32);
		// determine payload header size
		szPayloadHdr = RTP_GSMAMR_HEADER_SIZE;
		break;
#endif // INCLUDEGSMAMR
#if defined(INCLUDE_H261)
	case MH261:
		// determine fixed header size
		szFixedHdr = sizeof (MRTPHEADER) + (hdr.cc - 1) * sizeof (UINT32);
		// determine payload header size
		szPayloadHdr = RTP_H261_HEADER_SIZE;
		break;
#endif // INCLUDE_H261
#if defined(INCLUDE_H263)
	case MH263:
		{
			// determine fixed header size
	MRTPH263PAYLOADHEADER *h263hdr = (MRTPH263PAYLOADHEADER *) & plhdr;

			szFixedHdr = sizeof (MRTPHEADER) + (hdr.cc - 1) * sizeof (UINT32);
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
		szFixedHdr = sizeof (MRTPHEADER) + (hdr.cc - 1) * sizeof (UINT32);
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

void
CMRTPPacket::Packetise (int ts)
{
	int             j;

	// determine RTP fixed and payload header sizes
	ConfigPayload (ts);

	// concatenate complete packet
#ifdef _BIG_ENDIAN
	memcpy (buffer, &hdr, szFixedHdr);	// RTP fixed header
	memcpy (buffer + szFixedHdr, &plhdr, szPayloadHdr);	// RTP payload header
#elif defined(_LITTLE_ENDIAN)
	// little endian procedures
	// for RTP fixed header
	// the first 16 bits are in correct position, following 16 bits should be reversed
	for (j = 0; j < (szFixedHdr + 3) / 4; j++)
		Mmemrse (buffer + sizeof (UINT32) * j,
				 (unsigned char *) (&hdr) + sizeof (UINT32) * j,
				 sizeof (UINT32));

	// for RTP payload header
	// the whole structure should be reversed in groups of every 32 bits
	for (j = 0; j < (szPayloadHdr + 3) / 4; j++)
		Mmemrse (buffer + szFixedHdr + sizeof (UINT32) * j,
				 (unsigned char *) (&plhdr) + sizeof (UINT32) * j,
				 sizeof (UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif

	memcpy (buffer + szFixedHdr + szPayloadHdr, Data (), DataCount ());

	buffer_count = szFixedHdr + szPayloadHdr + DataCount ();
}

// return total number of bytes for RTP payload data
int
CMRTPPacket::Segmentalise (int inbytes)
{
	int             j;

	buffer_count = inbytes;

#ifdef _BIG_ENDIAN
	memcpy (&hdr, buffer, sizeof (UINT32));	// RTP fixed header
#elif defined(_LITTLE_ENDIAN)
	// little endian procedures
	// for RTP fixed header, rearrange first 32-bit row 1st
	Mmemrse ((unsigned char *) (&hdr), buffer, sizeof (UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif

	// extract RTP fixed header size
	szFixedHdr = sizeof (MRTPHEADER) + (hdr.cc - 1) * sizeof (UINT32);

	if (szFixedHdr > buffer_count)
	{
		return 0;				// Protects against invalid cc value
	}

#ifdef _BIG_ENDIAN
	memcpy (&plhdr, buffer + szFixedHdr, sizeof (UINT32));	// RTP payload header
#elif defined(_LITTLE_ENDIAN)
	// little endian procedures
	// for RTP payload header, rearrange first 32-bit row 1st
	Mmemrse ((unsigned char *) (&plhdr), buffer + szFixedHdr,
			 sizeof (UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif

	// extract RTP H.263 payload header size
	MRTPH263PAYLOADHEADER *h263hdr = (MRTPH263PAYLOADHEADER *) & plhdr;

	if (!h263hdr->modeA.f)
	{

		szPayloadHdr = RTP_H263_MODEA_HEADER_SIZE;
	}
	else if (!h263hdr->modeB.p)
	{
		szPayloadHdr = RTP_H263_MODEB_HEADER_SIZE;
	}
	else
	{
		szPayloadHdr = RTP_H263_MODEC_HEADER_SIZE;
	}

	// determine RTP payload data size
	int             count = buffer_count - szFixedHdr - szPayloadHdr;

	if (count <= 0)
	{
	char           *l_pcPayloadName = "unlisted RTP payload type";

		for (int i = 0;
			 i <
			 (int) (sizeof (gs_RtpPayloadType) /
					sizeof (gs_RtpPayloadType[0])); i++)
		{
			if (hdr.pt == gs_RtpPayloadType[i].l_uiRtpPayloadTypeIndex)
			{
				l_pcPayloadName =
					(char *) gs_RtpPayloadType[i].l_pccRtpPayloadTypeName;
				break;
			}
		}
		count = 0;
		DataCount (count);
		return count;			//prevents corruption if Fixed Header is invalid
	}
	DataCount (count);

	// separate headers and data
#ifdef _BIG_ENDIAN
	memcpy (&hdr, pfh, szFixedHdr);
	memcpy (&plhdr, buffer + szFixedHdr, szPayloadHdr);	// RTP payload header
#elif defined(_LITTLE_ENDIAN)
	// little endian procedures
	// for RTP fixed header
	for (j = 0; j < (szFixedHdr + 3) / 4; j++)
		Mmemrse ((unsigned char *) (&hdr) + sizeof (UINT32) * j,
				 buffer + sizeof (UINT32) * j, sizeof (UINT32));
	// for RTP payload header
	// the whole structure should be reversed in groups of every 32 bits
	for (j = 0; j < (szPayloadHdr + 3) / 4; j++)
		Mmemrse ((unsigned char *) (&plhdr) + sizeof (UINT32) * j,
				 buffer + szFixedHdr + sizeof (UINT32) * j, sizeof (UINT32));
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif

	// extract RTP payload data
	memcpy (base, buffer + szFixedHdr + szPayloadHdr, DataCount ());

	return DataCount ();
}

void
CMRTPPacket::ShallowCopy (CMRTPPacket & p_Packet)
{
	// Clean up memory for our local data
	Free ();

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

void
CMRTPPacket::setRTPBuffer (MOCTET & buf)
{
	buffer = &buf;
}

const unsigned char *
CMRTPPacket::Mmemrse (unsigned char *dest, const unsigned char *src, int n)
{
	unsigned char  *d = dest;

	//_ASSERTE(src!=NULL);
	//_ASSERTE(dest!=NULL);

	if ((!src) || (!dest))
	{
		return NULL;
	}
	for (int i = n - 1; i >= 0; i--)
	{
		*dest++ = src[i];
	}
	return d;
}

// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
#if !defined (MEMCHECK)
#define new DEBUG_NEW
#endif // !MEMCHECK
#undef THIS_FILE
static char     THIS_FILE[] = __FILE__;
#endif

/* Constants for MD5Transform routine.
 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void MD5Transform PROTO_LIST ((UINT4[4], unsigned char[64]));
static void Encode PROTO_LIST ((unsigned char *, UINT4 *, unsigned int));
static void Decode PROTO_LIST ((UINT4 *, unsigned char *, unsigned int));
static void MD5_memcpy PROTO_LIST ((POINTER, POINTER, unsigned int));
static void MD5_memset PROTO_LIST ((POINTER, int, unsigned int));

static unsigned char PADDING[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
   Rotation is separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac) { \
	(a) += F ((b), (c), (d)) + (x) + (UINT4)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
	(a) += G ((b), (c), (d)) + (x) + (UINT4)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
	(a) += H ((b), (c), (d)) + (x) + (UINT4)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
	(a) += I ((b), (c), (d)) + (x) + (UINT4)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
  }

/* MD5 initialization. Begins an MD5 operation, writing a new context.
 */
void
MD5Init (MD5_CTX * context)
{
	context->count[0] = context->count[1] = 0;

	/* Load magic initialization constants.
	 */
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;
}

/* MD5 block update operation. Continues an MD5 message-digest
	 operation, processing another message block, and updating the
	 context.
 */
void
MD5Update (MD5_CTX * context, unsigned char *input, unsigned int inputLen)
/* context */
/* input block */
/* length of input block */
{
	unsigned int    i, index, partLen;

	/* Compute number of bytes mod 64 */
	index = (unsigned int) ((context->count[0] >> 3) & 0x3F);

	/* Update number of bits */
	if ((context->count[0] += ((UINT4) inputLen << 3))
		< ((UINT4) inputLen << 3))
		context->count[1]++;
	context->count[1] += ((UINT4) inputLen >> 29);

	partLen = 64 - index;

	/* Transform as many times as possible.
	 */
	if (inputLen >= partLen)
	{
		MD5_memcpy
			((POINTER) & context->buffer[index], (POINTER) input, partLen);
		MD5Transform (context->state, context->buffer);

		for (i = partLen; i + 63 < inputLen; i += 64)
			MD5Transform (context->state, &input[i]);

		index = 0;
	}
	else
		i = 0;

	/* Buffer remaining input */
	MD5_memcpy
		((POINTER) & context->buffer[index], (POINTER) & input[i],
		 inputLen - i);
}

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
	 the message digest and zeroizing the context.
 */
void
MD5Final (unsigned char digest[16], MD5_CTX * context)
{
	unsigned char   bits[8];
	unsigned int    index, padLen;

	/* Save number of bits */
	Encode (bits, context->count, 8);

	/* Pad out to 56 mod 64.
	 */
	index = (unsigned int) ((context->count[0] >> 3) & 0x3f);
	padLen = (index < 56) ? (56 - index) : (120 - index);
	MD5Update (context, PADDING, padLen);

	/* Append length (before padding) */
	MD5Update (context, bits, 8);

	/* Store state in digest */
	Encode (digest, context->state, 16);

	/* Zeroize sensitive information.
	 */
	MD5_memset ((POINTER) context, 0, sizeof (*context));
}

/* MD5 basic transformation. Transforms state based on block.
 */
static void
MD5Transform (UINT4 state[4], unsigned char block[64])
{
	UINT4           a = state[0], b = state[1], c = state[2], d =
		state[3], x[16];

	Decode (x, block, 64);

	/* Round 1 */
	FF (a, b, c, d, x[0], S11, 0xd76aa478);	/* 1 */
	FF (d, a, b, c, x[1], S12, 0xe8c7b756);	/* 2 */
	FF (c, d, a, b, x[2], S13, 0x242070db);	/* 3 */
	FF (b, c, d, a, x[3], S14, 0xc1bdceee);	/* 4 */
	FF (a, b, c, d, x[4], S11, 0xf57c0faf);	/* 5 */
	FF (d, a, b, c, x[5], S12, 0x4787c62a);	/* 6 */
	FF (c, d, a, b, x[6], S13, 0xa8304613);	/* 7 */
	FF (b, c, d, a, x[7], S14, 0xfd469501);	/* 8 */
	FF (a, b, c, d, x[8], S11, 0x698098d8);	/* 9 */
	FF (d, a, b, c, x[9], S12, 0x8b44f7af);	/* 10 */
	FF (c, d, a, b, x[10], S13, 0xffff5bb1);	/* 11 */
	FF (b, c, d, a, x[11], S14, 0x895cd7be);	/* 12 */
	FF (a, b, c, d, x[12], S11, 0x6b901122);	/* 13 */
	FF (d, a, b, c, x[13], S12, 0xfd987193);	/* 14 */
	FF (c, d, a, b, x[14], S13, 0xa679438e);	/* 15 */
	FF (b, c, d, a, x[15], S14, 0x49b40821);	/* 16 */

	/* Round 2 */
	GG (a, b, c, d, x[1], S21, 0xf61e2562);	/* 17 */
	GG (d, a, b, c, x[6], S22, 0xc040b340);	/* 18 */
	GG (c, d, a, b, x[11], S23, 0x265e5a51);	/* 19 */
	GG (b, c, d, a, x[0], S24, 0xe9b6c7aa);	/* 20 */
	GG (a, b, c, d, x[5], S21, 0xd62f105d);	/* 21 */
	GG (d, a, b, c, x[10], S22, 0x2441453);	/* 22 */
	GG (c, d, a, b, x[15], S23, 0xd8a1e681);	/* 23 */
	GG (b, c, d, a, x[4], S24, 0xe7d3fbc8);	/* 24 */
	GG (a, b, c, d, x[9], S21, 0x21e1cde6);	/* 25 */
	GG (d, a, b, c, x[14], S22, 0xc33707d6);	/* 26 */
	GG (c, d, a, b, x[3], S23, 0xf4d50d87);	/* 27 */
	GG (b, c, d, a, x[8], S24, 0x455a14ed);	/* 28 */
	GG (a, b, c, d, x[13], S21, 0xa9e3e905);	/* 29 */
	GG (d, a, b, c, x[2], S22, 0xfcefa3f8);	/* 30 */
	GG (c, d, a, b, x[7], S23, 0x676f02d9);	/* 31 */
	GG (b, c, d, a, x[12], S24, 0x8d2a4c8a);	/* 32 */

	/* Round 3 */
	HH (a, b, c, d, x[5], S31, 0xfffa3942);	/* 33 */
	HH (d, a, b, c, x[8], S32, 0x8771f681);	/* 34 */
	HH (c, d, a, b, x[11], S33, 0x6d9d6122);	/* 35 */
	HH (b, c, d, a, x[14], S34, 0xfde5380c);	/* 36 */
	HH (a, b, c, d, x[1], S31, 0xa4beea44);	/* 37 */
	HH (d, a, b, c, x[4], S32, 0x4bdecfa9);	/* 38 */
	HH (c, d, a, b, x[7], S33, 0xf6bb4b60);	/* 39 */
	HH (b, c, d, a, x[10], S34, 0xbebfbc70);	/* 40 */
	HH (a, b, c, d, x[13], S31, 0x289b7ec6);	/* 41 */
	HH (d, a, b, c, x[0], S32, 0xeaa127fa);	/* 42 */
	HH (c, d, a, b, x[3], S33, 0xd4ef3085);	/* 43 */
	HH (b, c, d, a, x[6], S34, 0x4881d05);	/* 44 */
	HH (a, b, c, d, x[9], S31, 0xd9d4d039);	/* 45 */
	HH (d, a, b, c, x[12], S32, 0xe6db99e5);	/* 46 */
	HH (c, d, a, b, x[15], S33, 0x1fa27cf8);	/* 47 */
	HH (b, c, d, a, x[2], S34, 0xc4ac5665);	/* 48 */

	/* Round 4 */
	II (a, b, c, d, x[0], S41, 0xf4292244);	/* 49 */
	II (d, a, b, c, x[7], S42, 0x432aff97);	/* 50 */
	II (c, d, a, b, x[14], S43, 0xab9423a7);	/* 51 */
	II (b, c, d, a, x[5], S44, 0xfc93a039);	/* 52 */
	II (a, b, c, d, x[12], S41, 0x655b59c3);	/* 53 */
	II (d, a, b, c, x[3], S42, 0x8f0ccc92);	/* 54 */
	II (c, d, a, b, x[10], S43, 0xffeff47d);	/* 55 */
	II (b, c, d, a, x[1], S44, 0x85845dd1);	/* 56 */
	II (a, b, c, d, x[8], S41, 0x6fa87e4f);	/* 57 */
	II (d, a, b, c, x[15], S42, 0xfe2ce6e0);	/* 58 */
	II (c, d, a, b, x[6], S43, 0xa3014314);	/* 59 */
	II (b, c, d, a, x[13], S44, 0x4e0811a1);	/* 60 */
	II (a, b, c, d, x[4], S41, 0xf7537e82);	/* 61 */
	II (d, a, b, c, x[11], S42, 0xbd3af235);	/* 62 */
	II (c, d, a, b, x[2], S43, 0x2ad7d2bb);	/* 63 */
	II (b, c, d, a, x[9], S44, 0xeb86d391);	/* 64 */

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;

	/* Zeroize sensitive information.
	 */
	MD5_memset ((POINTER) x, 0, sizeof (x));
}

/* Encodes input (UINT4) into output (unsigned char). Assumes len is
	 a multiple of 4.
 */
static void
Encode (unsigned char *output, UINT4 * input, unsigned int len)
{
	unsigned int    i, j;

	for (i = 0, j = 0; j < len; i++, j += 4)
	{
		output[j] = (unsigned char) (input[i] & 0xff);
		output[j + 1] = (unsigned char) ((input[i] >> 8) & 0xff);
		output[j + 2] = (unsigned char) ((input[i] >> 16) & 0xff);
		output[j + 3] = (unsigned char) ((input[i] >> 24) & 0xff);
	}
}

/* Decodes input (unsigned char) into output (UINT4). Assumes len is
	 a multiple of 4.
 */
static void
Decode (UINT4 * output, unsigned char *input, unsigned int len)
{
	unsigned int    i, j;

	for (i = 0, j = 0; j < len; i++, j += 4)
		output[i] = ((UINT4) input[j]) | (((UINT4) input[j + 1]) << 8) |
			(((UINT4) input[j + 2]) << 16) | (((UINT4) input[j + 3]) << 24);
}

/* Note: Replace "for loop" with standard memcpy if possible.
 */
static void
MD5_memcpy (POINTER output, POINTER input, unsigned int len)
{
	unsigned int    i;

	for (i = 0; i < len; i++)
		output[i] = input[i];
}

/* Note: Replace "for loop" with standard memset if possible.
 */
static void
MD5_memset (POINTER output, int value, unsigned int len)
{
	unsigned int    i;

	for (i = 0; i < len; i++)
		((char *) output)[i] = (char) value;
}

#if 0

//////////////////////////////////////////////////////////////////////////////////////
//
//

////////////////////////////////////////////////////////////////////////////////////////
//
//CodecCapabilities implementation
//
//
int
CodecCapabilities::setCapability (const int capability)
{
	char            mod[] = { "CodecCapabilities::setCapability" };
	if (capabilityIndex > MAX_NUM_OF_CODEC_SUPPORTED)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, INFO,
				   "Failed to setCapability total number of codec supported is exceeded");
		return -1;
	}

	dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "setting codec capability to %d", capability);

	for (int i = 0; i < capabilityIndex; i++)
	{
		if (capabilities[i] == capability)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, INFO,
					   "Codec Capability=%d,  is already set ", capability);
			return 20;
		}
	}

	capabilities[capabilityIndex] = capability;
	capabilityIndex++;
	return 0;
}

void
CodecCapabilities::printCapabilities ()
{
	for (int i = 0; i < capabilityIndex; i++)
	{
		cout << "CodecCapability[" << i << "]=" << capabilities[i] << endl;
	}
}

int
CodecCapabilities::removeCapability (const int capability)
{
	char            mod[] = { "CodecCapabilities::removeCapability" };
	for (int i = 0; i < capabilityIndex; i++)
	{
		if (capabilities[i] == capability)
		{
			capabilities[i] = 0;
			capabilityIndex--;
			return 0;
		}
	}
	dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "Failed to remove codec capability=%d", capability);
	return -1;
}

int
CodecCapabilities::getCapability (const int capability)
{
	char            mod[] = { "CodecCapabilities::getCapability" };
	for (int i = 0; i < capabilityIndex; i++)
	{
		if (capabilities[i] == capability)
		{
			return 0;
		}
	}
	dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "codec capability=%d is not set", capability);
	return -1;
}

//

#endif

/* DTMF Tone Detection */

/*
 * calculate the power of each tone according
 * to a modified goertzel algorithm described in
 *  _digital signal processing applications using the
 *  ADSP-2100 family_ by Analog Devices
 *
 * input is 'data',  N sample values
 *
 * ouput is 'power', NUMTONES values
 *  corresponding to the power of each tone 
 */
#ifdef UNSIGNED
int
calc_power (unsigned char *data, float *power)
#else
int
calc_power (char *data, float *power)
#endif
{
	float           u0[NUMTONES], u1[NUMTONES], t, in;
	int             i, j;

	for (j = 0; j < NUMTONES; j++)
	{
		u0[j] = 0.0;
		u1[j] = 0.0;
	}

	for (i = 0; i < N; i++)
	{							/* feedback */
#ifdef UNSIGNED
		in = ((int) data[i] - 128) / 128.0;
#else
		in = data[i] / 128.0;
#endif
		for (j = 0; j < NUMTONES; j++)
		{
			t = u0[j];
			u0[j] = in + coef[j] * u0[j] - u1[j];
			u1[j] = t;
		}
	}

	for (j = 0; j < NUMTONES; j++)	/* feedforward */
	{
		power[j] = u0[j] * u0[j] + u1[j] * u1[j] - coef[j] * u0[j] * u1[j];
	}

	return (0);

}								/*END:int calc_power */

/*
 * detect which signals are present.
 *
 * return values defined in the include file
 * note: DTMF 3 and MF 7 conflict.  To resolve
 * this the program only reports MF 7 between
 * a KP and an ST, otherwise DTMF 3 is returned
 */
int
decode (char *data, struct dtmf_data_holder *zDtmfData)
{
	float           power[NUMTONES], thresh, maxpower, minpower;
	int             on[NUMTONES], on_count;
	int             bcount, rcount, ccount;
	int             row, col, b1, b2, i;
	int             r[4], c[4], b[8];

	//static int MFmode = 0;

	calc_power (data, power);

	for (i = 0, maxpower = 0.0, minpower = 99999.9; i < NUMTONES; i++)
	{
		if (power[i] > maxpower)
		{
			maxpower = power[i];
		}

		if (power[i] < minpower)
		{
			minpower = power[i];
		}
	}

/*
for(i=0;i<NUMTONES;i++) 
  printf("%f, ",power[i]);
printf("\n");
*/

	if (maxpower < THRESH)		/* silence? */
	{
		return (DSIL);
	}

	thresh = RANGE * maxpower;	/* allowable range of powers */

	for (i = 0, on_count = 0; i < NUMTONES; i++)
	{
		if (power[i] > thresh)
		{
			on[i] = 1;
			on_count++;
		}
		else
			on[i] = 0;
	}

/*
printf("%4d: ",on_count);
for(i=0;i<NUMTONES;i++)
  putchar('0' + on[i]);
printf("\n");
*/

	if (on_count == 1)
	{
		if (on[B7])
			return (D24);
		if (on[B8])
			return (D26);
		return (-1);
	}

	if (on_count == 2)
	{
		if (on[X1] && on[X2])
			return (DDT);
		if (on[X2] && on[X3])
			return (DRING);
		if (on[X3] && on[X4])
			return (DBUSY);

		b[0] = on[B1];
		b[1] = on[B2];
		b[2] = on[B3];
		b[3] = on[B4];
		b[4] = on[B5];
		b[5] = on[B6];
		b[6] = on[B7];
		b[7] = on[B8];
		c[0] = on[C1];
		c[1] = on[C2];
		c[2] = on[C3];
		c[3] = on[C4];
		r[0] = on[R1];
		r[1] = on[R2];
		r[2] = on[R3];
		r[3] = on[R4];

		for (i = 0, bcount = 0; i < 8; i++)
		{
			if (b[i])
			{
				bcount++;
				b2 = b1;
				b1 = i;
			}
		}

		for (i = 0, rcount = 0; i < 4; i++)
		{
			if (r[i])
			{
				rcount++;
				row = i;
			}
		}

		for (i = 0, ccount = 0; i < 4; i++)
		{
			if (c[i])
			{
				ccount++;
				col = i;
			}
		}

		if (rcount == 1 && ccount == 1)
		{						/* DTMF */
			if (col == 3)		/* A,B,C,D */
				return (DA + row);
			else
			{
				if (row == 3 && col == 0)
					return (DSTAR);
				if (row == 3 && col == 2)
					return (DPND);
				if (row == 3)
					return (D0);
				if (row == 0 && col == 2)
				{				/* DTMF 3 conflicts with MF 7 */
					if (!zDtmfData->MFmode)
						return (D3);
				}
				else
					return (D1 + col + row * 3);
			}
		}

		if (bcount == 2)
		{						/* MF */
			/* b1 has upper number, b2 has lower */
			switch (b1)
			{
			case 7:
				return ((b2 == 6) ? D2426 : -1);
			case 6:
				return (-1);
			case 5:
				if (b2 == 2 || b2 == 3)	/* KP */
					zDtmfData->MFmode = 1;
				if (b2 == 4)	/* ST */
					zDtmfData->MFmode = 0;
				return (DC11 + b2);
				/* MF 7 conflicts with DTMF 3, but if we made it
				 * here then DTMF 3 was already tested for 
				 */
			case 4:
				return ((b2 == 3) ? D0 : D7 + b2);
			case 3:
				return (D4 + b2);
			case 2:
				return (D2 + b2);
			case 1:
				return (D1);
			}
		}
		return (-1);
	}

	if (on_count == 0)
		return (DSIL);

	return (-1);

}								/*END:int decode */

/*DDN: 20091005*/
int
copy_frame (char *inbuf, char *buf)
{
	memcpy (buf, inbuf, N);

	return (1);

}								/*END: copy_frame */

/*
 *	copy frames, output the decoded
 *	results
*	DDN: 20091005	Code modified to fit in media manager
 */

void
dtmf_tone_to_ascii (int zCall, char *inbuf,
					struct dtmf_data_holder *zDtmfData, int processType)
{
	return;

#if 0
	static char     mod[] = "dtmf_tone_to_ascii";

#if 1

	/*DDN: It means we are going to receive RFC 2833 packets */
	if (gCall[zCall].telephonePayloadType > -1 && processType == PROCESS_DTMF)
	{
		return;
	}
#endif

	/*DDN: Only ULaw is supported. */
	if (gCall[zCall].codecType != 0)
	{
		return;
	}

	if (copy_frame (inbuf, zDtmfData->frame))	/*Always returns 1 */
	{
		zDtmfData->x = decode (zDtmfData->frame, zDtmfData);

		if (zDtmfData->x >= 0)
		{
			if (zDtmfData->x == DSIL)
			{

#if 0
	char            yStrTimeInfo[128];

				sprintf (yStrTimeInfo,
						 "lead:%d, trail:%d last_non_sil:%d cur_time:%d",
						 zDtmfData->lead_silence, zDtmfData->trail_silence,
						 zDtmfData->last_non_silent_time, yTmpApproxTime);
#endif

				zDtmfData->silence_time +=
					(zDtmfData->silence_time >= 0) ? 1 : 0;

#if 0
				if (zDtmfData->lookForLeadSilence &&
					(zDtmfData->last_non_silent_time +
					 zDtmfData->lead_silence) < yTmpApproxTime)
				{

					__DDNDEBUG__ (DEBUG_MODULE_RTP, "lead_silence_triggered",
								  "rtp_session", zCall);

					zDtmfData->lead_silence_triggered = 1;
				}
				else if (zDtmfData->lookForTrailSilence &&
						 (zDtmfData->last_non_silent_time +
						  zDtmfData->trail_silence) < yTmpApproxTime)
				{

					__DDNDEBUG__ (DEBUG_MODULE_RTP, "trail_silence_triggered",
								  "rtp_session", zCall);

					zDtmfData->trail_silence_triggered = 1;
				}
#endif
			}
			else
			{
				zDtmfData->silence_time = 0;
				zDtmfData->last_non_silent_time = yTmpApproxTime;

#if 0
				if (zDtmfData->lookForLeadSilence)
				{

					__DDNDEBUG__ (DEBUG_MODULE_RTP,
								  "Setting lookForTrailSilence = 1",
								  "rtp_session", zCall);

					/*DDN: No need to process leadSilence after first flush */
					zDtmfData->lead_silence_processed = 1;
					zDtmfData->lookForTrailSilence = 1;	/*Toggle */
					zDtmfData->lookForLeadSilence = 0;
					/*END: DDN: No need to process leadSilence after first flush */
				}
#endif
			}

			if (zDtmfData->silence_time == FLUSH_TIME)
			{
				//fputs("\nFLUSH_TIME\n",stdout);
				zDtmfData->silence_time = -1;	/* stop counting */
				//zDtmfData->x  = DSIL;
				//zDtmfData->last = DSIL;
			}

			if ((processType == PROCESS_DTMF
				 || processType == PROCESS_DTMF_AND_RECORD)
				&& zDtmfData->x != DSIL && zDtmfData->x != zDtmfData->last
				&& (zDtmfData->last == DSIL || zDtmfData->last == D24
					|| zDtmfData->last == D26 || zDtmfData->last == D2426
					|| zDtmfData->last == DDT || zDtmfData->last == DBUSY
					|| zDtmfData->last == DRING))
			{
	int             dtmfValue = zDtmfData->x;

				if (dtmfValue >= D0 && dtmfValue <= DD)
				{
	struct timeb    currentDtmfTime;
	long            current = 0;
	long            last = 0;

					ftime (&currentDtmfTime);

					current =
						(1000 * currentDtmfTime.time) +
						currentDtmfTime.millitm;

					last =
						(1000 * zDtmfData->lastTime.time) +
						zDtmfData->lastTime.millitm;

					if ((current - last < 100)
						&& (zDtmfData->x == gCall[zCall].lastDtmf))
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Ignoring duplicate DTMF (%d:%s).",
								   zDtmfData->x, dtran[zDtmfData->x]);
					}
					else
					{
						ftime (&zDtmfData->lastTime);

						sprintf (zDtmfData->result, dtran[zDtmfData->x],
								 strlen (dtran[zDtmfData->x]));

						if (gCall[zCall].dtmfAvailable)
						{
	int             i = gCall[zCall].dtmfCacheCount;

							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
									   TEL_BASE, INFO,
									   "Found unprocessed DTMF in cache.");

							if (i < 0)
								i = 0;

							gCall[zCall].dtmfInCache[i] =
								gCall[zCall].lastDtmf;

							gCall[zCall].dtmfCacheCount++;
						}

						gCall[zCall].dtmfAvailable = 1;
						gCall[zCall].lastDtmf = zDtmfData->x;

						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, INFO,
								   "Detected inband DTMF (%d:%s) last(%d:%s) range(%lf).",
								   zDtmfData->x, zDtmfData->result,
								   zDtmfData->last, dtran[zDtmfData->last],
								   RANGE);

						if (gCall[zCall].callState == CALL_STATE_CALL_BRIDGED
							&& gCall[zCall].callSubState ==						// MR 4854
							CALL_STATE_CALL_MEDIACONNECTED)
						{
	char            yStrTmpBuffer[2];

							sprintf (yStrTmpBuffer, "%c",
									 dtmf_tab[zDtmfData->x]);

							saveBridgeCallPacket (zCall, yStrTmpBuffer, dtmf_tab[zDtmfData->x], 0);	/* -1 for NO DTMF */
						}

					}
				}
				else
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Detected remaining Tone for (%d:%s).",
							   zDtmfData->x, dtran[zDtmfData->x]);
				}

			}

			zDtmfData->last = zDtmfData->x;

		}						/*if x > 0 */

	}							/*if copy_frame returns 1 */

#endif

}								/*END: dtmf_tone_to_ascii */

#ifdef VOICE_BIOMETRICS
/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static void readAVBCfg() 
{
	char		mod[]="readAVBCfg";
	char		yConfigFile[256];
	char		*ispbase;
	FILE		*pFp;
	int			i;
	int			rc;
	char		yTmpBuf[128];
	char		yServiceStr[8];
	char		errMsg[256];
	float		f;

	// set defaults
	gAvb_SpeechSize = 4000;		 // 160 * 25
	gAvb_SpeechSleep = 20;

	/* Get config file name */
	if((ispbase = (char *)getenv("ISPBASE")) == NULL)
	{
		dynVarLog (__LINE__, 0, mod, REPORT_DETAIL,
					   TEL_CONFIG_VALUE_NOT_FOUND, WARN,
			"Env. var. ISPBASE is not set.  "
			"Setting defaults of Voice Biometrics size/sleeptime to %d / %d.",
			gAvb_SpeechSize, gAvb_SpeechSleep);
		return;
	}
	sprintf(yConfigFile, "%s/Telecom/Tables/arcavb.cfg", ispbase);

	rc = get_name_value ("", "Continuous_Speech_Size", "", yTmpBuf,
			      sizeof (yTmpBuf), yConfigFile, errMsg);
	i = atoi(yTmpBuf);
	if ( i <= 0 )
	{
		dynVarLog (__LINE__, 0, mod, REPORT_DETAIL,
					   TEL_CONFIG_VALUE_NOT_FOUND, WARN,
			"Warning: Invalid value (%d) for Continuous_Speech_Size.  "
			"Setting to default of %d.",
			i, gAvb_SpeechSize);
	}
	if ( i % 160 != 0 )
	{
		dynVarLog (__LINE__, 0, mod, REPORT_DETAIL,
					   TEL_CONFIG_VALUE_NOT_FOUND, WARN,
			"Warning: Invalid value (%d) for Continuous_Speech_Size.  "
			"Must be divisible by 160.  "
			"Setting to default of %d.",
			i, gAvb_SpeechSize);
	}
	gAvb_SpeechSize = i;

	rc = get_name_value ("", "Continuous_Speech_Sleep", "", yTmpBuf,
			      sizeof (yTmpBuf), yConfigFile, errMsg);
	gAvb_SpeechSleep = atoi(yTmpBuf);

	if ( i <= 0 )
	{
		dynVarLog (__LINE__, 0, mod, REPORT_DETAIL,
					   TEL_CONFIG_VALUE_NOT_FOUND, WARN,
			"Warning: Invalid value (%d) for Continuous_Speech_Sleep.  "
			"Setting to default of %d.",
			yTmpBuf, gAvb_SpeechSleep);
	}

	rc = get_name_value ("", "Base_Directory", "", yTmpBuf,
			      sizeof (yTmpBuf), yConfigFile, errMsg);
	if ( rc < 0 )
	{
		dynVarLog (__LINE__, 0, mod, REPORT_DETAIL,
					   TEL_CONFIG_VALUE_NOT_FOUND, WARN,
			"Warning: Unable to get Base_Directory from (%s). "
			"Setting to default of /tmp.", yConfigFile);
		sprintf(gAvb_PCM_PhraseDir, "%s", "/tmp");
	}
	else
	{
		sprintf(gAvb_PCM_PhraseDir, "%s/speech", yTmpBuf);
	}

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"Voice Biometrics: gAvb_SpeechSize=%d; gAvb_SpeechSleep=%d speechDir=(%s)",
			gAvb_SpeechSize, gAvb_SpeechSleep, gAvb_PCM_PhraseDir);

	return;
} // END: readAVBCfg() 
#endif  // END: VOICE_BIOMETRICS
