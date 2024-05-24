#ident	"@(#)CDR_ServiceTran 95/12/05 2.2.0 Copyright 1996 AT&T"
static char ModuleName[] = "CDR_ServiceTran";
/*-----------------------------------------------------------------------------
 * CDR_ServiceTran.c -- Signify the start and end of a service transaction.
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
 * Author	    : R. Schmidt
 * Created On	    : 6/08/94
 * Last Modified By : G. Wenzel
 * Last Modified On : 12/05/95
 * 
 * HISTORY
 *
 * 12/05/95	G. Wenzel	Updated for ISP 2.2
 * 08/12/02	MPB	Added CDR_CUSTOM.
 *
 *-----------------------------------------------------------------------------
 */
#include <sys/time.h>
#include "COM_Mnemonics.h"
#include "ISPCommon.h"
#include "ispinc.h" 
#include "loginc.h" 
#include "CDRmsg.h"
#include "COMmsg.h"
#include "monitor.h"

char	*CDR_get_time();

/*-------------------------------------------------------------------------
CDR_ServiceTran(): This routine signifies the start or end of 
a service transaction.
-------------------------------------------------------------------------*/
int
CDR_ServiceTran(CDR_Type,Serv_Type,Serv_Name,Code,Cust_Data1,Cust_Data2)
int		CDR_Type;	/* CDR_START or CDR_END */
const	char	*Serv_Type;
const	char	*Serv_Name;
int		Code;		/* 1 - Success, 2 - Failure */
const	char	*Cust_Data1;
const	char	*Cust_Data2;
{
char	serv_type[4];
char	serv_name[27];
char	cust_data1[9];
char	cust_data2[512];
char	code[3];
int	ret_code = 0;
char	diag_mesg[1024];

sprintf(diag_mesg,"CDR_ServiceTran(%d, %s, %s, %d, %s, %s)",
    CDR_Type,Serv_Type,Serv_Name,Code,Cust_Data1,Cust_Data2);
ret_code = send_to_monitor(MON_API, CDR_SERVICETRAN, diag_mesg);

/* Has initialization of the service been performed once. */
if(GV_InitCalled != 1)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOTINIT,"Service");
    return (-1);
}

/* Replace all control characters in strings and truncate if necessary. */
CDR_strip_str(Serv_Type,serv_type,3);
CDR_strip_str(Serv_Name,serv_name,26);
CDR_strip_str(Cust_Data1,cust_data1,8);
CDR_strip_str(Cust_Data2,cust_data2, 511);
CDR_strip_str(Serv_Name,GV_CDR_ServTranName,26);

fprintf(stdout, "DDNDEBUG:%s::%d:: Key:%s serv_type:%s serv_name:%s time:%s cust1:%s cust2:%s\n", __FILE__, __LINE__, GV_CDR_Key, serv_type, serv_name, CDR_get_time(),cust_data1,cust_data2);fflush(stdout);

/* Send the proper Call Detail Recording message. */
switch(CDR_Type)
{
    case CDR_START:
	LOGXXPRT(ModuleName,REPORT_CDR,CDR_STARTSERVICE_MSG,GV_CDR_Key,
	    "SS",serv_type,serv_name,CDR_get_time(),cust_data1,cust_data2);
	break;
    case CDR_END:
	if(Code == 1)
	    strcpy(code,"01");
	else if(Code == 2)
	    strcpy(code,"02");
	else
	{
	    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_PARMNUM,"Code",Code);
	    return(-1);
	}
	LOGXXPRT(ModuleName,REPORT_CDR,CDR_ENDSERVICE_MSG,GV_CDR_Key,
	    "ES",serv_type,serv_name,code,CDR_get_time(),cust_data1,cust_data2);
	break;
    case CDR_CUSTOM:
	LOGXXPRT(ModuleName,REPORT_CDR,CDR_CUSTOMSERVICE_MSG,GV_CDR_Key,
	    "CS",serv_type,serv_name,CDR_get_time(),cust_data1,cust_data2);
	break;
    default:
	LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_PARMNUM,"CDR_Type",CDR_Type);
	return(-1);
	break;
}

return (0);
} /* CDR_ServiceTran() */
