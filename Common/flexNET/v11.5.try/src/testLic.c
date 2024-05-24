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

void chkOut_chkIn();
void licShutdown();
void getNumLicenses();

int flexlmExit();
int handleLicenseFailure();

LM_CODE(code, ENCRYPTION_SEED1, ENCRYPTION_SEED2, VENDOR_KEY1,
    VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);

int main(int argc, char *argv[])
{
	int		rc;
	char	yChar;
	int		ySwitch = 0;

(void) signal(SIGTERM, licShutdown);

    while((yChar = getopt(argc, argv, "n:s:c")) != EOF)
    {
        switch (yChar)
        {
            case 'c':
                ySwitch = 1;
                break;
            case 'n':
				numLics = atoi(optarg);
				printf("set numLics = %d\n", numLics);
                break;
            case 's':
				ySleepTime = atoi(optarg);
                break;
		}
	}

	if ( ySwitch == 1 )
	{
		chkOut_chkIn();
		exit(0);
	}
	getNumLicenses();
}

int licReconnAttempt()
{
	licenseOK++;

	fprintf(stderr, "Incremented handleLicenseFailure(); "
			"Set licenseOK to %d...\n", licenseOK);
	return(0);

} // handleLicenseFailure()

int licReconnSuccess()
{
	licenseOK = 0;

	fprintf(stderr, "Reconnection to license server re-established.");

	return(0);
} // handleLicenseFailure()

void licShutdown()
{
	int	i;
	
	fprintf(stderr, "Got SIGTERM; shutting down...\n");
	for (i=0; i<5; i++)
	{
		sleep(1);
	}

	fprintf(stderr, "Exiting now\n");
	
	exit(0);
} // END: licShutdown()

/*------------------------------------------------------------------------------
getNumLicenses():
------------------------------------------------------------------------------*/
void getNumLicenses()
{
	int		rc;

	char	log_buf[1024];
	char	feature[64];
	char	version[64];
	int		tot_lic_in_use, num_lics;

	memset((char *)log_buf, '\0', sizeof(log_buf));

	sprintf(feature, "%s", "IPTEL");
	sprintf(version, "%s", "2.0");

	rc =  flexLMGetNumLicenses(feature, version, &tot_lic_in_use,
				&num_lics, log_buf);
	printf("%d = flexLMGetNumLicenses(%s, %s, %d, %d, %s)\n",
			rc, feature, version, tot_lic_in_use, num_lics, log_buf);

} // END: getNumLicenses()

/*------------------------------------------------------------------------------
chkOut_chkIn():
------------------------------------------------------------------------------*/
void chkOut_chkIn()
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
		return;
	}

	sleep(ySleepTime);


	printf("Sleeping %d, licenseOK=%d\n", ySleepTime, licenseOK);
	sleep(ySleepTime);
	rc =  flexLMCheckIn(&lm_handle, feature, log_buf);
	printf("%d = flexLMCheckIn(%s)\n", rc, log_buf);

} /* END: chkOut_chkIn() */

int handleLicenseFailure()
{
	printf("Inside handleLicenseFailure()\n");
	return(0);
} // END: flexLMGetNumLicenses()
