/***************************************************************************
        File:           $Archive: /stacks/include/mtime.h $ 
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Eric Wang
        Synopsys:       Time manipulation
        Copyright 2001-2003 by Dilithium Networks.
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
/******************************************************************************
    History:

     1/Jul/2001 EW  Initial work on both CMTime and CMTimeSpan classes has
                    completed, changes are required to be documented after this
                    date.
    25/Jun/2001 EW  Creation of both CMTime and CMTimeSpan classes.
 ******************************************************************************/

/** @file mtime.h
 */

#if !defined(__MTIME_H__)
#define __MTIME_H__

#include "platform.h"

#if !defined(UNDER_CE)
    #include <assert.h>
    #include <time.h>
#else
    #define assert(a)
#endif

#if defined(WIN32)
    #include <Windows.h>
#endif  // defined(WIN32)

typedef UINT MTime;

// Universal constants relating to time.
#define MILLISECONDS_IN_SECOND  1000
#define MILLISECONDS_IN_MINUTE  60000
#define MILLISECONDS_IN_HOUR    3600000
#define MILLISECONDS_IN_DAY     86400000
#define SECONDS_IN_MINUTE       60
#define SECONDS_IN_HOUR         3600
#define MINUTES_IN_HOUR         60
#define HOURS_IN_DAY            24
#define DAYS_IN_NORMAL_YEAR     365
#define DAYS_IN_LEAP_YEAR       366
#define MONTHS_IN_YEAR          12
#define MONTH_FIRST             1
#define MONTH_LAST              12

/**
  * Defines the various components of a CMTIme
  */
enum MTIME_COMPONENT 
{
    MTIME_MILLISECOND       = 0,
    MTIME_SECOND,
    MTIME_MINUTE,
    MTIME_HOUR,
    MTIME_DAY,
    MTIME_MONTH,
    MTIME_YEAR,
    MTIME_NUM_COMPONENTS,
    MTIME_DFLT_RESOLUTION = MTIME_MILLISECOND
};

// Forward declaration.
class CMTimeSpan;

/**
  * @brief Represents an absolute time and date.
  */
class CMTime {

public:
    /**
      * Create a CMTime containing the current time
      */
    CMTime();

    /**
      * Create a CMTime from a string
      */
    CMTime(
        const char * str  ///< a structure defining a time after midnight, January 1, 1970
    );

    /**
      * indicates success or failure on setting time components
      */
    enum Status {
	Success,
	BadArgument
    };

    // Time specification functions.
    Status GetLocalTime();

    /**
      * Set the resolution of the stored time
      * @return Success or Failure as per CMTIme::Status
      */
    Status SetResolution(MTIME_COMPONENT uiResolution);

    /**
      * Set the various components of the stored time
      * @return Success or Failure as per CMTIme::Status
      */
    Status SetTime(
        UINT uiYear,            // year value
        UINT uiMonth,           // month value
        UINT uiDay,             // day value
        UINT uiHour,            // hour value
        UINT uiMinute,          // minute value
        UINT uiSecond = 0,      // seconds value
        UINT uiMillisecond = 0  // milliseconds value
    );

    /**
      * Set the year component of the stored time
      * @return Success or Failure as per CMTIme::Status
      */
    Status SetYear(
        UINT uiYear
    );

    /**
      * Set the months component of the stored time
      * @return Success or Failure as per CMTIme::Status
      */
    Status SetMonth(
        UINT uiMonth
    );

    /**
      * Set the days component of the stored time
      * @return Success or Failure as per CMTIme::Status
      */
    Status SetDay(
        UINT uiDay
    );

    /**
      * Set the hours component of the stored time
      * @return Success or Failure as per CMTIme::Status
      */
    Status SetHour(
        UINT uiHour
    );

    /**
      * Set the minutes component of the stored time
      * @return Success or Failure as per CMTIme::Status
      */
    Status SetMinute(
        UINT uiMinute
    );

    /**
      * Set the seconds component of the stored time
      * @return Success or Failure as per CMTIme::Status
      */
    Status SetSecond(
        UINT uiSecond
    );

    /**
      * Set the millisecond component of the stored time
      * @return Success or Failure as per CMTIme::Status
      */
    Status SetMillisecond(
        UINT uiMillisecond
    );

    /**
      * @return milliseconds component of stored time
      */
    int millisecond() const;

    /**
      * @return seconds component of stored time
      */
    int second() const;

    /**
      * @return minutes component of stored time
      */
    int minute() const;

    /**
      * @return hours component of stored time
      */
    int hour() const;

    /**
      * @return days component of stored time
      */
    int day() const;

    /**
      * @return months component of stored time
      */
    int month() const;

    /**
      * @return years component of stored time
      */
    int year() const;

    /**
      * @return a component of stored time
      */
    int operator[](
        UINT uiComponent
    ) const;

    /** Convert time to a string in format yyyy/mm/dd hh:mm:ss.uuu
      * @return pointer passed in as p_pcBuffer or NULL if failed.
      */
    char * AsString(
        char * p_pcBuffer,      /// Buffer to receive string
        int p_iSize             /// Size of buffer
    ) const;

    /**
      * @return TRUE if year component is a leap year
      */
    BOOL IsLeapYear() const;

    /**
      * Assign a new value to the time
      * @return reference to object
      */
    CMTime& operator=(const CMTime & timeSrc);

    /**
      * Add the specified time interval to the stored time
      * @return reference to object
      */
    CMTime& operator+=(const CMTimeSpan & timeSpan);
    CMTime& operator+=(int iTimeSpan);

    /**
      * Substract the specified time interval from the stored time
      * @return reference to object
      */
    CMTime& operator-=(const CMTimeSpan & timeSpan);
    CMTime& operator-=(int iTimeSpan);
    friend CMTimeSpan operator-(CMTime time_1, CMTime time_2);

    /**
      * Compare two times
      */
    BOOL operator==(const CMTime & time_2) const;
    BOOL operator>(const CMTime & time_2) const;
    BOOL operator>=(const CMTime & time_2) const;

    /**
      * Calculate the number of days between this time and the given time
      */
    int DaysFrom(const CMTime & time);

    /**
      * Get the time in seconds since 1 Jan 1970
      */
    double GetEpochTime();

private:
    // Time computation functions.
    void AddTimeSpan(UINT uiTimeSpan);
    void SubtractTimeSpan(UINT uiTimeSpan);

    // Variables.
    int     m_iResolution;                          // the time resolution when
                                                    // performing computations.
    int     m_iTimeData[MTIME_NUM_COMPONENTS];      // the time components.

};

/**
  * @brief Represents a time interval, or a relative time with respect to a reference time frame.
  *
  * Notes:
  *     -# All the member functions of CMTimeSpan are defined as inlines.  
  *        This means that the size of a CMTimeSpan object is limited to the size of an int, 
  *        which is 4 bytes on most platforms.
  *     -# The time interval represented by a CMTimeSpan object is intended to have a resolution 
  *        of 1 msec.  Whether this resolution is achieved in reality depends largely on the system 
  *        platform on which the object is run.
  *     -# Since the time interval is stored with a resolution of 1 msec, this limits the maximum 
  *        interval to around 596 hours (or 24.8 days) on a platform where the size of an int is 4 bytes.
  */
class CMTimeSpan
{
public:
    /**
     * Create a zero duration time interval
     */
    CMTimeSpan();

    /**
     * Create time interval with the specified duration
     */
    CMTimeSpan(
        int iMilliseconds       ///< time interval in milliseconds
    );

    /**
     * @return time interval in milliseconds
     */
    int in_milliseconds() const;

    /**
     * @return time interval in seconds
     */
    int in_seconds() const;

    /**
     * @return time interval in minutes
     */
    int in_minutes() const;

    /**
     * @return time interval in hours
     */
    int in_hours() const;

    /**
     * @return time interval in days
     */
    int in_days() const;

    /**
      * Assign a new value to the time interval
      */
    CMTimeSpan& operator=(CMTimeSpan timeSpan);
    CMTimeSpan& operator=(int iMilliseconds);

    /**
      * Add the specified time interval to the stored time interval
      * @return reference to object
      */
    CMTimeSpan& operator+=(CMTimeSpan timeSpan);
    CMTimeSpan& operator+=(int iMilliseconds);

    /**
      * Subtract the specified time interval from the stored time interval
      * @return reference to object
      */
    CMTimeSpan& operator-=(CMTimeSpan timeSpan);
    CMTimeSpan& operator-=(int iMilliseconds);

    /**
      * Compare two time intervals
      */
    BOOL operator==(CMTimeSpan span_2) const;
    BOOL operator==(int iMilliseconds) const;
    BOOL operator!=(CMTimeSpan span_2) const;
    BOOL operator!=(int iMilliseconds) const;
    BOOL operator>(CMTimeSpan span_2) const;
    BOOL operator>(int iMilliseconds) const;
    BOOL operator>=(CMTimeSpan span_2) const;
    BOOL operator>=(int iMilliseconds) const;
    BOOL operator<(CMTimeSpan span_2) const;
    BOOL operator<(int iMilliseconds) const;
    BOOL operator<=(CMTimeSpan span_2) const;
    BOOL operator<=(int iMilliseconds) const;

private:
    int     m_iMilliseconds;    // the time interval in msecs.

};

// Additional time related functions.
extern BOOL IsLeapYear(UINT uiYear);

// Additional operators for CMTime and CMTimeSpan classes.
extern CMTime operator+(CMTime initTime, CMTimeSpan timeSpan);
extern CMTime operator+(CMTime initTime, int iTimeSpan);
extern CMTime operator-(CMTime initTime, CMTimeSpan timeSpan);
extern BOOL operator!=(CMTime time_1, CMTime time_2);
extern BOOL operator<(CMTime time_1, CMTime time_2);
extern BOOL operator<=(CMTime time_1, CMTime time_2);
extern CMTimeSpan operator-(CMTime time_1, CMTime time_2);
extern CMTimeSpan operator+(CMTimeSpan span_1, CMTimeSpan span_2);
extern CMTimeSpan operator-(CMTimeSpan span_1, CMTimeSpan span_2);


#endif  // !__MTIME_H__
