#ifndef DTMF_UTILS_DOT_H
#define DTMF_UTILS_DOT_H

/* 
   this is the dtmf tab from the platform 
   as it was originally, the format hasn't changed 
*/

extern const int dtmf_tab[17];

/*
  takes a string and returns a value between 0-15 
  could be modified to takes a string as a decimal 
  and return that nnumber but I don't there is a case 
  for that yet,
*/

int arc_dtmf_char_to_int (char *ch);

/*
  converts it back to a string, we use the uppercase A B C D ... 
*/

const char *arc_dtmf_int_to_str (int val);

#endif
