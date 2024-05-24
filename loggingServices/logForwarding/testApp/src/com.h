
#ifndef TEST_COM
#define TEST_COM

#ifdef DEFINED_ROUTINE

char	gProgramName[64];
int		GV_tfd;
pid_t	GV_ClientPid;

#else

extern char		gProgramName[];
extern int		GV_tfd;
extern pid_t	GV_ClientPid;

#endif

#endif
