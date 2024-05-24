#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "dynVarLog.h"

extern "C" int LogARCMsg (char *parmModule, int parmMode,
           char *parmResourceName, char *parmPT_Object,
           char *parmApplName, int parmMsgId, char *parmMsg);

extern char gProgramName[];

int dynVarLog (int zLine, int zCall, const char *zpMod, int mode, int msgId, 
	int msgType, const char *msgFormat, ...)
{
	const char *mod = "dynVarLog";
	va_list ap;
	char m[1024] = "";
	char m1[1024] = ""; ;
	char type[32] = "";
	int rc;
	char lPortStr[10] = "";

	switch (msgType)
	{

		case 0:
			strcpy (type, "ERR: ");
			break;

		case 1:
			strcpy (type, "WRN: ");
			break;

		case 2:
			strcpy (type, "INF: ");
			break;

		default:
			strcpy (type, "INF: ");
			break;
	}

	memset (m1, '\0', sizeof (m1));
	va_start (ap, msgFormat);
	vsnprintf (m1, sizeof(m1), msgFormat, ap);
	va_end (ap);

	if( (mode == REPORT_CDR) ||
	    (mode == REPORT_EVENT))
	{
		sprintf(m, "%s", m1);
	}
	else
	{
		sprintf (m, "%s[%d]%s", type, zLine, m1);
	}

	sprintf (lPortStr, "%d", zCall);

	// LogARCMsg (zpMod, mode, lPortStr, "DYN", "ArcIPDynMgr", msgId, m);
	LogARCMsg ((char *)zpMod, mode, lPortStr, "DYN", gProgramName, msgId, m);

	return (0);

}/*END: int dynVarLog*/ 






