/***************************************************************************
        File:           $Archive: /stacks/include/mstring.h $ 
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       CMString definition
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
#ifndef __CMSTRING_H__
#define __CMSTRING_H__

/** @file mstring.h */

#include "platform.h"
#include "mbuffer.h"

/**
  * @brief Provides string manipulation functions.
  *
  * The internal character string of CMString is represented using a C zero
  * terminated string. Where possible, this uses the same API as the C++
  * standard class std::string
  */

class M_IMPORT CMString
{
M_IMPORT friend CMString operator+(const char * s1, const CMString &s2);

protected:
    // data
    char * m_string;
    int    m_capacity;
    int    m_length;

    bool Allocate(int p_new_capacity);
    bool BoundsCheck(int & p_iPosition) const;

public:
    /**
      *  Create a CMString from a null terminated string
      */
    CMString(
        const char *str=NULL    // Initial string value of the CMString object
    );

    /**
      *  Copy a CMString into another CMString
      */
    CMString(const CMString & s);

#ifdef _UNICODE
    CMString(const TCHAR * s);
#endif

    /**
      *  Destroys CMString object and free the internal storage
      */
    ~CMString();

    /**
      *  @return TRUE if the string is of zero length
      */
    inline bool empty() const { return *m_string == '\0'; }

    /**
      *  @return length of string excluding the null terminator
      */
    inline int  length() const { return m_length; }

    /**
      *  @return length of string excluding the null terminator
      */
    inline int  size() const { return m_length; }

    /**
      *  @return total capacity of memory allocated
      */
    inline int  capacity() const { return m_capacity; }

    /** Allocate at least the amount of memory specifed
      */
    void reserve(
        int p_iNewSize = 0  ///< Assure have at least this many bytes
    ) { Allocate(p_iNewSize); }


    /** Assign the source to the string.
      * @return Reference to the string being assigned to.
      */
    CMString & assign(
        const char * p_pcString    ///< Zero terminated C string
    );

    /** Assign the source to the string.
      * @return Reference to the string being assigned to.
      */
    CMString & assign(
        const char * p_pcBuffer,   ///< Point to character block
        int p_iCount               ///< Number of characters in block
    );

    /** Assign the source to the string.
      * @return Reference to the string being assigned to.
      */
    CMString & assign(
        const CMString & p_String, ///< Source string
        int p_iStartPosition = 0,  ///< Starting position in source string
        int p_iCount = INT_MAX     ///< Number of characters in sourse string
    );

    /** Assign the source to the string.
      * @return Reference to the string being assigned to.
      */
    CMString & assign(
        int p_iCount,  ///< Count of characters to insert
        char p_cValue  ///< Character to insert multiple times.
    );


    /** Append the source to the string.
      * @return Reference to the string being appended to.
      */
    CMString & append(
        const char * p_pcString    ///< Zero terminated C string
    );

    /** Append the source to the string.
      * @return Reference to the string being appended to.
      */
    CMString & append(
        const char * p_pcBuffer,   ///< Point to character block
        int p_iCount               ///< Number of characters in block
    );

    /** Append the source to the string.
      * @return Reference to the string being appended to.
      */
    CMString & append(
        const CMString & p_String, ///< Source string
        int p_iStartPosition = 0,  ///< Starting position in source string
        int p_iCount = INT_MAX     ///< Number of characters in sourse string
    );

    /** Append the source to the string.
      * @return Reference to the string being appended to.
      */
    CMString & append(
        int p_iCount,  ///< Count of characters to insert
        char p_cValue  ///< Character to insert multiple times.
    );


    /** Insert the source to the string.
      * @return Reference to the string being inserted to.
      */
    CMString & insert(
        int p_iInsertPosition,     ///< Position in string to insert new string
        const char * p_pcString    ///< Zero terminated C string
    );

    /** Insert the source to the string.
      * @return Reference to the string being inserted to.
      */
    CMString & insert(
        int p_iInsertPosition,     ///< Position in string to insert new string
        const char * p_pcBuffer,   ///< Point to character block
        int p_iCount               ///< Number of characters in block
    );

    /** Insert the source to the string.
      * @return Reference to the string being inserted to.
      */
    CMString & insert(
        int p_iInsertPosition,     ///< Position in string to insert new string
        const CMString & p_String, ///< Source string
        int p_iStartPosition = 0,  ///< Starting position in source string
        int p_iCount = INT_MAX     ///< Number of characters in sourse string
    );

    /** Insert the source to the string.
      * @return Reference to the string being inserted to.
      */
    CMString & insert(
        int p_iInsertPosition,     ///< Position in string to insert new string
        int p_iCount,  ///< Count of characters to insert
        char p_cValue  ///< Character to insert multiple times.
    );


    /** Replace the source to the string.
      * @return Reference to the string being replaced to.
      */
    CMString & replace(
        int p_iReplacePosition,    ///< Position in string to replace with new string
        int p_iReplaceCount,       ///< Count of characters to replace with new string
        const char * p_pcString    ///< Zero terminated C string
    );

    /** Replace the source to the string.
      * @return Reference to the string being replaced to.
      */
    CMString & replace(
        int p_iReplacePosition,    ///< Position in string to replace with new string
        int p_iReplaceCount,       ///< Count of characters to replace with new string
        const char * p_pcBuffer,   ///< Point to character block
        int p_iCount               ///< Number of characters in block
    );

    /** Replace the source to the string.
      * @return Reference to the string being replaced to.
      */
    CMString & replace(
        int p_iReplacePosition,    ///< Position in string to replace with new string
        int p_iReplaceCount,       ///< Count of characters to replace with new string
        const CMString & p_String, ///< Source string
        int p_iStartPosition = 0,  ///< Starting position in source string
        int p_iCount = INT_MAX     ///< Number of characters in sourse string
    );

    /** Replace the source to the string.
      * @return Reference to the string being replaced to.
      */
    CMString & replace(
        int p_iReplacePosition,  ///< Position in string to replace with new string
        int p_iReplaceCount,       ///< Count of characters to replace with new string
        int p_iCount,  ///< Count of characters to insert
        char p_cValue  ///< Character to insert multiple times.
    );

    /** Remove every occurrence of a given character from the source to the string.
      * @return Reference to the string after the replacement
      */
    CMString & remove(
        char p_cValue  ///< Character to remove
    );

    /** Erase the part of the string.
      * @return Reference to the string being erased.
      */
    CMString & erase(
        int p_iErasePosition = 0,    ///< Position in string to erase
        int p_iEraseCount = INT_MAX  ///< Count of characters to erase
    );


    /** Compare the source to the string.
      * @return < 0 for less than, 0 for equal to or > 0 for greater than
      */
    inline int compare(
        const char * p_pcString    ///< Zero terminated C string
    ) const { return strcmp(m_string, p_pcString); }

    /** Compare the source to the string.
      * @return < 0 for less than, 0 for equal to or > 0 for greater than
      */
    int compare(
        int p_iComparePosition,  ///< Position in string to compare with parameter
        int p_iCompareCount,     ///< Count of characters to to compare with parameter
        const char * p_pcString, ///< Zero terminated C string
        int p_iStartPosition = 0 ///< Starting position in parameter string
    ) const;

    /** Compare the source to the string.
      * @return < 0 for less than, 0 for equal to or > 0 for greater than
      */
    inline int compare(
        const CMString & p_String  ///< String to compare
    ) const { return strcmp(m_string, p_String.m_string); }

    /** Compare the source to the string.
      * @return < 0 for less than, 0 for equal to or > 0 for greater than
      */
    int compare(
        int p_iComparePosition,    ///< Position in string to compare with parameter
        int p_iCompareCount,       ///< Count of characters to to compare with parameter
        const CMString & p_String, ///< String to compare
        int p_iStartPosition = 0   ///< Starting position in parameter string
    ) const;

    /**  
      * Remove any leading and trailing spaces from the string
      */
    void trim( );


    /**
      *  @return Position of first matching character within the internal storage
      */
    int find(
        char p_cFind,           ///< Character to search for
        int p_iStartPos = 0     ///< Offset into string to start search
    ) const;

    /**
      *  @return Position of first matching character within the internal storage
      */
    int find(
        const char * p_pcFind, ///< Character string to search for
        int p_iStartPos = 0    ///< Offset into string to start search
    ) const;

    /**
      *  @return Position of first matching character within the internal storage
      */
    int find(
        const CMString & p_Find, ///< Character string to search for
        int p_iStartPos = 0      ///< Offset into string to start search
    ) const { return find(p_Find.m_string, p_iStartPos); }


    /**
      *  Performs a backward search of the internal storage looking for a matching character
      *  @return Position of last matching character within the internal storage
      */
    int rfind(
        char p_cFind,              ///< Character to search for
        int p_iStartPos = INT_MAX  ///< Offset into string to start search
    ) const;

    /**
      *  Performs a backward search of the internal storage looking for a matching character
      *  @return Position of last matching character within the internal storage
      */
    int rfind(
        const char * p_pcFind,     ///< Character to search for
        int p_iStartPos = INT_MAX, ///< Offset into string to start search
        int p_iCount = INT_MAX     ///< Count of characters to match in search
    ) const;

    /**
      *  Performs a backward search of the internal storage looking for a matching character
      *  @return Position of last matching character within the internal storage
      */
    int rfind(
        const CMString & p_Find,   ///< Character to search for
        int p_iStartPos = INT_MAX, ///< Offset into string to start search
        int p_iCount = INT_MAX     ///< Count of characters to match in search
    ) const { return rfind(p_Find.m_string, p_iStartPos, p_iCount); }

    /**
      *  Replaces every occurance of the given character p_cFind with the given character
      *  p_cReplace, within our internal buffer
      */
    void ReplaceAll( const char p_cFind, const char p_cReplace );

    /**
      *  Format a CMString for using the cfmt parameter. Extra parameters are supplied as variable arguments to sprintf()
      *
      * @return Number of characters formatted
      */
    int sprintf(
      const char * cfmt,   ///< C string for output format.
      ...                  ///< Extra parameters for sprintf() call.
    );

    /**
      *  Format a CMString for using the cfmt parameter. Extra parameters are supplied as variable arguments to sprintf()
      *
      * @return Number of characters formatted
      */
    int vsprintf(
      const char * cfmt,   ///< C string for output format.
      va_list p_args       ///< Extra arguments for vsprintf() call.
    );


    /** @return Substring of this string.
      */
    CMString substr(
        int p_iPosition = 0,    ///< Position to start sub string
        int p_iCount = INT_MAX  ///< Number of characters to copy
    ) const;

    /**
      * Return a const char * to the string data. On some systems, this may be equivalent to LPCSTR
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return const char * to the internal string data
      */
    inline const char * c_str() const { return m_string; }

    /**
      * Return a const char * to the string data. On some systems, this may be equivalent to LPCSTR
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return const char * to the internal string data
      */
    inline const char * data() const { return m_string; }

    // operators

    /**
      * Return a char to the string data at the index.
      *
      * @return const char * to the internal string data
      */
    inline char operator[](int i) const { return i >= 0 && i < m_length ? m_string[i] : '\0'; }

    /**
      * Return a char reference to the string data at the index.
      *
      * @return char & to the internal string data
      */
    inline char & operator[](int i) { return m_string[i]; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if the two strings are the same
      */
    inline bool operator==(const char * p_str) const { return compare(p_str) == 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if the two strings are the same
      */
    inline bool operator==(const CMString &p_str) const { return compare(p_str) == 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if the two strings are different
      */
    inline bool operator!=(const char * p_str) const { return compare(p_str) != 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if the two strings are different
      */
    inline bool operator!=(const CMString &p_str) const { return compare(p_str.m_string) != 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if string lexically earlier than the parameter
      */
    inline bool operator<(const char * p_str) const { return compare(p_str) < 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if string lexically earlier than the parameter
      */
    inline bool operator<(const CMString &p_str) const { return compare(p_str.m_string) < 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if string lexically later than the parameter
      */
    inline bool operator>(const char * p_str) const { return compare(p_str) > 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if string lexically later than the parameter
      */
    inline bool operator>(const CMString &p_str) const { return compare(p_str.m_string) > 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if string same or lexically earlier than the parameter
      */
    inline bool operator<=(const char * p_str) const { return compare(p_str) <= 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if string same or lexically earlier than the parameter
      */
    inline bool operator<=(const CMString &p_str) const { return compare(p_str.m_string) <= 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if string same or lexically later than the parameter
      */
    inline bool operator>=(const char * p_str) const { return compare(p_str) >= 0; }

    /**
      * Compare the contents of the string with another string
      *
      * @return true if string same or lexically later than the parameter
      */
    inline bool operator>=(const CMString &p_str) const { return compare(p_str.m_string) >= 0; }

    /**
      * Replace the contents of the string with another string
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return Reference to the CMString
      */
    CMString& operator=(const CMString &p_String) { return assign(p_String); }

    /**
      * Replace the contents of the string with another string
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return Reference to the CMString
      */
    CMString& operator=(const char * p_pcString) { return assign(p_pcString); }

    /**
      * Replace the contents of the string with a single character
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return Reference to the CMString
      */
    CMString& operator=(char p_cValue) { return assign(1, p_cValue); }

    /**
      * Replace the contents of the string with another string
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return Reference to the CMString
      */
    CMString& operator=(const unsigned char* p_pucString) { return assign((const char *)p_pucString); }

    /**
      * Concatenate another CMString to the string returning a third
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return New CMString
      */
    CMString operator+(const CMString &s) const;

    /**
      * Concatenate another CMString to the string returning a third
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return New CMString
      */
    CMString operator+(const char *s) const;

    /**
      * Concatenate another CMString to the string
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return Reference to the CMString
      */
    CMString& operator+=(const CMString & str) { return append(str); }

    /**
      * Concatenate a NULL terminated string to the string
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return Reference to the CMString
      */
    CMString& operator+=(const char *p_String) { return append(p_String); }

    /**
      * Concatenate a single character to the string
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return Reference to the CMString
      */
    CMString& operator+=(char p_cValue) { return append(1, p_cValue); }

    /**
      * Search the string for the given string
      *
      * Note that this pointer may be invalidated if the an operator or sprintf is used
      *
      * @return Reference to the CMString
      */
    bool contains(const CMString & p_strKeyword) const;



#ifdef __AFX_H__
    operator CString() const { return CString(m_string); }
#endif
};

#ifdef _UNICODE
class CMUnicode
{
public:
    CMUnicode();
    CMUnicode(const char *);
    CMUnicode(const TCHAR *);
    CMUnicode(const CMString &);
    CMUnicode(const CMUnicode &);
    CMUnicode &operator=(const CMUnicode &);
    ~CMUnicode();

    int Length() const { return wcslen(m_unicode); }
    operator const TCHAR *() const { return m_unicode; }

protected:
    TCHAR * m_unicode;
};
#endif


#endif // !__CMSTRING_H__
