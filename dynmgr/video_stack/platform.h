/* include/platform.h.  Generated by configure.  */
#line 2 "../../include/platform.h.in"
/***************************************************************************
        File:           $Archive: /stacks/include/platform.h $
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       Platform/os specific definitions
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

#ifndef __PLATFORM_H__
#define __PLATFORM_H__


/////////////////////////////////////////////////////
//
// if configure is run, the following will be set

#define INCLUDE_COMPILE_WARNING

// Uncomment below to enable application as single-threaded
//   By default, application is multi-threaded
/* #undef EXCLUDE_FILEIO */

// Uncomment below to enable application as single-threaded
//   By default, application is multi-threaded
/* #undef _SINGLETHREADED */

// Uncomment below to disable using CMResources access
//   By default, application utilises CMResources
/* #undef MPL_NO_CMRESOURCES */

// CMTimer uses MDynamicSleep instead of WaitForSingleObject
/* #undef USE_RECOVERABLE_PERIODIC_TIMER */


// Console I/O cannot use printf
/* #undef M_NO_PRINTF */

// set to name of "strlwr" (if available)
/* #undef M_STRLWR */

// set to name of "strupr" (if available)
/* #undef M_STRUPR */

// set to name of "strncasecmp" (if available)
#define M_STRNCASECMP strncasecmp

// set to name of "strdup" (if available)
#define M_STRDUP strdup

// set to name of "strstr" (if available)
#define M_STRSTR strstr

// set to name of "snprintf" (if available)
#define M_SNPRINTF snprintf

// set to name of "vsnprintf" (if available)
#define M_VSNPRINTF vsnprintf

// set to name of "access" (if available)
#define M_ACCESS access

// defined if arg3 of pthread_cond_timedwait is of type timespec_t
#define M_USE_TIMESPEC_T 1

// defined if host operating system defines CLOCKS_PER_SEC
#define M_HAS_CLOCKS_PER_SEC 1

// defined if host CPU is big-endian
/* #undef M_BIG_ENDIAN */

// defined if host has "assert" and assert.h
#define M_HAVE_ASSERT 1

// defined if system can include time.h and sys/time.h
#define TIME_WITH_SYS_TIME 1

// defined if host uses pthreads
#define HAVE_PTHREADS 1

// defined if host has "times" function
#define M_HAVE_TIMES 1

// defined if host has "clock_gettime" function and does not have "times"
/* #undef M_HAVE_CLOCK_GETTIME */

// defined as the unisgned 64 bit integer
#define UINT64 unsigned long long int

// defines for various include files
#define HAVE_STDARG_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define STDC_HEADERS 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_LIMITS_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_ARPA_INET_H 1
#define HAVE_NETDB_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_FCNTL_H 1
/* #undef HAVE_DIRECT_H */

/////////////////////////////////////////////////////
//
//  Additional logic associated with configure stuff

#if defined(M_USE_TIMESPEC_T)
typedef struct timespec timespec_t;
#endif

#if defined(M_HAS_CLOCKS_PER_SEC)
    #define MCLOCKS_PER_SEC CLOCKS_PER_SEC
#elif defined(_RTEMS)
    // Note: this is Board support package specific.
    // For now it is WMI MIPS 4700 specific
/*     #undef MCLOCKS_PER_SEC */
    #include <rtems.h>
#endif

/////////////////////////////////////////////////////
//
//  Windows specific stuff
//

#if (defined (WIN32) | defined(UNDER_CE)) & !defined (_WINDOWS)
    #define _WINDOWS
#endif // (WIN32 | UNDER_CE) & !_WINDOWS

#if defined(_WINDOWS) | defined(UNDER_CE)

    #if _MSC_VER>=1300
	#ifndef WINVER
	    #define WINVER 0x500
	#endif
	#define __USE_STL__ 1
    #endif

    #pragma warning(disable:4100) // unreferenced formal parameter
    #pragma warning(disable:4127) // conditional expression is constant
    #pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
    #pragma warning(disable:4503) // decorated name length exceeded, name was truncated
    #pragma warning(disable:4512) // assignment operator could not be generated
    #pragma warning(disable:4710) // function not inlined
    #pragma warning(disable:4786) // identifier was truncated

    #if (_MSC_VER == 1200) && (!defined _WIN32_WCE)

//      #pragma warning(disable: 4097)
//      #pragma warning(disable: 4514)

      // preload <limits> and kill warnings
      #pragma warning(push)
      #include <yvals.h>
      #pragma warning(disable:4663) // C++ language change: to explicitly specialize class template
      #include <limits>
      #pragma warning(pop)

      // preload <xlocale> and kill warnings
      #pragma warning(push)
      #include <yvals.h>
      #pragma warning(disable:4018)
      #pragma warning(disable:4244)
      #pragma warning(disable:4284)
      #pragma warning(disable:4290) // C++ Exception Specification ignored
      #pragma warning(disable:4511)
      #pragma warning(disable:4512)
      #pragma warning(disable:4663) // C++ language change: to explicitly specialize class template
      #include <xlocale>
      #pragma warning(pop)

      // preload <xlocnum> and kill warnings
      #pragma warning(push)
      #include <yvals.h>
      #pragma warning(disable:4018)
      #pragma warning(disable:4146)
      #pragma warning(disable:4284) // Will produce errors using infix notation
      #pragma warning(disable:4511)
      #pragma warning(disable:4512)
      #pragma warning(disable:4663) // C++ language change: to explicitly specialize class template
      #include <xlocnum>
      #pragma warning(pop)

      // preload <streambuf> and kill warnings
      #pragma warning(push)
      #include <yvals.h>
      #include <streambuf>
      #pragma warning(pop)

      // preload <ostream> and kill warnings
      #pragma warning(push)
      #include <yvals.h>
      #pragma warning(disable:4512)
      #pragma warning(disable:4663) // C++ language change: to explicitly specialize class template
      #include <ostream>
      #pragma warning(pop)

      // preload <iterator> and kill warnings
      #pragma warning(push)
      #include <yvals.h>
      #pragma warning(disable:4512)
      #include <iterator>
      #pragma warning(pop)

      // preload <vector> and kill warnings
      #pragma warning(push)
      #include <yvals.h>
      #pragma warning(disable:4018)
      #pragma warning(disable:4512)
      #pragma warning(disable:4663) // C++ language change: to explicitly specialize class template
      #include <vector>
      #pragma warning(pop)

      // preload <list> and kill warnings
      #pragma warning(push)
      #include <yvals.h>
      #pragma warning(disable:4284) // Will produce errors using infix notation
      #include <list>
      #pragma warning(pop)

      // preload <map> and kill warnings
      #pragma warning(push)
      #include <yvals.h>
      #pragma warning(disable:4284) // Will produce errors using infix notation
      #include <map>
      #pragma warning(pop)
    #endif

    #if !defined (_AFXDLL)
		/* This is necessary at this time due to some strange problem
		   with headers that make a build without this flag create
		   structures within this library of a different size to those
		   without the flag. So if ever an application is built with MFC
		   DLL support, it crashes. This needs to be tracked down and killed
		   sometime .... */
        #define _AFXDLL 1
    #endif

	#define _CRTDBG_MAP_ALLOC
    #if defined (_AFXDLL) & defined(__cplusplus)
        #include <afx.h>
	#elif !defined(MEMCHECK)
		#include <stdlib.h>
	    #include <crtdbg.h>
	#else
	    #define _ASSERTE(s)
	#endif

    #if defined(UNDER_CE)

        #include <Winsock.h>

		#define M_NO_PRINTF
		#define assert(a)
		#define _DONTHAVESTREAMS

    #else // UNDER_CE

        #include <winsock2.h>               // winsock2.h has to be put after afx.h
		#include <process.h>                // Multithreading is defined in process.h

		#define M_HAVE_ASSERT 1
		#define HAVE_FCNTL_H 1
		#define HAVE_DIRECT_H 1
		#define HAVE_SYS_STAT_H 1
		#define HAVE_TIME_H 1

		#define M_STRDUP strdup
		#define M_STRLWR _strlwr
		#define M_STRUPR _strupr
		#define M_STRRCHR strrchr
		#define M_STRSTR strstr
		#define M_ACCESS access

    #endif // UNDER_CE

    #include <limits.h>
    #include <float.h>

    #define MINFINITE INFINITE          // Infinite timeout
    #define MINDOUBLE DBL_MIN
    #define MAXDOUBLE DBL_MAX
    #define MININT    INT_MIN
    #define MAXINT    INT_MAX
    #define MAXFLOAT  FLT_MAX
    #define MINFLOAT  FLT_MIN
    #define DIR_SEP "\\"
    #define DIR_SEP_CHAR '\\'
    #define TMPDIR "\\temp"
    #define OPENMODE "rb"

    #define WM_VIDEO_SIZE	WM_USER + 101
    #define WM_SHOWINGCHILD	WM_USER + 102

    #define HAVE_STDARG_H 1
    #define HAVE_STDLIB_H 1
    #define HAVE_STRING_H 1

    #define M_SNPRINTF snprintf
    #define M_VSNPRINTF vsnprintf

    #define UINT64 unsigned long long int
#endif


/////////////////////////////////////////////////////
//
//  non-Windows stuff
//

#if ! defined(_WINDOWS)

    #define DIR_SEP "/"
    #define DIR_SEP_CHAR '/'
    #define TMPDIR "/tmp"
    #define OPENMODE "r"

    #if !defined(__asm)
        #define __asm _asm
        #if !defined(__forceinline)
            #define __forceinline inline
        #endif // !__forceinline
    #endif

#endif

/////////////////////////////////////////////////////
//
// Linux specific stuff
//

//  none

/////////////////////////////////////////////////////
//
// Solaris specific stuff
//

//  none

/////////////////////////////////////////////////////
//
// pSOS specific stuff
//

#if defined(_PSOS)
    #include <configs.h>
    #include <time.h>
    #include <prepc.h>
    #include <pthread.h>
    #include <unistd.h>
    #include <psos.h>
    #include <pna.h>
    #include <sys/stat.h>               // This #include <sys/stat.h> shall follow #include <sys/types.h>
    #include <rescfg.h>
#endif // _PSOS

/////////////////////////////////////////////////////
//
// DSPBIOSII specific stuff
//

#if defined(_DSPBIOSII)
    #if defined(_TICONLY)
    extern "C"
    {
    #endif // _TICONLY

    #include <file.h>
    #include <std.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdarg.h>
    #include <stdio.h>
    #include <time.h>
    #include <limits.h>
    #include <float.h>
    #include <errno.h>
    #include <string.h>
    #include <math.h>
    #include <ctype.h>

    #if defined(_TICONLY)
    }
    #endif // _TICONLY
#endif // _DSPBIOSII

/////////////////////////////////////////////////////
//
// VxWORKS specific stuff
//

#if defined(_VXWORKS)
    #include <vxWorks.h>
    #include <unistd.h>
    #include <time.h>
    #include <sys/times.h>
    #include <sockLib.h>
    #include <hostLib.h>
    #include <ioLib.h>
    #include <selectLib.h>
#endif

/////////////////////////////////////////////////////
//
// RTEMS specific stuff
//

#if defined(_RTEMS)
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/time.h>
    #include <netdb.h>
    #include <machine/endian.h>

    extern int select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
#endif

/////////////////////////////////////////////////////
//
// Symbian specific stuff
//

#if defined(_SYMBIAN)
    #include <e32std.h>
/*     #undef _UNICODE */
    #define MAX_PATH MAXPATHLEN

    // Symbian does not support file logging
    #define INCLUDE_FILE_LOGGING 0

    #include <stdarg.h>
    int Msnprintf(char * p_pcBuffer, int size, const char * p_pcFormat, ...);
    #define M_SNPRINTF snprintf
    int Mvsnprintf(char * p_pcBuffer, int size, const char * p_pcFormat, va_list p_pArgs);
    #define M_VSNPRINTF vsnprintf

    #define HAVE_STRING_H 1
    #define HAVE_STDLIB_H 1
    #define M_HAVE_ASSERT 1
    #define HAVE_LIMITS_H 1
    #define HAVE_UNISTD_H 1
    #define HAVE_FCNTL_H 1
    #define HAVE_SYS_STAT_H 1
    #define HAVE_SYS_TYPES_H 1
    #define HAVE_STDARG_H 1
#endif // _SYMBIAN

/////////////////////////////////////////////////////
//
// non-OS-specific stuff
//

#include <stdio.h>
#include <ctype.h>

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif

#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#endif

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#elif HAVE_SYS_TIME_H
# include <sys/time.h>
#elif HAVE_TIME_H
# include <time.h>
#endif

#if HAVE_STDARG_H
#  include <stdarg.h>
#endif

#if HAVE_LIMITS_H
#  include <limits.h>
#endif

#if HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif

#if HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#if HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#if HAVE_NETDB_H
#  include <netdb.h>
#endif

#if HAVE_SIGNAL_H
#  include <signal.h>
#endif

#if HAVE_PTHREADS
#  include <pthread.h>
#endif

#if M_HAVE_ASSERT
#  include <assert.h>
#endif

#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#if HAVE_DIRECT_H
#  include <direct.h>
#endif

// catchall if _ASSERTE is not defined for current platform
#if !defined	_ASSERTE
#define	_ASSERTE(s)
#endif

#if ! defined (FAR)
    #define FAR                     // Must be before #include "mtypes.h"
#endif

#ifndef	MAX_PATH
    #if defined(PATH_MAX)
        #define MAX_PATH PATH_MAX
    #elif defined(_POSIX_PATH_MAX)
        #define MAX_PATH _POSIX_PATH_MAX
    #else
        #define MAX_PATH 256
    #endif
#endif

#ifndef O_BINARY
#define O_BINARY	0
#endif

#ifndef	O_TEXT
#define	O_TEXT		0
#endif

#ifndef _S_IREAD
    #ifdef  S_IREAD
        #define  _S_IREAD   S_IREAD
    #endif
#endif

#ifndef _S_IWRITE
    #ifdef  S_IWRITE
        #define  _S_IWRITE   S_IWRITE
    #endif
#endif

#ifndef HIBYTE
    #ifdef M_BIG_ENDIAN
        #define HIBYTE(w)   ((unsigned char) (w))
        #define LOBYTE(w)   ((unsigned char) (((unsigned short) (w) >> 8) & 0xFF))
    #else
        #define LOBYTE(w)   ((unsigned char) (w))
        #define HIBYTE(w)   ((unsigned char) (((unsigned short) (w) >> 8) & 0xFF))
    #endif
#endif

#ifndef	MINFINITE
    #define MINFINITE ULONG_MAX         // Infinite timeout
#endif

#if defined (MEMCHECK)
    #include "memcheck.h"
    #define __PLACEMENT_NEW_INLINE
#endif // MEMCHECK

#ifndef	INADDR_NONE
    #define	INADDR_NONE	0xffffffff
#endif


///////////////////////////////////////////////////////////////////
//
//  #define graveyard
//

#if 0

#if ! defined(_DSPBIOSII) & ! defined(_TICONLY)
    #include <stdlib.h>
    #include <stdarg.h>

    #if !defined(UNDER_CE)
        #include <stdio.h>
        #include <assert.h>
        #include <signal.h>
    #endif
#endif

#if !defined(UNDER_CE)
    #define assert(a)
#endif

#if defined(_SOLARIS) | defined(_LINUX)
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/utsname.h>

#endif

#if defined(DOS) | defined(decmips) | defined(_WINDOWS)
    #if !defined(UNDER_CE)
        #include <time.h>
    #endif // !UNDER_CE

#elif defined(_LINUX)
    #include <time.h>
    #include <sys/times.h>
    #include <sys/time.h>
    #include <netdb.h>
#else
    #include <sys/time.h>
    #include <netdb.h>
#endif // DOS | decmips | _WINDOWS


#if defined(_LINUX) | defined(_SOLARIS) | defined(_VXWORKS) | defined(_RTEMS) | defined(_PSOS)

#if ! defined(_PSOS)
    #include <sys/types.h>
    #include <sys/stat.h>               // This #include <sys/stat.h> shall follow #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif //  !_PSOS

#if defined(_LINUX) | defined(_RTEMS)
    #include <fcntl.h>
#elif defined(_VXWORKS)
#elif defined(_PSOS)
#endif // _LINUX | _RTEMS

#endif // _LINUX | _SOLARIS | _VXWORKS | _RTEMS | _PSOS

#if defined(DOS)
    #include <dos.h>
#endif // DOS


#if defined(_DSPBIOSII)
#else
    #if !defined(UNDER_CE)
        #include <errno.h>
    #endif // !UNDER_CE
    #include <string.h>
    #include <math.h>
    #include <ctype.h>
#endif // _DSPBIOSII


#if defined(DOS) | defined(_WINDOWS) | defined(UNDER_CE)

#else  // DOS | _WINDOWS | UNDER_CE

    #if defined(_VXWORKS) | defined (_SOLARIS) | defined(_LINUX) | defined(_RTEMS) | defined(_PSOS)

        #include <limits.h>
        #include <float.h>

    #elif defined(_DSPBIOSII)
        #if defined(_TICONLY)
        extern "C"
        {
        #endif // _TICONLY

        #include <limits.h>
        #include <float.h>

        #if defined(_TICONLY)
        }
        #endif // _TICONLY

    #else
        #include <values.h>
    #endif // _VXWORKS | _SOLARIS | _LINUX | _RTEMS | _PSOS


#endif // DOS | _WINDOWS | UNDER_CE



#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

#if defined(NEEDRINT)
    #define rint(x) (floor((x) + 0.5))
#else
    extern M_IMPORT double rint(double);
#endif // NEEDRINT

#if defined(_PSOS) & defined(_APOLLO)
    extern M_IMPORT char *Apollo_Read(char *s, int len, FILE *fp=NULL);
    extern M_IMPORT int Apollo_Print(const char *, ...);
    #define fgets       Apollo_Read
    #define printf      Apollo_Print
#endif // _PSOS & _APOLLO

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // 0
//
//  end of graveyard
//
///////////////////////////////////////////////////////////////////


/** Selection if compiled inclusion of File Logging
    Set to one if want, zero if do no want it
  */
#ifndef INCLUDE_FILE_LOGGING
#define INCLUDE_FILE_LOGGING     1
#endif//INCLUDE_FILE_LOGGING


#include "mglobal.h"
#include "mtypes.h"


#ifdef EXCLUDE_LOGGING

#ifdef INCLUDE_LOGGING
/* #undef INCLUDE_LOGGING */
#endif

#if defined(_MSC_VER)
#pragma message("Logging is disabled")
#elif defined(__GNUC__)
#message Logging is disabled
#endif

#else // EXCLUDE_LOGGING

#ifndef INCLUDE_LOGGING
#define INCLUDE_LOGGING 1
#endif

#endif // EXCLUDE_LOGGING

// The following is for backward compatibility
#ifdef INCLUDE_LOGGING
#define INCLUDE_CONSOLE_IO
#endif

#ifndef INCLUDE_FULL_LOGGING
#define INCLUDE_FULL_LOGGING 1
#endif


#endif // !__PLATFORM_H__