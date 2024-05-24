/*****************************************************************************/
/* Copyright (C) 1999-2001 OSS Nokalva.  All rights reserved.                */
/*****************************************************************************/

/* THIS FILE IS PROPRIETARY MATERIAL OF OPEN SYSTEMS SOLUTIONS, INC.
 * AND MAY BE USED ONLY BY DIRECT LICENSEES OF OPEN SYSTEMS SOLUTIONS, INC.
 * THIS FILE MAY NOT BE DISTRIBUTED. */

/*****************************************************************************/
/*  FILE: @(#)printasn.h	9.2  00/11/08                   */
/*                                                                           */

#include "asn1hdr.h"
#include "ossdll.h"

#ifndef storing
#ifndef coding
#ifndef OSS_TOED
#include "ossglobl.h"
#endif /* not OSS_TOED */
#endif /* not coding */
#endif /* not storing */

#ifndef PRINTASN_H
#define PRINTASN_H

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_MSC_VER) && (defined(_WIN32) || defined(WIN32))
#pragma pack(push, ossPacking, 4)
#elif defined(_MSC_VER) && (defined(_WINDOWS) || defined(_MSDOS))
#pragma pack(1)
#elif defined(__BORLANDC__) && defined(__MSDOS__)
#ifdef _BC31
#pragma option -a-
#else
#pragma option -a1
#endif /* _BC31 */
#elif defined(__BORLANDC__) && defined(__WIN32__)
#pragma option -a4
#elif defined(__IBMC__) && (defined(__WIN32__) || defined(__OS2__))
#pragma pack(4)
#elif defined(__WATCOMC__) && defined(__NT__)
#pragma pack(push, 4)
#elif defined(__WATCOMC__) && (defined(__WINDOWS__) || defined(__DOS__))
#pragma pack(push, 1)
#endif /* _MSC_VER && _WIN32 */

#ifdef macintosh
#pragma options align=mac68k
#endif


/* Runtime support for ossPrintPER user API */
struct PrintPerSimplestRecordAddition {
         char          *comments;
         ossBoolean     commentsAllocMem;
         unsigned char *reserved;
};

struct PrintPerSimplestRecord {
	/* For a definition of these fields see the PrintPerRecord-structure */
         unsigned char   typeId;
         unsigned long   length;
         unsigned char  *encodedContent;
         unsigned char   encodedBitOffset;

         struct PrintPerSimplestRecordAddition *addition;
};

struct PrintPerRecordAddition {
         char                  *typeInformation;
         struct PrintPerRecord *top_level_record;
         ossBoolean             optional;
         void                  *internal_reserv;	
         void                  *internal_reserv2;
         unsigned char          *reserved;
};

struct PrintPerRecord {
         unsigned char  typeId; /* identifies the present record type        */
   
         ossBoolean     possibleLast; /* used when typeId = OSS_ASN1_TYPE_PPR 
                                         TRUE, if there is a chance that this 
                                         value is the last element of 
                                         constructed type                   */

         void *decodedContent; /* points to the decoded content in "displayed"
                                  format                                    */
         char *typeName;       /* points to the name of ASN.1 type          */
         char *fieldName;      /* points to the name of ASN.1 name          */

         char *qualifiedName;            /* points to the "full" name       */
         char *qualifiedNameOfTopRecord; /* points to full name of the top   
                                            level record                    */ 
         char *qualifiedNumber;          /* pointer to the identifer of the  
                                            record in the set of records    
                                            for example, 1.2.1.3            */
         char *qualifiedNumberOfTopRecord; /* points to the identifier of   
                                              top record; for example, 1.2.1 */

         unsigned int   depth;

         unsigned char *encodedContent;   /* points to the encoding for the   
                                             record                           */  
         unsigned char  encodedBitOffset; /* the offset of the encoding       */

         unsigned long  length;           /* length of encoding(in bits)      */

         unsigned int   numberOfSimplestRecord; /* number of simplestRecords  */
         struct PrintPerSimplestRecord *simplestRecords; /* pointer to an     
                                                            array of logical  
                                                            parts             */

         unsigned char bitsPerContentUnit; /* usually - 8, but could be differ
                                              for strings with constraints  
                                              specified                       */

         unsigned int offset; /* offset from a start of encoded data */ 

         ossBoolean allocMem; /* TRUE if decodedContent points to 
                               * allocated memory                  */
         
         struct PrintPerRecordAddition *addition; 
};

#if defined(_MSC_VER) && (defined(_WIN32) || defined(WIN32))
#pragma pack(pop, ossPacking)
#elif defined(_MSC_VER) && (defined(_WINDOWS) || defined(_MSDOS))
#pragma pack()
#elif defined(__BORLANDC__) && (defined(__WIN32__) || defined(__MSDOS__))
#pragma option -a.
#elif defined(__IBMC__) && (defined(__WIN32__) || defined(__OS2__))
#pragma pack()
#elif defined(__WATCOMC__) && (defined(__NT__) || defined(__WINDOWS__) || defined(__DOS__))
#pragma pack(pop)
#endif /* _MSC_VER && _WIN32 */

#ifdef macintosh
#pragma options align=reset
#endif

/*** Print Per flags  ***/
/* Do not print any encoding representation, decoded values only */
#define OSS_ASN_ONLY            0x00001L

/* Print full bytes in hex format, other data - in binary        */
#define OSS_HEXBYTES             0x00002L

/* Print each ASN.1 type after an empty line (for readability)   */
#define OSS_SEPARATE_ASN         0x00004L

/* Do not print braces ( "{" and "}" ) in output                 */
#define OSS_NOBRACES             0x00008L

/* Print absolute references                                     */
#define OSS_PRINT_ABSREF         0x00010L

/* Print length and offset information                           */
#define OSS_PRINT_OFFSET         0x00020L

/* Print qualified item numbers on each line                      */
#define OSS_PRINT_NUMBERS        0x00040L

/* Do not print by ossPrintPER itself                             */
#define OSS_NOPRINT              0x00080L

/* Print an ASN.1 type description                                */
#define OSS_PRINT_TYPE_INFO      0x00200L

/* Print some useful comments                                     */
#define OSS_PRINT_COMMENTS      0x00400L

/* Do not use print functions defined via OSS.PrintFunctionName
for formatting world->decodedContent (use for printing only)      */
#define OSS_DONT_FORMAT_DECODED_CONTENT 0x00800L

/* Valid values for field typeId in the PrintPerRecord structure  */
#define OSS_CONTENTS_TYPE_PPR             0
#define OSS_LENGTH_TYPE_PPR               1
#define OSS_PADDING_TYPE_PPR              2
#define OSS_ASN1_TYPE_PPR                 3
#define OSS_EXTENSION_LENGTH_TYPE_PPR     4
#define OSS_EXTENSION_RANGE_TYPE_PPR      5
#define OSS_CHOICE_INDEX_PPR              6
#define OSS_PDU_PADDING_TYPE_PPR          8
#define OSS_REALINF_TYPE_PPR              9
#define OSS_EXPLENGTH_TYPE_PPR            10
#define OSS_EXP_TYPE_PPR                  11
#define OSS_MANTISSA_TYPE_PPR             12
#define OSS_UNKNOWN_EXTENSION_PPR         13
#define OSS_LENGTH_LEADING_BIT_PPR        14
#define OSS_EXTENSION_FLAG_TYPE_PPR       15
#define OSS_EXTENSION_COUNT_TYPE_PPR      16
#define OSS_EXTENSION_PREAMBLE_TYPE_PPR   17
#define OSS_PREAMBLE_TYPE_PPR             18
#define OSS_TRAILING_BITS_PPR             19 
#define OSS_CHOICE_BEGIN_PPR              20
#define OSS_SEQ_SET_BEGIN_PPR             21
#define OSS_CHOICE_FINISHED_PPR           22
#define OSS_SEQ_SET_FINISHED_PPR          23
#define OSS_ADDITIONS_TYPE_PPR            24
#define OSS_VERSION_BRACKETS_BEGIN_PPR    25
#define OSS_VERSION_BRACKETS_FINISHED_PPR 26
#define OSS_EXT_COUNT_SIZE_FLAG_PPR       27
#define OSS_EXT_COUNT_SIZE_PPR            28
#define OSS_CHOICE_INDEX_SIZE_FLAG_PPR    29
#define OSS_CHOICE_INDEX_LENGTH_PPR       30
#define OSS_OPENTYPE_LENGTH_TYPE_PPR      31


typedef ossBoolean (DLL_ENTRY_FPTR *_System UserPrintPer)
        (struct ossGlobal *world, struct PrintPerRecord *data);

PUBLIC int  DLL_ENTRY ossPrintPER(struct ossGlobal *world,
                                  int              *pdunum,
                                  OssBuf           *input,
                                  void            **output,
                                  long              flags,
                                  UserPrintPer      userPrintPer); 


#ifdef __cplusplus
}
#endif

#endif /* PRINTASN_H */
