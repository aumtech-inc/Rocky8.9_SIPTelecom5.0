/*----------------------------------------------------------------------------
Program:	Telecom.h
Purpose:	To define constants used by Telecom Server.
		WARNING: Changes to this file may adversely affect operations.
Author:		Aumtech
Date:		07/22/2003
-----------------------------------------------------------------------------
#	Copyright (c) 2000 Aumtech, Inc.
#	All Rights Reserved.
#
#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Aumtech
#	which is protected under the copyright laws of the 
#	United States and a trade secret of Aumtech. It may 
#	not be used, copied, disclosed or transferred other 
#	than in accordance with the written permission of Aumtech.
#
#	The copyright notice above does not evidence any actual
#	or intended publication of such source code.
#
#----------------------------------------------------------------------------*/
#ifndef _TELECOM_H	/* for preventing multiple defines. */
#define _TELECOM_H

/* Return Codes */
#define		TEL_SUCCESS			 0
#define		TEL_FAXTONE			 1
#define		TEL_FAILURE			-1
#define		TEL_TIMEOUT			-2
#define		TEL_DISCONNECTED		-3
#define		TEL_EXTRA_STAR_IN_DOLLAR	-4
#define		TEL_AUTO_CREATE_PHRASE_FAIL	-5
#define		TEL_OVERFLOW			-6
#define		TEL_SOURCE_NONEXISTENT		-7
#define		TEL_DESTINATION_EXISTS		-8
#define		TEL_RESOURCE_UNAVAILABLE	-9
#define		TEL_LOST_AGENT			-10
#define		TEL_WRONG_TYPE_OR_COMP		-11
#define		TEL_INVALID_INPUT		-12
#define		TEL_LOST_CALLER			-13

#define         TEL_PLACED_CALL                 -14
#define         TEL_OCS_ERROR                   -15
#define         TEL_CDF_NOT_FOUND               -16

/* Speech Rec Return Codes */
#define TEL_SR_RECEIVED_DTMF        -17
#define TEL_SR_CHECK_WORD_RESULTS   -18
#define TEL_SR_SPOKE_TOO_LONG       -19
#define TEL_SR_NOTHING_RECOGNIZED   -20

#define TEL_CALL_SUSPENDED		-22
#define TEL_CALL_RESUMED		-23

#define TEL_FM_CONNECTED        -30
#define TEL_FM_SENDDATA         -31
#define TEL_FM_DISCONNECTED     -32

#define TEL_CONFERENCE_DUPLICATECONFERENCE -33
#define TEL_CONFERENCE_DUPLICATEUSER -34
#define TEL_CONFERENCE_INVALIDCONFERENCE -35
#define TEL_CONFERENCE_INVALIDUSER -36

#ifdef NETWORK_TRANSFER
#define TEL_MSG_FROM_APP            -40      // Network Transfer return code
#endif

#define TEL_AVB_NO_RESULTS			-50

#define TEL_NEW_MRCP_CONNECTION     -55

#define TEL_CALL_SS7_DISCONNECT_BASE    250
#define TEL_CALL_NORMAL_UNSPECIFIED     TEL_CALL_SS7_DISCONNECT_BASE +1 //Release Cause 31 -- Normal, unspecified
#define TEL_CALL_UNALLOCATED_NUMBER     TEL_CALL_SS7_DISCONNECT_BASE +2 //Release Cause 01 - Unallocated number,
#define TEL_CALL_NO_ROUTE_TO_NETWORK    TEL_CALL_SS7_DISCONNECT_BASE +3 //Release Cause 02 - No route to specified transit network,
#define TEL_CALL_NO_ROUTE_TO_DEST       TEL_CALL_SS7_DISCONNECT_BASE +4 //Release Cause 03 - No route to destination and
#define TEL_CALL_FACILITY_REJECTED      TEL_CALL_SS7_DISCONNECT_BASE +5 //Release Cause 29 - Facility rejected:
#define TEL_CALL_NORMAL_CLEARING        TEL_CALL_SS7_DISCONNECT_BASE +6 //Release Cause 16 - Normal Call Clearing:
#define TEL_CALL_USER_BUSY              TEL_CALL_SS7_DISCONNECT_BASE +7 //Release Cause 17 - User busy:
#define TEL_CALL_USER_NO_RESPONSE       TEL_CALL_SS7_DISCONNECT_BASE +8 //Release Cause 18 - User not responding and
#define TEL_CALL_USER_ALTERED           TEL_CALL_SS7_DISCONNECT_BASE +9 //Release Cause 19 - No answer from user (user alerted):
#define TEL_CALL_REJECTED               TEL_CALL_SS7_DISCONNECT_BASE +10 //Release Cause 21 - Call rejected:
#define TEL_CALL_BAD_DEST               TEL_CALL_SS7_DISCONNECT_BASE +11 //Release Cause 27 - Destination out of order:
#define TEL_CALL_ADDRESS_INCOMPLETE     TEL_CALL_SS7_DISCONNECT_BASE +12 //Release Cause 28 - Address Incomplete:

/* Miscellaneous Constants */
#define 	MAX_SIZE	256
#define 	ISP_RES_FIX	1
#define 	ISP_RES_DYN	2
#define		PORT_CONNECTED		1
#define		PORT_DISCONNECTED	2
#define		TID_FAXT	2000
#define		TID_ADISCON	2100

/*---------------------------------------------------------------------------
The following are parameter defines used by APIs and by the monitor
programs. Do not alter any values.
---------------------------------------------------------------------------*/
/* Language format for speaking numbers. */
#define 	S_AMENG		1
#define 	S_FRENCH	2
#define 	S_GERMAN	3
#define 	S_DUTCH		4
#define 	S_SPANISH	5	
#define 	S_BRITISH	6
#define 	S_FLEMISH	7
#define 	S_MANDARIN	8
#define 	S_CANTONESE	9

/* input formats */
#define		NUMERIC		10
#define		ALPHA		11
#define		NUMSTAR		12
#define		ALPHANUM	13

#define		AUTO		17	
#define		AUTOSKIP	18
#define		MANDATORY	19

/* call actions */
#define		ONHOOK		20
#define		OFFHOOK		21

/* outbound call diversion (Bridge and Transfer) */
#define		DIAL_OUT	22
#define		CONNECT		23
#define		BRIDGE		23
#define		TRANSFER	24
#define		ALL		25
/* 		LISTEN_ALL 	is defined below as 120 */
/* 		LISTEN_IGNORE 	is defined below as 126 */
/* 		ANALOG_HAIRPIN 	is defined below as 137 */

/* Interrupt options */
#define		INTRPT		26
#define		INTERRUPT	26
#define		NONINTRPT	27
#define		NONINTERRUPT	27
#define	FIRST_PARTY_INTRPT 	28 
#define	FIRST_PARTY_INTERRUPT 	28 
#define	SECOND_PARTY_INTRPT	29
#define	SECOND_PARTY_INTERRUPT	29
/* 		PLAYBACK_CONTROL is defined below */
/* 		PLAYBACK_CONTROL_FIRST_PARTY is defined below */
/* 		PLAYBACK_CONTROL_SECOND_PARTY is defined below */

/* following are outformat to speak */
#define		PHRASE		30
#define		DOLLAR		31
#define		NUMBER		32
#define		DIGIT		33
#define		DAY		34
#define		DATE_YTT	35
#define		DATE_MMDD	36
#define		DATE_MMDDYY	37
#define		DATE_MMDDYYYY	38
#define		DATE_DDMM	39
#define		DATE_DDMMYY	40
#define		DATE_DDMMYYYY	41
#define		TIME_STD	42
#define		TIME_STD_MN	43
#define		TIME_MIL	45
#define		TIME_MIL_MN	46
#define		DATE_MMYYYY	47
#define		DATE_MM		48
#define		DATE_YYYY   49

/* Following are input formats to speak */
#define		MILITARY	50		/* input format for time */
#define		STANDARD	51
#define		TICS		52		/* Seconds since 1/1/1970 */
#define		MM_DD_YY	53		/* string in format for dates*/
#define		DD_MM_YY	54
#define		YY_MM_DD	55

#define		ENABLE		59		/*ENABLE Dial Pulse Detection*/
#define		DISABLE		60		/*DISABLE Dial Pulse Detection*/

#define		PHRASE_TAG	64		/* input format for tag */
#define		PHRASENO	65		/* input format for phrase */
#define		INTEGER		66		/* integer informat */
#define		DOUBLE		67		/* double prec. float informat*/
#define		SOCSEC		68
#define		PHONE		69
#define		STRING		70

/* For use with TEL_SpeakFile & TEL_GetInput InBridge APIs */
#define		FIRST_PARTY_WHISPER	71   
#define		FIRST_PARTY	72   
#define		SECOND_PARTY	73  
#define		BOTH_PARTIES	74 

/* Used to set global $BEEP */
#define		YESBEEP		75
#define		NOBEEP		76

/* Outbound call phone number types */
#define		NAMERICAN				77
#define		NONE					78
#define		IP						79
#define     E_164                   164

/* Vocabulary file types for speech recognition. */
#define 	PVEVOCAB	80
#define 	STDVOCAB	81

/* Languages for speech recognition. */
#define 	L_AMENG		85
#define 	L_FRENCH	86
#define 	L_GERMAN	87
#define 	L_BRITISH	88
#define 	L_SPANISH	89	
#define 	L_DUTCH		90
#define 	L_FLEMISH	91

#define 	PHRASE_FILE	95
#define 	SYSTEM_PHRASE	96

/* Phrase speaking options */
#define 	SYNC				100
#define 	ASYNC				101

#define	YES			103
#define	NO			104

/* Phrase recording types. */
#define 	COMP_24ADPCM	107
#define 	COMP_32ADPCM	108
#define 	COMP_48PCM	109
#define 	COMP_64PCM	110
#define 	COMP_WAV	111
#define     COMP_GSM                112


#define     COMP_711		151
#define     COMP_711R	152
#define     COMP_729		153
#define     COMP_729R	154
#define     COMP_729A	155
#define     COMP_729AR	156

#define     COMP_WAV_H263R_QCIF		157
#define     COMP_711_H263R_QCIF		158
#define     COMP_711R_H263R_QCIF		159
#define     COMP_729_H263R_QCIF		160
#define     COMP_729R_H263R_QCIF		161
#define     COMP_729A_H263R_QCIF		162
#define     COMP_729AR_H263R_QCIF	163
#define     COMP_WAV_H263R_CIF		164
#define     COMP_711_H263R_CIF		165
#define     COMP_711R_H263R_CIF		166
#define     COMP_729_H263R_CIF		167
#define     COMP_729R_H263R_CIF		168
#define     COMP_729A_H263R_CIF		169
#define     COMP_729AR_H263R_CIF		170
#define     COMP_MVP_H263R_QCIF		171
#define     COMP_MVP_H263R_CIF		172
#define     COMP_VOX    			112
#define     COMP_729B               173
#define     COMP_729BR              174
#define     COMP_729B_H263R_CIF     175
#define     COMP_729BR_H263R_CIF    176
#define     COMP_729B_H263R_QCIF    177
#define     COMP_729BR_H263R_QCIF   178


#define		SCHEDULED_DNIS_AND_APP  112
#define		STANDALONE_APP		113
#define		SCHEDULED_DNIS_ONLY	114

/* For TEL_PortOperation */
#define         CHECK_DISCONNECT	115
#define         CLEAR_CHANNEL   	116     

/* TTS speak options */ 
#define         SPEAK_AND_KEEP   	117  
#define         SPEAK_AND_DELETE 	118 
#define         DONT_SPEAK_AND_KEEP 	119

#define		LISTEN_ALL		120	/* For TEL_BridgeCall  */
#define		PLAYBACK_CONTROL	121
#define		SECOND_PARTY_NO_CALLER	122 /* For TEL_GetInput */
#define		FIRST_PARTY_PLAYBACK_CONTROL	123
#define		SECOND_PARTY_PLAYBACK_CONTROL	124
#define		BLIND	133

#define 	ASYNC_QUEUE	125
#define		LISTEN_IGNORE		126	/* For TEL_BridgeCall  */
#define		COMP_MVP			127	/* For TEL_Record  */

/* QUEUED Phrase speaking options */
#define 	PLAY_QUEUE_ASYNC	128
#define 	PLAY_QUEUE_SYNC		129

/* Phrase queueing options */
#define 	PUT_QUEUE			130
#define 	CLEAR_QUEUE_ALL		131
#define 	PUT_QUEUE_ASYNC		132

#define 	APPEND				134 /* For TEL_Record */

/* Additional input and output formats for Speaking Silence */
#define		MILLISECONDS		135	/*informat*/
#define		SILENCE				136	/*outformat*/

#define		ANALOG_HAIRPIN		137

#define     SIP                 138
#define     H323                139

/* TEL_PortOperation defines */
#define 	GET_CONNECTION_STATUS 	0
#define 	CLEAR_DTMF 				1
#define 	GET_FUNCTIONAL_STATUS	2
#define 	STOP_ACTIVITY			3
#define 	WAIT_FOR_PLAY_END		4
#define 	GET_QUEUE_STATUS		5
// added joes Tue Mar 31 13:48:27 EDT 2015
#define 	RTP_RESET				6
// end port operations 

/* FAX Defines */
#define FAX_FROM_TO_HDR_FMT			0
#define FAX_USER_DEFINED_HDR_FMT	1
#define FAX_DISABLE_HDR_FMT			2

/* File types for sending fax. */
#define     MEM     94
#define     TEXT    96
#define     TIF     97
#define     LIST    98

/* FindMe */
#define     FM_DIAL_OUT     1
#define     FM_CONNECT      2
#define     FM_SEND_DATA    3
#define     FM_WAIT         4

#define     MAX_FINDME_CONTACTS     20

typedef struct
{
	int     originatorPort;
	int     originatorPID;
	char    contact[MAX_FINDME_CONTACTS][256];
	int     returnCode;
	int     numberOfRings;
	char    data[224];
} FindMeStruct;
								


/* PlayMedia defines */
#define			SPECIFIC				140
#define 			VIDEO_FILE			141
#define 			CLEAN_AUDIO			142
#define 			CLEAN_VIDEO			143
#define			CLEAR_QUEUE_AUDIO	145
#define			CLEAR_QUEUE_VIDEO	146
#define			ADD					147
#define			DELETE				148
#define			PLAY					149
#define 			VIDEO					150

/* Video Recording Type */
#define			COMP_H263R_QCIF	144
#define			COMP_H263R_CIF		173
#define			COMP_H263			174
#define			COMP_WAV_H263		175

#define 			MAX_ISP_PARM		176 /* Must be > last item above it */

#define     INTERNAL_PORT           177

typedef struct playMedia
{
	int party;
	int interruptOption;
	char audioFileName[256];
	char videoFileName[256];
	int sync;
	int audioInformat;
	int audioOutformat;
	int videoInformat;
	int videoOutformat;
	int audioLooping;
	int videoLooping;
	int addOnCurrentPlay;
	int syncAudioVideo;
	int list;
} PlayMedia;

typedef struct recordMedia
{
	int party;
	int interruptOption;
	char audioFileName[256];
	char videoFileName[256];
	int sync;
	int recordTime;
	int audioCompression;
	int videoCompression;
	int audioOverwrite;
	int videoOverwrite;
	int lead_silence;
	int trail_silence;
	int beep;
	char terminateChar;
} RecordMedia;

struct ArcSipDestinationList
{
    char    destination[256];       // Number or IP
    int     inputType;              // IP or None
    int     outboundChannel;
    int     resultCode;             // Call Progress Code
    char    resultData[256];        // Textual result message
};


/*-----------------------------------------------------------
Following are the Telecom Core api's prototype
-------------------------------------------------------------*/ 
extern int TEL_AnswerCall(int);
extern int TEL_RejectCall(char *);
extern int TEL_BridgeCall(int,int,int,const char *,const char *, int *, int *);
extern int TEL_BridgeThirdLeg(int,int,int,const char *,const char *, int *, int *);
extern int TEL_ClearDTMF();
extern int TEL_EndOfCall(void);
extern int TEL_ExitTelecom(void);
extern int TEL_GetGlobalString(const char *, char *);
extern int TEL_GetGlobal(const char *, int *);
extern int TEL_GetInput(int, int, char, int, int, int, char *, char *);
extern int TEL_InitTelecom(int, char **);
extern int TEL_InitiateCall(int, int, const char *, int *, int *);
extern int TEL_OutputDTMF(const char *);
extern int TEL_RecvFax(char *, int *, char *);
extern int TEL_SendFax(int, char *, char *);
extern int TEL_ScheduleFax(int, char *, int, char *, char *, int,
				int, int, int, char *);
extern int TEL_BuildFaxList(int,  char *, char *, int, int, int, 
				char *, int, char *);
extern int TEL_SetFaxTextDefaults(char *);
extern int TEL_SetGlobal(const char *, int);
extern int TEL_SetGlobalString(const char *, const char *);
extern int TEL_SpeakFile(int, int, char *, int);
extern int TEL_SpeakSystemPhrase(int);
extern int TEL_TransferCall(int,int,int,const char *,const char *,int *,int *);
extern int TEL_RecordPhrase(char *, int,int,int);
extern int TEL_SpeakTTS(int interruptOption,int cleanupOption, int syncOption,
                        int timeout, char *speechFile, char *userString);
extern int TEL_UnscheduleCall(char *);
extern int TEL_ScheduleCall(int,int,int,char *,char *,char *,char *,char *);
extern int TEL_GetCDFInfo(char *,int *,int *,int *,char *,char  *);
extern int TEL_PortOperation(int op_code, int port_no, int *status);
extern int TEL_StartNewApp(int appType, char *newApp, char *dnis, char *ani);

extern int TEL_Speak(int party, int interruptOption, int informat,
                	int outformat, void *data, int synchronous);
extern int TEL_Record(int party, char *filename, int record_time,
        		int compression, int overwrite, int lead_silence,
        		int trail_silence, int beep, int interruptOption,
        		char terminate_char, int synchronous);
extern int TEL_GetDTMF(int party, int firstDigitTimeOut, 
			int interDigitTimeOut, int tries, 
			int beep, char termChar, int length, 
			int terminateOption, int inputType, 
			char *data, char *futureUse);
extern int TEL_PromptAndCollect(char *sectionName,
				char *overrides, char *data);
extern int TEL_LoadTags(char *phraseTagFiles);

extern int TEL_DialExtension(int party, int numOfRings, char *extensionNumber,
                        int timeToWaitForDetection, int *retCode);
extern int TEL_DropCall(int party);
extern int TEL_SendInfoElements(int ToWhom, char *infoElement, int msgType);
// extern int TEL_SRLogEvent(char *zpText);
extern int TEL_PlayMedia(PlayMedia *zPlayMedia);
extern int TEL_RecordMedia(RecordMedia *zRecordMedia);
extern int TEL_BlastDial(int numRings, struct ArcSipDestinationList dlist[]);
extern int TEL_ConferenceStart(char *zConfId);
extern int TEL_ConfRemoveUser(char *zConfId, int isBeep, char *numOfChannel);
extern int TEL_ConfAddUser(char *zConfId, int isBeep, char *duplex, char *numOfChannel);
extern int TEL_ConferenceStop(char *zConfId);
extern int TEL_ConfSendData(char *zConfId, char *zConfData);


/* routines to handle return codes */
extern int	(*SERV_M1)();
extern int	(*SERV_M2)();
extern int	(*SERV_M3)();
extern int	(*SERV_M4)();
extern int	(*SERV_M5)();
extern int	(*SERV_M6)();
extern int	(*SERV_M7)();
extern int	(*SERV_M8)();
extern int	(*SERV_M9)();
extern int	(*SERV_M10)();
extern int	(*SERV_M11)();
extern int	(*SERV_M12)();
extern int	(*SERV_M13)();
extern int	(*SERV_M14)();
extern int	(*SERV_M15)();
extern int	(*SERV_ALL)(int);

extern int	(*SIR_cleanup)();
extern int	(*FAX_cleanup)();
extern int	(*TTS_cleanup)();

//extern	char	LAST_API[64];
#endif /* _TELECOM_H */
