#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <license.h>
#include "lm_code.h"
#include "TEL_LogMessages.h"

// static 	int	Licensed_Resources, Turnkey_License;

static char	gMsg[512] = "";

int chkOut_chkIn();
int handleLicenseFailure();

LM_CODE(code, ENCRYPTION_SEED1, ENCRYPTION_SEED2, VENDOR_KEY1,
    VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);

int main(int argc, char *argv[])
{
	int		rc;

	rc = chkOut_chkIn();

	if ( rc == 0 )
	{
		printf("0");
	}
	else
	{
		printf("-1");
	}

}

/*------------------------------------------------------------------------------
chkOut_chkIn():
------------------------------------------------------------------------------*/
int chkOut_chkIn()
{
	int		rc;
	int		numLics = 1;

	char	log_buf[1024];
	LM_HANDLE *lm_handle;
	char	feature[64];
	char	version[64];

	memset((char *)log_buf, '\0', sizeof(log_buf));

	sprintf(feature, "%s", "SPEECH_REC");
	sprintf(version, "%s", "2.0");
	rc =  flexLMCheckOut(feature, version, numLics, &lm_handle, log_buf);
	if (rc != 0)
	{
    	LogARCMsg("chkOut_chkIn", REPORT_NORMAL, "-1", "SRC", "chkSRLicense", 3281, log_buf);
		return(-1);
	}
	sprintf(gMsg, "%s feature is successfully licensed.", feature);
    LogARCMsg("main", REPORT_VERBOSE, "-1", "SRC", "chkSRLicense", 3281, gMsg);
	rc =  flexLMCheckIn(&lm_handle, feature, log_buf);
	return(0);
} /* END: chkOut_chkIn() */

int handleLicenseFailure()
{
	printf("Inside handleLicenseFailure()\n");
	return(0);
} // END: flexLMGetNumLicenses()
