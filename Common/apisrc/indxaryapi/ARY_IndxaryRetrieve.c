#ident	"@(#)ARY_IndxaryRetrieve 96/06/26 2.2.0 Copyright 1996 Aumtech, Inc."
static char ModuleName[]="ARY_IndxaryRetrieve";
/*-----------------------------------------------------------------------------
Program:	ARY_IndxaryRetrieve.c
Purpose:	Retrieve the field from the array.
Author:		M. Bhide
Date:		02/23/94
Update:		12/05/95 G. Wenzel updated for ISP 2.2
Update:		06/26/96 G. Wenzel removed error check on send_to_monitor
Update:		06/26/96 G. Wenzel added key_no, filename to COM_EKEYFIELD msg 
Update:		06/26/96 G. Wenzel added filename to COM_EEMTYBTREE msg
Update:		06/26/96 G. Wenzel added filename,errno to COM_EOPENDATFIL msg
Update:		06/26/96 G. Wenzel added send_to_monitor(MON_DATA, 0, data);	
Update:		06/09/98 ABK. added the checks for the COMMENT lines.
--------------------------------------------------------------------------------
 * Copyright (c) 1996, Aumtech, Inc.
 * All Rights Reserved.
 *
 * This is an unpublished work of Aumtech which is protected under 
 * the copyright laws of the United States and a trade secret
 * of Aumtech. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of Aumtech.
 *
------------------------------------------------------------------------------*/
#include "COM_Mnemonics.h"
#include "IndexArray.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "monitor.h"

extern errno;

/*----------------------------------------------------------------------------
This routine searchs for the given key in the specified file and returns the 
field data or all fields depending upon field_no.
Search technique used is sequential search algorithm and btree algorithm.
----------------------------------------------------------------------------*/
int ARY_IndxaryRetrieve(filename, key_no, key_value, ret_column, data)
const	char	filename[];		/* file name */
int	key_no;				/* key field - column number */
const	char	key_value[];		/* key value to searched for */
int	ret_column;			/* column to be retrieved */
char	*data;				/* stored value of resultant */
{
	FILE 	*fp;
	struct 	__record rec;
	int 	file_is_indexed = 0;
	int 	index_key, i, ret_code;
	char	diag_mesg[512];

	data[0] = '\0';

	sprintf(diag_mesg,"ARY_IndxaryRetrieve(%s, %d, %s, %d, %s)",
    	filename, key_no, key_value, ret_column, "data");
	ret_code = send_to_monitor(MON_API, ARY_INDXARYRETRIEVE, diag_mesg);

	/* Check to see if appropriate Init API has been called. */
	if(GV_InitCalled != 1)
	{
    		LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOTINIT, "Service");
    		return (-1);
	}

	/* search for the file name in index table */
	for(i=0; i< MAX_FILE; i++)
	{
    		if(strcmp(filename, __idx_tabl[i].file) == 0 && 
			__idx_tabl[i].key_no == key_no)
    		/* found entry w/ same key,index table - file already indexed */
    		{
			file_is_indexed = 1;
			index_key = i;
			break;
    		}
	}

	strcpy(rec.key, key_value);

	if(file_is_indexed)
    		/* search using btree technique */
    		ret_code = search_key(__idx_tabl[index_key].idx_ptr, 
				ret_column, &rec);
	else
    		/* search using sequential technique */
    		ret_code = search_seq(filename, key_no, ret_column, &rec);

	switch(ret_code)
	{
    	case	0:
		strcpy(data, rec.rec);	/* copy resultant data */
		send_to_monitor(MON_DATA, 0, data);	
		break;
    	case	-1:
		if (file_is_indexed)
	    		LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EEMPTYBTREE,
				filename);
		else
			LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EOPENDATFIL,
				filename, errno);
		break;
    	case	-2:
		sprintf(diag_mesg,
			"Key value <%s> not found in column %d of file %s.",
			key_value, key_no, filename);	
		LOGXXPRT(ModuleName, REPORT_VERBOSE, COM_ISPDEBUG, diag_mesg); 
		break;
    	case	-3:
		LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EKEYFIELD, 
					filename, key_no);
		break;
    	case	-4:
		LOGXXPRT(ModuleName,REPORT_NORMAL,COM_EKEYFNDNOFIELD,
			key_value, key_no, ret_column, filename);
		break;
	} /* case */

	return (ret_code);
} 

/*--------------------------------------------------------------------------
This routine searches for the given key number and returns the field data 
for the field number specified in the arguments. 
The algorithm used is sequential file search.
----------------------------------------------------------------------------*/
int search_seq(filename, key_no, field_no, key)
char filename[];		/* name  search  file */
int  key_no;			/* key_no is key field no in file */
int  field_no;			/* retrieve which field */
struct __record *key;		/* key value for which data to be search */
{
	int 	ret = 0;
	int 	file_key_no = 0;
	int 	var_used = 0;
	int 	file_fld_no = 0;
	char 	key_data[MAX_DATA];
	char 	buf[MAX_DATA];
	char 	*tmp = NULL;
	FILE 	*fp;

	key->rec[0] = '\0';
	if ((fp = fopen(filename,"r")) == NULL)
	{
    		LOGXXPRT(ModuleName,REPORT_NORMAL,COM_UTLFILE_OPEN,
		filename, errno);
    		return (-1);				
	}

	while (!feof(fp))	/* read file  */
	{
   		buf[0] = '\0';
    		fgets(buf, MAX_DATA-1, fp);
    		if (feof(fp))
    		{
			/* Reached end of file - didn't find a match */
			fclose(fp);
			return (-2);
    		}
		if(buf[0]=='#')
			continue;
	    	/* read key field into key_data */
    		if (UTL_GetField(buf, key_no, key_data) < 0 )
    		{
			fclose(fp);
			return (-3);	/* data corruption */
    		}

    		if(strcmp(key->key, key_data) == 0)	/* key found */
    		{
			if (field_no == 0) 
			{
				/* return the entire record */
	    			strcpy(key->rec, buf);
	    			fclose(fp);
	    			return (0);	/* data found */
			}
			else
			{
	    			/* return only the field  requested */
	    			if(UTL_GetField(buf, field_no, key->rec) < 0)
	    			{
					return (-4);	/* data not found */
	    			}
				fclose(fp);
	    			return (0);
			}
    		} 
	} /* while */

	if (fclose(fp) == EOF)
    		return (-1);
	else
    		return (0);
} 

/*-----------------------------------------------------------------------------
This routine searches for the key and returns the field data or 
all fields depending upon field_no variable is selected. 
The search technique used - btree algorithm.
------------------------------------------------------------------------------*/
int search_key(btree, field_no, rec)	/* the root of tree and record */
__NODEPTR	*btree;			/* pointer to root of tree */
int 		field_no;		/* field number to be retrived */
struct __record *rec;			/* record containing key to be search */
{
	int 	  direction;
	__NODEPTR *tmp = btree;

	rec->rec[0] = '\0';
	if (tmp == NULL) return (-1);	/* empty tree */

	while (tmp != NULL) /* search upto last leaf (EOF) */
	{
    		direction = strcmp(tmp->data.key, rec->key);
    		if (direction == 0)	/* string found */
    		{
			if ( field_no == 0)	
			{
				/* return the entire record */
	    			rec->rec[0] = '\0';
	    			strcpy(rec->rec, tmp->data.rec);	
	    			return (0);	
			}
			else
			{
	    			rec->rec[0] = '\0';
	    		if (UTL_GetField(tmp->data.rec, field_no,rec->rec) < 0)
	    			{
					strcpy(rec->rec, "\0");	
					return (-4);	/*  field not found */
	    			}
	    			return (0);	
			}
    		}
    		if (direction < 0)
			tmp = tmp->right;
   	 	else
			tmp = tmp->left;
	} /* while */

	return (-2);	/* not found */
} 
