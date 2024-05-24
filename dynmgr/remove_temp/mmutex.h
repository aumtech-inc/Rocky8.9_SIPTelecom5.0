/***************************************************************************
        File:           $Archive: /stacks/include/mmutex.h $ 
        Revision:       $Revision: 1.1 $
        Date:           $Date: 2006/04/10 16:24:42 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       CMMutex object header
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
#ifndef __CMMUTEX_H__
#define __CMMUTEX_H__

/** @file mmutex.h  */

#include "platform.h"

#if defined (_VXWORKS)
#include <semLib.h>
#include <semaphore.h>
#include <sysLib.h>
#elif defined (_SOLARIS) | defined(_LINUX) | defined(_PSOS)
#include <pthread.h>
#include <semaphore.h>
#elif defined (_DSPBIOSII)
extern "C" {
#include <sem.h>
}
#endif

/**
  * @brief provides mutex (mutual exclusion) support for process 
  * and thread synchronisation for shared data access. 
  *
  * It also provides a Boolean internal flag that can be set and reset.
  *
  * An anti-deadlocking mechanism is added in the destructor of CMMutex.
  *
  * See also CMGuard and CMUnguard
  */
class M_IMPORT CMMutex
{
private:
    /**
      flag. useful when mutexs are used to synchronise access to a bool variable.
      */
    MBOOL flag;

    /**
      number of threads trying to get access of the 
      same mutex-protected object at the same time,
      */
    int  waited_objects;    

protected:
#if defined(_WINDOWS)
    HANDLE  mutex;
#elif defined(_PSOS)
    unsigned long mutex;
#elif defined (_SOLARIS) | defined(_LINUX) | defined(_RTEMS)
    friend class CMCondition;
    friend class CMEvent;
    pthread_mutex_t mutex;
#elif defined(_VXWORKS)
    SEM_ID   mutex;
#elif defined(_DSPBIOSII)
    SEM_Handle mutex;
#elif defined(_SYMBIAN)
	RCriticalSection m_Mutex;
#else
#pragma message("mutex not defined on this platform")
#endif
#ifdef _LINUX
    int     old_cancel_state;
#endif

    unsigned int owner;


public:
    /**
      * Constructs a CMMutex object. An internal Boolean flag is reset to FALSE. 
      * The Mutex is created without being owned by the calling thread.
      */
    CMMutex();

    /**
      Destroys CMMutex object.
      */
    ~CMMutex();

    /**
      Requests ownership of the Mutex. 

      @return TRUE on success. FALSE otherwise.
      */
    MBOOL Lock(
#ifdef DCT_MUTEX_TRAP
                            MBOOL track = TRUE
#endif // DCT_MUTEX_TRAP
        );

    /**
      Releases ownership of the Mutex.

      @return TRUE on success. FALSE otherwise (most probably due to resource limitations).
      */
    MBOOL Release(   
#ifdef DCT_MUTEX_TRAP
                            MBOOL track = TRUE
#endif // DCT_MUTEX_TRAP
        );


    /**
      Sets the new status of the mutex by changing the internal Boolean flag to a new value "value" and returns the old value.

      Parameters
      MBOOL value

      @return Previous boolean value of the mutex status.
    */
    MBOOL Set(
        MBOOL value          ///<  New value of Boolean flag of Mutex.
    )
    { Lock(); MBOOL f = flag; flag = value; Release(); return f;}

    /**
      Returns the state of the mutex without blocking.

      @return Boolean flag indicating whether the mutex is set. 
     */
    MBOOL IsSet() {return flag;}

    /**
      Reflects whether any thread still waiting for this mutex

      @return True if any thread is waiting on this mutex
      */

    MBOOL Engaged() 
    { return waited_objects > 0; }
};


/**
  * @brief An implementation of the Classic Guard Pattern that acquires a mutex on construction,
  * and releases the mutex on destruction.
  * 
  * Typically used within scope to ensure mutex release on leaving scope.
  * 
  * See also CMUnguard
  */
class CMGuard
{
public:
    /**
      * Acquire the specified mutex until the CMGuard object is deleted, 
      * whereupon the mutex is released
      */
    CMGuard(CMMutex& theMutex) : myMutex(&theMutex) { myMutex->Lock(); }
    CMGuard(CMMutex* theMutex) : myMutex(theMutex) { myMutex->Lock(); }

    ~CMGuard(void) { myMutex->Release(); }
private:
    CMMutex* myMutex;
};


/**
  * @brief An inverse implementation of the CMGuard class. 
  *
  * This object will release the mutex
  * on constructions, and then re-acquire upon deletion
  * 
  * Typically used within scope to ensure mutex release on leaving scope.
  */

class CMUnguard
{
public:
    /**
      * Release the specified mutex until the CMGuard object is deleted, 
      * whereupon the mutex is re-acquired
      */
    CMUnguard(CMMutex& theMutex) : myMutex(&theMutex) { myMutex->Release(); }
    CMUnguard(CMMutex* theMutex) : myMutex(theMutex) { myMutex->Release(); }

    ~CMUnguard() { myMutex->Lock(); }
private:
    CMMutex* myMutex;
};

#endif // !__CMMUTEX_H__
