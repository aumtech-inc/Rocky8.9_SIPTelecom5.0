/*--------------------------------------------------------------------
File:		gaIncludes.h

Purpose:	All header information used in the global accessory 
		routines is included in this file.

Author:		Sandeep Agate

Update:		09/09/97 sja	Created the file
Update:		01/22/98 djb	Added #include <sys/stat.h>
Update:		01/06/00 djb	Added #include <fcntl.h>
Update:		2000/02/17 sja	Added #include <libgen.h>
--------------------------------------------------------------------*/
#ifndef gaIncludes_H	/* Just preventing multiple includes... */
#define gaIncludes_H

/* Include Files */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>

/* Externs */
extern int	errno;

#endif /* gaIncludes_H */
