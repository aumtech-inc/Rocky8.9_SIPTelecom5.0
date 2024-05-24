/***************************************************************************
        File:           $Archive: /common/coreapi/include/mobject.h $ 
        Revision:       $Revision: 1.1 $
        Date:           $Date: 2006/04/10 16:24:42 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       CMObject definition
        Copyright 1997-2003 by Dilithium Networks.
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
#ifndef __MOBJECT_H__
#define __MOBJECT_H__

/** @file mobject.h */

#include "platform.h"
#if !defined(_SINGLETHREADED)
#include "mmutex.h"
#endif


// Components
// IMPORTANT: shall update list in MObjectName() when this list is updated
// IMPORTANT: should append new components at the end of the list only!
enum MOBJECTID
{
    MMEDIA_UNSUPPORTED=-1,
    MMEDIA_NULLDATA=0,
    MMEDIA_NONSTANDARD,
    MUNKNOWN_OBJECT,

    // Comm devices
    MMODEM,
    MTCP,
    MUDP,
    MRTP,
    MRTDX,
    MISDN,
    MUART,

    // System standards
    MH320,
    MH323,
    MH324,
    MH324I,
    MGATEWAY,

    // Multiplexers
    MH221,
    MH223,
    MH2250,

    // Call signaling
    MQ931,
    MV140,

    // Command and control
    MH242,
    MH242TX,
    MH242RX,
    MH243,
    MH245,
    MH245H242XC,

    // Video codecs
    MH261,
    MH262,
    MH263,
    MVIDEO_NONSTANDARD,
    MVIDEO_IS11172,
    MMPEG4VIDEO,
    MH263H261XC,
    MH261H263XC,

    // Audio codecs
    MAUDIO_NONSTANDARD,
    MG7231,
    MG711,                          // MG711 centralises ALAW and ULAW with 56K and 64K rate 
    MG711ALAW64K,
    MG711ALAW56K,
    MG711ULAW64K,
    MG711ULAW56K,
    MG722_64K,
    MG722_56K,
    MG722_48K,
    MG728,
    MG729,
    MG729_ANNEXA,
    MAUDIO_IS11172,
    MAUDIO_IS13818,
    MG729_ANNEXASS,
    MG729_ANNEXAWB,
    MG7231_ANNEXC,
    MGSM_FULL_RATE,
    MGSM_HALF_RATE,
    MGSM_ENHANCED_RATE,
    MGSMAMR,
    MG729_EXT,
    MG7231G711XC,
    MG711G7231XC,

    // Data
    MDATA_DAC_NONSTANDARD,
    MDATA_DAC_DSVDCNTRL,
    MDATA_DAC_T120,
    MDATA_DAC_DSM_CC,
    MDATA_DAC_USER_DATA,
    MDATA_DAC_T84,
    MDATA_DAC_T434,
    MDATA_DAC_H224,
    MDATA_DAC_NLPD,
    MDATA_DAC_H222_DATA_PARTITIONING,

    // Encryption
    MENCRYPTION_NONSTANDARD,
    MH233_ENCRYPTION,
    MMEDIAPROCESSOR,
    MUSER_OBJECT,
    MSW_OBJECT,
    MHW_OBJECT,
    MCONNECTION_OBJECT,

    // Channel
    MCHANNEL,

    // System
    MH32XAPP,
    MH320APP,
    MH323APP,
    MH324APP,

    // Media devices
    MUNKNOWN_MEDIA,
    MVIDEOCAPTURE,
    MYUV422FILE,
    MYUV422MEM,
    MMSWINDOW,
    MMSAUDIO,
    MAUDIOFILE,
    MDIRECTSOUND,
    MXDISPLAY,
    MMETEORCAP,
    MSMC200H263,
    MSMC200G7231,
    MAPOLLOH263,
    MAPOLLOG7231,
    MVIDEOSTREAMFILE,
    MAUDIOSTREAMFILE,

    // Additional comm devices
    MDATAPUMP,                      // Data pump comm device
    ME1,                            // E1 comm device
    MQUEUEPUMP,                     // generic packet queue pump comm device
    MPREOPENED,                     // pre-opened handle

    MDEFAULT_AUDIO,
    MDEFAULT_VIDEO,
    MDEFAULT_DATA,

    // User Input
    MUSERINPUT_NONSTANDARD,
    MUSERINPUT_BASICSTRING,
    MUSERINPUT_IA5STRING,
    MUSERINPUT_GENERALSTRING,
    MUSERINPUT_DTMF,
    MUSERINPUT_HOOKFLASH,
    MUSERINPUT_EXTENDEDALPHANUMERIC,
    MUSERINPUT_ENCRYPTEDBASICSTRING,
    MUSERINPUT_ENCRYPTEDIA5STRING,
    MUSERINPUT_ENCRYPTEDGENERALSTRING,
    MUSERINPUT_SECUREDTMF,

    // Additional media device
    M3GPPFILE,

    // Other object additions start here

    // End of list
    MOBJECT_END
};

/** Get a text string name for the specified object identifier.
    @return pointer to null terminated C string for the object ID.
  */
M_IMPORT const char * MObjectName(int object_id);

/** Get an object identifier for which the text string name matches.
    @return object ID for name.
  */
M_IMPORT MOBJECTID MObjectFromName(const char * p_pcName);


/**
 * @brief The ancestor of many of the classes in the system. 
 *
 * It is required to provide common behaviour for many objects
 */

class M_IMPORT CMObject
{
friend class CMObjectPtr;
friend class CMObjectPtrQueue;

protected:  // data
    class CMObject      *next;          // used for linked list of objects
    MOBJECTID            m_object_id;      // used to assign id to objects

    void Id(MOBJECTID id) { m_object_id = id; }

public:
    /** Constructs an object.
    */
    CMObject(
        MOBJECTID id = MUNKNOWN_OBJECT  ///< Object Id.
    )
        : next(NULL), m_object_id(id) { }

    /** Destroys an object.
    */
    virtual ~CMObject(){}

    /** @return Object Id.
    */
    MOBJECTID Id() const {return m_object_id;}

    /**
     * Duplicates an object.
    */
    // virtual that needs to be overloaded if Append below is to be used
    virtual CMObject * CMObjectCopy() const { return NULL; }

};


class M_IMPORT CMObjectPtrQueue
{
protected:  // data
    CMObject            *queue_first;
    CMObject            *queue_last;
    int                 queue_length;
public:
    CMObjectPtrQueue();
    virtual ~CMObjectPtrQueue(){Clear();}
    CMObjectPtrQueue(const CMObjectPtrQueue &from);
    inline CMObjectPtrQueue &  operator=(const CMObjectPtrQueue &from) { Copy(from); return *this; }
    int                 Insert(CMObject* o);            // put object at top of list
    int                 Add(CMObject* o);
    inline int          AddCopy(const CMObject& o){return Add(o.CMObjectCopy());}
    CMObject*           Remove(int i);
    CMObject*           Remove(CMObject* o);
    inline int          Length() const {return queue_length;}
    void                Clear(bool deleteObjects = false);
    void                Append(const CMObjectPtrQueue &from);
    inline void         Copy(const CMObjectPtrQueue &from){Clear(); Append(from);}
    inline CMObject*    First()const{return queue_first;}
    inline CMObject*    Last()const{return queue_last;}
    bool                Member(CMObject *o) const;
    CMObject*           operator [] (int i) const;      // index element in list
};


class CMObjectPtr
{
protected:  // data
    CMObject * m_pointer;
public:
    inline CMObjectPtr(CMObject * p_pointer = NULL) : m_pointer(p_pointer) {}
    inline CMObjectPtr(const CMObjectPtrQueue & p_list) : m_pointer(p_list.First()) {}
    inline CMObject * operator->() const { return  m_pointer; }
    inline CMObject& operator* () const { return *m_pointer; }
    inline CMObjectPtr & operator++() { if (m_pointer != NULL) m_pointer = m_pointer->next; return *this; }
    inline operator bool() const { return m_pointer != NULL; }
};


template <class T> class CMListIterator;


template <class T> class CMList : public CMObjectPtrQueue
{
public:
    inline CMList() {}
    inline CMList(const CMList &from) { Append(from); }
    inline CMList & operator=(const CMList &from) { Copy(from); return *this; }

    inline int  Add(T * o)               { return CMObjectPtrQueue::Add(o); }
    inline int  AddCopy(const T & o)     { return CMObjectPtrQueue::AddCopy(o); }
    inline int  Insert(T * o)            { return CMObjectPtrQueue::Insert(o); }
    inline T *  Remove(int i)            { return (T*)CMObjectPtrQueue::Remove(i); }
    inline T *  Remove(T * o)            { return (T*)CMObjectPtrQueue::Remove(o); }
    inline T*   First() const            { return (T*)queue_first; }
    inline T*   Last() const             { return (T*)queue_last; }
    inline bool Member(T *o) const       { return CMObjectPtrQueue::Member(o); }
    inline T * operator [] (int i) const { return (T*)CMObjectPtrQueue::operator [](i); }

    typedef CMListIterator<T> Iterator;
};


#if defined(_SINGLETHREADED)
#else // _SINGLETHREADED
template <class T> class CMGuardedList : public CMObjectPtrQueue
{
protected:
    CMMutex mutex;
public:
    inline CMGuardedList() {}
    inline CMGuardedList(const CMGuardedList &from) { Append(from); }
    inline CMGuardedList & operator=(const CMGuardedList &from) { Copy(from); return *this; }

    inline int  Add(T * o)               { CMGuard guard(mutex); return CMObjectPtrQueue::Add(o); }
    inline int  AddCopy(const T & o)     { CMGuard guard(mutex); return CMObjectPtrQueue::AddCopy(o); }
    inline int  Insert(T * o)            { CMGuard guard(mutex); return CMObjectPtrQueue::Insert(o); }
    inline T *  Remove(int i)            { CMGuard guard(mutex); return (T*)CMObjectPtrQueue::Remove(i); }
    inline T *  Remove(T * o)            { CMGuard guard(mutex); return (T*)CMObjectPtrQueue::Remove(o); }
    inline T*   First()                  { CMGuard guard(mutex); return (T*)queue_first; }
    inline T*   Last()                   { CMGuard guard(mutex); return (T*)queue_last; }
    inline bool Member(T *o)             { CMGuard guard(mutex); return CMObjectPtrQueue::Member(o); }
    inline T * operator [] (int i)       { CMGuard guard(mutex); return (T*)CMObjectPtrQueue::operator [](i); }

    typedef CMListIterator<T> Iterator;
};
#endif // _SINGLETHREADED



template <class T> class CMListIterator : public CMObjectPtr
{
public:
    inline CMListIterator(T * p_pointer = NULL) : CMObjectPtr(p_pointer) {}
    inline CMListIterator(const CMList<T> & p_list) : CMObjectPtr(p_list) {}
    inline     operator T*() const { return  (T *)m_pointer; }
    inline T * operator ->() const { return  (T *)m_pointer; }
    inline T & operator  *() const { return *(T *)m_pointer; }
    inline CMListIterator & operator++() { CMObjectPtr::operator++(); return *this; }
};


#endif // !__MOBJECT_H__
