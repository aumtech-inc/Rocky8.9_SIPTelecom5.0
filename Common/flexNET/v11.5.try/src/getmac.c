/*----------------------------------------------------------------------------
Program:	getmac.c
Purpose:	Aumtech licensing routine(s) for getting a MAC address.
		There are two routines here. lic_get_mac_address and 
		ndstat_get_mac_address. ndstat_get_mac_address should NEVER
		be used by any program but arcLicense since it has major
		security holes.
Author:		George Wenzel
Date:		03/16/99
Date:		04/01/99
----------------------------------------------------------------------------*/
/*#include "license.h"  */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/errno.h>

extern int errno;
int get_mac_address(char *uname, char *mac_addr, char *msg);
int get_mac_address_centos7(char *uname, char *mac_addr, char *msg);
int isItCentos7(char *msg);
static int remove_colons(char *factory_mac_addr, char *mac_addr);

#undef DEBUG
#ifdef DEBUG
	static int log_msg(char *s);
	static char Msg[512];
#endif

#ifdef DEBUG
static int log_msg(s)
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

/*-------------------------------------------------------------
Get the mac address of the machine.
--------------------------------------------------------------*/
int get_mac_address(uname, mac_addr, msg)
char *uname;
char *mac_addr;
char *msg;
{
	FILE *fp;
	char line[256];
	char cmd[128];
	char junk[128];
	char factory_mac_addr[128];
	char in_use_mac_addr[128];
	int		rc;

	if ( isItCentos7(msg) )
	{
		rc = get_mac_address_centos7(uname, mac_addr, msg);
		return(0);
	}

    sprintf(cmd,"/sbin/ifconfig -a 2>/dev/null");
    fp = popen(cmd, "r" );
    if ( fp == NULL)
	{
                sprintf(msg, "Failed to execute '%s'. errno=%d", cmd, errno);
                return(-1);
	}
	if(fgets(line, 256, fp) == NULL)
	{
		pclose(fp);
		return(-1);
	}
  	line[strlen(line) -1] = '\0';	
  	sscanf(line, "%s %s %s %s %s", junk,junk,junk,junk,factory_mac_addr);
	remove_colons(factory_mac_addr, mac_addr);
	pclose(fp);
	return(0);	
}

/*-------------------------------------------------------------
Get the mac address of the machine for centos 7 os
--------------------------------------------------------------*/
int get_mac_address_centos7(uname, mac_addr, msg)
char *uname;
char *mac_addr;
char *msg;
{
    FILE *fp;
    char line[256];
    char cmd[512];
    char junk[128];
    char factory_mac_addr[128];
    char in_use_mac_addr[128];
    char interface[128] = "eth0";

    *mac_addr = '\0';
    sprintf(cmd, "ip a |grep -A1 \"%s:\" | grep link |awk \'{ print $2 }\'", interface);
    fp = popen(cmd, "r" );
    if ( fp == NULL)
    {
                sprintf(msg, "Failed to execute '%s'. errno=%d", cmd, errno);
                return(-1);
    }
    while(fgets(line, 256, fp) != NULL)
    {
        if (line[strlen(line)-1] == '\n')
        {
                line[strlen(line)-1] = '\0';
        }
		remove_colons(line, mac_addr);
    }
    pclose(fp);

    if ( mac_addr[0] == '\0' )
    {
         sprintf(msg, "Unable to find mac address for interface (%s).", interface);
         return(-1);
    }

    return(0);

}
/*-------------------------------------------------------------
--------------------------------------------------------------*/
int isItCentos7(char *msg)
{
	FILE *fp;
	char line[256];
	char cmd[128];
	char junk[128];
	char six[64] = "";
	char seven[64] = "";

	sprintf(cmd,"cat /etc/redhat-release 2>/dev/null");
	fp = popen(cmd, "r" );
	if ( fp == NULL)
	{
                sprintf(msg, "Failed to execute '%s'. errno=%d", cmd, errno);
                return(-1);
	}
	if(fgets(line, 256, fp) == NULL)
	{
		pclose(fp);
		return(-1);
	}
	msg[0] = '\0';
  	line[strlen(line) -1] = '\0';	
  	sscanf(line, "%s %s %s %s %s", junk,junk,six,seven,junk);
//	fprintf(stderr, "[%d] line:(%s) six:(%s)  seven:(%s)\n", __LINE__, line, six, seven);

	pclose(fp);
	if ( seven[0] == '7' )
	{
		return(1);
	}
	return(0);	

} // END: isItCentos7()

static int remove_colons(in_string, out_string)
char *in_string;
char *out_string;
{
	int i=0,j=0;
	char tmp_string[256];

	memset(tmp_string, 0, sizeof(tmp_string));
	while ( in_string[i] )
	{
	  	if ( in_string[i] != ':' ) 
			tmp_string[j++] = toupper(in_string[i]);
		i++;
	}
	strcpy(out_string, tmp_string);
	return(0);
} 

static int log_msg(s)
char *s;
{
	fprintf(stdout, "%s\n", s);
	fflush(stdout);
	return(0);
}

#ifdef TESTING
main(int argc, char **argv)
{
	char msg[256];
	char mac[64];
	int		rc;
	
	get_mac_address("junk", mac, msg);

	fprintf(stdout,"Got mac=<%s>\n", mac); fflush(stdout);
	return(0);

}
#endif
