/***************************************************************************
        File:           $Archive: /stacks/include/mevent.h $ 
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Albert Wong
        Synopsys:       Event Object
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
#ifndef __CMEVENT_H__
#define __CMEVENT_H__

/** @file mevent.h  */

/**
  * @brief The event class CMEvent implements basic event signaling between threads. 
  *
  * For example, a thread (CMThread) could block waiting for an event to be signalled and then continue its processing.
  * The event is broadcast to all waiting threads. I.e. if more than one thread is waiting for the same event 
  * at the same event, all threads will receive the signal once the event is set.
  *
  * CMEvent behaves as follows:  
  *   -# The event is initially in non-signalled state when created.
  *   -# Once the event is set, it will be in the signalled state and will stay in that state 
  *      until it is reset. This is independent of its previous signaling state.  I.e. when 
  *      there are one or more threads waiting for the same event, all the threads will receive 
  *      the signal and continue execution.  However, if no thread is waiting for the thread, nothing 
  *      will happen except switching the state to signalled state.
  *   -# When the event is reset, it will change the signal to non-signalled state. If there are one or 
  *      more threads waiting for the same event, they will remain waiting, unless their timeout expires.
  *   -# Before the event is destroyed, it is set to signalled state, independent of its previous signaling state.  
  *      Therefore, are pending threads will be released first before the event is destroyed.
  *
  * The correct operation of the CMEvent can be verified by using our TestCoreAPI application, which simulates 
  * the following four testing scenarios:
  *
  *   -# Two threads wait for one signal (Signal 1: Task 1 and Task 3 wait for signaling from Task 2 with an infinite timeout.)
  *   -# Two threads signal one waiting thread. Another thread tries to wait for the same signal after a short while. 
  *      The last thread should experience timeout (here it is 3 seconds).  (Signal 2: Task 1 and Task 3 signal Task 2 
  *      simultaneously.   Then Task 4 attempts to wait for the same signal and still gets it.)
  *   -# One thread resets signal and waits.  Another thread resets signal and sets signal after a while. First thread 
  *      should continue to wait without changing its timeout and then get the signal within its timeout (here it is 10 seconds). 
  *      (Signal 3: Task 1 gets signal even it is reset by Task 3 during the waiting time.)
  *   -# One thread sets signal.  Another thread resets and then waits for the signal. 2nd thread should TIMEOUT (here it is 10 seconds).  (Signal 4: Task 1 sets signal but Task 3 resets it and then waits.)
  *
  * For further illustration of the testing scenario, the expected result from TestCoreAPI is shown in the excerpt below:
  *
  *
  *    TestEvents Starting....<BR>
  *    \<\<\< SCENARIO 1 \>\>\>  <BR>
  *    taskSig1 is waiting for sig1 indefinitely... <BR>
  *    taskSig2 is about to set sig1 after 3 sec...<BR>
  *    taskSig3 is waiting for sig1 indefinitely...<BR>
  *    taskSig2 is to set sig1 and will wait for sig2.<BR>
  *    taskSig1 has received sig1 and will set sig2.<BR>
  *    taskSig3 has received sig1 and will set sig2.<BR>
  *    \<\<\< SCENARIO 2 \>\>\><BR>
  *    taskSig1 has set sig2.<BR>
  *    taskSig2 has received sig2.<BR>
  *    taskSig2 finishing.<BR>
  *    taskSig3 has set sig2.<BR>
  *    taskSig4 is waiting for sig2 with timeout of 3 seconds...<BR>
  *    taskSig4 has received sig2.<BR>
  *    taskSig4 finishing.<BR>
  *    \<\<\< SCENARIO 3 \>\>\><BR>
  *    taskSig1 has reset (from creation) sig3.<BR>
  *    taskSig1 is waiting for sig3 with timeout of 10 sec...<BR>
  *    taskSig1 has received sig3.<BR>
  *    taskSig3 has reset sig3.<BR>
  *    taskSig3 has set sig3.<BR>
  *    \<\<\< SCENARIO 4 \>\>\><BR>
  *    taskSig1 has set sig4.<BR>
  *    taskSig1 finishing.<BR>
  *    taskSig3 has reset sig4.<BR>
  *    taskSig3 is waiting for sig4 with timeout of 10 sec...<BR>
  *    taskSig3 cannot receive sig4 after timeout of 10 sec or with error.<BR>
  *    taskSig3 finishing.<BR>
  *    TestEvents finished.<BR>
  *
  * Note that the print statements might be out of order due to the scheduling of the threads.  
  * For example, in Scenario 3 above, although it shows that sig3 is received by taskSig1 before 
  * it is set by taskSig3, it may actually happen after that.
  */

#if defined(USE_CONDITION_VARIABLES)

#include "mcondition.h"


///////////////////////////////////////////////////////////////////////
// New version of Events (Uses condition variables)                  //
///////////////////////////////////////////////////////////////////////

class M_IMPORT CMEvent 
{
protected:
    CMCondition     condition;
    CMMutex         mutex;

public:
    CMEvent();
    ~CMEvent();
    BOOL Set();
    BOOL Wait(DWORD millisecs=MINFINITE);
    BOOL Reset();             
    BOOL Register();
};

#else // !USE_CONDITION_VARIABLES

///////////////////////////////////////////////////////////////////////
// Old version of Events (Does not use condition variables)          //
///////////////////////////////////////////////////////////////////////

#include "platform.h"

#if defined(_VXWORKS)
//#define _USE_SIGNAL
#define _USE_SEMAPHORE      // use semaphore to implement the signal.  comment it will use message queue instead.
    // NOTE:  As VxWorks OS does not have event function, implementation of
    //   event makes use of both mutex and message queue
    //   The following two header files are required for event 
    //   implementation.
#ifdef _USE_SIGNAL
#include "sysLib.h"
#include "mmutex.h"
typedef struct MTASKIDLIST
{
    struct MTASKIDLIST *next;
    int id;
} MTASKIDLIST;
#elif defined(_USE_SEMAPHORE)
#include "semLib.h"
#include "sysLib.h"
#include "mmutex.h"
#else
#include "mmutex.h"
#include <msgQLib.h>
#endif // _USE_SEMAPHORE
#endif

#if defined (_DSPBIOSII)
extern "C" {
#include <mbx.h>
}
#endif

class M_IMPORT CMEvent 
{
protected:
#if defined(_WINDOWS)
    HANDLE handle;
#elif defined(_PSOS)
    unsigned long cond;
    unsigned long mutex;
    BOOL        signalled;
#elif defined(_SOLARIS) | defined(_LINUX) | defined(_RTEMS)
    pthread_cond_t  cond;
    pthread_mutex_t mutex;
    BOOL            signalled;
#elif defined(_VXWORKS)
#ifdef _USE_SIGNAL
    CMMutex     access;
    inline void        Lock(){access.Lock();}
    inline void        Release(){access.Release();}
    MTASKIDLIST *list;
    MTASKIDLIST *last;
    int         num_waiting;
    BOOL        AddToList(int id);
    void        ClearList();
    BOOL        signalled;
#elif defined(_USE_SEMAPHORE)
    SEM_ID      mutex;
    BOOL        event_set;  // flag to indicate whether there is a set event
    CMMutex     Smutex;     // set mutex
#else
    int MAXMSG;
    int MAXMSGLEN;
    char *MSG;
    int MSGLEN;
    CMMutex     Cmutex;     // counting mutex
    CMMutex     Emutex;     // event mutex
    CMMutex     Smutex;     // set mutex
    MSG_Q_ID    msgQId;
    BOOL        event_detected;      // flag for 1st time msgQReceive
    BOOL        msg_present;     // flag for checking status of message queue

    int     count;          // wait for event count
    void CountInc(){Cmutex.Lock(); count++; Cmutex.Release();}
    void CountDec(){Cmutex.Lock(); count--; Cmutex.Release();}
    void CountClr(){Cmutex.Lock(); count=0; Cmutex.Release();}
    int  Count(){int i; Cmutex.Lock(); i = count; Cmutex.Release(); return i;}
#endif // _USE_SEMAPHORE
#elif defined(_DSPBIOSII)
    int             MAXMSGLEN;
    int             mbxlength;
    char            *MSG;
    MBX_Handle      mbx;
    int             count;
#elif defined(_SYMBIAN)
    RCriticalSection m_Mutex;
    bool             m_zSignalled;
    struct WaitingThread {
        TThreadId        m_WaitingThreadId;
        TRequestStatus   m_RequestStatus;
        WaitingThread *  m_pLink;
    } * m_pWaitingThreads;
#else
    echo "class not implemented on this platform";
#endif

public:
    /** 
     * Constructs an event signaling object.
     */
    CMEvent();

    /** 
     * Destroys an event object.
     */
    ~CMEvent();

    /** 
     * Sets an event object to signalled state (threads waiting on such event will wake-up).  
     * The event will be in signalled mode until Reset() is called.
     *
     * @return TRUE on success and FALSE on failure. Failure could result from resource limitations of the system.
     */
    BOOL Set();

    /** 
     * Waits for an event to be signalled. Calling thread will sleep until either the event 
     * object is signalled or millisecs time has lapsed.
     *
     * @return TRUE if Wait() was terminated by the event being signalled, and FALSE if time-out has occurred
     */
    BOOL Wait(
        DWORD millisecs=MINFINITE   ///< Amount of time to wait for event to be signalled. When lapsed, the function returns 
                                    ///< FALSE. If event is signalled, the function returns TRUE. The default value is MINFINITE, which 
                                    ///< indicates an infinite block until event is signalled.
    );

    /** 
     * Resets the state of an event to the non-signalled state. An event that is created with "manual reset" mode 
     * should be reset before being signalled again using Set(). 
     *
     * @return TRUE on success.  FALSE otherwise.
     */
    BOOL Reset();             

    /** 
     * Adds a task ID to the task list.
     *
     * @return TRUE on success. FALSE otherwise.
     */
    BOOL Register();
};

#endif // USE_CONDITION_VARIABLES

#endif // !__CMEVENT_H__
