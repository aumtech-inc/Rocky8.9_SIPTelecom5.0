#ident	"@(#)UTL_CreateFile 95/12/05 2.2.0 Copyright 1996 AT&T"
static char ModuleName[]="UTL_CreateFile";
/*-----------------------------------------------------------------------------
 * UTL_CreateFile.c -- Create a file.
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
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <unistd.h>
#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "COM_Mnemonics.h"
#include "monitor.h"

/*----------------------------------------------------------------------------
This routine creates the fax file and return fax file name 
----------------------------------------------------------------------------*/
int UTL_CreateFile(filename)
const	char 	*filename;
{
FILE 	*fp;
char	filepath[200];
int	ret_code = 0;
char	diag_mesg[128];

sprintf(diag_mesg,"UTL_CreateFile(%s)", filename);
ret_code = send_to_monitor(MON_API, UTL_CREATEFILE, diag_mesg);
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

/* Get full path of file. */
if((ret_code = UTL_full_path(filename,filepath)) != 0)
{
    /* Error message printed in UTL_full_path function. */
    return(ret_code);
}

/* Check for existence of file. */
if((ret_code = access(filepath, F_OK)) == 0)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_FILEEXISTS,filepath);
    return (-4);	/* UTL_FILE_EXISTS */
}

if((fp = fopen(filepath, "a")) == NULL)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_UTLFILE_OPEN,filepath);
    return (-1);
}
fclose(fp);

return (0);
} 
