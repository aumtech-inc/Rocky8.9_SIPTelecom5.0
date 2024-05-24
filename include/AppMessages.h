/*----------------------------------------------------------------------------
File:		AppMessages.h
Purpose:	This file defines structures used to pass messages between 
			the dynamic manager and the APIs running in an application.
Author:		
Base Code:	dev@omnix:/home/dev/isp2.2/IPTelecom/1.0/include/AppMessages.h
Date:		Wed Aug  7 12:55:22 EDT 2002

Update:	09/18/2002 ddn added #define DMOP_EVENT 9000
Update:	10/02/2002 ddn added #define DMOP_REJECTCALL 
Update:	10/02/2002 ddn added struct Msg_RejectCall
Update:	10/06/2002 ddn added #define DMOP_SPEAK_QUEUE_SYNC		24
Update:	10/06/2002 ddn added #define DMOP_SPEAK_QUEUE_ASYNC		25
Update:	10/17/2002 ddn added #define VENDOR_DEFAULT_BEEP		"\\VENDOR_DEFAULT"
Update:	11/01/2002 apj Added ttsKey to Msg_Speak.
Update:	11/07/2002 apj Added Set/Get global string structures.
Update:	11/12/2002 mpb Added request/release port defines.
Update:	11/13/2002 mpb Added structure for request port.
Update:	12/09/2002 ddn Added structure for send info element
Update:	12/09/2002 mpb Added structure for ocs app
Update:	01/09/2003 ddn Added DMOP_GC_CLOSE, DMOP_GC_OPEN
Update:	01/29/2003 mpb Added data field to Msg_NewCall.
Update:	02/10/2003 ddn Added Msg_AscToDM, Msg_StreamClient
Update:	02/10/2003 ddn Added Msg_DMToAsc, Msg_AppToAsc
Update:	02/17/2003 ddn Added DMOP_SUSPENDCALL and DMOP_RESUMECALL
Update:	02/22/2003 ddn Added DMOP_RESACT_STATUS
Update:	03/06/2003 apj Changed MAX_APP_MESSAGE_DATA from 230 to 236.
Update:	03/14/2003 ddn Changed Msg_AppToAsc to accomodate Msg_Speak options
Update:	03/20/2003 ddn Changed ttsKey to key since Streaming Client uses it.
Update:	03/20/2003 ddn Added async to Msg_AppToAsc.
Update:	03/21/2003 apj Added informat to Msg_InitiateCall.
Update:	04/09/2003 mpb Added DMOP_STARTFAXAPP & RESOP_START_FAX_APP.
Update:	04/09/2003 ddn Added struct Msg_StartFaxApp
Update:	04/22/2003 ddn Added deleteFile to Msg_Speak(Ref:Delete put_queued tts)
Update:	04/24/2003 apj Added Msg_SRGetXMLResult.
Update:	05/13/2003 apj Added vxml fifo related defines.
Update:	05/23/2003 apj Added MsgVxmlToResp.
Update:	05/28/2003 apj In Msg_ReservePort, changed portRequests size to fit 256
Update:	05/28/2003 apj In MsgToRes, changed ringEventTime size from 17 to 40.
Update:	06/03/2003 ddn #defined subscriber, message, node and session ids
Update:	06/03/2003 ddn #defined dictaiontime and request type
Update:	06/03/2003 ddn Removed stream server members from Msg_AppToAsc
					They will be extracted from file name at stream client
Update:	06/10/2003 apj Added grammarType to Msg_SRLoadGrammar struct.
Update:	06/12/2003 ddn Added fromProtocol and toProtocol to Msg_DMToDM
Update:	07/01/2003 apj Added allParties to Msg_Record.
Update:	07/22/2003 apj Changed value in Msg_SetGlobalString from 100 to 150.
Update:	09/22/2003 apj Added language to Msg_InitTelecom.
Update:	10/20/2003 ddn Added Following fields to MsgFromDmToDm
								int callingPortNetworkSlot;
								int callingPortVoiceSlot;
Update:	10/28/2003 ddn Added DMOP_GC_MAKECALL, DMOP_[A|B]_LEG_DISCONNECTED
Update: 01/15/2004 DDN Added SCOP  (Streaming client opcodes)
Update: 03/09/2004 APJ Added defines for VXML2 fork.
Update:	03/26/2004 ddn Added DMOP_DROP_B_LEG
Update: 06/22/2004 DDN defineed DMOP_RESET_PORT             999
Update: 11/22/2004 APJ Added DMOP_MODIFY_OUTBOUND_RET_CODE.
Update: 12/09/2004 APJ Added DMOP_TIMER_THREAD_HBIT.
Update: 01/28/2005 djb Changed the size of data field in the MsgFromDMToDM
                       from 32 to 64
Update: 02/17/2005 djb Added DMOP_TEC_STREAM
Update: 03/11/2005 djb Added DMOP_TDX_PLAY
Update: 04/06/2005 djb Added DMOP_GETDTMF_GETDIG and Msg_GetDtmfGetDig structure
Update: 04/13/2005 djb Added DMOP_DX_PLAYIOTTDATA
Update: 04/14/2005 djb Removed DMOP_TDX_PLAY and DMOP_DX_PLAYIOTTDATA
Update: 08/11/2005 djb Added DMOP_SRUNLOADGRAMMAR 
Update: 08/11/2005 djb Added grammarName to Msg_SRUnloadGrammar structure.
Update: 08/25/2005 djb Added DMOP_SPEECHDETECTED #define.
Update: 08/26/2005 djb Added Msg_SpeechDetected structure.

Update:	10/10/2003 apj Changed MAX_APP_MESSAGE_DATA  from 230->236.
Update: 03/08/2004 APJ Added defines for VXML2 fork.
Update: 03/30/2004 APJ Added Msg_SRLogEvent struct.
Update: 03/30/2004 APJ Added DMOP_SRLOGEVENT define.
Update:	03/31/2004 mpb Added MsgVxmlToResp.
Update:	11/23/2004 ddn Added DMOP_SETDTMF,DMOP_RTPDETAILS,DMOP_TRANSFERCALL
Update:	03/09/2005 djb Added deleteFile to Msg_Speak structure.
Update:	03/25/2005 djb Removed RESOP_VXML_PID  and DMOP_GETGLOBAL duplicates.
Update:	04/26/2005 ddn Added streaming related structures
Update: 08/11/2005 djb Added DMOP_SRUNLOADGRAMMAR
Update: 08/11/2005 djb Added grammarName to Msg_SRUnloadGrammar structure.
Update: 08/26/2005 djb Added DMOP_SPEECHDETECTED #define.
Update: 08/26/2005 djb Added Msg_SpeechDetected structure.
Update:	02/01/2003 rg  Added struct Msg_AscToDM.
Update:	02/01/2003 rg  Added struct Msg_DMToAsc.
Update:	02/01/2003 rg  Added struct Msg_StreamAudio.
Update: 05/02/2006 djb Removed duplicates: DMOP_GETGLOBAL,
                       DMOP_SRRECOGNIZEFROMCLIENT, DMOP_SRPROMPTEND, and
                       DMOP_SRDTMF.
Update: 09/12/2007 djb Added DMOP_SPEAKMRCPTTS and other MRCPTTS defines.
Update: 02/12/2009 djb Added Call Progress info
Update: 12/08/2009 rg  Added Msg_BridgeThirdLeg and DMOP_BRIDGE_THIRD_LEG
Update: 06/22/2010 djb Added Msg_BlastDial
Update: 11/10/2012 djb Added Network Transfer defines
----------------------------------------------------------------------------*/ 
#ifndef APP_MESSAGES_H
#define APP_MESSAGES_H

#define MAX_APP_MESSAGE_DATA		236
#define NULL_APP_CALL_NUM			-99
#define NULL_PASSWORD				-1
#define MAX_RES_DNIS				32	
#define MAX_RES_ANI					32
#define MAX_RES_DATA				128

/*Streaming client OPCODES*/
#define SCOP_CLOSE_SESSION          1
#define SCOP_CLOSE_SOCKET           2
#define SCOP_CLOSE_ALL              3
#define SCOP_STOP_DOWNLOAD          4
#define SCOP_STOP_CLIENT            5
#define SCOP_DOWNLOAD               6
#define SCOP_DOWNLOAD_FIRST_TIME    7

#define RESOP_FIREAPP    			101
#define RESOP_DISCONNECT 			102
#define RESOP_PORT_RESPONSE_PERM 	104
#define RESOP_STATIC_APP_GONE 		105
#define RESOP_GET_APP_TO_OVERLAY 	106
#define RESOP_REQUEST_PORT 			107
#define RESOP_RELEASE_PORT 			108
#define RESOP_START_OCS_APP 		109
#define RESOP_START_FAX_APP 		110
#define RESOP_VXML_PID 				111
#define RESOP_VXML2_PID 			112
#define RESOP_HEARTBEAT             113
#define RESOP_CHECK_CDF             114

#define RESOP_START_NTOCS_APP       115
#define RESOP_START_NTOCS_APP_DYN   116


#define DMOP_SPEAK 					1
#define DMOP_OUTPUTDTMF 			2
#define DMOP_STOPACTIVITY 			3
#define DMOP_STOPALLACTIVITY 		4
#define DMOP_GETDTMF 				5
#define DMOP_INITTELECOM 			6
#define DMOP_INITIATECALL 			7
#define DMOP_ANSWERCALL 			8
#define DMOP_BRIDGECONNECT 			9
#define DMOP_CLEARDTMF 				10
#define DMOP_DROPCALL 				11
#define DMOP_RECORD 				12
#define DMOP_DISCONNECT 			13
#define DMOP_APPDIED 				16
#define DMOP_CANTFIREAPP 			17
#define DMOP_PORTOPERATION 			18
#define DMOP_SPEECHREC 				19
#define DMOP_DIALEXTENSION 			20
#define DMOP_PORT_REQUEST_PERM 		21
#define DMOP_EXITTELECOM 			22
#define DMOP_REJECTCALL 			23
#define DMOP_SETGLOBALSTRING 		24
#define DMOP_GETGLOBALSTRING 		25
#define DMOP_SETGLOBAL				26
#define DMOP_GETGLOBAL				27
#define DMOP_PORTREQUEST			28
#define DMOP_SENDINFOELEMENTS       29
#define DMOP_STARTOCSAPP            30
#define DMOP_STARTFAXAPP            31
#define DMOP_RECORDMULTIPLE         32
#define DMOP_SPEAK_END				33
#define DMOP_SPEAK_CLEAR_QUEUE		34
#define DMOP_PLAYMEDIA				35
#define DMOP_RECORDMEDIA			36
#define DMOP_PLAYMEDIAVIDEO       	37
#define DMOP_PLAYMEDIAAUDIO			38

#define DMOP_TRANSFERCALL			40
#define DMOP_SETDTMF				41
#define DMOP_RTPDETAILS				42
#define DMOP_OUTPUTDTMF_SECOND_PARTY	43
#define DMOP_PROGRESSING_RECEIVED	44
#define DMOP_VIDEODETAILS			45

#define DMOP_STARTNTOCSAPP          46
#define DMOP_SENDMSGTOAPP           47

#define DMOP_SRADDWORDS 			50
#define DMOP_SRDELETEWORDS 			51
#define DMOP_SREXIT 				52
#define DMOP_SRGETPARAMETER 		53
#define DMOP_SRGETPHONETICSPELLING 	54
#define DMOP_SRGETRESULT 			55
#define DMOP_SRINIT 				56
#define DMOP_SRLEARNWORD 			57
#define DMOP_SRLOADGRAMMAR 			58
#define DMOP_SRRECOGNIZE 			59
#define DMOP_SRRELEASERESOURCE 		60
#define DMOP_SRRESERVERESOURCE 		61
#define DMOP_SRSETPARAMETER 		62
#define DMOP_SRUNLOADGRAMMARS 		63
#define DMOP_SRGETXMLRESULT 		64
#define DMOP_TTSSTART 				68
#define DMOP_TTSEND 				69
#define DMOP_SRLOGEVENT 			70
#define DMOP_SRUNLOADGRAMMAR        71
#define DMOP_SENDFAX 				72
#define DMOP_RECVFAX 				73
#define DMOP_NEW_CALL 				75
#define DMOP_HANGUP 				76
#define DMOP_BLASTDIAL              77
#define DMOP_TTSRELEASERESOURCE     78

#define GEMOP_NEW_CALL_RESPONSE 	80
#define GEMOP_TRANSFER_BLIND 		81
#define GEMOP_DROP_CALL 			82

#define DMOP_GC_CLOSE				83
#define DMOP_GC_OPEN				84

#define	DMOP_SUSPENDCALL			85
#define	DMOP_RESUMECALL				86

#define DMOP_RESACT_STATUS 			87
#define DMOP_BLOCKED 				88
#define DMOP_UNBLOCKED 				89
#define DMOP_GC_MAKECALL			90
#define DMOP_A_LEG_DISCONNECTED		92
#define DMOP_B_LEG_DISCONNECTED		93
#define DMOP_INTERNAL_STOP_APP_THRERAD              94
#define  DMOP_DROP_B_LEG             95
#define DMOP_GETDTMF_GETDIG			96

#define DMOP_SRRECOGNIZEFROMCLIENT  100
#define DMOP_SRPROMPTEND            101
#define DMOP_SRDTMF                 102
#define DMOP_STOPCH_SENDING         103
#define DMOP_SPEECHDETECTED         104

#define DMOP_HEARTBEAT				105

#define DMOP_SPEAKMRCPTTS           115
#define DMOP_START_TTS              116
#define DMOP_HALT_TTS_BACKEND       117
#define DMOP_TTS_COMPLETE_SUCCESS   118
#define DMOP_TTS_COMPLETE_FAILURE   119
#define DMOP_TTS_COMPLETE_TIMEOUT   120

#define DMOP_START_CALL_PROGRESS                121
#define DMOP_START_CALL_PROGRESS_RESPONSE       122
#define DMOP_END_CALL_PROGRESS                  123
#define DMOP_CANCEL_CALL_PROGRESS               124
#define DMOP_FAX_PROCEED                        125
#define DMOP_FAX_COMPLETE                       126
#define DMOP_FAX_CANCEL                         127
#define DMOP_FAX_SEND_SDPINFO                   128
#define DMOP_FAX_RESERVE_RESOURCE				129
#define DMOP_FAX_RELEASE_RESOURCE				130
#define DMOP_FAX_PLAYTONE						131
#define DMOP_FAX_STOPTONE						132
#define DMOP_STOPTONE						    132
/* this is added to alert a blocking API to a call connected situation */ 
/* joe S */
#define DMOP_CALL_CONNECTED					    133 	
#define DMOP_BRIDGE_CONNECTED					134 	
// MR 4655
#define DMOP_CALL_INITIATED						135
// END: MR 4655

#define DMOP_AIINIT					140
#define DMOP_AIPROCESS_CONTENT		141
#define DMOP_AIEXIT					142

#define	DMOP_TIMER_THREAD_HBIT		200
#define	DMOP_TEC_STREAM				201
#define DMOP_START_INBOUNDTHREAD	202
#define DMOP_START_VIDEOINBOUNDTHREAD	203
#define DMOP_START_OUTBOUNDTHREAD	204
#define DMOP_START_VIDEOOUTBOUNDTHREAD	205


/* FindMe */
#define DMOP_FM_CONNECT         	210
#define DMOP_FM_SEND_DATA       	211
#define DMOP_FM_CONNECT_SPANS   	212
#define DMOP_FM_DISCONNECT     		213
#define DMOP_LISTENCALL         214


#define	DMOP_FX_CLOSE				215

#define DMOP_MEDIACONNECT			216

#define DMOP_STARTRECORDSILENTLY    217
#define DMOP_STOPRECORDSILENTLY     218

#define DMOP_BRIDGE_THIRD_LEG		219

#define	DMOP_SHUTDOWN				250

#define DMOP_REREGISTER				300	

#define DMOP_GETPORTDETAILFORANI    996
#define DMOP_RTPPORTDETAIL          997

// 400 level for now is conferencing 
#define   DMOP_CONFERENCE_START 400
#define   DMOP_CONFERENCE_START_RESPONSE 401
#define   DMOP_CONFERENCE_ADD_USER 402 
#define   DMOP_CONFERENCE_ADD_USER_RESPONSE 403
#define   DMOP_CONFERENCE_REMOVE_USER 404
#define   DMOP_CONFERENCE_REMOVE_USER_RESPONSE 405 
#define   DMOP_CONFERENCE_STOP 406
#define   DMOP_CONFERENCE_STOP_RESPONSE 407 
#define   DMOP_CONFERENCE_SEND_DATA 408 
#define   DMOP_CONFERENCE_PLAY_AUDIO 409
#define   DMOP_CONFERENCE_PLAY_AUDIO_RESPONSE 410
#define   DMOP_INSERTDTMF 411
// end conference defines 

#ifdef VOICE_BIOMETRICS
#define   DMOP_VERIFY_CONTINUOUS_SETUP			600
#define   DMOP_VERIFY_CONTINUOUS_GET_RESULTS	601
#endif // VOICE_BIOMETRICS

#define DMOP_SENDVFU             	998

#define DMOP_RESET_PORT             999

/*Streaming client OPCODES*/
#define SCOP_CLOSE_SESSION          1
#define SCOP_CLOSE_SOCKET           2
#define SCOP_CLOSE_ALL              3
#define SCOP_STOP_DOWNLOAD          4
#define SCOP_STOP_CLIENT            5
#define SCOP_DOWNLOAD               6
#define SCOP_DOWNLOAD_FIRST_TIME    7

/*Redirection manager OPCODES*/
#define RMOP_REGISTER				300	
#define RMOP_DEREGISTER				301	

#define DMOP_STOPREGISTER			500	
#define DMOP_STARTEGISTER			501	

#define DMOP_RESTART_OUTPUT_THREAD 502

#define VENDOR_DEFAULT_BEEP			"\\VENDOR_DEFAULT"

#define ASC_DIR                 "/tmp"
#define ASC_MainFifo            "ArcStreamingClientMain"
#define ASC_DMFifo              "ArcStreamingClientDM"
#define ASC_SUBSCRIBERID        "subscriberid"
#define ASC_MESSAGEID           "msgid"
#define ASC_NODEID              "nodeid"
#define ASC_SESSIONID           "session"
#define ASC_REQUESTTYPE         "type"
#define ASC_DICTATIONTIME       "dictationtime"
#define ASC_DESTCODEC           "codec"
#define ASC_CODEC           ASC_DESTCODEC

#define ASC_VIDEO_MESSAGEID     "vmsgid"
#define ASC_VIDEO_NODEID        "vnodeid"
#define ASC_VIDEO_DICTATIONTIME "vdictationtime"
#define ASC_VIDEO_DESTCODEC     "vcodec"
#define ASC_VIDEO_CODEC     ASC_VIDEO_DESTCODEC

#define ASC_VIDEOMESSAGEID		"vmsgid"
#define ASC_VIDEONODEID			"vnodeid"
#define ASC_VIDEODICTATIONTIME	"vdictationtime"
#define ASC_VIDEOCODEC       	"vcodec"



#define VENDOR_DEFAULT_BEEP			"\\VENDOR_DEFAULT"

#define RESP_TO_VXML_FIFO "/tmp/RespToVxmlFifo"
#define RESP_FROM_VXML_FIFO "/tmp/ResFromVxmlFifo"

#define RESP_TO_VXML2_FIFO "/tmp/RespToVxml2Fifo"
#define RESP_FROM_VXML2_FIFO "/tmp/ResFromVxmlFifo"



// conferencing application messages 
//

/*
 * DMOP_CONFERENCE_START:
 * This message will go from media manager to conf. manager when app calls
 * TEL_ConfereceStart API
*/

typedef struct Msg_Conf_Start
{
  int opcode;                   //DMOP_CONFERENCE_START
  int appCallNum;
  int appPid;
  int appRef;
  int appPassword;
  char confId[64];              //Unique conf id genereated by media mgr e.g. CDR Key
} MSG_CONF_START;


/*
 * DMOP_CONFERENCE_START_RESPONSE:
 * This message will go rom conf manager to media manager as a result of
 * DMOP_CONFERENCE_START message.
 * Conference manager will try to create a new conference using aculab API.
*/

typedef struct Msg_Conf_Start_Response
{
  int opcode;                   //DMOP_CONFERENCE_START_RESPONSE 
  int appCallNum;
  int appPid;
  int appRef;
  int appPasswd;
  int resultCode;               // 0 Success, -1 Error
  char confId[64];              //Conf Id supplied in conf_start message
  char error[128];
} MSG_CONF_START_RESPONSE;


typedef struct Msg_Conf_Send_Data
{
  int opcode;                   //DMOP_CONFERENCE_ADD_USER
  int appCallNum;                  //Call Number of the user to be added.
  int appPid;
  int appRef;
  int appPasswd;
  char confId[64];              //Alredy existing conf id.
  char confOpData[128];
} MSG_CONF_SEND_DATA;


/*
 * DMOP_CONFERENCE_ADD_USER:
 * This message will go from media manager to conf. manager when app calls
 * TEL_ConfereceAddUser API.
 * rtpPort and codec info will be communicated using this message.
*/

typedef struct Msg_Conf_Add_User
{
  int opcode;                   //DMOP_CONFERENCE_ADD_USER
  int appCallNum;                  //Call Number of the user to be added.
  int appPid;
  int appRef;
  int appPasswd;
  int rtpPort;                  //rtp port where media manager will receive the mixed audio
  int codec;                    //0:711U, 8:711A, 18:729...
  char confId[64];              //Alredy existing conf id.
  int isBeep;
  char beepFile[96];
  int duplex;
  char remoteIP[32];
} MSG_CONF_ADD_USER;

/*
 * DMOP_CONFERENCE_ADD_USER_RESPONSE:
 * This message will go from conf manager to media manager as a result of
 * DMOP_CONFERENCE_ADD_USER message.
 * Conference manager will try to add a new user to the conf using aculab
API.
*/



typedef struct Msg_Conf_Add_User_Response
{
  int opcode;                   //DMOP_CONFERENCE_ADD_USER_RESPONSE
  int appCallNum;
  int appPid;
  int appRef;
  int appPasswd;
  int resultCode;               //0 for success  -1 for error.
  int rtpPort;                  //  RTP Port where media manager will send the audio
  char confId[64];              //Existing conf id
  char error[128];
} MSG_CONF_ADD_USER_RESPONSE;



/*
 * DMOP_CONFERENCE_REMOVE_USER:
 * This message will go from media manager to conf. manager when app calls
 * TEL_ConfereceRemoveUser API
*/

typedef struct Msg_Conf_Remove_User
{
  int opcode;                   //DMOP_CONFERENCE_REMOVE_USER
  int appCallNum;                  //Call Number of the user to be removed.
  int appPid;
  int appRef;
  int appPasswd;
  char confId[64];              //Alredy existing conf id.
  int isBeep;
  char beepFile[96];
} MSG_CONF_REMOVE_USER;



/*
 * DMOP_CONFERENCE_REMOVE_USER_RESPONSE:
 * This message will go from conf manager to media manager as a result of
 * DMOP_CONFERENCE_REMOVE_USER message.
 * Conference manager will try to remove that user from conference using
aculab API.
*/

typedef struct Msg_Conf_Remove_User_Response
{
  int opcode;                   //DMOP_CONFERENCE_REMOVE_USER_RESPONSE
  int appCallNum;
  int appPid;
  int appRef;
  int appPasswd;
  int resultCode;               //0 for success  -1 for error.
  char confId[64];              //Existing conf id
  char error[128];
} MSG_CONF_REMOVE_USER_RESPONSE;



/*
 * DMOP_CONFERENCE_STOP:
 * This message will go from media manager to conf. manager when app calls
 * TEL_ConfereceStop API
*/

typedef struct Msg_Conf_Stop
{
  int opcode;                   //DMOP_CONFERENCE_STOP
  int appCallNum;
  int appPid;
  int appRef;
  int appPassword;
  char confId[64];              //Existing conf id
} MSG_CONF_STOP;


/*
 * DMOP_CONFERENCE_STOP_RESPONSE:
 * This message will go from conf manager to media manager as a result of
 * DMOP_CONFERENCE_STOP message.
 * Conference manager will try to stop a conference using aculab API.
*/

typedef struct Msg_Conf_Stop_Response
{
  int opcode;                   //DMOP_CONFERENCE_STOP_RESPONSE
  int appCallNum;
  int appPid;
  int appRef;
  int appPasswd;
  int resultCode;               // 0 Success, -1 Error
  char confId[64];              //Existing conf id
  char error[128];
} MSG_CONF_STOP_RESPONSE;

typedef struct Msg_Insert_Dtmf
{
  int opcode;                   //DMOP_CONFERENCE_ADD_USER
  int appCallNum;                  //Call Number of the user to be added.
  int appPid;
  int appRef;
  int appPassword;
  char confId[64];              //Alredy existing conf id.
  char dtmf[32];
  int portNum;
  int sendResp;
} MSG_INSERT_DTMF;


typedef struct Msg_Conf_Play_Audio
{
  int opcode;                   //DMOP_CONFERENCE_START
  int appCallNum;
  int appPid;
  int appRef;
  int appPassword;
  char confId[64];              //Unique conf id genereated by media mgr e.g. CDR Key
  char audioFileName[128];
  int party;					//FIRST_PARTY or ALL_PARTIES
  int sync;						//ASYNC or SYNC
} MSG_CONF_PLAY_AUDIO;

typedef struct Msg_Conf_PlayAudio_Response
{
  int opcode;                   //DMOP_CONFERENCE_PLAY_AUDIO
  int appCallNum;                  //Call Number of the user to be added.
  int appPid;
  int appRef;
  int appPasswd;
  int resultCode;               // 0 Success, -1 Error
  char error[128];
} MSG_CONF_PLAYAUDIO_RESPONSE;

// end conference messages 


/* Generic structure for any message sent to dynamic manager */
struct MsgToDM 
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Unique # generated by the API */
		int appPassword;	/* Unique # generated by the DM */
		char data[MAX_APP_MESSAGE_DATA];
	};

/* Generic structure for any message sent to the application */
struct MsgToApp 
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Unique # generated by the API */
		int appPassword;	/* Unique # generated by the DM */
		int returnCode;		/* return code (result) */
		int vendorErrorCode;
		int alternateRetCode;
		char message[224];	/* Error or warning message */
	} ;

/* Generic structure for any message sent to Responsibility */
struct MsgToRes
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Application reference # */
		int appPassword;	/* Application password */
		int	dynMgrPid;
		char dnis[MAX_RES_DNIS];
		char ani[MAX_RES_ANI];
		char rdn[MAX_RES_ANI];
		char data[MAX_RES_DATA];
		char ringEventTime[40];
	};

/* Generic structure for any message sent to Redirector */
struct MsgToRedirector
	{
		int freePorts;		/*Number of free call slots*/
		int	staticPorts;	/*Number of ports that are static*/
	};

/*---------------------------------------------------------------------------
Messages from Responsibility to dynamic manager.
---------------------------------------------------------------------------*/
struct Msg_AppDied
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int returnCode;
	} ;

struct Msg_CantFireApp
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char dnis[MAX_RES_DNIS];
		char ani[MAX_RES_ANI];
		char data[MAX_RES_DATA];
		int returnCode;		/* Reason why we couldn't fire it */
	} ;

struct Msg_PortRequest
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int	portNumber;
		int returnCode;
	} ;

struct Msg_StartOcsApp
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int returnCode;
		char dnis[MAX_RES_DNIS];
		char ani[MAX_RES_ANI];
		char data[MAX_RES_DATA];
	} ;

struct Msg_StartFaxApp
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int returnCode;
		char dnis[MAX_RES_DNIS];
		char ani[MAX_RES_ANI];
		char data[MAX_RES_DATA];
	} ;
/*---------------------------------------------------------------------------
Messages from an application to dynamic manager.
---------------------------------------------------------------------------*/
struct Msg_GetDtmfGetDig
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
	} ;

struct Msg_AnswerCall
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int numRings;
	} ;

struct Msg_RejectCall
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char causeCode[MAX_APP_MESSAGE_DATA];
	} ;

struct Msg_BridgeConnect
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int listenType; /* 0:No Listen, 1:LISTEN_ALL, 2:LISTEN_IGNORE */
	} ;

struct Msg_DropCall
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int allParties;		/* 0=No, 1=Yes */
		int listenType; /* 0:No Listen, 1:LISTEN_ALL, 2:LISTEN_IGNORE */
	} ;

struct Msg_InitiateCall
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int numRings;
		char ipAddress[45];	/* Address of GW or endpoint  */
		char phoneNumber[75];	/* Actual phone # if given */
		int whichOne;		/* 1=A leg, 2=B leg of call */
		int appCallNum1;	
		int appCallNum2;	
		char ani[46];
		int informat;
		int listenType; 		/* 	0:No Listen, 	1:LISTEN_ALL, 
									2:LISTEN_IGNORE 3:TRANSFER_BLIND	*/
		char resourceType[30];
	} ;


struct Msg_BridgeThirdLeg
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int numRings;
		char ipAddress[45];	/* Address of GW or endpoint  */
		char phoneNumber[75];	/* Actual phone # if given */
		int whichOne;		/* 1=A leg, 2=B, 3=C leg of call */
		int appCallNum1;	
		int appCallNum2;	
		int appCallNum3;	
		char ani[46];
		int informat;
		int listenType; 		/* 	0:No Listen, 	1:LISTEN_ALL, 
									2:LISTEN_IGNORE 3:TRANSFER_BLIND	*/
		char resourceType[30];
	} ;


struct Msg_ListenCall
{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int numRings;
		char ipAddress[45];	/* Address of GW or endpoint  */
		char phoneNumber[75];	/* Actual phone # if given */
		int whichOne;		/* 1=A leg, 2=B leg of call */
		int appCallNum1;	
		int appCallNum2;	
		char ani[46];
		int informat;
		int listenType; 		/* 	0:No Listen, 	1:LISTEN_ALL, 
									2:LISTEN_IGNORE 3:TRANSFER_BLIND	*/
		char resourceType[30];
	} ;


struct Msg_InitTelecom
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int language;
		//char ringEventTime[64];
		//char responseFifo[MAX_APP_MESSAGE_DATA-88];
		char responseFifo[MAX_APP_MESSAGE_DATA-24];
		int killApp;
	} ;

struct Msg_OutputDTMF
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char dtmf_string[128];
	} ;

struct Msg_PortOperation
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int operation;	
		int callNum;	
	} ;

struct Msg_Record
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int party;
		int allParties;		/* 0=No, 1=Yes */
		char filename[100]; /* How long should this be ? */
		int record_time;
		int compression;
		int overwrite;
		int lead_silence;
		int trail_silence;
		char beepFile[96];
		int interrupt_option;
		char terminate_char;
		int synchronous;
	} ;

struct Msg_StartRecordSilently
	{
		int opcode;
		int appCallNum;	
        char inFilename[100];
        char outFilename[100];
	} ;

struct Msg_StopRecordSilently
	{
		int opcode;
		int appCallNum;	
	} ;

struct Msg_Speak
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int list;		/* 0=no list, 1= file is a list file */
		int allParties;		/* 0=No, 1=Yes */
		int interruptOption;	/* Not used by DM, only APIs */
		int synchronous;	/* 0=No, 1=Yes */
		int deleteFile;		/* 0=No, 1=Yes */
		char key[30];
		char file[128];
	};

struct Msg_SpeakMrcpTts
    {
        int opcode;
        int appCallNum;
        int appPid;
        int appRef;
        int appPassword;    /* Unique # generated by the DM */
        int list;       /* 0=no list, 1= file is a list file */
        int allParties;     /* 0=No, 1=Yes */
        int interruptOption;    /* Not used by DM, only APIs */
        int synchronous;    /* 0=No, 1=Yes */
        int deleteFile;     /* 0=No, 1=Yes */
        char key[30];
        char file[128];
        char resource[64];
    };

struct Msg_StreamAudio	/*From app to DM*/
{
	int opcode;
	int appCallNum;	
	int appPid; 
	int appRef;
	int appPassword;		/* Unique # generated by the DM */
	int dmId;
	int msgId;
	int nodeId;
	int requestType;
	long dictationTime;
	char subscriber[40];
	char sessionId[40];
};

struct Msg_AppToAsc		/*From app to arc straming client*/
{
	int opcode;
	int appCallNum;	
	int appPid; 
	int appRef;
	int appPassword;		/* Unique # generated by the DM */
	int allParties;			/* 0=No, 1=Yes */
	int interruptOption;	/* Not used by DM, only APIs */
	int synchronous;		/* 0=No, 1=Yes */
	int async;				/* 0=Non PUT_QUEUE_ASYNC, 1=PUT_QUEUE_ASYNC */
	int dmId;

	//int msgId;				/*Halt Bauver*/
	//int nodeId;				/*Halt Bauver*/
	//int dictationTime;		/*Halt Bauver*/
	//char subscriber[40];	/*New ICD Brandon*/

	char key[30];
	char file[200];

};

struct Msg_DMToAsc		/*From DM to Arc streaming client*/
{
	int opcode;
	int appCallNum;	
	int appPid; 
	int appRef;
	int appPassword;	/* Unique # generated by the DM */
	int dmId;
	int bufferNum;
	int msgId;				/*Halt Bauver*/
	int nodeId;				/*Halt Bauver*/
	int dictationTime;		/*Halt Bauver*/
	char subscriber[40];	/*New ICD Brandon*/
	char session[40];

};

struct Msg_AscToDM		/*From Arc Streaming client ot DM*/
{
    int opcode;
    int appCallNum;
    int appPid;
    int appRef;
    int appPassword;    /* Unique # generated by the DM */
    int dmId;
    int bufferNum;
    int returnCode;
    int bufferSize;
    int msgId;              /*Halt Bauver*/
    int nodeId;             /*Halt Bauver*/
    int dictationTime;      /*Halt Bauver*/
    int lastBuffer;         /*Halt Bauver*/
    char subscriber[40];    /*New ICD Brandon*/
    char file[128];

};

struct Msg_PlayMedia
	{	
		int opcode;
		int appCallNum;
		int appPid;
		int appRef;
		int appPassword;    /* Unique # generated by the DM */
		int party;
		int interruptOption;
		char audioFileName[256];
		char videoFileName[256];		// VIDEO_RM
		int sync;
		int audioinformat;
		int audiooutformat;
		int videoinformat;			// VIDEO_RM
		int videooutformat;			// VIDEO_RM

		int audioLooping;
		int videoLooping;			// VIDEO_RM
		int addOnCurrentPlay;
		int syncAudioVideo;			// VIDEO_RM
	
		char key[30];
	
		int isReadyToPlay;
		int isDonePlaying;
		int waitForKey;

	};

struct Msg_RecordMedia
	{
		int opcode;
		int appCallNum;
		int appPid;
		int appRef;
		int appPassword;    /* Unique # generated by the DM */
		int party;
		int interruptOption;
		char audioFileName[256];
		char videoFileName[256];			// VIDEO_RM
		int sync;
		int recordTime;
		int audioCompression;
		int videoCompression;			// VIDEO_RM
		int audioOverwrite;
		int videoOverwrite;			// VIDEO_RM
		int lead_silence;
		int trail_silence;
		int beep;
		char terminateChar;
		char beepFile[128];
		
	};



struct Msg_SpeechDetected
    {
        int opcode;
        int appCallNum;
        int appPid;
        int appRef;
        int appPassword;    /* Unique # generated by the DM */
		double milliSecs;
    } ;

struct Msg_StopActivity
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
	} ;

struct Msg_ReservePort
	{
		int opcode;
		int appCallNum;		/* Not used */
		int appPid; 		/* Not used */
		int appRef;		/* Not used */ 
		int appPassword;	/* For request, not used, for response,
					   password of first granted port. */
		int dynMgrPid;
		int maxCallNum; 	/* Highest port number requested */	
		int totalResources; 	/* total resources or # of licensed ports */
		char portRequests[MAX_APP_MESSAGE_DATA-12]; /* Port map */
	};

struct Msg_SpeechRec
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int party;
		char speechDataFifoName[100];
		char beepFile[100];
		int interrupt_option;
		int synchronous;
		char data[15];
	} ;

struct Msg_DialExtension
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char extensionNumber[128];
	} ;

struct Msg_SetGlobalString
	{
		int opcode;
		int appCallNum;
		int appPid;
		int appRef;
		int appPassword;
		char name[50];
		char value[150];
	};

struct Msg_GetGlobalString
	{
		int opcode;
		int appCallNum;
		int appPid;
		int appRef;
		int appPassword;
		char name[50];
	};

struct Msg_SetGlobal
	{
		int opcode;
		int appCallNum;
		int appPid;
		int appRef;
		int appPassword;
		char name[50];
		int value;
	};

struct Msg_GetGlobal
	{
		int opcode;
		int appCallNum;
		int appPid;
		int appRef;
		int appPassword;
		char name[50];
	};

struct Msg_InfoElements
	{
		int opcode;
		int appCallNum;
		int appPid;
		int appRef;
		int appPassword;
		int	party;
		int msgType;
		char file[128];
	};

	/*
	** addWordListFile has resourceName on line 1
	** 							category name on line 2
	**							number (n) of words on line 3
	**							word 1
	**							word 2....
	**							word n
	*/

struct Msg_SRAddWords
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char addWordsListFile[MAX_APP_MESSAGE_DATA];
		
	} ;

struct Msg_SRDeleteWords
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char deleteWordsListFile[MAX_APP_MESSAGE_DATA];
	} ;

struct Msg_SRExit
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
	} ;

struct Msg_SRGetParameter
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char name[100];
	} ;

struct Msg_SRGetPhoneticSpelling
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char resourceName[50];
		char audioPath[150];
	} ;

struct Msg_SRGetResult
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int alternativeNumber;
		int tokenType;
		char delimiter[10];
	} ;

struct Msg_SRGetXMLResult
	{
		int opcode;
		int appCallNum;
		int appPid;
		int appRef;
		int appPassword;
		char fileName[MAX_APP_MESSAGE_DATA];
	};

struct Msg_SRInit
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char resourceNamesFile[100];
		char appName[100];
	} ;

struct Msg_SRLearnWord
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char learnWordFile[MAX_APP_MESSAGE_DATA];
	} ;

struct Msg_SRLoadGrammar
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int grammarType;
		float weight;
		char grammarFile[MAX_APP_MESSAGE_DATA-4];
	} ;

struct Msg_SRRecognize
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int party;
		int bargein; /* 0=No, 1=Yes */
		int total_time;
		int lead_silence;
		int trail_silence;
		char beepFile[100];
		int interrupt_option;
		int tokenType;
		int alternatives;
		char resource[100];
	} ;

struct Msg_SRReleaseResource
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
	} ;

struct Msg_SRReserveResource
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
	} ;

struct Msg_SRSetParameter
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char nameValue[MAX_APP_MESSAGE_DATA]; /* name=value */
	} ;

struct Msg_SRUnloadGrammar
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char grammarName[MAX_APP_MESSAGE_DATA-4];
	} ;

struct Msg_AIInit
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char appFifo[100];
	} ;

struct Msg_AIExit
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
	} ;

struct Msg_AIProcessContent
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int aiOpcode;
		char content[MAX_APP_MESSAGE_DATA-4];
	} ;

struct Msg_CallProgress
    {
        int opcode;
        int appCallNum;
        int appPid;
        int appRef;
        int appPassword;    /* Unique # generated by the DM */
        int origOpcode;
        char nameValue[MAX_APP_MESSAGE_DATA-4];
    } ;

struct Msg_NewCall 
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Unique # generated by the API */
		int appPassword;	/* Unique # generated by the DM */
		char dnis[20];
		char ani[20];
		char data[128];
	};

struct Msg_HangUp 
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Unique # generated by the API */
		int appPassword;	/* Unique # generated by the DM */
		int all;			/* 1:Hangup all calls on that span,
							   0:Hangup this call only. */
	};

struct MsgToGemListener 
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Unique # generated by the API */
		int appPassword;	/* Unique # generated by the DM */
		int returnCode;		/* return code (result) */
		char message[224];	/* Error or warning message */
	} ;

struct Msg_SendFax 
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Unique # generated by the API */
		int appPassword;	/* Unique # generated by the DM */
		int fileType;
		time_t	timeStamp;
		char nameValue[75];
//		char sdp[120];
		char faxFile[105];

		//unsigned short width;
		//unsigned short resolution;

		//unsigned short pagelength;
		//unsigned short pagepad;
		//unsigned short topmargin;
		//unsigned short botmargin;
		//unsigned short leftmargin;
		//unsigned short rightmargin;
		//unsigned short linespace;
		//unsigned short font;
		//unsigned short tabstops;
		//unsigned char  units;
		//unsigned char  flags;
        char faxResourceName[25];
	};

struct Msg_RecvFax 
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Unique # generated by the API */
		int appPassword;	/* Unique # generated by the DM */
		time_t	timeStamp;
		char faxFile[105];
		char nameValue[50];
//		char sdp[120];
		char faxResourceName[25];
	};

struct Msg_FaxReserveResource 
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Unique # generated by the API */
		int appPassword;	/* Unique # generated by the DM */
		time_t	timeStamp;
		char data[80];
	};

struct Msg_Fax_Proceed
{   
	int opcode;
	int appCallNum;
	int appPid;
	int appRef;
	int appPassword;    /* Unique # generated by the DM */
	int sendRecvOp;
	char nameValue[MAX_APP_MESSAGE_DATA]; /* name=value */
	// nameValue-> for rc=0 "specialFifo=<fifo>&IP=<ip addr>&udpPort=<udp port>&rc=0"
	// nameValue-> for rc=-1 "specialFifo=<fifo>&IP=0.0.0.0&udpPort=0&rc=-1"
	//      If the rc=-1, then that means one of the calls to setup the
	//      fax receive failed, and the mediaMgr should take
	//      appropriate action. ( Note: we need to add this for call progress. )
} ;

struct Msg_Fax_Complete 
{
    int opcode;
    int appCallNum;
    int appPid;
    int appRef;
    int appPassword;    /* Unique # generated by the DM */
    int rc;
	int sendRecvOp;
	int numPagesReceived;
} ;

struct Msg_Fax_PlayTone
    {
        int opcode;
        int appCallNum;
        int appPid;
        int appRef;
        int appPassword;    /* Unique # generated by the DM */
        int sendOrRecv;     /* 0=Send, 1=Recv */
        char file[100];
    };

struct Msg_Fax_StopTone
    {
        int opcode;
        int appCallNum;
        int appPid;
        int appRef;
        int appPassword;    /* Unique # generated by the DM */
    };

struct MsgFromDMToDM 
	{
		int opcode;			/* Op code */
		int appCallNum;		/* Call # of app instance: 0,1, etc. */
		int appPid;			/* Application pid */
		int appRef;			/* Unique # generated by the API */
		int appPassword;	/* Unique # generated by the DM */
		int callingDM;
		int callingPort;
		int isItResult;
		int returnCode;
		//char data[128];
		int ipType;
		long callingPortNetworkSlot;
		long callingPortVoiceSlot;
		int numOfRings;
		char data[64];
		char protocolName[32];
		char protocolType[32];
	};


struct Msg_SRLogEvent
	{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		char logEventFile[100];
	} ;


struct Msg_FindMe
{ 
   int     opcode;
   int     appCallNum;
   int     appPid;
   int     appRef;
   int     appPassword;    /* Unique # generated by the DM */
   int     originatorCallNum;
   int     originatorPID;
   int     returnCode;
   char    data[64];
}; 


struct MsgVxmlToResp
{
	int appCallNum;
	int appPid;
};


/*From SIP SDM to Redirection manager*/

struct Msg_Register
{
	int opcode;
	int pid;
	int dmId;
	int port;
	char ip[100];
	char requestFifo[100];
};

/* BlastDial structures */
struct Msg_BlastDial
{
    int     opcode;
    int     appCallNum;
    int     appPid;
    int     appRef;
    int     appPassword;    /* Unique # generated by the DM */
    int     numRings;
    char    dataFile[64];
};

#ifdef NETWORK_TRANSFER
struct Msg_SendMsgToApp
{
  int   opcode;
  int   appCallNum;
  int   appPid;
  int   appRef;
  int   appPassword;    /* Unique # generated by the DM */
  int   sendingAppCallNum;
  int   sendingAppPid;
  int   opcodeCommand;
  int   rc;
  char  data[220];
};
#endif

#ifdef VOICE_BIOMETRICS
struct Msg_ContinuousSpeech
{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
		int	Keep_PCM_Files;
		char pin[32];
		unsigned ui_voiceprint_size;
		char file[200];
};

struct Msg_GetVerifyContinuousResults
{
		int opcode;
		int appCallNum;	
		int appPid; 
		int appRef;
		int appPassword;	/* Unique # generated by the DM */
};

struct Msg_AVBMsgToApp
    {
        int opcode;         /* Op code */
        int appCallNum;     /* Call # of app instance: 0,1, etc. */
        int appPid;         /* Application pid */
        int appRef;         /* Unique # generated by the API */
        int appPassword;    /* Unique # generated by the DM */
        int returnCode;     /* return code (result) */
        int vendorErrorCode;
        float       score;
        float       indThreshold;
        float       confidence;
        char        errMsg[216];
    };


#endif // END: VOICE_BIOMETRICS

#endif
