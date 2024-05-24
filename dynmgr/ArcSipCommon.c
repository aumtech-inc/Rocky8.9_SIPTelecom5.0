/*------------------------------------------------------------------------------
Program Name:	ArcSipCallMgr/ArcIPDynMgr
File Name:		ArcSipCommon.h
Purpose:  		Common #includes for SIP/2.0 Call Manager and Media manager
Author:			Aumtech, inc.
Update: 07/05/2011 djb MR 3371.  Updated logging messages.
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
Update: 07/07/2004 DDN Created the file
Update: 10/12/2005 Added the variable gSipRedirection for Redirection Manager 
Update: 09/12/2007 djb Added mrcpTTS items.
Update: 09/24/2014 djb MR 3839 - removed references to MRCPv1
Update: 03/30/2016 djb MR 4558 - removed gWriteBLegCDR
------------------------------------------------------------------------------*/

/*System header files*/

#define IMPORT_GLOBAL_EXTERNS 
#include "ArcSipCommon.h"
#include "dynVarLog.h"
//#include "arcXXX.h"
#include "ArcSipCodecs.h"


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/errno.h>
#include <sys/time.h>
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

/*END: System header files*/

/*Third party header files*/

#if 0
#include <eXosip2/eXosip.h>
#include <eXosip2.h>


#include <ortp/ortp.h>
#include <ortp/rtpsession.h>
#include <ortp/telephonyevents.h>
#include <ortp/payloadtype.h>
#include <glib.h>
#endif
//#ifndef ARC_SIP_REDIRECTOR
//#include "arc_video_packet.hpp"
//#endif


#include "ArcSipCommon.h"
#define IMPORT_GLOBAL_EXTERNS 


/*END: Third party header files*/

/*ARC header files */

#include <Telecom.h>
#include <AppMessages.h>
#include <BNS.h>
#include <ISPSYS.h>
#include <shm_def.h>
#include <log_defs.h>

/*END: ARC header files*/


extern pid_t gMediaMgrPid;
extern struct MsgToDM gExitAllStruct;
extern void *gShmBase;
extern key_t gShmKey;
extern int gShmId;

int	gShutDownMaxTime;


#if 0

pid_t	gParentPid;
int gResId = -1;
char gAuthUser[256];
char gAuthPassword[256];
char gAuthId[256];
char gAuthRealm[256];
char gFromUrl[256];

int	gMaxCallDuration = -1;

int gOutboundProxyRequired = 0;		///This variable is set to 1 if Call Manager will be using a proxy.
int gVideoSupported = 1;	///This variable is SIP_VideoSupport from .TEL.cfg
int gSipOutputDtmfMethod;	///This variable tells us what form of output dtmf to send.
int gRfc2833Payload = 96;	///This variable tells us what payload type we will be using for receiving dtmf.
int gSipPort = 5060;		///This variable is the port that eXosip will listen on and is set to 5060 by default.
int gSipRedirection = 0; 	///This variable is set to 1 if there are multiple Dynamic Managers running.
int gVfuDuration = 0; 	///This variable is the duration in milliseconds after which VFU message will be sent repeatedly.
int gBeepSleepDuration= 0; 	///This variable is the duration for which silence will be recorded while playing the beep.
char gOutboundProxy[256];           ///This variable is the proxy that we will be using.
char gDefaultGateway[256];	///This variable tells us what the default gateway is.
char gPreferredCodec[256];	///This variable is set to the codec that we will be using for this IVR.
char gAMRIndex[256];		///This variable is set to the AMR Index mode that we will be using for this IVR.
int	gMaxPTime;				///This variable is set to the AMR Maxptime that we will be using for this IVR.
char g729Annexb[32];		///This variable is set to the G729 annexb support that we will be using for this IVR.
char gPreferredVideoCodec[256];	///This variable is set to the codec that we will be using for this IVR.
char *ispbase;				///This variable is set to the isp base directory which is usually /home/arc/.ISP .
int maxNumberOfCalls;    	///This variable is the maximum number of calls that Call Manager can receive.
char gHostName[256];		///This variable is the host name of the box Dynamic Manager is running on.
char gHostIp[20];
char	gCacheDirectory[256];

int LOCAL_STARTING_RTP_PORT = 10500;	///This variable is the starting port that we will use for assigning incoming rtp packets.

char gToRespFifo[256];		///This variable gives us the name of the FIFO used to communicate with Responsibility.
char gFifoDir[256];			///This variable lets us know which directory to create and find FIFO's.
char gGlobalCfg[256];		///This variable contains the path where we can find the .Global.cfg file.
char gIpCfg[256];			///This variable contains the path where we can find the .TEL.cfg file.
char gIspBase[256];			///This variable contains the path to the ISP base directory.
char gProgramName[64];	
int gMrcpTTS_Enabled;

char g729b_silenceBuffer[2] = {0x34, 0x40};

int gWritePCForEveryCall = 0;			///This variable is set to 1 if we need to write a Present Call Record for each call.

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


/*Answering Machine Detection variable*/
int gDetectAnsweringMachine = false;

//end


#endif 

#define MAX_DWORD (0xFFFFFFFF)
#define VIDEO_BUFFER_SIZE             5000
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

#define LOCAL_STARTING_RTP_PORT_DEFAULT 	10500
#define LOCAL_STARTING_VIDEO_PORT 		20000

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
#define CLEAR_VIDEO_LIST 999



#if 0
class CodecCapabilities
{

public:
        CodecCapabilities()
        {
                capabilityIndex = 0;
        }
        ~CodecCapabilities(){}
        int initialize()
        {
                capabilityIndex = 0;
                for(int i=0; i<MAX_NUM_OF_CODEC_SUPPORTED; i++)
                {
                        capabilities[i] = 0;
                }
                return 0;
        }
        int setCapability(const int);
        int getCapability(const int);
        int removeCapability(const int);
        int resetCapability()
        {
                return initialize();
        }
        void printCapabilities();

private:
        int capabilities[MAX_NUM_OF_CODEC_SUPPORTED];
        int capabilityIndex;

};

#endif



#define lastMsgToDM msgToDM


struct lookup_t {
    char *str;
    int bitrate;
    int  type;
};


static struct lookup_t arc_codec_lookup[] = {
    {"PCMU", 8000, ARC_AUDIO_CODEC_G711U_8K},
    {"PCMA", 8000, ARC_AUDIO_CODEC_G711A_8K},
    //
    {"G729", 8000, ARC_AUDIO_CODEC_G729_8K},
    //
    {"SILK", 8000, ARC_AUDIO_CODEC_SILK_8K},
    {"SILK", 12000, ARC_AUDIO_CODEC_SILK_12K},
    {"SILK", 16000, ARC_AUDIO_CODEC_SILK_16K},
    {"SILK", 24000, ARC_AUDIO_CODEC_SILK_24K},
    {NULL, 0, 0}
};

int
arc_audio_codec_lookup(const char *a_line, int *rtpmap, int *bitrate){

  int rc = -1;
  int rate = -1;
  int len;
  char buff[24];
  char codec[24];
  int i;

  char *ptr;

  // a=rtpmap:102 SILK/12000

  ptr = strstr((char *)a_line, ":");

  if(ptr){
    ptr++;
    len = strcspn(ptr, " \t\n");
    if(len > 0 && len < sizeof(buff)){
      snprintf(buff, len + 1, "%s", ptr);
//      fprintf(stderr, " %s() a line=%s rtpmap=%d\n", __func__, a_line, atoi(buff));
      ptr += len + 1;
    }
  }

  if(ptr){
      len = strcspn(ptr, "/");
      if(len > 0 && len < sizeof(buff)){
         snprintf(buff, len + 1, "%s", ptr);
 //        fprintf(stderr, " %s() a line=%s codec=\"%s\"\n", __func__, a_line, buff);
         snprintf(codec, len + 1, "%s", ptr);
      }
      ptr += len  + 1;
  }

  if(ptr){
     len = strcspn(ptr, "\r\n ");
     if(len > 0 && len < sizeof(buff)){
         snprintf(buff, len + 1, "%s", ptr);
  //       fprintf(stderr, " %s() a line=%s bitrate=%d\n", __func__, a_line, atoi(buff));
         rate = atoi(buff);
     }
  }

  for(i = 0; arc_codec_lookup[i].str != NULL; i++){
      if((strcasecmp(arc_codec_lookup[i].str, codec) == 0) && (rate == arc_codec_lookup[i].bitrate)){
         rc = i;
         break;
      }
  }

  return rc;
}


int arc_fax_clone_globals_to_gCall(int zCall)
{

	int			i;
	int			 rc = 0;
	static char	mod[]="arc_fax_clone_globals_to_gCall";

   struct var_type_t {
     char *name;
     int type;
     void *src;
     void *dest;
     size_t dest_size;
   };

   enum var_type_e {
      GLOBAL_NONE = 0,
      GLOBAL_STRING, 
      GLOBAL_INT
   };

   if(zCall < 0 || zCall >= MAX_PORTS){
      return -1;
   }

   struct var_type_t name_value[] = {
    {"gFaxDebug", GLOBAL_INT, (void *)&gFaxDebug, (void *)&gCall[zCall].GV_FaxDebug, sizeof(gCall[zCall].GV_FaxDebug)},
    {"gFaxStationId", GLOBAL_STRING, (void *)gT30FaxStationId, (void *)gCall[zCall].GV_T30FaxStationId, sizeof(gCall[zCall].GV_T30FaxStationId)},
    {"gFaxHeaderInfo", GLOBAL_STRING, (void *)gT30HeaderInfo, (void *)gCall[zCall].GV_T30HeaderInfo, sizeof(gCall[zCall].GV_T30HeaderInfo)},
    {"gT30ErrorCorrection", GLOBAL_INT, (void *)&gT30ErrorCorrection, (void *)&gCall[zCall].GV_T30ErrorCorrection, sizeof(gCall[zCall].GV_T30ErrorCorrection)},
    {"gT38Enabled", GLOBAL_INT, (void *)&gT38Enabled, (void *)&gCall[zCall].GV_T38Enabled, sizeof(gCall[zCall].GV_T38Enabled)},
    {"gT38Transport", GLOBAL_INT, (void *)&gT38Transport, (void *)&gCall[zCall].GV_T38Transport, sizeof(gCall[zCall].GV_T38Transport)},
    {"gT38ErrorCorrectionMode", GLOBAL_INT, (void *)&gT38ErrorCorrectionMode, (void *)&gCall[zCall].GV_T38ErrorCorrectionMode, sizeof(gCall[zCall].GV_T38ErrorCorrectionMode)},
    {"gT38ErrorCorrectionSpan", GLOBAL_INT, (void *)&gT38ErrorCorrectionSpan, (void *)&gCall[zCall].GV_T38ErrorCorrectionSpan, sizeof(gCall[zCall].GV_T38ErrorCorrectionSpan)},
    {"gT38ErrorCorrectionEntries", GLOBAL_INT, (void *)&gT38ErrorCorrectionEntries, (void *)&gCall[zCall].GV_T38ErrorCorrectionEntries, sizeof(gCall[zCall].GV_T38ErrorCorrectionEntries)},
    {"gT38MaxBitRate", GLOBAL_INT, (void *)&gT38MaxBitRate, (void *)&gCall[zCall].GV_T38MaxBitRate, sizeof(gCall[zCall].GV_T38MaxBitRate)},
    {"gT38FaxFillBitRemoval", GLOBAL_INT, (void *)&gT38FaxFillBitRemoval, (void *)&gCall[zCall].GV_T38FaxFillBitRemoval, sizeof(gCall[zCall].GV_T38FaxFillBitRemoval)},
    {"gT38FaxVersion", GLOBAL_INT, (void *)&gT38FaxVersion, (void *)&gCall[zCall].GV_T38FaxVersion, sizeof(gCall[zCall].GV_T38FaxVersion)},
    {"gT38FaxTranscodingMMR", GLOBAL_INT, (void *)&gT38FaxTranscodingMMR, (void *)&gCall[zCall].GV_T38FaxTranscodingMMR, sizeof(gCall[zCall].GV_T38FaxTranscodingMMR)},
    {"gT38FaxTranscodingJBIG", GLOBAL_INT, (void *)&gT38FaxTranscodingJBIG, (void *)&gCall[zCall].GV_T38FaxTranscodingJBIG, sizeof(gCall[zCall].GV_T38FaxTranscodingJBIG)},
    {"gT38FaxRateManagement", GLOBAL_INT, (void *)&gT38FaxRateManagement, (void *)&gCall[zCall].GV_T38FaxRateManagement, sizeof(gCall[zCall].GV_T38FaxRateManagement)},
    {"gT38FaxMaxBuffer", GLOBAL_INT, (void *)&gT38FaxMaxBuffer, (void *)&gCall[zCall].GV_T38FaxMaxBuffer, sizeof(gCall[zCall].GV_T38FaxMaxBuffer)},
    {"gT38FaxMaxDataGram", GLOBAL_INT, (void *)&gT38FaxMaxDataGram, (void *)&gCall[zCall].GV_T38FaxMaxDataGram, sizeof(gCall[zCall].GV_T38FaxMaxDataGram)},
    {"gUdptlStartPort", GLOBAL_INT, (void *)&gUdptlStartPort, (void *)&gCall[zCall].GV_UdptlStartPort, sizeof(gCall[zCall].GV_UdptlStartPort)},
    {"gUdptlEndPort", GLOBAL_INT, (void *)&gUdptlEndPort, (void *)&gCall[zCall].GV_UdptlEndPort, sizeof(gCall[zCall].GV_UdptlEndPort)},
    {NULL, GLOBAL_NONE, NULL, NULL, 0}
   };

	for(i = 0; name_value[i].name; i++)
	{
		// add some checks here 
//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
//				" cloning %s type=%d dest=%p src=%p zCall=%d into zCall for fax",
//				name_value[i].name, name_value[i].type, name_value[i].dest,
//				name_value[i].src, zCall);
		switch(name_value[i].type)
		{
			case GLOBAL_STRING:
				snprintf((char *)name_value[i].dest, name_value[i].dest_size, "%s", (char *)name_value[i].src);
//				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
//						" type=string src=%s dst=%s",
//						(char *)name_value[i].src, (char *)name_value[i].dest);
				break;
			case GLOBAL_INT:
				memcpy(name_value[i].dest, name_value[i].src, name_value[i].dest_size);
//				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
//					" type=int src=%d dst=%d",
//					*(int *)name_value[i].src, *(int *)name_value[i].dest);
				break;
		}
	}
	return(0);
}


#define lastMsgToDM msgToDM


///This is the function Call Manager uses to communicate with Media Manager.
int notifyMediaMgr (int zLine, int zCall, struct MsgToDM *zpMsgToDM, char *mod)
{
	int yRc;
	int yIndex;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
			"[%d] writing op:%d to mm shared mem.", zLine, zpMsgToDM->opcode);

	pthread_mutex_lock (&(gNotifyMediaMgrLock));

	zpMsgToDM->appCallNum = zCall;

	gShmWriteCount++;

	memcpy ((struct MsgToDM *)
		&(gEncodedShmBase->msgCounter[gLastShmUsed].msgToDM), zpMsgToDM,
		sizeof (struct MsgToDM));

	gEncodedShmBase->msgCounter[gLastShmUsed].count = gShmWriteCount;

	gLastShmUsed++;

	if (gLastShmUsed >= (MAX_PORTS * 4))
	{
		gLastShmUsed = 0;
	}

	pthread_mutex_unlock (&(gNotifyMediaMgrLock));

	return (0);

}

int removeSharedMem (int zCall)
{
	char mod[] = "removeSharedMem";
	int yRc = -1;

	/*
	 * set shared memory key and queue 
	 */

	//gShmKey = SHMKEY_SIP;
	gShmKey = SHMKEY_SIP + ((key_t)gDynMgrId);

	/*
	 * remove shared memory  -  get shared memory segments id 
	 */

	gShmId = shmget (gShmKey, SHMSIZE_SIP, PERMS);

	yRc = shmctl (gShmId, IPC_RMID, 0);

	if ((yRc < 0) && (errno != EINVAL))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR, 
			"Failed to remove shm (key=0x%x, id=%ld).  "
			"[%d, %s]  Verify; may have to be removed manually using the ipcs/ipcrm command.",
			gShmKey, gShmId, errno, strerror(errno));

		return (ISP_FAIL);
	}

	return (ISP_SUCCESS);

}

int 
arc_exit (int zCode, char *zMessage)
{
	char yMessage[256];
	char mod[] = "arc_exit";

	sprintf (yMessage, "Exiting %s with %d. %s", gProgramName, zCode,
		 zMessage);

	dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_CALLMGR_SHUTDOWN, INFO, yMessage);
	notifyMediaMgr (__LINE__, 0, (struct MsgToDM *) &gExitAllStruct, mod);

	sleep (EXIT_SLEEP);

	if (arc_kill (gMediaMgrPid, 0) == -1)
	{
		if (errno == ESRCH)
		{
			gMediaMgrPid = -1;
		}
	}

	if (gMediaMgrPid != -1)
	{
		arc_kill (gMediaMgrPid, 15);

		sleep(2);

		arc_kill (gMediaMgrPid, 9);
	}

	/*
	 * 2: Shm Cleanup
	 */

	if (gShmBase)
	{
		shmdt (gShmBase);
		removeSharedMem (-1);
	}

	exit (zCode);

}


int arc_exit_new (int zLine, char *zFile, int zCode,
			int zMsgMode, int zMsgId, int zMsgType, char *zMessage)
{
	char yMessage[256];
	char mod[] = "arc_exit";

	sprintf (yMessage, "[%s, %d] Exiting %s with %d. %s", 
		zFile, zLine, gProgramName, zCode, zMessage);

	dynVarLog (__LINE__, -1, mod, zMsgMode, zMsgId, zMsgType, yMessage);

	notifyMediaMgr (__LINE__, 0, (struct MsgToDM *) &gExitAllStruct, mod);

	sleep (EXIT_SLEEP);

	if (arc_kill (gMediaMgrPid, 0) == -1)
	{
		if (errno == ESRCH)
		{
			gMediaMgrPid = -1;
		}
	}

	if (gMediaMgrPid != -1)
	{
		arc_kill (gMediaMgrPid, 15);

		sleep(2);

		arc_kill (gMediaMgrPid, 9);
	}

	/*
	 * 2: Shm Cleanup
	 */

	if (gShmBase)
	{
		shmdt (gShmBase);
		removeSharedMem (-1);
	}

	exit (zCode);

}

int arc_kill(int zpid, int zsiginf)
{
    char mod[] = "arc_kill";

    if(zpid < 0)
    {
        return 0;
    }

    dynVarLog( __LINE__, -1, mod, REPORT_DETAIL, TEL_KILL_PROCESS, WARN,
            "Killing pid=%d from ArcSipCallMgr with %d", zpid, zsiginf);
    return kill(zpid, zsiginf);
}


int
isModuleEnabled (int zModule)
{
	return (gDebugModules[zModule].enabled);

}/*END: int isModuleEnabled */

int getBridgeCallPacket(int zCall, char*zBuffer, int *bufferLen)
{
	int lastReadPointer = gCall[zCall].lastReadPointer;
	int yRc = 0;
	*bufferLen = 0;

	if(zCall < 0)
	{
		return (-1);
	}

	if(! gCall[zCall].bridgeCallData[lastReadPointer].filled)
	{
		return(-1);
	}

	if(gCall[zCall].bridgeCallData[lastReadPointer].dtmf == 1)
	{
		sprintf(zBuffer, "%c", gCall[zCall].bridgeCallData[lastReadPointer].data[0]);
		yRc = 1;
		*bufferLen = 1;
	}
	else
	{
		if(gCall[zCall].bridgeCallData[lastReadPointer].length == 1)
		{
			yRc = -1;	
		}
		else
		{
			memcpy(zBuffer, 
				gCall[zCall].bridgeCallData[lastReadPointer].data, 
				gCall[zCall].bridgeCallData[lastReadPointer].length);
			*bufferLen = gCall[zCall].bridgeCallData[lastReadPointer].length;
			yRc = 0;
#if 0
        FILE *fp;
        char filename[64] = "";
        sprintf(filename, "/tmp/get_bridgeData_%d",zCall);
        fp = fopen(filename, "a+");
        if(fp == NULL)
        {
            return -1;
        }
        fwrite(gCall[zCall].bridgeCallData[lastReadPointer].data, 1, 
			gCall[zCall].bridgeCallData[lastReadPointer].length, fp);
        fclose(fp);
#endif
		}
	}

	gCall[zCall].bridgeCallData[lastReadPointer].filled = 0;
	gCall[zCall].bridgeCallData[lastReadPointer].dtmf   = 0;

	gCall[zCall].lastReadPointer++;

	if(gCall[zCall].lastReadPointer >= BRIDGE_DATA_ARRAY_SIZE)
	{
		gCall[zCall].lastReadPointer = 0;
	}

	return (yRc);

}/*END: int getBridgeCallPacket*/


int saveBridgeCallPacket(int zCall, char*zBuffer, int zDtmfValue, int bufSize)
{

	int lastWritePointer = gCall[zCall].lastWritePointer;

	if(zDtmfValue > -1)
	{
		sprintf(gCall[zCall].bridgeCallData[lastWritePointer].data, "%c", zBuffer[0]);
		gCall[zCall].bridgeCallData[lastWritePointer].length = 1;
		gCall[zCall].bridgeCallData[lastWritePointer].dtmf = 1;

	}
	else
	{
		memcpy( gCall[zCall].bridgeCallData[lastWritePointer].data, zBuffer, bufSize);	
		gCall[zCall].bridgeCallData[lastWritePointer].length = bufSize;
		gCall[zCall].bridgeCallData[lastWritePointer].dtmf = 0;
#if 0
        FILE *fp;
        char filename[64] = "";
        sprintf(filename, "/tmp/save_bridgeData_%d",zCall);
        fp = fopen(filename, "a+");
        if(fp == NULL)
        {
            return -1;
        }
        fwrite(gCall[zCall].bridgeCallData[lastWritePointer].data, 1, 
			gCall[zCall].bridgeCallData[lastWritePointer].length, fp);
        fclose(fp);
#endif
	}

	gCall[zCall].bridgeCallData[lastWritePointer].filled = 1;

	gCall[zCall].lastWritePointer++;

	if(gCall[zCall].lastWritePointer >= BRIDGE_DATA_ARRAY_SIZE)
	{
		gCall[zCall].lastWritePointer = 0;
	}

	return (0);

}/*END: int saveBridgeCallPacket*/

int getWavHeaderLength(char *filebuff, int len)
{
   int DataStartPoint = 0;

   if(len >= 4 && filebuff != NULL)
   {
      if(filebuff[0] == 'R' &&
         filebuff[1] == 'I' &&
         filebuff[2] == 'F' &&
         filebuff[3] == 'F')
      {
         for(int i=4; i<64;i++)
         {
            if(filebuff[i] == 'd')
            {
               i++;
               if(filebuff[i] == 'a')
               {
                  i++;
                  if(filebuff[i] == 't')
                  {
                     i++;
                     if(filebuff[i] == 'a')
                     {
                        DataStartPoint = i+5;
                        return DataStartPoint;
                     }
                     else
                     {
                        i--;
                        continue;
                     }
                  }
                  else
                  {
                     i--;
                     continue;
                  }
               }
               else
               {
                  i--;
                  continue;
               }
            }
         }
      }
   }

   return DataStartPoint;

}/*int getDataStartingPoint*/

///Trim white space from the front and back of a string.
/*----------------------------------------------------------------------------
Trim white space from the front and back of a string.
----------------------------------------------------------------------------*/
int
trim_whitespace (char *string)
{
	int sLength;
	char *tmpString;
	char *ptr;

	if ((sLength = strlen (string)) == 0)
		return (0);

	tmpString = (char *) calloc ((size_t) (sLength + 1), sizeof (char));
	//printf("DM alloc line=%d mem=%p\n", __LINE__, tmpString); fflush(stdout);
	ptr = string;

	if (isspace (*ptr))
	{
		while (isspace (*ptr))
			ptr++;
	}

	sprintf (tmpString, "%s", ptr);
	ptr = &tmpString[(int) strlen (tmpString) - 1];
	while (isspace (*ptr))
	{
		*ptr = '\0';
		ptr--;
	}

	sprintf (string, "%s", tmpString);
	//printf("DM alloc freeing line=%d mem=%p\n", __LINE__, tmpString); fflush(stdout);
	free (tmpString);
	return (0);
}


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
int
get_name_value (char *section, char *name, char *defaultVal, char *value,
		int bufSize, char *file, char *err_msg)
{
	char mod[] = "get_name_value";
	FILE *fp;
	char line[512];
	char currentSection[80], lhs[256], rhs[256];
	int found, inDesiredSection = 0, sectionSpecified = 1;

	sprintf (value, "%s", defaultVal);
	err_msg[0] = '\0';

	if (section[0] == '\0')
	{
		/*
		 * If no section specified, we're always in desired section 
		 */
		sectionSpecified = 0;
		inDesiredSection = 1;
	}
	if (name[0] == '\0')
	{
		sprintf (err_msg, "No name passed to %s.", mod);
		return (-1);
	}
	if (file[0] == '\0')
	{
		sprintf (err_msg, "No file name passed to %s.", mod);
		return (-1);
	}
	if (bufSize <= 0)
	{
		sprintf (err_msg,
			 "Return bufSize (%d) passed to %s must be > 0.",
			 bufSize, mod);
		return (-1);
	}

	if ((fp = fopen (file, "r")) == NULL)
	{
		sprintf (err_msg, "Failed to open file (%s). [%d, %s]", file,
			 errno, strerror(errno));
		return (-1);
	}

	memset (line, 0, sizeof (line));

	found = 0;
	while (fgets (line, sizeof (line) - 1, fp))
	{
		/*
		 * Strip \n from the line if it exists 
		 */
		if (line[(int) strlen (line) - 1] == '\n')
		{
			line[(int) strlen (line) - 1] = '\0';
		}

		trim_whitespace (line);

		if (!inDesiredSection)
		{
			if (line[0] != '[')
			{
				memset (line, 0, sizeof (line));
				continue;
			}

			memset (currentSection, 0, sizeof (currentSection));
			sscanf (&line[1], "%[^]]", currentSection);

			if (strcmp (section, currentSection) != 0)
			{
				memset (line, 0, sizeof (line));
				continue;
			}
			inDesiredSection = 1;
			memset (line, 0, sizeof (line));
			continue;
		}
		else
		{
			/*
			 * If we are already in a section and have encountered
			 * another section AND a section was specified then
			 * get out of the loop.  
			 */
			if (line[0] == '[' && sectionSpecified)
			{
				memset (line, 0, sizeof (line));
				break;
			}
		}

		memset (lhs, 0, sizeof (lhs));
		memset (rhs, 0, sizeof (rhs));

		if (sscanf (line, "%[^=]=%[^=]", lhs, rhs) < 2)
		{
			memset (line, 0, sizeof (line));
			continue;
		}
		trim_whitespace (lhs);
		if (strcmp (lhs, name) != 0)
		{
			memset (line, 0, sizeof (line));
			continue;
		}
		found = 1;
		sprintf (value, "%.*s", bufSize, rhs);
		break;
	}			/* while fgets */
	fclose (fp);
	if (!found)
	{
		if (sectionSpecified)
			sprintf (err_msg,
				 "Failed to find (%s) under section (%s) in file (%s).",
				 name, section, file);
		else
			sprintf (err_msg,
				 "Failed to find (%s) entry in file (%s).",
				 name, file);
		return (-1);
	}

	return (0);

}				/*END: int get_name_value */

void
getRingEventTime (char *current_time)
{
	struct tm *tm;
	struct timeval tp;
	struct timezone tzp;
	int ret_code;

	//gettimeofday() is a UNIX system call. 
	if ((ret_code = gettimeofday (&tp, &tzp)) == -1)
	{
		current_time = NULL;
		return;
	}

	//localtime() is a UNIX system call. 
	tm = localtime (&tp.tv_sec);

	sprintf (current_time, "%02d%02d%04d%02d%02d%02d%02d", tm->tm_mon + 1,
		 tm->tm_mday, tm->tm_year + 1900, tm->tm_hour, tm->tm_min,
		 tm->tm_sec, tp.tv_usec / 10000);

	return;

}				/*END: void getRingEventTime */

void
setTime (char *time_str, long *tics)
{
	int rc;
	struct timeval tp;
	struct timezone tzp;
	struct tm *tm;

	if ((rc = gettimeofday (&tp, &tzp)) == -1)
	{
		strcpy (time_str, "0000000000000000");
		time (tics);
		return;
	}

	tm = localtime (&tp.tv_sec);
	sprintf (time_str, "%02d%02d%04d%02d%02d%02d%02d", tm->tm_mon + 1,
		 tm->tm_mday, tm->tm_year + 1900, tm->tm_hour, tm->tm_min,
		 tm->tm_sec, tp.tv_usec / 10000);
	*tics = tp.tv_sec;

	return;
}

int
arc_mkdir(char *zDir, int mode_t)
{
	int i = 0;
	char yDirName[256];
	sprintf(yDirName, "%s", "\0");

	if(!zDir)
	{
		return (-1);
	}

	if(!*zDir)
	{
		return (-1);
	}

    while(1)
    {
        if(zDir[i] == '/')
        {
            strncpy(yDirName, zDir, i+1);

            yDirName[i+1] = '\0';

            if(access(yDirName, F_OK|R_OK ) < 0)
            {
                if(errno == ENOENT)
                {
                    if (mkdir(yDirName, mode_t) < 0)
                    {
                        return (-1);
                    }

                    i++;
                    continue;
                }

                return(-1);
            }
        }
        else if(zDir[i] == 0)
        {
            sprintf(yDirName, "%s", zDir);

            if(access(yDirName, F_OK|R_OK ) < 0)
            {
                if(errno == ENOENT)
                {
                    if (mkdir(yDirName, mode_t) < 0)
                    {
                        return (-1);
                    }

                    return(0);
                }

                return(-1);
            }

            return(0);
        }

        i++;
    }

}/*END:int arc_mkdir*/


int loadPlayBackControls()
{
	char yErrMsg[256];
	char yStrTmp[256];

//char GV_SkipTerminateKey[512];
//char GV_SkipRewindKey[512];
//char GV_SkipForwardKey[512];
//char GV_SkipBackwardKey[512];
//char GV_PauseKey[512];
//char GV_ResumeKey[512];
//char GV_VolumeUpKey[512];
//char GV_VolumeDownKey[512];
//char GV_SpeedUpKey[512];
//char GV_SpeedDownKey[512];
//
	int yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "SKIP_TERMINATE", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_SkipTerminateKey, "%s", yStrTmp);

	yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "SKIP_REWIND", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_SkipRewindKey, "%s", yStrTmp);

	yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "SKIP_BACKWARD", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_SkipBackwardKey, "%s", yStrTmp);

	yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "SKIP_FORWARD", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_SkipForwardKey, "%s", yStrTmp);

	yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "PAUSE", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_PauseKey, "%s", yStrTmp);

	yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "RESUME", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_ResumeKey, "%s", yStrTmp);

	yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "VOLUME_UP", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_VolumeUpKey, "%s", yStrTmp);

	yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "VOLUME_DOWN", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_VolumeDownKey, "%s", yStrTmp);

	yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "SPEED_UP", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_SpeedUpKey, "%s", yStrTmp);

	yRc = get_name_value ("PLAYBACK_CONTROL_KEYS", "SPEED_DOWN", "!", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	sprintf (GV_SpeedDownKey, "%s", yStrTmp);


	return 0;

} //loadPlayBackControls

int convert_to_array (char * _headers, char _array[100][100])
{

	int idx = 0;

	char delim[] = ",";

	char *ptr = strtok(_headers, delim);

	while(ptr != NULL)
	{
		sprintf(_array[idx], "%s", ptr);
		ptr = strtok(NULL, delim);
		idx++;
		sprintf(_array[idx], "\0");

	} //while

	return 0;

} //convert_to_array

int
loadConfigFile (const char *progname, const char *path)
{
	int zCall = -1;
	char mod[] = "loadConfigFile";
    char buff[1024];

	char yErrMsg[256];
	char yStrTmp[256];
	char yStrTmpDebug[256];
	char yStrHeaderList[1024 * 10];

	char *pch = NULL;

	int yRc = 0;
	int i = 0;

	/*
	 * Load .TEL.cfg
	 */

	sprintf (gIpCfg, "%s/Telecom/Tables/.TEL.cfg", gIspBase);
	sprintf (gGlobalCfg, "%s/Global/.Global.cfg", gIspBase);

    if(progname != NULL)
	{
		if(strcmp(progname, "ArcSipRedirector") == 0)
		{
			snprintf(buff, sizeof(buff), "%s/Telecom/Tables/%s.cfg", gIspBase, progname);
			if(access(buff, F_OK) == 0)
			{
				snprintf(gIpCfg, sizeof(gIpCfg), "%s/Telecom/Tables/%s.cfg", gIspBase, progname);
			}
		}
	}

	/*
	 *		Systep Phrase Root Dir
	 */
	if(getenv("OBJDB"))
	{
		sprintf(gSystemPhraseRoot, "%s", getenv("OBJDB"));
	}
	else
	{
		sprintf(gSystemPhraseRoot, "%s/Telecom/phrases", getenv("ISPBASE"));
	}

	/*
	 * 	DNIS Source
	 */
	sprintf (yStrTmp, "%s", "\0");

	gDnisSource = DNIS_SOURCE_RURI;

	yRc = get_name_value ("\0", "DnisSource", "RURI", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	if (yStrTmp[0] && strcasecmp (yStrTmp, "TO") == 0)
	{
		gDnisSource = DNIS_SOURCE_TO;
	}


	/*
	 *	System Language
	 */
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "DefaultLanguage", "AMENG", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);

	if (!yStrTmp[0] || strcmp (yStrTmp, "NONE") == 0)
	{
		sprintf(gSystemLanguage, "%s", "AMENG");
	}
	else
	{
		sprintf(gSystemLanguage, "%s", yStrTmp);
	}


	/*
	 * Debug Modules
	 */
	sprintf (yStrTmp, "%s", "\0");
	sprintf (yStrTmpDebug, "%s", "\0");

	yRc = get_name_value ("\0", "Debug_Modules", "\0", yStrTmp,
			      sizeof (yStrTmp), gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		sprintf (yStrTmpDebug, "%s", yStrTmp);
	}


	pch = strstr (yStrTmpDebug, "FILE_IO");
	if (pch != NULL)
	{
		gDebugModules[DEBUG_MODULE_FILE].enabled = 1;
		pch = NULL;
	}

	pch = strstr (yStrTmpDebug, "AUDIO");
	if (pch != NULL)
	{
		gDebugModules[DEBUG_MODULE_AUDIO].enabled = 1;
		pch = NULL;
	}

	pch = strstr (yStrTmpDebug, "SR");
	if (pch != NULL)
	{
		gDebugModules[DEBUG_MODULE_SR].enabled = 1;
		pch = NULL;
	}

	pch = strstr (yStrTmpDebug, "SIP");
	if (pch != NULL)
	{
		gDebugModules[DEBUG_MODULE_SIP].enabled = 1;
		pch = NULL;
	}

	pch = strstr (yStrTmpDebug, "RTP");
	if (pch != NULL)
	{
		gDebugModules[DEBUG_MODULE_RTP].enabled = 1;
		pch = NULL;
	}

	pch = strstr (yStrTmpDebug, "CALL");
	if (pch != NULL)
	{
		gDebugModules[DEBUG_MODULE_CALL].enabled = 1;
		pch = NULL;
	}

	pch = strstr (yStrTmpDebug, "MEMORY");
	if (pch != NULL)
	{
		gDebugModules[DEBUG_MODULE_MEMORY].enabled = 1;
		pch = NULL;
	}

	pch = strstr (yStrTmpDebug, "STACK");
	if (pch != NULL)
	{
		gDebugModules[DEBUG_MODULE_STACK].enabled = 1;
		pch = NULL;
	}

	/*
	 * CDRs
	 */
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "ENABLE_CDRS", "\0", yStrTmp,
			      sizeof (yStrTmp), gGlobalCfg, yErrMsg);

	if (strstr (yStrTmp, "CALL_PRESENTED") != NULL)
	{
		gWritePCForEveryCall = 1;
	}

	/*
	 * END: CDRs
	 */

	/*
	 * Google Speech
	 */
	gGoogleSR_Enabled = 0;
	memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	yRc = get_name_value ("\0", "GOOGLE_SR", "\0", yStrTmp,
				sizeof (yStrTmp), gIpCfg, yErrMsg);
	if ( (strcmp(yStrTmp, "ON") == 0) ||
		(strcmp(yStrTmp, "On") == 0) ||
         (strcmp(yStrTmp, "on") == 0) )
	{
		gGoogleSR_Enabled = 1;
	}

	/*
	 * MRCP TTS
	 */
	gMrcpTTS_Enabled = 0;
	memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	yRc = get_name_value ("\0", "TTS_MRCP", "\0", yStrTmp,
				sizeof (yStrTmp), gIpCfg, yErrMsg);
	if ( (strcmp(yStrTmp, "ON") == 0) ||
		(strcmp(yStrTmp, "On") == 0) ||
         (strcmp(yStrTmp, "on") == 0) )
	{
		gMrcpTTS_Enabled = 1;
	}



	/*
	 * Cache directory
	 */
	sprintf (gCacheDirectory,   "%s", "\0");
	sprintf (yStrTmp,           "%s", "\0");
	yRc = get_name_value ("\0", "SIP_CacheDir", "\0", yStrTmp, sizeof (gCacheDirectory),
                  gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
        sprintf (gCacheDirectory, "%s", yStrTmp);
		yRc = arc_mkdir(gCacheDirectory, 0755);

		if(yRc < 0)
		{
        	sprintf (gCacheDirectory, "%s", ".");
		}
	}
	else
	{
        sprintf (gCacheDirectory, "%s", ".");
	}

	/*
	 * Default Gateway
	 */
	sprintf (gDefaultGateway, "%s", "\0");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "DefaultGateway", "\0", yStrTmp, sizeof (gDefaultGateway),
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
//__DDNDEBUG__(DEBUG_MODULE_SIP, "DefaultGateway", yStrTmp, -1);
		sprintf (gDefaultGateway, "%s", yStrTmp);
	}

	/*
	 * Preferred Codec
	 */
	sprintf (gPreferredCodec, "%s", "\0");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "SIP_PreferredCodec", "\0", yStrTmp, sizeof (gPreferredCodec),
			      gIpCfg, yErrMsg);



	if (yStrTmp[0])
	{
//__DDNDEBUG__(DEBUG_MODULE_SIP, "PreferredCodec", yStrTmp, -1);
		sprintf (gPreferredCodec, "%s", yStrTmp);
	}
//	sprintf (gPreferredVideoCodec, "%s", "h263");

	/*
	 * AMR Index
	 */
	sprintf (gAMRIndex, "%s", "\0");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("CODEC_AMR", "Index", "0", yStrTmp, sizeof (gAMRIndex),
			      gIpCfg, yErrMsg);



	if (yStrTmp[0])
	{
//__DDNDEBUG__(DEBUG_MODULE_SIP, "PreferredCodec", yStrTmp, -1);
		sprintf (gAMRIndex, "%s", yStrTmp);
	}



	/*
	 * AMR maxPTime
	 */
	gMaxPTime = 0;
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("CODEC_AMR", "maxptime", "0", yStrTmp, sizeof (gMaxPTime),
			      gIpCfg, yErrMsg);



	if (yStrTmp[0])
	{
		gMaxPTime = atoi(yStrTmp);
	}


	/*
     * AMR preferredPTime
     */
    gPreferredPTime = 20;
    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("\0", "preferred_ptime", "20", yStrTmp, sizeof (gPreferredPTime),
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        gPreferredPTime = atoi(yStrTmp);
    }

	/*
	 * G729 Annexb
	 */
	sprintf (g729Annexb, "%s", "NO");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("CODEC_G729", "annexb", "NO", yStrTmp, sizeof (g729Annexb),
			      gIpCfg, yErrMsg);



	if (yStrTmp[0])
	{
//__DDNDEBUG__(DEBUG_MODULE_SIP, "PreferredCodec", yStrTmp, -1);
		sprintf (g729Annexb, "%s", yStrTmp);
	}

#if 0	
	/*
	 * G729 Silence Buffer
	 */
	//sprintf ((char *)g729b_silenceBuffer, "%s", "3440");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("CODEC_G729", "silencePayload", "3440", yStrTmp, sizeof (g729b_silenceBuffer),
			      gIpCfg, yErrMsg);



	if (yStrTmp[0])
	{
//__DDNDEBUG__(DEBUG_MODULE_SIP, "PreferredCodec", yStrTmp, -1);
	//	sprintf ((char *)g729b_silenceBuffer, "%s", yStrTmp);
		memcpy(g729b_silenceBuffer, yStrTmp, sizeof(g729b_silenceBuffer));
	}
#endif


	/*
	 * Sip Proxy
	 */
	sprintf (gOutboundProxy, "%s", "\0");
	sprintf (yStrTmp, "%s", "\0");

	gOutboundProxyRequired = 0;

	yRc = get_name_value ("\0", "SIP_OutboundProxy", "\0", yStrTmp, sizeof (gOutboundProxy),
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
//__DDNDEBUG__(DEBUG_MODULE_SIP, "SIP_OutboundProxy", yStrTmp, -1);
		sprintf (gOutboundProxy, "%s", yStrTmp);
		gOutboundProxyRequired = 1;
	}
	/*
	 * Sip Authentication user
	 */
	sprintf (gAuthUser, "%s", "\0");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "SIP_Authentication_User", "\0", yStrTmp, sizeof (gOutboundProxy),
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		sprintf (gAuthUser, "%s", yStrTmp);
	}

	/*
	 * Sip Authentication Password
	 */
	sprintf (gAuthPassword, "%s", "\0");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "SIP_Authentication_Pass", "\0", yStrTmp, sizeof (gOutboundProxy),
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		sprintf (gAuthPassword, "%s", yStrTmp);
	}

	/*
	 * Sip Authentication User Id
	 */
	sprintf (gAuthId, "%s", "\0");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "SIP_Authentication_Id", "\0", yStrTmp, sizeof (gOutboundProxy),
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		sprintf (gAuthId, "%s", yStrTmp);
	}

	/*
	 * Sip From URL for  Bridge Call
	 */
	sprintf (gFromUrl, "%s", "\0");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "SIP_From", "\0", yStrTmp, sizeof (gOutboundProxy),
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		sprintf (gFromUrl, "%s", yStrTmp);
	}

	/*
	 * Sip Authentication realm
	 */
	sprintf (gAuthRealm, "%s", "\0");
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "SIP_Authentication_Realm", "\0", yStrTmp, sizeof (gOutboundProxy),
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		sprintf (gAuthRealm, "%s", yStrTmp);
	}

	/*
	 * Sip Port
	 */
	sprintf (yStrTmp, "%s", "\0");
	gSipPort = 5060;
	yRc = get_name_value ("\0", "SIP_Port", "5060", yStrTmp, 5, gIpCfg,
			      yErrMsg);
	if (yStrTmp[0])
	{

//__DDNDEBUG__(DEBUG_MODULE_SIP, "SIP_PORT", yStrTmp, -1);
		gSipPort = atoi (yStrTmp);
	}

	/*
	 * Sip Proxy
	 */
	sprintf (yStrTmp, "%s", "\0");
	gSipProxy = 0;
	yRc = get_name_value ("SIP", "SipProxy", "0", yStrTmp, 5, gIpCfg,
			      yErrMsg);

	if (yStrTmp[0])
	{

		if(strcasecmp(yStrTmp, "ON") == 0)
		{
			gSipProxy = 1;
		}
		else
		{
			gSipProxy = 0;
		}
	}

	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("SIP", "SipOutOfLicenseResponse", "503", yStrTmp, 5, gIpCfg, yErrMsg);

	int val;

    val = atoi(yStrTmp);

    switch(val){
     case 486:
        gSipOutOfLicenseResponse = 486;
        break;
	 case 503:
        gSipOutOfLicenseResponse = 503;
        break;
     default:
        gSipOutOfLicenseResponse = 503;
		break;
    }

	sprintf (yStrTmp, "%s", "\0");

	gRejectEmptyInvite  = 0;

	yRc = get_name_value ("SIP", "RejectInviteWithoutSdp", "0", yStrTmp, 5, gIpCfg,
					yErrMsg);



	if (yStrTmp[0])
	{
		if(strcasecmp(yStrTmp, "1") == 0)
		{
			gRejectEmptyInvite  = 1;
		}
		else
		{
			gRejectEmptyInvite  = 0;
		}
	}
	/*
	 * Sip Redirection
	 */
	sprintf (yStrTmp, "%s", "\0");
	gSipRedirection = 0;
	yRc = get_name_value ("SIP", "SipRedirection", "0", yStrTmp, 5, gIpCfg,
				      yErrMsg);
	
	if (yStrTmp[0])
	{
		if(strcasecmp(yStrTmp, "ON") == 0)
		{
			gSipRedirection = 1;
		}
		else
		{
			gSipRedirection = 0;
		}
	}

	/*
	 * UUI_Encoding 
	 */
	sprintf (yStrTmp, "%s", "\0");
	gUUIEncoding = 0;
	yRc = get_name_value ("SIP", "UUI_Encoding", "HEX", yStrTmp, sizeof(yStrTmp), gIpCfg,
				      yErrMsg);
	
	if (yStrTmp[0])
	{
		if(strcasecmp(yStrTmp, "HEX_JETBLUE") == 0)
		{
			gUUIEncoding  |= USER_AGENT_AVAYA_CM;
		}
		else if(strcasecmp(yStrTmp, "HEX_SYKES") == 0)
		{
			gUUIEncoding  |= USER_AGENT_AVAYA_SM;
		}
		else if(strcasecmp(yStrTmp, "HEX_CI") == 0)
		{
			gUUIEncoding  |= USER_AGENT_AVAYA_XM;
		}
		else
		{
			gUUIEncoding  |= USER_AGENT_UNKNOWN;
		}
	}

#if 0
	/*
	 * Redirector Min Spans  
	 */

	sprintf (yStrTmp, "%s", "\0");

    gSipRedirectorMinSpans = 0;

	yRc = get_name_value ("SIP", "SipRedirectorMinSpans", "0", yStrTmp, 5, gIpCfg, yErrMsg);
	
	if (yStrTmp[0])
	{
        gSipRedirectorMinSpans = atoi(yStrTmp);
	}

#endif 



#if 0 // added so that we can have both 
	if ( gSipProxy == 1 )
	{
		gSipRedirection = 0;
	}
#endif 

    /*
     * SIP Signaled digits - this is also handled 
     * 
     */

    sprintf (yStrTmp, "%s", "\0");
    gSipSignaledDigits = 0;
    yRc = get_name_value ("SIP", "SipSignaledDigitsEnable", "0", yStrTmp, 5, gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {

        if(strcasecmp(yStrTmp, "ON") == 0)
        {
            gSipSignaledDigits=1;
        }
        else
        {
            gSipSignaledDigits=0;
        }
    }


    /*  
     *  IPv6 enable 
     */

    sprintf (yStrTmp, "%s", "\0");
    gEnableIPv6Support = 0;
    yRc = get_name_value ("SIP", "EnableIPv6", "0", yStrTmp, 5, gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {

        if(strcasecmp(yStrTmp, "ON") == 0)
        {
            gEnableIPv6Support=1;
        }
        else
        {
            gEnableIPv6Support=0;
        }
    }


	/*
	 * SIP Headers to be saved for VoiceXML
	 */
	sprintf (yStrHeaderList, "%s", "\0");
	yRc = get_name_value ("SIP", "SaveHeaders", "user-agent", 
					yStrHeaderList, (1024 * 10), gIpCfg, yErrMsg);

	sprintf(gHeaderList, "%s", yStrHeaderList);

	convert_to_array(gHeaderList, gHeaderListArray);	

	/*
	 * Sip Max Call Duration Allowed
	 */
	sprintf (yStrTmp, "%s", "\0");
	yRc = get_name_value ("\0", "SIP_MaxCallDuration", "-1", yStrTmp, 7, gIpCfg,
			      yErrMsg);

	if (yStrTmp[0])
	{
		gMaxCallDuration = atoi(yStrTmp);

		if(gMaxCallDuration == 0)
		{
			gMaxCallDuration = -1;
		}
	}
	else
	{
		gMaxCallDuration = -1;
	}

    /* This is a parameter for the Redirector regex routing table */

    sprintf (yStrTmp, "%s", "\0");
    yRc = get_name_value ("RedirectorRoutingRules", "UnavailableDestAction", "Route", yStrTmp, 7, gIpCfg,
                  yErrMsg);

    if (yStrTmp[0])
    {
        if(strcasecmp(yStrTmp, "Route") == 0){
          gSipRedirectionDefaultRule = SIP_REDIRECTION_RULE_ROUTE;
        } else 
        if(strcasecmp(yStrTmp, "Reject") == 0){
          gSipRedirectionDefaultRule = SIP_REDIRECTION_RULE_REJECT;
        } else {
          gSipRedirectionDefaultRule = SIP_REDIRECTION_RULE_ROUTE;
        }
    }
    /* turn on this redirector table as a feature */

    sprintf (yStrTmp, "%s", "\0");
    yRc = get_name_value ("RedirectorRoutingRules", "UseRoutingTable", "Off", yStrTmp, 7, gIpCfg,
                  yErrMsg);

    if (yStrTmp[0])
    {
        if(strcasecmp(yStrTmp, "ON") == 0){
          gSipRedirectionUseTable = 1;
        } else
        if(strcasecmp(yStrTmp, "OFF") == 0){
          gSipRedirectionUseTable = 0;
        } else {
          gSipRedirectionUseTable = 0;
        }
    }




#if 0
	/*
	 * Sip Video Support
	 */
	sprintf (yStrTmp, "%s", "\0");
	gVideoSupported = 1;
	yRc = get_name_value ("\0", "SIP_VideoSupport", "1", yStrTmp, 5, gIpCfg,
			      yErrMsg);

	if (yStrTmp[0])
	{
		if(strncmp(yStrTmp, "0", 1) == 0)
		{
			gVideoSupported = 0;
		}
	}
#endif

//#ifdef ACU_RECORD_TRAILSILENCE
	
	/*
	 * Sip Trail Silence Detection Support
	 */
	sprintf (yStrTmp, "%s", "\0");
	gTrailSilenceDetection = 0;
	yRc = get_name_value ("\0", "SIP_TrailSilenceDetection", "0", yStrTmp, 5, gIpCfg,
			      yErrMsg);
		
	if (yStrTmp[0])
	{
		if(strncmp(yStrTmp, "1", 1) == 0)
		{
			gTrailSilenceDetection = 1;
		}
	}


	/*
	 * Sip Trail Silence Detection Ports Support
	 */
	sprintf (yStrTmp, "%s", "\0");
	gMaxAcuPorts = 0;
	yRc = get_name_value ("\0", "SIP_AculabTrailSilenceResources", "0", yStrTmp, 5, gIpCfg,
			      yErrMsg);
	
	if (yStrTmp[0])
	{
		gMaxAcuPorts = atoi (yStrTmp);
		if(gMaxAcuPorts < 0 || gMaxAcuPorts > 10)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, WARN, 
				"gMaxAcuPorts = %d is out of range.  Defaulting to 10.", gMaxAcuPorts);
			gMaxAcuPorts = 10;
		}
	}
//#endif


    /*
     * Sip Symmetric RTP Support
     */
    sprintf (yStrTmp, "%s", "\0");
    gSymmetricRtp = 0;
    yRc = get_name_value ("\0", "SIP_SymmetricRtp", "0", yStrTmp, 5, gIpCfg,
                  yErrMsg);

    if (yStrTmp[0])
    {
        if(strncmp(yStrTmp, "1", 1) == 0)
        {
            gSymmetricRtp = 1;
        }
    }


	/*
	 * Sip Output DTMF
	 */
	sprintf (yStrTmp, "%s", "\0");
	gSipOutputDtmfMethod = OUTPUT_DTMF_RFC_2833;

	yRc = get_name_value ("\0", "OutputDtmf", "RFC_2833_X", yStrTmp, 20,
			      gIpCfg, yErrMsg);

    if(yStrTmp[0] && strcmp(yStrTmp, "RFC_2833_X") == 0)    //Work around to make it case insensitive.  Making the change in get_name_value will affect lots of other params.
    {
        yRc = get_name_value ("\0", "OutputDTMF", "RFC_2833", yStrTmp, 20, gIpCfg, yErrMsg);
    }      

	if (yStrTmp[0])
	{

		if (strcmp (yStrTmp, "INBAND") == 0)
		{
			gSipOutputDtmfMethod = OUTPUT_DTMF_INBAND;
		}
		else if (strcmp (yStrTmp, "SIP_INFO") == 0)
		{
			gSipOutputDtmfMethod = OUTPUT_DTMF_SIP_INFO;
		}
		else
		{
			gSipOutputDtmfMethod = OUTPUT_DTMF_RFC_2833;
		}
	}

	/*
	 * RFC 2833 Payload type
	 */
	sprintf (yStrTmp, "%s", "\0");
	gRfc2833Payload = 96;

	yRc = get_name_value ("\0", "SIP_RFC2833Payload", "96", yStrTmp, 5,
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		gRfc2833Payload = atoi (yStrTmp);

	}


	/*
	 * Minimum RFC 2833 event delay in milliseconds
	 */
	sprintf (yStrTmp, "%s", "\0");
	gInterDigitDelay = 100;

	yRc = get_name_value ("\0", "SIP_InterDigitDuration", "100", yStrTmp, 5,
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		gInterDigitDelay = atoi (yStrTmp);
	}

	/*
	 * SIP_IP
	 */
	sprintf (yStrTmp, "%s", "\0");
	sprintf (gHostIp, "%s", "\0");
	
	yRc = get_name_value ("\0", "SIP_HostIP", "0.0.0.0", yStrTmp, 48,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		sprintf(gHostIp, "%s", yStrTmp);
	}

	/*
	 * CONTACT_IP
	 */

	sprintf (yStrTmp, "%s", "\0");
	sprintf (gContactIp, "%s", gHostIp);
	
	yRc = get_name_value ("\0", "SIP_ContactIP", gHostIp, yStrTmp, 48,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		sprintf(gContactIp, "%s", yStrTmp);
	}

	/*
	 * SDP_IP
	 */

	sprintf (yStrTmp, "%s", "\0");
	sprintf (gSdpIp, "%s", gHostIp);
	
	yRc = get_name_value ("\0", "SIP_SDPIP", gHostIp, yStrTmp, 48,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		sprintf(gSdpIp, "%s", yStrTmp);
	}

    /*
     * IPv6 interface info
     * 
     */

    sprintf(yStrTmp, "%s", "\0");
    sprintf(gHostIf, "%s", "\0");

    yRc = get_name_value("\0", "SIP_HostIF", "eth0", yStrTmp, 48, gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        sprintf(gHostIf, "%s", yStrTmp);
    }

	/*
	 * PercentInboundAllowed
	 */
	sprintf (yStrTmp, "%s", "\0");
	gPercentInboundAllowed = 100;

	yRc = get_name_value ("\0", "PercentInboundAllowed", "100", yStrTmp, 6,
			      gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		gPercentInboundAllowed = atoi (yStrTmp);
	}
	if ( gPercentInboundAllowed < 0 || gPercentInboundAllowed > 100 )
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, WARN, 
			"Invalid value (%d) set for PercentInboundAllowed in %s.  Defaulting to 100",
			gPercentInboundAllowed, gIpCfg);
		gPercentInboundAllowed = 100;
	}

	/*
	 *	SIP_VFUDuration
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "SIP_VFUDuration", "0", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gVfuDuration = atoi(yStrTmp);

	}
	else
	{
		gVfuDuration = 0;
	}
	/*
	 * END: SIP_VFUDuration
	 */


	/*
	 *	SIP_BeepDuration
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "SIP_BeepDuration", "0", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gBeepSleepDuration = atoi(yStrTmp);

	}
	else
	{
		gBeepSleepDuration = 100;
	}
	/*
	 * END: SIP_BeepDuration
	 */



	/*
	 * SIP_RtpStartPort
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "SIP_RtpStartPort", "10500", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		LOCAL_STARTING_RTP_PORT = atoi(yStrTmp);
		
	}
	else
	{
		LOCAL_STARTING_RTP_PORT = LOCAL_STARTING_RTP_PORT_DEFAULT;
	}


	/*
	 * SIP_RecycleBase
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "SIP_RecycleBase", "20000", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gCallMgrRecycleBase = atoi(yStrTmp);
	}

	if(gCallMgrRecycleBase <= 0)
	{
		gCallMgrRecycleBase = 20000;
	}
		

	/*
	 * SIP_RecycleDelta
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "SIP_RecycleDelta", "1000", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gCallMgrRecycleDelta = atoi(yStrTmp);
		
	}

	if(gCallMgrRecycleDelta <= 0)
	{
		gCallMgrRecycleDelta = 1;
	}

	/*
	 * SIP_RecycleTimeOut
	 */

	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "SIP_RecycleTimeOut", "360", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gShutDownMaxTime = atoi(yStrTmp);
		
	}

	if(gShutDownMaxTime <= 0)
	{
		gShutDownMaxTime = 120;
	}


	/*
	*  SIP_InboundEarlyMedia
	*/
	sprintf (yStrTmp, "%s", "\0");

	yRc = get_name_value ("\0", "SIP_InboundEarlyMedia", "0", yStrTmp, 16,
				gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gInboundEarlyMedia = atoi(yStrTmp);
	}
	else
	{
		gInboundEarlyMedia = 0;
	}
	/*
	* END: SIP_InboundEarlyMedia
	*/

    /*
     * PRACKMethod
     */
    yRc = get_name_value ("IP", "PRACKMethod", "Disabled", yStrTmp, sizeof(yStrTmp),
                  gIpCfg, yErrMsg);
    if (yStrTmp[0])
    {
        if (strcmp (yStrTmp, "Supported") == 0)
        {
            gPrackMethod = PRACK_SUPPORTED;
        }
        else if (strcmp (yStrTmp, "Required") == 0)
        {
            gPrackMethod = PRACK_REQUIRE;
        }
        else
        {
            gPrackMethod = PRACK_DISABLED;
        }
    }
    else
    {
        gPrackMethod = PRACK_DISABLED;
    }
	/*
	 * END: Load .TEL.cfg
	 */

	/*
	 * TO DO: Debug modules,  user agent name, dtmf output method
	 */
	
	/*
	 * InbandToneThreshold
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "InbandToneThreshold", "100.0", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gDtmfToneThreshold = atof(yStrTmp);
	}

	/*
	 * InbandToneRange
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "InbandToneRange", "0.2", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gDtmfToneRange = atof(yStrTmp);
	}

	/*
	 * InbandToneFlushTime
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "InbandToneFlushTime", "100", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gDtmfToneFlushTime = atoi(yStrTmp);
	}

	/*
	 * Support Early Media. Done for Global Crossing
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "EarlyMedia", "1", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gEarlyMediaSupported = atoi(yStrTmp);
	}

	/*
	 * RefreshRtpSession
	 */

	gRefreshRtpSession = 0;	/*Default, backward compatible value is 0*/
	
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "RefreshRtpSession", "0", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		if(yStrTmp[0] == '1')
		{
			gRefreshRtpSession = 1;
		}
	}

	/*
	 * InbandToneFlushTime
	 */
	sprintf (yStrTmp, "%s", "\0");
	
	yRc = get_name_value ("\0", "InbandToneFlushTime", "100", yStrTmp, 16,
			      gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		gDtmfToneFlushTime = atoi(yStrTmp);
	}
	
	/*
	 * SIP IP codes
	 */
	for(i = 0; i < 999; i++)
	{
		sprintf (yStrTmp, "%s", "21\0");

		sprintf( gSipCodes[i].name, "%d", i);

		sprintf(gSipCodes[i].value, "%s", "21");
	
		yRc = get_name_value ("SIP_RETCODE_MAPPING", gSipCodes[i].name, "21", yStrTmp, 4,
			      gIpCfg, yErrMsg);

		if(yRc == 0)
		{
			sprintf(gSipCodes[i].value, "%s", yStrTmp);
		}

	}/*END: for*/


    /*
     * Inband Tone ThreshHolds
     */

    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "ToneThreshold", "8000000.0", yStrTmp, 16, gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {

        //fprintf(stderr, " %s() InbandAudioDetection ToneThreshhold=%s\n", __func__, yStrTmp);
        gToneThreshMinimum = atof(yStrTmp);
        if(gToneThreshMinimum == 0.0){
           gToneThreshMinimum = 8000000.0;
        }
    }

    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "ToneThresholdDifference", "1000.0", yStrTmp, 16,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
       // fprintf(stderr, " %s() InbandAudioDetection-ToneThreshholdDiff=%s\n", __func__, yStrTmp);
        gToneThreshDifference = atof(yStrTmp);
        if(gToneThreshDifference == 0.0){
           gToneThreshDifference = 1000.0;
        }
    }

   // fprintf(stderr, " using %f for tone detection threshold \n", gToneThreshMinimum);
   // fprintf(stderr, " using %f for tone detection threshold diff\n", gToneThreshDifference);


   /* human detection thresholds */

    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "TrailSilenceNoiseThresh", "300.00", yStrTmp, 16,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        //fprintf(stderr, " %s() InbandAudioDetection-TrailSilenceNoiseThresh=%s\n", __func__, yStrTmp);
        gToneDetectionNoiseThresh = atof(yStrTmp);
        if(gToneThreshMinimum == 0.0){
           gToneThreshMinimum = 300.0;
        }
    }

    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "AnsweringLeadingAudioTrim", "750", yStrTmp, 16,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        //fprintf(stderr, " %s() InbandAudioDetection-TrailSilenceNoiseThresh=%s\n", __func__, yStrTmp);
        gAnsweringLeadingAudioTrim = atoi(yStrTmp);
        if( gAnsweringLeadingAudioTrim <= 0){
            gAnsweringLeadingAudioTrim = 750;
        }
    }


    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "AnsweringTrailSilenceTrigger", "750", yStrTmp, 16,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        //fprintf(stderr, " %s() InbandAudioDetection-TrailSilenceNoiseThresh=%s\n", __func__, yStrTmp);
        gAnsweringTrailSilenceTrigger = atoi(yStrTmp);
        if(gAnsweringTrailSilenceTrigger <= 0){
            gAnsweringTrailSilenceTrigger = 750;
        }
    }

    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "HumanAnsweringThresh", "3000000.0", yStrTmp, 16,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        gHumanAnsweringThresh = atof(yStrTmp);
        if(gHumanAnsweringThresh == 0.0){
           gHumanAnsweringThresh = 3000000.0;
        }
    }

    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "AnsweringDetectionNoiseThresh", "500.0", yStrTmp, 16,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        //fprintf(stderr, " %s() InbandAudioDetection-AnsweingdetectionNoiseThresh=%s\n", __func__, yStrTmp);
        gAnsweringDetectionNoiseThresh = atof(yStrTmp);
        if(gAnsweringDetectionNoiseThresh == 0.0){
           gAnsweringDetectionNoiseThresh = 500.0;
        }
    }

    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "AnsweringMachineThresh", "2000", yStrTmp, 16,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        //fprintf(stderr, " %s() InbandAudioDetection-AnsweringMachineThresh=%s\n", __func__, yStrTmp);
        gAnsweringMachineThresh = atoi(yStrTmp);
        if(gAnsweringMachineThresh == 0){
           gAnsweringMachineThresh = 2500;
        }
    }

    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "AnsweringOverallTimeout", "2500", yStrTmp, 16,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        //fprintf(stderr, " %s() InbandAudioDetection-AnsweringMachineThresh=%s\n", __func__, yStrTmp);
        gAnsweringOverallTimeout = atoi(yStrTmp);
        if(gAnsweringOverallTimeout == 0){
           gAnsweringOverallTimeout = 2500;
        }
    }


    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("InbandAudioDetection", "ToneDetectionDebugging", "OFF", yStrTmp, 16,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {

       //fprintf(stderr, " %s() InbandAudioDetection-ToneDetectionDebugging=%s\n", __func__, yStrTmp);
       if(strcasecmp(yStrTmp, "ON") == 0){
         gToneDebug = 1;
       } 
       if(strcasecmp(yStrTmp, "OFF") == 0){
         gToneDebug = 0;
       } else {
         gToneDebug = 1;
       }
    }

    sprintf (yStrTmp, "%s", "\0");
    yRc = get_name_value ("\0", "UserToUserPrefix", "", yStrTmp, 20,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
        sprintf(gTransferAAIPrefix, "%s", yStrTmp);
    }
    else
    {
        sprintf(gTransferAAIPrefix, "%s", "");
    }

	/*User-to-User param name (User-To-User or AAI or User-to-User etc)*/
    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("\0", "UserToUserParamName", "User-to-User", yStrTmp, 20,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
		sprintf(gTransferAAIParamName, "%s", yStrTmp);
    }
	else
	{
		sprintf(gTransferAAIParamName, "%s", "User-to-User");
	}

	/*User-to-User param encoding (none or hex)*/
    sprintf (yStrTmp, "%s", "\0");

    yRc = get_name_value ("\0", "UserToUserEncoding", "hex", yStrTmp, 20,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
		sprintf(gTransferAAIEncoding, "%s", yStrTmp);
    }
	else
	{
		sprintf(gTransferAAIEncoding, "%s", "hex");
	}

   /* end */

   /* FAX */

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gFaxDebug = 0; 		// default value
    yRc = get_name_value ("FAX", "FaxDebug", "OFF", yStrTmp, 25,
                  gIpCfg, yErrMsg);
	if (yStrTmp[0])
    {
		if(strcasecmp(yStrTmp, "ON") == 0)
		{
			gFaxDebug = 1;
		}
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
    memset((char *)gT30FaxStationId, '\0', sizeof(gT30FaxStationId));
    yRc = get_name_value ("FAX", "T30FaxStationId", "Arc Fax Station", yStrTmp, 256,
                  gIpCfg, yErrMsg);
    if (yStrTmp[0])
    {
       snprintf(gT30FaxStationId, sizeof(gT30FaxStationId), "%s", yStrTmp);
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
    memset((char *)gT30HeaderInfo, '\0', sizeof(gT30HeaderInfo));
    yRc = get_name_value ("FAX", "T30HeaderInfo", "Arc Fax Header Information", yStrTmp, 256,
                  gIpCfg, yErrMsg);
  
    if (yStrTmp[0])
    {
       snprintf(gT30HeaderInfo, sizeof(gT30HeaderInfo), "%s", yStrTmp);
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT30ErrorCorrection = 1;
    yRc = get_name_value ("FAX", "T30ErrorCorrection", "ON", yStrTmp, 25,
                  gIpCfg, yErrMsg);
    if (yStrTmp[0])
    {
		if(strcasecmp(yStrTmp, "OFF") == 0)
		{
			gT30ErrorCorrection = 0;
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38Enabled = 1;
    yRc = get_name_value ("FAX", "T38Enabled", "ON", yStrTmp, 25,
                  gIpCfg, yErrMsg);
    if (yStrTmp[0])
    {
		if(strcasecmp(yStrTmp, "OFF") == 0)
		{
			gT38Enabled = 0;
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));

    yRc = get_name_value ("FAX", "T38PacketInterval", "0", yStrTmp, 25,
                  gIpCfg, yErrMsg);
    if (yStrTmp[0])
    {

        gT38PacketInterval = atoi(yStrTmp);

        if( gT38PacketInterval < 0 || gT38PacketInterval > 1000){
            gT38PacketInterval = 0;
        }
    }


    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38Transport = 0;		// 0 -> udptl; 1 -> rtp
    yRc = get_name_value ("FAX", "T38Transport", "udptl", yStrTmp, 25,
                  gIpCfg, yErrMsg);
    if (yStrTmp[0])
    {
		if(strcasecmp(yStrTmp, "rtp") == 0)
		{
			gT38Transport = 1;
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38ErrorCorrectionMode = 0;
    yRc = get_name_value ("FAX", "T38ErrorCorrectionMode", "0", yStrTmp, 25,
                  gIpCfg, yErrMsg);
    if (yStrTmp[0])
    {

        int tmp = 0;

        tmp = atoi(yStrTmp);

        if(tmp > 2 || tmp < 0){
           tmp = 0;
        }
	
        gT38ErrorCorrectionMode = tmp;

    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38ErrorCorrectionSpan = 0;
    yRc = get_name_value ("FAX", "T38ErrorCorrectionSpan", "0", yStrTmp, 25,
                  gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		if(strcasecmp(yStrTmp, "1") == 0)
		{
			gT38ErrorCorrectionSpan = 1;
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38ErrorCorrectionEntries = 0;
    yRc = get_name_value ("FAX", "T38ErrorCorrectionEntries", "0", yStrTmp, 25,
                  gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		if(strcasecmp(yStrTmp, "1") == 0)
		{
			gT38ErrorCorrectionEntries = 1;
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38MaxBitRate = 9600;

    yRc = get_name_value ("FAX", "T38MaxBitRate", "9600", yStrTmp, 25,
                  gIpCfg, yErrMsg);

    struct fax_speed_lookup {
      char *string;
      int val;
    } fax_speed_lookup[] = {
      {"14400", 14400},
      {"12000", 12000},
      {"9600", 9600},
      {"7200", 7200},
      {"4800", 4800},
      {NULL, 0}
    };

	if (yStrTmp[0])
	{
           int i;

           for(i = 0; fax_speed_lookup[i].string; i++){
              if(strcmp(yStrTmp, fax_speed_lookup[i].string) == 0){
	             gT38MaxBitRate = fax_speed_lookup[i].val;
                 break;
              }
           }
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38FaxFillBitRemoval = 0;
    yRc = get_name_value ("FAX", "T38FaxFillBitRemoval", "0", yStrTmp, 25,
                  gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		if(strcasecmp(yStrTmp, "1") == 0)
		{
			gT38FaxFillBitRemoval = 1;
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38FaxVersion = 1;
    yRc = get_name_value ("FAX", "T38FaxVersion", "1", yStrTmp, 25,
                  gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		if(strcmp(yStrTmp, "1") != 0)
		{
			gT38FaxVersion = atoi(yStrTmp);
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38FaxTranscodingMMR = 0;
    yRc = get_name_value ("FAX", "T38FaxTranscodingMMR", "0", yStrTmp, 25,
                  gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		if(strcasecmp(yStrTmp, "1") == 0)
		{
			gT38FaxTranscodingMMR = 1;
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38FaxTranscodingJBIG = 0;
    yRc = get_name_value ("FAX", "T38FaxTranscodingJBIG", "0", yStrTmp, 25,
                  gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		if(strcasecmp(yStrTmp, "1") == 0)
		{
			gT38FaxTranscodingJBIG = 1;
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38FaxRateManagement = 1;		// 1 -> LocalTCF; 2 -> TransferredTCF
    yRc = get_name_value ("FAX", "T38FaxRateManagement", "LocalTCF", yStrTmp, 25,
                  gIpCfg, yErrMsg);
    if (yStrTmp[0])
    {
		if(strcasecmp(yStrTmp, "transferredTCF") == 0)
		{
			gT38FaxRateManagement = 2;
		} 
        if(strcasecmp(yStrTmp, "localTCF") == 0)
        {
            gT38FaxRateManagement = 1;
        }
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gT38FaxMaxBuffer = 2000;

    yRc = get_name_value ("FAX", "T38FaxMaxBufferiSize", "2000", yStrTmp, 25,
                  gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		gT38FaxMaxBuffer = atoi(yStrTmp);
			if (gT38FaxMaxBuffer <= 0)
			{
				dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, WARN, 
					"Invalid value (%d) set for T38FaxMaxBufferiSize in %s.  Defaulting to 2000",
					gT38FaxMaxBuffer, gIpCfg);
				gT38FaxMaxBuffer = 2000;
			}
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));

	gT38FaxMaxDataGram = 512;

    yRc = get_name_value ("FAX", "T38FaxMaxDatagram", "512", yStrTmp, 25,
                  gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
			gT38FaxMaxDataGram = atoi(yStrTmp);
			if ( gT38FaxMaxDataGram <= 0 )
			{
				dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, WARN, 
					"Invalid value (%d) set for T38FaxMaxDatagram in %s.  Defaulting to 512.",
					gT38FaxMaxDataGram, gIpCfg);
				gT38FaxMaxDataGram = 512;
			}
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gUdptlStartPort = 4500;
    yRc = get_name_value ("FAX", "UdptlStartPort", "4500", yStrTmp, 25,
                  gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		if(strcmp(yStrTmp, "4500") != 0)
		{
			gUdptlStartPort = atoi(yStrTmp);
			if ( gUdptlStartPort <= 0 )
			{
				dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, WARN, 
					"Invalid value (%d) set for UdptlStartPort in %s.  Defaulting to 4500.",
					gUdptlStartPort, gIpCfg);
				gUdptlStartPort = 4500;
			}
		} 
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gUdptlEndPort = 5500;
    yRc = get_name_value ("FAX", "UdptlEndPort", "5500", yStrTmp, 25,
                  gIpCfg, yErrMsg);
	if (yStrTmp[0])
	{
		if(strcmp(yStrTmp, "5500") != 0)
		{
			gUdptlEndPort = atoi(yStrTmp);
			if ( gUdptlEndPort <= 0 )
			{
				dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, WARN, 
					"Invalid value (%d) set for UdptlEndPort in %s.  Defaulting to 5500.",
					gUdptlEndPort, gIpCfg);
				gUdptlEndPort = 5500;
			}
		} 
    }

    sprintf (yStrTmp, "%s", "\0");
    yRc = get_name_value ("FAX", "T38ErrorCorrectionEntries", "0", yStrTmp, 25,
                  gIpCfg, yErrMsg);

    if (yStrTmp[0])
    {
       if(strcasecmp(yStrTmp, "0") == 0){
         gT38ErrorCorrectionEntries = 0;
       } else
       if(strcasecmp(yStrTmp, "1") == 0){
         gT38ErrorCorrectionEntries= 1;
       } else
       if(strcasecmp(yStrTmp, "2") == 0){
         gT38ErrorCorrectionEntries = 2;
       } else {
         gT38ErrorCorrectionEntries = 0;
       }
    }

    memset((char *)yStrTmp, '\0', sizeof(yStrTmp));
	gSendFaxTone = 1;
    yRc = get_name_value ("FAX", "SendFaxTone", "1", yStrTmp, 5,
                  gIpCfg, yErrMsg);

	if (yStrTmp[0])
	{
		if(strcasecmp(yStrTmp, "0") == 0)
		{
			gSendFaxTone = 0;
		} 
    }

    if(progname != NULL)
    {
        if(strcmp(progname, "ArcSipRedirector") == 0)
        {
            ;
        }
        else
        {
            printFaxConfig(); //Print only for ArcIPDynMgr
        }
    }

	loadPlayBackControls();

    return (0);

}/*END:loadConfigFile */

int printFaxConfig()
{
	static int zCall=0;

   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gFaxDebug:%d", gFaxDebug);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT30FaxStationId:%s", gT30FaxStationId);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT30HeaderInfo:%s", gT30HeaderInfo);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT30ErrorCorrection:%d", gT30ErrorCorrection);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38Enabled:%d", gT38Enabled);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38Transport:%d", gT38Transport);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38ErrorCorrectionMode:%d", gT38ErrorCorrectionMode);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38ErrorCorrectionSpan:%d", gT38ErrorCorrectionSpan);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38ErrorCorrectionEntries:%d", gT38ErrorCorrectionEntries);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38MaxBitRate:%d", gT38MaxBitRate);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38FaxFillBitRemoval:%d", gT38FaxFillBitRemoval);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38FaxVersion:%d", gT38FaxVersion);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38FaxTranscodingMMR:%d", gT38FaxTranscodingMMR);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38FaxTranscodingJBIG:%d", gT38FaxTranscodingJBIG);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38FaxRateManagement:%d", gT38FaxRateManagement);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38FaxMaxBuffer:%d", gT38FaxMaxBuffer);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gT38FaxMaxDataGram:%d", gT38FaxMaxDataGram);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gUdptlStartPort:%d", gUdptlStartPort);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gUdptlEndPort:%d", gUdptlEndPort);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"gSendFaxTone:%d", gSendFaxTone);

	return(0);
} /* END: printFaxConfig() */

int printFaxByPort(int zCall)
{

   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_FaxDebug:%d", zCall, gCall[zCall].GV_FaxDebug);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T30FaxStationId:%s", zCall, gCall[zCall].GV_T30FaxStationId);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T30HeaderInfo:%s", zCall, gCall[zCall].GV_T30HeaderInfo);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T30ErrorCorrection:%d", zCall, gCall[zCall].GV_T38Enabled);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38Enabled:%d", zCall, gCall[zCall].GV_T38Enabled);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38Transport:%d", zCall, gCall[zCall].GV_T38Transport);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38ErrorCorrectionMode:%d", zCall, gCall[zCall].GV_T38ErrorCorrectionMode);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38ErrorCorrectionSpan:%d", zCall, gCall[zCall].GV_T38ErrorCorrectionSpan);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38ErrorCorrectionEntries:%d", zCall, gCall[zCall].GV_T38ErrorCorrectionEntries);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38MaxBitRate:%d", zCall, gCall[zCall].GV_T38MaxBitRate);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38FaxFillBitRemoval:%d", zCall, gCall[zCall].GV_T38FaxFillBitRemoval);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38FaxVersion:%d", zCall, gCall[zCall].GV_T38FaxVersion);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38FaxTranscodingMMR:%d", zCall, gCall[zCall].GV_T38FaxTranscodingMMR);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38FaxTranscodingJBIG:%d", zCall, gCall[zCall].GV_T38FaxTranscodingJBIG);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38FaxRateManagement:%d", zCall, gCall[zCall].GV_T38FaxRateManagement);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38FaxMaxBuffer:%d", zCall, gCall[zCall].GV_T38FaxMaxBuffer);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_T38FaxMaxDataGram:%d", zCall, gCall[zCall].GV_T38FaxMaxDataGram);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_UdptlStartPort:%d", zCall, gCall[zCall].GV_UdptlStartPort);
   dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, FAX_BASE, INFO, 
		"[%d].GV_UdptlEndPort:%d", zCall, gCall[zCall].GV_UdptlEndPort);

	return(0);
} /* END: printFaxConfig() */

int arg_switch(char *mod, int parameter, char *value)
{
	gResId = -1;

	switch(parameter)
	{
			
		case 's':	/*Start Port for this Dynamic Manager*/
		{
			sscanf(value, "%d", &gStartPort);
		}
		break;
		
		case 'e':	/*End Port for this Dynamic Manager*/
		{
			sscanf(value, "%d", &gEndPort);
		}
		break;

		case 'd':	/*ID number for this Dynamic Manager*/
		{
			sscanf(value, "%d", &gDynMgrId);
		}

		break;
		
		case 'R':	/*ID number for this Resp*/
		case 'r':	/*ID number for this Resp*/
		{
			sscanf(value, "%d", &gResId);
		}

		break;
		
		default:
			break;
	}

	return(0);
}/*END: int arg_switch */

int parse_arguments(char *mod, int argc, char *argv[])
{
        int c;
        int i;

        while( --argc > 0 )
        {
                *++argv;
                if(argv[1] && argv[1][0] != '-')
                {
                        arg_switch(mod, (*argv)[1], argv[1]);
                        *++argv;
                        --argc;
                }
                else
                        arg_switch(mod, (*argv)[1]," ");
        }

        return (0);

}/*END: int parse_arguments */

unsigned long mCoreAPITime()
{
	unsigned long t=0;
	struct timeval tm;
	int         st;
	st = gettimeofday(&tm, NULL);
	if (! st) 
	{   //success
		t = (unsigned long) (tm.tv_sec*1000 + tm.tv_usec/1000);    // in millisec
	} 
	else 
	{
		t = 0;
	}
	// for Linux RedHat 6.0, it is unknown why t was only 0.1 nanosec.
	return t;
}

void MSleep(unsigned long milsecs)
{
	struct timespec l_req, l_rem;
	int             l_ret;

	l_rem.tv_sec = milsecs / 1000;
	l_rem.tv_nsec= (milsecs % 1000)*1000000;
	do
	{
		l_req = l_rem;
	}
	while((l_ret = nanosleep(&l_req, &l_rem)) == -1 && errno == EINTR);

	if(l_ret == -1)
	{
	}
}

#if 0
int util_u_sleep(long microSeconds)
{
    struct timeval SleepTime;

    SleepTime.tv_sec = 0;
    SleepTime.tv_usec = microSeconds;

    select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &SleepTime);

    return(0);

}/*END: int util_sleep*/
#endif

int util_u_sleep(long microSeconds)
{
    struct timespec lTimeSpec1, lTimeSpec2;
	int rc;
	static long maxNanoseconds = 999999999;
	long tmpNanoSeconds;
	int		i;

	    lTimeSpec1.tv_sec = 0;

	if  ( ((long ) microSeconds * 1000) < maxNanoseconds )
	{
    		lTimeSpec1.tv_nsec = microSeconds * 1000; 
	}
	else
	{
		tmpNanoSeconds = microSeconds * 1000;
		if ( tmpNanoSeconds  > maxNanoseconds )
		{
			lTimeSpec1.tv_sec = lTimeSpec1.tv_sec + 1;

			tmpNanoSeconds = tmpNanoSeconds - 1000000000;
		}
		lTimeSpec1.tv_nsec = tmpNanoSeconds;
	}

	i = 0;
	while ( (rc=nanosleep (&lTimeSpec1, &lTimeSpec2)) == -1 )
	{
		lTimeSpec1 = lTimeSpec2;
	}

    return (0);

}/*END: int util_sleep*/

void arcDynamicSleep(unsigned long  dwMilsecs, unsigned long  *dwPrevSleepTimestamp, unsigned long  *dwExpectSleepTimestamp)
{
	unsigned long l_dwCurrTime;
	unsigned long l_dwDiffTime, l_dwDelayTime = 0, l_dwSleepTime;  // for periodic timer fine-tuning
	unsigned long dwMilsecs2 ;

//	util_u_sleep(dwMilsecs * 1000);
//	return;

	if (dwMilsecs == 0)
    {
        return;
    }


	 dwMilsecs2 = (dwMilsecs << 2) & 0x3fff;
    l_dwCurrTime = mCoreAPITime();
    *dwExpectSleepTimestamp += dwMilsecs;
    if ((*dwPrevSleepTimestamp - l_dwCurrTime) < dwMilsecs2)     // check in case l_dwCurrTime runs slower than dwPrevSleepTimestamp
    {
        l_dwDiffTime = 0;
    }
    else
    {
        l_dwDiffTime = l_dwCurrTime >= *dwPrevSleepTimestamp ? l_dwCurrTime - *dwPrevSleepTimestamp : (MAX_DWORD - *dwPrevSleepTimestamp + 1) + l_dwCurrTime;
    }
    if (l_dwDiffTime < dwMilsecs)
    {
        // hasn't missed the timeshot, delay.
        // if time difference is 0, we don't do MSleep() as we don't want to relinquish process.
        // However, if l_dwSleepTime is 0 due to time adjustment, we MSleep() to give way.
        l_dwSleepTime = dwMilsecs - l_dwDiffTime; // l_dwSleepTime at this point always > 0.
        // adjust sleep time
        if (l_dwCurrTime >= *dwExpectSleepTimestamp)     // we ignore the case of max value loopover as it is not critical
        {
            // we may lag behind schedule
            l_dwSleepTime--;
            if ((l_dwCurrTime ^ *dwExpectSleepTimestamp) && l_dwSleepTime)
            {
                l_dwSleepTime--;
            }
            if (l_dwDelayTime > l_dwSleepTime)      // always l_dwSleepTime >= 0
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
            if (l_dwDelayTime > l_dwSleepTime)      // try to shorten l_dwSleepTime once more.  always l_dwSleepTime >= 0
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
            l_dwSleepTime++;    // add extra 1msec delay
		}
	}
	else
	{
		l_dwSleepTime = 0;
	}

	if (l_dwSleepTime > 0)
	{
        util_u_sleep(l_dwSleepTime * 1000);

      #if defined(_DEBUG)
        l_dwDiffTime = mCoreAPITime() - l_dwCurrTime;
        if ( (l_dwDiffTime > l_dwSleepTime*2) && (l_dwSleepTime >= 10) )
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


#if 0 
 /*ORTP*/ char offset127 = 127;
char offset0xD5 = 0xD5;
char offset0 = 0;

PayloadType pcmu8000 = {
	PAYLOAD_AUDIO_CONTINUOUS,
	8000,
	1.0,
	&offset127,
	1,
	64000,
	"PCMU/8000/1"
};

PayloadType pcma8000 = {
	PAYLOAD_AUDIO_CONTINUOUS,
	8000,
	1.0,
	&offset0xD5,
	1,
	64000,
	"PCMA/8000/1"
};

PayloadType pcm8000 = {
	PAYLOAD_AUDIO_CONTINUOUS,
	8000,
	2.0,
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

/*END: ORTP*/

#endif 


#if 0
/////////////////////////////////////////////////////////////////////////////////////////////////////
//
//arc_StreamBuffer Implementation
//



arc_StreamBuffer::arc_StreamBuffer(){

	currentLocation = 0;
	stream = NULL;
	sizeOfStream = 0;
}

arc_StreamBuffer::arc_StreamBuffer(char *zpStream)
{
	currentLocation = 0;
	stream = NULL;
	sizeOfStream = 0;

	if(zpStream!= NULL)
	{
        // this will always be four or eight bytes is that what we wanted?
        // or do we want strlen of zpStream ? or pass in the size explicitly ?
		sizeOfStream = sizeof(zpStream);
		stream = (char *)malloc(sizeOfStream);
		memcpy(stream, zpStream, sizeof(zpStream));
	}
}

arc_StreamBuffer::~arc_StreamBuffer()
{
	if( stream!= NULL)
	{
		free(stream);/*DDN_QUESTION:When this get freed ?*/
		/*RG done*/
	}
	currentLocation = 0;
	stream = NULL;
	sizeOfStream = 0;
}

int arc_StreamBuffer::setStreamBuffer(char *zpStream, long lsizeOfStream)
{
	currentLocation = 0;
	sizeOfStream = 0;

	/*DDN: Added since -998 was getting here.*/
	if(lsizeOfStream < 0 || lsizeOfStream > 4096)
	{
		return (-1);
	}

	if(zpStream!= NULL)
	{
		sizeOfStream = lsizeOfStream;
		stream = (char *)malloc(sizeOfStream);
		//memset(stream, 0, sizeOfStream);
		memcpy(stream, zpStream, sizeOfStream);
		return 0;
	}

	return -1;
}


long arc_StreamBuffer::appendStream(char* zpStream, long lsizeOfStream)
{
	if(zpStream != NULL && lsizeOfStream >= 0 && lsizeOfStream < 4097)
	{
		if(sizeOfStream == 0)
		{
			sizeOfStream = lsizeOfStream;
			stream = (char *)malloc(sizeOfStream);
			memcpy(stream, zpStream, sizeOfStream);
			return 0;
		}
		else
		{
			long totalreallocSize = sizeOfStream + lsizeOfStream;
			stream = (char *)realloc(stream , totalreallocSize);
			stream += sizeOfStream;
			sizeOfStream += lsizeOfStream;
			memcpy(stream, zpStream, lsizeOfStream);
			stream -= (sizeOfStream - lsizeOfStream);
			return 0;
		}
	}
	else
	{
		return -1;
	}

	return 0;
}


int arc_StreamBuffer::resetStreamBuffer()
{
	currentLocation = 0;
	sizeOfStream = 0;

	if(stream!= NULL)
	{
		free(stream);
		stream = NULL;
		return 0;
	}

	return -1;
}

long arc_StreamBuffer::Read(char *buffer, long size, int zPrintMessages)
{
	if(stream!= NULL)
	{
		if( (currentLocation + size) <= sizeOfStream)
		{
			long j = 0;
			for(long i = currentLocation; i< currentLocation + size; i++, j++)
			{
				buffer[j] = stream[i];
			}
			currentLocation += size;

			return j;
		}
		else
		{
			long j = 0;
			for(long i = currentLocation; i< sizeOfStream; i++, j++)
			{
				buffer[j] = stream[i];
			}
			currentLocation = sizeOfStream;
			return j;

		}
	}
	return -1;
}

//int arc_StreamBuffer::freePlayedStreamedBuffer(long playedDataLength)
//{
//}


int arc_StreamBuffer::Seek(long endPoint, long startPoint)
{
	if(stream!= NULL)
	{
		currentLocation = startPoint + endPoint;
		
		if(currentLocation > sizeOfStream)
			currentLocation = sizeOfStream;
		
		if(currentLocation <= sizeOfStream)
			return 0;
		else
			return -1;	
	}
	else
		return -1;	
}
#endif

int write_headers_to_file(int zCall, osip_message_t *msg)
{
	int idx = 0;
  	int rc = 0;
  	int count = 0;
  	osip_header_t *hdr = NULL;
  	char _buff[1024 * 10];
	char _name_value[1024];

	FILE *fp = NULL;
	char filename[64] = "";

	if(!msg)
		return -1;

	if(!gHeaderListArray[0][0])
		return -1;

	_buff[0] = 0;
	_name_value[0] = 0;

	for(idx=0; 	gHeaderListArray[idx][0]; idx++)
	{
  		osip_message_header_get_byname(msg, gHeaderListArray[idx], count, &hdr);

  		if(hdr && hdr->hvalue)
		{
			sprintf(_name_value, "%s=%s\n", gHeaderListArray[idx], hdr->hvalue);

			strcat(_buff, _name_value);

			_name_value[0] = 0;
		}
		else
		{
			sprintf(_name_value, "%s=%s\n", gHeaderListArray[idx], "undefined");

/*
			if(getenv("DDN_UUI"))
			{
				sprintf(_name_value, "%s=%s\n", gHeaderListArray[idx], getenv("DDN_UUI"));
			}
*/

			strcat(_buff, _name_value);

			_name_value[0] = 0;
		}

	} //for

	sprintf(filename, "/tmp/invite.headers.%d", zCall);
	fp = fopen(filename, "w");
	if(fp == NULL)
	{
		return -1;
	}
	
	fwrite(_buff, 1, strlen(_buff), fp);

	fclose(fp);

	return 0;

} // int write_headers_to_file

int lookup_host (const char *host)
{
	struct addrinfo hints, *res;
	int errcode;
	char addrstr[100];
	void *ptr;
	char *yStrSemiColonPtr;

    char yStrUrlToParse[256];
    char yStrHost[256];

#if 0
    osip_uri_t *sipDest = NULL;
#endif

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;

	if(!host || !host[0])
	{
		return (-1);
	}

	sprintf(yStrUrlToParse, "%s", host);


	yStrSemiColonPtr = strchr(yStrUrlToParse, ';');

	if(yStrSemiColonPtr)
	{
		yStrSemiColonPtr[0] = 0;
	}

	sprintf(yStrHost, "%s", yStrUrlToParse);

#if 0
    if( osip_uri_init(&sipDest) == -1)
    {
        return -1;
    }

//fprintf(stderr, "%s %d Dest(%s) to parse\n", __FILE__, __LINE__, yStrUrlToParse);

    if( osip_uri_parse(sipDest, yStrUrlToParse) == -1)
    {
		if(sipDest)
		{
			osip_uri_free(sipDest);
			sipDest = NULL;
		}

//fprintf(stderr, "%s %d Dest(%s) failed to parse\n", __FILE__, __LINE__, yStrUrlToParse);

		return -1;
	}

	sprintf(yStrHost, "%s", sipDest->host);
#endif

	errcode = getaddrinfo (yStrHost, NULL, &hints, &res);

	if (errcode != 0)
	{
		return -1;
	}

	while (res)
	{
		inet_ntop (res->ai_family, res->ai_addr->sa_data, addrstr, 100);

		switch (res->ai_family)
		{
			case AF_INET:
				ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
				break;
			case AF_INET6:
				ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
				break;
		}

		inet_ntop (res->ai_family, ptr, addrstr, 100);
		res = res->ai_next;
	}

	return 0;
}


int isItFreshDtmf (int zCall, struct timeval zTimeb)
{
    char mod[] = { "isItFreshDtmf" };

    long yLongLastDtmfTime = (gCall[zCall].lastDtmfTime.tv_sec * 1000) + gCall[zCall].lastDtmfTime.tv_usec + 250;
    long yLongCurrDtmfTime = (zTimeb.tv_sec * 1000) + zTimeb.tv_usec;

    if (yLongLastDtmfTime > yLongCurrDtmfTime)
    {

        char yStrTmpBuf[256];

        sprintf (yStrTmpBuf, "Repeated DTMF (%ld > %ld),  ignoring", yLongLastDtmfTime, yLongCurrDtmfTime);

        return (0);
    }

    return (1);

}/*END: int isItFreshDtmf */


// JOES utilities to set cross references for gCall arrays
// we may want to pass a type as well so that we can differentiate 

int 
gCallSetCrossRef(Call *gCall, size_t size, int zIndex, Call *ref, int zCall){

   int rc  = -1;

   int idx;
   int parent;

   if(zIndex < 0 || zIndex >= MAX_PORTS){
      return -1;
   }
  
   if(zCall < 0 || zCall >= MAX_PORTS){
       return -1;
   }

   if(!ref){
    return -1;
   }

   //fprintf(stderr, " %s() JOES ZCALL = %d\n", __func__, zCall);

   parent = gCall[zIndex].parent_idx;

   if(parent < 0 || parent > MAX_PORTS){
//       fprintf(stderr, " %s() invalid parent index=%d\n", __func__, parent);
       return -1;
   }
   
   //fprintf(stderr, " %s() JOES PARENT INDEX =%d\n", __func__, parent);

   idx = zCall % SPAN_WIDTH;

   if(idx >= 0 && idx <= SPAN_WIDTH){
     gCall[parent].crossRef[idx] = ref;
     rc = 0;
   }


   return rc;
}

int 
gCallDelCrossRef(Call *gCall, size_t size, int zIndex, int zCall)
{
   int rc = -1;

   int idx;
   int parent;

   if(zIndex < 0 || zIndex >= MAX_PORTS){
      return -1;
   } 


   if(zCall < 0 || zCall >= MAX_PORTS){
      return -1;
   }

   parent = gCall[zCall].parent_idx;	


#if 0
   if(parent >= 0 && parent < MAX_PORTS){
       // fprintf(stderr, " %s() invalid parent index=%d\n", __func__, parent);
       return -1;
   }
#endif

   // fprintf(stderr, " %s() JOES PARENT INDEX =%d\n", __func__, parent);

   if(parent < 0 || parent >= MAX_PORTS)
   {
      dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_DETAIL,
			TEL_INVALID_INTERNAL_VALUE, WARN,
            " Third Party Bridge - Parent index is out of Range, index=%d.", parent);
      return -1;
   }

   idx = zCall % SPAN_WIDTH;

   if(idx < 0 || idx >= SPAN_WIDTH){
      return -1;
   } 

   gCall[parent].crossRef[idx] = NULL;

   rc = 0;

   return rc;
}


void 
gCallDumpCrossRefs(Call *gCall, int zCall){

  int i;
  int parent;

  // END DEBUGGING
  return;

  if(gCall[zCall].parent_idx != zCall){
     parent = gCall[zCall].parent_idx;
  } else {
     fprintf(stderr, "%s(%d) crossport=%d\n", __func__, zCall, gCall[zCall].crossPort);
     return;
  } 

  fprintf(stderr, "%s(%d) crossport=%d cross ref map = ", __func__, zCall, gCall[zCall].crossPort);

  for(i = 0; i < SPAN_WIDTH; i++){

   if(gCall[parent].crossRef[i] != NULL && gCall[parent].crossRef[i] != &gCall[i]){
     fprintf(stderr, " %s() ERROR PTR != RIGHT PTR !!\n", __func__);
   }
  
   if(gCall[parent].crossRef[i]){
      fprintf(stderr, "|%2d", i);
   } else {
     fprintf(stderr, "|**");
   } 
  }

  fprintf(stderr, "|\n");
  return;

}

// 
// should return actual index of gCall structure for call 
// 

#if 0

// moved to the call manager 
int 
gCallFindThirdLeg(Call *gCall, int size, int myleg, int crossport){

   int rc = -1;
   int i;
   int idx, xport, parent;

   if(myleg < 0 || crossport < 0){
     return -1;
     //fprintf(stderr, " %s() invalid leg or crossport leg=%d crossport=%d\n", __func__, myleg, crossport);
   }
 
   parent = gCall[myleg].parent_idx; 
   idx = myleg % SPAN_WIDTH;
   xport = crossport % SPAN_WIDTH;

   gCallDumpCrossRefs(gCall, parent);

   for(i = 0; i < SPAN_WIDTH; i++){
      // is set 
      if(gCall[parent].crossRef[i] != NULL &&
        i != idx && 
        i != xport){
           if(gCall[parent].crossRef[i] == &gCall[i]){
             rc = i;
           }
           // fprintf(stderr, " %s() found third leg [%d] for leg [%d] and crossport [%d] address=%p\n", __func__, i, myleg, crossport, gCall[parent].crossRef[i]);
           break;
      }
   }

   return rc;
}

#endif 


