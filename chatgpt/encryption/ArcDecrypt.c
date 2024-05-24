#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "arcChatGPTEncrypt.h"

char	*gp_decryptedStr;

/*---------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
char *arcDecryptChatGPT_api_key(char *zEncodedStr)
{ 
	char	*ispbase;
	char	cfg[128];
	char	tmpCfg[128];
	FILE	*fp;
	FILE	*tmpFp;
	int		found;
	char	line[256];
	char	*p;
	int		len;
	char	left[64];
	char	right[128];
	int		rc;
	int		i;
	int		modifier;
	char	tmpStr[256] = "";;

	if ( (zEncodedStr[0] == '\0') || ( ! zEncodedStr ) || ( zEncodedStr == (char *)NULL) )
	{
//		fprintf(stderr, "[%s, %d] Failed to allocate memory: [%d, %s] Unable to decode api key.\n",
//			__FILE__, __LINE__, errno, stderr(errno));
	}

	if ((gp_decryptedStr = (char *)calloc(256, sizeof(char))) == NULL )
	{
//		fprintf(stderr, "[%s, %d] Failed to allocate memory: [%d, %s] Unable to decode api key.\n",
//			__FILE__, __LINE__, errno, stderr(errno));
		return(""); 	// null
	}
fprintf(stderr, "[%d] gp_decryptedStr  (%u: %s)\n", __LINE__, gp_decryptedStr, gp_decryptedStr);

	memset((char *)tmpStr, '\0', sizeof(tmpStr));
	if ((gp_decryptedStr = (char *)calloc(256, sizeof(char))) == NULL )
	{
//		fprintf(stderr, "[%s, %d] Failed to allocate memory: [%d, %s] Unable to decode api key.\n",
//			__FILE__, __LINE__, errno, stderr(errno));
		return(gp_decryptedStr); 	// null
	}
		
	modifier = ENCRYPTION_KEY_MODIFIER * -1;

	i = 0;
	p = gp_decryptedStr;
	while ( right[i] != '\n' && right[i] != '\0' )
	{
		tmpStr[i] = right[i] + modifier;
		i++;
		fprintf(stderr, "[%d] tmpStr=(%s)  gp_decryptedStr=(%s)\n", __LINE__, tmpStr, gp_decryptedStr);
	}

	if (found)
	{
		sprintf(gp_decryptedStr, "%s", tmpStr);
		return(gp_decryptedStr);
	}
	else
	{
		gp_decryptedStr = tmpStr;
		return("");
	}

	exit(0);
} // END: arcDecryptChatGPT_api_key()
