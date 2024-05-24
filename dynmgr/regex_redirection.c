#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <regex.h>

#include <regex_redirection.h>
#include <dynVarLog.h>


#define ACT_LIKE_TELECOM_TABLES 1

/*                                                                         */
/* the code from responsibility                                            */
/* there is another routine that basically does a strcmp on the string     */
/* we might need that routine to duplicate what we have in responsibility  */
/*                                                                         */

static void
convertStarToUnixPatternMatch (char *parmStarString, char *parmResultString)
{
    int i;
    char buf[1024];

    memset (parmResultString, 0, sizeof (parmResultString));

    for (i = 0; i < strlen (parmStarString); i++)
    {
        if (parmStarString[i] == '*')
            sprintf (buf, "%s", "[0-9]*");
        else
            sprintf (buf, "%c", parmStarString[i]);

        strcat (parmResultString, buf);
    }
    sprintf (buf, "%s", "$");
    strcat (parmResultString, buf);

    return;
}


/* end responsibility code */


int
arc_sip_redirection_add_destination (struct arc_sip_redirection_t *r, size_t size, int index, char *host, int port)
{

   int rc = -1;

   struct arc_sip_destination_t *elem;
   struct arc_sip_destination_t *list;


   if(index < 0 || index >= size){
      // fprintf(stderr, " %s() invalid index [%d]\n", __func__, index);
      return -1;
   }

   if (port == 0)
   {
      port = 5060;
   }

   elem = (struct arc_sip_destination_t *) calloc (1, sizeof (struct arc_sip_destination_t));

   if (elem)
   {

      elem->host = strdup (host);
      if (port <= 0)
      {
	 elem->port = 5060;
      }
      else
      {
	 elem->port = port;
      }
	 
	// elem->active = 1;

      list = r[index].head;

      if (list == NULL)
      {
	 r[index].head = elem;
	 rc = 0;
      }
      else
      {
	 while (list)
	 {
	    if ((strcmp (list->host, host) == 0) && list->port == port)
	    {
	       rc = 0;
           // throw away the one we wanted
           // to add 
	       if(elem->host)
                free (elem->host);
	       free (elem);
	       break;
	    }

	    if (list->next == NULL)
	    {
	       list->next = elem;
	       rc = 0;
	       break;
	    }
	    list = list->next;
	 }
      }
   }

   return rc;
}

/* returns number of records updated  */

int
arc_sip_redirection_set_active (struct arc_sip_redirection_t *r, size_t size, char *host, int port, enum arc_sip_redirection_op_e op)
{

   int rc = 0;
   struct arc_sip_destination_t *elem;

   int i;

   if (port == 0)
   {
      port = 5060;
   }

   for (i = 0; i < size; i++)
   {
      elem = r[i].head;
      while (elem)
      {
	 if ((strcmp (elem->host, host) == 0) && (elem->port == port))
	 {
	    switch (op)
	    {
	    case SIP_REDIRECTION_ACTIVATE:
	       elem->active = 1;
	       break;
	    case SIP_REDIRECTION_DEACTIVATE:
	       elem->active = 0;
	       break;
	    case SIP_REDIRECTION_UPDATE:
	       gettimeofday (&elem->last_tv, NULL);
	       break;
	    default:
	       break;
	    }
	    rc++;
	 }
	 elem = elem->next;
      }
   }

   return rc;
}


int
arc_sip_redirection_init (struct arc_sip_redirection_t *r, size_t size)
{

   int rc = -1;

   if (r)
   {
      memset (r, 0, sizeof (struct arc_sip_redirection_t) * size);
      rc = 0;
   }

   return rc;
}

char *
arc_sip_redirection_last_msg (struct arc_sip_redirection_t *r, char *buff, size_t buffsize)
{

   char *rc = "None";

   if ((r != NULL) && buff && buffsize)
   {
     snprintf (buff, buffsize, "%s", r->last_msg);
     rc = buff;
   }

   return rc;
}


/*
  free all malloced elements and regexes in structure
*/

void
arc_sip_redirection_free (struct arc_sip_redirection_t *r, size_t size)
{

   int i;

   struct arc_sip_destination_t *elem = NULL;
   struct arc_sip_destination_t *next = NULL;

   for (i = 0; i < SIP_REDIRECTION_TABLE_SIZE; i++)
   {

      // destination lists 

      elem = r[i].head;

      while (elem)
      {
	 if(elem->host)
			 free (elem->host);
	 next = elem->next;
	 free (elem);
	 elem = next;
      }


      r[i].head = NULL;

      // bucket elements 

      if(r[i].string)
          free (r[i].string);
      if(r[i].string)
          regfree (&r[i].re);
      r[i].string = NULL;

   }

   return;
}

static int
sip_redirection_add_regex (struct arc_sip_redirection_t *r, size_t size, int index, int lineno, char *regex, int literal)
{

   int rc = index;
   int i;
   char *default_regex = "*";
   int status;
   int regflags = REGEX_FLAGS; 
   char buff[1024];
   char *ptr;

   if (!r)
   {
      return rc;
   }

   if (index >= size)
   {
      return -1;
   }

   if ((strcmp (regex, "default") == 0) && (literal == 0))
   {

      if(r[SIP_REDIRECTION_DEFAULT_ROUTE].string == NULL){
       // fprintf (stderr, " %s() found regex keyword default adding to default index\n", __func__);
#ifdef ACT_LIKE_TELECOM_TABLES 
        convertStarToUnixPatternMatch (default_regex, buff);
        ptr = &buff[0];
#endif
       status = regcomp (&r[SIP_REDIRECTION_DEFAULT_ROUTE].re, ptr, regflags);
       if (status == 0)
       {
 	      r[SIP_REDIRECTION_DEFAULT_ROUTE].string = strdup (ptr);
       }
       else
       {
 	    snprintf(r->last_msg, sizeof(r->last_msg), " %s() failed to compile regex\n", __func__);
		dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL,  TEL_BASE, ERR, 
       		"REGEX failed to compile regex.  returning -1");
	    return -1;
       }
      }
      return SIP_REDIRECTION_DEFAULT_ROUTE;
   }


   for (i = 0; i < index; i++)
   {
#ifdef ACT_LIKE_TELECOM_TABLES 
   convertStarToUnixPatternMatch (regex, buff);
   ptr = &buff[0];
#endif
      if (r[i].string != NULL)
      {
	 if (strcmp (r[i].string, ptr) == 0)
	 {
	    snprintf (r->last_msg, sizeof(r->last_msg), " %s() matching regex [%s] already found at index %d, returing %d \n", __func__, ptr, i, i);
	    return i;
	 }
      }
   }

#ifdef ACT_LIKE_TELECOM_TABLES 
   convertStarToUnixPatternMatch (regex, buff);
   ptr = &buff[0];
#endif 

   status = regcomp (&r[index].re, ptr, regflags);

   if (status == 0)
   {
      r[index].string = strdup (ptr);
   }
   else
   {
      snprintf (r->last_msg, sizeof(r->last_msg), " %s() failed to compile regex\n", __func__);
      rc = -1;
   }

   return rc;
}

int
arc_sip_redirection_find_next (struct arc_sip_redirection_t *r, size_t size, int last, char *match)
{

   int rc = -1;
   int i;

   if (last >= size)
   {
      return -1;
   }

   if (last <= 0)
   {
      i = 0;
   }

   if(last > -1 && last < size){
     i = last;
   }

   for (i = i; i < size; i++)
   {
      if (r[i].string != NULL)
      {

#ifdef ACT_LIKE_TELECOM_TABLES

        if(strcmp(r[i].string, match) == 0){
           rc = 1;
           break;
        }
#endif
	    if (regexec (&r[i].re, match, 0, 0, 0) == 0)
	    {
            snprintf (r->last_msg, sizeof(r->last_msg), " %s() Found match for string[%s] and regex[%s] at index %d\n", __func__, match, r[i].string, i);
	        rc = i;
            break;
	    }
      }
   }

   return rc;
}



static int
tv_sort (const void *a, const void *b)
{

   int rc = -1;

   // handle NULL conditions 
   if (a && !b)
   {
      return 1;
   }

   if (!a && b)
   {
      return -1;
   }

   if (!a && !b)
   {
      return 0;
   }

   struct arc_sip_destination_t **dest_a = (struct arc_sip_destination_t **) a;
   struct arc_sip_destination_t **dest_b = (struct arc_sip_destination_t **) b;
   // 
   struct timeval *tv_a = &(*dest_a)->last_tv;
   struct timeval *tv_b = &(*dest_b)->last_tv;

   //fprintf(stderr, "entering sort a=%p b=%p cb tv_a=[%d:%d] tv_b=[%d:%d]", a, b, tv_a->tv_sec, tv_a->tv_usec,  tv_b->tv_sec, tv_b->tv_usec);

   if (tv_a->tv_sec > tv_b->tv_sec)
   {
      rc = 1;
   }
   else if (tv_a->tv_sec < tv_b->tv_sec)
   {
      rc = -1;
   }
   else if (tv_a->tv_sec == tv_b->tv_sec)
   {
      if (tv_a->tv_usec > tv_b->tv_usec)
      {
	 rc = 1;
      }
      else if (tv_a->tv_usec < tv_b->tv_usec)
      {
	 rc = -1;
      }
      else if (tv_a->tv_usec == tv_b->tv_usec)
      {
	 rc = 0;
      }
   }

   //fprintf(stderr, " returning rc=%d\n", rc);

   return rc;
}

int
arc_sip_redirection_get_destination (struct arc_sip_redirection_t *r, size_t size, int index, char *dest, size_t destsize, int *port)
{

   int rc = -1;
   int count = 0;

   struct arc_sip_destination_t *dests[SIP_REDIRECTION_TABLE_SIZE];
   struct arc_sip_destination_t *elem;

   if (!r)
   {
      return -1;
   }

   if (!r[index].head)
   {
      return -1;
   }

   elem = r[index].head;
   memset (dests, 0, sizeof (dests));

   while (elem)
   {
      if (elem->active)
      {
	 dests[count] = elem;
	 count++;
//     dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//			"REGEX ptr=%p host=%s port=%d ts=[%d:%4d]", 
//			elem, elem->host, elem->port,(int) elem->last_tv.tv_sec,(int) elem->last_tv.tv_usec);
      }
      elem = elem->next;
   }

//   dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//   			"REGEX count = %d\n", count);
   qsort (dests, count, sizeof (struct sip_destination_t *), tv_sort);


#if 0

   int i;

   for (i = 0; dests[i]; i++)
   {
      elem = dests[i];
//      fprintf (stderr, " %s() sorted element %d ptr=%p host=%s port=%d ts=[%d:%4d]\n", __func__, i, dests[i], elem->host, elem->port, elem->last_tv.tv_sec,
	       elem->last_tv.tv_usec);
   }

#endif

   if (dests[0] != NULL)
   {
      elem = dests[0];
      // snprintf (dest, destsize, "%s:%d", elem->host, elem->port);
      snprintf (dest, destsize, "%s", elem->host);
      *port = elem->port;
//		dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//      				"REGEX returning host=%s port=%d ts=[%d:%4d] index=%d",
//					elem->host, elem->port, elem->last_tv.tv_sec, elem->last_tv.tv_usec, index);
      arc_sip_redirection_set_active (r, size, elem->host, elem->port, SIP_REDIRECTION_UPDATE);
      rc = 0;
   }
   return rc;
}

static void 
arc_sip_redirection_free_destination_list(struct arc_sip_destination_t **list){

  struct arc_sip_destination_t *elem;
  struct arc_sip_destination_t *next;

  if(list && *list){
    elem = *list;

    while(elem){
      if(elem->host){
        free(elem->host);
      }
      next = elem->next;
      free(elem);
      elem = next;
    }

    *list = NULL;
  }

  return;
}

static void 
arc_sip_redirection_free_active_list(struct arc_sip_destination_t **list){

 struct arc_sip_destination_t *elem = NULL;
 struct arc_sip_destination_t *next = NULL;

  if(list){
    elem = *list;
    while(elem){
       next = elem->next;
       if(elem->host){
         free(elem->host);
       }
       free(elem);
       elem = next;
    }
    *list = NULL;
  }

  return;
}


static int  
arc_sip_redirection_build_active_list(struct arc_sip_redirection_t *r, size_t size, struct arc_sip_destination_t **list){

   int rc = 0;
   int i;

   struct arc_sip_destination_t *elem = NULL;
   struct arc_sip_destination_t *elem2 = NULL;
   struct arc_sip_destination_t *next = NULL;

   for(i = 0; i < size; i++){

     elem = r[i].head;

     while(elem){
        if(elem->active == 1){

          //fprintf(stderr, " %s() found active element for ip:port %s:%d duplicating it for reload list\n", __func__, elem->host, elem->port);

          elem2 = (struct arc_sip_destination_t *)calloc(1, sizeof(struct arc_sip_destination_t)); 

          if(elem2 != NULL){
            elem2->host = strdup(elem->host);
            elem2->port = elem->port;
            elem2->active = elem->active;
          } else {
            //fprintf(stderr, " %s() failed to allocate memory for duplicating registration list\n", __func__);
			dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL,  TEL_BASE, ERR, 
           	 "Failed to allocate memory for duplicating registration list");
            break;
          }

          if(*list == NULL){
            *list = elem2;
          } else {
            next = *list;
            while(next){

              if((strcmp(next->host, elem2->host) == 0) && next->port == elem2->port){
                 // fprintf(stderr, " %s() already in list host:port %s:%d\n", __func__, elem2->host, elem2->port);
                 if(elem2->host){
                   free(elem2->host);
                 } 
                 free(elem2);
                 break;
              }
             // not found in list add it 
             if(next->next == NULL){
                next->next = elem2;
                break;
              }
 
              next = next->next;
            } // end inner while 
          }
        }  // end if active 
        elem = elem->next;
     } // end outer while 
  } // end for

  return rc;
}


static int
arc_sip_redirection_test_active_destination(struct arc_sip_destination_t *list, char *host, int port){

  int rc = 0;
  struct arc_sip_destination_t *elem;

  if(list){
    elem = list;
    while(elem){
       if((strcmp(elem->host, host) == 0) && (elem->port == port)){
          rc = 1;
          break;
       }
      elem = elem->next;
    }
  }

  return rc;
}


int 
arc_sip_redirection_reload_table(struct arc_sip_redirection_t *r, size_t size, const char *file){

  int rc = -1;
  static time_t last;
  struct stat buff;
  int reload = 0;
  int i;

  struct arc_sip_destination_t *dests = NULL;
  struct arc_sip_destination_t *elem = NULL;

  if(access(file, F_OK) == 0){
    rc = stat(file, &buff);
    if(rc == 0){
       if((buff.st_mtime > last)){
         memcpy(&last, &buff.st_mtime, sizeof(last));
         // fprintf(stderr, " %s() DEBUG reloading redirector table mtime=%d last=%d\n", __func__, (int)buff.st_mtime, (int)last);
         reload = 1;
       }
    }
  }

  if(reload == 1){
   arc_sip_redirection_build_active_list(r, size, &dests);
   arc_sip_redirection_free(r, size);
   arc_sip_redirection_init(r, size);
   arc_sip_redirection_load_table (r, size, file);
//	arc_sip_redirection_print_table (r, size);
   for(i = 0; i < size; i++){
     elem = r[i].head;
     while(elem){
       if(arc_sip_redirection_test_active_destination(dests, elem->host, elem->port) == 1){
          // fprintf(stderr, " %s() setting host:port %s:%d back to active\n", __func__, elem->host, elem->port);
          arc_sip_redirection_set_active (r, size, elem->host, elem->port, SIP_REDIRECTION_ACTIVATE);
          // set formerly active hosts back to active
       }
       elem = elem->next;
     }
   }
   arc_sip_redirection_free_active_list(&dests);
  }

  return rc;
}

int
arc_sip_redirection_load_table (struct arc_sip_redirection_t *r, size_t size, const char *file)
{

   int rc = -1;
   char line[1024];
   int i;
   int len;
   char *reason = "None";
   int lineno = 0;

   int idx = 0;
   int real_index = 0;

   enum parse_state
   {
      BEGIN,
      END,
      COMMENTED,
      REGEX,
      HOST,
      READY,
      ERROR
   };

   int state;

   FILE *table;
   char *fields[256];
   char buff[1024];
   char host[320];
   int port;
   char *ptr;
   char *sep;

   table = fopen (file, "r");

   if (!table)
   {
      return -1;
   }

   memset (fields, 0, sizeof (fields));

   while (fgets (line, sizeof (line), table))
   {

      int pos = 0;
      int hosts = 0; 
      int literal = 0;
      state = BEGIN;
      lineno++;

      if (line[strlen (line) - 1] == '\n')
      {
	 line[strlen (line) - 1] = '\0';
      }

//	dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//			"REGEX line=(%s)  state=%d, ERROR=%d  HOST=%d  REGEX=%d END=%d  BEGIN=%d COMMENTED=%d READY=%d",
//			line, state, ERROR, HOST, REGEX, END, BEGIN, COMMENTED, READY);
      while (state != END)
      {

	 if (state == BEGIN)
	 {
	    ptr = line;
	    if (*ptr == '#' || *ptr == '\n' || *ptr == '\0')
	    {
	       state = COMMENTED;
	    }
	    else
	    {
	       state = REGEX;
	    }
	 }

	 if (state == COMMENTED)
	 {
	    state = END;
	 }

//	dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//			"REGEX state is now %d.", state);
	 if (state == REGEX)
	 {
	    if (line[0] == '"')
	    {
	       ptr++;
	       len = strcspn (ptr, "\"");
	       if (len)
	       {
		  snprintf (buff, len + 1, "%s", ptr);
		  //fprintf(stderr, " %s() quoted regex=[%s]\n", __func__, buff);
          literal = 1;
		  ptr += len + 2;
		  fields[pos++] = strdup (buff);
		  state = HOST;
	       }
	       else
	       {
		  reason = "Parse error in quoted regex";
		  state = ERROR;
	       }
	    }
	    else
	    {
	       len = strcspn (line, "|");
	       if (len)
	       {
		  snprintf (buff, len + 1, "%s", ptr);
		  //fprintf(stderr, " %s() un-quoted regex=[%s]\n", __func__, buff);
		  ptr += len + 1;
		  fields[pos++] = strdup (buff);
		  state = HOST;
	       }
	       else
	       {
		  reason = "Parse error in un-quoted regex";
		  state = ERROR;
	       }
	    }
	 }

	 if (state == HOST)
	 {
	    len = strcspn (ptr, "|");
	    if (len)
	    {
	       snprintf (buff, len + 1, "%s", ptr);
	       //fprintf(stderr, " %s() host=[%s]\n", __func__, buff);
	       fields[pos++] = strdup (buff);
	       ptr += len + 1;
	       hosts++;
	    }
	    else
	    {
	       if (hosts > 0)
	       {
		  state = READY;
	       }
	       else
	       {
		  reason = "Parse error host list";
		  state = ERROR;
	       }
	    }
	 }

	 if (state == READY)
	 {
	    int i;
	    real_index = sip_redirection_add_regex (r, size, idx, lineno, fields[0], literal);
//	dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//			"REGEX adding regex[%s] real_index=[%d]\n", fields[0], real_index);
	    // fprintf (stderr, " adding regex[%s] real_index=[%d]\n", fields[0], real_index);
	    for (i = 1; i < 256; i++)
	    {
	       if (fields[i] == NULL)
	       {
		  break;
	       }
	       if (fields[i][0] == '[')
	       {
//	dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//		   " parsing ipv6 address fields=[%s]", fields[i]);
		  // fprintf (stderr, " %s() parsing ipv6 address fields=[%s]\n", __func__, fields[i]);
		  // ip v6 port parsing 
		  len = strcspn (fields[i], "]");
		  if (len)
		  {
		     sep = fields[i] + len;
                     sep = strstr(sep, ":");
                     if(sep){
                       *sep = '\0';
                       port = atoi(sep + 1);
                     }
                     snprintf(host, sizeof(host), "%s", fields[i]);
		  }
		  ;;
	       }
	       else
	       {
		  sep = strstr (fields[i], ":");
		  if (sep)
		  {
		     port = atoi (sep + 1);
		     *sep = '\0';
		     snprintf (host, sizeof (host), "%s", fields[i]);
		  }
		  else
		  {
		     snprintf (host, sizeof (host), "%s", fields[i]);
		     port = 5060;
		  }
	       }

	       // fprintf (stderr, " real_index=%d host %i=[%s] port=%d", real_index, i, host, port);
	       // arc_sip_redirection_add_destination (r, size, idx, host, port);
	       arc_sip_redirection_add_destination (r, size, real_index, host, port);

//	dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//		"REGEX real_index=%d host %i=[%s] port=%d", real_index, i, host, port);
	    }
	    // fprintf (stderr, "\n");
	    state = END;
	    idx++;
	 }

	 if (state == ERROR)
	 {
#if 1
        dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL,  TEL_INVALID_DATA, ERR,
            " Error in parsing redirection table line=[%d] reason=%s\n", lineno, reason);
#endif
	    // fprintf (stderr, " %s() error in parsing redirection table line=[%s] reason=%s\n", __func__, ptr, reason);
	    state = END;
	 }

	 // free entries 
	 if (state == END)
	 {
	    for (i = 0; i < 256; i++)
	    {
	       if(fields[i])
               free (fields[i]);
           if(fields[i])
               fields[i] = NULL;
	    }
	 }
      }
   }

   if (table != NULL)
   {
      fclose (table);
   }

   return rc;

}

int
arc_sip_redirection_print_table (struct arc_sip_redirection_t *r, size_t size)
{
	int								i;
	struct arc_sip_destination_t	*elem;

   if (!r)
   {
     dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, "redirection table is null.  Nothing to print.");
      return -1;
   }

   if (!r[0].head)
   {
     dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, "redirection table is empty.  Nothing to print.");
      return -1;
   }

	for (i = 0; i < size; i++)
	{
   		if (!r[i].head)
		{
			i=size+1;
			continue;
		}

		dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO,
			"REGEX r[%d].string:(%s)", i, r[0].string);
		dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO,
			"REGEX r[%d].lineno:(%d)", i, r[0].lineno);
		dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO,
			"REGEX r[%d].last_msg:(%d)", i, r[0].last_msg);
		elem = r[i].head;
		while (elem)
		{
			dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO,
				"REGEX elem->[host, port, active] = [%s, %d, %d]", elem->host, elem->port, elem->active);
			elem = elem->next;
		}
	}

	return(0);
}
#ifdef MAIN

struct arc_sip_redirection_t sip_res[SIP_REDIRECTION_TABLE_SIZE];

int
main (int argc, char **argv)
{

   char *dnis[] = {
      "1234567890",
      "1234",
      "12345",
      "123456",
      "567890",
      "18885551212",
      "1-888-555-1212",
      "regex",
      NULL
   };

   int i;			// , j, k;
   //int rc;

   arc_sip_redirection_init (sip_res, SIP_REDIRECTION_TABLE_SIZE);

   arc_sip_redirection_load_table (sip_res, SIP_REDIRECTION_TABLE_SIZE, "test.cfg");

   arc_sip_redirection_set_active (sip_res, SIP_REDIRECTION_TABLE_SIZE, "10.0.10.45", 5060, SIP_REDIRECTION_ACTIVATE);
   arc_sip_redirection_set_active (sip_res, SIP_REDIRECTION_TABLE_SIZE, "10.0.10.45", 5061, SIP_REDIRECTION_ACTIVATE);
   arc_sip_redirection_set_active (sip_res, SIP_REDIRECTION_TABLE_SIZE, "10.0.10.45", 5062, SIP_REDIRECTION_ACTIVATE);
   arc_sip_redirection_set_active (sip_res, SIP_REDIRECTION_TABLE_SIZE, "10.0.10.45", 4500, SIP_REDIRECTION_ACTIVATE);
   arc_sip_redirection_set_active (sip_res, SIP_REDIRECTION_TABLE_SIZE, "10.0.10.46", 5060, SIP_REDIRECTION_ACTIVATE);
   arc_sip_redirection_set_active (sip_res, SIP_REDIRECTION_TABLE_SIZE, "10.0.10.46", 4500, SIP_REDIRECTION_ACTIVATE);
//	arc_sip_redirection_print_table (sip_res, SIP_REDIRECTION_TABLE_SIZE);

   for (i = 0; dnis[i] != NULL; i++)
   {

      int last = 0;
      int tries = 10;
      char buff[1024];
      int port;
      char uri[1024];
      int idx;
      int status;

//		dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
// 				"REGEX dnis[%d]=(%s)", i, dnis[i]);

      while (idx != -1)
      {
	 idx = arc_sip_redirection_find_next (sip_res, SIP_REDIRECTION_TABLE_SIZE, 0, dnis[i]);
	 last = idx + 1;
 
         if(idx == SIP_REDIRECTION_DEFAULT_ROUTE){
           ;; // fprintf(stderr, " %s() returned default route\n", __func__);
         }

//		dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//	 				"REGEX idx=%d last=%d", idx, last);
	 // fprintf (stderr, " idx=%d last=%d\n", idx, last);

	 if (idx != -1)
	 {
	    while (tries--)
	    {
	       status = arc_sip_redirection_get_destination (sip_res, SIP_REDIRECTION_TABLE_SIZE, idx, buff, sizeof (buff), &port);
	       if (status != -1)
	       {
		  snprintf (uri, sizeof (uri), "<sip:%s@%s:%d>", dnis[i], buff, port);
		   // fprintf (stderr, " destination host:port = %s matching idx=%d status=%d string=[%s] uri=%s\n", buff, idx, status, dnis[i], uri);
//		dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE,  TEL_BASE, INFO, 
//		   "REGEX destination host:port = %s matching idx=%d status=%d string=[%s] uri=%s", buff, idx, status, dnis[i], uri);
	       }
	    }
	 }
	 else
	 {
	    // fprintf (stderr, " %s() idx=%d\n", __func__, idx);
	    break;
	 }
      }				// end while 
   }

   arc_sip_redirection_free (sip_res, SIP_REDIRECTION_TABLE_SIZE);

   return 0;
}

#endif
