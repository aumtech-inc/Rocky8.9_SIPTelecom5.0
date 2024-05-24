/*----------------------------------------------------------------------------
Program:	FAX_Mnemonics.h
Purpose:	To define a set of id numbers used by send_to_monitor.
Author:		George Wenzel
Date:		06/21/99
----------------------------------------------------------------------------*/
#ifndef FAX_MNEMONICS_H
#define FAX_MNEMONICS_H

#define FAX_API_BASE 9000

#define TEL_SENDFAX		FAX_API_BASE+1
#define TEL_RECVFAX		FAX_API_BASE+2
#define TEL_SCHEDULEFAX		FAX_API_BASE+3
#define TEL_BUILDFAXLIST	FAX_API_BASE+4
#define TEL_SETFAXTEXTDEFAULTS	FAX_API_BASE+5

#endif
