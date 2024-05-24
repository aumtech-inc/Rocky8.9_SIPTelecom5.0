/*-----------------------------------------------------------------------------
Program:	textMonitor
Purpose: 	Built as an aid to capture the tel monitor ports in to a file
			to determine if each assigned port has an a-leg that is still 
			up.
Date:		11-10-2005 djb	Created this from isp_monitor.c
-----------------------------------------------------------------------------*/
#include "BNS.h"
#include "resp.h"
#include "ISPSYS.h"
#include "shm_def.h"
#include "monitor.h"
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <curses.h>
#include <ResStat.h>
#ifdef ISP_LOG
#include <ISPCommon.h>
#endif // ISP_LOG

#define CLEAR		1
#define NO_CLEAR	0

#define GET_STATUS	1	/* Used with monitor_status routine */
#define SET_STATUS	2	/* Used with monitor_status routine */

#define MORE		"---More---"

#define COL_PER_PAGE	78
#define LINES_PER_PAGE	18		/* Number of lines on screen */
#define MAX_LEN         256		/* Maximum length of line in file */

#define UNMONITOR	1	
#define	MONITOR		2

#define NOT_RUNNING_MSG_FORMAT  "%s Services not running."

#ifdef ISP_LOG
extern int LogARCMsg(char *parmModule, int parmMode, char *parmResourceName,
    char *parmPT_Object, char *parmApplName, int parmMsgId,  char *parmMsg);
static char	gMsg[512];
#endif // ISP_LOG

extern 	int write_fld();
extern 	int read_fld();
extern 	int  errno;

int 	GV_AppPid = 0;
static int get_service_info(char *prog_name, int *service, key_t *shm_key, long *que_no, char *service_name, char *title);
static 	int get_monitor_directory(char *dir);
static	void display_string_centered();
static	void ref();
static	void exit_program();
static	int  main_menu();		
static	void clear_lines();
static	void display_current_time();
static	void display_results();	
static	void display_message_q_status();
static	void display_message_q_info();
static	void display_cur_page();
static	void display_page_heading();
static	int  find_tran_proc_pid();	
static	int  select_process();
static	void calculate_file_size();
static	void get_shmem();
static	void display_last_page();
static	void display_options_line();
static	void DisplayResults(char *);
static	void display_resource_page();


extern int load_arc_custom_api(char *err_msg);
extern int read_apiName(int api_enum, char *apiName);
extern int Status_Resources(char *ResFile, char *logMsg);
static int recv_msg(int	q_id, Msg *message, long pid);
static void view_a_monitored_app(int sr_no, int app_pid, char *monitor_file);
static void display_resource_page(char *parmResFile);
static void display_page_heading(int selected_app, int page_no, int app_pid);
static void display_options_line();
static int get_monitor_directory(char *dir);
static int get_service_info(char *prog_name, int *service, key_t *shm_key, long *que_no, char *service_name, char *title);
static int monitor_apps(int arc_service, char *title);
static int display_monitor_app_header(char *title);
static void clear_lines(int i, int j);
static void display_current_time();
static void display_string_centered(char *str);
static int display(int row, int col, int option, char *data);
static int display_resource_status();
static int display_resource_page_heading(char *title, int page_no, char *file);
static int my_debug(char *s);
static int monitor_status(int action, int selection, int *status);
static int formatOutputForResourceMonitor(char *parmUnformattedFile, char *parmFormattedFile);
;








static	int	offset;
static	char	*Status[] = {"Init", /* 0 */
			 "Idle", /* 1 */
			 "Busy", /* 2 */
			 "Kill", /* 3 */
                         "Mont", /* 4 */
                         "Umon", /* 5 */
			 "Off ", /* 6 */
			 "POff", /* 7 */
			 "Susp", /* 8 */
			 "Cot", /* 9 */
			 "ROff"}; /* 10 */

static	key_t	Shm_key;
static	long	Que_no;
static	int	main_menu_choice;	
static	int	cur_pg_no = 1;	/* Page that is currently displayed */
static	int	max_pg = 1;	/* Total Pages to be displayed */
static	int	no_reco;	/* # of records in the shared memory table */
static	int     cur_file_pg = 1; /* pg is currently displayed from log file */
static	int     Mem_ok;         /* Successful access to shared memory ?*/
static	int	tot_file_lines=0;/* total number of lines in monitor file */
static	int     tot_file_pg=1;	/* total pages in the monitor tran_proc file */
static	int	port_no;	/* port number assign to application */
static	int	max_len_line=80; /* maxium length of line on current screen */
static	int	cur_col_pos=0;  /* current column position for display page */
static	char	Monitor_dir[512];
static	char	menu_screen_title[80];
static  char column_headings[128];
static  char Host_name[128];
static  char ArcServiceName[64];
static  char ModuleName[]="isp_monitor";
static 	char ResourceMgrProblemMsg[]="ResourceMgr may not be running.";
static	int	MsgLine;	/* Line # on screen for messages */

void exit_program(exit_code)
int exit_code;
{
	endwin();
	exit(exit_code);	
}


int main(int argc, char *argv[])
{
	int 	rc;
	char 	subtitle[80];
	char 	err_msg[256];
	char 	*ptr;
	static char	mod[]="main";

#ifdef ISP_LOG
	sprintf(gMsg, "[%d] %s", __LINE__, "Starting");
	LogARCMsg(mod, REPORT_VERBOSE, "3", "MON", "textMonitor", 712, gMsg);
#endif // ISP_LOG

	rc = get_monitor_directory(Monitor_dir);
	if (rc != 0) exit;

	rc = get_service_info(argv[0], &object, &Shm_key, &Que_no, 
				ArcServiceName, menu_screen_title);
	if (rc  != 0) exit;

	memset(Host_name, 0, sizeof(Host_name));
	rc = gethostname(Host_name, 100);
	ptr=strstr(Host_name,".");
	if (ptr != NULL) *ptr='\0';

	rc = load_arc_custom_api(err_msg);
	if (rc != 0)
	{
		printf("%s: %s.\n", err_msg, ModuleName);
		sleep(3);
	}	

	monitor_apps(object, subtitle); 

	exit_program(0);
}

/*-----------------------------------------------------------------------------
 This routime displays the main menu, gets the user's choice and returns it. 
-----------------------------------------------------------------------------*/
int main_menu(title, subtitle)			
char *title;
char *subtitle;
{
int	option;
char 	buffer[128];

	(void)clear();
	sprintf(buffer, "%s (%s)", title, Host_name);
	display_string_centered(buffer);
	
	display(8 , 23,CLEAR,"1. Monitor Applications");
	display(10, 23,CLEAR,"2. View Network Services Message Queue Status");
	display(12, 23,CLEAR,"3. View Resource Status");
	display(14, 23,CLEAR,"4. Exit");
	display(16, 28,CLEAR,"Choice");

	while ( 1 )
	{
		move(16,35);
		refresh();
	
		option = getch();
/*
		if (option == ERR) exit_program(1);
*/
		switch(option)
		{ 
		case '1':
			sprintf(subtitle,"Monitor Applications (%s)",Host_name);
			addch(option);
			refresh();
			return((int)(option-'0'));
			break;
		case '2':
			strcpy(subtitle,"View Network Services Message Queue Status");
			addch(option);
			refresh();
			return((int)(option-'0'));
			break;
		case '3':
			strcpy(subtitle,"View Resource Status");
			addch(option);
			refresh();
			return((int)(option-'0'));
			break;
		case '4':
			strcpy(subtitle,"");
			addch(option);
			refresh();
			return((int)(option-'0'));
			break;
		case 'Q':
		case 'q':
			(void)addch(option);
			(void)refresh();
			return(4);
		default:
			break;
		} /* switch */
	} /* while */
}

/*-----------------------------------------------------------------------------
This routine displays the status of the processes
-----------------------------------------------------------------------------*/
void display_results()	
{
register int i, j;
int	no_lines, api_enum;		/* # LINES displayed on the screen */
char	Host_name[20];
char	Emu_num[20];
char	Env[20], api_name[50];
char	Log_tbl[51];
char	Tabl_name[100], pgm_name[100], *pgm_ptr;
char	Proc_no[20];		/* process number as in the schd_tabl */
char	Proc_stat[20];		/* status as in shared memory */
char	Proc_id[20];		/* process id as in shared memory */
char	Tran_type[20];		/* transaction type as in schd_tabl */
char	Sig[3];
char	*ptr;
char	apiName[30];

	ptr = 0;
	get_shmem();
	if(Mem_ok)
	{
		ptr = tran_tabl_ptr;
		no_reco = (int)strlen(tran_tabl_ptr)/SHMEM_REC_LENGTH;
		max_pg = (no_reco - 1)/(LINES-8) + 1;
	}
	
	display(3,1,CLEAR,column_headings);

if (ptr)
{
	offset = (cur_pg_no - 1) * (LINES - 8);
		no_lines = no_reco;

	ptr += (SHMEM_REC_LENGTH*offset);	
	/* adjust the pointer to the offset */

	for(i=offset+1, j=5; i<= no_lines; i++, j++)
	{
		(void)sscanf(ptr, "%s%s%s%s%s%s%s%s%s%s\n", 
			Host_name, Emu_num, Proc_no, Env, Tran_type, Log_tbl, 
			Tabl_name, Proc_stat, Proc_id, Sig);

	memset (api_name, 0, sizeof(api_name));
	(void)sscanf(Emu_num, "%d", &api_enum);

	switch(api_enum)
	{
	case 0:
		sprintf(api_name, "%s", "N/A                   ");
		break;
	case CUSTOM_API:
		sprintf(api_name, "%s", "CUSTOM");
		break;
	default:
		memset(apiName, 0, sizeof(apiName));
		read_apiName(api_enum, apiName);
		sprintf(api_name, "%s        ", apiName);
		break;
	}
	
	/* Get name of program to display */	
	memset (pgm_name, 0, sizeof(pgm_name));
	if (strcmp(Tabl_name, "N/A") != 0)
	{
		if ((pgm_ptr = strrchr(Tabl_name, '/')) != NULL)
		{
			pgm_ptr ++;
			sprintf(pgm_name, "%s", pgm_ptr);
		}
		else
			sprintf(pgm_name, "%s", Tabl_name);
		}
	else
		sprintf(pgm_name, "%s", Tabl_name);

	printf("%-6d%-19s %-10s %-8s %-7s  %-25s\n",
			i, pgm_name, Proc_no, Status[atoi(Proc_stat)], Proc_id, api_name);
#if 0
	printf("%-6d", i);
	printf("%.19s", pgm_name);
	printf("%.10s", Proc_no);
	printf("%.8s", Status[atoi(Proc_stat)]);
	printf("%.7s", Proc_id);
	printf("%.25s\n", api_name);
#endif
#if 0
	move(j,0);
	clrtoeol();
	mvprintw(j,1, "%-6d", i);
	mvprintw(j,7, "%.19s", pgm_name);
	mvprintw(j,27, "%.10s", Proc_no);
	mvprintw(j,38, "%.8s", Status[atoi(Proc_stat)]);
	mvprintw(j,47, "%.7s", Proc_id);
	mvprintw(j,56, "%.25s", api_name);
#endif

	ptr += SHMEM_REC_LENGTH;    /* move the pointer to record end */
	} /* for */

	// clear_lines(j, 5+(LINES-8));
} /* if (ptr) */
else
{
	printf("No ptr\n");
}

	if(Mem_ok) (void)shmdt(tran_tabl_ptr);

//	display_current_time();
//	(void)refresh();
	return;
} /* of display_results() */

/*----------------------------------------------------------------------------
 This routine displays the status of the transaction & communication
 message queues
----------------------------------------------------------------------------*/
void display_message_q_status(title)
char *title;
{
	int ch;

	clear();
	display_string_centered(title);
	display_message_q_info();
	mvaddstr(LINES-4, 10, "Press Enter ");
	move(LINES-4, 22);

	for (;;)
	{
		timeout(2000);
		ch = getch();

		switch(ch)
		{
		case '\r':
			(void)clear();
			return;
		case ERR:	
			display_message_q_info();
			break;
		default:
			break;
		}
	} /* for */
}

static void display_message_q_info()
{
	struct	msqid_ds	buf1;
	char buf[100];

	if((resp_mqid = msgget(Que_no, PERMS)) < 0)
	{
		move(10,6);
		sprintf(buf,"Error accessing Network Services message queue. errno=%d. key=%d.", errno, Que_no); 
		addstr(buf); 
		if(errno == 2)
		{
			move(12,21);
			sprintf(buf,"Network Services may not be running.");
			addstr(buf);
		}
	}
	else
	if(msgctl(resp_mqid, IPC_STAT, &buf1) < 0)
	{
			move(10,6);
			sprintf(buf,"Error accessing message queue statistics. errno=%d. queue id=%d.", errno, resp_mqid); 
			addstr(buf);
	}
	else
	{
		move(12,12);
		clrtoeol();
		printw("Number of messages in queue: %d.", buf1.msg_qnum);
	}

	display_current_time();
	return;
}

/*-----------------------------------------------------------------------------
This routine display the transaction process log file. 
-----------------------------------------------------------------------------*/
static void view_a_monitored_app(int sr_no, int app_pid, char *monitor_file)
{
	int ch, prev_ch;


	cur_file_pg=1;
	(void)clear();
	display_page_heading(sr_no, cur_file_pg, app_pid);
	display_cur_page(monitor_file);		
	display_options_line();	
	(void)refresh();

	while ( 1 )
	{
	timeout(1000);
	ch = getch();

	switch(ch)
		{
		case 'q':		/* Quit from the option */
		case 'Q':
			clear();
			return;
		case 'N':		/* Page Down */
		case 'n': 
			(void)mvprintw(LINES-1, 70, "%s", "         ");
			if(cur_file_pg < tot_file_pg)
				{
				++cur_file_pg;
				display_page_heading(sr_no, cur_file_pg, app_pid);
				display_cur_page(monitor_file);			
				display(LINES-2,1,CLEAR,MORE);
				} 
			else
				{
				display(LINES-2,1,CLEAR,"");
				}
			refresh();
			prev_ch = 'N';
			break;
		case 'P':		/* Page Up */
		case 'p':
			(void)mvprintw(LINES-1, 70, "%s", "         ");
			if(cur_file_pg > 1)	
				{
			    	cur_file_pg--;		/* previous page */
				display_page_heading(sr_no, cur_file_pg, app_pid);
				display_cur_page(monitor_file);	
				display(LINES-2,1,CLEAR,MORE);
                               	}
			else
				{
    				(void)move(LINES-2,1);
			        (void)clrtoeol();
/*				(void)mvprintw(LINES-2,1,"%s","--beginning of file--"); */
				}
			(void)refresh();
			prev_ch = 'P';
			break;
		case 'S':		/* scroll */
		case 's':
			display_page_heading(sr_no, cur_file_pg, app_pid);	
			display_last_page(monitor_file);
			(void)refresh();		
			prev_ch = 'S';
			break;
		case ERR:
			display_page_heading(sr_no, cur_file_pg, app_pid);
			if (prev_ch == 'S')
				{
				display_last_page(monitor_file);
				(void)refresh();
				}
			else
				{
				(void)mvprintw(LINES-1, 70, "%s", "         ");
				display_cur_page(monitor_file);			
				(void)refresh();
				}
			if(cur_file_pg < tot_file_pg)
				{
				display(LINES-2,1,CLEAR,MORE);
				} 
			else
				{
				display(LINES-2,1,CLEAR,"");
				}
			if(cur_file_pg == 1)
				{
				display(LINES-2,1,CLEAR,"");
				}
			refresh();
			break;
		case	'D':
		case	'd':
			if (cur_col_pos <= 0)
				{
				cur_col_pos = 0;
				beep();
				}
			else 
				cur_col_pos --;
			display_cur_page(monitor_file);			
			(void)refresh();
			break;
		case	'C':
		case	'c':
			if ( ((cur_col_pos*COL_PER_PAGE) + COLS) < max_len_line)
				cur_col_pos ++;
			else
				beep();
			display_cur_page(monitor_file);			
			(void)refresh();
			break;
		default	:
			break;
		} /* switch */
	display_current_time();
	} /* while */
}

/*---------------------------------------------------------------------------
Display the data for the given page from the monitor file.
I'll give a dollar to anyone who can figure out the looping in this routine.
gpw 6/22/99
----------------------------------------------------------------------------*/
void display_cur_page(char *filename)
{
	int line_cnt=0;			/* line counter */
	int j;				/* loop variable */
	int page_cnt=0;			/* to count number of pages */
	char mesg[512];			
	FILE *fd_log;

	max_len_line = 80;

	/* Find the total number of lines in the file & refresh it */
	calculate_file_size(filename); 		
				
	if((fd_log = fopen(filename,"r")) == NULL)	
	{ 
		sprintf(mesg,"Unable to open <%s>. errno=%d.",filename, errno);
		display(LINES-2,1,CLEAR,mesg);
		refresh();
		return;
	}

	/* To skip the number of pages of the file */
	for(page_cnt=1; (!feof(fd_log)) && (page_cnt < cur_file_pg); page_cnt++)
	{
		/* following loop skip one page from file */
		for(line_cnt=1; (line_cnt <= LINES_PER_PAGE); line_cnt++)
		{
				(void)fgets(mesg,MAX_LEN,fd_log);
		} 
	}

	/* check for EOF */
	if(feof(fd_log))
	{
		display(LINES-2,1,CLEAR,"");
		(void)refresh();
		return;
	}

        /* Clear screen to show the file */
	clear_lines(3, 3+(LINES_PER_PAGE));

	/*  Display data lines for current page number  */
	for(line_cnt=1,j=3; !feof(fd_log) && line_cnt <= LINES_PER_PAGE;
								line_cnt++,j++)
	{
		/* read remaining lines */
		(void)fgets(mesg, MAX_LEN, fd_log);    
		if(feof(fd_log)) break;

		if ((int)strlen(mesg) <= (cur_col_pos * COL_PER_PAGE) )
			(void)mvprintw(j,0,"%s", " "); 	/* print nothing */
		else
			(void)mvprintw(j,0,"%.80s", 
				&mesg[(cur_col_pos * COL_PER_PAGE)]); 

		if ((int)strlen(mesg) > max_len_line)
			max_len_line = (int)strlen(mesg);
	}

	/* check for EOF */
	if(feof(fd_log))
	{
		display(LINES-2,1,CLEAR,"");
		(void)refresh();
	}

	if(fclose(fd_log) == EOF)
	{
		sprintf(mesg,"Unable to close <%s>. errno=%d.",filename, errno);
		display(LINES-2,1,CLEAR,mesg);
		exit_program(1);
	}
	return;
} 

static void display_resource_page(char *parmResFile)
{
	int line_cnt=0;			/* line counter */
	int j;				/* loop variable */
	int page_cnt=0;			/* to count number of pages */
	char mesg[512];			
	FILE *fd_log;
	char 	formattedResFile[256];
	int rc;

	max_len_line = 80;

	sprintf(formattedResFile, "%s.formatted", parmResFile);
	rc = formatOutputForResourceMonitor(parmResFile, formattedResFile);

	/* Find the total number of lines in the file & refresh it */
	calculate_file_size(formattedResFile); 		
				
	if((fd_log = fopen(formattedResFile,"r")) == NULL)	
	{ 
		sprintf(mesg,"Unable to open <%s>. errno=%d.",formattedResFile, errno);
		display(LINES-2,1,CLEAR,mesg);
		rc=getch(); /* gpw added 7/27/99 */
		refresh();
		return;
	}

	/* To skip the number of pages of the file */
	for(page_cnt=1; (!feof(fd_log)) && (page_cnt < cur_file_pg); page_cnt++)
	{
		/* following loop skip one page from file */
		for(line_cnt=1; (line_cnt <= LINES_PER_PAGE); line_cnt++)
		{
				(void)fgets(mesg,MAX_LEN,fd_log);
		} 
	}

	/* check for EOF */
	if(feof(fd_log))
	{
		display(LINES-2,1,CLEAR,"");
		(void)refresh();
		fclose(fd_log);
		unlink(formattedResFile);
		return;
	}

        /* Clear screen to show the file */
	clear_lines(4, 4+(LINES_PER_PAGE));

	/*  Display data lines for current page number  */
	for(line_cnt=1,j=4; !feof(fd_log) && line_cnt <= LINES_PER_PAGE;
								line_cnt++,j++)
	{
		/* read remaining lines */
		(void)fgets(mesg, MAX_LEN, fd_log);    
		if(feof(fd_log)) break;

		if ((int)strlen(mesg) <= (cur_col_pos * COL_PER_PAGE) )
			(void)mvprintw(j,0,"%s", " "); 	/* print nothing */
		else
			(void)mvprintw(j,0,"%.80s", 
				&mesg[(cur_col_pos * COL_PER_PAGE)]); 

		if ((int)strlen(mesg) > max_len_line)
			max_len_line = (int)strlen(mesg);
	}

	/* check for EOF */
	if(feof(fd_log))
	{
		display(LINES-2,1,CLEAR,"");
		(void)refresh();
	}

	if(fclose(fd_log) == EOF)
	{
		sprintf(mesg,"Unable to close <%s>. errno=%d.",formattedResFile, errno);
		display(LINES-2,1,CLEAR,mesg);
		unlink(formattedResFile);
		exit_program(1);
	}
	unlink(formattedResFile);
	return;
} /* END display_resource_page() */ 

/*----------------------------------------------------------------------------
display the page heading on the screen and options 
----------------------------------------------------------------------------*/
static void display_page_heading(int selected_app, int page_no,	int app_pid)
{
	char head[85];	

	sprintf(head,
	"Monitoring application # %d with pid=%d running on port %d  Page : %d ",
		selected_app, app_pid, port_no, page_no); 

	display_string_centered(head);	

	return;
} 

/*----------------------------------------------------------------------
Display the menu to show the file 
----------------------------------------------------------------------*/
static void display_options_line()
{

	mvaddch(LINES - 1,27,'N'|A_STANDOUT);
	addstr("ext  ");

	addch('P'|A_STANDOUT);
	addstr("rev  ");

	addch('S'|A_STANDOUT);
	addstr("croll ");

	addch('<'|A_STANDOUT);
	addch('-'|A_STANDOUT);
	addstr("  ");

	addch('-'|A_STANDOUT);
	addch('>'|A_STANDOUT);
	addstr("  ");

	addch('Q'|A_STANDOUT);
	addstr("uit");

	display_current_time();
	refresh();

	return;
}

/*------------------------------------------------------------------------
This tran_shmidroutines finds the tran_proc pid by using serial number of tran_proc  
-------------------------------------------------------------------------*/
int find_tran_proc_pid(which_one, app_pid)	
int which_one;
int *app_pid;
{
char	*ptr;
char	str_pid[10];
char	port_str[20];

	*app_pid=0;
	ptr = 0;
	Mem_ok = 1;
	if(which_one <= 0)
	{
		(void)shmdt(tran_tabl_ptr);
		return (-1);
	}

	if((tran_shmid = shmget(Shm_key, SIZE_SCHD_MEM, 0))<0)
	{
		clear_lines(5, 5+(LINES-8));
		(void)mvprintw(5, 0,NOT_RUNNING_MSG_FORMAT, ArcServiceName);
       		Mem_ok = 0;
		*app_pid = -1;
		return (-1);
	}

	if ((tran_tabl_ptr = shmat(tran_shmid, 0, 0)) < (void * )0)
	{
		clear_lines(5, 5+(LINES-8));
		(void)mvprintw(5, 0,NOT_RUNNING_MSG_FORMAT, ArcServiceName);
       		Mem_ok = 0;
		*app_pid = -1;
		return(-1);
	}

	if(Mem_ok)
	{
		ptr = tran_tabl_ptr;
		no_reco = (int)strlen(ptr)/SHMEM_REC_LENGTH;	
	}	

	if(ptr)
	{
		read_fld(tran_tabl_ptr,which_one-1,APPL_PID,str_pid);
		*app_pid = atoi(str_pid);
		read_fld(tran_tabl_ptr,which_one-1,APPL_RESOURCE,port_str);
		port_no = atoi(port_str);
	} 

	if(Mem_ok)
	(void)shmdt(tran_tabl_ptr);

	/* return the row number of the shared memory */ 
	return (which_one);	
} /* of find_tran_proc_pid() */

/*-----------------------------------------------------------------------------
This routine allow user to select the process of interest to start or stop
monitoring, or to view.
-----------------------------------------------------------------------------*/
int select_process(action)
int	action;
{
	static char enter[]="Enter the number of application to";
	static char press_enter[]="& press Enter : ";
	static char stop[]="stop monitoring";
	static char view[]="monitor";
	int which_one = 0;
	int ch;
	char	prompt[80];

	switch(action)
	{
	case UNMONITOR:
		sprintf(prompt,"%s %s %s", enter, stop, press_enter);
		display(LINES-2, 1, CLEAR, prompt);
		move(LINES-2, 68);
		break;
	case MONITOR:
		sprintf(prompt,"%s %s %s", enter, view, press_enter);
		display(LINES-2, 1, CLEAR, prompt);
		move(LINES-2, 60);
		break;
	}

	
	(void)echo();
	(void)scanw("%d",&which_one);
	(void)noecho();
	(void)refresh(); 
	
	/* Clear the prompt */
	display(LINES-2, 1, CLEAR, "");

	if (which_one == 0) return -1;	

	if ((which_one < 1) || (which_one > no_reco))
	{
		display(LINES-2, 1, CLEAR, "Choice out of range. Press Enter ");
		ch = getch();
		display(LINES-2, 1, CLEAR, "");
		return -1;
	}
	return (which_one);
} 

/*--------------------------------------------------------------------------
This routine calculates the total number of lines in the file and total number
of pages in the file, and refreshes the screen.
---------------------------------------------------------------------------*/
void calculate_file_size(file)	
char *file;		
{
	int cnt;
	FILE *fd;
	char tmp[MAX_LEN];

	if((fd = fopen(file,"r")) == NULL)
	{ 
		(void)move(LINES-2,10);
		(void)clrtoeol();
		(void)mvprintw(LINES-2,10,"Unable to open <%s>. errno=%d",
				file, errno);
		(void)refresh();
		return;
	}

	/* Count the total number of lines in the file  */
	/* assumption record contains MAX_LEN char */
	for (cnt=1; (!feof(fd)); cnt++)
	{
		(void)fgets(tmp, MAX_LEN, fd);	
	}

	tot_file_lines = cnt ;	/* store the total lines in the file*/
	tot_file_pg = (tot_file_lines / LINES_PER_PAGE) + 1;

	if(fclose(fd) == EOF)
	{
  		(void)mvprintw(LINES-2,70,"%s",
			"Error: Failed to close monitor file.");
		sleep(2);
  		(void)refresh();
 		exit_program(1);
  	}
}

/*---------------------------------------------------------------------------
This routine returs the shared memory pointer
----------------------------------------------------------------------------*/
void get_shmem()
{

      	Mem_ok = 0;
	if((tran_shmid = shmget(Shm_key, SIZE_SCHD_MEM, 0))<0)
	{
 		clear_lines(5, 5+(LINES-8));
		(void)mvprintw(5, 0,NOT_RUNNING_MSG_FORMAT, ArcServiceName);
        	(void)refresh();
 		return;
 	}

	if ((tran_tabl_ptr = shmat(tran_shmid, 0, 0)) < (void *)0)
	{
		clear_lines(5, 5+(LINES-8));
		(void)mvprintw(5, 0,NOT_RUNNING_MSG_FORMAT, ArcServiceName);
      	 	 (void)refresh();
 		return;
 	}
	Mem_ok = 1; 
	return;
} 

/*---------------------------------------------------------------------------
Display the data for the given page from the specified file.
----------------------------------------------------------------------------*/
void display_last_page(filename)
char *filename;
{
	int line_cnt=0;	
	int j ;			
	int page_cnt=0;
	char mesg[512];	
	FILE *fd_log;


	calculate_file_size(filename); 

	if((fd_log = fopen(filename,"r")) == NULL)
	{ 
		move(LINES-2,1);
		clrtoeol();
		sprintf(mesg,"Failed to open <%s>. errno=%d.",filename,errno); 
		display(LINES-2,1,CLEAR,mesg);
		sleep(2);
		refresh();
		return;
	}

	cur_file_pg = tot_file_pg;

	/* To skip the # number of pages of the file */
	for(page_cnt=1; (!feof(fd_log)) && page_cnt < tot_file_pg; page_cnt++)
	{
		for(line_cnt=1; line_cnt <= LINES_PER_PAGE; line_cnt++)
		{
			fgets(mesg,MAX_LEN,fd_log);
		}
	}

	/* check for end of file */
	if(feof(fd_log))
	{
		fclose(fd_log);
		move(LINES-2,1);
		clrtoeol();
/*		mvprintw(LINES-2,1,"%s","--end of file--"); */
		refresh();
		return;
	}

	clear_lines(3, 3+(LINES_PER_PAGE));  

	/*  Display the data lines for current page number  */
	for(line_cnt=1,j=3; !feof(fd_log)&&(line_cnt <= LINES_PER_PAGE);
								line_cnt++,j++)
	{
		/* read remaining line from file */
		fgets(mesg, MAX_LEN, fd_log);  
		if(feof(fd_log)) break;
		if ((int)strlen(mesg) <= (cur_col_pos * COL_PER_PAGE) )
			mvprintw(j,0,"%s", " "); 	/* print nothing */
		else
			mvprintw(j,0,"%.80s",&mesg[(cur_col_pos*COL_PER_PAGE)]); 
		if ((int)strlen(mesg) > max_len_line)
			max_len_line = (int)strlen(mesg);
		if(line_cnt > 14)
			sleep(2);
	}

	fflush(stderr);
	fflush(stdout);
	standout();
	mvprintw(LINES-1, 70, "scroll on");
	standend();
	refresh();

	/* check for EOF  */
	if(feof(fd_log))
	{
		(void)move(LINES-2,1);
		(void)clrtoeol();
	/*	(void)mvprintw(LINES-2,1,"%s","--end of file --");*/
		(void)refresh();
	}

	if(fclose(fd_log) == EOF)
	{
		(void)move(LINES-2,10);
		(void)clrtoeol();
		(void)mvprintw(LINES-2,10,"Unable to close file <%s>. errno=%d",
			filename, errno);
		exit_program(1);
	}
	refresh();
	return;
} 

/*-----------------------------------------------------------------------------
 this routine displays the tran_proc status 
-----------------------------------------------------------------------------*/
void DisplayResults(char *ResFile)	
{
int	i, j;
int	flag=0;
char	ReqType[12];
char	PreType[12];
char	ResNo[32];
char	ResStatus[7];
char	AppPid[5];
char	PortNum[5];
char	ApplName[156];
FILE	*ptr;
int	no_lines;		/* # LINES displayed on the screen */
char	buf[256];
char	dummy[10];
int	port;

ptr = fopen(ResFile, "r");
if(ptr == NULL)
	{
	clear_lines(5, 5+(LINES-8));
	(void)mvprintw(5, 0,ResourceMgrProblemMsg);
        (void)refresh();
        Mem_ok = 0;
	}
if(Mem_ok) { }

PreType[0]='\0';
(void)mvaddstr(4,1,"REQUEST  DEVICE         STATUS  PID    PORT  APPL NAME");
	j=6;
	ReqType[0]='\0';
	ResNo[0]='\0';
	ResStatus[0]='\0';
	AppPid[0]='\0';
	PortNum[0]='\0';
	ApplName[0]='\0';
	buf[0]='\0';
while(fgets(buf, sizeof(buf), ptr) != NULL)
	{
	 /* read the parameters */
	(void)sscanf(buf, "%s %s %s %s %s %s %s\n", 
		ReqType, dummy, ResNo, AppPid, ResStatus, PortNum, ApplName);

	(void)move(j,0);
	(void)clrtoeol();
	if(flag == 0 || strcmp(ReqType,PreType) !=0)
		{
		if(flag == 1)
			j++;
		(void)mvprintw(j,1, "%-6s", ReqType);
		sprintf(PreType,"%s", ReqType);
		flag = 1;
		}
	if ( strcmp(ResStatus, "FREE") == 0 )
	{
		ApplName[0]='\0';
		PortNum[0]='\0';
		AppPid[0]='\0';
	}
	else
	{
		port=atoi(PortNum);
		if ( port > 0 )
		{
			--port;
		}
		sprintf(PortNum, "%d", port);
	}
	(void)mvprintw(j,11, "%-16s", ResNo);
	(void)mvprintw(j,26, "%.6s", ResStatus);
	(void)mvprintw(j,33, "%.5s", AppPid);
	(void)mvprintw(j,41, "%.3s", PortNum);
	(void)mvprintw(j,47, "%.55s", ApplName);

	j++;
	ResNo[0]='\0';
	ResStatus[0]='\0';
	AppPid[0]='\0';
	PortNum[0]='\0';
	ApplName[0]='\0';
	buf[0]='\0';
	} /* if (ptr) */
fclose(ptr);
display_current_time();
(void)refresh();
return;
} /* of DisplayResults() */

/*-----------------------------------------------------------------------------
This routine sends the request to message queue.
-----------------------------------------------------------------------------*/
int 	send_msg(q_id, message) 
int	q_id;
Msg	*message; 
{
	int ret;

	if((ret=msgsnd(q_id, message, MSIZE, IPC_NOWAIT)) != 0) 
	{
		fprintf(stderr,
		"%s: Unable to send message to qid=%d. errno=%d\n", 
		ModuleName,q_id, errno);
		return (-1);
	}
	return (0);
} /* send_msg */

/*-----------------------------------------------------------------------------
This routine receives a message from the message queue.
-----------------------------------------------------------------------------*/
static int recv_msg(int	q_id, Msg *message, long pid)
{
	int	ret;
	if((ret=msgrcv(q_id, message, MSIZE, pid,0)) == -1) 
	{
		fprintf(stderr,
	"%s: Unable to receive message from qid=%d for pid=%d. errno=%d\n", 
		ModuleName,q_id, pid, errno);
		return (-1);

	}
	return(0);
}
/*----------------------------------------------------------------------------
Get the name of the directory where the monitor files are stored so we know
can (later) construct the name of the file into which detail monitor info
is written.
----------------------------------------------------------------------------*/
static int get_monitor_directory(char *dir)
{
	char	*isp_base_dir;

	isp_base_dir=(char *)getenv("ISPBASE");
	if (isp_base == NULL)
	{
		fprintf(stderr, 
	      "%s: Failed to access environment variable ISPBASE. errno=%d.\n", 
		ModuleName, errno);
		return (-1);
	}
	sprintf(dir, "%s/MONITOR", isp_base_dir);
}

/*----------------------------------------------------------------------------
Get various pieces of information specific to the service under which this
monitor runs. These will be used throughout the program.
----------------------------------------------------------------------------*/
static int get_service_info(char *prog_name, int *service, key_t *shm_key, long *que_no, char *service_name, char *title)
{
	int i;

		*service = TEL;
		sprintf(service_name, "%s", "Telecom");
		*shm_key = SHMKEY_TEL;
		*que_no  = TEL_RESP_MQUE;
		sprintf(title, "%s", "Telecom Server Monitor");
	return(0);
}


/*----------------------------------------------------------------------------
This routine displays the process status and lets the user choice which process
to monitor.
----------------------------------------------------------------------------*/
static int monitor_apps(int arc_service, char *title)
{
	int	ch, rc;
	int selection=0;
	int app_pid=0;
	int status;
	char monitor_file[256];
	char	*ptr;

	ptr = 0;
	get_shmem();
	if (! Mem_ok)
	{
		printf("Telecom is not running.\n"); fflush(stdout);
		return(0);
	}

	if (arc_service == TEL)

 		strcpy(column_headings,
	 	"App#. Application Name    Channel    Status   pid      API Name");
	else
 		strcpy(column_headings,
		"App#. Application Name    Resource   Status   pid      API Name");
	
//	display_monitor_app_header(title);
	display_results();
	if(cur_pg_no < max_pg)
		display(MsgLine,1,CLEAR,MORE);

#if 0
	ch = 0;
	while ( 1 )
	{
		selection=0; 

		timeout(2000);

		ch = getch();

		switch(ch)
		{
		case 'r':			/* Refresh */
		case 'R':
			display_monitor_app_header(title); 
			display_results();
			if(cur_pg_no < max_pg)
				display(MsgLine,1,CLEAR,MORE);
			break;
		case 'q':			/* Quit from the option */
		case 'Q':
			(void)clear();
			return;
		case 'N':			/* Page Down */
		case 'n':
			if(++cur_pg_no > max_pg)
				cur_pg_no = 1;
			(void)mvprintw(LINES - 1,70,"Page: %d  ", cur_pg_no );
			display_results();
			if(cur_pg_no < max_pg)
				display(MsgLine,1,CLEAR,MORE);
			else
				display(MsgLine,1,CLEAR,"");
			break;
		case 'P':			/* Page Up */
		case 'p':
			if(--cur_pg_no < 1)
				cur_pg_no = max_pg;
			(void)mvprintw(LINES-1,70,"Page: %d  ", cur_pg_no );
			display_results();
			if(cur_pg_no < max_pg)
				display(MsgLine,1,CLEAR,MORE);
			else
				display(MsgLine,1,CLEAR,"");
			break;
		case 'm':	/* Monitor the app */
		case 'M':		
/* gpw don't need	if (selection == 0) */
				selection = select_process(MONITOR);
/*			if (selection == -1) continue; */
			if (selection == -1) break;
/* gpw don't need 		display(LINES-2,1,CLEAR,""); */
			refresh();
 			rc = monitor_status(GET_STATUS, selection, &status);
			if (rc != 0) continue;
			/* If app already monitored, don't have to turn it on */
			if (status != STATUS_MONT)
			{
				status=STATUS_MONT; 
				rc=monitor_status(SET_STATUS,selection,&status);
				if (rc != 0) continue;
			}

			find_tran_proc_pid(selection, &app_pid); 
			sprintf(monitor_file,"%s/monitor.%-d", 
						Monitor_dir, app_pid);	
			view_a_monitored_app(selection, app_pid, monitor_file);
			display_monitor_app_header(title);
			display_results();
			break;
		case 'u':
		case 'U':		/* Unmonitor */
			if(Mem_ok)
			{
				selection = select_process(UNMONITOR);
				display(LINES-2,1,CLEAR,"");
				(void)refresh();
			}
			if(selection < 0) continue;	
 			rc = monitor_status(GET_STATUS, selection, &status);
			if (rc != 0) continue;
			/* If app not monitored, don't do anything */
			if (status != STATUS_MONT) continue;
			status=STATUS_UMON; 
			rc = monitor_status(SET_STATUS, selection, &status); 
			if (rc != 0) continue;
			break;		
		case ERR:
			display_results();
			break;
		default	:
			break;
		} /* switch */

	} /* while */
#endif
}

/*------------------------------------------------------------------------
This routine displays the heading for transaction process status
and displays the menu.
-------------------------------------------------------------------------*/
static int display_monitor_app_header(char *title)
{

	(void)clear();
	display_string_centered(title);

	mvaddstr(3,1, column_headings);

	mvaddch(LINES - 1,27,'N'|A_STANDOUT);
	addstr("ext ");

	addch('P'|A_STANDOUT);
	addstr("rev ");

	addch('R'|A_STANDOUT);
	addstr("efresh ");
	
	addch('M'|A_STANDOUT);
	addstr("onitor " );
	
	addch('U'|A_STANDOUT);
	addstr("nmonitor " );

	addch('Q'|A_STANDOUT);
	addstr("uit");

	display_current_time();
	mvprintw(LINES-1,70,"Page: %d  ", cur_pg_no );
	refresh();
	return(0);
}

/*---------------------------------------------------------------------------
This routine clears the given range of line on the screen
i - start line number
j - end line number 
----------------------------------------------------------------------------*/
static void clear_lines(int i, int j)
{
for (;i <= j; i++)
	{
	(void)move (i,0);
	(void)clrtoeol();
	}
return;
}

/*---------------------------------------------------------------------------
This routine display the current system time on the last line of the screen   
---------------------------------------------------------------------------*/
static void display_current_time()
{
	long cur_time;
	char *ptr;

	(void)time(&cur_time);
	ptr = ctime(&cur_time);
	/* gpw this is dangerous (below) */
	ptr[24] = 0;
	(void)mvaddstr(LINES-1,1,ptr);
	(void)refresh();
	return;
}

/*--------------------------------------------------------------------
This routine displays the string, e.g. a heading, at the centre of line 
----------------------------------------------------------------------*/
static void display_string_centered(char *str)
{
	int len, center;

	len = strlen(str);
	center = (80 - len)/2;

	printf("%s\n", str);	fflush(stdout);
//	(void)mvaddstr(1,center,str);
//	(void)move(2,center);
//	while (len-->0)
//		(void)addch('~');
	return;
}

static int display(int row, int col, int option, char *data)
{

//	mvprintw(row,col,data);
	printf("%s\n", data); fflush(stdout);

}
/* this is all junk */


/*-----------------------------------------------------------------------------
This routine displays the resource status information.
-----------------------------------------------------------------------------*/
static int display_resource_status()
{
	int ch, prev_ch;

	char ResFile[256];
	char logMsg[256];
	char title[]="Resource Status" ;

	cur_file_pg=1;
	clear();
	if (Status_Resources(ResFile, logMsg) != 0)
	{
		display_resource_page_heading(title,cur_file_pg, ResFile);  
		display(10,1,CLEAR,"Resource Manager may not be running. Press Enter.");
		ch = getch();
		return(0);	
	}

	display_resource_page_heading(title,cur_file_pg, ResFile);  
	display_resource_page(ResFile);		
	if(cur_file_pg < tot_file_pg)
	{
		display(MsgLine,1,CLEAR,MORE);
	}
	refresh();

	while ( 1 )
	{
		timeout(1000);
		ch = getch();
	
		switch(ch)
		{
		case 'q':
		case 'Q':
			clear();
			return(0);;
			break;
		case 'r':
		case 'R':
			if (Status_Resources(ResFile, logMsg) != 0)
			{
				display(10,1,CLEAR,"Not running. Press Enter.");
				ch = getch();
				return(0);	
			}
			display_resource_page_heading(title,cur_file_pg,ResFile);  
			display_resource_page(ResFile);		
			if(cur_file_pg < tot_file_pg)
				display(MsgLine,1,CLEAR,MORE);
			else
				display(MsgLine,1,CLEAR," ");
			refresh();
			break;
		case 'N':
		case 'n': 
			display(LINES-1,70,NO_CLEAR,"         ");

			if(cur_file_pg >= tot_file_pg)
				cur_file_pg = 0;
			
			++cur_file_pg;
			display_resource_page_heading(title,
						cur_file_pg, ResFile);  
			display_resource_page(ResFile);			
			if(cur_file_pg < tot_file_pg)
				display(MsgLine,1,CLEAR,MORE);
			else
				display(MsgLine,1,CLEAR," ");
			 
			refresh();
			prev_ch = 'N';
			break;
		case 'P':
		case 'p':
			display(LINES-1,70,NO_CLEAR,"         ");
			if(cur_file_pg > 1)	
			    	cur_file_pg--;		/* previous page */
			else
			{
				cur_file_pg = tot_file_pg;
			}

			display_resource_page_heading(title,
						cur_file_pg, ResFile);  
			display_resource_page(ResFile);	
			if(cur_file_pg < tot_file_pg)
				display(MsgLine,1,CLEAR,MORE);
			else
				display(MsgLine,1,CLEAR,"");
				
			refresh();
			prev_ch = 'P';
			break;
		case 'S':		/* scroll */
		case 's':
			display_resource_page_heading(title,
						cur_file_pg, ResFile);  
			display_last_page(ResFile);
			refresh();		
			prev_ch = 'S';
			break;
		default	:
			break;
		} /* switch */
		display_current_time();
	} /* while */
}

/*----------------------------------------------------------------------------
Display the page heading on the screen.
----------------------------------------------------------------------------*/
static int display_resource_page_heading(char *title, int page_no, char *file)
{
	char head[85];	
	char buf[150];

	sprintf(head,"%s", title);

	clear();
	display_string_centered(head);

	sprintf(buf, "%-22s%-12s%-10s%-7s%-10s%s", "REQUEST", "DEVICE", 
		"STATUS", "PID", "RESOURCE", "APPLICATION NAME");
	mvaddstr(3,0,buf);

	mvaddch(LINES - 1,27,'N'|A_STANDOUT);
	addstr("ext  ");

	addch('P'|A_STANDOUT);
	addstr("rev  ");

	addch('R'|A_STANDOUT);
	addstr("efresh  ");

	addch('Q'|A_STANDOUT);
	addstr("uit");

	display_current_time();
	mvprintw(LINES-1,70,"Page: %d  ", page_no );
	refresh();
	return(0);
} 

static int my_debug(char *s)
{
	char command[1024];
	sprintf(command, "echo %s >>/tmp/trash6", s);
	system(command);
	return(0);
}

static int monitor_status(int action, int selection, int *status)
{
	int rc;
	char data[64];
	char mesg[256];			

	get_shmem();

	switch(action)
	{
	case GET_STATUS:
		if (Mem_ok)
		{		
			read_fld(tran_tabl_ptr, selection-1, APPL_STATUS, data);
			*status=atoi(data);
		}
		else
			return(-1);
		break;
	case SET_STATUS:
		if (Mem_ok)
		{		
			rc=write_fld(tran_tabl_ptr, selection-1, 
						APPL_STATUS, *status);
			if (rc == -1)
			{
				sprintf(mesg,
				"Failed to update shared memory status");
				display(LINES-2,1,CLEAR,mesg);
				return(-1);
			}
		}
		else
			return(-1);
		break;
	default:
		sprintf(mesg, "Invalid parameter %d passed to monitor_status",
				action);	
		display(LINES-2,1,CLEAR,mesg);
		return(-1);
		break;
	}
	if (Mem_ok) shmdt(tran_tabl_ptr);

	return(0);
}

static int formatOutputForResourceMonitor(char *parmUnformattedFile, char *parmFormattedFile)
{
	FILE *fpUnformat, *fpFormat;
	char	lineBuf[256];
	char	requestPart1[20], requestPart2[30], device[20];
	char	pid[10], status[15], resNo[6], appName[128];

	if((fpUnformat = fopen(parmUnformattedFile, "r")) == NULL)
	{
		return(-1);
	}

	if((fpFormat = fopen(parmFormattedFile, "w+")) == NULL)
	{
		fclose(fpUnformat);
		return(-1);
	}

	memset(lineBuf, 0, sizeof(lineBuf));
	while(fgets(lineBuf, 256, fpUnformat) != NULL)
        {
		memset(requestPart1, 0, sizeof(requestPart1));
		memset(requestPart2, 0, sizeof(requestPart2));
		memset(device, 0, sizeof(device));
		memset(pid, 0, sizeof(pid));
		memset(status, 0, sizeof(status));
		memset(resNo, 0, sizeof(resNo));
		memset(appName, 0, sizeof(appName));

		sscanf(lineBuf, "%s %s %s %s %s %s %s", requestPart1, 
			requestPart2, device, pid, status, resNo, appName);
		fprintf(fpFormat, "%-4s%-18s%-12s%-10s%-7s%-10s%s\n",
			requestPart1, requestPart2, device, status, pid, resNo,
			appName);	
		memset(lineBuf, 0, sizeof(lineBuf));
	} /* End while(fgets(buf, BUFSIZ, ptr) != NULL) */

	fclose(fpUnformat);
	fclose(fpFormat);

	return(0);
} /* formatOutputForResourceMonitor() */