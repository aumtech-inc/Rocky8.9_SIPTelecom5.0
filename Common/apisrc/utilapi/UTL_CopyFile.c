#ident	"@(#)UTL_CopyFile 95/12/05 2.2.0 Copyright 1996 AT&T"
static char ModuleName[]="UTL_CopyFile";
/*-----------------------------------------------------------------------------
 * UTL_CopyFile.c -- Copy a file.
 *
 * Copyright (c) 1996, AT&T Network Systems Information Services Platform (ISP)
 * All Rights Reserved.
 *
 * This is an unpublished work of AT&T ISP which is protected under 
 * the copyright laws of the United States and a trade secret
 * of AT&T. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of AT&T.
 *
 * ISPID            : 
 * Author	    : M. Bhide
 * Created On	    : 2/23/94
 * Last Modified By : G. Wenzel
 * Last Modified On : 12/05/95
 * 
 * HISTORY
 *
 * 12/05/95	G. Wenzel	Updated to ISP 2.2
 *
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <unistd.h>
#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "COM_Mnemonics.h"
#include "monitor.h"

#define	MAX_SIZE	256

extern int LOGXXPRT(char *module, int requested_mode, int MessageID, char *bogus, ... );
extern int UTL_filecopy(FILE *in_ptr, FILE *out_ptr);
extern int UTL_full_path(char *file, char *fullpath);

/* Utility Heartbeat structures. */

/*----------------------------------------------------------------------------
This routine creates the fax file and return fax file name 
----------------------------------------------------------------------------*/
int UTL_CopyFile(src_file,dest_file)
const	char 	*src_file;
const	char 	*dest_file;
{
FILE	*src_ptr;
FILE	*dest_ptr;
char	src_path[200];
char	dest_path[200];
int	ret_code = 0;
char	diag_mesg[1024];

sprintf(diag_mesg,"UTL_CopyFile(%s, %s)", src_file, dest_file);
ret_code = send_to_monitor(MON_API, UTL_COPYFILE, diag_mesg);
if ( ret_code < 0 )
{
        sprintf(diag_mesg, "send_to_monitor failed. rc = %d", ret_code);
        LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ISPDEBUG, diag_mesg);
}

/* Check to see if Init'Service' API has been called. */
if(GV_InitCalled != 1)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOTINIT,"Service");
    return (-1);
}

/* Get full path of source file. */
if((ret_code = UTL_full_path(src_file, src_path)) != 0)
{
    /* Error message printed in UTL_full_path function. */
    return (ret_code);
}

/* Open source file for read only. */
if((src_ptr = fopen(src_path,"r")) == NULL)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOFILE,src_path);
    return (-5);	/* UTL_NO_FILE_EXISTS */
}

/* Get full path of destination file. */
if((ret_code = UTL_full_path(dest_file, dest_path)) != 0)
{
    /* Error message printed in UTL_full_path function. */
    return (ret_code);
}

/* Open destination file for write only. Truncate if created. */
if((dest_ptr = fopen(dest_path,"w+")) == NULL)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_UTLFILE_OPEN,dest_path);
    return (-1);
}

/* Copy source file to destination file. */
if((ret_code = UTL_filecopy(src_ptr, dest_ptr)) < 0)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_COPY,src_file,dest_file);
    return (-3);	/* UTL_FILE_COPY_FAIL */
}

fclose(src_ptr);
fclose(dest_ptr);

return (0);
} 
