
static int parseAnswerCallMessageResponse(char *responseMessage)
{
	char stringToFind[] = {"BANDWIDTH="};
	char    *zQuery;
	char    lhs[32];
	char    rhs[32];
	char    buf[MAX_APP_MESSAGE_DATA];	  
	int 	  index;
    char tmpVal[100];
	
	zQuery = (char*)strtok_r((char *)responseMessage, ",", (char**)&buf);

	strcat(zQuery, "^");

	for(index = 0; index < strlen(zQuery); index++)
	{
		sprintf(tmpVal, "%s", 0);
		if((strstr(zQuery + index, "AUDIO_CODEC") == zQuery + index )) 
		{
			getCodecValue(
							zQuery + index,
							tmpVal,
							"AUDIO_CODEC",
							'=',
							'^');

			if(!strcmp(tmpVal, "711"))
				sprintf(GV_Audio_Codec, "%d", 3);	
			else if(!strcmp(tmpVal, "711r"))
				sprintf(GV_Audio_Codec, "%d", 1);
			else
				sprintf(GV_Audio_Codec, "%d", 0);
			
			index = index + strlen("AUDIO_CODEC") - 1;
		}
		else
		if(strstr(zQuery + index, "VIDEO_CODEC") == zQuery + index)
		{
			getCodecValue(
								zQuery + index,
								tmpVal,
								"VIDEO_CODEC",
								'=',
								'^');

			if(strstr(tmpVal, "263"))
				sprintf(GV_Video_Codec, "%d", 2);	

		}

	}

#if 0
	while( (tmpBuffer != NULL) && (tmpBuffer[0] != 0) )
	{

		sprintf(lhs, "%s", "\0");
		sprintf(rhs, "%s", "\0");

		sscanf(tmpBuffer, "%[^=]=%s", lhs, rhs);


		if(! strcmp(lhs, "AUDIO_CODEC"))
		{
			if(!strcmp(rhs, "711"))
				sprintf(GV_Audio_Codec, "%d", 1);	
			else if(!strcmp(rhs, "711r"))
				sprintf(GV_Audio_Codec, "%d", 3);
			else
				sprintf(GV_Audio_Codec, "%d", 0);
			
		}
		else if(!strcmp(lhs, "VIDEO_CODEC"))
		{
			if(strstr(rhs, "263"))
				sprintf(GV_Video_Codec, "%d", 2);	
			
		}

		tmpBuffer = (char*)strtok_r((char *) NULL, ",", (char**)&buf);
	
	}
	

	

	if(strstr(responseMessage, stringToFind) != NULL)
	{
		sscanf(responseMessage+10, "%d", &GV_BandWidth);
	}
#endif
	
	return(0);
}



int getCodecValue(
        char    *zStrInput,
        char    *zStrOutput,
        char    *zStrKey,
        char    zChStartDelim,
        char    zChStopDelim)
{
    int         IsSingleLineCommentOn = 0;
    int         IsMultiLineCommentOn  = 0;
    int         yIntLength = 0;
    int         yIntIndex  = 0;
    char        *yStrTempStart;
    char        *yStrTempStop;
    char        yNext;
    char        yPrev;

    /*
     * Initialize vars
     */
    zStrOutput[0] = 0;

    if(!zStrInput || !(*zStrInput))
    {
        return(-1);
    }

    if(!zStrKey || !(*zStrKey))
    {

        return(-1);
    }

    yIntLength = strlen(zStrInput);

    for(yIntIndex = 0; yIntIndex < yIntLength; yIntIndex ++ )
    {
        if(yIntIndex == 0)
            yPrev = zStrInput[yIntIndex];
        else
            yPrev = zStrInput[yIntIndex - 1];

        if(yIntIndex == yIntLength - 1)
            yNext = zStrInput[yIntIndex];
        else
            yNext = zStrInput[yIntIndex + 1];


        if( (   yIntIndex == 0 ||
                yPrev == ' '    ||
                yPrev == '\t'   ||
                yPrev == '\n' )&&
                strstr(zStrInput + yIntIndex, zStrKey) ==
                zStrInput + yIntIndex)
        {
            //yIntIndex += strlen(zStrKey);

            yStrTempStart = strchr(zStrInput + yIntIndex,
                    zChStartDelim) + 1;

            yStrTempStop =  strchr(zStrInput + yIntIndex,
                    zChStopDelim);

            if(!yStrTempStop    ||
                !yStrTempStart  ||
                yStrTempStop <  yStrTempStart)
                {
                    zStrOutput[0] = 0;
                    return(0);
                }

            if( strchr(zStrInput + yIntIndex, '\n') &&
                strchr(zStrInput + yIntIndex, '\n') <
                yStrTempStop)
                {
                    zStrOutput[0] = 0;
                    return(0);
                }

            strncpy(zStrOutput,
                    yStrTempStart,
                    yStrTempStop - yStrTempStart);

            zStrOutput[yStrTempStop - yStrTempStart] = 0;


            return(0);
        }

    }

    return(0);

} /* END: getPlayMediaValue() */
