#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


#include "arc_sdp.h"

int
arc_sdp_init (struct arc_sdp_t *sdp)
{

  if (sdp != NULL) {
    memset (sdp, 0, sizeof (struct arc_sdp_t));
  }
  return 0;
}

int
arc_sdp_push (struct arc_sdp_t * sdp, char *fmt, ...)
{

  char buff[128];
  va_list ap;
  int rc;

  if (!sdp) {
    return -1;
  }

  if (sdp->count >= ARC_SDP_MAX_ENTRIES) {
    return -1;
  }

  va_start (ap, fmt);
  rc = vsnprintf (buff, sizeof (buff), fmt, ap);
  va_end (ap);

  sdp->sdp[sdp->count] = strdup (buff);
  sdp->count++;

  return rc;

}

int
arc_sdp_finalize (struct arc_sdp_t * sdp, char *dest, int destsize)
{

  int rc = 0;
  int i;

  if (!dest) {
    return -1;
  }

  if (!sdp) {
    return -1;
  }

  dest[0] = '\0';

  for (i = 0; i < sdp->count; i++) {
    if (sdp->sdp[i] != NULL) {
      if (rc < destsize) {
        strncat(&dest[rc], sdp->sdp[i], destsize - rc);
        rc += strlen (sdp->sdp[i]);
      }
      free (sdp->sdp[i]);
    }
  }

  return rc;
}


#ifdef MAIN


int
main ()
{

  int i;
  struct arc_sdp_t sdp;
  char buff[2048];
  int rc;

  arc_sdp_init (&sdp);

  for (i = 0; i < ARC_SDP_MAX_ENTRIES - 1; i++) {
    arc_sdp_push (&sdp, "test=value %d\r\n", i);
  }

  rc = arc_sdp_finalize (&sdp, buff, sizeof (buff));
//  fprintf (stderr, "rc=%d strlen=%d buf=\n%s\n", rc, strlen(buff), buff);

  return 0;
}


#endif
