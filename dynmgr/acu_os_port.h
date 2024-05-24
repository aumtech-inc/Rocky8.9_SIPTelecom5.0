/*****************************************************************************\
*
* Product:  Aculab SDK
* Filename: acu_os_port.h
*
* Copyright (C)2003 Aculab Plc
*
* This file provides a library of functions that hide operating system specifics 
* behind a platform-independent interface.
*
* This file is provided to make the Aculab example code clearer.  It is an example
* of how to write code using Aculab's API libraries.  Customers are free to write
* their applications in any way they choose.
*
* PLEASE NOTE: This file is NOT SUPPORTED by Aculab.  Do not use it in your
* applications unless you understand it!
*
\*****************************************************************************/
#ifndef ACU_OS_PORT_H_INCLUDED
#define ACU_OS_PORT_H_INCLUDED

#ifndef WIN32
	//  Needed for exit()
#	include <stdlib.h>
	//  Needed for memset()
#	include <string.h>
	//  Needed for inet_addr()
#	include <arpa/inet.h>
#endif

#include <acu_type.h>
//#include <os_port.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Miscellaneous
 */

typedef int ACU_OS_BOOL;
typedef int ACU_OS_ERR;

/* boolean types */
#define ACU_OS_TRUE		1
#define ACU_OS_FALSE	0

/* errors */
#define ERR_ACU_OS_NO_ERROR		0
#define ERR_ACU_OS_NO_MEMORY	1 /* out of memory */
#define ERR_ACU_OS_SIGNAL		2 /* OS interrupted a blocking operation (e.g. because of a signal) */
#define ERR_ACU_OS_TIMED_OUT	3 /* wait finished without anything happening */
#define ERR_ACU_OS_PARAMETER	4 /* function given invalid parameter */
#define ERR_ACU_OS_ERROR		5 /* unexpected OS error occurred */
#define ERR_ACU_OS_WRONG_STATE	6 /* thread is in the wrong state for request */


/*
 * Thread functions 
 */

typedef struct _ACU_OS_THREAD ACU_OS_THREAD; /* opaque structure to represent a thread */
typedef int (*ACU_OS_THREAD_FN)(void* parameters); /* prototype for thread functions */

typedef enum _ACU_OS_THREAD_PRIORITY
{
	ACU_OS_THREAD_PRIORITY_LOW	=	0,
	ACU_OS_THREAD_PRIORITY_NORMAL =	1,
	ACU_OS_THREAD_PRIORITY_HIGH	=	2,
} ACU_OS_THREAD_PRIORITY;


ACU_OS_THREAD* acu_os_create_thread(ACU_OS_THREAD_FN thread_func, void* parameters); /* create a thread */
ACU_OS_ERR acu_os_destroy_thread(ACU_OS_THREAD* thread); /* clean up after a thread has terminated */
ACU_OS_ERR acu_os_wait_for_thread(ACU_OS_THREAD* thread); /* block until a thread halts */
ACU_OS_ERR acu_os_set_thread_priority(ACU_OS_THREAD* thread, ACU_OS_THREAD_PRIORITY new_priority); /* change a thread's priority*/
ACU_OS_ERR acu_os_get_thread_exit_code(ACU_OS_THREAD* thread, int* exit_code); /* get a thread's exit code */


/*
 * Synchronisation functions
 */

typedef struct _ACU_OS_CRIT_SEC ACU_OS_CRIT_SEC; /* opaque structure to represent a critical section */

ACU_OS_CRIT_SEC* acu_os_create_critical_section(void); /* allocate a critical section */
void acu_os_destroy_critical_section(ACU_OS_CRIT_SEC* crit_sec); /* free a critical section */
ACU_OS_BOOL acu_os_lock_critical_section(ACU_OS_CRIT_SEC* crit_sec); /* lock a critical section */
ACU_OS_BOOL acu_os_unlock_critical_section(ACU_OS_CRIT_SEC* crit_sec); /* unlock a critical section */


/* 
 * wait object functions 
 */



typedef struct _ACU_SDK_WAIT_OBJECT ACU_SDK_WAIT_OBJECT; /* opaque structure to represent a wait object */
#define ACU_OS_INFINITE -1 /* infinite wait */	
 
ACU_SDK_WAIT_OBJECT* acu_os_create_wait_object(void); /* create a new wait object */
ACU_SDK_WAIT_OBJECT* acu_os_convert_wait_object( ACU_WAIT_OBJECT wait_object );
void acu_os_destroy_wait_object(ACU_SDK_WAIT_OBJECT* wait_object); /* destroy a wait object */
ACU_OS_ERR acu_os_wait_for_wait_object(ACU_SDK_WAIT_OBJECT* wait_object, int timeout); /* Wait for a single wait object to be signalled */
ACU_OS_ERR acu_os_wait_for_any_wait_object(unsigned int object_count, ACU_SDK_WAIT_OBJECT* objects[], ACU_OS_BOOL* signalled, int timeout);
ACU_OS_ERR acu_os_signal_wait_object(ACU_SDK_WAIT_OBJECT* wait_object); /* signal a wait object */
ACU_OS_ERR acu_os_unsignal_wait_object(ACU_SDK_WAIT_OBJECT* wait_object); /* unsignal a wait object */

/* Allow conversion from Prosody events.  This relies on the Prosody headers having been included previously */
#if defined TiNGTYPE_WINNT || defined TiNGTYPE_LINUX || defined TiNGTYPE_SOL
#include <smdrvr.h>
ACU_SDK_WAIT_OBJECT* acu_os_create_wait_object_from_prosody(tSMEventId prosody_event); /* create a new wait object based on an existing Prosody event */
#endif


ACU_OS_ERR acu_os_sleep(unsigned int time); /* sleep for a number of milliseconds */


/* 
 * I/O functions 
 */

ACU_OS_ERR acu_os_initialise_screen(void); /* initialise the screen to allow us to position the cursor */
ACU_OS_ERR acu_os_uninitialise_screen(void); /* release the screen buffer */
void acu_os_printf_at(int x, int y, char* fmt, ...); /* print text at the specified location */
ACU_OS_ERR acu_os_clear_scr(void); /* clear the screen */
void acu_os_set_cursor_pos( int x, int y );	/* Set the cursor position */

char acu_os_get_next_keypress(void); /* return the next available keypress */

/*
 * Memory functions
 */

void* acu_os_alloc(size_t size); /* allocate size bytes */
void acu_os_free(void* mem); /* release memory allocated with acu_os_alloc() */

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif

