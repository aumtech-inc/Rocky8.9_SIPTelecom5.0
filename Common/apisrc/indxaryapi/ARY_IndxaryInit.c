#ident	"@(#)ARY_IndxaryInit 96/06/26 2.2.0 Copyright 1996 Aumtech, Inc."
static char ModuleName[]="ARY_IndxaryInit";
/*-----------------------------------------------------------------------------
Program:	ARY_IndxaryInit.c
Purpose:	Initialize the index array file into memory.
Author:		M. Bhide
Date:		02/23/94
Update:		12/05/95 G. Wenzel updated for ISP 2.2
Update:		03/04/96 G. Wenzel cleaned up the routine, added errno.
Update:		06/26/96 G. Wenzel removed error checking on send_to_monitor
Update: 	06/26/96 G. Wenzel added errno to COM_UTLFILE_OPEN msg
Update:		06/26/96 G. Wenzel added key_no, filename to COM_EKEYFIELD msg.
Update:		06/26/96 G. Wenzel added use of MAX_ARY_FILE_SIZE
Update:		06/26/96 G. Wenzel added MAX_ARY_FILE_SIZE to COM_EFILESIZLIM
Update:		06/26/96 G. Wenzel added filename to COM_EEMPTYBTREE
Update:		06/26/96 G. Wenzel added filename to COM_EDUPKEYIGNORE msg
Update:		06/09/98 ABK. added the checks for the COMMENT lines.
-------------------------------------------------------------------------------
 * Copyright (c) 1996, Aumtech, Inc.
 * All Rights Reserved.
 *
 * This is an unpublished work of Aumtech which is protected under 
 * the copyright laws of the United States and a trade secret
 * of Aumtech. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include "COM_Mnemonics.h"
#include "IndexArray.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "monitor.h"

extern errno;

/*---------------------------------------------------------------------
The file specified will be sorted by the key number and loaded into memory.
Maximum of MAX_FILES files per application of 10,000 bytes each may be loaded.
---------------------------------------------------------------------*/
int ARY_IndxaryInit(filename, key)
const	char	*filename;
int	key;				/* column number */
{
int 	i;
int	ret_code = 0;
struct	stat 	stat_buf;
char	diag_mesg[256];

sprintf(diag_mesg,"ARY_IndxaryInit(%s, %d)", filename, key);
ret_code = send_to_monitor(MON_API, ARY_INDXARYINIT, diag_mesg);

/* Check to see if the appropriate Init API has been called. */
if(GV_InitCalled != 1)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOTINIT, "Service");
    return (-1);
}

if( key <= 0)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_PARMNUM,"key",key);
    return (-1);
}

if(stat(filename, &stat_buf) < 0)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ESTATFAILED, filename, errno);
    return (-1);
}

/* check file size, if file size > MAX_ARY_FILE_SIZE bytes do not index it */
if(stat_buf.st_size > MAX_ARY_FILE_SIZE)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EFILSIZLIM,
	filename,MAX_ARY_FILE_SIZE);
    return (-1);
}

for (i=0; i< MAX_FILE; i++)
{
    if(__idx_tabl[i].idx_ptr == NULL)
    {
	/* copy entries into table from the file */
	strcpy(__idx_tabl[i].file, filename);

	/* build the index in memory & save ptr */
	__idx_tabl[i].idx_ptr = build_tree(filename, key);
	if(__idx_tabl[i].idx_ptr == NULL)
	{
	    /* Empty tree created for given file in memory */
	    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EEMPTYBTREE, filename);
	    return (-1);
	}
	__idx_tabl[i].key_no = key;	/* save key in table */
	break;
    }
} /* end for */

return (0);
} /*  ARY_IndxaryInit */

/*---------------------------------------------------------------------
This routine creates the tree of records in memory sorted by the key. 
----------------------------------------------------------------------*/
__NODEPTR *build_tree(filename, key_no)
char	*filename;
int 	key_no;
{
__NODEPTR *ptree;
__NODEPTR *p, *q;
struct __record rec;
char buf[MAX_DATA];
FILE *fp;		/* file containing record  */

ptree = p = q = NULL;

/* Open file. */
if((fp = fopen(filename,"r")) == NULL)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_UTLFILE_OPEN,filename,errno);
    return (NULL);
}
#ifdef DEBUG
    fprintf(stderr, "Open file for read %s\n", filename);
#endif

while(1)
{
	/* Get first line. */
	if ((fgets(buf, MAX_DATA-1, fp)) == NULL || (feof(fp)))
	{
    		LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EFILEMPTY,filename);
    		fclose(fp);
    		return (NULL);
	}
#ifdef DEBUG
    fprintf(stderr, "Read first record from file\n%s", buf);
#endif

	/* Remove new line character. */
	buf[strlen(buf) -1] = '\0';
	if(buf[0]=='#')
		continue;
	else if (strlen(buf) == 0)
		continue;
	else
		break;
}


if (UTL_GetField(buf, key_no, rec.key) < 0)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EKEYFIELD, key_no, filename);
    fclose(fp);
    return (NULL);
}
#ifdef DEBUG
    printf("The field is : |%s|\n", buf);
#endif

/* Initialize buffer. */
rec.rec[0] ='\0';	

/* Load buffer with first line. */
strcpy(rec.rec, buf);

/* Create a tree node in memory. */
ptree = maketree(rec);

/* Load whole file into memory. */
while (!feof(fp))
{
    buf[0] = '\0';
    /* read complete line into the buffer */
    if (fgets(buf, MAX_DATA-1, fp) == NULL )
    {
	fclose(fp);
	return(ptree);		/* read all file */
    }
    /* Check for end of file. */
    if(feof(fp))
    {
	fclose(fp);
	return(ptree);		/* read all file */
    }
#ifdef DEBUG
    fprintf(stderr, "Read next record from file\n%s\n", buf);
#endif
    /* Remove new line character. */
    buf[strlen(buf) -1] = '\0';
    if(buf[0]=='#')
	continue;
    if (strlen(buf) == 0)
	continue;

    /* Get key word. */
    if (UTL_GetField(buf, key_no, rec.key) < 0)
    {
	LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EKEYFIELD, key_no, filename);
	fclose(fp);
	return (NULL);		/* return NULL value */
    }
#ifdef DEBUG
    printf("THE field is : |%s|\n", buf);
#endif
    rec.rec[0] ='\0';		/* initialize buffer */
    strcpy(rec.rec, buf);	/* copy whole record line */
    p = q = ptree;

    /* Sort. */
    while (( strcmp(rec.key, p->data.key) != 0) && (q != NULL) ) 
    {
	p = q;
	if (strcmp(rec.key, p->data.key) < 0)
	    q = p -> left;
	else 
	    q = p -> right;
    }
    if (strcmp(rec.key, p->data.key) == 0)
    {
	LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EDUPKEYIGNORE,rec.key, filename);
    }
    if (strcmp(rec.key, p->data.key) < 0)
    {
	if(setleft(p, rec) < 0)
	{
	    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EINSERTNODE,rec.key);
	    fclose(fp);
	    return (NULL);
	}
#ifdef DEBUG
    fprintf(stderr, "set this record to left child\n");
#endif
    }
    else 
    {
	if (setright(p ,rec) < 0)
	{
	    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EINSERTNODE,rec.key);
	    fclose(fp);
	    return (NULL);
	}
#ifdef DEBUG
    fprintf(stderr, "set this record to right child\n");
#endif
    }

} /* end while */
fclose(fp);

return (NULL);
} /* build_tree */

/*-----------------------------------------------------------------------------
This routine creates the tree node
-----------------------------------------------------------------------------*/
__NODEPTR *maketree(rec)
struct __record rec;
{
    __NODEPTR *p;

    /* Create empty node. */
    p = getnode();
    if ( p == NULL )
    {
	LOGXXPRT(ModuleName,REPORT_NORMAL,COM_UTLFILE_MALLOC);
	return (NULL);
    }
    strcpy(p->data.key, rec.key);
    strcpy(p->data.rec, rec.rec);
    p->left = NULL;
    p->right = NULL;
    return (p);
} /* of maketree */
/*------------------------------------------------------------------------------
This routine set the node at the left child of the given node 
------------------------------------------------------------------------------*/
int setleft(p, rec)
__NODEPTR *p;
struct __record rec;
{
    if (p == NULL)
	return (-1);
    else if (p->left != NULL)
	return (-1);
    else
    {
	if((p->left = maketree(rec)) == NULL)
		return (-1);
    }
return (0);
} /* of setleft */
/*------------------------------------------------------------------------------
This routine set the node at the right child of the given node 
------------------------------------------------------------------------------*/
int setright(p, rec)
__NODEPTR *p;
struct __record rec;
{
    if (p == NULL)
	return (-1);
    else if (p->right != NULL)
	return (-1);
    else
    {
	if((p->right = maketree(rec)) == NULL)
	    return (-1);
    }
return (0);
} /* of setright */
/*------------------------------------------------------------------------------
This routine creates empty node and return the node  
------------------------------------------------------------------------------*/
__NODEPTR *getnode()
{
__NODEPTR *p;

p = (__NODEPTR *) malloc (sizeof(struct __tnode));

return (p);
} /* of getnode */
