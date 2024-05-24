#ident	"@(#)UTL_AddToFile 95/12/05 2.2.0 Copyright 1996 Aumtech"
static char ModuleName[]="UTL_AddToFile";
/*-----------------------------------------------------------------------------
 * UTL_AddToFile.c -- Add to file.
 *
 * Copyright (c) 1996, Aumtech, Inc.
 * All Rights Reserved.
 *
 * This is an unpublished work of Aumtech ISP which is protected under 
 * the copyright laws of the United States and a trade secret
 * of Aumtech. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of Aumtech.
 *
 * ISPID            : 
 * Author	    : M. Bhide
 * Created On	    : 2/23/94
 * Last Modified By : G. Wenzel
 * Last Modified On : 12/05/95
 * 
 * HISTORY
 *
 * 12/5/95	G. Wenzel	Updated for ISP 2.2
 * 09/10/99 	gpw		remove DIAGNOSE messages
 *
 *-----------------------------------------------------------------------------
 */

#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "COM_Mnemonics.h"
#include "monitor.h"

/*----------------------------------------------------------------------------
This routine appends a line to a file 
----------------------------------------------------------------------------*/
int UTL_AddToFile(filename, data)
const	char	*filename;
const	char	*data;
{
int	ret_code;
FILE 	*fp;
char	filepath[200];
char	diag_mesg[256];

sprintf(diag_mesg,"UTL_AddToFile(%s, %s)", filename, "data_string");
ret_code = send_to_monitor(MON_API, UTL_ADDTOFILE, diag_mesg);
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
if((ret_code = UTL_full_path(filename, filepath)) != 0)
{
    /* Error message printed in UTL_full_path function. */
    return (ret_code);
}

/* Check for existence of file. */
if((ret_code = access(filepath, F_OK)) != 0)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOFILE,filepath);
    return (-5);	/* UTL_NO_FILE_EXISTS */
}

if((fp = fopen(filepath, "a")) == NULL)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_UTLFILE_OPEN,filepath);
    return (-1);
}
fprintf(fp, "%s", data);
fclose(fp);

return (0);
}
