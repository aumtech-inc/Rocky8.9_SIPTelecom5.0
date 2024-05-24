/*---------------------------------------------------------------------------- 
Program:	FaxServer.c 
Purpose:	To send scheduled faxes.  
Author:		George Wenzel, based on work previously done by gpw, 
		mb, sja, djb, and apj
Date:		03/19/2003
Update: 04/22/03 mpb Changed mkdirp to mkdir and back to mkdirp.
----------------------------------------------------------------------------*/

#include <stdlib.h>


#include "TEL_Common.h" 
#include "ISPCommon.h" 
#include "arcFAX.h" 
#include <time.h> 

/* constant definitions */

#define DONT_FAIL_ON_BUSY 2
#define USE_DEFAULT	0
#define FAX_SUCCESS	0
#define A_SUCCESS	0 
#define ELIGIBLE	0 
#define FAILURE		-1 
#define ALREADY_LOCKED	-2 
#define	NOT_ELIGIBLE	-3
#define	INVALID_PARM	-4	
#define	NO_FAX_FILE	-5	
#define	TOO_MANY_TRIES	-6

#define WORK_DIR	"work"
#define ERR_DIR		"errors"
#define SENT_DIR	"sent"
#define FAX_DIR		"faxfile"
#define HDR_DIR		"header"

#define ANSWERED_BY_FAX	151 	/* Phone was answered by a fax */
#define MAX_RINGS	10	/* number of rings when we send the fax */

extern char *GV_TELmsg[];

/* Global Variables */

char	Fax_work_dir[256];
char	Fax_sent_dir[256];
char	Fax_err_dir[256];

char	Hdr_work_dir[256];
char	Hdr_sent_dir[256];
char	Hdr_err_dir[256];

char 	Result_line[256];	/* Result of processing; appended to header */
char 	log_buf[512];		/* General message string for logging */

char 	Current_yyyymmddhhmmss[40];

static char	ModuleName[] = "FaxServer";
static int	Port;
static int 	Retry_interval;	/* number of seconds to wait before next try */ 

static int createDirIfNotPresent(char *zDir)
{
	int rc;
	mode_t          mode;
	struct stat     statBuf;

	rc = access(zDir, F_OK);
	if(rc == 0)
	{
        /* Is it really directory */
		rc = stat(zDir, &statBuf);
		if (rc != 0)
			return(-1);

		mode = statBuf.st_mode & S_IFMT;
		if ( mode != S_IFDIR)
			return(-1);
	
		return(0);
	}

	rc = mkdir(zDir, 0755);
	return(rc);
}

static int mkdirp(char *zDir, int perms)
{
char *pNextLevel;
int rc;
char lBuildDir[512];

	pNextLevel = (char *)strtok(zDir, "/");
	if(pNextLevel == NULL)
		return(-1);

	memset(lBuildDir, 0, sizeof(lBuildDir));
	if(zDir[0] == '/')
        sprintf(lBuildDir, "%s", "/");

	strcat(lBuildDir, pNextLevel);
	for(;;)
	{
		rc = createDirIfNotPresent(lBuildDir);
		if(rc != 0)
		return(-1);

		if ((pNextLevel=strtok(NULL, "/")) == NULL)
		{
			break;
		}
		strcat(lBuildDir, "/");
		strcat(lBuildDir, pNextLevel);
	}

return(0);
}


main (int argc, char *argv[])
{
static	char mod[]="main";
time_t   sec;
char 	header_file[60];
char 	fax_file[200];
char 	fax_phone[40];
char 	orig_filename[128];
char 	send_time[64];
int  	tries;
int  	del_flag;
char 	CDR_data[256];
char 	file_type_str[128];
char 	base[128];
int  	rc;
int  	default_tries;		/* not used */
int  	timeout;		/* not used */
int 	fail_on_busy;		/* update try count if fail on busy ? */	
int 	phone_format;
int 	vendor_rc;		/* when sending fax, vendor's return code */
int	default_retry_interval;
int	default_fail_on_busy;
char	PortNumber[10];
int		yIntFaxFileLen = 0;

	rc = TEL_InitTelecom (argc, argv);
	if (rc != 0 ) 
	{
		sprintf(log_buf, "TEL_InitTelecom failed. rc=%d.",rc);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);
		exit(-1);
	}

	setFaxTextValues();

	TEL_GetGlobalString("$PORT_NAME", PortNumber);
	Port = atoi(PortNumber);

	TEL_GetGlobalString("$APP_TO_APP_INFO", header_file);


	if(header_file[0])
	{
		int		yIntHdrPathLen = 0;

		yIntHdrPathLen = strlen(header_file);

		if(header_file[yIntHdrPathLen -1] == '|')
		{
			header_file[yIntHdrPathLen -1] = 0;
		}
	}

	getFaxConfig(base, &timeout, &default_tries, &default_fail_on_busy, 
			&default_retry_interval); 
 
	/* Set the directory names */
	sprintf(Fax_work_dir, 	"%s/%s/%s", 	base, WORK_DIR, FAX_DIR);
	sprintf(Hdr_work_dir, 	"%s/%s/%s", 	base, WORK_DIR, HDR_DIR);
	sprintf(Fax_err_dir, 	"%s/%s/%s", 	base, ERR_DIR,  FAX_DIR);
	sprintf(Hdr_err_dir, 	"%s/%s/%s", 	base, ERR_DIR,  HDR_DIR);
	sprintf(Fax_sent_dir, 	"%s/%s/%s", 	base, SENT_DIR, FAX_DIR);
	sprintf(Hdr_sent_dir, 	"%s/%s/%s", 	base, SENT_DIR, HDR_DIR);

	rc=get_header_data(header_file,file_type_str, orig_filename, 
			fax_phone, send_time, &tries, &del_flag, 
			&fail_on_busy, &Retry_interval, &phone_format, CDR_data); 

	if ( rc != ELIGIBLE )
	{
		sprintf(log_buf, "Failed to successfully read header data.  "
			"Unable to send fax for header file (%s).", header_file);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);
		dispose_on_failure( header_file);
		rc = TEL_ExitTelecom();
		exit(0);

	}
	sprintf(log_buf, "Successfully retrieved header data from header file (%s); "
			"tries %d, send time (%s), fax phone (%s) phone format (%d)",
			header_file, tries, send_time, fax_phone, phone_format);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);

	if(fax_phone[0] == '\0')
	{
		dispose_on_failure( header_file);
		sprintf(log_buf,"No fax phone # for header file (%s).  "
			"Unable to send fax.", header_file);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);	
		rc = TEL_ExitTelecom();
		exit(0);
	}

	if(fail_on_busy == USE_DEFAULT)
		 fail_on_busy = default_fail_on_busy;
	if(Retry_interval == USE_DEFAULT)
		Retry_interval = default_retry_interval;

	/* Construct name of actual file to fax out. */
	sprintf(fax_file, "%s/%s/%s",
		 Fax_work_dir, header_file, orig_filename); 

	yIntFaxFileLen = strlen(fax_file);

	if(fax_file[yIntFaxFileLen -1] == '|')
	{
		fax_file[yIntFaxFileLen -1] = 0;
	}

	rc = send_fax(	file_type_str, fax_phone, fax_file, send_time,
					CDR_data, tries, fail_on_busy, phone_format, &vendor_rc);
		
	switch(rc)
	{
	case FAX_SUCCESS:
		dispose_on_success(header_file,del_flag);
		break;
	case FAILURE:
		/* If got a busy & not failing on busy, add 1 to tries
		   so when we don't exhaust our tries, & when we 
		   decrement later, we stay at the same try count. */
		if (vendor_rc==51 && fail_on_busy==DONT_FAIL_ON_BUSY) 
			tries++;
		if (tries == 1)
			dispose_on_failure( header_file);
		else
			update_tries(header_file, file_type_str, orig_filename, 
				fax_phone, send_time, tries, del_flag, fail_on_busy,CDR_data);
		break;
	case NO_FAX_FILE:
		dispose_on_failure( header_file);
		break;	
	default:
		sprintf(log_buf,"Unexpected return from send_fax: %d", rc);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);
		break;
	} /* switch */

	exit_program();

} /* main */

int exit_program()
{
	int rc;

	rc = TEL_ExitTelecom();
	exit(0);
}


/*-----------------------------------------------------------------------------
get_header_data()	Read the header file, ignoring the commented lines
			and reading the parameter and CDR data lines.
			The 1st uncommented line should be the parameter line.
			The 2nd uncommented line should be the CDR data line.
	Note: This function also judges whether the header is eligible.
-----------------------------------------------------------------------------*/
int get_header_data(header_file,file_type_str,orig_filename,fax_phone,send_time,tries,
				 del_flag,fail_on_busy,retry_seconds, phone_format, CDR_data)
char 	*header_file;
char 	*file_type_str;
char 	*orig_filename;
char 	*fax_phone;
char 	*send_time;
int  	*tries;
int  	*del_flag;
int  	*fail_on_busy;
int  	*retry_seconds;
int  	*phone_format;
char 	*CDR_data;
{
static	char mod[]="get_header_data";
FILE	*fp1;
int	line;		/* # of the uncommented line we seek */
char	str[256];
char	current_yyyymmddhhmmss[30];
char	cdr_data[256];
long 	sec;
int 	i_del_flag, i;
int	i_tries = 0;
int	i_busy_flg = 0;
int	i_retry_sec = 0;
int	i_phone_format = 78;
char 	hdr_pathname[256];
char 	*base;
char	i_fax_phone[30];
char	i_send_time[30];
char 	time_str[20];	
struct tm	*PT_time;
int		yIntHdrPathLen = 0;


sprintf(hdr_pathname, "%s/%s", Hdr_work_dir, header_file);

yIntHdrPathLen = strlen(hdr_pathname);

if(hdr_pathname[yIntHdrPathLen -1] == '|')
{
		hdr_pathname[yIntHdrPathLen -1] = 0;
}

if((fp1=fopen(hdr_pathname, "r")) == NULL)
	{
	sprintf(log_buf, "Failed to open <%s> to get header data. [%d, %s]",
			hdr_pathname,errno,strerror(errno));
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	return(FAILURE);
	}

line =  1;

while(fgets(str, 256, fp1) != NULL)
{
	if (str[0] == '#') continue;

	str[strlen(str) -1] ='\0';

sprintf(log_buf, "Read line <%s>", str);
fax_log(mod, REPORT_VERBOSE, 0, log_buf);

	switch(line)
	{	
		case 1:
			/* read the parameter line */
			sscanf(str, "%s %s %s %s %d %d %d %d %d", 
				file_type_str, orig_filename, i_fax_phone, 
				i_send_time, &i_tries, &i_del_flag,
				&i_busy_flg, &i_retry_sec, &i_phone_format);
			*del_flag = i_del_flag;
			*tries = i_tries;
			*retry_seconds = i_retry_sec;
			*fail_on_busy = i_busy_flg;
			*phone_format = i_phone_format;
			strcpy(fax_phone, i_fax_phone);	
			strcpy(send_time, i_send_time);	
			line ++;	
			break;
		case 2:
			/* read the CDR data line; chop CR if needed */
			if (str[(int)strlen(str)-1] == '\n')
				str[(int)strlen(str)-1] = '\0';
			strcpy(CDR_data, str);
			line++;
			break;
		default:
			/* found a third or subsequent uncommented line */
			sprintf(log_buf, 
			"Warning: unexpected line found in file %s Line=%s",
							hdr_pathname, str);
			fax_log(mod, REPORT_NORMAL, 0, log_buf); 	
			break;
	} /* switch */
}
fclose(fp1);
	
if(i_tries == 0)
	{
	format_time(current_yyyymmddhhmmss, time_str);
	sprintf(Result_line,"RESULT: %s Number of tries exceeded.", time_str);

	sprintf(log_buf, "%s", Result_line);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	return(TOO_MANY_TRIES);
	}
if((i_del_flag != 1) && (i_del_flag != 2))
	{
	sprintf(log_buf, "Warning: Invalid delete flag value in %s: %d. Using 2.", 
			hdr_pathname, i_del_flag);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	*del_flag = 2;
	}
return(ELIGIBLE);
}

/*----------------------------------------------------------------------------
High level function to peform the dial out and send the fax.
----------------------------------------------------------------------------*/
int send_fax(file_type_str, fax_phone, fax_file, send_time, CDR_data, 
					tries, fail_on_busy, phone_format, vendor_rc)
char 	*file_type_str;
char 	*fax_phone;
char 	*fax_file;
char 	*send_time;
char 	*CDR_data;
int 	tries;
int 	fail_on_busy;
int 	phone_format;
int 	*vendor_rc;
{
static char mod[]="send_fax";
int 	rc, rc2;
int 	channel;			/* channel over which call is made */
char 	result[128];			/* used in Event record */
char 	fax_pathname[256];		/* used in Event record */
time_t 	start_sec;			/* fax start time in seconds */
time_t 	end_sec;			/* fax end time in seconds */
char 	start_yyyymmddhhmmss[30];	/* start date/time sending the fax */
char 	end_yyyymmddhhmmss[30];  	/* end date/time for sending the fax */
int	retcode;
int	dialogic_code;
char	api_that_failed[64];
char 	time1[20];	/* temp string for formatted time */
char 	time2[20];	/* temp string for formatted time */
char 	time3[20];	/* temp string for formatted time */
struct tm	*PT_time;

*vendor_rc = 0;  

if (access(fax_file, F_OK) != 0)
	{
	sprintf(log_buf, "Fax file <%s> doesn't exist.", fax_file);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	return(NO_FAX_FILE);
	}
time(&start_sec);
PT_time=localtime(&start_sec);
strftime(start_yyyymmddhhmmss, sizeof(start_yyyymmddhhmmss),
	"%Y%m%d%H%M%S", PT_time);

rc = dial_fax(file_type_str, fax_file, fax_phone, phone_format,
			&retcode, &dialogic_code, api_that_failed);
*vendor_rc = retcode;	

time(&end_sec);
PT_time=localtime(&end_sec);
strftime(end_yyyymmddhhmmss, sizeof(end_yyyymmddhhmmss),
	"%Y%m%d%H%M%S", PT_time);

strcpy(Current_yyyymmddhhmmss, end_yyyymmddhhmmss);

if (rc == 0) 
	{
	/* Write CEV success record to system log. */
    	strcpy (result, "SUCCESS");
  	rc2 = CDR_CustomEvent (CEV_STRING, fax_phone, 0);
  	rc2 = CDR_CustomEvent (CEV_STRING, result, 0);
  	rc2 = CDR_CustomEvent (CEV_INTEGER, &start_sec, 0);
  	rc2 = CDR_CustomEvent (CEV_INTEGER, &end_sec, 0);
  	rc2 = CDR_CustomEvent (CEV_STRING, fax_file, 0);
  	rc2 = CDR_CustomEvent (CEV_STRING, CDR_data, 1);

	/* Format result line for header file. */
	format_time(Current_yyyymmddhhmmss, 	time1);
	format_time(start_yyyymmddhhmmss, 	time2);
	format_time(end_yyyymmddhhmmss, 	time3);
//	sprintf (Result_line, 
//	"TRY: %s Fax sent to %s on port %d. pid=%d. Start: %s. End: %s.",
//				time1, fax_phone, Port, getpid(), time2, time3);
	return(FAX_SUCCESS);	
	}
	else
	{
	/* Write error message to system log. */
	sprintf( log_buf, "%s failed sending <%s> to <%s>. Port=%d. rc=%d. ret_code=%d.",
		api_that_failed, fax_file, fax_phone, Port, rc, retcode);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	
	/* Write CEV failure record to system log. */
	sprintf (result, "FAILURE(%c,%d,%d)", api_that_failed[4], rc, retcode);
  	rc2 = CDR_CustomEvent (CEV_STRING, fax_phone, 0);
  	rc2 = CDR_CustomEvent (CEV_STRING, result, 0);
  	rc2 = CDR_CustomEvent (CEV_INTEGER, &start_sec, 0);
  	rc2 = CDR_CustomEvent (CEV_INTEGER, &end_sec, 0);
  	rc2 = CDR_CustomEvent (CEV_STRING, fax_file, 0);
  	rc2 = CDR_CustomEvent (CEV_STRING, CDR_data, 1);
	
	/* Format result line for header file. */
	format_time(Current_yyyymmddhhmmss,time1);
//	sprintf(Result_line, 
//		"TRY: %s Fax to <%s> failed on port %d. pid=%d. [%s,%d,%d,%d]", 
//		time1, fax_phone, Port, getpid(), 
//		api_that_failed, rc, retcode, dialogic_code);
	return(FAILURE);	
	}
}

/*-----------------------------------------------------------------------------
dispose_on_success:	Append the Result_line to the header and move it to
			the header sent directory.

			Phrase or File:	If the delete flag is set then delete
			the file containing either a text file or an unloaded
			fax phrase. If the delete flag is not set, move the 
			file to the sent directory.
-----------------------------------------------------------------------------*/
int dispose_on_success(header_file,del_flag)
char 	*header_file;
int	del_flag;
{	
static char mod[]="dispose_on_success";
char 	hdr_pathname[256];
char 	fax_pathname[256];
char 	pathname[256];
char 	command[256];
char	tmpBuffer[256];
int	sent_dir_OK = 1;
int	what_was_faxed;
FILE	*ptr;
char	cmd[256];

sprintf(hdr_pathname, "%s/%s", Hdr_work_dir, header_file);
sprintf(fax_pathname, "%s/%s", Fax_work_dir, header_file);

/* Write the result into the header file */
ptr= fopen(hdr_pathname, "a");
if (ptr == NULL) 
	{
	sprintf(log_buf,
	"Warning: Failed to open %s attempting to append result: %s. errno=%d",
						hdr_pathname, Result_line);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	}	
else
	{	
	fprintf(ptr, "# %s\n", Result_line);
	}
fclose(ptr);

sprintf(tmpBuffer, "%s", Hdr_sent_dir);
/* Make header sent dir, if necessary */
if (access(Hdr_sent_dir, F_OK) != 0)
	{
	sprintf(tmpBuffer, "%s", Hdr_sent_dir);
	if (mkdirp(Hdr_sent_dir, 0755)== -1)
		{
		sprintf(log_buf,"Warning:Failed to make directory %s",Hdr_sent_dir);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);
		/* indicate problem, but don't return */	
		}
	sprintf(Hdr_sent_dir, "%s", tmpBuffer);
	}

sprintf(Hdr_sent_dir, 	"%s", 	tmpBuffer);

/* move the  header file to the header sent directory */
sprintf(pathname, "%s/%s", Hdr_sent_dir, header_file);
if (rename(hdr_pathname, pathname) != 0)
	{
	sprintf(log_buf,"Warning: Failed to move %s to %s error(%d)",
				hdr_pathname,pathname, errno);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	}
if ( del_flag == 1 )
	{
	sprintf(cmd, "rm %s/*", fax_pathname);
	system(cmd);
	if(rmdir(fax_pathname) != 0)
		{
		sprintf(log_buf, "Warning: Unable to delete <%s>. errno=%d.", 
						fax_pathname, errno);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);
		}	
	return(FAX_SUCCESS);	
	} /* end of if /

/* We have sent a file or unloaded phrase, but not deleted it */

sprintf(tmpBuffer, "%s", Fax_sent_dir);
/* Create sent directory, if necessary */
if (access(Fax_sent_dir, F_OK) != 0)
	{
	if(mkdirp(Fax_sent_dir, 0755)== -1)
		{
		sprintf(log_buf,"Failed to make directory <%s>. errno=%d.",
					Fax_sent_dir,errno);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);
		return(FAILURE);
		}
	}

sprintf(Fax_sent_dir, "%s", tmpBuffer);
/* move the fax file  */
sprintf(pathname, "%s/%s", Fax_sent_dir, header_file);
if (rename(fax_pathname, pathname) != 0)
	{
	sprintf(log_buf,"Warning: Failed to move <%s> to <%s>. errno=%d.",
				fax_pathname,pathname, errno);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	return(FAILURE);
	}
return(FAX_SUCCESS);
}

/*-----------------------------------------------------------------------------
dispose_on_failure:	Append the Result_line to the header and move it to
			the header error directory.
			Move the file we tried to fax (containing a text file
			or an unloaded phrase file) to the fax error 
			directory regardless of the value of the delete flag 
			in the header.
-----------------------------------------------------------------------------*/
int dispose_on_failure(header_file)
char *header_file;
{	
static char mod[]="dispose_on_failure";
char 	hdr_pathname[256];
char 	fax_pathname[256];
char 	pathname[256]="";
char 	command[256];
int 	err_dir_OK = 1;
FILE	*ptr;
char	tmpBuffer[256];

sprintf(hdr_pathname, "%s/%s", Hdr_work_dir, header_file);
sprintf(fax_pathname, "%s/%s", Fax_work_dir, header_file);

ptr= fopen(hdr_pathname, "a");
if(ptr == NULL) 
	{
	sprintf(log_buf,
	"Failed to open <%s> while attempting to append result: <%s> errno=%d.",
	hdr_pathname, Result_line, errno);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	return(FAILURE);
	}	

fprintf(ptr, "# %s\n", Result_line);
fclose(ptr);

/* make the header directory if necessary */
if(access(Hdr_err_dir, F_OK) != 0)
	{
	sprintf(tmpBuffer, "%s", Hdr_err_dir);
	if(mkdirp(Hdr_err_dir, 0755)== -1)
		{
		sprintf(log_buf,"Failed to make directory <%s> errno=%d.",
				Hdr_err_dir, errno);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);
		/* indicate a problem, but don't return */ 
		}
	sprintf(Hdr_err_dir, "%s", tmpBuffer);
	}

/* move the  header file to the header error directory */
	sprintf(pathname, "%s/%s", Hdr_err_dir, header_file);

if (rename(hdr_pathname, pathname) != 0)
	{
	sprintf(log_buf,"Failed to move <%s> to <%s> errno=%d.",
		 hdr_pathname, pathname,errno);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	return(FAILURE);
	}


if (access(Fax_err_dir, F_OK) != 0)
	{
	sprintf(tmpBuffer, "%s", Fax_err_dir);
	if(mkdirp(Fax_err_dir, 0755)== -1)
		{
		sprintf(log_buf,"Failed to make directory <%s> errno=%d.",
				Fax_err_dir, errno);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);
		return(FAILURE);
		}
	sprintf(Fax_err_dir, "%s", tmpBuffer);
	}

/* move the fax file to the fax error directory */
sprintf(pathname, "%s/%s", Fax_err_dir, header_file);

if(rename(fax_pathname, pathname) != 0)
	{
	sprintf(log_buf,"Failed to move <%s> to <%s> errno=%d.", 
			fax_pathname, pathname,errno);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	return(FAILURE);
	}
return(FAX_SUCCESS);
}

/*-----------------------------------------------------------------------------
update_tries: 	Decrement the try count and add Retry_interval minutes
			to the send_time then update the header lines.

		Note: 	When header lines are updated, all previous lines are
			retained in the file preceded by a "#". The last two
			(uncommented) lines should be the parameter line and 
			the CDR data line.	
-----------------------------------------------------------------------------*/
update_tries(header_file, file_type_str, orig_filename, fax_phone, send_time, 
		tries,del_flag,failonbusy,CDR_data)
char	*header_file;
char	*file_type_str;
char	*orig_filename;
char	*fax_phone;
char	*send_time;
int	tries;
int	del_flag;
int	failonbusy;
char	*CDR_data;
{
static char mod[]="update_tries";
FILE	*hdr_ptr;
FILE	*tmp_ptr;
FILE	*fp;
char 	hdr_pathname[256];
char 	tmp_pathname[256];
char 	zero_pathname[256];
char 	pathname[256];
char 	line[256];
char 	new_send_yyyymmddhhmmss[30];
time_t	sec;
struct tm	*PT_time;

sprintf(hdr_pathname, "%s/%s", Hdr_work_dir, header_file);
sprintf(tmp_pathname, "%s/.%s.tmp", Hdr_work_dir, header_file);
sprintf(zero_pathname, "%s/0.%s", Hdr_work_dir, header_file);
/* Note that the tmp_pathname is a "dot" file */

hdr_ptr = fopen(hdr_pathname, "r");
if(hdr_ptr == NULL) 
	{
	sprintf(log_buf,
		"Failed to open header file <%s> to update tries & time and "
		"append result. errno=%d. Result: %s", 
					hdr_pathname,errno,Result_line);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	return(FAILURE);
	}

tmp_ptr = fopen(tmp_pathname, "w");
if(tmp_ptr == NULL)
	{
	sprintf(log_buf,
		"Failed to open temporary file <%s> to update tries & time and "
		"append result. errno=%d. Result:%s", 
					tmp_pathname,errno,Result_line);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	fclose(hdr_ptr);
	return(FAILURE);
	}

while(fgets(line, 256, hdr_ptr) != NULL)
	{
	if (line[0] == '#')
		fprintf(tmp_ptr, "%s", line);
	else
		fprintf(tmp_ptr, "# %s", line);
	}

fclose(hdr_ptr);

if ( time(&sec) != -1) 
	{
	sec = sec +  Retry_interval;
	PT_time=localtime(&sec);
	strftime(new_send_yyyymmddhhmmss, sizeof(new_send_yyyymmddhhmmss),
		"%Y%m%d%H%M%S", PT_time);
	}
else
	{
	sprintf(new_send_yyyymmddhhmmss,"%s", send_time);
	sprintf(log_buf, "Failed to get current time; using original send time.");
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	}

tries = tries - 1;
fprintf(tmp_ptr, "# %s\n", Result_line); 
fprintf(tmp_ptr, "%s %s %s %s %d %d %d %d\n",
	file_type_str, orig_filename,fax_phone,
	new_send_yyyymmddhhmmss,tries,del_flag,failonbusy,Retry_interval);
fprintf(tmp_ptr, "%s\n", CDR_data); 
fclose(tmp_ptr);

if (rename(tmp_pathname, hdr_pathname) != 0)
	{
	sprintf(log_buf,
		"Failed to update header file <%s> with new tries %d & new "
		"send time <%s>. Didn't append result: <%s> errno=%d.", 
			hdr_pathname,tries,new_send_yyyymmddhhmmss, Result_line,errno);
	fax_log(mod, REPORT_NORMAL, 0, log_buf);
	return(FAILURE);
	}

if((fp=fopen(zero_pathname, "w")) == NULL)
{
	sprintf(log_buf, "Failed to open <%s> for write. errno=%d.", 
			zero_pathname, errno);
	fax_log(mod,REPORT_NORMAL,TEL_BASE,Msg);
	return TEL_FAILURE;
}
fclose(fp);

return(FAX_SUCCESS);
}

/*-----------------------------------------------------------------------------
This function returns a pointer to a formatted date-time string, given an
unformatted date time string.
Example:
	Input:  9605171530
	Output:	96/05/17 15:30
-----------------------------------------------------------------------------*/
int format_time(yyyymmddhhmmss, s)
char 	*yyyymmddhhmmss;
char 	*s;
{
sprintf(s,"yyyy/mm/dd hh:mm:ss");
s[0] = yyyymmddhhmmss[0];		/* y */
s[1] = yyyymmddhhmmss[1];		/* y */
s[2] = yyyymmddhhmmss[2];		/* y */
s[3] = yyyymmddhhmmss[3];		/* y */
				/* / */	
s[5] = yyyymmddhhmmss[4];		/* m */
s[6] = yyyymmddhhmmss[5];		/* m */
				/* / */	
s[8] = yyyymmddhhmmss[6];		/* d */
s[9] = yyyymmddhhmmss[7];		/* d */
				/*   */	
s[11] = yyyymmddhhmmss[8];		/* h */
s[12]= yyyymmddhhmmss[9];		/* h */
				/* : */	
s[14]= yyyymmddhhmmss[10];		/* m */
s[15] =yyyymmddhhmmss[11];		/* m */
				/* : */	
s[17] =yyyymmddhhmmss[12];	/* s */
s[18] =yyyymmddhhmmss[13];	/* s */

return(0);
}

/*-----------------------------------------------------------------------------
This routine makes the outbound call and actually sends out the fax.
-----------------------------------------------------------------------------*/
int	dial_fax(file_type_str, fax_file, fax_phone, phone_format,
			retcode, dialogic_code, who_failed)
char	*file_type_str;
char	*fax_file;
char	*fax_phone;
int		phone_format;
int	*retcode;
int	*dialogic_code;
char	*who_failed;
{
	static char mod[]="dial_fax";
	int	rc;
	int	ret_code=0;
	int	outbound_code=0;
	int	channel;
	int	file_type;
	time_t   startTime;
	time_t   disconnectTime;
	
	*retcode = 0;

	// rc = TEL_InitiateCall(MAX_RINGS,NONE,fax_phone,&ret_code,&channel);
	rc = TEL_InitiateCall(MAX_RINGS, phone_format, fax_phone, &ret_code, &channel);
	if(rc != 0) 
	{
		strcpy(who_failed,"TEL_InitiateCall");
		*retcode = ret_code;
	//	TEL_GetGlobal("$OUTBOUND_RET_CODE", &outbound_code);
	//	*dialogic_code = outbound_code;
		sprintf(log_buf,
	"TEL_InitiateCall(%d,%d,%s,%d,&channel) failed. rc=%d. ret_code=%d.",
			MAX_RINGS,NONE,fax_phone,ret_code,rc,ret_code);
		fax_log(mod, REPORT_VERBOSE, 0, log_buf);
		return(rc);
	}
	
	
	time(&startTime);

	rc = TEL_SetGlobal("$KILL_APP_GRACE_PERIOD", 10);
	if(rc != 0) 
	{
		sprintf(log_buf, "Failed to set app grace period. rc=%d.",rc);
		fax_log(mod, REPORT_VERBOSE, 0, log_buf);
		/* Don't exit */
	}


	if(!strcmp(file_type_str,"TIF")) 
		file_type=TIF;
	else if(!strcmp(file_type_str,"TEXT")) 
		file_type=TEXT;
	else if(!strcmp(file_type_str,"LIST")) 
		file_type=LIST;

	sprintf(log_buf, "Calling TEL_SendFax for file (%s).", fax_file);
	fax_log(mod, REPORT_VERBOSE, 0, log_buf);

	rc = TEL_SendFax(file_type, fax_file, "");

	if (rc != 0)
	{
		strcpy(who_failed,"TEL_SendFax");
		sprintf(log_buf,"TEL_SendFax(%d,%s) failed. rc=%d.",
			file_type, fax_file, rc);
		fax_log(mod, REPORT_VERBOSE, 0, log_buf);
		if((rc == -3) || (rc == -1))
		{
			time(&disconnectTime);
			if((disconnectTime - startTime) > 60)
			{
				return(0);
			}
		}
		return(rc);
	}
	sprintf(log_buf, "TEL_SendFax(%d, %s) succeeded.  rc=%d",
				file_type, fax_file, rc);
	fax_log(mod, REPORT_VERBOSE, 0, log_buf);


	return(0);
}

/*-----------------------------------------------------------------------------
This routine sets the fax default values, if needed.
-----------------------------------------------------------------------------*/
int	setFaxTextValues()
{
	static char 	mod[]="setFaxTextValues";
	int		rc;
	char		faxDefaultFile[256];

#if 0

	/* First, ensure that the defaults are set. */
	rc=TEL_SetFaxTextDefaults("");

	sprintf(faxDefaultFile, "%s", "FaxServer.defaults");
	if (access(faxDefaultFile, F_OK) != 0)
	{
		/* Nothing to set.  Just return */
		return(0);	
	}

	rc=TEL_SetFaxTextDefaults(faxDefaultFile);
	if (rc != TEL_SUCCESS)
	{
		sprintf(log_buf,
		"Failed to set fax text defaults from <%s>. rc=%d.",
			faxDefaultFile, rc);
		fax_log(mod, REPORT_NORMAL, 0, log_buf);
		rc=TEL_SetFaxTextDefaults("");
	}

#endif

	return(0);
}

//int LogOff() { }

//int NotifyCaller() { }
/*-----------------------------------------------------------------------------
Get the configuration information for outbound faxes. 
-----------------------------------------------------------------------------*/
getFaxConfig(base_fax_dir, timeout, default_tries, fail_on_busy, retry_interval)
char *base_fax_dir;
int *timeout;
int *default_tries;
int *fail_on_busy; 
int *retry_interval;
{
char 	*base;
char 	table_dir[]="Telecom/Tables";
FILE 	*fp;
int 	line_no;	
int 	def_timeout=20;	
int 	def_default_tries=3;
int 	def_fail_on_busy=1;
int 	def_retry_interval=10; /* minutes */
char 	config_file[128]; 
char 	line[256]; 
char 	string[128]; 

/* Set all the defaults */
base 		= getenv("ISPBASE");
*timeout 	= def_timeout;
*default_tries 	= def_default_tries;
*fail_on_busy 	= def_fail_on_busy;
*retry_interval = def_retry_interval;	

sprintf(base_fax_dir,"%s/Fax",base);

sprintf(config_file,"%s/%s/FaxServer.cfg", base, table_dir);

if ((fp = fopen(config_file, "r")) == NULL)
{ /* all defaults are set */
	return;
}

line_no = 1;
while (fgets(line, 256, fp) != NULL)
{
	if ( line[0] == '#') continue;
	switch(line_no)
	{
		case 1: 
			sscanf(line,"%s", base_fax_dir);
			line_no++;
			break; 
		case 2: 	
			sscanf(line,"%s", string);
			*timeout = atoi(string);
			line_no++;
			break;
		case 3: 	
			sscanf(line,"%s", string);
			*default_tries = atoi(string);
			line_no++;
			break;
		case 4: 	
			sscanf(line,"%s", string);
			*fail_on_busy = atoi(string);
			line_no++;
			break;
		case 5: 	
			sscanf(line,"%s", string);
			*retry_interval = atoi(string);
			line_no++;
			break;
	}
}

fclose(fp);	
return;
}
