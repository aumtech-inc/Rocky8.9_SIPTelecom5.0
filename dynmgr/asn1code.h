/*****************************************************************************/
/* Copyright (C) 1989-2001 OSS Nokalva.  All rights reserved.                */
/*****************************************************************************/

/* THIS FILE IS PROPRIETARY MATERIAL OF OPEN SYSTEMS SOLUTIONS, INC.
 * AND MAY BE USED ONLY BY DIRECT LICENSEES OF OPEN SYSTEMS SOLUTIONS, INC.
 * THIS FILE MAY NOT BE DISTRIBUTED. */

/*****************************************************************************/
/* FILE: @(#)asn1code.h	9.9  01/03/30                       */
/*****************************************************************************/
#ifndef ASN1CODE_H
#define ASN1CODE_H

#include <stdio.h>
#include <stddef.h>
#include "asn1hdr.h"
#include "ossdll.h"

#define ERR_MSG_LENGTH 512      /* length of error messages to be output */
/*** encoder/decoder flags ***/
#define DEBUGPDU 0x02           /* produce tracing output */
#define BUFFER_PROVIDED 0x04    /* use caller-provided buffer */
#define RESTRAIN 0x08           /* limit output buffer to user-specified size*/
#define NOTRAPPING 0x200        /* do no signal trapping */
#define NOCONSTRAIN 0x800       /* ignore calling constraint checker */
#define AUTOMATIC_ENCDEC 0x2000 /* do automatic encoding/decoding of
				 * open types */

/*** encoder flags ***/
#define DEFINITE 0x10           /* force definite-length encoding */
#define INDEFINITE 0x20         /* force indefinite-length encoding */
#define FRONT_ALIGN 0x80        /* align output to front of output buffer */
#define BACK_ALIGN 0x100        /* align output to back of output buffer */
#define DEFAULT_ALIGN 0         /* use most efficient align (back or front) */
#define DETERMINE_ENC_LENGTH 0x40 /* generate only total encoding length
                                 * (for use in user memory managers) */

/*** decoder flags ***/
#define IGNORE_PDU_TAG 0x1      /* on second decoding do not issue an error
				 * but ignore an invalid PDU tag in an open
				 * type encoding of a field with IMPLICIT tag
				 * marked with the directive DeferDecoding */
#define DEBUG_ERRORS 0x10       /* same as DEBUGPDU */
#define OSS_RELAXED 0x400       /* use relaxed decoding rules */
#define RELAXBER OSS_RELAXED    /* use relaxed BER */
#define RELAXPER OSS_RELAXED    /* use relaxed PER */
#define OBJECT_HANDLE 0x1000    /* temporarily mark object of type with
				 * NOCOPY directive to not free it by
				 * ossFreePDU() (for use in user memory
				 * managers) */
#define FREE_ENCODED 0x10000    /* when calling ossFreePDU() and using
				 * DeferDecoding and OBJHANDLE directives,
				 * free the field "encoded" of OpenTypeExtended */
#define DONT_FREE_ENCODED 0x40000 /* when automatically decoding, don't free
				 * memory allocated for encoded OpenType values
				 * by the first outermost call of the decoder
				 * for enclosing PDU */
#define DONT_DO_ENCODED 0x80000 /* when calling ossCpyValue()/ossCmpValue(),
				 * don't copy/compare encoded OpenType values */
#define DONT_DO_DECODED 0x100000 /* when calling ossCpyValue()/ossCmpValue(),
				 * don't copy/compare decoded OpenType values */
#define SMART_POINTER   0x200000 /* temporarily set to inform memory manager
				  * that it is supposed to treat input handle
				  * (or to return output handle) as smart pointer
				  * rather than plain memory address (for use
				  * in custom memory manager) */ 
#define IGNORE_DEFER_DECODING 0x400000 /* force runtime to automatically process
				    values with DeferDecoding directive applied */

/*** compatibility flags ***/

#define OSS_V412_TIME_AND_WIDE_CHAR_STRINGS          0x01
#define OSS_TRUNCATE_0_SECONDS_FROM_GENERALIZED_TIME 0x02
#define OSS_TRUNCATE_0_SECONDS_FROM_UTC_TIME         0x04
#define OSS_EXTENDED_UNRESTRICTED_CHAR_STRINGS       0x08
#define OSS_ALLOW_ZERO_LENGTH_OPENTYPE_STRINGS	     0x10
#define OSS_ALLOW_TIME_DIFFERENTIAL_IN_UTC_TIME      0x20
#define OSS_ALLOW_TIME_DIFFERENTIAL                  0x20
#define OSS_PACK_WIDE_STRINGS                        0x40
#define OSS_AUTO_ENCODE_WITHOUT_CHECKING_CONSTRAINT  0x80

/*** reserved flags for internal use ***/
#define RESERVED_FLAG1 0x8000
#define RESERVED_FLAG2 0x4000
#define RESERVED_FLAG3 0x20000

/* Erorr of dynamic linking loader */
#define CANT_LOAD_DYNLIBRARY                       97 /* error when opening dynamic library */
#define CANT_UNLOAD_DYNLIBRARY                        98 /* error when closing dynamic library */

/* LED compatibility error return codes */
#define OSS_LEAN_BAD_VERSION                    87 /* versions of LEAN
			    * encoder/decoder and control-table do not match */
#define OSS_LEAN_AUTOMATIC_ENCDEC_NOT_SUPPORTED 88 /* The AUTOMATIC_ENCDEC
			* directive is not supported for LEAN endoder/decoder */
#define OSS_LEAN_RELAYSAFE_NOT_SUPPORTED        89 /* The relaySafe mode
			    * is not supported for LEAN endoder/decoder */
#define OSS_LEAN_DER_ENCODER_NOT_SUPPORTED      90 /* The DER encoding rules
			    * are not supported for LEAN endoder/decoder */
#define OSS_LEAN_REALTYPE_NOT_SUPPORTED         91 /* The REAL type
			    * is not supported for LEAN endoder/decoder */
#define OSS_LEAN_REMOVE_NOT_SUPPORTED           92 /* The REMOVE directive
			    * is not supported for LEAN endoder/decoder */
#define OSS_LEAN_NOCOPY_NOT_SUPPORTED           93 /* The NOCOPY directive
			    * is not supported for LEAN endoder/decoder */
#define OSS_LEAN_DEFERDECODING_NOT_SUPPORTED    94 /* The DeferDecoding
			* directive is not supported for LEAN endoder/decoder */
#define OSS_LEAN_USERHOOKS_NOT_SUPPORTED        95 /* The user hook directives
			    * are not supported for LEAN endoder/decoder */
#define OSS_LEAN_TRUNCATE_SOF_NOT_SUPPORTED     96 /* The Truncate directive
			    * is not supported for LEAN decoder */

/* PER tracing error return codes */
#define PRINTPER_DLL_NOT_LINKED    84 /* PER tracing DLL was not linked */ 

/*** standalone constraint checker return codes ***/
#define USER_CONSTRAINT_FUNCTION_ERROR               83
#define SINGLE_VALUE_CONSTRAINT_SIGNED_INTEGER       82
#define SINGLE_VALUE_CONSTRAINT_UNSIGNED_INTEGER     81
#define SINGLE_VALUE_CONSTRAINT_REAL                 80
#define SINGLE_VALUE_CONSTRAINT_STRING               79
#define SINGLE_VALUE_CONSTRAINT_COMPLEX_TYPE         78
#define VALUE_RANGE_CONSTRAINT_SIGNED_INTEGER        77
#define VALUE_RANGE_CONSTRAINT_UNSIGNED_INTEGER      76
#define VALUE_RANGE_CONSTRAINT_REAL                  75
#define SIZE_CONSTRAINT_STRING                       74
#define SIZE_CONSTRAINT_SETOF_SEQOF                  73
#define PERMITTED_ALPHABET_CONSTRAINT                72
#define ABSENCE_CONSTRAINT                           71
#define PRESENCE_CONSTRAINT                          70
#define TABLE_CONSTRAINT                             69
#define COMPONENT_RELATION_CONSTRAINT                68
#define VALUE_NOT_AMONG_ENUMERATED_CONSTRAINT        67
#define TYPE_CONSTRAINT                              66


/*** ascii/ebcdic conversion error return codes *****/
#define CANT_OPEN_TO_ASCII         65 /* error when opening iconv tables */
#define CANT_OPEN_FROM_ASCII       64 /* error when opening iconv tables */
#define ASCII_EBCDIC_CONV_ERROR    63 /* error in iconv call */
#define CANT_CLOSE_TO_ASCII        62 /* error when closing iconv tables */
#define CANT_CLOSE_FROM_ASCII      61 /* error when closing iconv tables */

/*** common return codes ***/
#define CANT_OPEN_INPUT_FILE		86 /* error while opening input file */
#define CANT_OPEN_ENCODER_OUTPUT_FILE 60 /* error when opening encoder output file */
#define CANT_CLOSE_ENCODER_OUTPUT_FILE 59/* error when opening encoder output file */
#define ENCODER_OUTPUT_FILE_ALREADY_OPEN 58/* encoder output file is already opened */

#define CANT_OPEN_DECODER_INPUT_FILE 57 /* error when opening decoder input file */
#define CANT_CLOSE_DECODER_INPUT_FILE 56 /* error when opening decoder input file */
#define DECODER_INPUT_FILE_ALREADY_OPEN 55 /* decoder input file is already opened */
#define ERROR_READING_FROM_DECODER_INPUT_FILE 54 /* error reading from decoder
                                       * input file */
#define INFO_OBJECT_SET_NOT_EXTENSIBLE 53 /* information object set is not
                                       * extensible */
#define INFO_OBJECT_NOT_FOUND      52 /* information object was not found */
#define INFO_OBJECT_UNIQUE_FIELD_NOT_FOUND 51 /* unique field was not found
                                       * in information object set */
#define INFO_OBJECT_ALREADY_EXISTS 50 /* object already exists */
#define INFO_OBJECT_SET_TABLE_COPY_ERROR 49 /* error when copying information
                                       * object set table */
#define START_BUF_NOT_SET          48 /* initial buffer to start decoding from
                                       * is not set */
#define CANT_SET_START_BUF         47 /* initial buffer to start decoding from
                                       * cannot be set */
#define CANT_CLOSE_TRACE_FILE      46 /* error when closing a trace file */
#define MUTEX_NOT_CREATED          45 /* mutex was not created */
#define OPEN_TYPE_ERROR            44 /* error in automatic encoding/decoding/
                                       * copying an open type */
#define PER_DLL_NOT_LINKED         43 /* PER DLL was not linked */
#define BERDER_DLL_NOT_LINKED      42 /* BER/DER DLL was not linked */
#define API_DLL_NOT_LINKED         41 /* API DLL was not linked */
#define OPEN_TYPE_USER_FIELD_ERROR 40 /* error was returned by a user-defined
                                       * user field copying/comparing function */
#define TRACING_CODE_NOT_LINKED    39 /* tracing code not linked or initialized */
#define MEM_MGR_DLL_NOT_LINKED     38 /* memory manager DLL was not linked */
#define COMPARATOR_CODE_NOT_LINKED 37 /* value comparator code was not linked */
#define COMPARATOR_DLL_NOT_LINKED  36 /* value comparator DLL was not linked */
#define CONSTRAINT_DLL_NOT_LINKED  35 /* constraint checker DLL was not linked */
#define COPIER_DLL_NOT_LINKED      34 /* value copier DLL was not linked */
#define OUT_OF_RANGE               33 /* parameter value range error */
#define REAL_CODE_NOT_LINKED       32 /* REAL code was not linked */
#define REAL_DLL_NOT_LINKED        31 /* REAL DLL was not linked */
#define TYPE_NOT_SUPPORTED         30 /* ASN.1 type is not supported */
#define TABLE_MISMATCH             29 /* C++ API: PDUcls function called with
				       * a ossControl object which refers to
				       * control table different from one the
				       * PDU was defined in */
#define TRACE_FILE_ALREADY_OPEN    28 /* the trace file has been opened */
#define CANT_OPEN_TRACE_FILE       27 /* error when opening a trace file */
#define OID_DLL_NOT_LINKED         26 /* OBJECT IDENTIFIER DLL was not linked */
#define UNIMPLEMENTED              25 /* unimplemented type or feature */
#define CANT_OPEN_TRACE_WINDOW     24 /* error when opening a trace window */
#define UNAVAIL_ENCRULES           23 /* the encoding rules requested are
				       * not implemented yet or were not
				       * linked because the encoder/decoder
				       * function pointers were not
				       * initialized by a call to ossinit() */
#define BAD_ENCRULES               22 /* unknown encoding rules set in the
				       * ossGlobal structure */
#define NULL_FCN                   21 /* attempt was made to call the
				       * encoder/decoder via a NULL pointer */
#define NULL_TBL                   20 /* attempt was made to pass a NULL
				       * control table pointer */
#define ACCESS_SERIALIZATION_ERROR 19 /* error occured during access to
				       * global data in a multi-threaded
				       * environment */
#define CONSTRAINT_VIOLATED        17 /* constraint violation error occured */
#define OUT_MEMORY                  8 /* memory-allocation error */
#define BAD_VERSION                 7 /* versions of encoder/decoder and
				       * control-table do not match */
#define PDU_RANGE                   3 /* pdu specified out of range */
#define MORE_BUF                    1 /* user-provided outbut buffer
				       * too small */

/*** encoder return codes ***/
#define FATAL_ERROR      18  /* *serious* error, could not free memory, &etc */
#define TOO_LONG         16  /* type was longer than shown in SIZE constraint */
#define BAD_TABLE        15  /* table was bad, but not NULL */
#define MEM_ERROR        14  /* memory violation signal trapped */
#define INDEFINITE_NOT_SUPPORTED 13 /* BER indefinite-length encoding is
                              * not supported for Spartan or time-optimized
                              * encoder/decoder */
#define BAD_TIME         12  /* bad value in time type */
#define BAD_PTR          11  /* unexpected NULL pointer in input buffer */
#define BAD_OBJID        10  /* object identifier conflicts with x.208 */
#define BAD_CHOICE        9  /* unknown selector for a choice */
#define BAD_ARG           6  /* something weird was passed--probably a NULL
			      * pointer */
#define PDU_ENCODED       0  /* PDU successfully encoded */

/*** decoder return codes ***/
/* MORE_BUF, BAD_VERSION, OUT_MEMORY, PDU_RANGE and BAD_ARG defined above */
#define EOF_INPUT_FILE   85  /* EOF decoder input file, no PDU was decoded */
#define LIMITED          10  /* implementation limit exceeded. eg:
			      * integer value too great */
#define PDU_MISMATCH      9  /* the PDU tag that the user specified was different
			      * from the tag found in the encoded data */
#define DATA_ERROR        5  /* an error exists in the encoded data */
#define MORE_INPUT        4  /* the PDU is not fully decoded, but the end
			      * of the input buffer has been reached */
#define NEGATIVE_UINTEGER 2  /* the first bit of the encoding is encountered
                              * set to 1 while decoding an unsigned integer */
#define PDU_DECODED       0  /* PDU successfully decoded */

typedef struct ossGlobal *OSSGlobal; /* avoid warning messages by C compiler */

extern int asn1chop;         /* 0 means don't truncate strings; non-zero
			      * value means truncate long input strings
			      * (OCTET STRING, BIT STRING, CharacterStrings)
			      * to be asn1chop bytes long. Used by printPDU. */

extern size_t ossblock;      /* if > 0, size of largest block to allocate */
extern size_t ossprefx;      /* size of reserved OSAK buffer prefix */

#if defined(macintosh) && defined(__CFM68K__)
#pragma import on
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern void *(*mallocp)(size_t p);  /* function which allocates memory */
extern void  (*freep)(void *p);     /* function which frees memory */

#ifdef EOF
extern FILE *asn1out;

/* pointer to output function used by printPDU; default to fprintf. */
extern int (*asn1prnt) (FILE *stream, const char *format, ...);
#endif

#ifndef storing
#ifndef coding
#ifndef OSS_TOED
#include "ossglobl.h"
#endif /* not OSS_TOED */
#endif /* not coding */
#endif /* not storing */

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

typedef struct {
    long           length;
    unsigned char *value;
} OssBuf;

typedef struct {
    long           length;
    unsigned char *value; 
    long           byteOffset;
} OssBufExtended;

/* Runtime support for encoded OBJECT IDENTIFIERs */
typedef struct {
    unsigned short length;
    unsigned char *value;
} OssEncodedOID;

/* Runtime support for encoded RELATIVE-OID */
typedef OssEncodedOID OssEncodedRelOID;

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

PUBLIC extern int DLL_ENTRY encode(struct ossGlobal *world,
		int pdunum, void *inbuf, char **outbuf, long *outlen,
		void *ctl_tbl, unsigned flags, char errmsg[ERR_MSG_LENGTH]);

PUBLIC extern int DLL_ENTRY decode(struct ossGlobal *world,
		int *pdunum, char **inbuf, long *inlen, void **outbuf,
			long *outlen, void *ctl_tbl, unsigned flags,
						char errmsg[ERR_MSG_LENGTH]);

#define PDU_FREED 0

PUBLIC int  DLL_ENTRY freePDU(struct ossGlobal *world, int pdunum, void *data, void *ctl_tbl);
PUBLIC void DLL_ENTRY freeBUF(struct ossGlobal *world, void *data);

#define PDU_PRINTED 0

PUBLIC int DLL_ENTRY printPDU(struct ossGlobal *world, int pdunum, void *data, void *ctl_tbl);


#define VALUE_COPIED 0

PUBLIC extern int DLL_ENTRY ossCpyValue (struct ossGlobal *world,
				int pdunum, void *source, void **destination);

#define VALUES_EQUAL      0  /* The values are the same */
#define VALUES_NOT_EQUAL  1  /* The values are not the same */

PUBLIC extern int DLL_ENTRY ossCmpValue (struct ossGlobal *world,
			int pdunum, void *originalData, void *copiedData);


#define INITIALIZATION_SUCCESSFUL 0

PUBLIC int  DLL_ENTRY ossinit(struct ossGlobal *world,
							void *ctl_tbl);
PUBLIC int  DLL_ENTRY ossDupWorld(struct ossGlobal *org_world, 
				  struct ossGlobal *dup_world);
PUBLIC void DLL_ENTRY ossterm(struct ossGlobal *world);
extern int            ossPrint(struct ossGlobal *, const char *, ...);

PUBLIC int  DLL_ENTRY ossEncode(struct ossGlobal *world,
				int              pdunum,
				void            *input,
				OssBuf          *output);

PUBLIC int  DLL_ENTRY ossDecode(struct ossGlobal *world,
				int             *pdunum,
				OssBuf          *input,
				void           **output);

PUBLIC int  DLL_ENTRY ossPrintPDU(struct ossGlobal *world,
				int                pdunum,
				void              *data);

PUBLIC int  DLL_ENTRY ossFreePDU(struct ossGlobal *world,
				int               pdunum,
				void             *data);

PUBLIC void DLL_ENTRY ossFreeBuf(struct ossGlobal *world,
				void              *data);

extern int  DLL_ENTRY ossTest(struct ossGlobal *world,
				int            pdunum,
				void          *data);

extern int DLL_ENTRY ossTestEncoding(struct ossGlobal *world,
				int            pdunum,
				OssBuf        *encodedData);

extern int DLL_ENTRY ossTestConcatenatedEncodings(struct ossGlobal *world,
				int            pdunum,
				OssBuf        *encoded_message);

extern int DLL_ENTRY ossTestMultipleEncodings(struct ossGlobal *world,
				int            pdunum,
				OssBuf        *encoded_messages,
				int            number_of_encoded_messages);

extern int DLL_ENTRY ossEcnTestMultipleEncodings(struct ossGlobal *world,
				int            pdunum,
				OssBuf        *encoded_messages,
				int            number_of_encoded_messages,
				unsigned char *padding_bits);

PUBLIC void DLL_ENTRY ossPrintHex(struct ossGlobal *world,
				char               *encodedData,
				long                length);

PUBLIC int  DLL_ENTRY ossCheckConstraints(struct ossGlobal *world,
				int                        pdunum,
				void                      *data);

PUBLIC long DLL_ENTRY ossDetermineEncodingLength(struct ossGlobal *world,
				int                                pdunum,
				void                              *data);

PUBLIC long DLL_ENTRY ossGetBytesReadByDecoder(struct ossGlobal *world);

PUBLIC unsigned long DLL_ENTRY
		ossDetermineMessageLength(struct ossGlobal *world,
				OssBuf                     *buf);

PUBLIC int  DLL_ENTRY ossOpenTraceFile(struct ossGlobal *world,
				char                    *fileName);

PUBLIC int  DLL_ENTRY ossCloseTraceFile(struct ossGlobal *world);

PUBLIC int DLL_ENTRY ossEncodedOidToAsnVal(struct ossGlobal *world,
			const OssEncodedOID *encodedOID, OssBuf *valOID);
PUBLIC int DLL_ENTRY ossEncodedOidToDotVal(struct ossGlobal *world,
			const OssEncodedOID *encodedOID, OssBuf *dotOID);
PUBLIC int DLL_ENTRY ossAsnValToEncodedOid(struct ossGlobal *world,
			const char *valOID, OssEncodedOID *encodedOID);
PUBLIC int DLL_ENTRY ossDotValToEncodedOid(struct ossGlobal *world,
			const char *dotOID, OssEncodedOID *encodedOID);

PUBLIC int DLL_ENTRY ossEncodedRelOidToAsnVal(struct ossGlobal *world,
			const OssEncodedRelOID *encodedROID, OssBuf *valROID);
PUBLIC int DLL_ENTRY ossEncodedRelOidToDotVal(struct ossGlobal *world,
			const OssEncodedRelOID *encodedROID, OssBuf *dotROID);
PUBLIC int DLL_ENTRY ossAsnValToEncodedRelOid(struct ossGlobal *world,
			const char *valROID, OssEncodedRelOID *encodedROID);
PUBLIC int DLL_ENTRY ossDotValToEncodedRelOid(struct ossGlobal *world,
			const char *dotROID, OssEncodedRelOID *encodedROID);

PUBLIC int DLL_ENTRY ossGeneralizedTimeToInts(struct ossGlobal *world,
		char *timeString, unsigned int *year, unsigned int *month,
		unsigned int *day, unsigned int *hour, unsigned int *minute,
		unsigned int *second, unsigned int *fraction, 
		unsigned short *precision, int *local_uts_mindiff);
PUBLIC int DLL_ENTRY ossUTCTimeToInts(struct ossGlobal *world,
		char *timeString, unsigned int *year, unsigned int *month,
		unsigned int *day, unsigned int *hour, unsigned int *minute,
		unsigned int *second, int *uts_mindiff);
PUBLIC int DLL_ENTRY ossIntsToGeneralizedTime(struct ossGlobal *world,
		unsigned int year, unsigned int month, unsigned int day,
		unsigned int hour, unsigned int minute,	unsigned int second,
		unsigned int fraction, unsigned short precision,
		int local_utc_mindiff, char *timeString); 
PUBLIC int DLL_ENTRY ossIntsToUTCTime(struct ossGlobal *world,
		unsigned int year, unsigned int month, unsigned int day,
		unsigned int hour, unsigned int minute,	unsigned int second,
		int local_utc_mindiff, char *timeString); 
PUBLIC int DLL_ENTRY ossAddInfoObject(struct ossGlobal *world,
					int objSetIndex, void *object);
PUBLIC int DLL_ENTRY ossRemoveInfoObject(struct ossGlobal *world,
					int objSetIndex, void *object);
PUBLIC void *DLL_ENTRY ossGetInfoObject(struct ossGlobal *world,
					int objSetIndex, void *uniquValue);
PUBLIC void *DLL_ENTRY ossGetInfoObjectSet(struct ossGlobal *,
							int objSetIndex);

PUBLIC void DLL_ENTRY ossPrintOctetAsASCII(struct ossGlobal *world, char* display);
PUBLIC void DLL_ENTRY ossPrintOctetAsIPAddress(struct ossGlobal *world, char* display);
PUBLIC int DLL_ENTRY ossGeneralizedTimeToShorts(struct ossGlobal *,
	char *timeString, unsigned short *year, unsigned short *month,
	unsigned short *day, unsigned short *hour, unsigned short *minute,
	unsigned short *second, unsigned int *fraction, unsigned short *precision,
	int *local_utc_mindiff);
PUBLIC int DLL_ENTRY ossUTCTimeToShorts(struct ossGlobal *world,
	char *timeString, unsigned short *year, unsigned short *month,
	unsigned short *day, unsigned short *hour, unsigned short *minute,
	unsigned short *second, int *utc_mindiff);

#define OSS_PLUS_INFINITY  "PLUS_INFINITY"
#define OSS_MINUS_INFINITY "MINUS_INFINITY"
#define DISP_PLUS_INFINITY "PLUS-INFINITY"
#define DISP_MINUS_INFINITY "MINUS-INFINITY"
#define ossNaN             "NOT_A_NUMBER"


typedef enum {
    OSS_DEFAULT_MEMMGR = 0,	/* memory is malloc'ed for each pointer in
				 * data tree */
    OSS_FILE_MEMMGR,		/* file memory manager with memory malloc'ed
				 * for each pointer in data tree */
    OSS_SOCKET_MEMMGR,		/* TCP/IP socket and file memory manager with memory
				 * malloc'ed for each pointer in data tree */
    OSS_FLAT_MEMMGR,		/* memory is malloc'ed in large blocks */
    OSS_OSAK_MEMMGR,		/* OSAK-buffer memory manager */
    OSS_USER_MEMMGR,		/* user memory manager */
    OSS_CPP_MEMMGR		/* fsream, TCP/IP socket and file memory
				 * manager with memory malloc'ed for each
				 * pointer in data tree */
} OssMemMgrType;

typedef enum {
    OSS_UNKNOWN_OBJECT = 0,
    OSS_FILE,
    OSS_SOCKET,
    OSS_OSAK_BUFFER,
    OSS_FSTREAM,
    OSS_MEMORY
} OssObjType;

PUBLIC OssObjType DLL_ENTRY ossTestObj(struct ossGlobal *world, void *objHndl);
PUBLIC void *DLL_ENTRY ossGetObj(struct ossGlobal *world, void *objHndl);
PUBLIC void *DLL_ENTRY ossUnmarkObj(struct ossGlobal *world, void *objHndl);
PUBLIC void *DLL_ENTRY ossMarkObj(struct ossGlobal *world, OssObjType objType,
							void *object);
PUBLIC void  DLL_ENTRY ossFreeObjectStack(struct ossGlobal *world);
PUBLIC void  DLL_ENTRY ossSetTimeout(struct ossGlobal *world, long timeout);
PUBLIC int   DLL_ENTRY ossGetBuffer(struct ossGlobal *world, OssBufExtended *buf);
PUBLIC int   DLL_ENTRY ossSetBuffer(struct ossGlobal *world, OssBufExtended *buf);
PUBLIC int   DLL_ENTRY ossSetTemporaryBuffer(struct ossGlobal *world, OssBuf *buf);
PUBLIC int   DLL_ENTRY ossSetFormatterBufferSize(struct ossGlobal *world, unsigned long size);
PUBLIC unsigned long DLL_ENTRY ossGetFormatterBufferSize(struct ossGlobal *world);
PUBLIC int   DLL_ENTRY ossSetUserStack(struct ossGlobal *world, OssBuf *buf);
PUBLIC int   DLL_ENTRY ossOpenDecoderInputFile(struct ossGlobal *world, char *fname);
PUBLIC int   DLL_ENTRY ossCloseDecoderInputFile(struct ossGlobal *world);
PUBLIC int   DLL_ENTRY ossOpenEncoderOutputFile(struct ossGlobal *world, char *fname);
PUBLIC int   DLL_ENTRY ossCloseEncoderOutputFile(struct ossGlobal *world);
PUBLIC long  DLL_ENTRY ossSetMinFileSize(struct ossGlobal *world, long minsize);
PUBLIC long  DLL_ENTRY ossGetMinFileSize(struct ossGlobal *world);
PUBLIC void  DLL_ENTRY ossSetUserMallocFreeRealloc(struct ossGlobal *world,
		void *(*ossUserMalloc)(struct ossGlobal *world, size_t size),
		void  (*ossUserFree)(struct ossGlobal *world, void *buf),
		void *(*ossUserRealloc)(struct ossGlobal *world, void *buf, size_t size));
PUBLIC void  DLL_ENTRY ossGetUserMallocFreeRealloc(struct ossGlobal *world,
		void *(**ossUserMalloc)(struct ossGlobal *world, size_t size),
		void  (**ossUserFree)(struct ossGlobal *world, void *buf),
		void *(**ossUserRealloc)(struct ossGlobal *world, void *buf, size_t size));
PUBLIC int   DLL_ENTRY ossInitSync(void);
PUBLIC void  DLL_ENTRY ossTermSync(void);
PUBLIC void  DLL_ENTRY ossSetUserPrint(struct ossGlobal *world, FILE *stream,
		int (*ossUserPrint)(struct ossGlobal *world, const char *format, ...));
PUBLIC void  DLL_ENTRY ossGetUserPrint(struct ossGlobal *world, FILE **stream,
		int (**ossUserPrint)(struct ossGlobal *world, const char *format, ...));
PUBLIC void  DLL_ENTRY ossSetUserVprint(struct ossGlobal *world, FILE *stream,
		int (*ossUserVprint)(struct ossGlobal *world,
		const char *fmt, va_list ap));
PUBLIC void  DLL_ENTRY ossGetUserVprint(struct ossGlobal *world, FILE **stream,
		int (**ossUserVprint)(struct ossGlobal *world,
		const char *fmt, va_list ap));
PUBLIC void  DLL_ENTRY ossSetUserFieldCpyCmp(struct ossGlobal *world,
	int (*ossUserFieldCpy)(struct ossGlobal *world, OpenType *src, OpenType *dest),
	int (*ossUserFieldCmp)(struct ossGlobal *world, OpenType *src, OpenType *dest));
PUBLIC void  DLL_ENTRY ossGetUserFieldCpyCmp(struct ossGlobal *world,
	int (**ossUserFieldCpy)(struct ossGlobal *world, OpenType *src, OpenType *dest),
	int (**ossUserFieldCmp)(struct ossGlobal *world, OpenType *src, OpenType *dest));
PUBLIC int   DLL_ENTRY_FDEF ossGetPrintIndent(struct ossGlobal *world);
PUBLIC void  DLL_ENTRY_FDEF ossSetPrintIndent(struct ossGlobal *world, int indent_level);

extern void *DLL_ENTRY ossGetHeader(void);

#if defined(__IBMC__) && (defined(__WIN32__) || defined(__OS2__))
extern void           *getStartAddress(struct ossGlobal *, char *);
extern void DLL_ENTRY  ossWterm(struct ossGlobal *);
extern int  DLL_ENTRY  ossReadLine(struct ossGlobal *, HWND, FILE *, char *, MEMBLOCK *, LONG);
extern void DLL_ENTRY  ossFreeList(struct ossGlobal *);
extern void DLL_ENTRY  ossSaveTraceInfo(struct ossGlobal *, HWND, char *);
extern int  DLL_ENTRY  oss_test(struct ossGlobal *);
int                    ossGeneric(struct ossGlobal *, HWND);

extern int  DLL_ENTRY  ossPrintWin(struct ossGlobal *, const char *,
			int, int, int, int, int, int, int, int, int, int);
PUBLIC int  DLL_ENTRY  ossVprintWin(struct ossGlobal *, const char *, va_list);
extern HINSTANCE DLL_ENTRY ossLoadMemoryManager(struct ossGlobal *,
						OssMemMgrType, char *);
extern HINSTANCE DLL_ENTRY ossLoadDll(struct ossGlobal *, char *);
extern int  DLL_ENTRY  ossWinit(struct ossGlobal *, void *, char *, HWND);
extern int  DLL_ENTRY  ossOpenTraceWindow(struct ossGlobal *);
#else 
#if !defined(DYNAMICLINKING_NOTSUPPORTED)
int                     ossUinit(struct ossGlobal *world, void *ctl_tbl, char *ctl_dll);
void                   *ossUOpenDynlib(const char * ctl_dll_name);
int                     ossUCloseDynlib(struct ossGlobal *world);
void                    ossUterm(struct ossGlobal *world);
#endif
#endif /* __IBMC__ && (__WIN32__ || __OS2__) */

#ifdef __hpux
/* There is a real signal "SIGBUS", even if ANSI-C is compiled */
#define SIGBUS _SIGBUS
#endif /* __hpux */

#ifdef __cplusplus
}
#endif

#if defined(macintosh) && defined(__CFM68K__)
#pragma import reset
#endif

#endif /* ASN1CODE_H */
