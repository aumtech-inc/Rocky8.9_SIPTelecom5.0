/*-----------------------------------------------------------------------------
Program:	conv.c
Purpose:	These appear to be the conversion routines that are used
		by Service Creation. If so, this is probably not the right
		place to store them.
Author:		Deepak Patil.
Date Written:	Unknown
Last Update:	12/05/95
Last Update by:	George Wenzel (just addeded this header; no code changes.)
-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <ISPCommon.h>

#if 0
typedef char *Str;
typedef char Character;
typedef Str Date;
typedef float Float;
typedef long Long;
typedef double Double;
typedef int Integer;
typedef Str Phone;
typedef Str SocialSecurity;
typedef Str String;
typedef Str Time;
#endif

#if 0
extern void IntToString (Integer Source, String Destination);
extern void LongToString (Long Source, String Destination);
extern void DoubleToString (Double Source, String Destination);
extern void FloatToString (Float Source, String Destination);
extern void DateToString (Date Source, String Destination);
extern void PhoneToString (Phone Source, String Destination);
extern void SSNToString (SocialSecurity Source, String Destination);
extern void TimeToString (Time Source, String Destination);
extern void StringToInt (String Source, Integer * Destination);
extern void StringToFloat (String Source, Float * Destination);
extern void StringToDouble (String Source, Double * Destination);
extern void StringToLong (String Source, Long * Destination);
extern void StringToDate(String Source, Date Destination);
extern void StringToPhone(String Source, Phone Destination);
extern void StringToSSN(String Source, SocialSecurity Destination);
extern void StringToTime(String Source, Time Destination);
#else
#endif

#define MY_FLOAT     7610.567123
#ifdef DEEP_DBG
main()
{
  char xx[50];
  char yy[50];

  sprintf(xx, "%s", "10/12/1994");
  DateToString(xx, yy);
  printf(" yy=%s\n", yy);
}
#endif

int StringToInt(String Source, Integer * Destination)
{
  *Destination = atoi(Source);
}

/*
 * there is precision problem here
 * (two digits after decimal point only)
 */
int StringToLong( String Source, Long * Destination)
{
  sscanf(Source, "%ld", Destination);
}

int StringToDouble( String Source, Double * Destination)
{
  sscanf(Source, "%f", Destination);
}

int StringToFloat( String Source, Float * Destination)
{
  sscanf(Source, "%f", Destination);
}

int IntToString( Integer Source, String Destination)
{
  sprintf(Destination, "%d", Source);
}

/*
 * precision here is 2 digits after decimal point
 */
int LongToString( Long Source, String Destination)
{
  sprintf(Destination, "%ld", Source);
}

int DoubleToString( Double Source, String Destination)
{
  sprintf(Destination, "%f", Source);
}

int FloatToString( Float Source, String Destination)
{
  sprintf(Destination, "%f", Source);
}

int DateToString (Date Source, String Destination)
{
  sprintf(Destination, "%s", Source);
}

int PhoneToString (Phone Source, String Destination)
{
  sprintf(Destination, "%s", Source);
}

int SSNToString (SocialSecurity Source, String Destination)
{
  sprintf(Destination, "%s", Source);
}

int TimeToString (Time Source, String Destination)
{
  sprintf(Destination, "%s", Source);
}

int StringToDate(String Source, Date Destination)
{
  sprintf(Destination, "%s", Source);
}

int StringToPhone(String Source, Phone Destination)
{
  sprintf(Destination, "%s", Source);
}

int StringToSSN(String Source, SocialSecurity Destination)
{
  sprintf(Destination, "%s", Source);
}

int StringToTime(String Source, Time Destination)
{
  sprintf(Destination, "%s", Source);
}
