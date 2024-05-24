#ident	"@(#)TELmsg.h 97/02/07 2.2.0 Copyright 1996 Aumtech, Inc"
/*-----------------------------------------------------------------------------
Program:	TELmsg.h
Purpose:	ISP Telecom message mnemonic name definitions 
 		This file provides version number, base number and 
		mnemonic definitions which index into the GV_TELmsg array
		found in TELmsg.c.
Author: 	George Wenzel
Date:		02/13/96
Update:         02/13/96 G. Wenzel Added TEL_ECOMPRESSION message
Update:         02/29/96 G. Wenzel make messages #defines instead of enums
Update:         03/01/96 G. Wenzel renumbered all messages.
Update:		03/05/96 G. Wenzel added TEL_NO_SRC_PHR
Update:		03/06/96 G. Wenzel fixed numbering of fax msgs 10xx->11xx
Update:		03/14/96 G. Wenzel added TEL_E_DUP_CDR
Update:		03/14/96 G. Wenzel added TEL_W_2_CDR_KEYS
Update:		03/14/96 G. Wenzel added TEL_E_NULL_CDR_TIME
Update:		03/26/96 G. Wenzel added TEL_E_RING_NO_ANSWER
Update:		03/26/96 G. Wenzel added TEL_E_NO_HUMAN_OR_FAX
Update:		04/09/96 G. Wenzel added TEL_E_SUBSCRIBER, TEL_E_GET_FAX_INFO
Update:		04/11/96 G. Wenzel added TEL_BAD_IO_FORMATS, TEL_BAD_INFORMAT
Update:		04/11/96 G. Wenzel added TEL_BAD_OUTFORMAT
Update:		04/11/96 G. Wenzel changed TEL_ESPKDATE to TEL_CANT_SPEAK_RC
Update:		04/11/96 G. Wenzel removed TEL_EBADFORMAT
Update:		04/11/96 G. Wenzel changed TEL_EUNABLE2SPEAK to TEL_CANT_SPEAK
Update:		04/11/96 G. Wenzel removed TEL_EOUTINMISMATCH	1040
Update:		04/11/96 G. Wenzel added TEL_E_SPEAKING 
Update:		04/11/96 G. Wenzel added TEL_E_GET_FAX_PAGE
Update:		04/11/96 G. Wenzel added TEL_E_CREATE_FAX_PAGE
Update:		04/11/96 G. Wenzel added TEL_BAD_CHAR 1044
Update:         07/03/96 G. Wenzel added TEL_ELANGNOLOAD
Update:		07/03/96 G. Wenzel added TEL_BAD_VOC_COMP
Update:		07/03/96 G. Wenzel removed  TEL_ENOPVEFILE 1091 (not used)
Update:		07/19/96 S. Agate Added TEL_BAD_DOLLAR
Update:		07/24/97 mpb Added TEL_StartNewApp messages
Update:		08/21/97 sja Added TEL_ScheduleCall, TEL_UnscheduleCall,
Update:				TEL_GetCDFInfo & TEL_SpeakTTS messages
Update:		08/27/97 sja Added TEL_EADEVOPEN
Update:		05/09/00 gpw Added message numbers for use with LogARCMsg.
Update:		05/18/00 apj Added TEL_TIMEOUT_GET_NET_SLOT_DYNMGR.
Update:		05/26/00 gpw Added several messages; most for TEL_GetDTMF.
Update:		01/29/01 jmp Added #defines for Philips SIR errors
Update:		03/05/01 gpw revised TEL_PromptAndCollect set of msgs.
Update:		03/07/01 gpw added TEL_UNKNOWN_OVERRIDE_MSG 	
Update:		03/09/01 gpw added TEL_CANT_WRITE_TO_FILE		
Update:		04/17/01 gpw added TEL_TOO_MANY_ITEMS_IN_LIST_TAG
Update:		08/20/01 apj added TEL_FAILED_TO_GET_SPEED.
-------------------------------------------------------------------------------
 * Copyright (c) 2001, Aumtech, Inc.
 * All Rights Reserved.
 *
 * This is an unpublished work of Aumtech which is protected under 
 * the copyright laws of the United States and a trade secret
 * of Aumtech. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of Aumtech.
 *----------------------------------------------------------------------------*/

#ifndef _TELMSG_H	/* avoid multiple include problems	*/
#define _TELMSG_H

#define	TEL_MSG_VERSION		1
#define TEL_MSG_BASE		1000

#define TEL_ECALLDISCONN	1000
#define TEL_EAPPTERMINATE	1001
#define TEL_E_DUP_CDR		1002
#define TEL_EVENDORFAIL		1003
#define TEL_EVENDORACTIVE	1004
#define TEL_ECURRENTDIR		1005
#define TEL_EAPPREG		1006
#define TEL_EUNDEFINED		1007
#define TEL_ESETCHANPARM	1008
#define TEL_ECOMPRESSION	1009
#define TEL_W_2_CDR_KEYS	1010
#define TEL_E_NULL_CDR_TIME	1011


/* InitTelecom messages. */
#define TEL_EOPENSPEECHFIL	1020
#define TEL_EREPTCALL		1021
#define TEL_EDYNANALOG		1022
#define TEL_EAVDEVOPEN		1023
#define TEL_EANDEVOPEN		1024

/* AnswerCall messages. */
#define TEL_EWAITNXTCAL		1030
#define TEL_ESTATICISDN		1031
#define TEL_EANSTIMEOUT		1032

/* Output Phrase messages. */
#define TEL_EDATEFMT		1041
#define TEL_ENONNUMERICSTR	1042
#define TEL_EDIGITLENGTH	1043
#define TEL_BAD_CHAR		1044
#define TEL_CANT_SPEAK		1045
#define TEL_CANT_SPEAK_RC	1046
#define TEL_ESPKTIMDIGRNG	1047
#define TEL_ETVSPKTIM		1048
#define TEL_BAD_IO_FORMATS	1049
#define TEL_BAD_INFORMAT	1050
#define TEL_BAD_OUTFORMAT	1051
#define TEL_BAD_DOLLAR		1052

/* InputDTMF messages. */
#define TEL_ETYPEOPTION		1060
#define TEL_EUSERRETRY		1061
#define TEL_EMAXUSERRETRY	1062
#define TEL_EINPUTNONNUM	1063
#define TEL_EIGNORESTAR		1064
#define TEL_ETERMAUTOSKIP	1065
#define TEL_EALPHAINPUT		1066
#define TEL_EINPUTLENGTH	1067
#define TEL_EINPUTCHAR		1068
#define TEL_EINPUTDIGIT		1069

/* Bridge and Transfer. */
#define TEL_EBADCALLOUT		1080
#define TEL_ERESOURCE		1081
#define TEL_ECONVPHN		1082
#define TEL_ECONSTRPHN		1083
#define TEL_EDIALSTR		1084
#define TEL_E_RING_NO_ANSWER	1085
#define TEL_E_NO_HUMAN_OR_FAX	1086

/* Speech Recognition APIs  */
#define TEL_ESPKPHRASE		1090
/*				1091 */
#define TEL_ESPKVOCAB		1092
#define TEL_BAD_VOC_COMP	1093

/* Input Voice API */
#define TEL_EVOCNOLOAD		1100
#define TEL_ETIMEOUTIN		1101
#define TEL_EUNEXPVAL		1102
#define TEL_EUNKNOWN		1103
#define TEL_ETVSIRGO		1104
#define TEL_ELANGNOLOAD		1105

/* Phrase API messages. */

#define TEL_EFILETOPHRASE	1110
#define TEL_EPHRASETOFILE	1111
#define TEL_ESAMEPHRASE		1112
#define TEL_ECREATEPHRASE	1113
#define TEL_ECOPYPHRASE		1114
#define TEL_EPHRASEEXISTS	1115
#define TEL_EPHRASETYPE		1116
#define TEL_ESETPHRTYPE		1117
#define TEL_EBADPHRTYPE		1118
#define TEL_EDELETEPHRASE	1119
#define TEL_ERECORDPHRASE	1120
#define TEL_ERECORDTIME		1121
#define TEL_ERECORDTYPE		1122
#define TEL_EPHRASECOMP		1123
#define TEL_ESTARTPOS		1124
#define TEL_EFILEFORMAT		1125
#define TEL_NO_SRC_PHR		1126
#define TEL_E_SPEAKING		1127


/* FAX messages. */
#define TEL_E_GET_FAX_PAGE	1129
#define TEL_ECREATEFAX		1130
#define TEL_ERECVFAX		1131
#define TEL_EFAXPAGES		1132
#define TEL_ENUMFAXPAGES	1133
#define TEL_E_SUBSCRIBER	1134
#define TEL_E_GET_FAX_INFO	1135
#define TEL_E_SET_FAX_INFO	1136
#define TEL_E_NO_FAX_QUEUE	1137
#define TEL_E_VOICE_2_FAX	1138
#define TEL_E_BAD_START_PHR	1139
#define TEL_E_CREATE_FAX_Q	1140
#define TEL_E_SET_TYPE_COMP	1141
#define TEL_E_CONVERT_FILE	1142
#define TEL_E_ADD_FAX_PAGE	1143
#define TEL_E_CREATE_TEMP	1144
#define TEL_E_CREATE_FAX_PAGE	1145

#define TEL_E_NULL_APP		1146
#define TEL_E_NULL_DNIS		1147
#define TEL_E_APP_ACCESS 	1148
#define TEL_E_CUR_DIR 		1149
#define TEL_E_EXEC		1150
#define TEL_START_NEW_APP	1151
#define TEL_E_CHDIR		1152
#define TEL_E_SEND_REQ		1153
#define TEL_E_RECV_REQ		1154
#define TEL_E_APPNOTSCHED	1155
#define TEL_E_SHMAT		1156
#define TEL_E_MSGGET		1157

/*
 *	NOTE:	1158, 1159, 1160 are unused
 */

/* TEL_ScheduleCall Messages 	(1161 - 1180) */
#define TEL_ETIMEFORMAT		1161
#define TEL_EINVALIDYEAR	1162
#define TEL_EINVALIDMONTH	1163
#define TEL_EINVALIDDAY		1164
#define TEL_EINVALIDHOUR	1165
#define TEL_EINVALIDMINUTE	1166
#define TEL_EINVALIDSECOND	1167
#define TEL_EMAKECDFNAME	1168
#define TEL_ESCHEDCALL		1169


/* TEL_UnscheduleCall Messages 	(1181 - 1200) */
#define TEL_EISPBASE		1181
#define TEL_EUNLINK		1182
#define TEL_EPLACEDCALL		1183
#define TEL_EOCSERROR		1184


/* TEL_GetCDFInfo Messages 	(1201 - 1220) */
#define TEL_ECDFINFO		1201
#define TEL_ECDFNOTFOUND	1202


/* TEL_SpeakTTS Messages 	(1221 - 1240) */
#define TEL_EAFILERM		1221
#define TEL_EACREATEFIFO	1222
#define TEL_EAOPENFIFOREAD	1223
#define TEL_EAOPENFIFOWRITE	1224
#define TEL_EAFIFOWRITE		1225
#define TEL_EAFIFOREAD		1226
#define TEL_EARESULTFILE 	1227
#define TEL_EAUSERSTRLEN	1228
#define TEL_EAFILERENAME	1229

/* Messages for LogARCMsg */
#define TEL_BASE			1300
#define TEL_TELECOM_NOT_INIT		TEL_BASE+1
#define TEL_INVALID_PARM		TEL_BASE+2
#define TEL_INVALID_PARM_WARN		TEL_BASE+3
#define TEL_NO_FILE			TEL_BASE+4
#define TEL_CANT_OPEN_FILE		TEL_BASE+5
#define TEL_CANT_PLAY_FILE		TEL_BASE+6
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
#define TEL_DX_LISTEN_FAILED		TEL_BASE+18
#define TEL_DT_LISTEN_FAILED		TEL_BASE+19
#define TEL_GET_XMIT_SLOT_FAILED	TEL_BASE+20
#define TEL_GET_XMIT_SLOT_FAILED2	TEL_BASE+21
#define TEL_CANT_OPEN_FILE_ASYNC	TEL_BASE+22
#define TEL_CANT_PLAY_FILE_ASYNC	TEL_BASE+23
#define TEL_FAILED_TO_GET_DIGIT		TEL_BASE+24
#define TEL_FAILED_TO_CLEAR_DIGIT	TEL_BASE+25
#define TEL_CANT_STOP_ASYNC_PHRASE	TEL_BASE+26
#define TEL_FILENAME_TOO_LONG		TEL_BASE+27
#define TEL_CANT_OVERWRITE_FILE		TEL_BASE+28
#define TEL_FAILED_TO_RECORD		TEL_BASE+29
#define TEL_IO_STOPPED			TEL_BASE+30
#define TEL_SUFFIX_MISMATCH		TEL_BASE+31
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
#define TEL_SCHEDULE_CALL_FAILED	TEL_BASE+42
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
#define TEL_CANT_WRITE_TO_FILE		TEL_BASE+65
#define TEL_FAILED_TO_GET_SPEED		TEL_BASE+66

/* TEL_LoadTags, TEL_PromptAndCollect, TEL_Save Messages */
/* Note a few gaps were left intentionally in case we had to add msgs */
#define TEL_MAX_TAG_FILES_EXCEEDED	TEL_BASE+90
#define TEL_INVALID_TAG_FORMAT		TEL_BASE+91
#define TEL_DUPLICATE_TAG		TEL_BASE+93
#define TEL_EMPTY_LIST_TAG		TEL_BASE+95
#define TEL_BAD_PHRASE_MENU_TMP		TEL_BASE+96
#define TEL_BAD_PHRASE_MENU		TEL_BASE+97
#define TEL_BAD_NAMEVALUE_FORMAT	TEL_BASE+98
#define TEL_UNKNOWN_SECTION_PARM	TEL_BASE+99
#define TEL_INVALID_SECTION_PARM	TEL_BASE+100 
#define TEL_NULL_TAG			TEL_BASE+101
#define TEL_NO_DEFINED_TAG		TEL_BASE+102
#define TEL_SPOKE_TAG			TEL_BASE+103
#define TEL_FAILED_TO_SPEAK_TAG		TEL_BASE+106
#define TEL_UNABLE_TO_FIND_TAG		TEL_BASE+107
#define TEL_SECTION_NOT_FOUND		TEL_BASE+108
#define TEL_TAGS_NOT_LOADED		TEL_BASE+109
#define TEL_OVERRIDE_PARM		TEL_BASE+110
#define TEL_OVERRIDE_TAG		TEL_BASE+111
#define TEL_INVALID_OVERRIDE		TEL_BASE+112
#define TEL_UNKNOWN_OVERRIDE 		TEL_BASE+113
#define TEL_GAVE_UP			TEL_BASE+114
#define TEL_NO_TAG_FROM_FILE		TEL_BASE+115
#define TEL_TOO_MANY_ITEMS_IN_LIST_TAG	TEL_BASE+116

#define TEL_TELECOM_NOT_INIT_MSG "Telecom Services has not been intialized."
#define TEL_INVALID_PHONE_NO_MSG "Cannot form a 4, 7 or 10 digit phone number from <%s>."
#define TEL_INVALID_PHONE_NO_2_MSG "Invalid phone number: <%s>."
#define TEL_CANT_FORM_DIALSTRING_MSG "Cannot form a valid dial string from <%s>."
#define TEL_DT_LISTEN_FAILED_MSG "dt_listen_failed. [%s]"
#define TEL_DX_LISTEN_FAILED_MSG "dx_listen_failed. [%s]"
#define TEL_GET_XMIT_SLOT_FAILED_MSG "dx_getxmitslot failed. [%s]"
#define TEL_GET_XMIT_SLOT_FAILED2_MSG "Failed to get xmit slot for <%s>. [%s]"
#define TEL_CANT_OPEN_VOICE_DEVICE_MSG "Unable to open voice device <%s>. errno=%d. [%s]"

#define TEL_IO_STOPPED_MSG "The current I/O activity is forceably stopped."	
#define TEL_CALLER_STILL_CONNECTED_MSG "SECOND_PARTY_NO_CALLER is not valid because the caller is still connected."

#define TEL_OUTBOUND_CALL_FAILED_MSG	"Outbound call to <%s> failed on port %d. rc=%d. [%s]"
#define TEL_NO_CONNECT_UNLESS_DIAL_OUT_MSG "You cannot CONNECT. No successful DIAL_OUT is currently in effect."
#define TEL_CALLER_DISCONNECTED_ON_PORT_MSG "Caller (1st party) disconnected on port %d."
#define TEL_AGENT_DISCONNECTED_ON_PORT_MSG "Agent (2nd party) disconnected on port %d."
#define TEL_INVALID_GLOBAL_PARM_MSG "Invalid global parameter value (%s) specified for %d. Valid values: %s."		
#define TEL_CANT_CREATE_FIFO_MSG "Failed to create fifo <%s>. errno=%d. [%s]"
#define TEL_CANT_OPEN_FIFO_MSG 	"Failed to open fifo <%s>. errno=%d. [%s]"
#define TEL_ISDN_ONLY_MSG  "This API is for use with ISDN only."
#define TEL_CANT_DELETE_FILE_MSG  "Failed to delete <%s>. errno=%d. [%s]"
#define TEL_CANT_GET_ISPBASE_MSG "Environment variable $ISPBASE is not set."
#define TEL_TIMEOUT_GET_NET_SLOT_DYNMGR_MSG "Timed out waiting for network device time slot from the Dynamic Manager. Timeout value=%d seconds."


#define TEL_TELECOM_NOT_INIT_MSG "Telecom Services has not been intialized."
#define TEL_INVALID_PHONE_NO_MSG "Cannot form a 4, 7 or 10 digit phone number from <%s>."
#define TEL_INVALID_PHONE_NO_2_MSG "Invalid phone number: <%s>."
#define TEL_CANT_FORM_DIALSTRING_MSG "Cannot form a valid dial string from <%s>."
#define TEL_DT_LISTEN_FAILED_MSG "dt_listen_failed. [%s]"
#define TEL_DX_LISTEN_FAILED_MSG "dx_listen_failed. [%s]"
#define TEL_GET_XMIT_SLOT_FAILED_MSG "dx_getxmitslot failed. [%s]"
#define TEL_GET_XMIT_SLOT_FAILED2_MSG "Failed to get xmit slot for <%s>. [%s]"

#define TEL_MAX_TAG_FILES_EXCEEDED_MSG "Maximum number of tag files <%d> exceeded."
#define TEL_INVALID_TAG_FORMAT_TMP_MSG	"Invalid format found in temporary tag file <%s> on line number %d. <%s>."
#define TEL_INVALID_TAG_FORMAT_MSG	"Invalid format found in tag file <%s> on line number %d. <%s>."
#define TEL_DUPLICATE_TAG_MSG	"Duplicate tag <%s> found in file <%s>."
#define TEL_EMPTY_LIST_TAG_TMP_MSG	"Empty list tag <%s> found in temporary file <%s> on line number %d."
#define TEL_EMPTY_LIST_TAG_MSG	"Empty list tag <%s> in file <%s> on line # %d, not counting blank or commented lines."
#define TEL_BAD_PHRASE_MENU_TMP_MSG	"Invalid tag <%s> for list tag <%s> in temporary phrase file <%s> on line number %d."
#define TEL_BAD_PHRASE_MENU_MSG	"Invalid tag <%s> for list tag <%s> in file <%s> on line # %d, not counting blank or commented lines."
#define TEL_BAD_NAMEVALUE_FORMAT_MSG	"Invalid entry <%s> under section <%s> in file <%s>. Must be name=value pair."
#define TEL_UNKNOWN_SECTION_PARM_MSG	"Urecognized name in name=value pair <%s=%s> under section <%s> in file <%s>."
#define TEL_NULL_TAG_MSG	"Invalid tag <%s> in section <%s>. <%s> tag cannot be empty."
#define TEL_NO_DEFINED_TAG_MSG	"Invalid <%s>=<%s> entry in section <%s>. <%s> is not defined."
#define TEL_INVALID_SECTION_PARM_MSG "Invalid value entry <%s>=<%s> in section <%s>.  Valid values: %s."
#define TEL_NO_TAG_FROM_FILE_MSG	"Unable to find tag for file <%s>. Cannot save loaded tags to <%s>."
#define TEL_TOO_MANY_ITEMS_IN_LIST_TAG_MSG	"Failed to add item # %d in list tag=<%s>. Maximum list size is %d."
#define TEL_FAILED_TO_SPEAK_TAG_MSG	"Failed to speak tag <%s>, content <%s>.  rc=%d."
#define TEL_UNABLE_TO_FIND_TAG_MSG	"Unable to find tag <%s>."
#define TEL_SPOKE_TAG_MSG	"Successfully spoke tag <%s>, content <%s>."
#define TEL_SECTION_NOT_FOUND_MSG "Section <%s> is not loaded."
#define TEL_TAGS_NOT_LOADED_MSG "No tags are currently loaded. TEL_LoadTags may not have completed successfully."
#define TEL_OVERRIDE_PARM_MSG "Parameter <%s> in section <%s>, originally <%s>, overriden to <%s>."
#define TEL_INVALID_OVERRIDE_MSG "Invalid override value <%s> specified for <%s>. Using original value of <%s>."
#define TEL_UNKNOWN_OVERRIDE_MSG "Invalid override attempt <%s> ignored."
#define TEL_OVERRIDE_TAG_MSG "Tag:%s, element:%s, originally <%s>, overriden to <%s>."
#define TEL_GAVE_UP_MSG "Failed to get input after %d tries. Last try result: %s."

/* Philips TEL_ SIR error messages 1500 - 1525 */

#define	SIR_BASE	1500

#define	SIR_EUNABLETORELEASESIR		SIR_BASE+1
#define	SIR_EDSSPSADESTROYFAILED	SIR_BASE+2
#define	SIR_EDSRMDESTROYFAILED		SIR_BASE+3
#define	SIR_USINGEXISTINGRESOURCE	SIR_BASE+4
#define	SIR_EGENERALRESOURCEMGRERR	SIR_BASE+5
#define	SIR_ENOSPEECHRESOURCEAVAIL	SIR_BASE+6
#define	SIR_EDSSPOPENFAILED		SIR_BASE+7
#define	SIR_EUNABLETOGETSIRRESOURC	SIR_BASE+8
#define	SIR_EUNABLETOACTIVATERESOU	SIR_BASE+9
#define	SIR_EVADERROR			SIR_BASE+10
#define	SIR_EDXXERROR			SIR_BASE+11
#define	SIR_EDSSPPUTENDOFSENTFAIL	SIR_BASE+12
#define	SIR_EDSSPRETRIEVESUBUFSIZE	SIR_BASE+13
#define	SIR_EDSSPRETRIEVESUPATHS	SIR_BASE+14
#define	SIR_EDSSPDESTROYRESULTFAIL	SIR_BASE+15
#define	SIR_EDSSPPUTAUDIODATA		SIR_BASE+16
#define	SIR_EFUNCTIONPARAMETERERR	SIR_BASE+17
#define	SIR_EFILEIOERROR		SIR_BASE+19
#define	SIR_EDSRMCREATEFAILED		SIR_BASE+20
#define	SIR_EDSSPSACREATEFAILED		SIR_BASE+21
#define	SIR_ERMSTARTREGISTRFAILED	SIR_BASE+22
#define	SIR_SUCCESSMSG			SIR_BASE+23
#define	SIR_EDSSPREGONEVENT		SIR_BASE+24
#define	SIR_EUNKNOWNEVENT		SIR_BASE+25
#endif	/* _TELMSG_H	*/
