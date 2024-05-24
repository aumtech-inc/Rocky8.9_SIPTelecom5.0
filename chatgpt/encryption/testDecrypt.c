#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "arcChatGPTEncrypt.h"

extern char *arcDecryptChatGPT_api_key(char *zEncodedStr);

int main(int argc, char *argv[])
{
	char	theString[256];
	char	*chatGPT_api_key;
	int		rc;
	char	encoded[256] = "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";

	chatGPT_api_key = (char *)arcDecryptChatGPT_api_key(encoded);

	printf("(%s) = arcDecryptChatGPT_api_key()\n", chatGPT_api_key);
	
}
