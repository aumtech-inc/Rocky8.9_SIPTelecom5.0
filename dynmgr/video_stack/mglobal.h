/***************************************************************************
        File:           $Archive: /stacks/include/mglobal.h $
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       Global definitions
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
#ifndef __MGLOBAL_H__
#define __MGLOBAL_H__

/** @file mglobal.h */

#ifndef MAXFILENAMELEN
#define MAXFILENAMELEN 128
#endif

#define PATH_ENV_SEPARATOR ":"

#define INIT_FCT "init_dl_"

#ifndef MIN
#define MIN(a,b)        (((a) <= (b))? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)        (((a) >= (b))? (a) : (b))
#endif

#ifndef min
#define min(a,b)        MIN((a),(b))
#endif

#ifndef max
#define max(a,b)        MAX((a),(b))
#endif

#ifndef sign
#define sign(a)         ((a) < 0 ? -1 : 1)
#endif

#define FOREVERY(p,l)   for((p) = (l); (p) != NULL; (p) = (p)->next)
#define LAST(p,l)       for((p) = (l); (p)->next != NULL; (p) = (p)->next)

#define IFNEQS(a, b)    if (strcmp(a, b))
#define IFEQS(a, b)     if (! strcmp(a, b))
#define EFEQS(a, b)     else IFEQS(a,b)
#define NELTS(x)        (sizeof((x)) / sizeof(((x)[0])))
#define EQS(a, b)       (! strcmp((a), (b)))
#define SIGN(a)         ((a) >=0 ? 1 : -1)

#define NUMBYTES(bits)  (((bits) + 7) >> 3)

/*                I M P O R T A N T
                    to compile in a DLL you need to define DLL */
#if (defined (_WINDOWS) || defined(UNDER_CE)) & (!defined(M_EXPORT)) & defined(DLL)
    #define M_EXPORT    __declspec( dllexport )
#else
    #define M_EXPORT
#endif

/* note when building M_IMPORT should be M_EXPORT */
#if (defined(_WINDOWS) || defined(UNDER_CE)) & defined(DLL)
    #define M_IMPORT M_EXPORT
#else
    #if (defined(_WINDOWS) || defined(UNDER_CE)) & (!defined(M_IMPORT)) & defined(DLL)
        #define M_IMPORT    __declspec( dllimport )
    #else
        #define M_IMPORT
    #endif
#endif

// Gregorian calendar, num of 100ns intervals between 15/10/1582 inclusive and 1/1/1970 exclusive
#define GREG_CONSTATNTS 1.2219292800000e+017

#endif // !MGLOBAL_H
