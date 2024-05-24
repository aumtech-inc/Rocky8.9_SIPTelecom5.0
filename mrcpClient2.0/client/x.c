#include <stdio.h>

////////////////////////////////
int main(int argc, char* argv[])
////////////////////////////////
{

#ifdef EXOSIP_4
    fprintf(stderr, "exosip\n");
#else
    fprintf(stderr, "not exosip\n");
#endif // EXOSIP_4

}
