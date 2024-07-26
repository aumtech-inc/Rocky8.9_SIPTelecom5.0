/*-----------------------------------------------------------------------------
Program:	WSS_Mnemonics.h
Purpose:	To define a set of id numbers used by send_to_monitor.
Author:		George Wenzel
Date:		08/11/99
-----------------------------------------------------------------------------*/

#ifndef WSS_MNEMONICS_H
#define WSS_MNEMONICS_H

#define WSS_API_BASE 6000

#define WSS_INIT		WSS_API_BASE+1
#define	WSS_EXIT		WSS_API_BASE+2
#define	WSS_RECVDATA		WSS_API_BASE+3
#define	WSS_SENDDATA		WSS_API_BASE+4
#define	WSS_SENDFILE		WSS_API_BASE+5
#define	WSS_RECVFILE		WSS_API_BASE+6
#define	WSS_GETGLOBAL		WSS_API_BASE+7
#define	WSS_SETGLOBAL		WSS_API_BASE+8
#define	WSS_GETGLOBALSTRING	WSS_API_BASE+9

#endif
