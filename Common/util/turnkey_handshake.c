/*------------------------------------------------------------------------------
Program:	turnkey_handshake.c
Purpose:	This file contains routines need to by turnkey applications
		to handshake with Responsibility so that they do not get 
		killed.
Author:		George Wenzel
Date:		07/28/99 
Update:	2000/10/07 sja 4 Linux: Added function prototypes
------------------------------------------------------------------------------*/
#include <ISPSYS.h>
#include <BNS.h>
#include <shm_def.h>

/*
 * Static Function Prototypes
 */
static int attach_shared_memory();
extern int write_fld();

/*------------------------------------------------------------------------------
Perform the handshake with Responsibility
------------------------------------------------------------------------------*/
int turnkey_handshake()
{
	int rc;
	extern int GV_shm_index;

	rc = attach_shared_memory();
	if ( rc != 0)
	{
		return(-1);
	}

	write_fld(tran_tabl_ptr, GV_shm_index, 5, 0);	

	rc = shmdt(tran_tabl_ptr); 
	if (rc == -1)
        {
        	fprintf(stdout, "Warning: failed to detach shared memory\n");
		fflush(stdout);
        }
	return(0);
}

/*------------------------------------------------------------------------------
Attach to shared memory
------------------------------------------------------------------------------*/
static int attach_shared_memory()
{
	static char	mod[] = "attach_shared_memory";

	int tran_shmid;

	tran_shmid = shmget(SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);

	tran_tabl_ptr = shmat(tran_shmid, 0, 0);
	if (tran_tabl_ptr == (char *) -1)
        {          
        	return(-1);
        }
	return(0);
} 
