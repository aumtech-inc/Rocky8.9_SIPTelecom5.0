/*----------------------------------------------------------------------------
Program:	arcFifos.h
Purpose:	Define common constants and names for the fifos.
Author:		Aumtech
Update:	08/18/2004 djb	Created the file.
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
#ifndef _ARC_FIFOS_H	/* for preventing multiple defines. */
#define _ARC_FIFOS_H

// #define DYNMGR_REQUEST_FIFO		"dynmgr.request.fifo"	// RequestFifo
#define DYNMGR_REQUEST_FIFO		"RequestFifo"	// RequestFifo
// #define APPL_RESPONSE_FIFO		"appl.response.fifo"	// ResponseFifo
#define APPL_RESPONSE_FIFO		"ResponseFifo"	// ResponseFifo

#define DYNMGR_TO_RESP_FIFO		"dynmgr.to.resp.fifo"	// ResFifo

#define RTPSPEAK_REQUEST_FIFO	"rtpSpeak.request.fifo" // ReqSpeakProcessFifo
#define RTPSPEAK_RESPONSE_FIFO	"rtpSpeak.response.fifo"// ResSpeakProcessFifo

#define SNMP_TRAP_FIFO			"snmp.trap.fifo"		// arcSNMP.fifo
#define SNMP_REQUEST_FIFO		"snmp.request.fifo"		// arcSNMPRequest.fifo

//#define MRCP_TO_SR_CLIENT       "to.mrcpClient.fifo"   // ReqAppToSRClient.<pid>
#define MRCP_TO_SR_CLIENT       "ReqAppToSRClient"   // ReqAppToSRClient.<pid>

#define MRCP_TO_SR_CLIENT2      "to.mrcpClient2.fifo"  // ReqAppToSRClient.<pid>
//#define MRCP_TO_SR_CLIENT2      "ReqAppToSRClient"  // ReqAppToSRClient.<pid>

#define AI_REQUEST_FIFO			"ai.request.fifo"  // chatGPT fifo

#define TTS_FIFO      "tts.fifo"

#define STONES_CP_FIFO     "to.sipTonesCP.fifo"
#define STONES_FAX_FIFO    "to.sipTonesFax.fifo"

#define CONF_MGR_FIFO   "ArcConferenceMgr.fifo"

#define NAGIOS_FIFO             "arcNagios.fifo"        // arcNagios fifo

#if 0
#define RESP_TO_VXML2_FIFO		"respToVxml2.fifo"		// RespToVxml2Fifo
#define RESP_FROM_VXML2_FIFO	"vxml2ToResp.fifo"		// ResFromVxml2Fifo
#endif

typedef struct
{
	int     index;
	char    subDir[128];
} ArcFifoDirsStruct;

#define MAX_FIFO_SUBDIRS 		10

#define DYNMGR_FIFO_INDEX		0
#define RESP_FIFO_INDEX			1
#define SNMP_FIFO_INDEX			2
#define APPL_FIFO_INDEX			3
#define MRCP_FIFO_INDEX			4
#define MRCP_TTS_FIFO_INDEX		5
#define STONES_CP_FIFO_INDEX	6
#define STONES_FAX_FIFO_INDEX	7
#define NAGIOS_FIFO_INDEX       8
#define AI_FIFO_INDEX       	9


#ifdef UTL_FIFO_DEF
ArcFifoDirsStruct   arcFifoDirs[MAX_FIFO_SUBDIRS] =
{
	{ DYNMGR_FIFO_INDEX,        "-1" /* dynmgr */   },
	{ RESP_FIFO_INDEX,          "resp"      },
	{ SNMP_FIFO_INDEX,          "snmp"      },
	{ APPL_FIFO_INDEX,     		"-1" /* appl */ },
	{ MRCP_FIFO_INDEX,      	"-1" },
	{ MRCP_TTS_FIFO_INDEX,      "-1" },
	{ STONES_CP_FIFO_INDEX,		"-1" },
	{ STONES_FAX_FIFO_INDEX,	"-1" },
	{ NAGIOS_FIFO_INDEX,        "nagios" },
	{ AI_FIFO_INDEX, 	   	"-1" }

};
#endif

#endif /* _ARC_FIFOS_H */
