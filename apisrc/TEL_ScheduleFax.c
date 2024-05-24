/*------------------------------------------------------------------------ 
Program: 	TEL_ScheduleFax 
Purpose: 	Schedules a fax to be sent by the FaxServer application by 
			writing all the necessary information to a header file and 
			moving the appropriate files to the work directory.
Update:		12/10/02 apj Changes necessary for Single DM structure.
Update:		03/21/03 apj isdncfg parameters will be added to phone string 
					     in initiate call.
Update:	04/22/03 mpb Changed mkdirp to mkdir and back to mkdirp. 
Update:	06/02/03 apj if mkdirp fails, it changes the dir buf. So in the
				     failure log message use the original dir buffer.
Update: 08/25/04 djb Changed sys_errlist strerror(errno).
------------------------------------------------------------------------------
	Header file format:
 	line 1: name of the header file
 	line 2: readable description of the actions to be taken
 	line 3: readable description of the actions to be taken (cont'd)
 	line 4: file type: LIST,TIF,TEXT; filename; phone number type: NONE, 
		NAMERICAN, fax number, send time, try count, 
		delete flag (0-NO, 1-YES), fail_on_busy, retry seconds
	line 5: Call event data 
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "arcFAX.h"

#define NAMERISDN	79
static char mod[] = "TEL_ScheduleFax";

/* Function prototypes */
extern get_fax_config(char *, int *, int *, int *, int *);
static int	edit_and_validate_date_time(char *date_time);
static int	validate_date_time(char * date_time);
static int	write_to_file (char *filename, char *string, char *zeroFileName);

#define WORK_DIR	"work"
#define FAX_DIR		"faxfile"
#define HDR_DIR		"header"

#define MAX_LOG_DATA	150	/* max size of CDR logging data */
#define RET_INVALID_PARAMETER	-1
#define DONT_DELETE	"Don't delete copy on success."
#define DO_DELETE	"Delete copy on success."
#define DONT_FAIL_ON_BUSY "Don't fail on busy."
#define FAIL_ON_BUSY	"Fail on busy."

/* Global Variables */
static char	Hdr_filename[256];
static char	Fax_dir_name[256];

int TEL_ScheduleFax(int file_type, char *file_to_fax, int phone_format,
			 char *fax_phone_no, char *delivery_time, int tries,
			 int delete_flag, int fail_on_busy, 
			 int retry_seconds, char *log_data)
{
	char 	apiAndParms[MAX_LENGTH];
	int 	ret;	
	char	date_and_time[50];
	char	tif_text_full_pathname[256];
	char	list_full_pathname[256];
	char 	base[128];
	char   	fax_dir[128];	
	char   	tmpDir[128];	
	char   	hdr_dir[128];
	char	header_lines[512];	
	char	pid_str[10];	
	char 	hdr_full_pathname[200];	/* Full pathname of the header file */
	char 	zero_file[200];	/* Full pathname of the zero file */
	char 	fax_full_dirname[200];/* Full pathname of dir to store fax files */
	char	yyyymmddhhmmss[30]; 
	time_t	sec; 
	struct tm lTM;
	char	disposal_option[50]; 		/* for the header lines */
	char	busy_option[50]; 		/* for the header lines */
	char	first_try_time[50]; 		/* for the header lines */
	int		timeout; 	
	int 	default_tries; 
	int		default_fail_on_busy; 
	int		default_retry_seconds;
	char    phone_str[64];
	char    adjusted_phoneno[64];
	int		adjusted_delete_flag;
	char	file_type_str[64];
	char	*sptr;
	char 	msg_failed_to_create[]="Failed to create directory (%s). errno=%d.";
	char    msg_cant_form_dialstring[]="Cannot form a valid dial string from (%s).";

	sprintf(apiAndParms,"%s(%s,%s,%s,%s,%s,%d,%s,%s,%d,%s)", mod, 
		arc_val_tostr(file_type), file_to_fax, arc_val_tostr(phone_format), fax_phone_no, 
		delivery_time, tries, arc_val_tostr(delete_flag), arc_val_tostr(fail_on_busy), 
		retry_seconds, "log_data");
	ret = apiInit(mod, TEL_SCHEDULEFAX, apiAndParms, 1, 0, 0); 
	if (ret != 0) HANDLE_RETURN(ret);
 
	/* Get default values so bad parameter values can be overridden */
	get_fax_config(base, &timeout, &default_tries, 
		&default_fail_on_busy, &default_retry_seconds);

	switch(file_type)
	{
		case LIST:
			if(verifyListFiles(file_to_fax) != 0)
			{
				/* Error message(s) written in subroutine */
				HANDLE_RETURN(TEL_SOURCE_NONEXISTENT);
			}
			break;
		case TEXT:
		case TIF:
			if(access(file_to_fax, F_OK) !=0) 
			{
				sprintf(Msg,"ERR: File (%s) does not exist.", file_to_fax);
				fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
				HANDLE_RETURN(TEL_SOURCE_NONEXISTENT); 
			}
			break;

		default:
			sprintf(Msg, "ERR: Invalid file_type: %d. Valid values: LIST, TIF or "
				"TEXT", file_type); 
			fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
    		HANDLE_RETURN(-1);
			break;
	}

    switch(phone_format)
    {
        case NONE:
        case NAMERICAN:
        case IP:
        case SIP:
            break;
        default:
    	sprintf(Msg,
			"Invalid phone_format %d. Valid values: NAMERICAN or NONE",
			phone_format);
    	fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
    	HANDLE_RETURN(-1);
		break;
	}

	sprintf(adjusted_phoneno, "%s", fax_phone_no);
	
	time(&sec);
	lTM = *(struct tm *)localtime(&sec);
	strftime(yyyymmddhhmmss, sizeof(yyyymmddhhmmss), "%Y%m%d%H%M%S", &lTM); 

	/* Copy the passed parm to a temp variable, because
	   edit routine may update it. */
	strcpy(date_and_time, delivery_time);
	
	ret = edit_and_validate_date_time (date_and_time);
	if(ret != TEL_SUCCESS)
	{
		/* Note: make sure we put  original input (delivery_time) in the msg. */
		sprintf(Msg,"ERR: Invalid date or time: %s.", delivery_time);
		fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
		HANDLE_RETURN(RET_INVALID_PARAMETER);
	}

	if(retry_seconds == 0)
		retry_seconds = default_retry_seconds;
	else if(retry_seconds < 0)
	{
		sprintf(Msg, "ERR: Invalid value for retry_seconds: %d. Using "
			"default of %d.",retry_seconds,default_retry_seconds);
		fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
		retry_seconds = default_retry_seconds;
	}

	if(fail_on_busy == 0)
		fail_on_busy = default_fail_on_busy;
	else if(fail_on_busy != 1 && fail_on_busy != 2)
	{
		sprintf(Msg, "ERR: Invalid value for fail_on_busy: %d. Valid values: 0,1,2. "
			"Using default of %d.", fail_on_busy, default_fail_on_busy);
		fax_log(mod, REPORT_NORMAL, TEL_BASE,Msg);
		fail_on_busy = default_fail_on_busy;
	}

	if(tries < 0)
	{
		sprintf(Msg,"ERR: Invalid value for tries: %d. Using default of %d.", 
			tries, default_tries);
		tries = default_tries;
		fax_log(mod, REPORT_NORMAL, TEL_BASE,Msg);
	}

	if((delete_flag != 1) && (delete_flag != 2))
	{
		sprintf(Msg,"ERR: Invalid value for delete flag: %d. Valid values: 1, 2. "
			"Using 2 (don't delete).", delete_flag);  
		fax_log(mod, REPORT_NORMAL, TEL_BASE,Msg);
		adjusted_delete_flag=2;
	}
	else
		adjusted_delete_flag=delete_flag;

	/* truncate log data if too long */	
	if(log_data != NULL)
	{
		if(strlen(log_data) > MAX_LOG_DATA) 
		{
			log_data[MAX_LOG_DATA] = '\0';
		}
	}

	/* If tries specified is 0, then use the default number of tries. */
	if(tries == 0) 
		tries = default_tries;

	fflush(stdout);
	sprintf(fax_dir, 	"%s/%s/%s", base, WORK_DIR, FAX_DIR);
	sprintf(hdr_dir, 	"%s/%s/%s", base, WORK_DIR, HDR_DIR);
	sprintf(pid_str, 	"%d",getpid());
	sprintf(Hdr_filename, 	"%s.%s",    yyyymmddhhmmss, pid_str);
	sprintf(Fax_dir_name, 	"%s.%s",    yyyymmddhhmmss, pid_str);

	sprintf(tmpDir, "%s", fax_dir);
	if(access(tmpDir, F_OK) != 0)
	{
		ret = mkdirp(tmpDir, 0755);
		if (ret != 0)
		{
			sprintf(Msg, msg_failed_to_create, fax_dir, errno);
			fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
			HANDLE_RETURN(TEL_FAILURE);
		}
	}

	sprintf(tmpDir, "%s", hdr_dir);
	if(access(tmpDir, F_OK) != 0 )
	{
		ret = mkdirp(tmpDir, 0755);
		if (ret != 0)
		{
			sprintf(Msg, msg_failed_to_create, hdr_dir, errno);
			fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
			HANDLE_RETURN(TEL_FAILURE);
		}
	}

	sprintf(hdr_full_pathname,"%s/%s", hdr_dir, Hdr_filename);
	sprintf(zero_file,"%s/0.%s", hdr_dir, Hdr_filename);
	sprintf(fax_full_dirname,"%s/%s", fax_dir, Fax_dir_name);

	sprintf(tmpDir, "%s", fax_full_dirname);
	ret = mkdirp(tmpDir, 0755);
	if (ret != 0)
	{
		sprintf(Msg, msg_failed_to_create, fax_full_dirname, errno);
		fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
		HANDLE_RETURN(TEL_FAILURE);
	}

	/* Set header comment strings */
	strcpy(disposal_option, DONT_DELETE);	

	if(adjusted_delete_flag == 1) 
		strcpy(disposal_option, DO_DELETE);	

	strcpy(busy_option, DONT_FAIL_ON_BUSY);	
	
	if(fail_on_busy == 1) 
		strcpy(busy_option, FAIL_ON_BUSY);	
	
	if(strcmp(delivery_time,"0") == 0)
		strcpy(first_try_time, "immediately");
	else
		format_time(date_and_time, first_try_time);

	switch(file_type)
	{
		case LIST:
			CreateNewList(file_to_fax,fax_full_dirname);

			sprintf(header_lines, "# %s\n# Send file %s to %s. 1st try: %s. "
				"Tries: %d.\n# %s %s Retry after %d seconds.\nLIST %s %s %s %d "
				"%d %d %d %d\n%s\n", hdr_full_pathname,	
				file_to_fax, adjusted_phoneno, first_try_time, tries, 
				disposal_option, busy_option, retry_seconds,
				file_to_fax, adjusted_phoneno, date_and_time, tries, 
				adjusted_delete_flag, fail_on_busy, retry_seconds, phone_format, log_data);

			ret = write_to_file(hdr_full_pathname, header_lines, zero_file);
			if(ret != TEL_SUCCESS)
			{
				unlink(hdr_full_pathname); /* just in case */
				unlink(fax_full_dirname);
				HANDLE_RETURN(TEL_FAILURE);
			}
			else
			{
				sleep(1); 	/* See note in header */	
				HANDLE_RETURN(TEL_SUCCESS);
			}
			break;

		case TEXT:
		case TIF:
			if(file_type == TIF)
				strcpy(file_type_str, "TIF");
			else
				strcpy(file_type_str, "TEXT");

			sptr = strrchr(file_to_fax, '/');
			if(sptr == NULL)
			{
				sptr = file_to_fax;		//DDN: Added 03172010 MR 2924
				sprintf(tif_text_full_pathname, "%s/%s", fax_full_dirname,
					file_to_fax);
			}
			else
			{
				sptr++;
				sprintf(tif_text_full_pathname,"%s/%s", fax_full_dirname, sptr);
			}

			copyFile(file_to_fax, tif_text_full_pathname); 

#if 0

			sprintf(header_lines, "# %s\n# Send file %s to %s. 1st try: %s. "
				"Tries: %d.\n# %s %s Retry after %d seconds.\n%s %s %s %s %d "
				"%d %d %d\n%s\n", hdr_full_pathname, file_to_fax, 
				adjusted_phoneno, first_try_time, tries, disposal_option, 
				busy_option, retry_seconds, file_type_str, file_to_fax, 
				adjusted_phoneno, date_and_time, tries, adjusted_delete_flag,
				fail_on_busy, retry_seconds, log_data);
#endif
			
			//DDN: Changed 03172010 MR 2924
			sprintf(header_lines, "# %s\n# Send file %s to %s. 1st try: %s. "
				"Tries: %d.\n# %s %s Retry after %d seconds.\n%s %s %s %s %d "
				"%d %d %d %d %d\n%s\n", hdr_full_pathname, sptr,
				adjusted_phoneno, first_try_time, tries, disposal_option, 
				busy_option, retry_seconds, file_type_str, sptr, 
				adjusted_phoneno, date_and_time, tries, adjusted_delete_flag,
				fail_on_busy, retry_seconds, phone_format, log_data);

			ret = write_to_file(hdr_full_pathname, header_lines, zero_file);
			if(ret != TEL_SUCCESS)
			{
				unlink(hdr_full_pathname); /* just in case */
				unlink(fax_full_dirname);
				HANDLE_RETURN(TEL_FAILURE);
			}
			else
			{
				sleep(1);
				return(TEL_SUCCESS);
			}
			break;
	} /* end of switch on file_type */
	return(TEL_SUCCESS);
}

/*-----------------------------------------------------------------------------
	Edit an input string to the form yymmddhhmmss.
	Then validate the date.	
	Valid input formats:
	Explicit date:	yymmddhhmmss	yymmddhhmm	yymmddhh
	Implicit date:	hhmmss 		hhmm 		hh
------------------------------------------------------------------------------*/
static int edit_and_validate_date_time(char *date_time)
{
	int len, ret;
	char temp[128];
	char yyyymmddhhmmss[20];
	char yyyymmdd[20];
	char cc[20];
	time_t	sec;
	struct tm lTM;
	
	time(&sec);
	lTM = *(struct tm *)localtime(&sec);
	strftime(yyyymmddhhmmss, sizeof(yyyymmddhhmmss), "%Y%m%d%H%M%S", &lTM);

	strcpy(yyyymmdd,yyyymmddhhmmss);
	yyyymmdd[8]='\0';

	strcpy(cc,yyyymmddhhmmss);
	cc[2]='\0';

	len = strlen(date_time);	
	switch(len)
	{
		case 1: /* 0 - immediately */
			sprintf(temp,"%s", date_time);
			break;

		case 2:		/* hh */
			sprintf(temp,"%s%s0000",yyyymmdd, date_time);
			break;

		case 4:		/* hhmm */
			sprintf(temp, "%s%s00", yyyymmdd, date_time);
			break;

		case 6:		/* hhmmss */
			sprintf(temp, "%s%s", 	yyyymmdd, date_time);
			break;

		case 12:	/* yymmddhhmmss */
			sprintf(temp,"%s%s",cc, date_time);
			break;

		case 14:/* yyyymmddhhmmss */
			sprintf(temp,"%s", date_time);
			break;

		default:	
			return(TEL_FAILURE);
	}

	/* If she passed "0", don't bother to check the date */
	if(strcmp(temp,"0") == 0) 
		return(TEL_SUCCESS);
	
	/* If valid date/time, update the passed date with the edited date. */
	ret = validate_date_time (temp);
	if(ret == TEL_SUCCESS) 
		strcpy(date_time, temp);
	return(ret);
}

/*-----------------------------------------------------------------------------
	Validate a date and time string.
	The input can be YYMMDDHHMMSS or YYYYMMDDHHSS.
-----------------------------------------------------------------------------*/
static int	validate_date_time (char *date_time)
{
	char date_str[128];
	char time_str[128];	
	char yyyymmddhhmmss[30];
	char buf[30];	
	int length;
	int	hour;	
	int	min;
	int	sec;
	struct tm lTM;
	time_t	tics;

	int	year;
	int	month;
	int	day;
	int max_day;
	int	j;

	for(j=0; j<(int)strlen(date_time); j++)
	{
		if(!isdigit(date_time[j]))
			return(TEL_FAILURE);
	}

	length = strlen(date_time);
	switch(length)
	{
		case 12:
			/* Get the time string and date string separated */
			strcpy(time_str, &(date_time[6]));
			strcpy(date_str, date_time);
			date_str[6] = '\0';
			break;

		case 14:
			/* get the time string and date string separated */
			strcpy(time_str, &(date_time[8]));
			strcpy(date_str, date_time);
			date_str[8] = '\0';
			break;

		default:
			return(TEL_FAILURE);
	}

	length = strlen(time_str);
	if(length != 6) 
		return(TEL_FAILURE);

	strcpy(buf, time_str);	
	sec = atoi(&(buf[4])); 
	buf[4] = '\0';
	min = atoi (&(buf[2])); 
	buf[2] = '\0';
	hour = atoi(buf);

	if((sec < 0  || sec > 59) ||
		(min < 0  || min > 59) ||
		(hour < 0  || hour> 23)) 
	{
		return(TEL_FAILURE);
	}

	buf[0] = '\0';
	strcpy(buf, date_str);
	length = strlen(buf);
	switch(length)
	{
		case 8:
			day = atoi(&(buf[6])); 
			buf[6] = '\0';
			month = atoi(&(buf[4])); 
			buf[4] = '\0';
			year = atoi(buf); 	/* 4 digit year */
			break;	

		case 6:
			day = atoi(&(buf[4])); 
			buf[4] = '\0';
			month = atoi(&(buf[2])); 
			buf[2] = '\0';
           	time(&tics);
			lTM = *(struct tm *)localtime(&tics);
			strftime(yyyymmddhhmmss, sizeof(yyyymmddhhmmss), 
				"%Y%m%d%H%M%S", &lTM);
			yyyymmddhhmmss[4]='\0';
			year = atoi(yyyymmddhhmmss) + atoi(buf);
			break;

		default:
			return(TEL_FAILURE);
	}

	switch(month)
	{
		case 4:
		case 6:
		case 9:
		case 11:
			if(day < 1 || day > 30) 
				return(TEL_FAILURE);
			break;

		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			if(day < 1 || day > 31) 
				return(TEL_FAILURE);
			break;

		case 2:
			if((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
				max_day = 29;
			else
				max_day = 28;

			if(day < 1 || day > max_day) 
				return(TEL_FAILURE);
			break;

		default:
			return(TEL_FAILURE);
	}

	return(TEL_SUCCESS);
}

/*----------------------------------------------------------------------------
Append a string to a file.
----------------------------------------------------------------------------*/
static int	write_to_file (char *filename, char *string, char *zeroFileName)
{
	FILE	*fp;

	if((fp=fopen(filename, "w")) == NULL)
	{
		sprintf(Msg, "ERR: Failed to open (%s) for write. errno=%d.", 
			filename, errno);
		fax_log(mod,REPORT_NORMAL,TEL_BASE,Msg);
		return TEL_FAILURE;
	}
	
	fprintf(fp, "%s", string);
	fclose(fp);

	if((fp=fopen(zeroFileName, "w")) == NULL)
	{
		sprintf(Msg, "ERR: Failed to open (%s) for write. errno=%d.", 
			zeroFileName, errno);
		fax_log(mod,REPORT_NORMAL,TEL_BASE,Msg);
		return TEL_FAILURE;
	}
	
	fclose(fp);
	return(TEL_SUCCESS);
}

/*-----------------------------------------------------------------------------
This function returns a formatted date-time string, given an unformatted date 
time string.
Example:
        Input:  199805171530
        Output: 1998/05/17 15:30
-----------------------------------------------------------------------------*/
int format_time(yyyymmddhhmmss,s)
char *yyyymmddhhmmss;
char *s;
{
	sprintf(s,"yyyy/mm/dd hh:mm:ss");
	s[0] = yyyymmddhhmmss[0];         /* y */
	s[1] = yyyymmddhhmmss[1];         /* y */
	s[2] = yyyymmddhhmmss[2];         /* y */
	s[3] = yyyymmddhhmmss[3];         /* y */
                                      /* / */
	s[5] = yyyymmddhhmmss[4];         /* m */
	s[6] = yyyymmddhhmmss[5];         /* m */
                                      /* / */
	s[8] = yyyymmddhhmmss[6];         /* d */
	s[9] = yyyymmddhhmmss[7];         /* d */
                                      /*   */
	s[11] = yyyymmddhhmmss[8];        /* h */
	s[12]= yyyymmddhhmmss[9];         /* h */
                                      /* : */
	s[14]= yyyymmddhhmmss[10];        /* m */
	s[15] =yyyymmddhhmmss[11];        /* m */
                                      /* : */
	s[17] =yyyymmddhhmmss[12];        /* s */
	s[18] =yyyymmddhhmmss[13];        /* s */

	return(0);
}

/*----------------------------------------------------------------------------
Copy the first file to the second file.
----------------------------------------------------------------------------*/
int copyFile(char *file1, char *file2)
{
	int	rfd, wfd, nread, nwrite, n;
	char	buf[512];
	char 	msg_cant_open_file[]="Failed to open file (%s). errno=%d. [%s]";

	if((rfd = open(file1,O_RDONLY,  0)) == -1)
	{
		sprintf(Msg, msg_cant_open_file,file1,errno,strerror(errno));
		fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
		return(-1);
	}

	if((wfd = open(file2,O_RDWR | O_CREAT, 0666)) == -1)
	{
		sprintf(Msg, msg_cant_open_file,file2,errno,strerror(errno));
		fax_log(mod, REPORT_NORMAL, TEL_BASE, Msg);
		close(rfd);
		return(-1);
	}

	while((nread = read(rfd, buf, sizeof(buf))) != 0)
	{
		if(nread == -1)
		{
			sprintf(Msg,
			"ERR: Failed to copy file (%s) to (%s). errno=%d. [%s]", 
				file1,  file2, errno, strerror(errno));
			fax_log(mod, REPORT_NORMAL, TEL_BASE,Msg);
			close(rfd);
			close(wfd);
			return(-1);
		}
		nwrite = 0;
		do{
			if((n = write(wfd, &buf[nwrite], nread - nwrite)) == -1)
			{
				sprintf(Msg,
				 "ERR: Failed to write to file (%s). errno=%d.", 
					file2, errno);
				fax_log(mod, REPORT_NORMAL, 
					TEL_BASE,Msg);
				close(rfd);
				close(wfd);
				return(-1);
			}
		nwrite += n;
		}while (nwrite < nread);
		
	}
	close(rfd);
	close(wfd);

	return(0);
}

/*-----------------------------------------------------------------------------
Create a new list file in the specified directory by:
	1) Open original list file.
	2) Open new list file.
	3) For each line in original list file:
		a) Copy the fax file specified there to the destination dir.
		b) Write the new fax file's full pathname in the new list file.
	4) Close both files.
-----------------------------------------------------------------------------*/
int CreateNewList(char *orig_list, char *directory)
{
	FILE	*fp_orig, *fp_new;
	char buf[512];
	char new_list[256];	/* New list file in destination dir. */
	char new_faxfile[256]; 	/* Name of fax file in new list file */

	char faxfile[256];	/* Fax file in original list file */
	char faxtype[32];	/* Fax type in original list file */
	char startPage[32];	/* Starting page of a tiff file */
	char numberOfPages[32];	/* Starting page of a tiff file */
	char eject[32];		/* Eject indicator in original list file */
	char comment[256];	/* Comment in original list file */
	char *ptr;			

	fp_orig = fopen(orig_list, "r");
	if(fp_orig == NULL)
	{
		sprintf(Msg, "ERR: Failed to open list file (%s). [%d, %s]", 
			orig_list, errno, strerror(errno));
		fax_log(mod, REPORT_NORMAL, TEL_BASE,Msg);
		return(-1);
	}

	/* Determine name of new list file, in destination directory */	
	ptr = strrchr(orig_list,'/');
	if(ptr != NULL)
	{
		ptr++;
		sprintf(new_list, "%s/%s",directory, ptr);
	}
	else
		sprintf(new_list, "%s/%s", directory, orig_list);
	
	fp_new = fopen(new_list, "a+");
	if (fp_new == NULL)
	{
		sprintf(Msg,"ERR: Failed to open new list file %s. errno=%d.", 
			new_list);
		fax_log(mod, REPORT_NORMAL,TEL_BASE, Msg);
		return(-1);
	}

	/* Read from original, copy the file, write line to new list */
	while(fgets(buf, BUFSIZ, fp_orig) != NULL)
	{
		buf[strlen(buf) -1] = '\0';
		
		/* Just copy over any lines that are comments */
		if((buf[0] == '#') || (strcmp(buf,"") == 0))
		{
			fprintf(fp_new,"%s\n", buf);
			fflush(fp_new);
			continue;
		}

		sscanf(buf,"%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^%]|",
				faxfile,faxtype,startPage,numberOfPages,
				eject,comment);
		faxfile[strlen(faxfile)] = '\0';
		faxtype[strlen(faxtype)] = '\0';
		startPage[strlen(startPage)] = '\0';
		numberOfPages[strlen(numberOfPages)] = '\0';
		eject[strlen(eject)] = '\0';
		comment[strlen(comment)] = '\0';

		ptr = strrchr(faxfile,'/');
		if(ptr != NULL)
		{
			ptr++;
			sprintf(new_faxfile, "%s/%s",directory, ptr);
		}
		else
			sprintf(new_faxfile, "%s/%s", directory, faxfile);
	
		copyFile(faxfile, new_faxfile);

		fprintf(fp_new, "%s|%s|%s|%s|%s|%s|\n",
		new_faxfile,faxtype,startPage,numberOfPages,eject,comment);
		fflush(fp_new);
	}

	fclose(fp_orig);
	fclose(fp_new);

	return(0);
}

/*-----------------------------------------------------------------------------
Verify that that list file and all the files in it actually exist.
-----------------------------------------------------------------------------*/ 
int verifyListFiles(char *listFile)
{
	int	errors=0;
	FILE *fp;
	char buf[512];
	char faxfile[256];
	char faxtype[32];
	char eject[32];
	char startPage[32];
	char numberOfPages[32];
	char comment[128];

	fp = fopen(listFile, "r");
	if(fp == NULL)
	{
		sprintf(Msg, "ERR: Failed to open list file %s. [%d, %s]", 
			listFile, errno, strerror(errno));
		fax_log(mod,REPORT_NORMAL,TEL_BASE,Msg);
		return(-1);
	}

	while(fgets(buf, BUFSIZ, fp) != NULL)
	{
		buf[strlen(buf) -1] = '\0';
		if (buf[0] == '#') continue;
		if (strcmp(buf,"") == 0) continue;
		sscanf(buf,"%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]",
				faxfile,faxtype,
				startPage, numberOfPages, eject,comment);
		faxfile[strlen(faxfile)] = '\0';

		if(access(faxfile, F_OK) !=0) 
		{
			sprintf(Msg,"ERR: File %s in list file %s does not exist.", 
					faxfile, listFile);
			fax_log(mod, REPORT_NORMAL,
					TEL_BASE,Msg);
			errors++;
		}
	}

	fclose(fp);
	if(errors == 0) 
		return(0);
	return(-1);
}
