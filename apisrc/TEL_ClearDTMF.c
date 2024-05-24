/*-----------------------------------------------------------------------------
Program:        TEL_ClearDTMF.c
Purpose:        Clear the DTMF buffer.
		Note: This API has no interaction with the dynamic manager.
Author:         Wenzel-Joglekar
Date:		08/24/2000
Update:		09/14/2000 gpw did all the work on this. Anand did nothing.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

int TEL_ClearDTMF() 
{
	static char mod[]="TEL_ClearDTMF";
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s()";
	int rc;

	sprintf(apiAndParms,apiAndParmsFormat, mod);
        rc = apiInit(mod, TEL_CLEARDTMF, apiAndParms, 0, 0, 0);
        if (rc != 0) HANDLE_RETURN(rc);
	
	memset(GV_TypeAheadBuffer1, '\0', sizeof(GV_TypeAheadBuffer1));
	HANDLE_RETURN(0);
}
