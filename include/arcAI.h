/* ----------------------------------------------------------------------
Purpose:	Common includes, defines, globals, and prototypes
--------------------------------------------------------------------------*/  
#ifndef ARC_AI_H
#define ARC_AI_H

#define AI_ADD_CONTENT				20
#define AI_ADD_CONTENT_AND_PROCESS	21
#define AI_PROCESS_LOADED			22
#define AI_CLEAR_CONTENT			23

int TEL_AIInit();
int TEL_AIProcessContent(int aiOpcode, char *content, char *intent);
int TEL_AIExit();

#endif
