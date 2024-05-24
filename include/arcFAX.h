#ident    "@(#)arcFAX.h 2000/05/22 Release 3.1 Copyright 2000 Aumtech, Inc." 
/*----------------------------------------------------------------------------
Program:	arcFAX.h
Purpose:	To define constants used by the Fax APIs
		WARNING: Changes to this file may adversely affect operations.
Author:		Aumtech
Date:		05/22/2000
-----------------------------------------------------------------------------
#	Copyright (c) 1999, 2000 Aumtech, Inc.
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
#ifndef _ARCFAX_H	/* for preventing multiple defines. */
#define _ARCFAX_H

/* File types for sending fax. */
#define 	MEM		94
#define 	TEXT		96
#define 	TIF		97
#define 	LIST		98


#define     CREATE_NEW_LIST_FILE    1
#define     APPEND_TO_LIST_FILE     0


/*-----------------------------------------------------------
Following are the Fax api's prototype
-------------------------------------------------------------*/ 
extern int TEL_RecvFax(char *fax_file, int *fax_pages, char *prompt_phrase);
extern int TEL_SendFax(int file_type, char *fax_data, char *prompt_phrase);
extern int TEL_BuildFaxList(int, char *, char *, int, int, int, char *,
					int, char *);
extern int TEL_ScheduleFax (int, char *, int , char *, char *, int,
                         int, int , int , char *);
extern int TEL_SetFaxTextDefaults(char *);

#endif /* _ARCFAX_H */
