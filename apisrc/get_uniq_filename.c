#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>


int get_uniq_filename(char  *filename)
{
char    template[20];
int     proc_id, rc;
long    seconds;

        filename[0] = '\0';
        proc_id = getpid();
        sprintf(filename, "%d", proc_id);
        time(&seconds);
        sprintf(template, "%ld", seconds);
        strcat(filename, &template[4]);/* append all but 1st 4 digits of tics */

        /* Check to see if file exists, if so, try again. */
        rc = access(filename, F_OK);
        if(rc == 0)
        {
                 get_uniq_filename(filename);
        }
        return(0);
}



