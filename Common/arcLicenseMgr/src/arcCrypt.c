/*
Tue Jun  3 11:56:17 EDT 2002 ddn created the file
*/

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

int arcEncrypt(char *iString, char *oString);
int arcDecrypt(char *iString, char *oString);

/*
** Static Function Prototypes
*/
static int decrypt1(char *iEncryptedString, char *oDecryptedString);
static int decrypt2(char *iString, char *oString);
static int decrypt3(char *iString, char *oString);
static int encrypt1(char *iDecryptedString, char *oEncryptedString);
static int encrypt2(char *iString, char *oString);
static int encrypt3(char *iString, char *oString);

int arcDecrypt(char *iString, char *oString)
{
	int zLevel;

	int yiLen = strlen(iString);

	char chLevel; 

	char yStrTempOut[200];
	char yStrTempOut1[200];
	char yStrTempOut2[200];
	char yStrTempOut3[200];

	if(!iString || !(*iString))
	{
		oString[0] = 0;
		return -1;
	}

	chLevel = iString[yiLen - 1];

	zLevel = chLevel - 48;

	iString[yiLen - 1] = '\0';

	sprintf(yStrTempOut, "%s", iString);

	if(zLevel <= 0)
	{
		oString[0] = 0;
		return -1;
	}

	if(zLevel >= 5)
	{
		oString[0] = 0;
		return -1;
	}


	switch (zLevel)
	{
		case 1:
			decrypt1(yStrTempOut, yStrTempOut1);
			sprintf(yStrTempOut, "%s", yStrTempOut1);
			decrypt2(yStrTempOut, yStrTempOut1);
			sprintf(yStrTempOut, "%s", yStrTempOut1);
			decrypt3(yStrTempOut1, oString);
			break;
		case 2:
			decrypt1(yStrTempOut, yStrTempOut1);
			sprintf(yStrTempOut, "%s", yStrTempOut1);
			decrypt3(yStrTempOut, yStrTempOut1);
			decrypt2(yStrTempOut1, oString);
			break;
		case 3:
			decrypt1(yStrTempOut, yStrTempOut1);
			decrypt3(yStrTempOut1, oString);
			break;
		case 4:
			decrypt1(yStrTempOut, oString);
			sprintf(yStrTempOut, "%s", oString);
				
	}

	return (0);	

} // END: int arcDecrypt(char *iString, char *oString)

int arcEncrypt(char *iString, char *oString)
{
	int zLevel;

	char yStrTempOut[200];
	char yStrTempOut1[200];
	char yStrTempOut2[200];
	char yStrTempOut3[200];

	time_t xT;

	if(!iString ||!(*iString) )
	{
		oString[0] = 0;
		return -1;
	}

	sprintf(yStrTempOut, "%s", iString);

	zLevel = 1 + (int) (time(&xT) % 4);

	//printf("Level: <%d>\n", zLevel);

	if(zLevel <= 0)
	{
		oString[0] = 0;
		return -1;
	}

	if(zLevel >= 5)
	{
		oString[0] = 0;
		return -1;
	}

	switch (zLevel)
	{
		case 1:
				encrypt3(yStrTempOut, yStrTempOut1);
				sprintf(yStrTempOut, "%s", yStrTempOut1);
				encrypt2(yStrTempOut, yStrTempOut1);
				sprintf(yStrTempOut, "%s", yStrTempOut1);
				encrypt1(yStrTempOut, yStrTempOut1);
				sprintf(yStrTempOut, "%s", yStrTempOut1);
				break;
		case 2:
				encrypt2(yStrTempOut, yStrTempOut1);
				sprintf(yStrTempOut, "%s", yStrTempOut1);
		case 3:
				encrypt3(yStrTempOut, yStrTempOut1);
				sprintf(yStrTempOut, "%s", yStrTempOut1);
		case 4:
				encrypt1(yStrTempOut, yStrTempOut1);
				sprintf(yStrTempOut, "%s", yStrTempOut1);
	}
	
	sprintf(oString, "%s%d", yStrTempOut, zLevel);	

	return(0);
	
} // END: int arcEncrypt(char *iString, char *oString)

static int decrypt1(char *iEncryptedString, char *oDecryptedString)
{
	int			i, j = 0, val1, val2;
	char		tmpBuf[10], tmpBuf2[10];
	char		encryptBuf[80], charBuf[10];

	//printf("INPUT D1 <%s>\n", iEncryptedString);
	memset(encryptBuf, 0, sizeof(encryptBuf));

	if((int)strlen(iEncryptedString) == 0)
	{
		oDecryptedString[0] = '\0';
		return(0);
	}

	for(i=0; i<(int)strlen(iEncryptedString); i+=3)
	{
		memset(charBuf, 0, sizeof(charBuf));
		strncat(charBuf, &iEncryptedString[i], 3);

		memset(tmpBuf2, 0, sizeof(tmpBuf2));
		sprintf(tmpBuf2, "%c", atoi(charBuf));

		strcat(encryptBuf, tmpBuf2);
	}

	val1 = encryptBuf[0];
	sprintf(oDecryptedString, "%c", (val1 - 7));

	i = 0;
	j = 0;
	while(i <=  (int)(strlen(encryptBuf) - 2))
	{
		if(encryptBuf[i] == '`')
		{
			i ++;
		}
		else
		{
			val1 = oDecryptedString[j];
			if(encryptBuf[i + 1] == '`')
			{
				val2 = encryptBuf[i + 2];
				sprintf(tmpBuf, "%c",
					((val1 - val2) + (j + 1)));
			}
			else
			{
				val2 = encryptBuf[i + 1];
				sprintf(tmpBuf, "%c",((val1 + val2) -(j + 1)));				
			}
			strcat(oDecryptedString, tmpBuf); 

			j ++;
			i ++;
		}
	} /* while */

	//printf("OUTPUT D1 <%s>\n", oDecryptedString);
	return(0);

} /* END: decrypt1() */

static int decrypt2(char *iString, char *oString)
{
		
	int i, j;
	int yLen = 0;

	//printf("INPUT D2 <%s>\n", iString);
	yLen = strlen(iString);
	for(i = yLen - 1, j=0; i >= 0; i--, j++)
	{
		oString[j] = iString[i];
	} 

	oString[j] = '\0';
	//printf("OUTPUT D2 <%s>\n", oString);
	return 0;

}/*END: static int decrypt2(char *iString, char *oString) */

static int decrypt3(char *iString, char *oString)
{
	int yLen = strlen(iString);

	int i = 0, j = 0;
	//printf("INPUT D3 <%s>\n", iString);

	while(1)
	{
		if(i == yLen - 1)
		{
			oString[j++] = iString[i];
			break;
		}

		oString[j++] = iString[i + 1];
		oString[j++] = iString[i];

		if(i == yLen - 2)
			break;

		i +=2;
	}

	oString[j] = '\0';

	//printf("OUTPUT D3 <%s>\n", oString);
	return 0;

}/*END: static int decrypt3(char *iString, char *oString)*/

static int encrypt1(char *iDecryptedString, char *oEncryptedString)
{
	int		Asc_1, Asc_2, i;
	char		tmpBuf[10];
	char		eBuf[80], charBuf[10];
	//printf("INPUT E1 <%s>\n", iDecryptedString);

	Asc_1 = iDecryptedString[0];

	memset(eBuf, 0, sizeof(eBuf));

	sprintf(eBuf, "%c", (iDecryptedString[0] + 7));

	for(i = 0; i < (int)(strlen(iDecryptedString) - 1); i++)
	{
		Asc_1 = iDecryptedString[i];
		Asc_2 = iDecryptedString[i+1];
		
		if(Asc_1 > Asc_2)
		{
			strcat(eBuf, "`");
			
			sprintf(tmpBuf, "%c", ((Asc_1 - Asc_2) + (i + 1)));
			strcat(eBuf, tmpBuf);
		}
		else
		{
			sprintf(tmpBuf, "%c", ((Asc_2 - Asc_1) + (i + 1)));
			strcat(eBuf, tmpBuf);
		}
	} /* for */

	sprintf(oEncryptedString, "%s", "\0");

	for(i=0; i<(int)strlen(eBuf); i++)
	{
		memset(charBuf, 0, sizeof(charBuf));
		sprintf(charBuf, "%.3d", eBuf[i]);
		strcat(oEncryptedString, charBuf);
	}
	//printf("OUTPUT E1 <%s>\n", oEncryptedString);

	return(0);

} /* END: encrypt1() */



static int encrypt2(char *iString, char *oString)
{
	int i, j;

	int yLen = 0;

	sprintf(oString, "%s", "");

	//printf("INPUT E2 <%s>\n", iString);

	yLen = strlen(iString);

	for(i = yLen - 1, j=0; i >= 0; i--, j++)
	{
		oString[j] = iString[i];
	} 
	
	oString[j] = '\0';

	//printf("OUTPUT E2 <%s>\n", oString);

	return 0;

}/*END: static int encrypt2(char *iString, char *oString) */

static int encrypt3(char *iString, char *oString)
{
		
	int yLen = strlen(iString);

	int i = 0, j = 0;

	sprintf(oString, "%s", "");

	//printf("INPUT E3 <%s>\n", iString);

	while(1)
	{

		if(i == yLen - 1)
		{
			oString[j++] = iString[i];
			break;
		}

		oString[j++] = iString[i + 1];
		oString[j++] = iString[i];

		if(i == yLen - 2)
			break;

		i +=2;
	}

	oString[j] = '\0';

	//printf("OUTPUT E3 <%s>\n", oString);

	return (0);

}/*END: static int encrypt3(char *iString, char *oString) */

#if 0

int main()
{

	char x[100] = "1234567890";
	char y[200] = "";
	char z[200] = "";


	arcEncrypt(x, y);

//printf("%s\n", y);
	arcDecrypt(y, z);


//printf("%s\n", z);
	return 0;
}

#endif


