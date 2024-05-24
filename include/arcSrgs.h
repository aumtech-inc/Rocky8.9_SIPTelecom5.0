/*----------------------------------------------------------------------------
File Name   :   arcSrgs.h
Purpose     :   API definitions to access the srgs library..
Author      :   Aumtech, inc
Date        :   Tue Dec 23 10:20:29 EST 2003
Update      :   Tue Dec 23 10:20:29 EST 2003 ddn created
_____________________________________________________________________________*/

#include<stdio.h>
#include<stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

int arc_srgs_init(char  *logMsg);
int arc_srgs_init_all(char  *logMsg);
int arc_srgs_exit(char  *logMsg);
int arc_srgs_recognize(char *dtmf, char *logMsg);
int arc_srgs_loadGrammar(char *grammarName, char *grammarFile, char *logMsg);
int arc_srgs_unloadGrammar(char *grammarName, char *logMsg);
int arc_srgs_unloadAllGrammars(char *logMsg);
int arc_srgs_setparam(char *name, char * value, char *logMsg);
int arc_srgs_getCriteria(int * len, char *logMsg);

int arc_srgs_getresult(int zPort, int zAlternativeNumber, int zTokenType,
    char *zDelimiter, char *zResult, int *zOverallConfidence, char *zErrMsg);

#ifdef __cplusplus
}
#endif

