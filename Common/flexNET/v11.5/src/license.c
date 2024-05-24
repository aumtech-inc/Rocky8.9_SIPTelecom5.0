/*----------------------------------------------------------------------------
Program:	license.c
Purpose:	Aumtech licensing routines based on Flexlm licensing product.
Author:		George Wenzel
Date:		03/22/99
Update:		04/06/01 gpw added lic_get_feature_expiration(), extract_date
Update:		04/10/01 gpw added lic_get_feature_count_as_yymmdd 
Update:		04/10/01 gpw added extract_date_yymmdd
Update:		05/03/2005 djb	Upgraded it to flexlm9.5 with floating licenses
----------------------------------------------------------------------------*/
#include "lmpolicy.h"
#include "lmclient.h"
#include "lm_attr.h"
#include "lm_code.h"
#include "license.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/errno.h>
LM_CODE(code, ENCRYPTION_SEED1, ENCRYPTION_SEED2, VENDOR_KEY1,
	VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);

extern int UTL_GetProfileString(char *section, char *key, char *defaultVal,
                char *value, long bufSize, char *file, char *zErrorMsg);

extern int errno;

//extern int licReconnAttempt();
//extern int licReconnSuccess();

#ifdef WITH_HANDLE_LIC
int handleLicenseFailure();
#else
extern int handleLicenseFailure();
#endif

static int clean_string(char *msg);
static int get_license_file_name(char *feature, char *licenseFileList, 
			char *err_msg);
static int getVendorInfo(LM_HANDLE *job, char *feature,
		int *totLicensesInUse, int *totLicenses, char *err_msg);
static int sValidateArgs(char *feature, char *subFeature, char *err_msg);

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int flexLMCheckIn(LM_HANDLE **lm_handle, char *feature, char *err_msg)
{
	int				rc;

	err_msg[0] = '\0';

	lc_checkin(*lm_handle, feature, 0);
	lc_free_job(*lm_handle);
	return(0);
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int flexLMGetNumLicenses(char *feature, char *version, 
			int *tot_lic_in_use,  int *num_lic, char *err_msg)
{
	LM_HANDLE		*lm_job;
	int				rc;
	char			yTmpFeature[64];
	char			yTmpErrorMsg[256];
	char			*pStr;
	char			license_file[128];

	rc = 0;
	err_msg[0] = '\0';

	*tot_lic_in_use = -1;
	*num_lic = -1;

	if ( (rc = sValidateArgs(feature, yTmpFeature, err_msg)) != 0 )
	{
		return(-1);
	}

	if ( (rc = get_license_file_name(yTmpFeature, license_file,
				yTmpErrorMsg)) == -1)
	{
		sprintf(err_msg, "Unable to validate license.  %s", yTmpErrorMsg);
		return(-1);
	}

	rc = setenv("LM_LICENSE_FILE", license_file, 0);

	if ((rc = lc_init((LM_HANDLE *)0, "arclicd", &code, &lm_job)) && 
							rc != LM_DEMOKIT)
	{
		pStr = lc_errstring(lm_job);
		sprintf(err_msg, "lc_init failed.  rc=%d.  %s", rc, pStr);
		return(-1);
	}

	rc = getVendorInfo(lm_job, feature, tot_lic_in_use, num_lic, err_msg);

	return(rc);

} // END: flexLMGetNumLicenses()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int flexLMCheckOut(char *feature, char *version, int numLicenses, 
			LM_HANDLE **lm_handle, char *err_msg)
{
	int				rc, status;
	char			license_file[256];
	char			yTmpFeature[64];
	char			yTmpErrorMsg[256];
	VENDORCODE		code;
	char			*pStr;

	rc = 0;

	*err_msg='\0';

	if ( (rc = sValidateArgs(feature, yTmpFeature, err_msg)) != 0 )
	{
		return(-1);
	}

	if ( (rc = get_license_file_name(yTmpFeature, license_file,
				yTmpErrorMsg)) == -1)
	{
		sprintf(err_msg, "Unable to validate license.  %s", yTmpErrorMsg);
		return(-1);
	}

	rc = setenv("LM_LICENSE_FILE", license_file, 0);

	if ((rc = lc_new_job(0, lc_new_job_arg2, &code, lm_handle)) != 0)
	{
		pStr = lc_errstring(*lm_handle);
		sprintf(err_msg, "Unable to check out licenses.  "
				"lc_new_job() returned %d.  %s", rc, pStr);
		return(-1);
	}

	if ((rc = lc_set_attr(*lm_handle, LM_A_CKOUT_INSTALL_LIC,
					(LM_A_VAL_TYPE)0)) != 0)
	{
		pStr = lc_errstring(*lm_handle);
		sprintf(err_msg, "Warning: Unable to set LM_A_CKOUT_INSTALL_LIC "
				"attribute to disable .flexlmrc.  rc=[%d, %s]",
				rc, pStr);
	}

	if ((rc = lc_set_attr(*lm_handle, LM_A_TCP_TIMEOUT,
					(LM_A_VAL_TYPE)0)) != 0)
	{
		pStr = lc_errstring(*lm_handle);
		sprintf(err_msg, "Warning: Unable to set LM_A_TCP_TIMEOUT "
				"attribute to disable timeout.  rc=[%d, %s]",
				rc, pStr);
		return(-1);
	}

	if ((rc = lc_set_attr(*lm_handle, LM_A_RETRY_COUNT,
				(LM_A_VAL_TYPE)5)) != 0)
	{
		pStr = lc_errstring(*lm_handle);
		sprintf(err_msg, "Warning: Unable to set LM_A_RETRY_COUNT. rc=[%d, %s]",
				rc, pStr);
		return(-1);
	}

#if 0
	if ((rc = lc_set_attr(*lm_handle, LM_A_USER_RECONNECT,
				(LM_A_VAL_TYPE)licReconnAttempt)) != 0)
	{
		pStr = lc_errstring(*lm_handle);
		sprintf(err_msg, "Warning: Unable to set LM_A_USER_RECONNECT "
				"attribute to disable timeout.  rc=[%d, %s]",
				rc, pStr);
		return(-1);
	}
	printf("Set LM_A_USER_RECONNECT\n"); fflush(stdout);

	if ((rc = lc_set_attr(*lm_handle, LM_A_USER_RECONNECT_DONE,
				(LM_A_VAL_TYPE)licReconnSuccess)) != 0)
	{
		pStr = lc_errstring(*lm_handle);
		sprintf(err_msg, "Warning: Unable to set LM_A_USER_RECONNECT_DONE "
				"attribute to disable timeout.  rc=[%d, %s]",
				rc, pStr);
		return(-1);
	}
	printf("Set LM_A_USER_RECONNECT_DONE\n"); fflush(stdout);
#endif

	if ((rc = lc_set_attr(*lm_handle, LM_A_USER_EXITCALL,
				(LM_A_VAL_TYPE)handleLicenseFailure)) != 0)
	{
		pStr = lc_errstring(*lm_handle);
		sprintf(err_msg, "Warning: Unable to set LM_A_USER_EXITCALL "
				"attribute to disable timeout.  rc=[%d, %s]",
				rc, pStr);
		return(-1);
	}

	if ((rc = lc_checkout(*lm_handle, feature, version, 
			numLicenses, LM_CO_NOWAIT, &code,
                                LM_DUP_NONE)) != 0)
    {
		pStr = lc_errstring(*lm_handle);
		sprintf(err_msg, "Unable to check out licenses.  "
				"lc_checkout() returned %d.  %s", rc, pStr);
		lc_free_job(*lm_handle);
		return(-1);
	}

	*err_msg='\0';
	return(0);
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int sValidateArgs(char *feature, char *subFeature, char *err_msg)
{
	if ( (strcmp(feature, "TEL") == 0 ) ||
	     (strcmp(feature, "IPTEL") == 0 ) ||
	     (strcmp(feature, "VIDEO") == 0 ) ||
	     (strcmp(feature, "SPEECH_REC") == 0 ) )
	{
		sprintf(subFeature, "%s", "telLicenseServerList");
	}
	else
	{
		sprintf(err_msg, "Unable to validate license.  "
				" Unknown feature received (%s).", feature);
		return(-1);
	}
#if 0
	if (strcmp(feature, "VMXL") == 0 )
	{
		sprintf(subFeature, "%s", "vxmlLicenseServerList");
	}
	else
	{
		sprintf(subFeature, "%s", "telLicenseServerList");
	}
#endif

	return(0);
} // END: sValidateArgs()
/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int get_license_file_name(char *feature, char *licenseFileList, 
			char *err_msg)
{
	static char		lTmpBuf[64];
	static int		got_file_already=0;
	static char		hold_license_file[256];
	char			*ispbase;
	char			base_env_var[]="ISPBASE";
	int 			rc;

	err_msg[0] = '\0';
	licenseFileList[0] = '\0';
	if ( ! got_file_already )
	{
		if ((ispbase = getenv(base_env_var)) != NULL)
		{
			got_file_already=1;
			sprintf(hold_license_file,"%s/Global/.Global.cfg", 
						ispbase);
		}
		else
		{
			sprintf(err_msg, "Unable to get evironment variable %s.",
				base_env_var); 
			return(-1);
		}
	}

	if ((rc = UTL_GetProfileString("Settings", feature, "", lTmpBuf,
			sizeof(lTmpBuf) - 1, hold_license_file, err_msg)) != 0)
	{
		return(-1);
	}
	sprintf(licenseFileList, "%s", lTmpBuf);


	return(0);

} // END: get_license_file_name()

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int getVendorInfo(LM_HANDLE *job, char *feature,
		int *totLicensesInUse, int *totLicenses, char *err_msg)
{
	CONFIG				*conf, *c;
	LM_VD_GENERIC_INFO	gi;
	LM_VD_FEATURE_INFO	fi;
	int					first = 1;
	char				err[512];
	char				*pStr;
	int					rc;
		
	c = (CONFIG *)0; 

	for (conf = lc_next_conf(job, feature, &c);conf; 
					conf=lc_next_conf(job, feature, &c))
	{
		if (first)
		{
/* 
 * 			get generic daemon info
 */
			gi.feat = conf;
			if (( rc = lc_get_attr(job, LM_A_VD_GENERIC_INFO, 
								(short *)&gi)) != 0 )
			{
				pStr = lc_errstring(job);
				sprintf(err_msg, "lc_get_attr(LM_A_VD_GENERIC_INFO) failed.  "
					"rc=%d.  %s", rc, pStr);
				return(-1);
			}
			else 
			{
#if 0
				printf("Generic Info: \n");
				printf("%-30s%d\n", "conn_timeout", 
					gi.conn_timeout);
				printf("%-30s%d\n", "user_init1", gi.user_init1);
				printf("%-30s%d\n", "user_init2", gi.user_init2);
				printf("%-30s%d\n", "outfilter", gi.outfilter);
				printf("%-30s%d\n", "infilter", gi.infilter);
				printf("%-30s%d\n", "callback", gi.callback);
				printf("%-30s%d\n", "vendor_msg", gi.vendor_msg);
				printf("%-30s%d\n", "vendor_challenge",
							gi.vendor_challenge);
				printf("%-30s%d\n", "lockfile", gi.lockfile);
				printf("%-30s%d\n", "read_wait", gi.read_wait);
				printf("%-30s%d\n", "dump_send_data", 
							gi.dump_send_data);
				printf("%-30s%d\n", "normal_hostid", 
							gi.normal_hostid);
				printf("%-30s%d\n", "conn_timeout", 
							gi.conn_timeout);
				printf("%-30s%d\n", "enforce_startdate", 
							gi.enforce_startdate);
				printf("%-30s%d\n", "tell_startdate", 
							gi.tell_startdate);
				printf("%-30s%d\n", "minimum_user_timeout", 
					   	gi.minimum_user_timeout);
				printf("%-30s%d\n", "min_lmremove", 
							gi.min_lmremove);
				printf("%-30s%d\n", "use_featset", 
							gi.use_featset);
				printf("%-30s%x\n", "dup_sel", gi.dup_sel);
				printf("%-30s%d\n", "use_all_feature_lines", 
					    	gi.use_all_feature_lines);
				printf("%-30s%d\n", "do_checkroot", 
							gi.do_checkroot);
				printf("%-30s%d\n", "show_vendor_def", 
							gi.show_vendor_def);
				printf("%-30s%d\n", "allow_borrow", 
							gi.allow_borrow);
#endif
			}
			first = 0;
		}
		fi.feat = conf;
		if ((rc=lc_get_attr(job, LM_A_VD_FEATURE_INFO, (short *)&fi)) != 0)
		{
			pStr = lc_errstring(job);
			sprintf(err_msg, "lc_get_attr(LM_A_VD_FEATURE_INFO) failed.  "
					"rc=%d.  %s", rc, pStr);
			return(-1);
		}
		else
		{
/* 
 * 			get specific feature info
 */
#if 0
			printf("\n%-30s%s\n", "feature", conf->feature);
			printf("%-30s%s\n", "code", conf->code);
			printf("%-30s%d\n", "rev", fi.rev);
			printf("%-30s%d\n", "timeout", fi.timeout);
			printf("%-30s%d\n", "linger", fi.linger);
			printf("%-30s%x\n", "dup_select", fi.dup_select);
			printf("%-30s%d\n", "res", fi.res);
			printf("%-30s%d\n", "tot_lic_in_use", fi.tot_lic_in_use);
			printf("%-30s%d\n", "float_in_use", fi.float_in_use);
			printf("%-30s%d\n", "user_cnt", fi.user_cnt);
			printf("%-30s%d\n", "num_lic", fi.num_lic);
			printf("%-30s%d\n", "queue_cnt", fi.queue_cnt);
			printf("%-30s%d\n", "overdraft", fi.overdraft);
#endif
			*totLicensesInUse	= fi.tot_lic_in_use;
			*totLicenses = fi.num_lic;
		}
#if 0
		printf("CONFIG info:\n");
		printf("%-30s%d\n", "type", conf->type);
		printf("%-30s%s\n", "version", conf->version);
		printf("%-30s%s\n", "exp date", conf->date);
		printf("%-30s%d\n", "num users",conf->users);
		printf("%-30s%s\n", "license key",conf->code);
		if (conf->lc_vendor_def)
			printf("%-30s%s\n", "vendor_def",conf->lc_vendor_def);
#endif
	}
	if (first)
	{
		if (lc_get_errno(job))
		{
			pStr = lc_errstring(job);
			sprintf(err_msg, "Can't get feature %s (%s).", feature, pStr);
			return(-1);
		}
		else
		{
			sprintf(err_msg, "No such feature %s.", feature);
			return(-1);
		}
	}
	return(0);
} // END: getVendorInfo()


/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int clean_string(char *msg)
{
	register int i=0;

	while(msg[i])
	{
		if (msg[i] == '\n') msg[i]=';';
		if (msg[i] == '\t') msg[i]=' ';
		i++;
	}
	return(0);
}

#ifdef WITH_HANDLE_LIC
int handleLicenseFailure()
{
	return(0);
}
#endif
