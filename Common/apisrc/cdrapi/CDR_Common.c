#ident	"@(#)CDR_Common 99/04/05 2.8.0 Copyright 1999 Aumtech, Inc."

// not used static char ModuleName[]="CDR_Common";

/*-----------------------------------------------------------------------------
Program:	CDR_Common.c
Purpose:	This file contains common routines used by other CDR APIs.
Author: 	Unknown
Date:		09/23/94
Update:		04/05/99 gpw added 1900 to year & made CDR_Time[20], not 15
Update:     03/27/2006  ddn MR1002 Check for '\0' in CDR_strip_str
-------------------------------------------------------------------------------
Copyright (c) 1999, Aumtech,Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include <time.h>
#include <sys/time.h>
#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "CDRmsg.h"

static char	CDR_Time[20];
/*--------------------------------------------------------------------------
This routine constructs the time of day in 100ths of seconds in format:
MMDDYYHHMMSSXX where XX is 100ths of seconds.
--------------------------------------------------------------------------*/
char* CDR_get_time()
{
int	ret_code;
struct	timeval  tp;
struct	timezone tzp;
struct	tm	 *tm;
struct	tm	 result_tm;

/* gettimeofday() is a UNIX system call. */
if((ret_code = gettimeofday(&tp, &tzp)) == -1)
{
    return ((char *)-1);
}

/* localtime() is a UNIX system call. */
tm = localtime_r(&tp.tv_sec, &result_tm);
sprintf( CDR_Time, "%02d%02d%04d%02d%02d%02d%02d", 
    (int)tm->tm_mon + 1, 
    (int)tm->tm_mday, 
    (int)tm->tm_year+1900, 
    (int)tm->tm_hour, 
    (int)tm->tm_min, 
    (int)tm->tm_sec, 
    (int)tp.tv_usec / 10000
    );

return(CDR_Time);
}
/*---------------------------------------------------------------------------
This routine will substitute all control characters (with a space)
in the input string and put the result in the output string.
---------------------------------------------------------------------------*/
int CDR_strip_str(input, output, bytes)
char	*input;
char	*output;
int	bytes;
{
int	i;
int	j;

/* Substitute control characters. */
for(i = 0, j = 0; i < bytes; i++)
{
  if(input[i] == '\0')/*MR 1002 coredump*/
  {
    break;
  }

  if(input[i] <= 0x1f && input[i] > 0x0)
  {
    /* Substitute space. */
    output[j++] = ' ';
  }
  else
  {
    /* Copy character. */
    output[j++] = input[i];
  }
}
output[j] = '\0';
return(0);
}
