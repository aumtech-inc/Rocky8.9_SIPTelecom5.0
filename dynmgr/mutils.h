/***************************************************************************
        File:           $Archive: /stack/include/mutils.h $
        Revision:       $Revision: 1.1 $
        Date:           $Date: 2006/04/10 16:24:42 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       useful functions header
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
#ifndef __MUTILS_H__
#define __MUTILS_H__

/** @file mutils.h  */

#include "platform.h"
#include "mstring.h"
#include "mbasictypes.h"


#define MNullify(a)      memset(&(a), 0, sizeof(a))

/////// Macros
// Note for these macros,
//   Byte   8 bit
//   MBOOL   sizeof(MBOOL) i.e. 1 byte or 4 bytes
//   char   1
//   short  2
//   int    4

// 8sep00 MAJ removed '\' at end of lines as gcc shokes on them

/** @defgroup timefunctions Time Functions
  */

/** @defgroup stringfunctions String Functions
  */

/**
  * @defgroup macros Macros
  * @{
  */

/** Transfers a char value stored in "src" to a char value stored in "dest". 
  * @hideinitializer
  * @param dest Destination as char
  * @param src  Source value as char
  */
#define MChar2Char(/* char */dest, /* char */src)       (dest) = (src);

/**
  * Transfers data in a buffer pointed by "src" to a char value stored in "dest". "Src" increments by the size of char afterwards.
  * @hideinitializer
  * @param dest Destination value as char
  * @param src  Source as MOCTET *
  */
#define MBuf2Char(/* char */dest, /* MOCTET* */src)     (dest) = (char)*(src); (src) += sizeof(char);

/**
  * Transfers a char value stored in "src" to a buffer pointed by "dest". "Dest" increments by the size of octet afterwards
  * @hideinitializer
  * @param dest Destination as MOCTET *
  * @param src  Source value as char
  */
#define MChar2Buf(/* MOCTET* */dest, /* char */src)     memcpy(dest, &(src), sizeof(MOCTET)); dest += sizeof(MOCTET);

/**
  * Sets "dest" TRUE when "src" is TRUE, FALSE otherwise
  * @hideinitializer
  * @param dest Destination as MBOOL *
  * @param src  Source value as MBOOL
  */
#define MBool2Bool(/* MBOOL* */dest, /* MBOOL */src)      if (src) {(dest) = TRUE;} else {(dest) = FALSE;}

/**
  * Sets "dest" TRUE if "src" points to a non-zero octet, FALSE otherwise. "Src" increments by the size of octet afterwards
  * @hideinitializer
  * @param dest Destination as MBOOL
  * @param src  Source value as MOCTECT*
  */
#define M1Buf2Bool(/* MBOOL */dest, /* MOCTET* */src)    (dest) = (MBOOL)*(src); (src) += sizeof(MOCTET);

/**
  * Sets the content pointing by "dest" TRUE when "src" is TRUE, FALSE otherwise. "Dest" increments by the size of octet afterwards
  * @hideinitializer
  * @param dest Destination as MOCTET *
  * @param src  Source value as MBOOL
  */
#define MBool21Buf(/* MOCTET* */dest, /* MBOOL */src)    if (src) {*(dest) = TRUE; } else {*(dest) = FALSE; } (dest) += sizeof(MOCTET);

/**
  * Transfers a short value stored in "src" to a short value stored in "dest". 
  * @hideinitializer
  * @param dest Destination as short
  * @param src  Source value as short
  */
#define MShort2Short(/* short */dest, /* short */src)   if (MBigEndian()) {(dest) = (short)(src);} else {(dest) = (short)MBE2LEShort((src));}

/**
  * Transfers an integer value stored in "src" to an integer value stored in "dest". 
  * @hideinitializer
  * @param dest Destination as int
  * @param src  Source value as int
  */
#define MInt2Int(/* int */dest, /* int */src)           if (MBigEndian()) {(dest) = (int)(src);} else {(dest) = (int)MBE2LEInt((src));}

#if defined(__MMCC__) & defined(__MMCC_C54X_TI_V31)

#define MBuf2Short(/* short */dest, /* MOCTET* */src) \
                dest = (src[1] & 0xFF); \
                dest |= (src[0] & 0xFF)<<8; \
                (src) += 2;
#define MShort2Buf(/* MOCTET* */dest, /* short */src) \
                dest[0] = (src >> 8) & 0xFF; \
                dest[1] = (src & 0xFF); \
                (dest) += 2;
#define MBuf2Int(/* int */dest, /* MOCTET* */src) \
                dest = (src[3] & 0xFF); \
                dest |= (src[2] & 0xFF)<<8; \
                dest |= (src[1] & 0xFF)<<16; \
                dest |= (src[0] & 0xFF)<<24; \
                (src) += 4;
#define MInt2Buf(/* MOCTET* */dest, /* int */src) \
                dest[0] = (((long)src) >> 24) & 0xFF; \
                dest[1] = (((long)src) >> 16) & 0xFF; \
                dest[2] = (((long)src) >> 8) & 0xFF; \
                dest[3] = ((long)src) & 0xFF; \
                (dest) += 4;
#define MBuf2Dbl(/* double */dest, /* MOCTET* */src) \
                ((MOCTET*)&dest)[0] = (src[7] & 0xFF); \
                ((MOCTET*)&dest)[1] |= (src[6] & 0xFF)<<8; \
                ((MOCTET*)&dest)[2] |= (src[5] & 0xFF)<<16; \
                ((MOCTET*)&dest)[3] |= (src[4] & 0xFF)<<24; \
                ((MOCTET*)&dest)[4] |= (src[3] & 0xFF)<<32; \
                ((MOCTET*)&dest)[5] |= (src[2] & 0xFF)<<40; \
                ((MOCTET*)&dest)[6] |= (src[1] & 0xFF)<<48; \
                ((MOCTET*)&dest)[7] |= (src[0] & 0xFF)<<56; \
                (src) += 8;
#define MDbl2Buf(/* MOCTET* */dest, /* double */src) \
                dest[0] = ((MOCTET*)&dest)[7]; \
                dest[1] = ((MOCTET*)&dest)[6]; \
                dest[2] = ((MOCTET*)&dest)[5]; \
                dest[3] = ((MOCTET*)&dest)[4]; \
                dest[4] = ((MOCTET*)&dest)[3]; \
                dest[5] = ((MOCTET*)&dest)[2]; \
                dest[6] = ((MOCTET*)&dest)[1]; \
                dest[7] = ((MOCTET*)&dest)[0]; \
                (dest) += 8;

#else // normal

/**
  * Converts data in a buffer pointed by "src" to a short value stored in "dest". "Src" increments by the size of short afterwards.
  * @hideinitializer
  * @param dest Destination as short
  * @param src  Source value as MOCTET *
  */
#define MBuf2Short(/* short */dest, /* MOCTET* */src)   if (MBigEndian()) {(dest) = *((short*)(src));} else {(dest) = MBE2LEShort(*(short*)(src));} (src) += sizeof(short);

/**
  * Transfers a short value stored in "src" to a buffer pointed by "dest". "Dest" increments by the size of short afterwards.
  * @hideinitializer
  * @param dest Destination as MOCTET *
  * @param src  Source value as short
  */
#define MShort2Buf(/* MOCTET* */dest, /* short */src)   if (MBigEndian()) {*(short*)(dest) = (src);} else {*(short*)(dest) = MLE2BEShort((src));}(dest) += sizeof(short);

/**
  * Coverts data in a buffer pointed by "src" to an integer value stored in "dest". "Src" increments by the size of integer afterwards
  * @hideinitializer
  * @param dest Destination as int
  * @param src  Source value as MOCTET *
  */
#define MBuf2Int(/* int */dest, /* MOCTET* */src)       if (MBigEndian()) {(dest) = *((int*)(src));} else {(dest) = MBE2LEInt(*(int*)(src)); } (src) += sizeof(int);

/**
  * Transfers an integer value stored in "src" to a buffer pointed by "dest". "Dest" increments by the size of integer afterwards.
  * @hideinitializer
  * @param dest Destination as MOCTET *
  * @param src  Source value as int
  */
#define MInt2Buf(/* MOCTET* */dest, /* int */src)       if (MBigEndian()) {*(int*)(dest) = (src);} else {*(int*)(dest) = MLE2BEInt((src));} (dest) += sizeof(int);

/**
  * Coverts data in a buffer pointed by "src" to a double value stored in "dest". "Src" increments by the size of double afterwards
  * @hideinitializer
  * @param dest Destination as double
  * @param src  Source value as MOCTET *
  */
#define MBuf2Dbl(/* double */dest, /* MOCTET* */src)    if (MBigEndian()) {(dest) = *((double*)(src));} else {(dest) = MBE2LEDbl(*(int*)(src)); } (src) += sizeof(double);

/**
  *  Transfers a double value stored in "src" to a buffer pointed by "dest". "Dest" increments by the size of double afterwards
  * @hideinitializer
  * @param dest Destination as MOCTET *
  * @param src  Source value as double
  */
#define MDbl2Buf(/* MOCTET* */dest, /* double */src)    if (MBigEndian()) {*(double*)(dest) = (src);} else {*(int*)(dest) = MLE2BEDbl((src));} (dest) += sizeof(double);


/** @} */

#endif // __MMCC__ & __MMCC_C54X_TI_V31

#ifdef _VXWORKS
#include <ioLib.h>
#endif // _VXWORKS

#ifdef __cplusplus
extern "C" {
#endif

extern const MOCTET *g_pzBitReversedByte;
extern const int g_iBitReversedByteTableLen;

// the following function prototypes should be C compatible
#ifdef _VXWORKS
//M_IMPORT int strncasecmp(char *s1, char *s2, int n);
#endif // _VXWORKS

/** Does a non case significant comapre fo two strings.
  *
  * @ingroup functions
  */
extern M_IMPORT int   Mstrncasecmp(const char *s1, const char* s2, int n);

/** Searches for the last instance of the character in the string.
  *
  * @ingroup functions
  */
extern M_IMPORT char* Mstrrchr(const char *s, char c);

/** Searches for a substring within a string.
  *
  * @ingroup functions
  */
extern M_EXPORT char* Mstrstr(const char* s1, const char* s2);

#if ! defined(UNDER_CE)
/** Determines the file size of the specified file.
  * @return Size of the specified file in bytes on success.  
  * 0 may indicate the file does not exist that or an error has occurred.
  * @ingroup functions
  */
extern M_IMPORT int     Mfilesize(
    char *fname         ///< Name of file
);
#endif

/** Copies a memory block in reverse byte sequence.
  *
  * Sufficient memory resources for both "from" and "to" should be allocated 
  * with a minimum of "size" bytes before memcpyrvs() is called.
  * @ingroup functions
  */
extern M_IMPORT void    Mmemcpyrvs(
    unsigned char *to,      ///< Pointer to beginning of memory block to copy to
    unsigned char *from,    ///< Pointer to beginning of memory block to copy from
    int size                ///< Size of memory block to be copied
);

/** Duplicates a string using the standard C function strdup().
  * @return Pointer to a new duplicated string.  NULL on error. Use the standard library 
  * free() function call to free the allocated string.
  * @ingroup functions stringfunctions
  */
extern M_IMPORT char*   Mstrdup(
    const char *str         ///< Pointer to string to be duplicated
);

/** Converts all characters in the specified string to lower case.
  *
  * As string conversion is performed in-place, the passed-in "str" parameter shall be non-constant.
  * @ingroup functions stringfunctions
  */
extern M_IMPORT char* Mstrlwr(char *str);

/** Converts all characters in the specified string to upper case.
  *
  * As string conversion is performed in-place, the passed-in "str" parameter shall be non-constant.
  * @ingroup functions stringfunctions
  */
extern M_IMPORT char* Mstrupr(char *str);

/** Converts all characters in the specified string to lower case.
  *
  * As string conversion is performed in-place, the passed-in "str" parameter shall be non-constant.
  * @ingroup functions stringfunctions
  */
extern M_IMPORT void    MStringToLower(
    char* str       ///< Pointer to string to be converted to lower case
);

/** Converts all characters in the specified string to upper case.
  *
  * As string conversion is performed in-place, the passed-in "str" parameter shall be non-constant.
  * @ingroup functions stringfunctions
  */
extern M_IMPORT void    MStringToUpper(
    char* str       ///< Pointer to string to be converted to upper case
);

#if !defined(_SINGLETHREADED)

/** Converts from number of seconds to time string
  *
  * The format of the string will be in the form of "days :: hours :: minutes : seconds".  
  * If days is zero, it does not appear in the string.  If days and hours are both zero, 
  * both of them do not appear in the string.  If days, hours and minutes are all zero, 
  * all of them do not appear in the string.
  * @ingroup timefunctions
  */
extern M_IMPORT void    MSecondsToTimeString(
    double s,       ///< Number of seconds elapsed
    CMString &str   ///< Reference of time string for retrieving current time in string form.  
);

#endif

/** Frees memory resource of the specified string. This function should only be called to 
  * free strings allocated or created using ::MNewString().
  * @ingroup functions stringfunctions
  */
extern M_IMPORT void    MDeleteString(
    char *str       ///< Pointer to character string to be deleted
);

/** Duplicates string.  Memory resource for the new string and to be returned is internally allocated
  * free strings allocated or created using ::MNewString().
  *
  * The allocated string should be free using ::MDeleteString().
  * @return Pointer to a new duplicated string.  If NULL is returned, use MLastError() to retrieve the error code
  * @ingroup functions stringfunctions
  */
extern M_IMPORT char*   MNewString(
    const char *str ///< Pointer to string to be duplicated
);

#ifdef _WINDOWS
#if ! defined(UNDER_CE)
/** Prompts user for a file name to be opened or saved through a system-dependant dialog box
  *
  * This function is defined on the Windows platforms only
  * @return Pointer to a string containing the name of the file.  NULL on error or cancellation.
  * @ingroup functions
  */
extern M_IMPORT char*   MGetFileNameTitle(
    HINSTANCE hInst,                    ///< Handle of application instance 
    HWND hWnd,                          ///< Handle of application window 
    MBOOL p_zForSave,                    ///< Flag indicating the file is for saving (TRUE) or opening (FALSE) 
    const char* p_szString_filter,      ///< Pointer to string describing the way of filtering files.   It should have types separated by "|" and the lot terminated by "||".   e.g. "All files (*.*)|*.*||" 
    const char* p_szTitlebar,           ///< Pointer to string containing the name to be shown on the title bar of the dialog box 
    const char* p_szDefaultDir = NULL   ///< Pointer to string containing the default directory
);

extern M_IMPORT MBOOL    MCenterWindow(HWND hwndChild, HWND hwndParent);
#endif

/** Obtains the corresponding error description message for the specified error.
  * @return Pointer to read-only zero-terminated error message description string if available. 
  * "Message not available"  is returned if no error message is available for the specified error.
  * @ingroup functions
  */
extern M_IMPORT char*   MGetLastErrorMessage(
    int err         ///< Error number.  See valid errors.
);
#endif

/** Sets initial reference time clock tick counts using MCoreAPITime().
  * @ingroup timefunctions
  */
M_IMPORT void                           MCoreAPIInitReferenceTime();

/** @return Reference time in clock tick counts.  It should have been set by MCoreAPIInitReferenceTime()
  * @ingroup timefunctions
  */
M_IMPORT extern unsigned long           MCoreAPIReferenceTime();

/** @return Current system clock tick count in milliseconds.
  * @ingroup timefunctions
  */
M_IMPORT extern "C++" unsigned long     MCoreAPITime();

#define MCoreAPIWallclockTime MCoreAPITime // backward compatibility

/** Retrieve current local time in string format.
  *
  * The macro SZ_DATETIME_BUFFER gives the maximum size of the string buffer
  * @return Pointer to the string.
  * @ingroup timefunctions stringfunctions
  */
M_IMPORT extern char*                   MDateTimeString(
    char* gszDateTimeBuf        ///< Pointer to a string that stores the current local time.
);
#define SZ_DATETIME_BUFFER  25

/** Validate the input date.
  * @return TRUE if the date is valid, FALSE otherwise.
  * @ingroup timefunctions
  */
M_IMPORT MBOOL                           MDateValid(
    const unsigned short day1,      ///< Day to be validated.
    const unsigned short month1,    ///< Month to be validated.
    const int year1,                ///< Year to be validated.
    const unsigned short day2,      ///< Reference day
    const unsigned short month2,    ///< Reference month
    const int year2                 ///< Reference year
);

// To generate a random number
M_IMPORT void Msrand();
M_IMPORT USHORT MGenerateRandomNumber();

#if defined(M_NO_PRINTF) + defined(_DOXYGEN)

/** @fn int MConsolePrintf(const char *format, ...);
  * Prints the arguments to the OS console window, similar to the printf function
  * @return Number of output characters.
  * @ingroup logfunctions
  */
extern M_IMPORT int MConsolePrintf(
    const char *format,         ///< Format string 
    ...                         ///< arguments to print. 
);

/** @fn void MConsoleFlush();
  * Flushes the console output, if it is being buffered.
  * @ingroup logfunctions
  */
#define MConsoleFlush()  // do nothing

#else // M_NO_PRINTF

#define MConsolePrintf printf
#define MConsoleFlush() fflush(stdout)

#endif // M_NO_PRINTF


// MACRO:  MSetupStaticElapsedTimeValidation(...)
//      DWORD prev_time_varname (static)
//      DWORD curr_time_varname
//      DWORD time_period
//      Action function
// Note:    It may require {} for the source code concerned.
//          These macros are global in nature and therefore, multiple
//          instances share the same set of static variables.
#define MSetupStaticElapsedTimeValidationBasic(prev_time_varname,           \
                        curr_time_varname, time_period)                     \
            static DWORD (prev_time_varname) = 0;                           \
            DWORD (curr_time_varname) = MCoreAPITime();                     \
            DWORD diff_time = (curr_time_varname) - (prev_time_varname)
#define IF_MSetupStaticElapsedTimeValidated(prev_time_varname,              \
                        curr_time_varname, time_period)                     \
            MSetupStaticElapsedTimeValidationBasic((prev_time_varname),     \
                    (curr_time_varname), (time_period));                    \
            if (diff_time >= (time_period))
#define MSetupStaticElapsedTimeValidation(prev_time_varname,                \
                        curr_time_varname, time_period, action_func)        \
            IF_MSetupStaticElapsedTimeValidated((prev_time_varname),        \
                    (curr_time_varname), (time_period)) {                   \
                (action_func);                                              \
                (prev_time_varname) = (curr_time_varname);                  \
            }
#define MSetupStaticElapsedTimeValidation2(prev_time_varname,               \
                        curr_time_varname, time_period, action_func,        \
                        action_func2)                                       \
            IF_MSetupStaticElapsedTimeValidated((prev_time_varname),        \
                    (curr_time_varname), (time_period)) {                   \
                (action_func);                                              \
                (action_func2);                                             \
                (prev_time_varname) = (curr_time_varname);                  \
            }
#define MSetupStaticElapsedTimeValidation3(prev_time_varname,               \
                        curr_time_varname, time_period, action_func,        \
                        action_func2, action_func3)                         \
            IF_MSetupStaticElapsedTimeValidated((prev_time_varname),        \
                    (curr_time_varname), (time_period)) {                   \
                (action_func);                                              \
                (action_func2);                                             \
                (action_func3);                                             \
                (prev_time_varname) = (curr_time_varname);                  \
            }

#ifdef __cplusplus
}
#endif

//
// Memory and string utilities
//
#define MMEMCLEAR(p) memset(&p,0x0,sizeof(p))    // Clear an object of known size

//
// Copy first part of a string (if too big for dest). Result is NULL terminated
//
#define MSTRNCPY_FIRST(_dct_dest_pc, _dct_src_pc, _dct_dest_len_i)  \
{                                                                   \
    strncpy((char*)(_dct_dest_pc),                                  \
            (const char*)(_dct_src_pc),                             \
            (size_t)MIN((_dct_dest_len_i),strlen((_dct_src_pc))+1));\
    _dct_dest_pc[(_dct_dest_len_i)-1] = '\0';                       \
}

//
// Copy last part of a string (if too big for dest). Result is NULL terminated
//
#define MSTRNCPY_LAST(_dct_dest_pc, _dct_src_pc, _dct_dest_len_i)   \
{                                                                   \
    int _dct_index_i;                                               \
    _dct_index_i = strlen((_dct_src_pc)) + 1 - (_dct_dest_len_i);   \
    _dct_index_i = (_dct_index_i < 0 )?(0):(_dct_index_i);          \
    strncpy((char*)(_dct_dest_pc),                                  \
            (const char*)&((_dct_src_pc)[_dct_index_i]),            \
            (size_t)MIN((_dct_dest_len_i),strlen((_dct_src_pc))+1));\
    (_dct_dest_pc)[(_dct_dest_len_i)-1] = '\0';                     \
}

/** @return A UINT64 object that provides the number of 100ns time intervals since 15 October 1582 till this function is called.
  */
M_IMPORT extern UINT64 MGregTimeIntervals();

/**Get the executable filename for running program.
  */
M_IMPORT extern bool MGetModuleFileName(CMString & p_FileName);

/** @return Last error produced by a CoreAPI function call. The error code is only 
  * valid when a function fails. If a function succeeds a call to MLastError() return code is invalid
  * @ingroup functions
  */
extern M_IMPORT long MLastError();

/** Stores corresponding error identifier of CoreAPI when an error occurs.
  * @return Previous error identifier stored.
  * @ingroup functions
  */
extern M_IMPORT long MLastError(
    long err        ///< Error identifier.
);

#endif // !__MUTILS_H__
