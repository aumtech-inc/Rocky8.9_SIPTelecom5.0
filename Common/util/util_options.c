#ident "@(#)util_options 96/01/01 2.2.0 Copyright 1996 Aumtech, Inc."
/*---------------------------------------------------------------------
Program:	util_options
Purpose:
-
- This function will take a set of command line arguments and
- break it up into matching switch flags and arguments.  Passed
- to the function should be argc, argv and the name of the
- funtion to call with the results.

- The function called will be passed the following information:
-
- function_prototype(int c, char *string)
-
- int c = the flag being passed.
-
- char string = the string which follows the flag if it does not
-               start with '-'.
-
- An example of command line arguments might be:
-
- prog -q -r -p testpgm -6
-
- The function called by util_options will be called four times
- in this case.  Each time called it will be passed the following:
-
-    function_prototype(q," ");
-
-    function_prototype(r," ");
-
-    function_prototype(p,"testpgm");
-
-    function_prototype(6," ");
-
-
- ------------------------------
-
- A sample program is as follows:
-
- 
- #include <stdio.h>
- 
- int eric_switch();
- 
- main(int argc, char *argv[]){
- 
- 
-   util_options(argc,argv,eric_switch);
- 
- }
- 
- 
- int eric_switch(int c, char *token){
- 
-   switch(c){
- 
-     case 'A':
- 	printf("A was passed\n");
-     break;
- 
-     case 'e':
- 	printf("the token is %s\n",token);
-     break;
- 
- 
-     default:
- 	printf("%c is unknown, so who cares\n",c);
-     break;
- 
-   }
- 
- }
-
-----------------------------------------------------------------*/
#include <stdio.h>


int util_options(int argc, char *argv[], char *(*PT_Function)()){

  int c;

  while( --argc > 0 ){

    *++argv;

    if(argv[1][0] != '-'){
        PT_Function((*argv)[1], argv[1]);
        *++argv;
  	--argc;
    }
    else{
	PT_Function((*argv)[1]," ");
    }

  }

  return (1);

}
