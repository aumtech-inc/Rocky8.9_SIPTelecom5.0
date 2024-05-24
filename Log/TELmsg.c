#ident	"@(#)TELmsg.c 99/05/11 2.8.2 Copyright 1999 Aumtech, Inc."
/*-----------------------------------------------------------------------------
Program:	TELmsg.c 
Purpose:	ISP Telecom message definitions 
		To define message body format definitions for ISP telecom 
		messages. The index of a message definition is the numeric 
		value of its mnemonic defined in TELmsg.h.
		To initialize TEL_msg_num, the total number of messages which 
		are currently defined.
 		CAUTION: The message index must match to the numeric value 
				of its mnemonic defined in TELmsg.h
Author:		J. Huang
Date:		04/26/94
Update:		04/03/97 M. Bhide Added %s to TEL_EALPHAINPUT
Update:		07/24/97 M. Bhide Added defines for StartNewApp
Update:		08/21/97 sja added TEL_ScheduleCall, TEL_UnscheduleCall,
Update:		08/21/97 sja added TEL_GetCDFInfo & TEL_SpeakTTS messages
Update:		08/27/97 sja added TEL_EAVDEVOPEN
Update:		08/27/97 sja added TEL_EANDEVOPEN
Update:		01/26/98 mpb removed \n after TELSpeakTTS api messages.
Update:		04/12/99 mpb chgd msg# 1080 to add port # & retcode error msg.
Update:		05/11/99 gpw updated msgs 1181, 1182, 1184.
Update:		05/11/99 gpw changed puntuation in some messages.
Update: 07/28/99 mpb Added [%s] to TEL_E_NO_HUMAN_OR_FAX message.
-------------------------------------------------------------------------------
 * Copyright (c) 1999, Aumtech, Inc.
 * All Rights Reserved.
 *
 * This is an unpublished work of Aumtech which is protected under 
 * the copyright laws of the United States and a trade secret
 * of Aumtech. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of Aumtech.
Update: 05/12/99 mpb Changed ERECORDTIME msg to give range.
-----------------------------------------------------------------------------*/

char *GV_TELmsg[]= {
/* TELECOM Error Messages */
"Call disconnected.",					/* TEL_ECALLDISCONN */
"INFO: Signal received. Application terminated on port %s.",
							/* TEL_EAPPTERMINATE */
"Attempt to write duplicate %s CDR. Key=<%s>.",		/* TEL_E_DUP_CDR */
"Vendor software call %s failed. rc=%d",		/* TEL_EVENDORFAIL */
"Vendor software is not active for port %d. rc=%d",	/* TEL_EVENDORACTIVE */
"Unable to get current working directory. errno=%d",	/* TEL_ECURRENTDIR */
"Application can't register with vendor software. rc=%d", /* TEL_EAPPREG */
"%s not defined",					/* TEL_EUNDEFINED */
"Cannot set channel parameters. rc=%d.",		/* TEL_ESETCHANPARM */
"Invalid compression %d.",				/* TEL_ECOMPRESSION */
"WARNING: CDR key set twice. <%s> and <%s>",		/* TEL_W_2_CDR_KEYS */
"Null time passed to %s CDR.",				/* TEL_E_NULL_CDR_TIME*/
"",							/* 1012 */
"",							/* 1013 */
"",							/* 1014 */
"",							/* 1015 */
"",							/* 1016 */
"",							/* 1017 */
"",							/* 1018 */
"",							/* 1019 */

/* InitTelecom Error Messages */
"Unable to open speech file %s. errno=%d.",		/* TEL_EOPENSPEECHFIL */
"%s called more than once",				/* TEL_EREPTCALL */
"Dynamic analog applications are not supported.",	/* TEL_EDYNANALOG */
"Unable to open voice device <%s>. errno=%d. [%s]",	/* TEL_EAVDEVOPEN */
"Unable to open Network device <%s>. errno=%d. [%s]",	/* TEL_EANDEVOPEN */
"",							/* 1025 */
"",							/* 1026 */
"",							/* 1027 */
"",							/* 1028 */
"",							/* 1029 */

/* AnswerCall Error Messages */
"Error waiting for next call. rc=%d.",			/* TEL_EWAITNXTCAL */
"No incoming calls for static ISDN applications.",	/* TEL_ESTATICISDN */
"INFO: Timeout for static analog applications.",	/* TEL_EANSTIMEOUT */
"",							/* 1033 */
"",							/* 1034 */
"",							/* 1035 */
"",							/* 1036 */
"",							/* 1037 */
"",							/* 1038 */
"",							/* 1039 */
/* OutputPhrase Error Messages */
"",							/* 1040 */
"Incorrect date format %s, day %d, month %d, year %d", 	/* TEL_EDATEFMT */
"%s non_numeric string = %s", 				/* TEL_ENONNUMERICSTR */
"%s doesn't handle more than %d digits",		/* TEL_EDIGITLENGTH */
"Non A-Z,0-9 character(s) beginning in position %d of <%s> not spoken.",
							/* TEL_BAD_CHAR */
"Unable to speak %s: inf=%s. outf=%s. value=<%s>. %s",  /* TEL_CANT_SPEAK */
"Unable to speak %s: inf=%s. outf=%s. value=<%s>. rc=%d",/* TEL_CANT_SPEAK_RC */
"speak_time input <%s> is too long.",			/* TEL_ESPKTIMDIGRNG */
"Unable to speak time <%s>. inf=%d outf=%d, rc=%d.",	/* TEL_ETVSPKTIM */
"Informat %s (%d) not valid for outformat %s.",		/* TEL_BAD_IO_FORMATS */
"Invalid informat: %d.",				/* TEL_BAD_INFORMAT */
"Invalid outformat: %d.",				/* TEL_BAD_OUTFORMAT */
"Non $,0-9 character(s) beginning in position %d of <%s> not spoken",
							/* TEL_BAD_DOLLAR */
"",							/* 1053 */
"",							/* 1054 */
"",							/* 1055 */
"",							/* 1056 */
"",							/* 1057 */
"",							/* 1058 */
"",							/* 1059 */
/* InputDTMF Error Messages */
"Illegal combo of type=ALPHA, option=AUTOSKIP",		/* TEL_ETYPEOPTION */
"%s: User retry = %d.",					/* TEL_EUSERRETRY */
"Exceeded maximum user retry = %d.",			/* TEL_EMAXUSERRETRY */
"Input of non_numeric data <%s>.",			/* TEL_EINPUTNONNUM */
"INFO: Ignoring extra * in dollar input string.",	/* TEL_EIGNORESTAR */
"Detected termination key in autoskip mode.",		/* TEL_ETERMAUTOSKIP */
"Invalid alpha character code <%s>",			/* TEL_EALPHAINPUT */
"%s length problem",					/* TEL_EINPUTLENGTH */
"Undefined alpha character",				/* TEL_EINPUTCHAR */
"Invalid input digit",					/* TEL_EINPUTDIGIT */
"",							/* 1070 */
"",							/* 1071 */
"",							/* 1072 */
"",							/* 1073 */
"",							/* 1074 */
"",							/* 1075 */
"",							/* 1076 */
"",							/* 1077 */
"",							/* 1078 */
"",							/* 1079 */

/* Bridge and Transfer temporary. */
"Outbound call to <%s> failed on port %d. rc=%d. [%s]",	/* TEL_EBADCALLOUT */
"Unable to get outbound calling resource %s.",		/* TEL_ERESOURCE */
"Cannot form a 4, 7, or 10 digit phone number from <%s>.", /* TEL_ECONVPHN */
"Cannot form a valid dial string from <%s>.",		/* TEL_ECONSTRPHN */
"(Debug) dial string is %s.",				/* TEL_EDIALSTR */
"Got ring no answer dialing <%s>.",			/*TEL_E_RING_NO_ANSWER*/
"No answer by human or fax. Dialed <%s>. rc=%d.[%s]", /* TEL_E_NO_HUMAN_OR_FAX*/
"",							/* 1087 */
"",							/* 1088 */
"",							/* 1089 */

/* LoadVocab API */
"Error loading language phrase %d for %s. rc=%d.",	/* TEL_ESPKPHRASE */
"",							/* 1091 */
"Error loading vocabulary phrase %d. rc=%d.",		/* TEL_ESPKVOCAB */
"Invalid compression %d for vocab phrase %d. Must be %d.",/* TEL_BAD_VOC_COMP */
"",							/* 1094 */
"",							/* 1095 */
"",							/* 1096 */
"",							/* 1097 */
"",							/* 1098 */
"",							/* 1099 */

/* Input Voice messages (Speech Recognition) */
"No vocabulary has been loaded.",			/* TEL_EVOCNOLOAD */
"Time out on input.",					/* TEL_ETIMEOUTIN */
"Unexpected return value %d from voice recognition.",	/* TEL_EUNEXPVAL */
"Unknown return value %d from voice recognition.",	/* TEL_EUNKNOWN */
"Unable to start speech recognition: rc=%d.",		/* TEL_ETVSIRGO */
"No language has been loaded.",				/* TEL_ELANGNOLOAD */
"",							/* 1106 */
"",							/* 1107 */
"",							/* 1108 */
"",							/* 1109 */

/* Phrase API messages. */
"Cannot copy file %s to phrase %d. rc=%d.",		/* TEL_EFILETOPHRASE */
"Cannot copy phrase %d to file %s",			/* TEL_EPHRASETOFILE */
"INFO: Phrase numbers same for source and destination",	/* TEL_ESAMEPHRASE */
"Cannot create a phrase. rc=%d",			/* TEL_ECREATEPHRASE */
"Cannot copy phrase %d to %d. rc=%d",			/* TEL_ECOPYPHRASE */
"Destination file %s exists",				/* TEL_EPHRASEEXISTS */
"Cannot get phrase type for phrase %d. rc=%d.",		/* TEL_EPHRASETYPE */
"Cannot set phrase type for phrase %d. rc=%d",		/* TEL_ESETPHRTYPE */
"Invalid phrase type (%d) for phrase %d.",		/* TEL_EBADPHRTYPE */
"Cannot delete phrase %d. rc=%d",			/* TEL_EDELETEPHRASE */
"Cannot record phrase %d. rc=%d.",			/* TEL_ERECORDPHRASE */
"Record time is %d, should be between %d & %d. Using 60 sec",/*TEL_ERECORDTIME*/
"Recording compression %d undefined. Using ADPCM.",	/* TEL_ERECORDTYPE */
"Recording compression does not match that of phrase %d",/* TEL_EPHRASECOMP */
"Start position greater than length of phrase",		/* TEL_ESTARTPOS */
"File <%s> is not an unloaded fax or voice phrase, or is corrupted.",
							/* TEL_EFILEFORMAT */

"Source phrase %d does not exist.",			/* TEL_NO_SRC_PHR */
"Error speaking phrase %d rc=%d",			/* TEL_E_SPEAKING */
"",							/* 1128 */
/* Fax Error Messages */
"Unable to get fax page # %d for home phrase %d. rc=%d, faxrc=%d",
							/* TEL_E_GET_FAX_PAGE*/
"Unable to create fax message %d. rc=%d",		/* TEL_ECREATEFAX */
"Error receiving fax. Home phrase=%d, rc=%d, faxrc=%d",	/* TEL_ERECVFAX */ 
"Unable to get number of fax pages. faxrc=%d",		/* TEL_EFAXPAGES */
"A fax is limited to less than %d pages",		/* TEL_ENUMFAXPAGES */
"Failed to set %s to <%s> for phrase %d. rc=%d",	/* TEL_E_SUBSCRIBER */
"Failed to get fax info for phrase %d. rc=%d",		/* TEL_E_GET_FAX_INFO */
"Failed to set fax info for phrase %d. rc=%d",		/* TEL_E_SET_FAX_INFO */
"Phrase %d has no fax queue.",				/* TEL_E_NO_FAX_QUEUE */
"Failed to convert voice call to fax call and/or %s fax. rc=%d. faxrc=%d",
							/* TEL_E_VOICE_2_FAX */
"Invalid phrase number: %d= %d + %d (offset)",		/* TEL_E_BAD_START_PHR*/
"Failed to create fax queue anchor for phrase %d. rc=%d",
							/* TEL_E_CREATE_FAX_Q */
"Failed to set type and compression for phrase %d. rc=%d",
							/* TEL_E_SET_TYPE_COMP*/
"Failed to convert file <%s> to a phrase. rc=%d",	/* TEL_E_CONVERT_FILE */
"Failed to add phrase %d to queue of home phrase %d. rc=%d, faxrc=%d",
							/* TEL_E_ADD_FAX_PAGE */
"Failed to create temp phrase to store text file to be faxed. rc=%d",
							/* TEL_E_CREATE_TEMP */
"Unable to create a new fax page. rc=%d",	/* TEL_E_CREATE_FAX_PAGE */
"NULL application name passed.",                        /* TEL_E_NULL_APP */
"NULL DNIS passed.",                                    /* TEL_E_NULL_DNIS */
"Cannot access application (%s), errno=%d (%s)",	/* TEL_E_APP_ACCESS */
"Cannot get current working dir. errno=%d (%s)",	/* TEL_E_CUR_DIR */
"Failed to start app (%s), errno=%d (%s)",		/* TEL_E_EXEC */
"INFO: Starting new app <%s>",				/* TEL_START_NEW_APP */
"Cannot chdir to appDir <%s>. errno=%d. [%s]",		/* TEL_E_CHDIR */
"Failed to send app. request to Responsibility",	/* TEL_E_SEND_REQ */
"Failed to recv app. request reply from Responsibility",/* TEL_E_RECV_REQ */
"App. <%s> is not scheduled to fire at this time.",	/* TEL_E_APPNOTSCHED */
"Cannot attach to shmem. key=%ld. errno=%d. [%s]",	/* TEL_E_SHMAT */
"Cannot attach to dyn. mgr. message queue. errno=%d. [%s]",/* TEL_E_MSGGET */
"",
"",
"",							/* 1160 */

/* TEL_ScheduleCall Messages */

"Invalid callTime format (%s), Valid formats (%s)",	/* TEL_ETIMEFORMAT */
"Invalid year (%s) in callTime",			/* TEL_EINVALIDYEAR */
"Invalid month (%s) in callTime",			/* TEL_EINVALIDMONTH */
"Invalid day (%s) in callTime",				/* TEL_EINVALIDDAY */
"Invalid hour (%s) in callTime",			/* TEL_EINVALIDHOUR */
"Invalid minutes (%s) in callTime",			/* TEL_EINVALIDMINUTE */
"Invalid seconds (%s) in callTime",			/* TEL_EINVALIDSECOND */
"Failed to generate call definition filename (%s)",	/* TEL_EMAKECDFNAME */
"Failed to schedule outbound call (%s)",		/* TEL_ESCHEDCALL */
"",							/* 1170 */
"",							/* 1171 */
"",							/* 1172 */
"",							/* 1173 */
"",							/* 1174 */
"",							/* 1175 */
"",							/* 1176 */
"",							/* 1177 */
"",							/* 1178 */
"",							/* 1179 */
"",							/* 1180 */

/* TEL_UnScheduleCall Messages */

"Environment variable ISPBASE is not set.",		/* TEL_EISPBASE */
"Failed to unschedule call. Cannot remove file <%s>. errno=%d. [%s]",/* TEL_EUNLINK*/
"Failed to unschedule call. Call with cdf <%s> has already been placed.",/* TEL_EPLACEDCALL */
"Failed to unschedule call. Call attempt for <%s> has already failed.",	/* TEL_EOCSERROR */
"",							/* 1185 */
"",							/* 1186 */
"",							/* 1187 */
"",							/* 1188 */
"",							/* 1189 */
"",							/* 1190 */
"",							/* 1191 */
"",							/* 1192 */
"",							/* 1193 */
"",							/* 1194 */
"",							/* 1195 */
"",							/* 1196 */
"",							/* 1197 */
"",							/* 1198 */
"",							/* 1199 */
"",							/* 1200 */

/* TEL_GetCDFInfo Messages */

"Unable to get CDF info (%s)",				/* TEL_ECDFINFO */
"CDF (%s) not found",					/* TEL_ECDFNOTFOUND */
"",							/* 1203 */
"",							/* 1204 */
"",							/* 1205 */
"",							/* 1206 */
"",							/* 1207 */
"",							/* 1208 */
"",							/* 1209 */
"",							/* 1210 */
"",							/* 1211 */
"",							/* 1212 */
"",							/* 1213 */
"",							/* 1214 */
"",							/* 1215 */
"",							/* 1216 */
"",							/* 1217 */
"",							/* 1218 */
"",							/* 1219 */
"",							/* 1220 */

/* TEL_SpeakTTS Messages */

"Failed to remove tmp speech file (%s), errno=%d (%s)",/* TEL_EAFILERM */
"Failed to create result FIFO (%s), errno=%d (%s)",	/* TEL_EACREATEFIFO */
"Failed to open result FIFO (%s) for reading, errno=%d (%s)",
							/* TEL_EOPENFIFOREAD */
"Failed to open request FIFO (%s) for writing, errno=%d (%s)",
							/* TEL_EOPENFIFOWRITE */
"Failed to write TTS request to FIFO (%s), errno=%d (%s)",
							/* TEL_EFIFOWRITE */
"Failed to read TTS result from FIFO, errno=%d (%s)", /* TEL_EFIFOREAD */
"Cannot access TTS result file (%s), errno=%d (%s)",	/* TEL_EARESULTFILE */

"User string len (%d) should <= %d",			/* TEL_EAUSERSTRLEN */
"Failed to rename tmp speech file (%s) to (%s), errno=%d (%s)",
							/* TEL_EAFILERENAME */
"",							/* 1230 */
"",							/* 1231 */
"",							/* 1232 */
"",							/* 1233 */
"",							/* 1234 */
"",							/* 1235 */
"",							/* 1236 */
"",							/* 1237 */
"",							/* 1238 */
"",							/* 1239 */
"",							/* 1240 */



""
};

int GV_TEL_msgnum = { sizeof(GV_TELmsg) / sizeof(GV_TELmsg[0]) };
