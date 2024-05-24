#ident  "@(#)arcFAXInternal.h 2000/05/22 Release 3.1 Copyright 2000 Aumtech, Inc."
/*----------------------------------------------------------------------------
Program:	arcFAXInternal.h
Purpose:	To define constants used by the VFX Fax APIs.
		WARNING: Changes to this file may adversely affect operations.
Author:		Aumtech
Update: 05/05/99 djb	Created the file
Update:	05/27/99 djb	Changed arcFaxInternal.h to ARCFaxInternal.h
Update: 06/11/99 apj    In call to the routines defined in FAX_Common.c, add 
			ModuleName argument.
Update: 05/01/00 apj    Added slot number parameter in getAFaxResource and
			establishFaxRouting function prototypes.
Update:	05/08/00 gpw 	Added #defines for all fax messages.
Update:	05/22/00 gpw 	Changed name from ARCFaxInternal.h
----------------------------------------------------------------------------*/
#ifndef _ARCFAX_INTERNAL_H	/* for preventing multiple defines. */
#define _ARCFAX_INTERNAL_H

/* define the type and subtype to be set for requesting a fax resource */
#define RESOURCE_TYPE           "FAX"
#define RESOURCE_SUB_TYPE       "VFX"

/*------------------------------------------------------------------------------
Fax Common Function Prototypes	: defined in FAX_Common.c
------------------------------------------------------------------------------*/
extern int	getAFaxResource(char *, char *, char *);
extern int	establishFaxRouting(char *, char *, char *, int *);
extern int	closeFaxConnection(char *, int);
extern void	validateFaxPageParams(char *, char *, int *, int *);
extern void	setTextToDefaults();


/* Message numbers for all fax messages */

/* TEL_SendFax Messages */
#define FAX_BASE 1700
#define FAX_TELECOM_NOT_INIT		FAX_BASE+0
#define FAX_INVALID_PARM 		FAX_BASE+1
#define FAX_INVALID_PARM_WARN 		FAX_BASE+2
#define FAX_INVALID_SYSTEM_PARM		FAX_BASE+3
#define FAX_GOT_RESOURCE		FAX_BASE+4
#define FAX_FREE_RESOURCE_REQUEST	FAX_BASE+5
#define FAX_FREE_RESOURCE_FAIL		FAX_BASE+6
#define FAX_FREE_RESOURCE_FAIL_WARN	FAX_BASE+7
#define FAX_FAIL_TO_INIT_CHAN		FAX_BASE+8
#define FAX_INIT_CHAN			FAX_BASE+9
#define FAX_EXTRA_MEM_FILE		FAX_BASE+10
#define FAX_CANT_OPEN_FILE		FAX_BASE+11
#define FAX_SEND_FAIL_SYSTEM		FAX_BASE+12
#define FAX_SEND_FAIL_DISCONNECT	FAX_BASE+13
#define FAX_SEND_FAIL_OTHER		FAX_BASE+14
#define FAX_SEND_FAIL_VERBOSE		FAX_BASE+15
#define FAX_NO_LIST_ITEMS		FAX_BASE+16
#define FAX_CANT_OPEN_FILE_IN_LIST	FAX_BASE+17
#define FAX_SEND_SUCCESS		FAX_BASE+18
#define FAX_CANT_OPEN_LIST_FILE		FAX_BASE+19
#define FAX_ERROR_IN_LIST_FILE		FAX_BASE+20
#define FAX_CONTENT_FILE_NOT_OK		FAX_BASE+21
#define FAX_MAX_LIST_ITEMS		FAX_BASE+22
#define FAX_INVALID_PARM_IN_LIST	FAX_BASE+23


#define FAX_BAD_RECV_FILENAME		FAX_BASE+30
#define FAX_BAD_RECV_DIR		FAX_BASE+32
#define FAX_RECV_FAIL_SYSTEM		FAX_BASE+33
#define FAX_RECV_FAIL_DISCONNECT	FAX_BASE+34
#define FAX_RECV_FAIL_OTHER		FAX_BASE+35
#define FAX_RECV_FAIL_VERBOSE		FAX_BASE+36
#define FAX_CLOSE_CONNECT		FAX_BASE+37
#define FAX_CANT_ACCESS_PROMPT		FAX_BASE+38


#define FAX_NO_FILE			FAX_BASE+39
#define FAX_BAD_PHONE_NO		FAX_BASE+40
#define FAX_CANT_CREATE_DIR		FAX_BASE+41
#define FAX_COPY_FAILED			FAX_BASE+42
#define FAX_WRITE_FAILED		FAX_BASE+43

#define FAX_NO_FILE_FOR_LIST		FAX_BASE+44

#define FAX_GET_RESOURCE_FAILED			FAX_BASE+45
#define FAX_GET_RESOURCE_FAILED_GENERAL		FAX_BASE+46
#define FAX_GET_RESOURCE_FAILED_CANT_SEND	FAX_BASE+47
#define FAX_GET_RESOURCE_FAILED_NO_RESPONSE	FAX_BASE+48
#define FAX_ALL_RESOURCES_IN_USE		FAX_BASE+49
#define FAX_NO_RESOURCES_CONFIGURED		FAX_BASE+50
#define FAX_CANT_OPEN_CHANNEL			FAX_BASE+51
#define FAX_CANT_GET_XMIT_SLOT			FAX_BASE+52
#define FAX_CANT_GET_XMIT_SLOT_NET		FAX_BASE+53
#define FAX_FAILED_TO_SET_SLOT			FAX_BASE+54
#define FAX_CANT_CONNECT_NET_TO_FAX		FAX_BASE+55
#define FAX_CANT_CONNECT_FAX_TO_NET		FAX_BASE+56
#define FAX_REQUEST_TO_DYNMGR_FAILED		FAX_BASE+57
#define FAX_CANT_BREAK_FAX_NET_CONNECTION 	FAX_BASE+58
#define FAX_CANT_REESTABLISH_VOICE_NET_CONNECTION FAX_BASE+59

#define FAX_TELECOM_NOT_INIT_MSG "Telecom Services has not been initialized."

#endif /* _ARCFAX_INTERNAL_H */
