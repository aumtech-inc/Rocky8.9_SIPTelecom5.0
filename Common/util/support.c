/*------------------------------------------------------------------------------
Program:	support.c
Purpose: 	Support modules for reading and writing into the shared 
		memory segment.
Author:		Mahesh Joshi
Date:		12/28/96
Update:		02/12/96 M. Joshi added this header
Update:		07/27/99 mpb added APPL_FIELD5 in read_arry, write_fld.
------------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include "shm_def.h"
/*------------------------------------------------------------------------------
This routine updates a column in the shared memory tables with the given values.
------------------------------------------------------------------------------*/
int write_arry(char *ptr, int ind, int stor_arry[])	/* This routine is to update shared memory */
{
int	i = 0;
char	tmp[30];
char	*schd_tabl_ptr;

if (ind < 0)
	{
	fprintf(stderr, "write_arry(ptr, %d, array): Invalid value/not supported\n", ind);
	return (0);
	}

schd_tabl_ptr = ptr;		/* save the pointer address */

switch(ind)
	{
	case	APPL_START_TIME:	/* application start time */
	case	APPL_RESOURCE:		/* application # */
	case	APPL_STATUS:		/* status of the application */
	case	APPL_PID:		/* proc id's of application */
	case	APPL_SIGNAL:		/* signal to the application */
		while(strlen(ptr) > 0)
			{
			ptr += vec[ind-1];   /* position the pointer */
			sprintf(tmp, "%*d", fld_siz[ind-1], stor_arry[i]);
			(void)memcpy(ptr, tmp, strlen(tmp));
					/* copy the value into the memory */
			ptr += (SHMEM_REC_LENGTH-vec[ind-1]);
					/* position pointer to next record */
			i++;
			}
		break;
	default	:
		fprintf(stderr, "write_arry(ptr, %d, array): index not supported\n", ind);
		break;
	}
ptr = schd_tabl_ptr;			/* restore the pointer */

return (0);
} /* write_arry */

/*------------------------------------------------------------------------------
This routine returns a desired column from the shared memory tables.
------------------------------------------------------------------------------*/
int read_arry(char *ptr, int ind, int stor_arry[])	/* This routine is used to read shared memory */
{
int	i = 0;
char	*schd_tabl_ptr;

if (ind < 0)
	{
	fprintf(stderr, "read_arry(ptr, %d, array): Invalid value/not supported\n", ind);
	return (0);
	}

schd_tabl_ptr = ptr;
switch(ind)
	{
	case	APPL_START_TIME:	/* application start time */
	case	APPL_RESOURCE:		/* application # */
	case	APPL_STATUS:		/* status of the application */
	case	APPL_PID:		/* proc id's of application */
	case	APPL_FIELD5:		/* turnkey update field */
	case	APPL_SIGNAL:		/* signal field for application */
		while(strlen(ptr) > 0)
			{
			ptr += vec[ind-1];	
				/* position the pointer to the field */
			sscanf(ptr, "%d", &stor_arry[i]); /* read the field */
			/* position the pointer to */
			ptr += (SHMEM_REC_LENGTH-vec[ind-1]);	 	
					/* the begining of next record */
			i++;
			}
		break;
	default	:
		fprintf(stderr, "read_arry(ptr, %d, array): index not supported\n", ind);
		break;
	}
ptr = schd_tabl_ptr;

return (0);
} /* read_arry */

/*----------------------------------------------------------------------------
This routine updates an field in the shared memory tables with the given value.
------------------------------------------------------------------------------*/
int write_fld(char *ptr, int rec, int ind, int stor_arry)	/* This routine updates a given field */
{
char	tmp[30];
char	*schd_tabl_ptr;

if (rec < 0 || ind < 0 || stor_arry < 0)
	{
	fprintf(stderr, "write_fld(ptr, %d, %d, %d): Invalid value/not supported\n", rec, ind, stor_arry);
	return (0);
	}
schd_tabl_ptr = ptr;
switch(ind)
	{
	case	APPL_API:		/* current api # */
	case	APPL_START_TIME:	/* application start time */
	case	APPL_NAME:		/* application name */
	case	APPL_STATUS:		/* status of the application */
	case	APPL_PID:		/* pid */
	case	APPL_FIELD5:		/* turnkey handshake */
	case	APPL_SIGNAL:		/* signal to the application */
		ptr += (rec*SHMEM_REC_LENGTH);
		/* position the pointer to the field */
		ptr += vec[ind-1];
		sprintf(tmp, "%*d", fld_siz[ind-1], stor_arry);
		(void)memcpy(ptr, tmp, strlen(tmp));
		break;
	default	:
		fprintf(stderr, "write_fld(ptr, %d, %d, %d): index not supported\n", rec, ind, stor_arry);
		break;
	}
ptr = schd_tabl_ptr;

return (0);
} /* write_fld */
/*-----------------------------------------------------------------------------
This routine returns the value of the desired field from the shared memory.
-----------------------------------------------------------------------------*/
int read_fld(char *ptr, int rec, int ind, char *stor_arry)	/* This routine updates a given field */
{
char	*schd_tabl_ptr;

if (rec < 0 || ind < 0)
	{
	fprintf(stderr, "read_fld(ptr, %d, %d, output): Invalid value/not supported\n", rec, ind);
	return (0);
	}
schd_tabl_ptr = ptr;
switch(ind)
	{
	case	APPL_START_TIME:	/* application start time */
	case	APPL_RESOURCE:		/* application # */
	case	APPL_API:		/* current api running */
	case	APPL_FIELD5:		/* reserve name */
	case	APPL_NAME:		/* application name */
	case	APPL_STATUS:		/* status of the application */
	case	APPL_PID:		/* proc_id of the application */
	case	APPL_SIGNAL:		/* signal to the application */
		ptr += (rec*SHMEM_REC_LENGTH);
		ptr += vec[ind-1];	/* position the pointer to the field */
		sscanf(ptr, "%s", stor_arry); /* read the field */
		break;
	default	:
		fprintf(stderr, "read_fld(ptr, %d, %d, output): index not supported\n", rec, ind);
		break;
	}
ptr = schd_tabl_ptr;

return (0);
} /* read_fld */

/******************************** eof() ************************************/
