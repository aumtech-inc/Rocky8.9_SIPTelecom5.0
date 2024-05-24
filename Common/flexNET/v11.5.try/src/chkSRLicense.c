#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <license.h>
#include "lm_code.h"

static 	int	Licensed_Resources, Turnkey_License;
static	int	ySleepTime = 10;
static 	int	numLics = 50;
static	int	licenseOK = 0;

int chkOut_chkIn();

LM_CODE(code, ENCRYPTION_SEED1, ENCRYPTION_SEED2, VENDOR_KEY1,
    VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);

int main(int argc, char *argv[])
{
	int		rc;

	rc = chkOut_chkIn();
}

/*------------------------------------------------------------------------------
chkOut_chkIn():
------------------------------------------------------------------------------*/
int chkOut_chkIn()
{
	int		rc;

	char	log_buf[1024];
	LM_HANDLE *lm_handle;
	char	feature[64];
	char	version[64];

	memset((char *)log_buf, '\0', sizeof(log_buf));

	sprintf(feature, "%s", "SPEECH_REC");
	sprintf(version, "%s", "2.0");

	rc=0;
	printf("Calling  %d = flexLMCheckOut(%s, %s, %d, , %s)\n",
			rc, feature, version, numLics, log_buf);
	rc =  flexLMCheckOut(feature, version, numLics, &lm_handle, log_buf);
	printf("%d = flexLMCheckOut(%s, %s, %d, , %s)\n",
			rc, feature, version, numLics, log_buf);
	if (rc != 0)
	{
		return(rc);
	}

	rc =  flexLMCheckIn(&lm_handle, feature, log_buf);
	printf("%d = flexLMCheckIn(%s)\n", rc, log_buf);
	return(0);

} /* END: chkOut_chkIn() */
