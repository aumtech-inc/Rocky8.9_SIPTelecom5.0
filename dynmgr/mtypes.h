/***************************************************************************
        File:           $Archive: /stacks/include/mtypes.h $ 
        Revision:       $Revision: 1.1 $ 
        Date:           $Date: 2006/04/10 16:24:42 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       miscellanous types
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
#ifndef __MTYPES_H__
#define __MTYPES_H__

/** @file mtypes.h  */

#include "platform.h"   // added to force system definition of types first
#include "mglobal.h"

typedef unsigned char       MOCTET;

// NOTE: the following declarations are required for non-Windows platforms
#ifndef _WINDOWS
// Windows header file has a typedef for BOOL, so we #define it here to ensure 
// we get the correct C++ type
typedef bool                MBOOL;

typedef unsigned char       BYTE;
typedef unsigned long       DWORD;

#ifndef _VXWORKS
typedef unsigned short      USHORT;
typedef unsigned int        UINT;
#ifndef UCHAR
typedef unsigned char       UCHAR;
#endif // UCHAR
#endif // _VXWORKS

typedef unsigned short      WORD;
typedef float               FLOAT;
typedef FLOAT               *PFLOAT;
typedef void                *LPVOID;
//typedef const char FAR      *LPCSTR;
typedef long                __int32;
typedef short               __int16;
#endif // _WINDOWS

#if defined(__BORLANDC__) | defined (__WATCOMC__) | defined(_WINDOWS) | defined(__ZTC__)
typedef  long  int   Word32;
typedef  short int   Word16;
typedef  short int   Flag;

#elif defined(__sun) & !defined(__sparc)
// identifier structure modified to fix different platform compilation
typedef short Word16;
typedef long  Word32;
typedef int   Flag;

#elif defined(__unix__) | defined(_VXWORKS) | defined(_RTEMS) | defined(_PSOS)
// identifier structure modified to fix different platform compilation
typedef short Word16;
typedef int   Word32;
typedef int   Flag;

#elif defined(_DSPBIOSII)
typedef short Word16;
typedef int   Word32;
typedef int   Flag;

#elif defined(_SYMBIAN)

typedef TInt16  Word16;
typedef TInt32  Word32;
typedef TInt    Flag;

#else
#error "Word16, Word32 and Flag are not defined in this platform."
#endif

#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#define FALSE false
#define TRUE  true


#if !defined(UINT32) & !defined(_VXWORKS)
typedef unsigned int UINT32;
#endif

#if !defined(UINT8) & !defined(_VXWORKS)
typedef   unsigned char UINT8;
#endif

#if !defined(UINT16) & !defined(_VXWORKS)
typedef  unsigned short UINT16;
#endif

#ifndef UINT64
#define UINT64              CMUnsignedInt64 
#endif

#ifndef MAX_UINT32
#define MAX_UINT32          (0xFFFFFFFF)
#endif

#ifndef MAX_UINT64
#define MAX_UINT64          (0xFFFFFFFFFFFFFFFF)
#endif

#ifndef MAX_INT32
#define MAX_INT32           (0x7FFFFFFF)
#endif

#ifndef MAX_BYTE
#define MAX_BYTE            (0xFF)
#endif

#ifndef MAX_WORD
#define MAX_WORD            (0xFFFF)
#endif

#ifndef MAX_DWORD
#define MAX_DWORD           (0xFFFFFFFF)
#endif

#ifndef MAX_QWORD
#define MAX_QWORD           (0xFFFFFFFFFFFFFFFF)
#endif

#ifndef NULL
#define NULL                (0)
#endif

#define MAXINT64STRLEN      17

#ifdef _DSPBIOSII
typedef struct in_addr
{
    union
    {
        struct { unsigned char s_b1,s_b2,s_b3,s_b4; }   S_un_b;
        struct { unsigned short s_w1,s_w2; }            S_un_w;
        unsigned long                                   S_addr;
    } S_un;
} in_addr;

typedef struct sockaddr_in
{
    short               sin_family;
    unsigned short      sin_port;
    struct in_addr      sin_addr;
    char                sin_zero[8];
} sockaddr_in;
#endif //_DSPBIOSII

#endif // !__MTYPES_H__
