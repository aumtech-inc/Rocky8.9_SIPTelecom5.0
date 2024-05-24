#ifndef ENUM_DOT_H
#define ENUM_DOT_H

#include <ares.h>

char *
enum_convert (const char *num, const char *tld);

int
enum_lookup (struct ares_naptr_list_t **list, const char *e_num, int type);

int 
enum_match(struct ares_naptr_list_t *list, const char *service, char *str, char **replacement);

#define ENUM_SERVICE_SIP "E2U+sip"
#define ENUM_SERVICE_MAILTO "E2U+mailto"
#define ENUM_SERVICE_WEB_HTTP  "E2U+web:http"


#endif
