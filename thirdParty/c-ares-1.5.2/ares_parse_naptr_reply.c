/* Copyright 1998 by the Massachusetts Institute of Technology.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

/*
 * ares_parse_naptr_reply created by Joseph Santapau 
 */

#include "setup.h"

#if defined(WIN32) && !defined(WATT32)
#include "nameser.h"
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#endif

#include <stdlib.h>
#include <string.h>
#include "ares.h"
#include "ares_dns.h"
#include "ares_private.h"

int ares_parse_naptr_reply(const unsigned char* abuf, int alen, struct ares_naptr_list_t **list)
{
  
  int id, type;
  unsigned int qdcount, ancount, nscount, arcount, i;
  const unsigned char *aptr, *p;
  char *name;
  int dlen;
  int rc = ARES_SUCCESS;
  int status;
  long len;

  *list = NULL;

  /* Won't happen, but check anyway, for safety. */
  if (alen < HFIXEDSZ)
    return;

  /* Parse the answer header. */
  qdcount = DNS_HEADER_QDCOUNT(abuf);
  ancount = DNS_HEADER_ANCOUNT(abuf);

  /* Expand the name from the question, and skip past the question. */

  aptr = abuf + HFIXEDSZ;

  status = ares_expand_name(aptr, abuf, alen, &name, &len);

  if (status != ARES_SUCCESS)
    return status;

  if (aptr + len + QFIXEDSZ > abuf + alen)
  {
    free(name);
    return ARES_EBADRESP;
  }
  aptr += len + QFIXEDSZ;

  free(name);

  for (i = 0; i < ancount; i++)
  {
      uint16_t order;
      uint16_t preference;
      char *flag = NULL;
      char *service = NULL;
      char *buff = NULL;
      char *re = NULL;
      char *replacement = NULL;
      char delim;

      status = ares_expand_name(aptr, abuf, alen, &name, &len);
      if (status != ARES_SUCCESS)
        break;

      free(name);

      aptr += len;

      if (aptr + RRFIXEDSZ > abuf + alen)
      {
        status = ARES_EBADRESP;
        break;
      }

      int rr_type = DNS_RR_TYPE(aptr);
      int rr_class = DNS_RR_CLASS(aptr);
      int rr_len = DNS_RR_LEN(aptr);

      aptr += RRFIXEDSZ;

      if(aptr && rr_type == T_NAPTR){

       order = DNS__16BIT(aptr);
       preference = DNS__16BIT(aptr + 2);

       p = aptr + 4;

       status = ares_expand_string(p, abuf, alen, (unsigned char **)&flag, &len);
       if (status != ARES_SUCCESS){
         flag ? free(flag) : 0 ;
         return status;
       }
       p += len;

       status = ares_expand_string(p, abuf, alen, (unsigned char **)&service, &len);
       if (status != ARES_SUCCESS){
         flag ? free(flag) : 0 ;
         service ? free(service) : 0 ;
         return status;
       }
       p += len;

       status = ares_expand_string(p, abuf, alen, (unsigned char **)&buff, &len);
       if (status != ARES_SUCCESS){
         flag ? free(flag) : 0;
         service ? free(service) : 0;
         buff ? free(buff) : 0;
         return status;
       }
       p += len; 

      }

      if(buff){
        char *sep;
        char *end;
        delim = buff[0];
        sep = strchr(&buff[1], delim);
        if(sep){
          replacement = &sep[1];
          end = strchr(replacement, delim);
          if(end)
             *end = '\0';
          re = &buff[1];
          *sep = '\0';
        }
      }

      if(flag && service && re && replacement)
        ares_naptr_list_add(list, order, preference, flag, service, re, replacement);

      flag ? free(flag) : 0;
      service ? free(service) : 0;
      buff ? free(buff) : 0;
    
      aptr += rr_len;
  }

  return rc;
}



