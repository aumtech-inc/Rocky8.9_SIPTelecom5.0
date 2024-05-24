/*----------------------------------------------------------------------------
Program:	arcLicense.c
Purpose:	This program is to be run on the machine for which a license
		is being requested. It gets the flexlm hostid as well as the 
		mac address of the machine.
Author:		George Wenzel
Date:		10/26/2000
----------------------------------------------------------------------------*/
#include "lmpolicy.h"
#include "license.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/errno.h>

#define LICENSE_INFO_FILE "/tmp/license.txt"
#define CANCEL_LICENSING		-99

extern int isItCentos7(char *msg);

static int display_and_store_info(char *uname, char *service,
			int num_resources, char *hostid, char *mac_addr, char *licenseModel,
			char *product, char *version);
static int get_info_sip(char *service, int *num_resources, char *licenseModel,
				char *product);
static int get_info_sdm(char *service, int *num_resources, char *licenseModel,
				char *product);
static int sGetProcessor(char *zProcessor, char *zErrorMsg);
static int sGetBusBits(int *zBusBits, char *zErrorMsg);
static int sGetPkgInfo(char *zProduct, char *zPkgInfoFile,
                    char *zVersion, char *zMsg);
static int sGetOS(char *zOS, char *zErrorMsg);

extern int errno;

#undef DEBUG
#ifdef DEBUG
	static int log_msg(char *s);
	static char Msg[512];
#endif

struct productInfo
{
	char		service[32];
	char		version[8];
	char		serviceStr[128];
	int			numPorts;
};

static struct productInfo	gProd_Info[6];
static char					os[8] = "5.1";
static int					busbits;
static char					processor[64];

main(int argc, char **argv)
{
#define MAX_COUNT  7
#define SLEEP_TIME 3
	int rc;
	int count;
	char mac_addr[64];
	char uname[64];
	char hostid[64];
	char licenseModel[64];
	char msg[512];
	char service[32];
	int  num_resources=0;
    char version[16];
    char product[128];
	int	index;

    memset((char *)hostid, '\0', sizeof(hostid));
    memset((char *)service, '\0', sizeof(service));
    memset((char *)version, '\0', sizeof(version));
    memset((char *)uname, '\0', sizeof(uname));
    memset((char *)product, '\0', sizeof(product));
    memset((char *)processor, '\0', sizeof(processor));
    memset((char *)msg, '\0', sizeof(msg));

	memset((struct productInfo *)gProd_Info, '\0', sizeof(gProd_Info));

	num_resources=0;

	gethostname(uname, sizeof(uname)-1);

	if ((rc = sGetOS(os, msg)) < 0 )
	{
		printf("%s\n", msg);
		exit(0);
	}
	if ((rc = sGetProcessor(processor, msg)) < 0 )
	{
		printf("%s\n", msg);
		exit(0);
	}
	if ((rc = sGetBusBits(&busbits, msg)) < 0 )
	{
		printf("%s\n", msg);
		exit(0);
	}

#ifdef LIC_SIP
	if ((rc = get_info_sip(service, &num_resources, licenseModel, product)) != 0)
#else
	if ((rc = get_info_sdm(service, &num_resources, licenseModel, product)) != 0)
#endif
	{
		exit(0);
	}

	count=0;
	while ( count <= MAX_COUNT )
	{	
		rc = get_mac_address("current host", mac_addr, msg);
		if (rc == 0) break;
		sleep(SLEEP_TIME);
		if (rc != 0)
		{
			if ( count < MAX_COUNT)
			{
				fprintf(stdout,"."); 
				fflush(stdout);
			}
			else
			{
			fprintf(stdout,","); 
			fflush(stdout);
				
			rc = get_mac_address("current host", 
							mac_addr, msg);
			if (rc != 0)
			{	
			fprintf(stderr,"Unable to get mac address: %s\n", msg); 
			fprintf(stderr,"Please try again.\n"); 
			fflush(stderr);		
			}
			}
		}
		count++;
	}

	if (rc == 0)
	{
		display_and_store_info(uname, service, num_resources, 
							hostid, mac_addr, licenseModel, product, version);
	}

	return(0);
}

static int display_and_store_info(char *uname, char *service,
			int num_resources, char *hostid, char *mac_addr, char *licenseModel,
			char *product, char *version)
{
	FILE *fp;
	char license_info_file[]=LICENSE_INFO_FILE;
	time_t current_time;
	struct tm *today;
	char time_str[128];
	char resource_prompt[256];
	int			index;

	time(&current_time);
	today = localtime( &current_time );
	strftime(time_str, 50, "%d %B %y %H:%M:%S", today );

	fprintf(stdout, "\n");
	fprintf(stdout, "Date of request: %s\n", time_str); 
	fprintf(stdout, "Host name: %s\n", uname); 
	fprintf(stdout, "Machine id: %s\n", mac_addr); 

	for (index=0; gProd_Info[index].service[0] != '\0'; index++)
	{
		fprintf(stdout, "Service requested: %s\n", 
						gProd_Info[index].serviceStr);
	}
	fprintf(stdout, "Resources requested: %d\n", num_resources); 
	fprintf(stdout, "License model: %s\n", licenseModel); 
	fprintf(stdout, "\n");
	fflush(stdout);

	if ((fp=fopen(license_info_file, "w")) == NULL) return(0);
	fprintf(stdout, "\n");
	fprintf(fp, "Date of request: %s\n", time_str); 
	fprintf(fp, "Host name: %s\n", uname); 
	fprintf(fp, "Machine id: %s\n", mac_addr); 
	for (index=0; gProd_Info[index].service[0] != '\0'; index++)
	{
		fprintf(fp, "Service requested: %s\n", 
						gProd_Info[index].serviceStr);
	}
	fprintf(fp, "Resources requested: %d\n", num_resources); 
	fprintf(fp, "License Model: %s\n", licenseModel); 
	fclose(fp);
	fprintf(stdout, "(This information is saved in %s)\n", LICENSE_INFO_FILE);
	fprintf(stdout, "\n");
	fprintf(stdout, "Please forward this information to Aumtech, Inc.\n");
	fprintf(stdout, "\n");
	return(0);
}

#ifdef DEBUG
int log_msg(s)
char *s;
{
#include <time.h>

	char msg[512];
	time_t current_time;
	struct tm *today;
	char time_str[128];

	time(&current_time);
	today = localtime( &current_time );
	strftime(time_str, 50, "%d %H:%M:%S", today );
	sprintf(msg, "%s: %s\n", time_str, s);

	fprintf(stdout, msg); fflush(stdout);

	return(0);
}
#endif

static int log_msg(s)
char *s;
{
	fprintf(stdout, "%s\n", s);
	fflush(stdout);
	return(0);
}

static int convert_to_upper(in_string, out_string)
char *in_string;
char *out_string;
{
	int i=0,j=0;
	char tmp_string[512];

	while ( in_string[i] )
	{
	  	if ( in_string[i] != ':' ) 
			tmp_string[j++] = toupper(in_string[i]);
		i++;
	}
	strcpy(out_string, tmp_string);
	return(0);
} 

static int get_info_sdm(char *service, int *num_resources, char *licenseModel,
				char *product)
{
	int	rc;
	char s_num_resources[32] = "";
	char s_lic_model[32] = "";
	char s_ans[10];
	char s_bogus[10];
	int  ans;
	int	keepLooping = 1;
	char		pkgFile[128]="";
	char		errMsg[256];
	int			index;
	char		version[32]="";
	int			firstTime = 1;

	*num_resources = 0;
	index = 0;
	while (keepLooping)
	{
		system("tput clear");
		if ( ! firstTime ) 
		{
			if  ( gProd_Info[0].service[0] != '\0' )
			{
				printf("\n\nProducts already selected include:\n");
				for (index=0; gProd_Info[index].service[0] != '\0'; index++)
				{
					printf("    %s", gProd_Info[index].service);
					if ( gProd_Info[index].numPorts > 0 )
					{
						printf(" - %d ports\n", gProd_Info[index].numPorts);
					}
					else
					{
						printf("\n");
					}
					fflush(stdout);
				}
			}
		}
		firstTime = 0;

		printf("\nAumtech Product: \n");
		printf("1) Telecom Services for Dialogic 6.1      (TEL)\n");
		printf("2) Telecom Services for Dialogic HMP 3.1  (TEL_HMP)\n");
		printf("3) Speech Recognition                     (SPEECH_REC)\n\n");
		printf("Select the product be licensed [1-3, or 'q' to quit]: ");
		fflush(stdout);
		
		ans=0;
		fgets(s_ans, sizeof(s_ans) - 1, stdin);
		if ( s_ans[strlen(s_ans) - 1] == '\n' )
		{
			s_ans[strlen(s_ans) - 1] = '\0';
		}
		ans = atoi(s_ans);

		switch(ans)
		{
			case 1:
				sprintf(pkgFile, "%s", "/home/.arcSingleDMTEL.pkginfo");
				sprintf(product, "%s", "arcSingleDMTEL");

				sprintf(gProd_Info[index].service, "%s", "TEL");
				if ((rc = sGetPkgInfo(product, pkgFile,
						gProd_Info[index].version, errMsg)) == -1)
				{
					sprintf(gProd_Info[index].version, "%s", "6.1");
				}

				while (*num_resources < 1)
				{	
					printf("\nNumber of resources requested [> 0]: ");
					fgets(s_num_resources, sizeof(s_num_resources) - 1, stdin);
					*num_resources = atoi(s_num_resources);
				}
				gProd_Info[index].numPorts = *num_resources;
				sprintf(gProd_Info[index].serviceStr, "%s_%s_%s_%d_%s",
					gProd_Info[index].service,
					gProd_Info[index].version,
					os,
					busbits,
					processor);

				index++;
	
				break;
			case 2:
				sprintf(pkgFile, "%s", "/home/.arcSingleDMTEL_HMP.pkginfo");
				sprintf(product, "%s", "arcDMTEL_HMP");

				sprintf(gProd_Info[index].service, "%s", "TEL_HMP");
				if ((rc = sGetPkgInfo(product, pkgFile,
						gProd_Info[index].version, errMsg)) == -1)
				{
					sprintf(gProd_Info[index].version, "%s", "6.1");
				}

				while (*num_resources < 1)
				{	
					printf("\nNumber of resources requested [> 0]: ");
					fgets(s_num_resources, sizeof(s_num_resources) - 1, stdin);
					*num_resources = atoi(s_num_resources);
				}
				gProd_Info[index].numPorts = *num_resources;
				sprintf(gProd_Info[index].serviceStr, "%s_%s_%s_%d_%s",
					gProd_Info[index].service,
					gProd_Info[index].version,
					os,
					busbits,
					processor);

				index++;
	
				break;
			case 3:
				sprintf(pkgFile, "%s", "");
				sprintf(product, "%s", "");
				sprintf(gProd_Info[index].service, "%s", "SPEECH_REC");
				sprintf(gProd_Info[index].version, "%s", "2.0");
				sprintf(gProd_Info[index].serviceStr, "%s_%s_%s_%d_%s",
					gProd_Info[index].service,
					gProd_Info[index].version,
					os,
					busbits,
					processor);
				index++;
				break;
			default:
				if ( strcmp(s_ans, "q") == 0 )
				{
					keepLooping = 0;
					continue;
				}
				printf("Invalid entry (%s).  Try again...\n", s_ans);
				continue;
				break;
		}
	}

	while (strcmp(s_lic_model, "") == 0 )
	{
		printf("\nLicensing Model \n");
		printf("1) Node-Locked\n");
		printf("2) Floating\n\n");
		printf("Select the Licensing Model [1 or 2]: ");
		fgets(s_lic_model, sizeof(s_lic_model) - 1, stdin);
		ans = atoi(s_lic_model);
		if ( (ans != 1) && (ans != 2) )
		{
			printf("Invalid entry (%s).  Must be 1 or 2.  Hit <enter> to "
				"try again.", s_lic_model);
			fgets(s_lic_model, sizeof(s_lic_model) - 1, stdin);
			sprintf(s_lic_model, "%s", "");
			continue;
		}
	}
	switch(ans)
	{
		case 1:
			sprintf(licenseModel, "%s", "Node-Locked");
			break;
		case 2:
			sprintf(licenseModel, "%s", "Floating");
			break;
	}

	return(0);
}

static int get_info_sip(char *service, int *num_resources, char *licenseModel,
				char *product)
{
	int	rc;
	char s_num_resources[32] = "";
	char s_lic_model[32] = "";
	char s_ans[10];
	char s_bogus[10];
	int  ans;
	int	keepLooping = 1;
	char		pkgFile[128]="";
	char		errMsg[256];
	int			index;
	char		version[32]="";
	int			firstTime = 1;

	*num_resources = 0;
	index = 0;
	while (keepLooping)
	{
		system("tput clear");
		if ( ! firstTime ) 
		{
			if  ( gProd_Info[0].service[0] != '\0' )
			{
				printf("\n\nProducts already selected include:\n");
				for (index=0; gProd_Info[index].service[0] != '\0'; index++)
				{
					printf("    %s", gProd_Info[index].service);
					if ( gProd_Info[index].numPorts > 0 )
					{
						printf(" - %d ports\n", gProd_Info[index].numPorts);
					}
					else
					{
						printf("\n");
					}
					fflush(stdout);
				}
			}
		}
		firstTime = 0;

		printf("\nAumtech Product: \n");
		printf("1) SIP Telecom Services               (IPTEL)\n");
		printf("2) Speech Recognition                 (SPEECH_REC)\n\n");
		printf("Select the product be licensed [1, 2, or 'q' to quit]: ");
		fflush(stdout);
		
		ans=0;
		fgets(s_ans, sizeof(s_ans) - 1, stdin);
		if ( s_ans[strlen(s_ans) - 1] == '\n' )
		{
			s_ans[strlen(s_ans) - 1] = '\0';
		}
		ans = atoi(s_ans);

		switch(ans)
		{
			case 1:
				sprintf(pkgFile, "%s", "/home/.arcSIPTEL.pkginfo");
				sprintf(product, "%s", "arcSIPTEL");

				sprintf(gProd_Info[index].service, "%s", "IPTEL");
				if ((rc = sGetPkgInfo(product, pkgFile,
										gProd_Info[index].version, errMsg)) == -1)
				{
					sprintf(gProd_Info[index].version, "%s", "6.1");
				}
				sprintf(version, "%s", gProd_Info[index].version);
				while (*num_resources < 1)
				{	
					printf("\nNumber of resources requested [> 0]: ");
					fgets(s_num_resources, sizeof(s_num_resources) - 1, stdin);
					*num_resources = atoi(s_num_resources);
				}
				gProd_Info[index].numPorts = *num_resources;
				sprintf(gProd_Info[index].serviceStr, "%s_%s_%s_%d_%s",
					gProd_Info[index].service,
					gProd_Info[index].version,
					os,
					busbits,
					processor);
				index++;
				break;
			case 2:
				sprintf(pkgFile, "%s", "");
				sprintf(product, "%s", "");
				sprintf(gProd_Info[index].service, "%s", "SPEECH_REC");
				sprintf(gProd_Info[index].version, "%s", "2.0");
				sprintf(gProd_Info[index].serviceStr, "%s_%s_%s_%d_%s",
					gProd_Info[index].service,
					gProd_Info[index].version,
					os,
					busbits,
					processor);
				index++;
				break;
			default:
				if ( strcmp(s_ans, "q") == 0 )
				{
					keepLooping = 0;
					continue;
				}
				printf("Invalid entry (%s).  Try again...\n", s_ans);
				continue;
				break;
		}
	}

	while (strcmp(s_lic_model, "") == 0 )
	{
		printf("\nLicensing Model \n");
		printf("1) Node-Locked\n");
		printf("2) Floating\n\n");
		printf("Select the Licensing Model [1 or 2]: ");
		fgets(s_lic_model, sizeof(s_lic_model) - 1, stdin);
		ans = atoi(s_lic_model);
		if ( (ans != 1) && (ans != 2) )
		{
			printf("Invalid entry (%s).  Must be 1 or 2.  Hit <enter> to "
				"try again.", s_lic_model);
			fgets(s_lic_model, sizeof(s_lic_model) - 1, stdin);
			sprintf(s_lic_model, "%s", "");
			continue;
		}
	}
	switch(ans)
	{
		case 1:
			sprintf(licenseModel, "%s", "Node-Locked");
			break;
		case 2:
			sprintf(licenseModel, "%s", "Floating");
			break;
	}

	return(0);
}

static int sGetOS(char *zOS, char *zErrorMsg)
{
	FILE			*fp;
	char			file[64] = "/etc/redhat-release";
	char			str[1024];
	char			os[32] = "";
	char			*ptr;
	int				centos7;
	char			junk[128];

	memset((char *) os, '\0', sizeof(os));
	zErrorMsg[0] = '\0';

	centos7 = isItCentos7(zErrorMsg);

	if((fp = fopen(file, "r")) == NULL)
    {
//        sprintf(zErrorMsg,
//			"Failed to open file (%s) for reading, [%d: %s]  ",
//			"Defaulte operating system to 5.1",
//			file, errno, strerror(errno));
	sprintf(zOS, "%s", "5.1");
        return(0);
    }

	memset((char *)str, '\0', sizeof(str));

	while(fgets(str, sizeof(str)-1, fp))
	{
		str[strlen(str) -1] = '\0';
		if ( centos7 )
		{
			sscanf(str, "%s %s %s %s %s", junk,junk,junk,os,junk);
		}
		else
		{
			sscanf(str, "%s %s %s %s", junk,junk,os,junk);
		}
		os[3]='\0';
		break;
	}
	sprintf(zOS, "%s", os);
	fclose(fp);
	return(0);

} // END: sGetOS() 

static int sGetProcessor(char *zProcessor, char *zErrorMsg)
{
	FILE			*fp;
	char			file[64] = "/proc/cpuinfo";
	char			str[1024];
	char			processor[8];
	char			*ptr;

	memset((char *) processor, '\0', sizeof(processor));
	zErrorMsg[0] = '\0';

	if((fp = fopen(file, "r")) == NULL)
    {
        sprintf(zErrorMsg,
			"Failed to open file (%s) for reading, [%d: %s]  ",
			"Cannot generate license request.  "
			"Contact Aumtech support to resolve this.",
			file, errno, strerror(errno));
        return(-1);
    }

	memset((char *)str, '\0', sizeof(str));

	while(fgets(str, sizeof(str)-1, fp))
	{
		if ( strncmp(str, "model name", 10) != 0 )
		{
			continue;
		}

		if ( (ptr=strstr(str, "AMD")) != NULL) 
		{
			sprintf(zProcessor, "%s", "AMD");
			fclose(fp);
			return(0);
		}

		if ( (ptr=strstr(str, "Xeon")) != NULL) 
		{
			sprintf(zProcessor, "%s", "Xeon");
			fclose(fp);
			return(0);
		}

		if ( (ptr=strstr(str, "Intel")) != NULL) 
		{
			sprintf(zProcessor, "%s", "Intel");
			fclose(fp);
			return(0);
		}
	}
	sprintf(zErrorMsg, 
			"Unable to determine processor for this system.  "
			"Please email the file (%s) to Aumtech along with this "
			"error message.  Cannot generate license request.",
			file);
	fclose(fp);
	return(-1);

} // END: sGetProcessor() 

static int sGetBusBits(int *zBusBits, char *zErrorMsg)
{
	FILE			*fp;
	char			file[64] = "/proc/cpuinfo";
	char			str[1024];
	char			processor[8];
	char			*ptr;

	memset((char *) processor, '\0', sizeof(processor));
	zErrorMsg[0] = '\0';

	*zBusBits = 32;
	if((fp = fopen(file, "r")) == NULL)
    {
        sprintf(zErrorMsg,
			"Failed to open file (%s) for reading, [%d: %s]  ",
			"Cannot generate license request.  "
			"Contact Aumtech support to resolve this.  ",
			file, errno, strerror(errno));
        return(-1);
    }

	memset((char *)str, '\0', sizeof(str));

	while(fgets(str, sizeof(str)-1, fp))
	{
		if ( strncmp(str, "cache_alignment", 15) != 0 )
		{
			continue;
		}

		if ( (ptr=strstr(str, "64")) != NULL) 
		{
			*zBusBits = 64;
			fclose(fp);
			return(0);
		}
	}
	fclose(fp);
	return(0);


} // END: sGetBusBits()

/*------------------------------------------------------------------------------
sGetPkgInfo():
------------------------------------------------------------------------------*/
static int sGetPkgInfo(char *zProduct, char *zPkgInfoFile,
                    char *zVersion, char *zMsg)
{
    FILE        *fp;
    char        buf[1024];
    char        *p;
    int         i;
    char        sVersion[64];
    char        sInstalled[64];

    memset((char *)sVersion, '\0', sizeof(sVersion));
    zMsg[0] = '\0';

    if ((fp = fopen(zPkgInfoFile, "r")) == NULL)
    {
        sprintf(zMsg, "Error: Unable to open (%s).  [%d, %s]. "
            "Product (%s) is not installed.  Install %s and retry.",
            zPkgInfoFile, errno, strerror(errno), zProduct, zProduct);
        return(-1);
    }

    (void)fseek(fp, 0, SEEK_END);
    (void)fseek(fp, -20, SEEK_CUR);

    memset((char *)buf, '\0', sizeof(buf));
    if ((fgets(buf, sizeof(buf) - 1, fp)) == NULL)
    {
        sprintf(zMsg, "Error: Unable to determine status/version of (%s).  "
            "Verify product %s is installed and retry.", zProduct, zProduct);
        fclose(fp);
        return(-1);
    }

    if ((strstr(buf, "REMOVED")) != NULL)
    {
        sprintf(zMsg, "Error: (%s) shows that %s is not installed.  "
            "Install %s and retry.", zPkgInfoFile, zProduct, zProduct);
        fclose(fp);
        return(-1);
    }

    // It is verified that it is installed; must get the version now.

    (void)fseek(fp, 0, SEEK_SET);
    memset((char *) buf, '\0', sizeof(buf));
    while  ((fgets(buf, sizeof(buf) - 1, fp)) != NULL)
    {
        buf[strlen(buf) - 1] = '\0';

        if ((p = strstr(buf, "VERSION")) == NULL)
        {
            continue;
        }
        sprintf(buf, "%s", p);
        p = strstr(buf, ":");
        p++;
        i = 0;
        memset((char *)sVersion, '\0', sizeof(sVersion));
        while (*p != '\0')
        {
            if ( (isdigit(*p)) || (*p == '.') )
            {
                sVersion[i++] = *p;
            }
            p++;
        }
    }
    if ( sVersion[0] == '\0' )
    {
        sprintf(zMsg, "Error: Unable to determine version from file (%s).  "
            "Verify the installation of %s and retry.", zPkgInfoFile, zProduct);
        fclose(fp);
        return(-1);
    }
    sprintf(zVersion, "%s", sVersion);

    fclose(fp);
    return(0);

} // END: sGetPkgInfo()

