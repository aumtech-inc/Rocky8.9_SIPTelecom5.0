#ident	"@(#)UTL_Substring 99/09/10 2.2.0 Copyright 1999 Aumtech"
static char ModuleName[]="UTL_Substring";
/*-----------------------------------------------------------------------------
Program:	UTL_Substring.c
Purpose:	Copies a subset of a string from source to destination.
Author:		Aumtech, Inc.
Date:		Unknown
Update:		09/10/99 gpw removed verbose log messages.
-----------------------------------------------------------------------------*/

#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "COM_Mnemonics.h"
#include "monitor.h"

extern int LOGXXPRT(char *module, int requested_mode, int MessageID, char *bogus, ... );

/*----------------------------------------------------------------------------
This routine will copy the subset of the string data from source string 
to destination string. The string is copied only for a given length 
and from a given position. 
-----------------------------------------------------------------------------*/
int UTL_Substring(destination, source, strtpos, len)
char	*destination;				/* destination string */
const	char	*source;			/* source string */
int 	strtpos;				/* starting position */
int 	len;					/* string length to be copied */
{
int	ret_code = 0;
char	diag_mesg[1024];

*destination = '\0';				/* initialize the destination */

if(GV_HideData)
{
  sprintf(diag_mesg,"UTL_Substring(%s, %s, %d, %d)",
    "destination_string", "source_string", strtpos, len);
}
else
{
  sprintf(diag_mesg,"UTL_Substring(%s, %s, %d, %d)",
    "destination_string", source, strtpos, len);
}


ret_code = send_to_monitor(MON_API, UTL_SUBSTRING, diag_mesg);
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

/* validate starting position */
if(strtpos < 0)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_PARMNUM,"start position",strtpos);
    return (-1);
}

if(strtpos == 0)
    strtpos = 1;	/* default starting position */

/* validate length */
if(len < 0)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_PARMNUM,"length",len);
    return (-1);
}

/* nothing to be copied */
if(len == 0)
    return (0);

/* empty source string */
if(strlen(source) == 0)
    return (0);

if(strtpos > (int)strlen(source))
{
    //LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_STRTSPOS);
    return (-1);
}

/* Adjust for zero based arrays. */
strtpos = strtpos - 1;

/* Truncate the string if neccesary. */
if((strtpos + len) >= (int)strlen(source))
{
    len = strlen(source) - strtpos; 
}

/* move the pointer to starting position */
source = source + strtpos;

/* copy data into destination string */ 
strncpy(destination, source, len);
destination[len] = '\0';

return (0);
}
