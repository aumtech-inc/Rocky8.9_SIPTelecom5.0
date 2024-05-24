/***************************************************************************
        File:           $Archive: /stacks/include/mbuffer.h $
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       CMBuffer definition
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
#ifndef __MBUFFER_H__
#define __MBUFFER_H__

#include "platform.h"
#include "mobject.h"

/** @file mbuffer.h
 */

#ifdef _VXWORKS
#undef Free
#endif

/**
 * @brief Provides basic packet (buffering) support. 
 * 
 * It is also used as a base class for CMPacket oriented classes
 */
class M_IMPORT CMBuffer : public CMObject
{
private:
    virtual CMObject * CMObjectCopy() const { return NULL; }

protected:
    MOCTET      *base;
    MOCTET      *cur_ptr;
    int         max_data_size;
#ifdef _DEBUG
    BOOL        isARtpPacket;
#endif //_DEBUG

public:
    /**
      * Constructs object. By default the packet buffer is not allocated. 
      * See DataMakeSpace() for later allocation/growing of packet buffer.
      */
    CMBuffer(
        int sz=0    ///< Initial size of the packet buffer. Default is 0, which means the buffer is not allocated any memory resource initially
    );

    /**
      * Destroys object. 
      *
      * Note packet buffer is not freed in the destructor. It should be explicitly freed 
      * by a call to the member function CMBuffer::Free().
      */
    ~CMBuffer();   // note Destructor DOES NOT FREE allocated memory. SHOULD CALL ::Free() to free allocated memory

    /**
      * Copy constructor.
      */
    CMBuffer(const CMBuffer & p_other);

    /*
     * Makes a deep copy of CMBuffer, copying contents as well as pointers.
     *
     */
    CMBuffer & operator= (const CMBuffer & p_other);

    /*
     *  Returns pointer to start of buffer. See also CurrentPointer()
     *
     *  Care should be taken in using this pointer as it could change in a subsequent 
     *  call to DataMakeSpace() or DataAppend().
     *
     * @return Pointer of packet buffer. 
     */
    const MOCTET    *Data() const {return base;}                // inline const removed for proper overloading

    /**
      * Retreive the allocated size of the packet buffer
      *
      * @return total allocated size of the packet buffer
      */
    inline int      MaxDataSize(){return max_data_size;}

    /**
      * Return the number of bytes used since the last ResetPointer() operation as a result of 
      * DataAppend() and AdvancePointer() operations.
      *
      * @return Current bytes in the packet buffer
      */
    inline int      DataCount() const {return BytePosition();}

    /**
      * Sets the data count variable in the packet. It arbitrarily overwrites the original packet data counter
      */
    inline void     DataCount(int n) {cur_ptr = base + n;}

    /**
      * Appends an array of octets to the packet at the current pointer position. 
      * The pointer position is advanced. 
      *
      * This function checks for space in the 
      * packet before the operation and if there is not enough space, the current packet 
      * buffer is reallocated to be big enough to fit the data.
      */
    BOOL            DataAppend(
        const MOCTET *b,   ///< Pointer to a buffer containing octets to be appended 
        int n              ///< Number of octets to be appended
    );

    /**
      * Appends one octet to the packet. NOTE this member function does not check whether the packet buffer has space.
     */
    inline void     DataAppend(
        MOCTET o             ///< A single octet to be appended to
    ){*cur_ptr++ = o;}

    /**
     * Returns the current byte position in buffer as an index
     *
     * @return the current byte position 
     */
    inline int      BytePosition() const {return (int) (cur_ptr - base);}

    /**
      * 
      * @return TRUE if packet is full according to current pointer position and maximum 
      * allocated space.  Note DataMakeSpace() could be used to extend the packet buffer.
      */
    inline BOOL     Full(){return (BytePosition() >= MaxDataSize());}

    /**
     * Resets current packet buffer pointer to the base of the buffer.
     */
    inline void     ResetPointer(){cur_ptr = base;}

    /**
      * Advances current packet buffer pointer by n with respect to the current position
      */
    inline void     AdvancePointer(
        int n       ///< Number of octets that the data buffer pointer is to move forward
    ) {cur_ptr += n;}

    /**
     * Returns pointer to current insert position. See also Data()
     *
     * Care should be taken in using this pointer as it could change in a subsequent 
     * call to DataMakeSpace() or DataAppend().
     *
     * @return Pointer of current packet buffer. 
     */
    inline MOCTET*  CurrentPointer(){return cur_ptr;}

    // virtual functions

    /**
     * Allocates (or Reallocates) the buffer if the current buffer does not have space to fit n octets. 
     *
     * @return TRUE on success and FALSE on failure.
     */
    virtual BOOL            DataMakeSpace(int n);

    /**
     * Frees packet buffer.
     */
    virtual void            Free();
};

// function prototypes below are used by packet handler callbacks
typedef int (* MPACKETREADFUNCTION)(void *user_arg,  CMBuffer &pkt, unsigned long waittime);
typedef int (* MPACKETWRITEFUNCTION)(void *user_arg, CMBuffer &pkt);

#endif // !__MBUFFER_H__
