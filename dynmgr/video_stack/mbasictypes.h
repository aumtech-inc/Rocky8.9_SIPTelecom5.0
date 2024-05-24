/***************************************************************************
        File:           $Archive: /stacks/include/mbasictypes.h $ 
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       Basic types classes
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

/** @file mbasictypes.h
 */

/** @mainpage CoreAPI
  *
  * @ref \htmlonly <font size="2"><b><a target="_top" href="../reference%20index.htm">Back to Reference Manual</a></b></font> \endhtmlonly
  *
  * @section intro Overview
  *
  * The CoreAPI library provides C++ classes and functions for interfacing to the platform 
  * operating system in terms of multithreading, messaging, 
  * timers, events, mutual exclusion, packet manipulation, file access with 64-bit pointers 
  * and a variety of other functions essential to the development of multimedia and data 
  * communication software. 
  *
  * Additional libraries are also supplied with CoreAPI to handle communications device (CommAPI) 
  * and media devices (MediaDevAPI)
  *
  * CoreAPI allows application code to be independent of the host operating system and
  * any communication or media devices. The result is application source code that is 
  * consistent across platforms and easier to read and maintain.
  *  
  * @section contents Contents
  *
  *   - @ref \htmlonly <a class="el" href="hierarchy.html">Class Hierarchy</a> \endhtmlonly
  *   - @ref logfunctions
  *   - @ref timefunctions
  *   - @ref stringfunctions
  *   - @ref functions
  *   - @ref basictypes
  *   - @ref macros
  *
  */

#ifndef __MBASICTYPES_H__
#define __MBASICTYPES_H__

#include "platform.h"
#include "mglobal.h"
#include "mtypes.h"
#include "mstring.h"

/** @defgroup macros Macros
  * @{ 
  */

/** Converts a 16-bit short value from little-Endian to big-Endian.
  * @hideinitializer
  */
#define MLE2BEShort(s)  ( (((s) & 0xff)<<8) | (((s) & 0xff00)>>8) )

/** Converts a 16-bit short value from little-Endian to big-Endian
  * @hideinitializer
  */
#define MBE2LEShort(s)  MLE2BEShort((s))

/** Converts a 32-bit integer value from little-Endian to big-Endian.
  * @hideinitializer
  */
#define MLE2BEInt(s)    ( (((s) & 0xff)<<24) | (((s) & 0xff00)<<8) | (((s) & 0xff0000)>>8) | (((s) & 0xff000000)>>24) )

/** Converts a 32-bit integer value from big-Endian to little-Endian
  * @hideinitializer
  */
#define MBE2LEInt(s)    MLE2BEInt((s))

/** Converts a 64-bit double value from little-Endian to big-Endian.
  * @hideinitializer
  */
#define MLE2BEDbl(s)    ((double)( (((MOCTET*)(&(s)))[7]<<56) | (((MOCTET*)(&(s)))[6]<<48) | (((MOCTET*)(&(s)))[5]<<40) | (((MOCTET*)(&(s)))[4]<<32) \
                         | (((MOCTET*)(&(s)))[3]<<24) | (((MOCTET*)(&(s)))[2]<<16)  | (((MOCTET*)(&(s)))[1]<<8) | (((MOCTET*)(&(s)))[0]) ))

/** Converts a 64-bit double value from big-Endian to little-Endian
  * @hideinitializer
  */
#define MBE2LEDbl(s)    MLE2BEDbl((s))

/** @} */

#define TWO_TO_32       (4294967296.0)

/////////////////////////////////////// CMUnsignedInt64 ////////////////////////////////
//

/** @defgroup basictypes Basic Data Types */

#ifdef UINT64

#define MMAKE64(high, low) ((((UINT64)(high))<<32)|low)

#else // UINT64

#define UINT64 CMUnsignedInt64

#define MMAKE64(high, low) CMUnsignedInt64(high, low)

#define INCLUDE_UINT64 1

/**  
  * @brief Defines an unsigned 64 bit integer class
  *
  * @ingroup basictypes;
  */
class M_IMPORT CMUnsignedInt64
{
private:    //data
    unsigned long high;
    unsigned long low;
public:

    /**
     *  Constructs a 64-bit unsigned integer object.  The default value is 0. 
     */
    CMUnsignedInt64(
        unsigned long high_long=0,    ///< Most significant long word
        unsigned long low_long=0      ///< Least significant long word
    )
    {high = high_long; low = low_long;}

    /**
     *  Copies an object of type CMUnsignedInt64. 
     */
    CMUnsignedInt64(const CMUnsignedInt64 &i);

    /**
     *  Assigns a 64-bit unsigned integer value to the object. 
     */
    inline void         Set(
        unsigned long h,    ///< Most significant long word
        unsigned long l     ///< Least significant long word
    ) {high = h; low = l;}

    /** 
     * @return Least significant long word 
     */
    inline unsigned long       LowLong() const {return low;}

    /**
     * @return most significant long word 
     */
    inline unsigned long       HighLong() const {return high;}

    /**
     * Sets the object to the binary value indicated by the eight bytes at the specified pointer. 
     * The bytes are assumed to be in little-endian format.
     */
    void                FromLEStr(
        unsigned char * c       ///< Pointer to a little-Endian 64 bit value from which the value is retreived
    );

    /**
     * Copy the value of the object to eight bytes at the specified pointer. The resultant bytes will be in
     * little-endian format
     */
    void                ToLEStr(
        unsigned char * c       ///< Pointer to a little-Endian 64-bit unsigned integer to which the value is set
    ) const;

    /**
     * Sets the object to the binary value indicated by the eight bytes at the specified pointer. 
     * The bytes are assumed to be in big-endian format.
     */
    void                FromBEStr(
        unsigned char * c       ///< Pointer to a big-Endian 64 bit value from which the value is retreived
    );

    /**
     * Copy the value of the object to eight bytes at the specified pointer. The resultant bytes will be in
     * big-endian format
     */
    void                ToBEStr(
        unsigned char * c       ///< Pointer to a big-Endian 64-bit unsigned integer to which the value is set
    ) const;

    /**
      * Convert the value to a big-endian HEX string in the specified buffer
      * @return buffer pointer that was passed in
      */
    char *               HexStringBE(
        char *str,              ///< Pointer to a buffer in which the final string is stored
        int len                 ///< Length of the buffer
    ) const;

    /**
      * Convert the value to a little-endian HEX string in the specified buffer
      * @return buffer pointer that was passed in
      */
    char*               HexStringLE(
        char *str,              ///< Pointer to a buffer in which the final string is stored
        int len                 ///< Length of the buffer
    ) const;

    /**
      * Copy the value from another CMUnsignedInt64
      * @return reference to the object
      */
    CMUnsignedInt64&    operator=(const CMUnsignedInt64 &o) {high = o.HighLong(); low = o.LowLong(); return *this;}

    /**
      * Copy the value from a signed 32-bit integer
      * @return reference to the object
      */
    CMUnsignedInt64&    operator=(int i)    {high = 0; low = (unsigned long) i; return *this;}

    /**
      * Copy the value from an unsigned 32-bit integer
      * @return reference to the object
      */
    CMUnsignedInt64&    operator=(unsigned long i)    {high = 0; low = i; return *this;}

    /**
      * Copy the value from an double precision value
      * @return reference to the object
      */
    CMUnsignedInt64&    operator=(double d);
    
    /**
      * Add a CMUnsignedInt64 to the value
      * @return reference to the object
      */
    CMUnsignedInt64     operator+(const CMUnsignedInt64& o);

    /**
      * Add an unsigned 32-bit integer to the value
      * @return reference to the object
      */
    CMUnsignedInt64     operator+(unsigned long i);

    /**
      * Increment the value by one as postfix and prefix operators
      * @return reference to the object
      */
    CMUnsignedInt64     operator++();
    CMUnsignedInt64     operator++(int );

    /**
      * Substract another CMUnsignedInt64 from the value
      * @return reference to the object
      */
    CMUnsignedInt64     operator-(const CMUnsignedInt64& o);

    /**
      * Substract an unsigned integer from the value
      * @return reference to the object
      */
    CMUnsignedInt64     operator-(unsigned long i);

    /**
      * Decrement the value by one as postfix and prefix operators
      * @return reference to the object
      */
    CMUnsignedInt64     operator--();       // prefix
    CMUnsignedInt64     operator--(int);    // postfix

    /**
      * Compare the object to another CMUnsignedInt64 value
      * @return TRUE if the values are equal, FALSE if the values are not equal
      */
    MBOOL    operator == (const CMUnsignedInt64& o) { return ((low == o.LowLong()) && (high == o.HighLong()));}
    MBOOL    operator == (unsigned long l) { return ((high==0) && (low == l));}

    /**
      * Compare the object to another CMUnsignedInt64 value
      * @return TRUE if object is greater than the argument, FALSE if the object is less than or equal to the argument
      */
    MBOOL    operator >  (const CMUnsignedInt64& o) { return ((high >  o.HighLong()) || ((high==o.HighLong()) && (low >  o.LowLong()))); }
    MBOOL    operator >  (unsigned long l) { return ((high > 0) || ((high==0) && (low >  l)));}

    /**
      * Compare the object to another CMUnsignedInt64 value
      * @return TRUE if object is greater than or equal to the argument, FALSE if the object is less than the argument
      */
    MBOOL    operator >= (const CMUnsignedInt64& o) { return ((high >  o.HighLong()) || ((high==o.HighLong()) && (low >= o.LowLong()))); }
    MBOOL    operator >= (unsigned long l) { return ((high > 0) || ((high==0) && (low >= l)));}

    /**
      * Compare the object to another CMUnsignedInt64 value
      * @return TRUE if object is less than the argument, FALSE if the greater than or equal to the argument
      */
    MBOOL    operator <  (const CMUnsignedInt64& o) { return ((high <  o.HighLong()) || ((high==o.HighLong()) && (low <  o.LowLong()))); }
    MBOOL    operator <  (unsigned long l) { return ((high==0) && (low <  l));}

    /**
      * Compare the object to another integer value
      * @return TRUE if object is less than or equal to the argument, FALSE if the greater than the argument
      */
    MBOOL    operator <= (const CMUnsignedInt64& o) { return ((high <  o.HighLong()) || ((high==o.HighLong()) && (low <= o.LowLong()))); }
    MBOOL    operator <= (unsigned long l) { return ((high==0) && (low <= l));}
};


#endif // UINT64


/** @defgroup functions Miscellaneous Functions
  * @{ 
  */

/** Checks if the computing system processes data in big-Endian style.
  * @return TRUE if the processor stores numbers in memory in big-Endian style.
  */
M_IMPORT extern MBOOL MBigEndian();

/** @} */


#endif // !__MBASICTYPES_H__
