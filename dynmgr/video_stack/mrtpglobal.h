#ifndef __MRTP_GLOBAL_H_
#define __MRTP_GLOBAL_H_
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// reverse bytes in array
const unsigned char* Mmemrse(unsigned char* dest, const unsigned char *src, int n)
{
	unsigned char *d=dest;

	//_ASSERTE(src!=NULL);
	//_ASSERTE(dest!=NULL);

	if ((!src) || (!dest)) 
	{
		return NULL;
	}
	for(int i=n-1; i >=0; i--) 
	{
		//cout<<"RGDEBUG::src["<<i<<"]="<<src[i]<<endl;
		*dest++ = src[i];
	}
	return d;
}

#endif
