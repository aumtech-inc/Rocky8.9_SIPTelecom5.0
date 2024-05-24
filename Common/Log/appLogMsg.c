/*-----------------------------------------------------------------------------
Program:	appLogMsg.c
Purpose:	Log a message to the ISP.cur.
Build:		cd $ISPBASE/Telecom/Applications
      		makegen appLogMsg
  ---------------------------------------------------------------------------*/
#include <stdio.h>
#include "ISPCommon.h"

int main(int argc, char *argv[])
{
	char	mod[]		= "main";
	int		reportMode;
	int		msgId		= 736;
	char	buf[256];

/*
#define REPORT_NORMAL            1
#define REPORT_VERBOSE           2
#define REPORT_DETAIL            128
#define REPORT_DIAGNOSE          64
#define REPORT_CDR                  16
#define REPORT_CEV                  32
*/

	reportMode = REPORT_CEV; 
	//reportMode = REPORT_EVENT; 

	sprintf(buf, "%s", "This is a test.");
    LogARCMsg(mod, reportMode, "3", "SRC", argv[0], msgId, buf);

	printf("Message (%s) logged.\n", buf);

} // END: main()
