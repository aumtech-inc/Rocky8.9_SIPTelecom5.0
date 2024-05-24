#include <stdio.h>

/* 
   this is the dtmf tab from the platform 
   as it was originally, the format hasn't changed 
*/

const int dtmf_tab[17] =
  { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '#', 'A', 'B', 'C',
'D', '\0' };

/*
  some rutines to be used from where ever DTMF is detected 
  callmanager, media manager, etc ... 
*/


/*
  takes a string and returns a value between 0-15 
  could be modified to takes a string as a decimal 
  and return that nnumber but I don't there is a case 
  for that yet,
*/

int
arc_dtmf_char_to_int (char *ch)
{

  int val = -1;

  if (!ch || !ch[0]) {
    return val;
  }

  switch (ch[0]) {
  case '0':
    val = 0;
    break;
  case '1':
    val = 1;
    break;
  case '2':
    val = 2;
    break;
  case '3':
    val = 3;
    break;
  case '4':
    val = 4;
    break;
  case '5':
    val = 5;
    break;
  case '6':
    val = 6;
    break;
  case '7':
    val = 7;
    break;
  case '8':
    val = 8;
    break;
  case '9':
    val = 9;
    break;
  case '*':
    val = 10;
    break;
  case '#':
    val = 11;
    break;
  case 'A':
  case 'a':
    val = 12;
    break;
  case 'B':
  case 'b':
    val = 13;
    break;
  case 'C':
  case 'c':
    val = 14;
    break;
  case 'D':
  case 'd':
    val = 15;
    break;
  };

  return val;
}

const char *
arc_dtmf_int_to_str (int val)
{

  const char *rc = NULL;
  static const char *lookup_tab[] = {
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "*",
    "#",
    "A",
    "B",
    "C",
    "D"
  };

  if (val >= 0 && val <= 15) {
    rc = lookup_tab[val];
  }

  return rc;
}


#ifdef MAIN


#include <dtmf_utils.h>

int
main ()
{

  char *test[] = {
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "*",
    "#",
    "a",
    "A",
    "b",
    "B",
    "c",
    "C",
    "d",
    "D",
    "R",
    "r",
    "X",
    "x",
    "y",
    "z",
    NULL
  };

  char test2[] = "0123456789aAbBcCdDrRxXyYzX";

  int i;

  for (i = 0; test[i]; i++) {
    fprintf (stderr, "i=%d in=%s out=%d\n", i, test[i],
             arc_dtmf_char_to_int (test[i]));
    fprintf (stderr, "i=%d in=%d out=%s\n", i, arc_dtmf_char_to_int (test[i]),
             arc_dtmf_int_to_str (i));
  }

  for(i = 0; test2[i]; i++){
    fprintf (stderr, "i=%d in=%c out=%d\n", i, test2[i],
             arc_dtmf_char_to_int (&test2[i]));
    fprintf (stderr, "i=%d in=%d out=%s\n", i, arc_dtmf_char_to_int (&test2[i]),
             arc_dtmf_int_to_str (i));
  }

  return 0;
}

#endif
