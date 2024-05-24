/*****************************************************************************/
/* Copyright (C) 1989-2001 OSS Nokalva.  All rights reserved.                */
/*****************************************************************************/

/* THIS FILE IS PROPRIETARY MATERIAL OF OPEN SYSTEMS SOLUTIONS, INC.
 * AND MAY BE USED ONLY BY DIRECT LICENSEES OF OPEN SYSTEMS SOLUTIONS, INC.
 * THIS FILE MAY NOT BE DISTRIBUTED. */

/**************************************************************************/
/* FILE: @(#)asn1util.h	9.1  00/10/30			  */
/*							 		  */
/* function: Support routines definitions for the optimized		  */
/* encoder/decoder generated by the OSS ASN.1 Compiler			  */
/*									  */
/*									  */
/* changes:								  */
/*	11/16/90  pet	created						  */
/*									  */
/**************************************************************************/

#include "ossdll.h"

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

/* The _MEM_ARRAY_SIZE size should be such that the size of the encDecVar
 * field be equal or greater than that of the world->c structure */

#ifdef __hp9000s300
#define _MEM_ARRAY_SIZE	34
#endif

#ifdef __alpha
#ifdef __osf__
#define _MEM_ARRAY_SIZE	43
#endif	/* __osf__ */
#endif	/* __alpha */

#ifdef _AIX
#define _MEM_ARRAY_SIZE	52
#endif
#ifdef __hp9000s700
#define _MEM_ARRAY_SIZE	60
#endif

#ifdef __NeXT__
#define _MEM_ARRAY_SIZE	66
#endif

#ifdef VAXC
#define _MEM_ARRAY_SIZE	78
#endif


#ifdef __mips
#	define _MEM_ARRAY_SIZE       70
#endif	/* __mips */

#ifdef _FTX	/* Stratus's Fault-Tolerant Unix */
#define _MEM_ARRAY_SIZE  84
#endif

#ifdef __HIGHC__
#define _MEM_ARRAY_SIZE       52
#endif	/* __HIGHC__ */

#ifdef __arm
#define _MEM_ARRAY_SIZE       78
#endif	/* __arm */

#if defined(_WIN32) || defined(_WINDOWS) ||\
    defined(__OS2__) || defined(NETWARE_DLL)
#define _MEM_ARRAY_SIZE 52
#endif	/* _WIN32 || _WINDOWS || __OS2__ || NETWARE_DLL */

#ifdef _MCCPPC
#define _MEM_ARRAY_SIZE       72
#endif	/* _MCCPPC */

#ifndef _MEM_ARRAY_SIZE
#define _MEM_ARRAY_SIZE 60
#endif

typedef struct _mem_array_ {
    short           _used;	                /* Next available entry */
    void           *_entry[_MEM_ARRAY_SIZE];	/* Pointers to allocated
						 * memory */
    struct _mem_array_ *_next;	/* Pointer to additional mem_array */
} _mem_array;


#ifndef OSSDEBUG
#define OSSDEBUG 0
#endif /* OSSDEBUG */

typedef struct _encoding_ {
	       long   length;           /* length of the encoding */
	       char   *value;           /* pointer to encoding octets */
} _encoding;

typedef struct _enc_block_ {
    struct _enc_block_ *next;        /* nested setofs form a list of these */
    long   size;                     /* size of the encodings array */
    long   used;                     /* number of items used in the array */
    _encoding       *enc;            /* pointer to array of encodings */
    _mem_array      mem;             /* previous encoding saved*/
    _mem_array      *mem_tail;
    char            *pos;
    long            max_len;
    ossBoolean           buffer_provided;
    long            _encoding_length;
} _enc_block;

struct _enum_data {
	int         num;     /* number of enumerations */
	OSS_INT32   *enums;  /* pointer to sorted array of enumerations */
};

struct _char_data {
	int      num;  /* number of characters in PermittedAlphabet */
	void     *pa;  /* pointer to PermittedAlphabet char string */
	void     *ia;  /* pointer to inverted indices string */
};


#ifndef _OSSNOANSI

#if defined(macintosh) && defined(__CFM68K__)
#pragma import on
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern void *DLL_ENTRY _oss_enc_getmem(struct ossGlobal *g,ossBoolean _userbuf);
extern void *DLL_ENTRY _oss_dec_getmem(struct ossGlobal *g,long _size,ossBoolean _userbuf);
extern void *DLL_ENTRY _oss_dec_getmem_internal(struct ossGlobal *g, long size, ossBoolean userbuf);
extern void *DLL_ENTRY _oss_dec_gettempmem(struct ossGlobal *g,long _size);
extern void  DLL_ENTRY _oss_dec_freetempmem(struct ossGlobal *g, void *p);
extern void  DLL_ENTRY _oss_enc_push(struct ossGlobal *g,void *_p);
extern void *DLL_ENTRY _oss_enc_pop(struct ossGlobal *g);
extern void  DLL_ENTRY _oss_freeMem(struct ossGlobal *g,_mem_array *_p);
extern void  DLL_ENTRY _oss_releaseEntry(struct ossGlobal *g, void *entry,
					ossBoolean tmp);
extern void  DLL_ENTRY _oss_releaseMem(struct ossGlobal *g,_mem_array *_p);
#define _oss_freeTempMem _oss_releaseMem
extern void DLL_ENTRY  _oss_set_outmem_d(struct ossGlobal *g, long _final_max_len,
					long *_totalsize, char **_outbuf);
extern void DLL_ENTRY  _oss_set_outmem_i(struct ossGlobal *g,long _final_max_len,
					long *_totalsize,char **_outbuf);
extern void DLL_ENTRY  _oss_set_outmem_p(struct ossGlobal *g,
					long *_totalsize,char **_outbuf);
extern void DLL_ENTRY  _oss_set_outmem_pb(struct ossGlobal *g,
			long *_totalsize, char **_outbuf, unsigned flags);
extern void     _oss_hdl_signal(int _signal);
extern void DLL_ENTRY _oss_free_creal(struct ossGlobal *g, char *p);
extern int  DLL_ENTRY ossMinit(struct ossGlobal *g);
extern void DLL_ENTRY _oss_beginBlock(struct ossGlobal *g, long count,
    char **pos, long *max_len);
extern void DLL_ENTRY _oss_nextItem(struct ossGlobal *g, long *max_len);
extern void DLL_ENTRY _oss_endBlock(struct ossGlobal *g, char **pos, long *max_len,
    unsigned char ct);
extern void DLL_ENTRY _oss_freeDerBlocks(struct ossGlobal *g);
extern void DLL_ENTRY _oss_freeGlobals(struct ossGlobal *g, ossBoolean decoding);
#ifdef __cplusplus
}
#endif

#if defined(macintosh) && defined(__CFM68K__)
#pragma import reset
#endif

#else
extern void    *_oss_enc_getmem();
extern void    *_oss_dec_getmem();
extern void     _oss_enc_push();
extern void    *_oss_enc_pop();
extern void     _oss_freeMem();
extern void     _oss_releaseMem();
#define _oss_freeTempMem _oss_releaseMem
extern void     _oss_set_outmem_d();
extern void     _oss_set_outmem_i();
extern void     _oss_set_outmem_p();
extern void     _oss_set_outmem_pb();
extern void     _oss_free_creal();

extern void     _oss_hdl_signal();	/* signal handler */

extern void _oss_beginBlock();
extern void _oss_nextItem();
extern void _oss_endBlock();
extern void _oss_freeDerBlocks();
extern void _oss_freeGlobals();

#endif /* _OSSNOANSI */

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

