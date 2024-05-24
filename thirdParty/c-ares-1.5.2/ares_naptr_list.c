#include <stdio.h>

#include "ares.h"

int
ares_naptr_list_add (struct ares_naptr_list_t **list, uint16_t order, uint16_t preference, const char *flag, const char *service, const char *re,
                     const char *replacement)
{

 int rc = -1;
 struct ares_naptr_list_t *elem;
 struct ares_naptr_list_t *next;

 if (list) {
  elem = calloc (1, sizeof (struct ares_naptr_list_t));
  if (elem) {
   elem->order = order;
   elem->preference = preference;
   snprintf (elem->flag, sizeof (elem->flag), "%s", flag);
   snprintf (elem->service, sizeof (elem->service), "%s", service);
   snprintf (elem->re, sizeof (elem->re), "%s", re);
   snprintf (elem->replacement, sizeof (elem->replacement), "%s", replacement);
  }
  if (*list == NULL) {
   *list = elem;
  }
  else {
   // sort on insert ?
   next = *list;
   *list = elem;
   (*list)->next = next;
  }
 }

 return rc;
}

void
ares_naptr_list_free (struct ares_naptr_list_t **list)
{

 struct ares_naptr_list_t *elem;
 struct ares_naptr_list_t *next;

 if (list && *list) {
  elem = *list;
  while (elem) {
   next = elem->next;
   free (elem);
   elem = next;
  }
  *list = NULL;
 }
 return;
}


#ifdef MAIN 

int 
main(){

  int i;
   
  struct ares_naptr_list_t *elem = NULL;

  for(i = 0; i < 255; i++){
    ares_naptr_list_add(&elem, i, i, "u", "service", "re", "replacement");
  }
  ares_naptr_list_free(&elem);

  return 0;
}

#endif 

