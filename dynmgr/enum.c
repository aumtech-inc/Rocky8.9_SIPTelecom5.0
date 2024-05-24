#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <resolv.h>
#include <sys/types.h>
#include <regex.h>

#include <ares.h>
#include "ares_dns.h"

char *
enum_convert (const char *num, const char *tld)
{

  char *rc = NULL;
  char buf[256];
  char *ptr = buf;
  int len = strlen (num) + 1;

  memset(buf, 0, sizeof(buf));


  
  for(len; len > -1; len--){
    if (isdigit (num[len])) {
      ptr[0] = num[len];
      ptr[1] = '.';
      ptr += 2;
    }
  }

  //fprintf(stderr, " %s() buf = %s\n", __func__, buf);

  if (tld) {
    snprintf (ptr, sizeof (buf) - strlen (buf), "%s", tld);
  }
  else {
    snprintf (ptr, sizeof (buf) - strlen (buf), "e164.arpa");
  }

  return strdup (buf);
}


void 
dns_cb(void *arg, int status, int timeouts, unsigned char *abuf, int alen){


  if (status != ARES_SUCCESS)
  {
      //fprintf(stderr, "%s\n", ares_strerror(status));
      return;
  }

  ares_parse_naptr_reply(abuf, alen, (struct ares_naptr_list_t **)arg);
}

int
enum_lookup (struct ares_naptr_list_t **list, const char *e_num, int type)
{

  int rc = -1;
  unsigned char *buf; 
  int buflen;
  ares_channel chan;
  int nfds, count;
  fd_set readers, writers;

  res_init ();
  ares_init (&chan);

  if(ares_mkquery(e_num, ns_c_any, ns_t_naptr, 1234, 1, &buf, &buflen) == ARES_SUCCESS){
    //fprintf(stderr, " %s: ares success\n", __func__);
    ares_send(chan, buf, buflen, dns_cb, (void *)list);
    ares_free_string(buf);
  }

  while(1){
    FD_ZERO(&readers);
    FD_ZERO(&writers);
    nfds = ares_fds(chan, &readers, &writers);
    if(nfds == 0)
      break;
    count = select(nfds, &readers, &writers, NULL, NULL);
    ares_process(chan, &readers, &writers);
  }

  ares_destroy(chan);

  return rc;
}

int 
enum_match(struct ares_naptr_list_t *list, const char *service, char *str, char **replacement){

   struct ares_naptr_list_t *elem;
   int rc = -1;
   regex_t reg;
   int reflags = REG_NOSUB;

   *replacement = NULL;
   elem = list;

   while(elem){
      if(!strcmp(service, elem->service)){
         //fprintf(stderr, "here\n");
         if(regcomp(&reg, elem->re, reflags) == 0){
            //fprintf(stderr, "here\n");
            rc = regexec(&reg, str, 0, NULL, 0);
            if(rc == 0){
               //fprintf(stderr, " JOES %s() found match %s\n", __func__, str);
               *replacement = strdup(elem->replacement);
               regfree(&reg);
               break;
            }
            regfree(&reg);
         } 
      }
      elem = elem->next;
   }

   return rc;

}

#define ENUM_SERVICE_SIP "E2U+sip"
#define ENUM_SERVICE_MAILTO "E2U+mailto"
#define ENUM_SERVICE_WEB_HTTP  "E2U+web:http"

#ifdef MAIN

int
main ()
{

  char *test;
  ares_naptr_list_t *list = NULL;
  ares_naptr_list_t *listptr = NULL;
  char *rep = NULL;

  test = enum_convert ("4145", "aumtechinc.com");
  //fprintf(stderr, " test = %s\n", test);
  enum_lookup (&list, test, 0);

  //fprintf(stderr, " %s() list=%p\n", __func__, list);

  printf("match=%d \n", enum_match(list, ENUM_SERVICE_SIP, "", &rep));
  printf("rep=%s\n", rep);

  listptr = list;

  while(listptr){
    fprintf(stderr, " ENUM Record: |%d|%d|%s|%s|%s|%s|\n", listptr->order,
    listptr->preference, listptr->flag, listptr->service, listptr->re, listptr->replacement);
    listptr = listptr->next;
  }
  
  ares_naptr_list_free(&list);
  free(rep);
  free (test);
  return 0;
}

#endif
