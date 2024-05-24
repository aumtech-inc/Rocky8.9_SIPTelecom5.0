/***************************************************************************
        File:           $Archive: /stacks/include/mtimer.h $ 
        Revision:       $Revision: 1.1.1.1 $ 
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       timer class 
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
#ifndef __MTIMER_H__
#define __MTIMER_H__

#include "platform.h"
#include "mthread.h"
#include "mcondition.h"

/** @file mtimer.h
 */

#if defined(_DSPBIOSII)
extern "C" {
#include <tsk.h>
#include <clk.h>
}
#endif

#if defined(_VXWORKS)
#include <tickLib.h>
#include <taskLib.h>
#endif


extern M_IMPORT void  MDynamicSleep(DWORD dwMilsecs, DWORD &dwPrevSleepTimestamp, DWORD &dwExpectSleepTimestamp);


// forward definition
class M_IMPORT CMTimer;

/**
  * \var MTIMERCALLBACK
  * defines a pointer to a CMTimer callback function that takes one argument
  */
typedef void (* MTIMERCALLBACK)(CMTimer *, void *);

/**
  * \var MTIMERCALLBACK
  * defines a pointer to a CMTimer callback function that takes two arguments
  */
typedef void (* MTIMERCALLBACK2ARGS)(CMTimer *, void *, void *);

/**
  * \var MTIMERCALLBACK
  * defines a pointer to a CMTimer callback function that takes three arguments
  */
typedef void (* MTIMERCALLBACK3ARGS)(CMTimer *, void *, void *, void *);


#if defined(TIMER_TESTBED_ACTIVE)
typedef struct TimerArg
{
    int     nNumber;
    int     nPeriod;
    int     nLateVal;
    int     nActivated;
    int     nLateMean;
    int     pnLateTally[199];
    int     nMaxLate;
    int     nMinLate;
} TimerArg;
#endif // TIMER_TESTBED_ACTIVE


///////////////////////////////////////////////////////////////////////
// Using a single thread for management of CMTimer
///////////////////////////////////////////////////////////////////////

// Uncomment the following to use statistical drift recalculation
// (doesn't seem to improve things much in windows)
//#define USE_STATISTICAL_DRIFT

#define MTimerRunning                           CMTimer::Running

#if defined(USE_STATISTICAL_DRIFT)
#define MAX_TIMER_STATISTICS                    20

  // Approx guide for statistical drift (eg WIN32 usu 7ms late)
  #if defined(WIN32)
    #define INITIAL_TIMER_ADJUSTMENT            (-7)    // milliseconds
  #elif defined(_LINUX)
    #define INITIAL_TIMER_ADJUSTMENT            (-8)    // milliseconds
  #elif defined(_VXWORKS)
    #define INITIAL_TIMER_ADJUSTMENT            (-1)    // milliseconds
  #else
    #define INITIAL_TIMER_ADJUSTMENT            0       // milliseconds
  #endif // WIN32
#endif // USE_STATISTICAL_DRIFT

#define PERIODIC_TIMER_THREAD_CREATE_THRESHOLD  1000    // milliseconds


typedef struct RUNNINGTIMERREC__
{
    struct RUNNINGTIMERREC__*   next;
    CMTimer*                    timer;
} RUNNINGTIMERS;

/**
  * @brief Provides general timer support. 
  *
  * The resolution of the timer depends on the operating system’s thread context switching. 
  * The CMTimer callback function is implemented using a thread that calls the user supplied function. 
  */
class M_IMPORT CMTimer
{
friend int timer_management_thread(void *p_pUser);
friend int user_callback_thread(void *p_pUser);

//---------------------------------------------------------------------
// STATIC Member functions / variables
//   - Used for global manipulations of the timer class and associated
//     list.
//---------------------------------------------------------------------
private:
    static CMMutex*         m_pListAccess;
    static CMCondition*     m_pCondition;
    static RUNNINGTIMERS*   m_pHeadList;
    static CMThread*        m_pThread;
#if defined(USE_STATISTICAL_DRIFT)
    static int              m_pnStats[MAX_TIMER_STATISTICS];
    static int              m_nStatIndex;
    static int              m_nStatAdjustment;
#endif // USE_STATISTICAL_DRIFT

protected:
    static BOOL             Initialize();


public:
    static BOOL             DestroyManagerThread();

    static void             RunningListLock();
    static void             RunningListRelease();

    static BOOL             AddToTimersListNoLock(CMTimer* p_pTimer);
    static BOOL             AddToTimersList(CMTimer* p_pTimer);
    static void             DeleteFromTimersListNoLock(CMTimer* p_pTimer);
    static BOOL             MoveDownTimersListNoLock(CMTimer* p_pTimer);                // Periodic timers get move down list iso removed
    static BOOL             MoveDownTimersList(CMTimer* p_pTimer);                      // Periodic timers get move down list iso removed

    static CMMutex*         ListAccess()        { return m_pListAccess; }
    static CMCondition*     ListCondition()     { return m_pCondition; }
    static RUNNINGTIMERS*   ListHead()          { return m_pHeadList; }
    static CMThread*        ManagementThread()  { return m_pThread; }

#if defined(USE_STATISTICAL_DRIFT)
    static int              StatAdjustment()    { return m_nStatAdjustment / MAX_TIMER_STATISTICS; }
    static void             StatRecalculation(CMTimer *p_pTimer);
#endif // USE_STATISTICAL_DRIFT

    BOOL                    RunningNoLock() const;

    DWORD                   start_time;
    DWORD                   end_time;
    DWORD                   scheduled_time;
    BOOL                    m_zQuit;

    class CMThread          thread_user_callback;
    class CMCondition       timer_lapsed;

//---------------------------------------------------------------------

private:
    MTIMERCALLBACK          callback;
    void*                   user_arg;
    void*                   user_arg2;
    void*                   user_arg3;
    int                     num_arg;
    int                     periodic;
    DWORD                   millisecs;

public:

    /**
      *  Constructs a CMTimer object.
      */
    CMTimer();

    /**
      *  Destroys the CMTimer object. Running timer is killed.
      */
    ~CMTimer();

    /**
      *  Starts a timer and lets it run for "millisecs" milliseconds. 
      *  This form of starting a timer is not intended for implementing callbacks at the end of the timer (alarm time). 
      *  It is meant for starting it and stopping it and retrieving lapsed time using the CMTimer:: Lapsed() function below.
      *
      * @return TRUE on success and FALSE on failure.
      */

    BOOL                    Start(
        DWORD millisecs                 ///< The length of time (in milliseconds) the timer should run
    );
    
    //@{
    /**
      *  Start a timer that calls a callback function when the specified time has elapsed.
      *  The callback can be called once only after the specified intervcal, or continuously at the specified interval 
      *  until the timer is stopped. 
      *
      *  <EM>Warning:</EM> Care should be taken in the user routine if a periodic timer is selected as the current implementation of
      *  CMTimer creates a new thread for each invocation of the callback function when the period is greater than or equal to 
      *  PERIODIC_TIMER_THREAD_CREATE_THRESHOLD, which has the default of 1000ms. If any callback function takes
      *  longer than the periodic timer interval, then there may be multiple simulataneous invocations of the callback 
      *  function in seperate threads. This behaviour may be changed in future releases of CoreAPI
      *
      *  Start a timer that calls a callback function when the specified time has elapsed.
      *  The callback can be called once only after the specified intervcal, or continuously at the specified interval 
      *  until the timer is stopped. 
      *
      * @return TRUE on success and FALSE on failure.
      */
    BOOL                    Start(
        DWORD millisecs,                ///< The length of time (in milliseconds) the timer should run before calling the user routine.
        MTIMERCALLBACK user_routine,    ///< The user routine.
        void * user_arg,                ///< The first user argument which will be passed as the first argument to the user routine.
        BOOL periodic                   ///< If TRUE a periodic timer is started which will repeatedly call the user routine at the specified timer interval
                                        ///< If FALSE, the user routine is called once only
    );
    BOOL                    Start2Args(
        DWORD millisecs,                  ///< The length of time (in milliseconds) the timer should run before calling the user routine.
        MTIMERCALLBACK2ARGS user_routine, ///< The user routine.
        void * user_arg1,                 ///< The first user argument which will be passed as the first argument to the user routine.
        void * user_arg2,                 ///< The second user argument which will be passed as the first argument to the user routine.  
        BOOL periodic                     ///< If TRUE a periodic timer is started which will repeatedly call the user routine at the specified timer interval
                                          ///< If FALSE, the user routine is called once only
    );
    BOOL                    Start3Args(
        DWORD millisecs,                  ///< The length of time (in milliseconds) the timer should run before calling the user routine.
        MTIMERCALLBACK3ARGS user_routine, ///< The user routine.
        void * user_arg1,                 ///< The first user argument which will be passed as the first argument to the user routine.
        void * user_arg2,                 ///< The second user argument which will be passed as the first argument to the user routine.
        void * user_arg3,                 ///< The third user argument which will be passed as the first argument to the user routine.  
        BOOL periodic                     ///< If TRUE a periodic timer is started which will repeatedly call the user routine at the specified timer interval
                                          ///< If FALSE, the user routine is called once only  
    );
    //@}


    //@{
    /**
      *  Changes the timer interval after the timer has been started.
      */
    BOOL                    SetFunction(
        DWORD millisecs                   ///< The length of time (in milliseconds) the timer should run
    );

    /**
      *  Changes the timer interval and the callback function after the timer has been started.
      *
      * See CMTimer::Start for more information
      */
    BOOL                    SetFunction(    
        DWORD millisecs,                  ///< The length of time (in milliseconds) the timer should run  
        MTIMERCALLBACK user_routine,    ///< The user routine.
        void * user_arg,                ///< The first user argument which will be passed as the first argument to the user routine.
        BOOL periodic                   ///< If TRUE a periodic timer is started which will repeatedly call the user routine at the specified timer interval
                                        ///< If FALSE, the user routine is called once only
    );
    BOOL                    SetFunction2Args(
        DWORD millisecs,                  ///< The length of time (in milliseconds) the timer should run
        MTIMERCALLBACK2ARGS user_routine, ///< The user routine.
        void * user_arg1,                 ///< The first user argument which will be passed as the first argument to the user routine.
        void * user_arg2,                 ///< The second user argument which will be passed as the first argument to the user routine.  
        BOOL periodic                     ///< If TRUE a periodic timer is started which will repeatedly call the user routine at the specified timer interval
                                          ///< If FALSE, the user routine is called once only
    );
    BOOL                    SetFunction3Args(
        DWORD millisecs,                  ///< The length of time (in milliseconds) the timer should run   
        MTIMERCALLBACK3ARGS user_routine, ///< The user routine.
        void * user_arg1,                 ///< The first user argument which will be passed as the first argument to the user routine.
        void * user_arg2,                 ///< The second user argument which will be passed as the first argument to the user routine.
        void * user_arg3,                 ///< The third user argument which will be passed as the first argument to the user routine.  
        BOOL periodic                     ///< If TRUE a periodic timer is started which will repeatedly call the user routine at the specified timer interval
                                          ///< If FALSE, the user routine is called once only  
    );
    //@}

    /**
      *  Kills the currently running timer.
      *
      *  @return TRUE on success and FALSE on failure, usually because the timer is not running.
      */
    BOOL                    Kill();

    /**
      *  Checks whether the timer is still running (end-time was not reached).
      *
      *  TRUE if the timer is running.  FALSE otherwise.
      */
    BOOL                    Running() const;

#if defined(_ENABLE_EXTERNAL_THREAD_PRIO_SETTINGS)
    BOOL                    SetPriority(int pri);
#else // !_ENABLE_EXTERNAL_THREAD_PRIO_SETTINGS
    BOOL                    SetPriority(MTHREADPRIORITY pri);
#endif // _ENABLE_EXTERNAL_THREAD_PRIO_SETTINGS

    /**
     * Returns the lapsed time between the start of the timer and the timer being killed.  
     *
     * Note this function DOES NOT return the lapsed time for a running timer. 
     * It is meant to be used with CMTimer::Kill() to kill the timer before the lapsed 
     * time can be determined.
     *
     * @return 0 if the timer has not been started or is running without being killed.   
     */
    inline DWORD            Lapse()
                            {
#if defined(_WINDOWS) | defined(_SOLARIS) | defined(_LINUX) | defined(_RTEMS) | defined(_SYMBIAN)
                                return end_time - start_time;
#elif defined(_VXWORKS) | defined (_DSPBIOSII) | defined(_PSOS)
                                return MGetMilSecLapse() - start_time;
#else
#pragma message("CMTimer::Lapse not defined on this platform")
#endif // _WINDOWS) | _SOLARIS | _LINUX | _RTEMS
                            }

    DWORD                   Scheduled() { return scheduled_time; }
};

#endif // !__MTIMER_H__
