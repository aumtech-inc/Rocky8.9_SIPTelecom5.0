
/*-----------------------------------------------------------------------------
Program:	update_config.c
Purpose:	To update a guidefinition file inserting or removing entries
		under specified headers.
Author:		George Wenzel
Date:		09/02/99
-----------------------------------------------------------------------------*/
#define CREATE	1 
#define INSERT	2
#define REMOVE 	3

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>

extern int errno;

char Msg[256];

FILE	*fp_in, *fp_out;

main(int argc, char **argv)
{
	
	char 	guidef_file[256];
	char	operation[128];
	char	header[128];
	char	entry[128]; 
	int	opcode;

	strcpy(operation, 	argv[1]);
	strcpy(guidef_file, 	argv[2]);
	strcpy(header, 		argv[3]);
	strcpy(entry, 		argv[4]);

	if (strcmp(operation, "INSERT") == 0)
		opcode=INSERT;
	else
	if (strcmp(operation, "REMOVE") == 0)
		opcode=REMOVE;
	else
	{
		sprintf(Msg,"Invalid operation: <%s>.", operation);
		log_it(Msg);
		exit(0);
	}

	switch(opcode)
	{
	case	INSERT:
		insert_entry(guidef_file, header, entry);
		break;
	case	REMOVE:
		remove_entry(guidef_file, header, entry);
		break;
	default:
		sprintf(Msg,"Invalid opcode: %d.", opcode);
		log_it(Msg);
		break;	

	}
}

int remove_entry(char *file, char *header, char *entry)
{
	int pid;
	char outfile[256];
	char	line[256];
	char	command[256];
	int 	entry_exists=0, entry_written=0, header_found=0;

	if (access(file, F_OK) != 0)
	{	
		/* No file so we don't need to remove anything */
		return(0);	
	}

	fp_in = fopen(file, "r");
	if (fp_in == NULL)
	{
		sprintf(Msg,"Failed to open <%s>. errno=%d", file, errno);
		log_it(Msg);
		return(-1);
	}

	pid=getpid();
	sprintf(outfile,"%s.%d", file, pid);   
	/* sprintf(outfile,"%s.2", file );  */
	
	fp_out = fopen(outfile, "w");
	if (fp_out == NULL)
	{
		sprintf(Msg,"Failed to open <%s>. errno=%d", outfile, errno);
		log_it(Msg);
		fclose(fp_in);
		return(-1);
	}
	
	while(fgets(line, sizeof(line)-1, fp_in))
	{
		if (line[strlen(line)-1] == '\n') line[strlen(line)-1]='\0';

/* gpwDEBUG	fprintf(stdout,"read: <%s> ", line); fflush(stdout); */

		/* It is a header */
		if (line[0] == '[')
		{
			if (strcmp(line, header) == 0)
			{
				/* This is the header we seek */
				header_found = 1;	
			}
			/* Always write the header */
			fprintf(fp_out, "%s\n", line);
/*gpwDEBUG		fprintf(stdout,"wrote header: <%s>\n", line);  */
			continue;
		}

		/* It's not a header */
		if (header_found)
		{
			if (strcmp(line,entry) == 0)
			{
				/* Header & entry sought; don't write */
				header_found=0;
				continue;
			}
		}
		fprintf(fp_out, "%s\n", line);
/*gpwDEBUG	fprintf(stdout,"wrote line: <%s>\n", line);  */
	}
	
	fclose(fp_in); 
	fclose(fp_out); 

	sprintf(command,"cp %s %s", outfile , file);
	system(command);
	sprintf(command,"rm %s", outfile);
	system(command);

	return(0);
}


int insert_entry(char *file, char *header, char *entry)
{
	int pid;
	char outfile[256];
	char	line[256];
	char	command[256];
	int 	entry_exists=0, entry_written=0, header_found=0;
		
	if (access(file, F_OK) != 0)
	{	
		/* create the file */
		fp_in = fopen(file, "w");
		fclose(fp_in);
	}

	fp_in = fopen(file, "r");
	if (fp_in == NULL)
	{
		sprintf(Msg,"Failed to open <%s>. errno=%d", file, errno);
		log_it(Msg);
		return(-1);
	}

	pid=getpid();
	/* sprintf(outfile,"%s.%d", file, pid);   */
	sprintf(outfile,"%s.2", file ); 
	
	fp_out = fopen(outfile, "w");
	if (fp_out == NULL)
	{
		sprintf(Msg,"Failed to open <%s>. errno=%d", outfile, errno);
		log_it(Msg);
		fclose(fp_in);
		return(-1);
	}

	while(fgets(line, sizeof(line)-1, fp_in))
	{
		if (line[strlen(line)-1] == '\n') line[strlen(line)-1]='\0';

/* gpwDEBUG	fprintf(stdout,"read: <%s> ", line); fflush(stdout); */

		/* It's not a header */
		if (line[0] != '[')
		{
			/* See if it is the entry we are looking to insert */
			if (strcmp(line,entry) == 0)
			{
				if (header_found)
				{
					entry_exists=1;
				}
			}
			if (strcmp(line,"") != 0 )
			{
				/* write it if it is not blank */
				fprintf(fp_out, "%s\n", line);
/* gpwDEBUG			fprintf(stdout,"wrote1: <%s>\n", line);  */
				continue;
			}
		} 
		
		/* It is a header */


		if (header_found)
		{
			/* This must be a subsequent header */
			if (! entry_written && ! entry_exists)
			{
				fprintf(fp_out, "%s\n", entry);
/* gpwDEBUG			fprintf(stdout,"wrote entry: <%s>\n",entry); */
				entry_written=1;
			}
			fprintf(fp_out, "%s\n", line); 
/* gpwDEBUG		fprintf(stdout,"wrote3: <%s>\n", line); fflush(stdout); */
		}
		else
		if (strcmp(line, header) == 0)
		{
			/* This is the header we seek */
			header_found = 1;	
			fprintf(fp_out, "%s\n", line);
/* gpwDEBUG			fprintf(stdout,"wrote4: <%s>\n", line); fflush(stdout); */
		}
		else
		/* It is some other header & we haven't yet found one we seek */
		{
			fprintf(fp_out, "%s\n", line);
/*gpwDEBUG		fprintf(stdout,"wrote5: <%s>\n", line); fflush(stdout); */
		}
	}

	fclose(fp_in);

	if ( entry_exists || entry_written )
	{
		;
	}
	else
	{
		if (! header_found)
		{
			fprintf(fp_out, "\n%s\n", header);
/* gpw DEBUG		fprintf(stdout,"wrote head: <%s>\n", header); fflush(stdout); */
		}
		fprintf(fp_out, "%s\n", entry);
/*gpwDEBUG	fprintf(stdout,"wrote entry2: <%s>\n", entry); fflush(stdout);*/
	}
	fclose(fp_out); 

	sprintf(command,"cp %s %s", outfile , file);
	system(command);
	sprintf(command,"rm %s", outfile);
	system(command);
	return(0);
}

int log_it(char *s)
{
	fprintf(stdout,"%s\n", s);
	fflush(stdout);
	return(0);
}

