#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <eXosip2/eXosip.h>

#include "osip_utils.h"


int 
arc_encode_hex_str(char *src, char *dest, size_t destsize){

  int rc = 0; 
  int count = 0;

   if(!src || !dest){
     return -1;
   }

   while(*src && destsize > 3){
     snprintf(&dest[rc], 3, "%02x", *src);
     rc += 2; count++; 
     destsize -= 2;
     src++;
   } 

   dest[rc] = '\0';
   return rc;
}

/*
   Here dest can contain just about anything, including binary data 
*/


int 
arc_decode_hex_str(char *src, char *dest, size_t destsize){

  int rc = 0;
  char buff[3] = "";
  int count = 0;
  int len;

  if(!src || !dest){
    return -1;
  }

  len = strlen(src);

  while(*src && (len >= 2) && destsize){
	buff[0] = src[count];
	buff[1] = src[count + 1];
    buff[2] = '\0';
    // fprintf(stderr, "char in buff=[%s]\n", buff);
    dest[rc] = strtol(buff, NULL, 16);
    count += 2;
    destsize--;
    len -= 2;
    rc++;
  } 

  dest[rc] = '\0';

  return rc;

}


int 
arc_encode_hex_escapes(char *src, char *dest, size_t destsize){

  int rc = 0;

  const char table[] = {
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1,
   1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
   0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };


  if(!src || !dest){
    return -1;
  }

  while(*src && destsize > 4){
      if(table[(int)*src]){
        snprintf(&dest[rc], 4, "%%%2x", *src);
        rc += 3;
        destsize -= 3;
      } else {
	    dest[rc] = *src;
        rc++;
        destsize--;
      }
      src++;
  }

  dest[rc] = '\0';

  return rc;
}



int 
arc_decode_hex_escapes(char *src, char *dest, size_t destsize){

  int rc = 0;

  signed char lookup[] =  {
   (signed char)0x00, (signed char)0x01, (signed char)0x02, (signed char)0x03, (signed char)0x04, (signed char)0x05, (signed char)0x06, (signed char)0x07, 
   (signed char)0x08, (signed char)0x09, (signed char)0x0a, (signed char)0x0b, (signed char)0x0c, (signed char)0x0d, (signed char)0x0e, (signed char)0x0f, 
   (signed char)0x10, (signed char)0x11, (signed char)0x12, (signed char)0x13, (signed char)0x14, (signed char)0x15, (signed char)0x16, (signed char)0x17, 
   (signed char)0x18, (signed char)0x19, (signed char)0x1a, (signed char)0x1b, (signed char)0x1c, (signed char)0x1d, (signed char)0x1e, (signed char)0x1f, 
   (signed char)0x20, (signed char)0x21, (signed char)0x22, (signed char)0x23, (signed char)0x24, (signed char)0x25, (signed char)0x26, (signed char)0x27, 
   (signed char)0x28, (signed char)0x29, (signed char)0x2a, (signed char)0x2b, (signed char)0x2c, (signed char)0x2d, (signed char)0x2e, (signed char)0x2f, 
   (signed char)0x30, (signed char)0x31, (signed char)0x32, (signed char)0x33, (signed char)0x34, (signed char)0x35, (signed char)0x36, (signed char)0x37, 
   (signed char)0x38, (signed char)0x39, (signed char)0x3a, (signed char)0x3b, (signed char)0x3c, (signed char)0x3d, (signed char)0x3e, (signed char)0x3f, 
   (signed char)0x40, (signed char)0x41, (signed char)0x42, (signed char)0x43, (signed char)0x44, (signed char)0x45, (signed char)0x46, (signed char)0x47, 
   (signed char)0x48, (signed char)0x49, (signed char)0x4a, (signed char)0x4b, (signed char)0x4c, (signed char)0x4d, (signed char)0x4e, (signed char)0x4f, 
   (signed char)0x50, (signed char)0x51, (signed char)0x52, (signed char)0x53, (signed char)0x54, (signed char)0x55, (signed char)0x56, (signed char)0x57, 
   (signed char)0x58, (signed char)0x59, (signed char)0x5a, (signed char)0x5b, (signed char)0x5c, (signed char)0x5d, (signed char)0x5e, (signed char)0x5f, 
   (signed char)0x60, (signed char)0x61, (signed char)0x62, (signed char)0x63, (signed char)0x64, (signed char)0x65, (signed char)0x66, (signed char)0x67, 
   (signed char)0x68, (signed char)0x69, (signed char)0x6a, (signed char)0x6b, (signed char)0x6c, (signed char)0x6d, (signed char)0x6e, (signed char)0x6f, 
   (signed char)0x70, (signed char)0x71, (signed char)0x72, (signed char)0x73, (signed char)0x74, (signed char)0x75, (signed char)0x76, (signed char)0x77, 
   (signed char)0x78, (signed char)0x79, (signed char)0x7a, (signed char)0x7b, (signed char)0x7c, (signed char)0x7d, (signed char)0x7e, (signed char)0x7f, 
   (signed char)0x80, (signed char)0x81, (signed char)0x82, (signed char)0x83, (signed char)0x84, (signed char)0x85, (signed char)0x86, (signed char)0x87, 
   (signed char)0x88, (signed char)0x89, (signed char)0x8a, (signed char)0x8b, (signed char)0x8c, (signed char)0x8d, (signed char)0x8e, (signed char)0x8f, 
   (signed char)0x90, (signed char)0x91, (signed char)0x92, (signed char)0x93, (signed char)0x94, (signed char)0x95, (signed char)0x96, (signed char)0x97, 
   (signed char)0x98, (signed char)0x99, (signed char)0x9a, (signed char)0x9b, (signed char)0x9c, (signed char)0x9d, (signed char)0x9e, (signed char)0x9f, 
   (signed char)0xa0, (signed char)0xa1, (signed char)0xa2, (signed char)0xa3, (signed char)0xa4, (signed char)0xa5, (signed char)0xa6, (signed char)0xa7, 
   (signed char)0xa8, (signed char)0xa9, (signed char)0xaa, (signed char)0xab, (signed char)0xac, (signed char)0xad, (signed char)0xae, (signed char)0xaf, 
   (signed char)0xb0, (signed char)0xb1, (signed char)0xb2, (signed char)0xb3, (signed char)0xb4, (signed char)0xb5, (signed char)0xb6, (signed char)0xb7, 
   (signed char)0xb8, (signed char)0xb9, (signed char)0xba, (signed char)0xbb, (signed char)0xbc, (signed char)0xbd, (signed char)0xbe, (signed char)0xbf, 
   (signed char)0xc0, (signed char)0xc1, (signed char)0xc2, (signed char)0xc3, (signed char)0xc4, (signed char)0xc5, (signed char)0xc6, (signed char)0xc7, 
   (signed char)0xc8, (signed char)0xc9, (signed char)0xca, (signed char)0xcb, (signed char)0xcc, (signed char)0xcd, (signed char)0xce, (signed char)0xcf, 
   (signed char)0xd0, (signed char)0xd1, (signed char)0xd2, (signed char)0xd3, (signed char)0xd4, (signed char)0xd5, (signed char)0xd6, (signed char)0xd7, 
   (signed char)0xd8, (signed char)0xd9, (signed char)0xda, (signed char)0xdb, (signed char)0xdc, (signed char)0xdd, (signed char)0xde, (signed char)0xdf, 
   (signed char)0xe0, (signed char)0xe1, (signed char)0xe2, (signed char)0xe3, (signed char)0xe4, (signed char)0xe5, (signed char)0xe6, (signed char)0xe7, 
   (signed char)0xe8, (signed char)0xe9, (signed char)0xea, (signed char)0xeb, (signed char)0xec, (signed char)0xed, (signed char)0xee, (signed char)0xef, 
   (signed char)0xf0, (signed char)0xf1, (signed char)0xf2, (signed char)0xf3, (signed char)0xf4, (signed char)0xf5, (signed char)0xf6, (signed char)0xf7, 
   (signed char)0xf8, (signed char)0xf9, (signed char)0xfa, (signed char)0xfb, (signed char)0xfc, (signed char)0xfd, (signed char)0xfe, (signed char)0xff
   };
 
  char buff[3] = "";
  int val = 0;

  if(!src || !dest){
    return -1;
  }

  while(*src && destsize){
    switch(*src){
     case '%':
        buff[0] = src[1];
        buff[1] = src[2];
        buff[2] = '\0';
        val = strtol(buff, NULL, 16);
        if(val >= 0 && val < 256){
          //fprintf(stderr, " %s() val=%d\n", __func__, val);
          dest[rc] = lookup[val];
          destsize--;
          rc++;
        }
        src += 3;
        break;
	 default:
		dest[rc] = *src;
		src++;
        destsize--;
        rc++;
	    break;
    }
  }

  dest[rc] = '\0';
  
  return rc;
}

int
arc_sip_msg_get_useragent(osip_message_t *msg, char *dest){

  int rc = 0;
  int count = 0;
  osip_header_t *hdr = NULL;
  char buff[1024] = "";

  if(!msg || !dest){
    return -1;
  }

  osip_message_header_get_byname(msg, "User-Agent", count, &hdr);

  // place the text of the header into buff and parse and decode encoding...
  if(hdr && hdr->hvalue){
     snprintf(buff, sizeof(buff), "%s", hdr->hvalue);
  } else {
    return -1;
  }

  rc = strlen(buff);
  return rc;

}




int 
arc_sip_msg_get_usertouser(osip_message_t *msg, char *dest, size_t size, int decode){

  int rc = 0;
  int count = 0;
  osip_header_t *hdr = NULL;
  char buff[1024] = "";
  int encoding = ARC_SIP_MSG_ENCODE_NONE;
  char *ptr;
  int len;

  if(!msg || !dest){
    return -1;
  }

  osip_message_header_get_byname(msg, "User-to-user", count, &hdr);
  // place the text of the header into buff and parse and decode encoding...
  if(hdr && hdr->hvalue){
     snprintf(buff, sizeof(buff), "%s", hdr->hvalue);
     //fprintf(stdout, "DDN_DEBUG: %s() returned %s\n", __func__, buff);
  } else {
    //fprintf(stdout, "DDN_DEBUG: %s() returned nothing\n", __func__);
    return -1;
  }

  // parse encoding 

  ptr = strstr(buff, "encoding=");

  if(ptr != NULL){
    *ptr = '\0';
    ptr += strlen("encoding=");
    if(strstr(ptr, "hex")){
       encoding = ARC_SIP_MSG_ENCODE_HEX;
    } else 
    if(strstr(ptr, "text")){
       encoding = ARC_SIP_MSG_ENCODE_TEXT;
    }
  }

  switch(encoding){
    case ARC_SIP_MSG_ENCODE_NONE:
         //fprintf(stderr, " %s() case ARC_SIP_MSG_ENCODE_NONE\n", __func__);
         snprintf(dest, sizeof(buff), "%s", buff);
		 rc = strlen(buff);
         break;
    case ARC_SIP_MSG_ENCODE_TEXT:
         //fprintf(stderr, " %s() case ARC_SIP_MSG_ENCODE_TEXT\n", __func__);
         len = arc_decode_hex_escapes(buff, dest, size);
		 rc = len;
         break;
    case ARC_SIP_MSG_ENCODE_HEX: 
         //fprintf(stderr, " %s() case ARC_SIP_MSG_ENCODE_HEX \n", __func__);
         len = arc_decode_hex_str(buff, dest, size);
         rc = len;
         break;
    default:
//         fprintf(stderr, " %s() hit default case in handling encoding\n", __func__);
		 break;
  }

  //fprintf(stderr, "DDN_DEBUG: %s() returned %s len %d\n", __func__, buff, rc);
  return rc;
}

int 
arc_sip_msg_set_usertouser(osip_message_t *msg, char *str, int encoding){

   int rc = -1;
   char buff[1024];

   if(!msg || !str){
      return 0;
   }

   switch(encoding){
    case ARC_SIP_MSG_ENCODE_NONE:
         snprintf(buff, sizeof(buff), "%s", str);
         break;
    case ARC_SIP_MSG_ENCODE_TEXT:  
         break;
    case ARC_SIP_MSG_ENCODE_HEX: 
         break;
   }

   // osip_message_header_set_byname("User-to-user", buff);

   return rc;
}


#ifdef MAIN 

#include <stdlib.h>

int main(int argc, char **argv){

  FILE *infile = NULL;
  char in_buff[1024];
  char out_buff[1024];

  if(argc > 1 && argv[1]){
   infile = fopen(argv[1], "r");
  }

  if(!infile){
    fprintf(stderr, " %s() could not open %s as infile exiting...\n", __func__, argv[0]);
    exit(1);
  }

  while(fgets(in_buff, sizeof(in_buff) / 2, infile)){
	  in_buff[strlen(in_buff)-1] = '\0';
      fprintf(stderr, " %s() line read in_buff=[%s]\n", __func__, in_buff);
      arc_encode_hex_str(in_buff, out_buff, sizeof(out_buff));
      fprintf(stderr, "   hex encode in_buff=[%s], out_buff[%s]\n", in_buff, out_buff);
      memset(in_buff, 0, sizeof(in_buff));
      arc_decode_hex_str(out_buff, in_buff, sizeof(in_buff));
      fprintf(stderr, "    hex decode in_buff=[%s], out_buff[%s]\n",in_buff, out_buff);
      arc_encode_hex_escapes(in_buff, out_buff, sizeof(out_buff));
      fprintf(stderr, "     hex escapes encode in_buff=[%s], out_buff[%s]\n", in_buff, out_buff);
      arc_decode_hex_escapes(in_buff, out_buff, sizeof(out_buff));
      fprintf(stderr, "      hex escapes decode in_buff=[%s], out_buff[%s]\n", in_buff, out_buff);
  }

  return 0;

}

#endif 




