#ifndef OSIP_UTILS_DOT_H
#define OSIP_UTILS_DOT_H

#include <stdio.h> 
#include <stdlib.h> 
#include <eXosip2/eXosip.h>

enum arc_sip_msg_encoding_e {
   ARC_SIP_MSG_ENCODE_NONE,
   ARC_SIP_MSG_ENCODE_TEXT, // uses url escapes 
   ARC_SIP_MSG_ENCODE_HEX   // hex encodes the entire string 
};

int arc_encode_hex_str(char *src, char *dest, size_t destsize);

int arc_decode_hex_str(char *src, char *dest, size_t destsize);

int arc_encode_hex_escapes(char *src, char *dest, size_t destsize);

int arc_decode_hex_escapes(char *src, char *dest, size_t destsize);

int arc_sip_msg_get_usertouser(osip_message_t *msg, char *dest, size_t size, int decode);

int arc_sip_msg_set_usertouser(osip_message_t *msg, char *str, int encoding);

int arc_sip_msg_get_useragent(osip_message_t *msg, char *dest);
#endif 

