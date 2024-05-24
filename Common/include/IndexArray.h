#ident    "@(#)IndexArray.h 94/8/09 Release 2.2 Copyright 1996 Aumtech, Inc."
/*-----------------------------------------------------------------------------
Program:	IndexArray.h
Purpose:	To define constants used with the index array routines.
Author:		Unknown
Date:		1994
Update:		06/26/96 G. Wenzel added MAX_ARY_FILE_SIZE
-------------------------------------------------------------------------------
Copyright (c) 1996, Aumtech, Inc.
All Rights Reserved.

THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Aumtech
which is protected under the copyright laws of the 
United States and a trade secret of Aumtech. It may 
not be used, copied, disclosed or transferred other 
than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/

#include	<sys/stat.h>  

#define	IN		0		/* following defines are used */
#define	OUT		1		/* by an index array */
#define	YES		1
#define	NO 		0
#define	MAX_FILE	10
#define	KEY_SIZE	80
#define	MAX_DATA	256
#define MAX_ARY_FILE_SIZE 10000		/* Maximum size of the file */

/*  data structure used by index array */
struct __record	{
	char key[KEY_SIZE];
	char rec[MAX_DATA];	/* actual  record */
	};			/* record structure in file */

struct  __tnode	{
	struct __record	data;		/* data structure of record */
	struct __tnode *left;		/* pointer to left child */
	struct __tnode *right;		/* pointer to right child */
	}; 

/* define btree data structure */

typedef struct __tnode __NODEPTR; 
				/* data structure defined for binary tree */

typedef struct  		/* store index table i.e. filename & root */
	{
	char file[80];			/* file name */
	int  key_no;			/* key field in file - field no */
	__NODEPTR *idx_ptr;		/* pointer to file index,root of tree */
	} __file_idx;	

__file_idx __idx_tabl[MAX_FILE]; 	/* maximum file 10 limit */

__NODEPTR	*maketree();
__NODEPTR 	*getnode();
__NODEPTR 	*build_tree();
int 		setleft();
int 		setright();
