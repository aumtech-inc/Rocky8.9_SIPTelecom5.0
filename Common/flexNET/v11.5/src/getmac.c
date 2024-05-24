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
#include <ctype.h>
#include <sys/errno.h>

extern int errno;
int get_mac_address(char *interface, char *mac_addr, char *msg);
static int remove_colons(char *factory_mac_addr, char *mac_addr);

/*-------------------------------------------------------------
Get the mac address of the machine for centos 7 os
--------------------------------------------------------------*/
int get_mac_address(char *interface, char *mac_addr, char *msg)
{
    FILE *fp;
    char line[256];
    char cmd[512];

    *mac_addr = '\0';
    sprintf(cmd, "nmcli device show  %s |grep GENERAL.HWADDR | awk \'{ print $2 }\'", interface);
    fp = popen(cmd, "r" );
    if ( fp == NULL)
    {
		fprintf(stderr, "[%d] Failed to get mac address with command (%s). Correct and retry.", __LINE__, cmd);
		return(-1);
    }

	memset((char *)line, '\0', sizeof(line));
    fgets(line, 256, fp);
    pclose(fp);

	if ( strlen(line) ==  0)
    {
		fprintf(stderr, "[%d] Failed to get mac address with command (%s). Correct and retry.", __LINE__, cmd);
		return(-1);
	}

	if (line[strlen(line)-1] == '\n')
	{
		line[strlen(line)-1] = '\0';
	}
	remove_colons(line, mac_addr);
    if ( mac_addr[0] == '\0' )
    {
		fprintf(stderr, "[%d] Failed to get mac address with command (%s). Correct and retry.", __LINE__, cmd);
		return(-1);
	}

    return(0);

} // END: get_mac_address()

/*-------------------------------------------------------------
--------------------------------------------------------------*/
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
