#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "TEL_Common.h"
#include <ocmHeaders.h>

/*------------------------------------------------------------------------------
TEL_FindMe():
------------------------------------------------------------------------------*/
int TEL_FindMe(int option, FindMeStruct *findMeInfo, int *retCode)
{
	static char mod[]="TEL_FindMe";
	char			apiAndParms[128] = "";
	char			apiAndParmsFormat[]="%s(%s)";
	int				rc;

	telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"Error:TEL_FindMe is not supported for SIP.");
	return(-1);
} // END: TEL_FindMe()

