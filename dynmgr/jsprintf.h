#ifndef ARC_PRINTF_DOT_H
#define ARC_PRINTF_DOT_H

#include <stdio.h> 

#ifdef ARC_DEBUG 
#define JSPRINTF(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define JSPRINTF(fmt, ...) 
#endif

#endif
