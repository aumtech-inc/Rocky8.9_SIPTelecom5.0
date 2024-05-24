/*****************************************************************************/
/* Copyright (C) 1989-2001 Open Systems Solutions, Inc.  All rights reserved.*/
/*****************************************************************************/

/* THIS FILE IS PROPRIETARY MATERIAL OF OPEN SYSTEMS SOLUTIONS, INC.
 * AND MAY BE USED ONLY BY DIRECT LICENSEES OF OPEN SYSTEMS SOLUTIONS, INC.
 * THIS FILE MAY NOT BE DISTRIBUTED. */

/*************************************************************************/
/* FILE: @(#)etype.h	9.6  01/03/05			 */
#ifndef ETYPE_H
#define ETYPE_H
#include <stddef.h>		/* has size_t */
#include "ossdll.h"
#define OSS_SPARTAN_AWARE 10
#ifndef NULL
#define NULL ((void*)0)
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
typedef struct ossGlobal *_oss_qjJ;
typedef unsigned short Etag;
typedef struct efield *_oss_JQ;
typedef struct etype *_oss_QQ;
typedef struct _oss_Jw *_oss_wjJ;
typedef struct eheader *Eheader;
struct etype {
	long	max_length;
	size_t	_oss_Q;
	size_t	_oss_WJ;
	char	*_oss_H;
	size_t	_oss_JJ;
	size_t	_oss_Hj;
	unsigned short int _oss_jw;
	unsigned short int _oss_qH;
	unsigned short int _oss_JwJ;
	unsigned short int _oss_wwJ;
	int	_oss_Qj;
	unsigned short int _oss_j;
};
struct efield {
	size_t	_oss_W;
	unsigned short int etype;
	short int	_oss_qwJ;
	unsigned short int _oss_ww;
	unsigned char	_oss_Wj;
};
struct ConstraintEntry {
	char	_oss_HwJ;
	char	_oss_QJ;
	void	*_oss_q;
};
struct InnerSubtypeEntry {
	char	_oss_jH;
	unsigned char	_oss_w;
	unsigned short	efield;
	unsigned short	_oss_q;
};
struct eValRef {
	char	*_oss_H;
	void	*_oss_WjJ;
	unsigned short	etype;
};
typedef struct OSetTableEntry {
	void *_oss_HjJ;
	unsigned short _oss_QjJ;
	void *_oss_jwJ;
	unsigned short _oss_WwJ;
} OSetTableEntry;
typedef struct WideAlphabet {
	OSS_UINT32	_oss_W;
	struct {
	short _oss_qw;
	char	*_oss_jQ;
	} _oss_JjJ;
	void *	_oss_wH;
} WideAlphabet;
struct eheader {
#if defined(__WATCOMC__) && defined(__DOS__)
	void (*_oss_J)(struct ossGlobal *);
#else
	void (DLL_ENTRY_FPTR *_System _oss_J)(struct ossGlobal *);
#endif /* __WATCOMC__ && __DOS__ */
	long	pdudecoder;
	unsigned short int _oss_WQ;
	unsigned short int _oss_w;
	unsigned short int _oss_Qw,
	_oss_qQ;
	unsigned short *pduarray;
	_oss_QQ	etypes;
	_oss_JQ	_oss_qj;
	void	**_oss_jj;
	unsigned short *_oss_Q;
	struct ConstraintEntry *_oss_j;
	struct InnerSubtypeEntry *_oss_Jj;
	void	*_oss_HQ;
	unsigned short int _oss_wQ;
	void	*_oss_Ww;
	unsigned short	_oss_Hw;
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
#endif /* ETYPE_H */
