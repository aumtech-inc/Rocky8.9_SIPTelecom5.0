#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "arcChatGPTEncrypt.h"

static int encryptIt(char *inStr, char *encryptedStr);

int main(int argc, char *argv[])
{ 
	char	*ispbase;
	char	cfg[128];
	char	tmpCfg[128];
	FILE	*fp;
	FILE	*tmpFp;
	int		found;
	char	line[256];
	int		len;
	char	left[64];
	char	right[128];
	int		rc;
	char	encryptedStr[256];

	if((ispbase = (char *)getenv("ISPBASE")) == NULL)
	{
		fprintf(stderr, "Environment variable ISPBASE is not set.  Unable to encrypt ChatGPT_api_key.\n"
		                "Set ISPBASE and retry.\n");
		exit(-1);
	}

	sprintf(cfg, "%s/Telecom/Tables/.TEL.cfg", ispbase);
	sprintf(tmpCfg, "%s", "/tmp/.TEL.cfg");

	if ( (fp = fopen(cfg, "r")) == NULL )
	{
		fprintf(stderr, "Failed to open (%s) for reading. [%d, %s]  Unable to encrypt ChatGPT_api_key\n",
			cfg, errno, strerror(errno));
		return(-1);
	}

	if ( (tmpFp = fopen(tmpCfg, "w")) == NULL )
	{
		fprintf(stderr, "Failed to open (%s) for writing. [%d, %s]  Unable to encrypt ChatGPT_api_key\n",
			tmpCfg, errno, strerror(errno));
		return(-1);
	}

	found = 0;
	memset((char *)line, '\0', sizeof(line));
	memset((char *)left, '\0', sizeof(left));
	memset((char *)right, '\0', sizeof(right));
	len = strlen(CHAT_API_KEY);
	while (fgets (line, sizeof (line) - 1, fp))
	{
		if ( ! found ) 
		{
//			fprintf(stdout, "[%d] %s", __LINE__, line);
			if ( strncmp(line, CHAT_API_KEY, len) == 0 )
			{
				found = 1;
				if ( (rc = sscanf (line, "%[^=]=%[^=]", left, right)) < 2 )
				{
					fprintf(stderr, "Invalid line for ChatGPT_api_key in %s.  Must be %s=<valid key>.  Correct and retry.\n",
						cfg, CHAT_API_KEY);
					fclose(fp);
					fclose(tmpFp);
					unlink(tmpCfg);
					return(-1);
				}
//				printf("Calling encryptIt(%s)\n", right);
				rc = encryptIt(right, encryptedStr);
//				printf("right (%s)\n", right);
//				printf("encryptedStr (%s)\n", encryptedStr);
				fprintf(tmpFp, "%s=%s\n", CHAT_API_KEY, encryptedStr);
				continue;
			}
			else
			{
				fprintf(tmpFp, "%s", line);
				memset((char *)line, '\0', sizeof(line));
				continue;
			}
		}	
		fprintf(tmpFp, "%s", line);
		memset((char *)line, '\0', sizeof(line));
	}
	fclose(fp);
	fclose(tmpFp);

	//	Now rename temp TEL.cfg to the real TEL.cfg
	if (found)
	{
		if ( (fp = fopen(cfg, "w")) == NULL )
		{
			fprintf(stderr, "Failed to open (%s) for writing. [%d, %s]  Unable to encrypt ChatGPT_api_key\n",
				cfg, errno, strerror(errno));
			return(-1);
		}
	
		if ( (tmpFp = fopen(tmpCfg, "r")) == NULL )
		{
			fprintf(stderr, "Failed to open (%s) for reading. [%d, %s]  Unable to encrypt ChatGPT_api_key\n",
				tmpCfg, errno, strerror(errno));
			return(-1);
		}

		memset((char *)line, '\0', sizeof(line));
		while (fgets (line, sizeof (line) - 1, tmpFp))
		{
			fprintf(fp, "%s", line);
		}
		fclose(fp);
		fclose(tmpFp);
		printf("Encryption of %s in %s successfully completed.\n",  CHAT_API_KEY, cfg);
		unlink(tmpCfg);
	}
	else
	{
		fprintf(stderr, "ERROR: The (%s) field does not exist %s. Unable to encrypt. Correct and retry.\n",
			CHAT_API_KEY, cfg);
	}


	exit(0);
} // END: main()

/*---------------------------------------------------------------------------------
  ---------------------------------------------------------------------------------*/
static int encryptIt(char *inStr, char *encryptedStr)
{
// #define ENCRYPTION_KEY_MODIFIER			-9
	char	in[128];
	char	out[128];
	int		i;
	
	memset((char *)out, '\0', sizeof(out));
	sprintf(in, "%s", inStr);
	i = 0;
	while ( in[i] != '\n' && in[i] != '\0' )
	{
		out[i] = in[i] + ENCRYPTION_KEY_MODIFIER;
		i++;

	}
	sprintf(encryptedStr, "%s", out);

	return(0);
} // END: encryptIt()
