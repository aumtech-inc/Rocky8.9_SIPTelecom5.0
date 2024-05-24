/*------------------------------------------------------------------------------
File:		portActive.c
Purpose:	Determine whether ports are active or not.
Author:		Sandeep Agate
Date:		10/02/97 sja	Created the file
Update:		03/24/98 gpw	Passed all error messages instead of loggin them
Update:		03/25/98 gpw	Added convert_to_upper for onOff
Update:	03/03/99 mpb	changed MAXPORTS from 110 to 130
Update:	04/19/99 mpb	changed MAXPORTS from 130 to 200
Update:	07/26/99 mpb	Added Warnning to a error message.
------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <time.h>

#undef DEBUG 

#define MAXPORTS		200

/* Typedefs */
typedef struct resactEntry
{
	short	slotTaken;

	char 	port[5];
	char 	host[50];
	char 	token1[50];
	char 	token2[50];
	char 	priority[5];
	char 	rule[5];
	char 	startDate[10];
	char 	stopDate[10];
	char 	startTime[10];
	char 	stopTime[10];
	char 	onOff[5];
} ResactEntry;

typedef struct resactTable
{
	char		name[256];
	ResactEntry	entries[MAXPORTS];
} ResactTable;


/* Static Vars */
static ResactTable	resact;			/* The res. act. table */
static short		tableInMemory = 0;	/* Have we loaded the table? */

/* Global Function Prototypes */
int loadResactTable(char *failureReason);
int portActive(int port, int *state, char *failureReason);

/* Static Function Prototypes */
static int checkRule(char *start_date_str, char *stop_date_str, 
      char *start_time_str, char *stop_time_str, int rule, char *failureReason);
static int ulGetField(char delim, char *buffer, int fld_num, char *field);
static int ulReadLine(char *the_line, int maxlen, FILE *fp);
static int convert_to_upper(char *buf);

extern int	errno;

/*------------------------------------------------------------------------------
portActive(): Check if the port is active.
state:		0 	--> port is not active
		1	--> port is active
Return Codes:	0	SUCCESS		Port status determined successfully
		-1	FAILURE		Unable to determine port status
------------------------------------------------------------------------------*/
int portActive(int port, int *state, char *failureReason)
{
	int	rc;
	short	ruleError;

	*state = 1; /* Default: state = active */

	if(tableInMemory == 0)
	{
		sprintf(failureReason, "Resact. table not loaded");
		return(-1);
	}
	if(port < 0 || port >= MAXPORTS)
	{
		sprintf(failureReason, "Port %d should be >= 0 & <= %d", 
							port, MAXPORTS);
		return(-1);
	}

	/*  Check if we need to change the state of the port.
	    If rule succeeds, change state, else continue.  */

	ruleError = 0;
	rc = checkRule( resact.entries[port].startDate,
			resact.entries[port].stopDate,
			resact.entries[port].startTime,
			resact.entries[port].stopTime,
			atoi(resact.entries[port].rule),
			failureReason);

	switch(rc)
	{
		case 0:		/* Success 			*/
			ruleError = 0;
			break;

		case 1:		/* Rule failed 			*/
		case 2:		/* Invalid input		*/
		case 3:		/* Logic error			*/
		case 4:		/* Rule not implemented 	*/
			ruleError = 1;
			break;

		default:	/* Miscellaneous Error 		*/
			return(-1);
	} 

	if(ruleError) return(0);

	/* All is well, now change state */
	if(strcmp(resact.entries[port].onOff, "OFF") == 0)
	{
		*state = 0;
	}
	else	/* Anything else but OFF means port is ON */
	{
		*state = 1;
	}
	return(0);
} 

/*------------------------------------------------------------------------------
loadResactTable(): Load the resource activation table into memory.
Return Codes:	0	SUCCESS		Loaded table successfully
		-1	FAILURE		Unable to load table
------------------------------------------------------------------------------*/
int loadResactTable(char *failureReason)
{
	FILE	*configFp;
	char	line[128];
	short	invalidEntry;
	struct 	utsname myhost;
	int	rc;
	int	port_no;
	char	*ispbase;
	register int	i;

	/* Table Fields */
	char port[5];
	char host[50];
	char token1[50];
	char token2[50];
	char priority[5];
	char rule[5];
	char startDate[10];
	char stopDate[10];
	char startTime[10];
	char stopTime[10];
	char onOff[256];

	tableInMemory = 0;
	memset(&resact, 0, sizeof(ResactTable));

	/*  Mark all ports to be active. */
	for(i=0; i<MAXPORTS; i++)
	{
		sprintf(resact.entries[i].onOff, "%s", "ON");
	}

	/* Get local host information */
	if(uname(&myhost) < 0)
	{
		sprintf(failureReason, "Cannot get host information, "
					"errno=%d (%s)", 
				 	errno, strerror(errno));
		return(-1);
	}
	if((ispbase = (char *)getenv("ISPBASE")) == (char *)NULL)
	{
		sprintf(failureReason, "Env. var ISPBASE not set");
		return(-1);
	}

  	sprintf(resact.name, "%s/Telecom/Tables/resact", ispbase); 

	if(access(resact.name, F_OK|R_OK) < 0)
	{
		sprintf(failureReason, "Warnning:Cannot access res. activation"
					" file (%s), errno=%d (%s)", 
				 	resact.name, errno, strerror(errno));
		return(-1);
	}
	if((configFp = fopen(resact.name, "r")) == NULL)
	{
		sprintf(failureReason, "Cannot open table config file "
					"(%s) for reading, errno=%d (%s)", 
				 	resact.name, errno, strerror(errno));
		return(-1);
	}
	memset(line, '\0', sizeof(line));
	while(ulReadLine(line, sizeof(line)-1, configFp) != 0)
	{
		invalidEntry = 0;

		memset(port, 		0, sizeof(port));
		memset(host, 		0, sizeof(host));
		memset(token1, 		0, sizeof(token1));
		memset(token2, 		0, sizeof(token2));
		memset(priority, 	0, sizeof(priority));
		memset(rule, 		0, sizeof(rule));
		memset(startDate, 	0, sizeof(startDate));
		memset(stopDate, 	0, sizeof(stopDate));
		memset(startTime, 	0, sizeof(startTime));
		memset(stopTime, 	0, sizeof(stopTime));
		memset(onOff, 		0, sizeof(onOff));

		ulGetField('|', line, 1, 	port);
		ulGetField('|', line, 2, 	host);
		ulGetField('|', line, 3, 	token1);
		ulGetField('|', line, 4,	token2);
		ulGetField('|', line, 5, 	priority);
		ulGetField('|', line, 6, 	rule);
		ulGetField('|', line, 7, 	startDate);
		ulGetField('|', line, 8, 	stopDate);
		ulGetField('|', line, 9, 	startTime);
		ulGetField('|', line, 10, 	stopTime);
		ulGetField('|', line, 11,	onOff);

		/* If any of the required fields are blank, skip line.
		 * Fields that are currently ignored are commented out.
		 * We can add error checking for these lines when we choose
		 * to use them.  */
		if(	port[0] 	== '\0' ||
			host[0] 	== '\0' ||
/* 			token1[0] 	== '\0' || */
/* 			token2[0] 	== '\0' || */
/* 			priority[0] 	== '\0' || */
			rule[0] 	== '\0' ||
			startDate[0] 	== '\0' ||
			stopDate[0] 	== '\0' ||
			startTime[0] 	== '\0' ||
			stopTime[0] 	== '\0' ||
			onOff[0] 	== '\0'     )
		{
			invalidEntry = 1;
		}

		port_no=atoi(port);

		/* Check field values */
		if(port_no < 0 || port_no >= MAXPORTS)
		{
			invalidEntry = 1;
		}
		/* Is this entry for our host? */
		if((host[0] != '*') && (strcmp(host, myhost.nodename) != 0))
		{
			invalidEntry = 1;
		}
		if(invalidEntry) continue;
		
		/* All's well. Now save the entry.  */
		sprintf(resact.entries[port_no].port,		"%s",port);
		sprintf(resact.entries[port_no].host,		"%s",host);
		sprintf(resact.entries[port_no].token1,		"%s",token1);
		sprintf(resact.entries[port_no].token2,		"%s",token2);
		sprintf(resact.entries[port_no].priority,	"%s",priority);
		sprintf(resact.entries[port_no].rule,		"%s",rule);
		sprintf(resact.entries[port_no].startDate,	"%s",startDate);
		sprintf(resact.entries[port_no].stopDate,	"%s",stopDate);
		sprintf(resact.entries[port_no].startTime,	"%s",startTime);
		sprintf(resact.entries[port_no].stopTime,	"%s",stopTime);
		convert_to_upper(onOff);
		sprintf(resact.entries[port_no].onOff,		"%s",onOff);
		resact.entries[port_no].slotTaken = 1;

	} /* while */

	fclose(configFp);
	tableInMemory = 1;
	return(0);
} 

/*---------------------------------------------------------------------------
checkRule(): Check port status based on rules.
------------------------------------------------------------------------------*/
static int checkRule(char *start_date_str, char *stop_date_str,
		char *start_time_str, char *stop_time_str, 
		int rule, char *failureReason)
{
	time_t		now;
	struct	tm	*t;
	int		start_date, stop_date, start_time, stop_time, ret;

	start_date 	= atoi(start_date_str);
	stop_date 	= atoi(stop_date_str);
	start_time 	= atoi(start_time_str);
	stop_time  	= atoi(stop_time_str);


	if ((time(&now) == (time_t) -1) || ((t=localtime(&now)) == NULL))
	{
		sprintf(failureReason,"Failed to get current time,errno=%d(%s)",
						errno, strerror(errno));
		return(-1);
	}

	switch(rule)
	{
		case 0:
			ret = sc_rule0(start_date, stop_date, start_time,
					stop_time, t->tm_year, t->tm_mon,
					t->tm_wday, t->tm_mday, t->tm_hour,
					t->tm_min, t->tm_sec);
			break;
		case 1:
			ret = sc_rule1(start_date, stop_date, start_time,
					stop_time, t->tm_year, t->tm_mon, 
					t->tm_wday, t->tm_mday, t->tm_hour, 
					t->tm_min, t->tm_sec);
			break;
		case 2:
			ret =  sc_rule2(start_date, stop_date, start_time, 
					stop_time, t->tm_year, t->tm_mon, 
					t->tm_wday, t->tm_mday, t->tm_hour, 
					t->tm_min, t->tm_sec);
			break;
		case 3:
			ret = sc_rule3(start_date, stop_date, start_time, 
					stop_time, t->tm_year, t->tm_mon, 
					t->tm_wday, t->tm_mday, t->tm_hour, 
					t->tm_min, t->tm_sec);
			break;
		case 4:
			ret =  sc_rule4(start_date, stop_date, start_time, 
					stop_time, t->tm_year, t->tm_mon, 
					t->tm_wday, t->tm_mday, t->tm_hour, 
					t->tm_min, t->tm_sec);
			break;
		case 5:
			ret =  sc_rule5(start_date, stop_date, start_time, 
					stop_time, t->tm_year, t->tm_mon, 
					t->tm_wday, t->tm_mday, t->tm_hour, 
					t->tm_min, t->tm_sec);
			break;
		case 6:
			ret = sc_rule6(start_date, stop_date, start_time, 
					stop_time, t->tm_year, t->tm_mon, 
					t->tm_wday, t->tm_mday, t->tm_hour, 
					t->tm_min, t->tm_sec);
			break;
		case 7:
			ret = sc_rule7(start_date, stop_date, start_time, 
					stop_time, t->tm_year, t->tm_mon, 
					t->tm_wday, t->tm_mday, t->tm_hour, 
					t->tm_min, t->tm_sec);
			break;
		case 8:
			ret = sc_rule8(start_date, stop_date, start_time, 
					stop_time, t->tm_year, t->tm_mon, 
					t->tm_wday, t->tm_mday, t->tm_hour, 
					t->tm_min, t->tm_sec);
			break;
		case 9:
			ret =  sc_rule9(start_date, stop_date, start_time, 
					stop_time, t->tm_year, t->tm_mon, 
					t->tm_wday, t->tm_mday, t->tm_hour, 
					t->tm_min, t->tm_sec);
			break;
		default:
			sprintf(failureReason,"Invalid rule # %d passed.");
			return(-1);
	} 

	return(ret);
} 

/*--------------------------------------------------------------------
ulReadLine(): 	Read a line from a file pointer
--------------------------------------------------------------------*/
static int ulReadLine(char *the_line, int maxlen, FILE *fp)
{
	char	*ptr;
	char	mybuf[BUFSIZ];

	strcpy(the_line, "");
	while(fgets(mybuf,maxlen,fp))
	{
		if(!strcmp(mybuf,""))
			continue;
		mybuf[(int)strlen(mybuf)-1] = '\0';
		ptr = mybuf;
		while(isspace(*ptr++))
			;
		--ptr;
		if(!strcmp(ptr,""))
			continue;
		if(ptr[0] == '#')
			continue;
		strcpy(the_line, mybuf);
		return(1);
	} /* while */
	return(0);
} /* END: ulReadLine() */
/*--------------------------------------------------------------------
ulGetField(): 	This routine extracts the value of the desired field 
		from the data record.

Dependencies: 	None.

Input:
	delim	: that separates fields in the buffer.
	buffer	: containing the line to be parsed.
	fld_num	: Field # to be extracted from the buffer.
	field	: Return buffer containing extracted field.

Output:
	-1	: Error
	>= 0	: Length of field extracted
--------------------------------------------------------------------*/
static int ulGetField(char delim, char *buffer, int fld_num, char *field)
{
	register int     i;
	int     fld_len, state, wc;

	fld_len = state = wc = 0;

	field[0] = '\0';
	if(fld_num < 0)
        	return (-1);

	for(i=0;i<(int)strlen(buffer);i++) 
        {
        	if(buffer[i] == delim || buffer[i] == '\n')
                {
                	state = 0;
                	if(buffer[i] == delim && buffer[i-1] == delim)
                        	++wc;
                }
        	else if (state == 0)
                {
                	state = 1;
                	++wc;
                }
        	if (fld_num == wc && state == 1)
		{
                	field[fld_len++] = buffer[i];
		}
        	if (fld_num == wc && state == 0)
		{
                	break;
		}
        } /* for */
	if(fld_len > 0)
        {
        	field[fld_len] = '\0';
        	while(field[0] == ' ')
                {
                	for (i=0; field[i] != '\0'; i++)
                        	field[i] = field[i+1];
                }
        	fld_len = strlen(field);
        	return (fld_len);
        }
	return(-1);
} /* END: ulGetField() */

/*--------------------------------------------------------------------
convert_to_upper():
--------------------------------------------------------------------*/
static int convert_to_upper(char *buf)
{
        register int    index;

        if(buf == (char *)NULL) return(0);
        for(index=0; index<(int)strlen(buf); index++)
                 buf[index] = toupper(buf[index]);
        return(0);
}

