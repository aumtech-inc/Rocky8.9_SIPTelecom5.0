/***************************************************************************
        File:           $Archive: /stacks/include/mthread.h $
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       Thread class definition
        Copyright 1996-2003 by Dilithium Networks.
The material in this file is subject to copyright. It may not
be used, copied or transferred by any means without the prior written
approval of Dilithium Networks.
DILITHIUM NETWORKS DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL DILITHIUM NETWORKS BE LIABLE FOR
ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
****************************************************************************/
#ifndef __CMTHREAD_H__
#define __CMTHREAD_H__

#include "mmutex.h"
#include "mevent.h"
#include "mstring.h"

/** @file mthread.h
 */

#if defined(_SOLARIS) | defined(_LINUX) | defined(_PSOS)
#include <errno.h>
#include <pthread.h>
#elif (defined(_WINDOWS) || defined(WIN32)) & ! defined(UNDER_CE)
#include <process.h>
#endif // _WINDOWS | WIN32 & ! UNDER_CE

#if defined(_VXWORKS)
#include <taskLib.h>
#endif

#if defined(_DSPBIOSII)
extern "C" { 
#include <tsk.h>
}
#endif

/**
 * \enum MTHREADPRIORITY
 * This defines the priorities of thread within the system. 
 * These are mapped to operating system specific values.
 */

enum MTHREADPRIORITY
{
    MTHREAD_PRIORITY_IDLE,                  ///< thread is idle
    MTHREAD_PRIORITY_LOWEST,                ///< lowest running priority
    MTHREAD_PRIORITY_BELOW_NORMAL,          ///< below normal priority
    MTHREAD_PRIORITY_NORMAL,                ///< normal thread priority
    MTHREAD_PRIORITY_ABOVE_NORMAL,          ///< above normal priority
    MTHREAD_PRIORITY_HIGHEST,               ///< highest priority
    MTHREAD_PRIORITY_TIME_CRITICAL          ///< hard real-time
};

M_IMPORT unsigned int MGetThreadId(void);
M_IMPORT void MThreadCleanup(void);
M_IMPORT void MSleep(DWORD milsecs);

/**
  * \var MTHREADFUNCTION
  * defines a pointer to the thread <i>main</i> function as required by 
  * CMThread::CreateAndRun
  */
typedef  void (* MTHREADFUNCTION) (void * thread_arg);

#if defined(_SOLARIS) | defined(_LINUX) | defined(_RTEMS) | defined(_PSOS)
typedef     pthread_t       DCT_ThreadType;
#elif defined(_WINDOWS) & !defined(UNDER_CE)
typedef     unsigned long   DCT_ThreadType;
#elif defined(UNDER_CE)
typedef     HANDLE          DCT_ThreadType;
#elif defined(_VXWORKS)
typedef     int             DCT_ThreadType;        
#elif defined(_DSPBIOSII)
typedef     TSK_Handle      DCT_ThreadType;     
#endif

// Mutex list
struct MUTEXNODE                                            // Node for mutex list
{
    CMMutex*    m_pMutex;
    MUTEXNODE*  m_pNext;
};

/**
  * @brief Provides a common interface to threads in operating systems that support 
  * multithreading. 
  *
  * A thread created using CMThread has a CMMessageQueue message queue that it owns 
  * and that it can peek at and get messages from. Other threads (including the owner thread) can post 
  * messages to the thread but cannot peek at or get messages from its queue. 
  */
class M_IMPORT CMThread
{
friend
#if defined (_SOLARIS) | defined(_LINUX) | defined(_RTEMS) | defined(_PSOS)
  void*
#elif defined(_VXWORKS)
  int
#elif (defined(_WINDOWS) & ! defined (UNDER_CE))
  unsigned _stdcall
#elif defined(_DSPBIOSII)
  void
#elif defined(_SYMBIAN)
  TInt
#else
  DWORD WINAPI
#endif
        our_own_thread_routine(void * our_arg);
private:
    char                    myName[10];
protected:
    BOOL                    quit;
    CMSimpleMessageQueue    message_queue;
    CMEvent                 started_event;
    CMEvent                 ended_event; 
#if defined(_SOLARIS) | defined(_LINUX) | defined(_RTEMS) | defined(_PSOS)
    pthread_t               handle;
    pthread_attr_t          thread_attr;
#if defined(_PSOS)
public:
    unsigned long           tid;
protected:
#endif
#elif defined(_WINDOWS) & !defined(UNDER_CE)
    unsigned long           handle;
#elif defined(UNDER_CE)
    HANDLE                  handle;
#elif defined(_VXWORKS)
    int                     handle;
#elif defined(_DSPBIOSII)
    TSK_Handle              handle;
#elif defined(_SYMBIAN)
    RThread                 m_Thread;
#else 
    #error"Handle is not defined for this platform."
#endif
    MTHREADFUNCTION         routine;
    void *                  routine_parameter;
    int                     priority;
    long                    thread_num;
    unsigned int            thread_id;

public:
    // Get my own thread object
    static CMThread*        MyThread(void);

    /**
      *  Constructs a CMThread object.
      */
    CMThread();

    /**
      *  Destroys a CMThread object. The running thread is terminated or killed if it does not terminate within a certain time.
      */
    ~CMThread();

    /**
      *  @return Identification number of the thread.  This number is unique for each CMThread object and is different from MGetThreadId().
      */
    unsigned int GetId() {return thread_id;}

    /**
      *  @return TRUE if thread associated with object is running.  FALSE otherwise.
      */
    BOOL Running();

    /**
      *  Indicate if the thread has been request to terminate using the Quit() member function. 
      *  A thread should periodically use this function to see whether it should terminate
      *
      *  @return TRUE if the calling thread is being requested to terminate. 
      */
    BOOL  ShouldQuit() {return quit;} 
    
    /**
      *  Informs a thread that it should quit (terminate).
      *
      *  This helper function does not terminate the thread but rather sets a flag (an internal Mutex) 
      *  that the thread routine can poll using the ShouldQuit() function.
      */
    void Quit();

    /**
      *  Wait for a thread to quit, but then kill the thread if this does not occur within a specific time limit
      *
      * This function sets a flag that the thread routine would poll using the ShouldQuit() function and 
      * then posts a message to the thread (message id = 0xFFFFFFFF on 32-bit machines or 0xFFFFFFFFFFFFFFFF on 64-bit machines) 
      * in case it is waiting for a message.
      */
    void QuitOrDie(
        long waitfor = 100    ///< how long to wait before abnormally terminating the thread.  
                              ///< Default period of waiting time is 100ms
    );


    /**
      *  Wait for the thread to exit..
      *
      *  @return TRUE if the thread exited within the time period
      */
    BOOL    Wait(
        long waitfor = 1000    ///< Time to wait in milliseconds. Default period is 1 second
    );


    /**
      *  Creates and starts the thread, then waits until it signals it has started before returning. 
      *
      *  <EM>Warning:</EM>Under Windows 95/NT, this function fails if the requested stack size exceeds available memory
      * 
      * MTHREADFUNCTION is a thread function that should be defined as:
      * <p><br><code>void function_name(void *user_arg)</code>
      *
      * <b>Important:</b> The function cannot be a none static member function. Under Win32, this function 
      * should have a c_decl calling convention.
      * 
      * @return TRUE on success and FALSE on failure
      */
    // CreateAndRun() creates a thread and runs it. Note the user's routine is passed control from
    // our own routine which does some house keeping first.
#if defined(_ENABLE_EXTERNAL_THREAD_PRIO_SETTINGS)
    BOOL CreateAndRun(
        MTHREADFUNCTION  user_routine,      ///< The user routine.
        void *user_arg,                     ///< The user argument which will be passed as the first argument to the user routine above.
        int pri,                            ///< thread priority
        unsigned int stacksize=0,           ///< initial stack size (if required by host operating system)
        char *taskname=(char*)NULL          ///< descriptive name of thread (used for logging)
#else
    BOOL CreateAndRun(
        MTHREADFUNCTION  user_routine,      ///< The user routine.
        void *user_arg,                     ///< The user argument which will be passed as the first argument to the user routine above.
        MTHREADPRIORITY pri,                ///< see \ref MTHREADPRIORITY
        unsigned int stacksize=0,           ///< initial stack size (if required by host operating system)
        char *taskname=(char*)NULL          ///< descriptive name of thread (used for logging)
#endif
    );

    /**
      *  Sets the thread priority that the operating system allows.
      *
      *  On some platforms (i.e. Solaris) this function may not be available.
      *
      *  @return TRUE on success and FALSE on failure.
      */
#if defined(_ENABLE_EXTERNAL_THREAD_PRIO_SETTINGS)
    BOOL SetPriority(
        int pri       ///< Thread priority (see CreateAndRun())

    );
#else
    BOOL SetPriority(
        MTHREADPRIORITY pri     ///< see \ref MTHREADPRIORITY
    );
#endif


    /**
      *  Resumes thread if it was suspended.
      *
      *  @return TRUE on success and FALSE on failure.
      */
    BOOL Resume(void);

    /**
     *  Explicitly yields control to other threads.
     */
    void YieldProcess();

    /**
     *  @return the message queue for this thread
     */
    inline CMMessageQueue * MessageQueue() { return &message_queue; }

    /**
     *  see CMMessageQueue::WaitForMessage()
     */
    inline BOOL    WaitForMessage(DWORD millisecs=MINFINITE){return message_queue.WaitForMessage(millisecs);}

    /**
     *  see CMMessageQueue::Get()
     */
    inline CMMessage * Get(unsigned int filter_min=0, unsigned int filter_max=0){return message_queue.Get(filter_min, filter_max);}

    /**
     *  see CMMessageQueue::Peek()
     */
    inline CMMessage * Peek(BOOL remove, unsigned int filter_min=0, unsigned int filter_max=0){return message_queue.Peek(remove, filter_min, filter_max);}

    /**
     *  see CMMessageQueue::Post()
     */
    inline BOOL    Post(CMMessage * msg){return message_queue.Post(msg);}

    /**
     *  see CMMessageQueue::Post()
     */
    inline BOOL    Post(DWORD message_id, long p1 = 0, long p2 = 0, long p3 = 0, long p4 = 0){return message_queue.Post(message_id, p1, p2 , p3, p4);}

    /**
     *  see CMMessageQueue::Empty()
     */
    inline BOOL    IsEmpty() { return message_queue.IsEmpty();}

#ifdef DCT_MUTEX_TRAP
    DCT_ThreadType  Handle() {return handle;}                   // Read thread handle
    void  Handle(DCT_ThreadType p_Handle) {handle = p_Handle;}  // Set thread handle
#endif // DCT_MUTEX_TRAP
};

#ifdef DCT_MUTEX_TRAP
bool    MThreadMutexAdd(CMMutex*   p_pMutex);  // Add mutex to current threads list
bool    MThreadMutexDel(CMMutex*   p_pMutex);  // Del mutex from current threads list
bool    MThreadAdd(unsigned long p_ulTID);                                  // Add thread to thread list
bool    MThreadDelNL(unsigned long p_ulTID);                                // Del thread from thread list
#endif // DCT_MUTEX_TRAP

extern void    MThreadKillable(BOOL     p_zKillable, void* p_pvUndo);

extern void    MThreadKillable(void*    p_pvUndo);


#endif // !__CMTHREAD_H__
