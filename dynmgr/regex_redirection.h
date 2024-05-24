#ifndef REGEX_REDIRECTION_DOT_H
#define REGEX_REDIRECTION_DOT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <regex.h>

#ifdef __cplusplus 
extern "C" {
#endif

#define SIP_REDIRECTION_TABLE_SIZE 1024
#define SIP_REDIRECTION_DEFAULT_ROUTE (SIP_REDIRECTION_TABLE_SIZE - 1)
// #define REGEX_FLAGS (REG_NEWLINE | REG_EXTENDED)
#define REGEX_FLAGS (0) 

struct arc_sip_destination_t
{
   char *host;
   unsigned short int port;
   int active;
   struct timeval last_tv;
   struct arc_sip_destination_t *next;
};

struct arc_sip_redirection_t
{
   char *string;
   regex_t re;
   regmatch_t match;
   int lineno;
   char last_msg[256];
   struct arc_sip_destination_t *head;
};

/* this is used internally in the loading of the table          */
/* might be useful for other operations so it is being exported */

int
arc_sip_redirection_add_destination (struct arc_sip_redirection_t *r, size_t size, int index, char *host, int port);

enum arc_sip_redirection_op_e
{
   SIP_REDIRECTION_ACTIVATE,
   SIP_REDIRECTION_DEACTIVATE,
   SIP_REDIRECTION_UPDATE
};

/* this is to be called from the current redirection code */

int arc_sip_redirection_set_active (struct arc_sip_redirection_t *r, size_t size, char *host, int port, enum arc_sip_redirection_op_e op);

int arc_sip_redirection_init (struct arc_sip_redirection_t *r, size_t size);

/* get the last message/failure code anytime you think you need it */

char * arc_sip_redirection_last_msg (struct arc_sip_redirection_t *r, char *buff, size_t buffsize);

/*
  free all malloced elements and regexes in structure
*/

void arc_sip_redirection_free (struct arc_sip_redirection_t *r, size_t size);

int arc_sip_redirection_find_next (struct arc_sip_redirection_t *r, size_t size, int last, char *match);

int arc_sip_redirection_get_destination (struct arc_sip_redirection_t *r, size_t size, int index, char *dest, size_t destsize, int *port);

int arc_sip_redirection_load_table (struct arc_sip_redirection_t *r, size_t size, const char *file);

int arc_sip_redirection_reload_table(struct arc_sip_redirection_t *r, size_t size, const char *file);

int arc_sip_redirection_print_table (struct arc_sip_redirection_t *r, size_t size);


#ifdef __cplusplus 
};
#endif


#endif
