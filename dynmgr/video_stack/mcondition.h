/***************************************************************************
        File:           $Archive: /stacks/include/mcondition.h $
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       Condition Object
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
#ifndef __CMCONDITION_H__
#define __CMCONDITION_H__

/** @file mcondition.h  */

#include "platform.h"
#include "mmutex.h"
#include "mevent.h"


#if defined(_VXWORKS)
//#define _USE_SIGNAL
#define _USE_SEMAPHORE      // use semaphore to implement the signal.  comment it will use message queue instead.
    // NOTE:  As VxWorks OS does not have event function, implementation of
    //   event makes use of both mutex and message queue
    //   The following two header files are required for event 
    //   implementation.
#ifdef _USE_SIGNAL
#include "sysLib.h"
typedef struct MTASKIDLIST
{
    struct MTASKIDLIST *next;
    int id;
} MTASKIDLIST;
#elif defined(_USE_SEMAPHORE)
  #include "semLib.h"
  #include "sysLib.h"
#else
  #include <msgQLib.h>
#endif // _USE_SEMAPHORE
#endif // _VXWORKS

#if defined (_DSPBIOSII)
extern "C" {
  #include <mbx.h>
}
#endif


class M_IMPORT CMCondition
{
protected:
#if defined(_WINDOWS)
    HANDLE              handle;
#elif defined(_SOLARIS) | defined(_LINUX) | defined(_RTEMS) | defined(_PSOS)
    pthread_cond_t cond;
    BOOL        signalled;
#elif defined(_VXWORKS)
  #ifdef _USE_SIGNAL
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
    CMEvent m_Event;
#else
    echo "class not implemented on this platform";
#endif

public:
    CMCondition();
    ~CMCondition();

    BOOL Set();
    BOOL Wait(CMMutex* mutex, DWORD millisecs=MINFINITE);
    BOOL Reset();             
    BOOL Register();
};

//
// Disable thread pre-emption
//
void    MTaskLock();

//
// Enable thread pre-emption
//
void    MTaskUnlock();


#endif // !__CMEVENT_H__
