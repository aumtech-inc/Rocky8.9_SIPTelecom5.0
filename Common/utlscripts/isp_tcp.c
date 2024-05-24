#include <stdio.h>
#include "ispinc.h"

char *list[500];
int  count;

int my_switch(int c, char *token);

int main(int argc, char *argv[]){

  list[0] = (char *) malloc(ISP_MAXPGMNAME);
  count = 1;

  util_options(argc,argv,my_switch);

  list[count] = 0;

/*
  execv(list[0],list);
*/

  execl("testlogin",
	"-P","testlogin",
	"-y","123",
	"-p","14",
	"-t","TELNET",
	"-H","ashok",
	"-s","1",
	"-h","20",
	(char *) 0);


  exit(1); /* should not get here */

}

int my_switch(int c, char *token){

  switch (c){

    case 'P':
      strcpy(list[0],token);
    break;

  }

  list[count] = (char *) malloc(4);
  sprintf(list[count++],"-%c",c);

  list[count] = (char *) malloc(strlen(token)+3);
  strcpy(list[count++],token);
  

}
