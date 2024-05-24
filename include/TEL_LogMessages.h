/*-----------------------------------------------------------------------------
File:		TEL_LogMessages.h
Purpose:	To hold all the message numbers, messages, and other log
		related information necessary for the Telecom Services APIs.

Note:		In Dialogic Telecom Services, much of this information was
		kept in TELmsg.h

Author:		George Wenzel
Date:		09/12/2000
Update:		03/02/2001 gpw added TEL_UnscheduleCall messages.
Update:		03/08/2001 gpw removed unnecessary crap that no one owned up to
Update:		03/27/2001 mpb Added REPORT_CDR define.
Update:		07/10/2001 apj Added TEL_CUST_DATA_TOO_LONG.
Update:		07/17/2001 apj Added Messages for TEL_LoadTags, TEL_SaveTags,
				TEL_PromptAndCollect.
Update:		07/07/2014 djb Added voice biometrics defines
-----------------------------------------------------------------------------*/
#ifndef LOG_MESSAGES_H
#define LOG_MESSAGES_H

// ----------------------------------------------------------------------------
// New Logging declarations start here
// ----------------------------------------------------------------------------

/* MSG TYPE */
#define ERR 0
#define WARN 1
#define INFO 2

/*  MODES  */
#define REPORT_NORMAL   1
#define REPORT_VERBOSE  2
#define REPORT_CDR      16
#define REPORT_DIAGNOSE 64
#define REPORT_DETAIL   128
#define REPORT_EVENT	256

/* Messages for LogARCMsg */
#define TEL_BASE			1300					//  TEL  - size 700 - range 1300-1999
#define RESP_BASE			TEL_BASE + 700			//  RESP - size 300 - range 2000-2299
#define FAX_BASE			TEL_BASE + 1000 		//  FAX  - size 100 - range 2300-2399
#define SR_BASE				FAX_BASE + 100 			//  SR   - size 100 - range 2400-2499
#define TTS_BASE			SR_BASE  + 100 			//  TTS  - size 100 - range 2500-2599
#define AVB_BASE            TTS_BASE  + 100          //  SV  - size 100 - range 2600-2699

#define SR_BASE				FAX_BASE + 100 			//  SR   - size 100 - range 2400-2499
#define TTS_BASE			SR_BASE  + 100 			//  TTS  - size 100 - range 2500-2599
#define AI_BASE				TTS_BASE  + 100 		//  AI   - size 100 - range 2600-2699

#define SR_AFFINITY_ERROR					SR_BASE + 1
#define SR_CALL_NOT_ACTIVE					SR_BASE + 2
#define SR_DATA_NOT_FOUND					SR_BASE + 3
#define SR_FILE_IO_ERROR					SR_BASE + 4
#define SR_INITIALIZATION_ERROR				SR_BASE + 5
#define SR_INCOMPLETE_MSG					SR_BASE + 6
#define SR_INVALID_DATA						SR_BASE + 7
#define SR_INVALID_OPCODE					SR_BASE + 8
#define SR_INVALID_PARM						SR_BASE + 9
#define SR_IPC_ERROR						SR_BASE + 10
#define SR_LICENSE_FAILURE					SR_BASE + 11
#define SR_LOCK_ERROR						SR_BASE + 12
#define SR_MEMORY_ERROR						SR_BASE + 13
#define SR_MRCP_SIGNALING					SR_BASE + 14
#define SR_NETWORK_ERROR					SR_BASE + 15
#define SR_NO_ACTIVE_GRAMMARS				SR_BASE + 16
#define SR_OS_SIGNALING						SR_BASE + 17
#define SR_PID_ERROR						SR_BASE + 18
#define SR_PTHREAD_ERROR					SR_BASE + 19
#define SR_QUEUE_ERROR						SR_BASE + 20
#define SR_RECOGNITION_FAILURE				SR_BASE + 21
#define SR_RECOGNITION_RESULT				SR_BASE + 22
#define SR_RESOURCE_NOT_AVAILABLE			SR_BASE + 23
#define SR_SERVER_ERROR						SR_BASE + 24
#define SR_SIP_SIGNALING					SR_BASE + 25
#define SR_SOCKET_ERROR						SR_BASE + 26
#define SR_SYSTEM_CALL_ERROR				SR_BASE + 27
#define SR_TOO_MANY_INIT_FAILURES			SR_BASE + 28

#define FAX_API_TIMEOUT_WAIT_FOR_RESPONSE	FAX_BASE + 1
#define FAX_BUILDFAX_FAILURE				FAX_BASE + 2
#define FAX_INVALID_DATA					FAX_BASE + 3
#define FAX_INVALID_PARM					FAX_BASE + 4
#define FAX_RESOURCE_NOT_AVAILABLE			FAX_BASE + 5
#define FAX_INITIALIZATION_ERROR			FAX_BASE + 6
#define FAX_TRANSMISSION_FAILURE			FAX_BASE + 7
#define FAX_FILE_IO_ERROR					FAX_BASE + 8
#define FAX_GENERAL_FAILURE					FAX_BASE + 9

#define TTS_AFFINITY_ERROR					TTS_BASE + 1
#define TTS_SERVER_ERROR					TTS_BASE + 2
#define TTS_DATA_NOT_FOUND					TTS_BASE + 3
#define TTS_FILE_IO_ERROR					TTS_BASE + 4
#define TTS_INCOMPLETE_MSG					TTS_BASE + 5
#define TTS_INITIALIZATION_ERROR			TTS_BASE + 6
#define TTS_INVALID_DATA					TTS_BASE + 7
#define TTS_INVALID_OPCODE					TTS_BASE + 8
#define TTS_INVALID_PARM					TTS_BASE + 9
#define TTS_IPC_ERROR						TTS_BASE + 10
#define TTS_MEMORY_ERROR					TTS_BASE + 11
#define TTS_MRCP_SIGNALING					TTS_BASE + 12
#define TTS_OS_SIGNALING					TTS_BASE + 13
#define TTS_PTHREAD_ERROR					TTS_BASE + 14
#define TTS_QUEUE_ERROR						TTS_BASE + 15
#define TTS_SOCKET_ERROR					TTS_BASE + 16
#define TTS_RESOURCE_NOT_AVAILABLE			TTS_BASE + 17
#define TTS_SIP_SIGNALING					TTS_BASE + 18

#define TEL_RESP_STARTUP					RESP_BASE + 1
#define TEL_RESP_SHUTDOWN					RESP_BASE + 2
#define TEL_RESP_APP_NOT_FOUND				RESP_BASE + 3
#define TEL_RESP_APP_RESPAWNING				RESP_BASE + 4
#define TEL_RESP_CLEANUP					RESP_BASE + 5
#define TEL_RESP_CANT_FIRE_APP				RESP_BASE + 6
#define TEL_RESP_DYNMGR_DEFUNCT				RESP_BASE + 7
#define TEL_RESP_INSTANCE_CONFLICT			RESP_BASE + 8
#define TEL_RESP_KILL_TIME					RESP_BASE + 9
#define TEL_RESP_KILLING					RESP_BASE + 10
#define TEL_RESP_HEARTBEAT					RESP_BASE + 11
#define TEL_RESP_NETWORK_SERVICES			RESP_BASE + 12
#define TEL_RESP_POLL_FAILED				RESP_BASE + 13
#define TEL_RESP_PROCESS_HAS_STOPPED		RESP_BASE + 14
#define TEL_RESP_PROCESS_STATIC_PORT 		RESP_BASE + 15
#define TEL_RESP_STATIC_PORT				RESP_BASE + 16
#define TEL_RESP_VERIFY_PROCESS				RESP_BASE + 17
#define TEL_RESP_USAGE						RESP_BASE + 18

// new mnemonics for telecom
#define TEL_REDIRECTOR_STARTUP				TEL_BASE + 200
#define TEL_REDIRECTOR_SHUTDOWN				TEL_BASE + 201
#define TEL_CALLMGR_STARTUP					TEL_BASE + 300
#define TEL_CALLMGR_SHUTDOWN				TEL_BASE + 301
#define TEL_MEDIAMGR_STARTUP				TEL_BASE + 302
#define TEL_MEDIAMGR_SHUTDOWN				TEL_BASE + 303

#define TEL_AFFINITY_ERROR					TEL_BASE + 100 
#define TEL_API_NOT_SUPPORTED				TEL_BASE + 101 
#define TEL_API_TIMEOUT_WAIT_FOR_RESPONSE	TEL_BASE + 102 
#define TEL_APP_NOT_SCHEDULED				TEL_BASE + 103 
#define TEL_BRIDGECALL_FAILURE				TEL_BASE + 104
#define TEL_CALLER_DISCONNECTED				TEL_BASE + 105
#define TEL_CANT_FIRE_APP					TEL_BASE + 106
#define TEL_CODEC_ERROR						TEL_BASE + 107
#define TEL_CONFIG_VALUE_NOT_FOUND			TEL_BASE + 108
#define TEL_DATA_MISMATCH					TEL_BASE + 109
#define TEL_DATA_NOT_FOUND					TEL_BASE + 110
#define TEL_DTMF_DATA						TEL_BASE + 111
#define TEL_EXOSIP_ERROR					TEL_BASE + 112
#define TEL_FILE_IO_ERROR					TEL_BASE + 113
#define TEL_HEARTBEAT						TEL_BASE + 114
#define TEL_INITIALIZATION_ERROR			TEL_BASE + 115
#define TEL_INVALID_DATA					TEL_BASE + 116
#define TEL_INVALID_ELEMENT					TEL_BASE + 117		// invalid elements of structure
#define TEL_INVALID_INTERNAL_VALUE			TEL_BASE + 118
#define TEL_INVALID_OPCODE					TEL_BASE + 119
#define TEL_INVALID_PARM					TEL_BASE + 120		// using it
#define TEL_INVALID_PARM_WARN				TEL_BASE + 121
#define TEL_INVALID_PORT					TEL_BASE + 122
#define TEL_IPC_ERROR						TEL_BASE + 123
#define TEL_KILL_PROCESS					TEL_BASE + 124
#define TEL_LICENSE_FAILURE					TEL_BASE + 125
#define TEL_MAX_TAG_FILES_EXCEEDED			TEL_BASE + 126
#define TEL_MEMORY_ERROR					TEL_BASE + 127
#define TEL_NETWORK_ERROR					TEL_BASE + 128
#define TEL_NOTIFYCALLER_FAILED				TEL_BASE + 129
#define TEL_OS_SIGNALING					TEL_BASE + 130
#define TEL_PHRASE_DIR_EMPTY				TEL_BASE + 131
#define TEL_PHRASE_FILENAME_BAD				TEL_BASE + 132
#define TEL_PID_ERROR						TEL_BASE + 133
#define TEL_PTHREAD_ERROR					TEL_BASE + 134
#define TEL_PTHREAD_ALREADY_RUNNING			TEL_BASE + 135
#define TEL_PROCESS_NOT_FOUND				TEL_BASE + 136
#define TEL_RECORD_ERROR					TEL_BASE + 137
#define TEL_REMOTE_NOT_CONNECTED			TEL_BASE + 138
#define TEL_RESOURCE_NOT_AVAILABLE			TEL_BASE + 139
#define TEL_RTP_ERROR						TEL_BASE + 140
#define TEL_SCHEDULE_CALL_FAILED			TEL_BASE + 141
#define TEL_SIP_SIGNALING					TEL_BASE + 142
#define TEL_SNMP_ERROR						TEL_BASE + 143
#define TEL_SPEAK_ERROR						TEL_BASE + 144
#define TEL_STREAMING						TEL_BASE + 145
#define TEL_STREAMING_ERROR					TEL_BASE + 146
#define TEL_SYSTEM_CALL_ERROR				TEL_BASE + 147
#define TEL_TAGS_NOT_LOADED					TEL_BASE + 148
#define TEL_TELECOM_NOT_INIT				TEL_BASE + 149
#define TEL_UNEXPECTED_RESPONSE				TEL_BASE + 150
#define TEL_UNKNOWN_EVENT					TEL_BASE + 151
#define TEL_UNKNOWN_PORT					TEL_BASE + 152
#define TEL_REGISTRATION_ERROR				TEL_BASE + 153

#define AVB_INITIALIZATION_ERROR    AVB_BASE + 1
#define AVB_ENROLLTRAIN_ERROR       AVB_BASE + 2
#define AVB_VERIFY_ERROR            AVB_BASE + 3
#define AVB_INDV_THRESHOLD_ERROR	AVB_BASE + 4
#define AVB_IO_ERROR                AVB_BASE + 5




//extern int LogARCMsg(char *parmModule, int parmMode, char *parmResourceName,
//	char *parmPT_Object, char *parmApplName, int parmMsgId,  char *parmMsg); 
// ----------------------------------------------------------------------------
// END - New Logging declarations start here
// ----------------------------------------------------------------------------

#define	TEL_MSG_VERSION		1
#define TEL_MSG_BASE		1000

/* TEL_UnscheduleCall */
#define TEL_EUNLINK 		1
#define TEL_EPLACEDCALL		2 
#define TEL_EOCSERROR		3

#define TEL_EUNLINK_MSG "Failed to unschedule call. Cannot remove file <%s>. errno=%d. [%s]"
#define TEL_EPLACEDCALL_MSG "Failed to unschedule call. Call with cdf <%s> has already been placed."
#define TEL_EOCSERROR_MSG "Failed to unschedule call. Call attempt for <%s> has already failed." 

/* TEL_StartNewApp */
#define TEL_E_NULL_APP          1146
#define TEL_E_NULL_DNIS         1147
#define TEL_E_NULL_DNIS_MSG	"Null DNIS passed"
#define TEL_E_APP_ACCESS        1148
#define TEL_E_APP_ACCESS_MSG	"Cannot access application <%s>, errno=%d. [%s]"
#define TEL_E_CUR_DIR           1149
#define TEL_E_CUR_DIR_MSG	"Cannot get current working dir. errno=%d. [%s]"
#define TEL_E_EXEC              1150
#define TEL_E_EXEC_MSG		"Failed to start application <%s>. errno=%d. [%s]"
#define TEL_START_NEW_APP       1151
#define TEL_START_NEW_APP_MSG	"Starting new application <%s>."     
#define TEL_E_CHDIR             1152
#define TEL_E_CHDIR_MSG "Cannot change directory to <%s>. errno=%d. [%s]"
#define TEL_E_SEND_REQ          1153
#define TEL_E_SEND_REQ_MSG	"Failed to send application request to Responsibility." 
#define TEL_E_RECV_REQ          1154
#define TEL_E_RECV_REQ_MSG	"Failed to receive application request reply from Responsibility."
#define TEL_E_APPNOTSCHED       1155
#define TEL_E_APPNOTSCHED_MSG	"Application <%s> is not scheduled to fire at this time."
#define TEL_E_SHMAT             1156
#define TEL_E_SHMAT_MSG		"Cannot attach to shmem. key=%ld. errno=%d. [%s]"
#define TEL_E_MSGGET            1157
#define TEL_E_MSGGET_MSG "Cannot attach to dynamic manager message queue. errno=%d. [%s]"



// old mnemonics for telecom - '// using it' means still using it.
#define TEL_NO_FILE					TEL_BASE+4
#define TEL_CANT_OPEN_FILE			TEL_BASE+5
#define TEL_CANT_PLAY_FILE			TEL_BASE+6
#define TEL_CANT_ACCESS_FILE		TEL_BASE+7
#define TEL_CANT_ACCESS_DIRECTORY	TEL_BASE+8
#define TEL_FAILED_TO_CHANGE_VOLUME	TEL_BASE+9
#define TEL_FAILED_TO_GET_VOLUME	TEL_BASE+10
#define TEL_FAILED_TO_CHANGE_SPEED	TEL_BASE+11
#define TEL_INVALID_PHONE_NO		TEL_BASE+12
#define TEL_INVALID_PHONE_NO_2		TEL_BASE+13
#define TEL_CANT_FORM_DIALSTRING	TEL_BASE+14
#define TEL_CALLER_STILL_CONNECTED	TEL_BASE+15
#define TEL_TERMINATION_CAUSE		TEL_BASE+16
#define TEL_GOT_DISCONNECT_TONE		TEL_BASE+17
#define TEL_CANT_OPEN_FILE_ASYNC	TEL_BASE+22
#define TEL_CANT_PLAY_FILE_ASYNC	TEL_BASE+23
#define TEL_FAILED_TO_GET_DIGIT		TEL_BASE+24
#define TEL_FAILED_TO_CLEAR_DIGIT	TEL_BASE+25
#define TEL_CANT_STOP_ASYNC_PHRASE	TEL_BASE+26
#define TEL_FILENAME_TOO_LONG		TEL_BASE+27
#define TEL_CANT_OVERWRITE_FILE		TEL_BASE+28
#define TEL_FAILED_TO_RECORD		TEL_BASE+29
#define TEL_IO_STOPPED				TEL_BASE+30
#define TEL_SUFFIX_MISMATCH			TEL_BASE+31
#define TEL_OUTBOUND_CALL_FAILED	TEL_BASE+32
#define TEL_BAD_DIAL_SEQUENCE		TEL_BASE+33
#define TEL_CANT_OPEN_VOICE_DEVICE	TEL_BASE+34
#define TEL_NO_CONNECT_UNLESS_DIAL_OUT	TEL_BASE+35
#define TEL_CALLER_DISCONNECTED_ON_PORT	TEL_BASE+36
#define TEL_AGENT_DISCONNECTED_ON_PORT	TEL_BASE+37
#define TEL_ALREADY_CALLED		TEL_BASE+38
#define TEL_NO_RLT_WITH_DIAL_OUT	TEL_BASE+39
#define TEL_ISDN_ONLY			TEL_BASE+40
#define TEL_CANT_GET_CHANNEL_STATUS	TEL_BASE+41
#define TEL_LOGMGR_ERROR		TEL_BASE+43
#define TEL_OBJDB_NOT_SET		TEL_BASE+44 /* snmp */
#define TEL_CANT_ACCESS_SPEECH_DIR	TEL_BASE+45 /* snmp */
#define TEL_APP_TO_APP_INFO_TOO_LONG	TEL_BASE+46 
#define TEL_INVALID_CONTROL_KEYS	TEL_BASE+47 
#define TEL_INVALID_GLOBAL_PARM		TEL_BASE+48 
#define TEL_CANT_CREATE_FIFO		TEL_BASE+49
#define TEL_CANT_OPEN_FIFO		TEL_BASE+50
#define TEL_CANT_DELETE_FILE		TEL_BASE+51
#define TEL_NOT_IN_WHISPER_MODE		TEL_BASE+52
#define TEL_CANT_GET_ISPBASE		TEL_BASE+53
#define TEL_DPD_CHAR_WARN		TEL_BASE+54
#define TEL_CANT_OUTPUT_DTMF		TEL_BASE+55
#define TEL_TIMEOUT_GET_NET_SLOT_DYNMGR TEL_BASE+56
#define TEL_NO_LAST_PHRASE 		TEL_BASE+57
#define TEL_CANT_SPEAK_LAST_PHRASE	TEL_BASE+58
#define TEL_TRUNCATING_INPUT		TEL_BASE+59
#define TEL_INVALID_INPUT_GIVEN		TEL_BASE+60
#define TEL_UNDEFINED_ALPHA_CHAR	TEL_BASE+61
#define TEL_EXCEEDED_RETRIES		TEL_BASE+62
#define TEL_INVALID_ALPHA_INPUT		TEL_BASE+63
#define TEL_EXTRA_STAR			TEL_BASE+64
#define TEL_ECDFNOTFOUND		TEL_BASE+66
#define TEL_ECDFINFO			TEL_BASE+67
#define TEL_CANT_RENAME_FILE		TEL_BASE+68
#define TEL_CANT_FIND_FILE		TEL_BASE+69
#define TEL_CANT_FIND_IN_FILE		TEL_BASE+70
#define TEL_POLL_FAILED			TEL_BASE+71
#define TEL_CUST_DATA_TOO_LONG		TEL_BASE+72

#define TEL_CANT_WRITE_TO_FILE		TEL_BASE+74
#define TEL_INVALID_TAG_FORMAT		TEL_BASE+75
#define TEL_EMPTY_LIST_TAG		TEL_BASE+76
#define TEL_TOO_MANY_ITEMS_IN_LIST_TAG	TEL_BASE+77
#define TEL_BAD_PHRASE_MENU_TMP		TEL_BASE+78
#define TEL_BAD_PHRASE_MENU		TEL_BASE+79
#define TEL_BAD_NAMEVALUE_FORMAT	TEL_BASE+80
#define TEL_UNKNOWN_SECTION_PARM	TEL_BASE+81
#define TEL_NULL_TAG			TEL_BASE+82
#define TEL_NO_DEFINED_TAG		TEL_BASE+83
#define TEL_INVALID_SECTION_PARM	TEL_BASE+84
#define TEL_DUPLICATE_TAG		TEL_BASE+85
#define TEL_NO_TAG_FROM_FILE		TEL_BASE+86

#define TEL_SECTION_NOT_FOUND		TEL_BASE+88
#define TEL_GAVE_UP			TEL_BASE+89
#define TEL_OVERRIDE_PARM		TEL_BASE+90
#define TEL_INVALID_OVERRIDE		TEL_BASE+91
#define TEL_UNKNOWN_OVERRIDE		TEL_BASE+92
#define TEL_FAILED_TO_SPEAK_TAG		TEL_BASE+93
#define TEL_SPOKE_TAG			TEL_BASE+94
#define TEL_UNABLE_TO_FIND_TAG		TEL_BASE+95
#define TEL_OVERRIDE_TAG		TEL_BASE+96

/* New messages put in for H323 */
#define TEL_CANT_WRITE_FIFO		TEL_BASE+100 
#define TEL_CANT_READ_FIFO		TEL_BASE+101 

/* TEL_SpeakMessages that are apparently required 3/8/2001 */
/* gpw threw away a bunch of other crap */
#define TEL_ENONNUMERICSTR	1042
#define TEL_CANT_SPEAK		1045
#define TEL_SPEAK_WARN		1046  


#define TEL_TELECOM_NOT_INIT_MSG "Telecom Services has not been intialized."
#define TEL_INVALID_PHONE_NO_MSG "Cannot form a 4, 7 or 10 digit phone number from <%s>."
#define TEL_INVALID_PHONE_NO_2_MSG "Invalid phone number: <%s>."
#define TEL_CANT_FORM_DIALSTRING_MSG "Cannot form a valid dial string from <%s>."
#define TEL_IO_STOPPED_MSG "The current I/O activity is forceably stopped."	
#define TEL_CALLER_STILL_CONNECTED_MSG "SECOND_PARTY_NO_CALLER is not valid because the caller is still connected."

#define TEL_OUTBOUND_CALL_FAILED_MSG	"Outbound call to <%s> failed on port %d. rc=%d. [%s]"
#define TEL_NO_CONNECT_UNLESS_DIAL_OUT_MSG "You cannot CONNECT. No successful DIAL_OUT is currently in effect."
#define TEL_CALLER_DISCONNECTED_ON_PORT_MSG "Caller (1st party) disconnected on port %d."
#define TEL_AGENT_DISCONNECTED_ON_PORT_MSG "Agent (2nd party) disconnected on port %d."
#define TEL_INVALID_GLOBAL_PARM_MSG "Invalid global parameter value (%d) specified for %s. Valid values: %s."		
#define TEL_CANT_CREATE_FIFO_MSG "Failed to create fifo <%s>. errno=%d. [%s]"
#define TEL_CANT_OPEN_FIFO_MSG 	"Failed to open fifo <%s>. errno=%d. [%s]"
#define TEL_ISDN_ONLY_MSG  "This API is for use with ISDN only."
#define TEL_CANT_DELETE_FILE_MSG  "Failed to delete <%s>. errno=%d. [%s]"
#define TEL_CANT_GET_ISPBASE_MSG "Environment variable $ISPBASE is not set."
#define TEL_CANT_SPEAK_MSG "Failed to speak <%s> for informat=<%s>, outformat=<%s> because %s."
#define TEL_SPEAK_WARN_MSG "Invalid characters found in <%s> for informat=<%s>, outformat=<%s>."
#define TEL_ECDFNOTFOUND_MSG "CDF (%s) not found." 
#define TEL_ECDFINFO_MSG "Unable to get CDF info (%s)."

#define TEL_MAX_TAG_FILES_EXCEEDED_MSG	"Maximum number of tag files <%d> exceeded."
#define TEL_CANT_WRITE_TO_FILE_MSG	
#define TEL_INVALID_TAG_FORMAT_TMP_MSG	"Invalid format found in temporary tag file <%s> on line number %d. <%s>."
#define TEL_INVALID_TAG_FORMAT_MSG	"Invalid format found in tag file <%s> on line number %d. <%s>."	
#define TEL_EMPTY_LIST_TAG_TMP_MSG      "Empty list tag <%s> found in temporary file <%s> on line number %d."
#define TEL_EMPTY_LIST_TAG_MSG		"Empty list tag <%s> in file <%s> on line # %d, not counting blank or commented lines."
#define TEL_TOO_MANY_ITEMS_IN_LIST_TAG_MSG      "Failed to add item # %d in list tag=<%s>. Maximum list size is %d."
#define TEL_BAD_PHRASE_MENU_TMP_MSG     "Invalid tag <%s> for list tag <%s> in temporary phrase file <%s> on line number %d."
#define TEL_BAD_PHRASE_MENU_MSG "Invalid tag <%s> for list tag <%s> in file <%s> on line # %d, not counting blank or commented lines."
#define TEL_BAD_NAMEVALUE_FORMAT_MSG    "Invalid entry <%s> under section <%s> in file <%s>. Must be name=value pair."
#define TEL_UNKNOWN_SECTION_PARM_MSG    "Urecognized name in name=value pair <%s=%s> under section <%s> in file <%s>."
#define TEL_NULL_TAG_MSG        "Invalid tag <%s> in section <%s>. <%s> tag cannot be empty."
#define TEL_NO_DEFINED_TAG_MSG  "Invalid <%s>=<%s> entry in section <%s>. <%s> is not defined."
#define TEL_INVALID_SECTION_PARM_MSG "Invalid value entry <%s>=<%s> in section <%s>.  Valid values: %s."
#define TEL_DUPLICATE_TAG_MSG   "Duplicate tag <%s> found in file <%s>."
#define TEL_NO_TAG_FROM_FILE_MSG        "Unable to find tag for file <%s>. Cannot save loaded tags to <%s>."

#define TEL_TAGS_NOT_LOADED_MSG "No tags are currently loaded. TEL_LoadTags may not have completed successfully."
#define TEL_SECTION_NOT_FOUND_MSG "Section <%s> is not loaded."
#define TEL_GAVE_UP_MSG "Failed to get input after %d tries. Last try result: %s."
#define TEL_OVERRIDE_PARM_MSG "Parameter <%s> in section <%s>, originally <%s>,overriden to <%s>."
#define TEL_INVALID_OVERRIDE_MSG "Invalid override value <%s> specified for <%s>. Using original value of <%s>."
#define TEL_UNKNOWN_OVERRIDE_MSG "Invalid override attempt <%s> ignored."
#define TEL_TAGS_NOT_LOADED_MSG "No tags are currently loaded. TEL_LoadTags may not have completed successfully." 
#define TEL_FAILED_TO_SPEAK_TAG_MSG     "Failed to speak tag <%s>, content <%s>. rc=%d."
#define TEL_SPOKE_TAG_MSG       "Successfully spoke tag <%s>, content <%s>."
#define TEL_UNABLE_TO_FIND_TAG_MSG      "Unable to find tag <%s>."
#define TEL_OVERRIDE_TAG_MSG "Tag:%s, element:%s, originally <%s>, overriden to <%s>."

/* New messages for H323 */

#define TEL_CANT_WRITE_FIFO_MSG "Failed to write to <%s>. errno=%d. [%s]"
#define TEL_CANT_READ_FIFO_MSG "Failed to read from <%s>. errno=%d. [%s]"
#define TEL_UNEXPECTED_RESPONSE_MSG "Unexpected message: Got: {opcode,appPid,appRef}={%d,%d,%d}, expected {%d,%d,%d}."

#endif // LOG_MESSAGES_H