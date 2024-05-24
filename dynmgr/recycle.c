#include <stdio.h>
#include <unistd.h>

#include "dynVarLog.h"
#include "jsprintf.h"

#define JS_DEBUG 0

int
arc_recycle_down_file (char *progname, int id)
{

  FILE *outfile;
  char buff[128];
  int rc = -1;

  snprintf (buff, sizeof (buff), "%s.%d.recycle", progname, id);

  outfile = fopen (buff, "w+");

  if (outfile) {
    fprintf (outfile, "%d\n", getpid ());
    fclose (outfile);
    rc = 0;
  }

  JSPRINTF(" %s() id=%d returning %d\n", __func__, id, rc);
  return rc;
}

int
arc_recycle_up_file (char *progname, int id)
{

  FILE *outfile;
  char buff[128];
  int rc;

  // we are coming up delete our recycle file 

  snprintf (buff, sizeof (buff), "%s.%d.recycle", progname, id);

  if (access (buff, F_OK) == 0) {
    rc = unlink (buff);
    if (rc == 0) {
      JSPRINTF(" %s() unlinked old recycle file\n", __func__);
    }
    else {
      JSPRINTF(" %s() unable to unlink recycle file\n", __func__);
    }
  }

  snprintf (buff, sizeof (buff), "%s.%d.run", progname, id);

  outfile = fopen (buff, "w+");

  if (outfile) {
    fprintf (outfile, "%d\n", getpid ());
    fclose (outfile);
  }
  
  JSPRINTF(" %s() id=%d returning %d\n", __func__, id, rc);
  return rc;
}


int
arc_can_recycle (char *progname, int id)
{

  int i;
  char buff[128];
  int rc = 1;

  for (i = 0; i < 255; i++) {

    if(i == id){
     // skip myself 
     continue;
    }

    snprintf (buff, sizeof (buff), "%s.%d.recycle", progname, i);
    if (access (buff, F_OK) == 0) {
      JSPRINTF(" %s() found [%s] file cannot recycle at this moment\n", __func__, buff);
      rc =  0;
      break;
    }

  }

  JSPRINTF(" %s() returning %d progname=%s id=%d\n", __func__, rc, progname, id);
  return rc;
}

