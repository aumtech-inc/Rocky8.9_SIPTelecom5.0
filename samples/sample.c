/*---------------------------------------------------------------------------
File:		sample.c
Purpose:	This is a sample program. It is intended to illustrate the 
		basic structure of a typical Aumtech Telecom Services 
		menu based application, demonstrate a few frequently used 
		Telecom Services APIs, and provide examples of
		well-structured and well-commented routines.

		This application, if unmodified, speaks a set of instructions,
		displays them on the screen, records a set of prompts, 
		and prompts the user for DTMF input until the user presses 
		the exit menu choice. After each entry the program speaks back 
		what the user entered. If you have already recorded a set of
		prompts and do not want to re-record them each time you 
		run this program, just change "#define RECORD_NEW_PROMPTS" to
		"#undef RECORD_NEW_PROMPTS" below. 

		Note: You should tail the contents of the Aumtech system log
		as well as nohup.out when you run this program. The Aumtech
		system log is $HOME/.ISP/LOG/ISP.cur. nohup.out is located
		in the $TELECOM/Exec directory.

Author:		Aumtech, Inc.
Date:		02/21/01 gpw created IP Telecom version
---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Telecom.h"

#define RECORD_NEW_PROMPTS

#define EXIT 1
#define DONT_EXIT 2

#define EXIT_CHOICE 9

#define INSTRUCTIONS_PHRASE	"instructions.wav"
#define INVALID_CHOICE_PHRASE	"invalid_choice.wav"
#define MENU_PHRASE		"menu.wav"
#define YOU_ENTERED_PHRASE	"you_entered.wav"

/* Function prototype of log off routine assigned to SERV_M3 */
int disconnect_detected();
static int handleError (char *apiName, int errCode, int action);
static int get_the_menu_choice(int *choice);
static int valid_choice(int choice);
static void LogMsg(char *s);
static int record_all_prompts();
static int record_prompt(char *phrase_file, char *help_text);
static void provide_instructions();
static int process_the_choice(int choice);
static void myLogOff () ;



/*-----------------------------------------------------------------------------
Main application routine.
-----------------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
	int rc;
	int choice;

	/* Specify routine to be called whenever caller hangs up. */	
	SERV_M3=disconnect_detected;

	rc = TEL_InitTelecom (argc, argv);
	if (rc != TEL_SUCCESS) handleError ("TEL_InitTelecom",rc,EXIT);
	
	rc = TEL_AnswerCall (6);
	if (rc != TEL_SUCCESS) handleError ("TEL_AnswerCall",rc,EXIT);

	provide_instructions();
		
#ifdef RECORD_NEW_PROMPTS
	record_all_prompts();
#endif

	choice = 0;
	while ( choice != EXIT_CHOICE )
	{
		get_the_menu_choice(&choice);
		if ( (valid_choice(choice)) && (choice != EXIT_CHOICE) )
			process_the_choice(choice);
	}

	myLogOff();
}

/*----------------------------------------------------------------------------
This routine speaks instructions and writes them to wherever LogMsg writes.
----------------------------------------------------------------------------*/
static void provide_instructions()
{
	int rc;
	char data[80];
	int timeout=20;
	int tries=1;
	int beep=NO;
	int length=1;

	while ( 1 )
	{
		TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, 
			PHRASE_FILE, PHRASE, INSTRUCTIONS_PHRASE, SYNC);
		TEL_GetDTMF(FIRST_PARTY, timeout, timeout, tries, beep, '#', 
				length, AUTOSKIP, NUMSTAR, data, "");
		if (strcmp(data,"1") == 0) 
			continue;
		else
			break;
	}
	
	LogMsg("Instructions for Aumtech's sample app, 'sample.c'\n");
	LogMsg("To modify this app,");
	LogMsg("    type 'Ta', then 'vi sample.c'");
	LogMsg("    after exiting vi, type 'makegen sample'");
	LogMsg("To stop recording new prompts each time you run this program,");
	LogMsg("    type 'Ta', 'vi sample.c'");
	LogMsg("    change '#define RECORD_NEW_PHRASES' to '#undef RECORD_NEW_PHRASES'");
	LogMsg("    after exiting vi, type 'makegen sample'");
	LogMsg("If you have not disabled prompt recording as explained above,");
	LogMsg("    you will be prompted to record your prompts.");
	LogMsg("\nPress any DTMF key to continue.");
	
}

/*---------------------------------------------------------------------------
This routine allows you to record each of the prompts for this application in 
your own voice.
---------------------------------------------------------------------------*/
static int record_all_prompts()
{
	record_prompt(MENU_PHRASE, "Please press 1, 2, or 3, or 9 to exit"); 
	record_prompt(INVALID_CHOICE_PHRASE, "Your choice is not valid");
	record_prompt(YOU_ENTERED_PHRASE, "You entered...");
	LogMsg("\nProceeding with the application, using your new prompts.");
	sleep(2);
	return(0);
}


/*-----------------------------------------------------------------------------
This routine speaks a phrase and collects a user choice. 
-----------------------------------------------------------------------------*/
static int get_the_menu_choice(int *choice)
{
	int rc;
	char choice_str[80];
	int timeout=8;
	int tries=3;
	int beep=NO;
	int length=1;
	
	TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, 
				PHRASE_FILE, PHRASE, MENU_PHRASE, SYNC);
	TEL_GetDTMF(FIRST_PARTY, timeout, timeout, tries, beep, '#', 
				length, AUTOSKIP, NUMERIC, choice_str, ""); 
	*choice=atoi(choice_str);
	return(0);
}

/*----------------------------------------------------------------------------
This routine checks the validity of the menu choice passed to it. If it is
valid, it returns a 1, otherwise it returns a 0 and tells you that the
choice is invalid.
----------------------------------------------------------------------------*/
static int valid_choice(int choice)
{	
	switch(choice)
	{
		case 1:	
		case 2:	
		case 3:	
		case EXIT_CHOICE:	
			return(1);
			break;	
		default:	
			TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, 
			     PHRASE_FILE, PHRASE, INVALID_CHOICE_PHRASE,SYNC);
			return(0);
			break;	
	}
}

/*----------------------------------------------------------------------------
This routine process the menu choice passed to it. In this sample program
it simply speaks back the choice. In a more realistic application it might
have a switch on the choice to call a different routine to process each of
the legitimate menu choices.
----------------------------------------------------------------------------*/
static int process_the_choice(int choice)
{		
	TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, PHRASE_FILE, PHRASE, 
						YOU_ENTERED_PHRASE, SYNC);
	TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, INTEGER, NUMBER,
						&choice, SYNC);
	return(0);
}

	
/*----------------------------------------------------------------------------
This routine is provided so that you can record the prompts for this 
sample application in your own voice. To disable the recording of new prompts
each time you run this application, simply undefine the variable
RECORD_NEW_PROMPT above.
----------------------------------------------------------------------------*/
static int record_prompt(char *phrase_file, char *help_text)
{
	int rc;
	int record_time_max=30;
	int record_compression=COMP_WAV;
	int overwrite=YES;
	int lead_silence=5;
	int trail_silence=5;
	int beep=YES;
	char help_msg[512];

	sprintf(help_msg, "\nAfter the beep, say '%s' then press 1.", 
		help_text);
	
	LogMsg(help_msg);
	sleep(5);
	rc = TEL_Record(FIRST_PARTY, phrase_file, record_time_max, 
		record_compression, overwrite, lead_silence, trail_silence,
		beep, FIRST_PARTY_INTERRUPT, '1', SYNC);
	if (rc != TEL_SUCCESS) handleError ("TEL_Record",rc, DONT_EXIT);
	sleep(1);
}


			
/*-----------------------------------------------------------------------------
This routine is required. Your application will not link if this routine is
not present. This routine is called automatically if you application is killed,
therefore it is a good place to put code that must be executed before your
application dies, e.g., code that logs data about the call. 
-----------------------------------------------------------------------------*/
static void myLogOff () 
{ 
	LogMsg("myLogOff routine terminating the application.");
	TEL_ExitTelecom();
	exit(0);
}

/*-----------------------------------------------------------------------------
This routine is designed to be used with the SERV_M3 variable. Setting the
SERV_M3 variable to the name of this routine will cause this routine to 
execute any time a TEL_ API detects that the caller has hung up.
-----------------------------------------------------------------------------*/
int disconnect_detected()
{
	LogMsg("Disconnect detected. Exiting.");
	TEL_ExitTelecom();
	exit(0);
}

/*----------------------------------------------------------------------------
This is a simple error handling routine which logs the name of the API that
failed and the return code with which it failed. This simple function does
"application level logging". It is always a good idea to have your 
applications do such logging to supplement the information written by the
Aumtech APIs and other program. Aumtech APIs and other programs log to the
"Aumtech system log". This file is ISP.cur in the directory
$HOME/.ISP/LOG.
----------------------------------------------------------------------------*/
static int handleError (char *apiName, int errCode, int action)
{
	char	m[256];
	sprintf (m, "%s failed. rc=%d.", apiName, errCode);
	LogMsg(m);

	if (action == EXIT) 
	{
		TEL_ExitTelecom();
		exit (1);
	}
}

/*----------------------------------------------------------------------------
This is a simple logging routine that does "application level logging". It is
a good idea to do all such logging through a single routine such as this 
(usually one defined in a separate library). This makes it easy to make 
systemwide changes to your log messages. For example, you can add the pid to
all log messages simply by changing this routine.  This routine logs to 
stdout for convenience. Your log routine should log to a file.
----------------------------------------------------------------------------*/
static void LogMsg(char *s)
{
	fprintf(stdout,"%s\n", s);
	fflush(stdout);

}

/*-----------------------------------------------------------------------------
Text of the instructions:

This is a sample program that will help you to understand the structure and
functionality of a simple menu driven Aumtech Telecom Services application.

You can interrupt these intructions any time you like by pressing a DTMF key.

You should open a terminal window on the machine on which are running this
application.  Please do so now. (Pause)

In that window, type capital T, small e, and press Enter.

Now tail the file nohup.out. That is, type tail -f nohup.out.

Instructions for running this program and in particular for guiding you thru
the recording of prompts, will be displayed.

If you want to hear these instructions again, press 1, otherwise press any
other DTMF key.
----------------------------------------------------------------------------*/
 
