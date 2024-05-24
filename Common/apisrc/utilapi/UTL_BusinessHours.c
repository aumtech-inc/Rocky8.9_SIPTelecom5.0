#ident	"@(#)UTL_BusinessHours 12/05/95 2.2.0 Copyright 1996 Aumtech, Inc."
static char ModuleName[]="UTL_BusinessHours";
/*-----------------------------------------------------------------------------
Program:	UTL_BusinessHours.c
Purpose:	Get business hours from an ascii file.
Author:		A. Ashok
Date:		01/20/95
Update:		12/05/95 G. Wenzel Updated for ISP 2.2.
Update:		08/06/95 G. Wenzel removed "I am here" msg in IsOut routine.
Update:	06/07/99 mpb Added errno & msg to COM_ERR_FILEACCESS message.
Update:		03/01/2000 gpw added current_year & fixed Year 2000 bug, when a
		null date and time are passed.
Update:	2000/10/07 sja 4 Linux: Added function prototypes
Update:	2000/11/30 sja	Changed sys_errlist[] to strerror()
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "COM_Mnemonics.h"
#include "monitor.h"

#define MAX_DATA_SIZE	512
#define TIME_LEN	5

#define  OPEN_STR	"OPEN"
#define  OUT_STR	"OUT"
#define  EXCEPT_STR	"EXCEPTION"

static int day_tab[2][13] = {
	{0,31,28,31,30,31,30,31,31,30,31,30,31},
	{0,31,29,31,30,31,30,31,31,30,31,30,31}
};


/*
 * Static Function Prototypes
 */
static int	ConvDt2Day(char *date, char *pday);
static int Convert2days(int pyear,int pmon, int pday);
static int dayOfYear(int year,int month,int day);
static int TotLeapYrs(int pstartYr,int pendYr);
static int	IsOpen(char *fname,char *pday,char *ptime);
static int	IsOut(char *fname,char *pday,char *ptime);
static int CheckTimeFormat(char *ptime);
static int IsTimeInRange(char *prange, char *ptime);
static int LookUpException(char *fname,char *pday,char *time) ;
static int	ProcessBussHour(char *fname,char *pday,char *time);
static int	ProcessException(char *fname,char *pday,char *time);
static int Profile_GetString(char *initFileName,char *section,char *token,
							char *str);
static FILE *FileOpen(char *x,char *y);
static int FileClose(FILE *fp);
static int FileExists (char * filename);

int UTL_BusinessHours(char *fname,char *date,char *ptime,int *retcode)
{
char	diag_mesg[1024];
char	retDay[20];
int	retVal=0,ret_code;
time_t	lclock;
struct	tm *ltime;
char	mydate[30],mytime[30];
int	current_year;

	sprintf(diag_mesg,"UTL_BusinessHours(%s %s %s %s)", 
		fname,date,ptime,"retcode"); 
	ret_code = send_to_monitor(MON_API, UTL_BUSINESSHOURS, diag_mesg);
	if ( ret_code < 0 )
	{
        sprintf(diag_mesg, "send_to_monitor failed. rc = %d", ret_code);
        LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ISPDEBUG, diag_mesg);
	}

	/* Check to see if Init'Service' API has been called. */
	if(GV_InitCalled != 1){
    	  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOTINIT,"Service");
    	  return (-1);
	}

	if (FileExists(fname) <= 0){
	  return -1;
	}


	if ((date[0] == '\0') || (ptime[0] == '\0')){
           time(&lclock);
	   ltime = localtime(&lclock); 
	}

	if (date[0] == '\0')
	{
		/* current year may need adjustment so day of week is correct */
		current_year=ltime->tm_year;
		if (current_year >= 100) current_year = current_year - 100;
	   	sprintf(mydate,"%02d/%02d/%02d",ltime->tm_mon + 1,
				ltime->tm_mday, current_year); 
	}
	else {
	   if (strlen(date) > 8){
	     LOGXXPRT(ModuleName,REPORT_NORMAL,COM_GENSTR,
			"ERROR: date string bad format");
	     return -1;
	   } else
	     strcpy(mydate,date);
	}

	if (ptime[0] == '\0')
	   sprintf(mytime,"%02d:%02d",ltime->tm_hour,ltime->tm_min);
	else {
	   if (strlen(ptime) > 5){
	     LOGXXPRT(ModuleName,REPORT_NORMAL,COM_GENSTR,
			"ERROR: time string bad format");
	     return -1;
	   } else
	     strcpy(mytime,ptime);
	}

	sprintf(diag_mesg,"date[%s] time[%s]", mydate,mytime); 
	LOGXXPRT(ModuleName,REPORT_DEBUG,COM_ISPDEBUG, diag_mesg);

	retVal=LookUpException(fname,mydate,mytime);
	if (retVal < 0)
	  return retVal;

	if (retVal == 0){ 
	  ConvDt2Day(mydate,retDay);
	  retVal = ProcessBussHour(fname,retDay,mytime);
	} else 
	  if (retVal == 1)
	    retVal = ProcessException(fname,mydate,mytime);

        /* Less than 0 is error. */
	if (retVal < 0) 
	{
		return retVal;
	}
	else 
	{
          	/* Greater than 0 is OPEN_HRS, CLOSED_HRS, or OUT_HRS. */
	  	*retcode = retVal; 
	}

	return 0;
}

static int	ConvDt2Day(char *date, char *pday)
{
char	*tmpPtr,convStr[50],actDay[10]; 		
int	i,j,day,mon,yr,offset,elapsedDays;
char	operStr[10],monStr[3],dayStr[3],yrStr[3];
/* These must be in this order since Jan 1 1970 is Thursday. */
char    *dayOfWeek[7] = { "THURSDAY",
			  "FRIDAY",
			  "SATURDAY",
			  "SUNDAY",
			  "MONDAY",
			  "TUESDAY",
			  "WEDNESDAY",
			 };


	strcpy(operStr,date);

	tmpPtr=(char *)strtok(operStr,"/");
	strcpy(monStr,tmpPtr);

	tmpPtr=(char *)strtok(NULL,"/");
	strcpy(dayStr,tmpPtr);

	tmpPtr=(char *)strtok(NULL,"/");
	strcpy(yrStr,tmpPtr);


	day = atoi(dayStr);
	mon = atoi(monStr);
	yr = atoi(yrStr);

	/* Calculate num days elapsed since Jan 1 1970	which is Thursday*/

	elapsedDays = Convert2days(yr,mon,day); 

	offset = elapsedDays % 7;

	LOGXXPRT(ModuleName,REPORT_DEBUG,COM_ISPDEBUG,dayOfWeek[offset]);
	strcpy(pday,dayOfWeek[offset]);

}

static int Convert2days(int pyear,int pmon, int pday)
{
int	StartYr,EndYr;
int	totYears,leapYears,normYears;
int	numDays;

	StartYr = 1970; 
	if(pyear < 70)
	  EndYr = 2000 + pyear;
	else
	  EndYr = 1900 + pyear;

	/* Number of years since 1970 	*/
	totYears = EndYr - StartYr;

	/* Calculate number of leap years between start year and End Year */
	leapYears = TotLeapYrs(StartYr,EndYr); 
	normYears = totYears - leapYears;

	/* Number of days since 1970 Jan 1 */
	numDays = normYears*365 + leapYears*366 + dayOfYear(pyear,pmon,pday)-1;

	return numDays;
 
}

static int dayOfYear(int year,int month,int day)
{
int	i,leap;

	/* Century years (eg 2000) divisible by 400 are leap years. */
	leap = (year%4 == 0 && year%100 != 0) || year%400 ==0;

	for(i=1;i<month;i++)
	  day += day_tab[leap][i];

	return day;

}

static int TotLeapYrs(int pstartYr,int pendYr)
{
int begYr,leap,leapCnt;

	leapCnt = 0;
	begYr = pstartYr;
	while (begYr < pendYr){
	  /* Century years (eg 2000) divisible by 400 are leap years. */
	  leap =  (begYr%4 == 0 && begYr%100 != 0) || begYr%400 == 0;
	  if (leap)
	    leapCnt++;
	  begYr++;
	}

	return leapCnt;
}


static int	IsOpen(char *fname,char *pday,char *ptime)
{
int	i,retVal=0;
char	tokStr[20];
char	timeRange[20];

	i=1;
	sprintf(tokStr,"%s%d",OPEN_STR,i);
	while ((retVal=Profile_GetString(fname,pday,tokStr,timeRange))==1){
		retVal = IsTimeInRange(timeRange,ptime);
		if (retVal != 0)
		  return retVal;
		i++;
	        sprintf(tokStr,"%s%d",OPEN_STR,i);
	}
	return retVal;

}

static int	IsOut(char *fname,char *pday,char *ptime)
{
int	i,retVal=0;
char	tokStr[20];
char	timeRange[100];

	i=1;
	sprintf(tokStr,"%s%d",OUT_STR,i);
	while ((retVal=Profile_GetString(fname,pday,tokStr,timeRange)) == 1){
		retVal = IsTimeInRange(timeRange,ptime);
		if (retVal != 0)
		  return retVal;
		i++;
	        sprintf(tokStr,"%s%d",OUT_STR,i);
	}
	return retVal;

}

static int CheckTimeFormat(char *ptime)
{
char	operStr[10],tmpbuf[255];
int	i,len;
unsigned int errFlag = 0;

	strcpy(operStr,ptime);
	len = strlen(operStr);

	if (len != 5)	/* time format hh:mm len is 5 */ 
	  errFlag = 1; 

	for (i=0;i<TIME_LEN ;i++){
	   switch(i){
	     case 0: if ((operStr[i] >= '0' ) && (operStr[i] <= '2'))
		     break;
		   else
	  	     errFlag = 1; 
	     case 1: if ((operStr[i] >= '0' ) && (operStr[i] <= '9'))
		     break;
		   else
	  	     errFlag = 1; 
	     case 2: if (operStr[i] == ':' )
		     break;
		   else
	  	     errFlag = 1; 
	     case 3: if ((operStr[i] >= '0' ) && (operStr[i] <= '5'))
		     break;
		   else
	  	     errFlag = 1; 
	     case 4: if ((operStr[i] >= '0' ) && (operStr[i] <= '9'))
		     break;
		   else
	  	     errFlag = 1; 
	     default:
		    break;
	   }
	   if (errFlag == 1)
		break;
	}

	if (errFlag == 1){
	  sprintf(tmpbuf,"ERROR:Bad File Format- Illegal time - %s.\n",ptime);
	  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_GENSTR,tmpbuf);
          return -1;
	}
	return 0;

}

static int IsTimeInRange(char *prange, char *ptime)
{
char	begTim[50],endTim[50],operStr[10];
char	*tmpPtr,tmpbuf[255];
int	offLen,i,retVal;
unsigned int found;

	strcpy(operStr,prange);

	i=0;
	found = 0;
	while (operStr[i] != '\0'){
          if (operStr[i] == '-'){
	    found = 1;
	    operStr[i] = '\0';
	    break;
	  }
	  i++;
	}
	if (found == 1){
	  strcpy(begTim,operStr);
	  offLen = strlen(operStr)+1;
	  strcpy(endTim,&prange[offLen]);
	} else {
	  sprintf(tmpbuf,"ERROR:Bad File Format- Illegal time - %s.\n",prange);
	  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_GENSTR,tmpbuf);
          return -2;
	}

	retVal = CheckTimeFormat(begTim);
	if (retVal < 0)
	  return -2;

	retVal = CheckTimeFormat(endTim);
	if (retVal < 0)
	  return -2;

	if ((strcmp(begTim,ptime) <= 0) && (strcmp(ptime,endTim) <= 0))
	   return 1;
	else
	   return 0;

}


static int LookUpException(char *fname,char *pday,char *time) 
{
int	retVal;
char	timeRange[20];

	retVal=Profile_GetString(fname,EXCEPT_STR,pday,timeRange);

	return retVal;
}

static int	ProcessBussHour(char *fname,char *pday,char *time)
{
int	retVal;

	  retVal=IsOpen(fname,pday,time);

	  if (retVal < 0)
	    return retVal;

	  if (retVal == 1)
	    return OPEN_HRS;

	  if (retVal == 0){ 
	      retVal = IsOut(fname,pday,time);
	      if (retVal < 0)
	        return retVal;
	      if (retVal == 1)
	        return OUT_HRS;
	      else 
	        return CLOSED_HRS;
	  }
}

static int	ProcessException(char *fname,char *pday,char *time)
{
int	retVal;
char	yesNo[20];
char	tmpbuf[255];

	  retVal=Profile_GetString(fname,EXCEPT_STR,pday,yesNo);
	  if (strcmp(yesNo,"NO") == 0)
	    return CLOSED_HRS;
	  else
	    if (strcmp(yesNo,"YES") == 0){
	      retVal = ProcessBussHour(fname,pday,time);
	      return retVal;
            } else {
  	      sprintf(tmpbuf,"ERROR:Bad File Format-Illegaltoken-%s.\n",yesNo);
	      LOGXXPRT(ModuleName,REPORT_NORMAL,COM_GENSTR,tmpbuf);
	      return -2;
	    }
}

/* section eg MONDAY */
/* token   eg OPEN1 */
/* str     eg 08:00-12:00, YES (to right of =) */
static int Profile_GetString(char *initFileName,char *section,char *token,				char *str)
{
  /* write curr proj */
  char line[255];
  char tmpbuf[255];
  FILE *fp;
  char outStr[MAX_DATA_SIZE+1];
  char theSection[MAX_DATA_SIZE+1];

  fp = FileOpen(initFileName, "r");
  sprintf (theSection, "[%s]", section);
  if (fp != NULL)
  {
    /* If section is not found, then bad file format. */
    fgets(line, sizeof(line), fp);
    while ( strncmp(line, theSection, strlen(theSection)) != 0 ) { 
      if ( fgets(line, sizeof(line), fp) == NULL ) {
        FileClose(fp);
        outStr[0] = '\0';
  	sprintf (tmpbuf, "ERROR: Bad File Format- Probably, missing day of \
		week or incorrect Open/Out entry for a specific day.\n");
	LOGXXPRT(ModuleName,REPORT_NORMAL,COM_GENSTR,tmpbuf);
        return -2;
      }
    }
    while ( strncmp(line, token, strlen(token)) != 0 ) {
      if ( fgets(line, sizeof(line), fp) == NULL || line[0] == '[' ) {
        FileClose(fp);
        outStr[0] = '\0';
        return 0;
      }
    }
    /* Get right side of equation. */
    strcpy(outStr, &line[strlen(token)+1]);
    if ( strrchr(outStr, '\n') != NULL ) {
      outStr[strlen(outStr)-1] = '\0';  /* knock off the CRLF */
    }
    FileClose(fp);
    strcpy(str, outStr);
    return 1;
  }
  else
  {
    return 0;
  }
}


static FILE *FileOpen(char *x,char *y)
{
	return fopen(x,y);
}

static int FileClose(FILE *fp)
{
	return fclose(fp);
}

static int FileExists (char * filename)
{
  FILE *Result;
  char comment[150];

  Result = FileOpen(filename, "r");
  if (!Result) { 
    if (errno == ENOENT) 
    {
      LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOFILE, filename);
      return(0);
    }

    LOGXXPRT(ModuleName,REPORT_NORMAL, COM_ERR_FILEACCESS,
				 filename, errno, strerror(errno));
    return(-1);
  }
  FileClose(Result);
  return(1);
}
