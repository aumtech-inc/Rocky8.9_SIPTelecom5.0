#ident	"@(#)TEL_SetFaxTextDefaults 00/05/22 3.1   Copyright 2000 Aumtech"
static char ModuleName[]="TEL_SetFaxTextDefaults";
/*-----------------------------------------------------------------------------
Program:	TEL_SetFaxTextDefaults.c
Purpose:	Set the characteristics of text data to be sent out.
Author: 	djb
Update: 	05/26/99 djb	Created the file.
Update: 07/07/1999 djb	Added validation for margins as well as pagelength.
Update: 07/08/1999 djb	Fixed bug in validateAscii().
Update: 07/09/1999 djb	Log RES_LOW in message rather than a number.
Update: 09/08/1999 gpw  fixed "Unable to open" error message.
Update: 05/02/2000 gpw  made dumpASCII() subject to ifdef DEBUG
Update: 05/02/2000 gpw  Calls fax_log (in FAX_Common.c) to do all logging
Update: 05/02/2000 gpw  Changed TEL_Mnemonics.h to FAX_Mnemonics.h
Update: 08/25/04 djb Changed sys_errlist strerror(errno).
------------------------------------------------------------------------------*/
#include "Telecom.h"
#include "Dialogic.h" 
#include "faxlib.h"
#include "ispinc.h"
#include "FAX_Mnemonics.h"
#include "monitor.h"
#include "arcFAXInternal.h"

/*
 * struct to store the defaults
 */
typedef struct
{
	unsigned short	width;		
	unsigned short	resolution;
	ushort		pagelength;
	ushort		pagepad;
	ushort		topmargin;
	ushort		botmargin;
	ushort		leftmargin;
	ushort		rightmargin;
	ushort		linespace;
	ushort		font;
	ushort		tabstops;
	ushort		units;
	ushort		flags;
} AsciiDefaultStruct;

/*
 * Declare and define text description variables with defaults.  
 * If any of these default values change, be sure to make the corresponding
 * change in gvAsciiDefaultSettings (immediately below).
 */
unsigned short	gvWidth=DF_WID1728;
unsigned short	gvResolution=DF_RESLO;

/*
 * DF_ASCIIDATA is a dialogic structure which does not include width and
 * resolution.  gvAsciiData is the working structure to be passed to 
 * fx_sendfax(); it must be initialized to defaults.
 * If any of these default values change, be sure to make the corresponding
 * change in gvAsciiDefaultSettings (immediately below).
 */
DF_ASCIIDATA    gvAsciiData=
{
	110,		/* pagelength */
	DF_PAD,		/* pagepad */
	2,		/* topmargin */
	2,		/* botmargin */
	3,		/* leftmargin */
	3,		/* rightmargin */
	DF_SINGLESPACE,	/* linespace */
	DF_FONT_0,	/* font */
	8,		/* tabstops */
	DF_UNITS_IN10,	/* units */
	' '		/* reserved */
};

/*
 * Storage for default values;
 */
static AsciiDefaultStruct	gvAsciiDefaultSettings=
{
	DF_WID1728,	/* width */
	DF_RESLO,	/* resolution */
	110,		/* pagelength */
	DF_PAD,		/* pagepad */
	2,		/* topmargin */
	2,		/* botmargin */
	3,		/* leftmargin */
	3,		/* rightmargin */
	DF_SINGLESPACE,	/* linespace */
	DF_FONT_0,	/* font */
	8,		/* tabstops */
	DF_UNITS_IN10,	/* units */
	' '		/* reserved */
};

/*
 * Definitions for the different types of text characteritics
 */
#define	LIST_TERMINATOR	"NULL"
static char	*validNames[] =
{
	"TEXT_WIDTH",
	"TEXT_RESOLUTION",
	"TEXT_PAGELENGTH",
	"TEXT_TOPMARGIN",
	"TEXT_BOTMARGIN",
	"TEXT_LEFTMARGIN",
	"TEXT_RIGHTMARGIN",
	"TEXT_LINESPACING",
	"TEXT_FONT",
	"TEXT_UNITS",
	LIST_TERMINATOR
};

typedef struct 
{
	char	validValue[64];		/* predefined value */
	int	dValue;			/* corresponding Dialogic value */
} ValidValuesStruct;

/*
 * Definitions for valid values of TEXT_WIDTH
 */
ValidValuesStruct widthValues[] =
{ 
	{ "WIDTH_1728",		DF_WID1728 },	/* default */
	{ "WIDTH_2048",		DF_WID2048 },
	{ "WIDTH_2432",		DF_WID2432 },
	{ LIST_TERMINATOR,	0 }
};

/*
 * Definitions for valid values of TEXT_RESOLUTION
 */
ValidValuesStruct resolutionValues[] =
{ 
	{ "RES_LOW",		DF_RESLO },	/* default */
	{ "RES_HIGH",		DF_RESHI },
	{ LIST_TERMINATOR,	0 }
};

/*
 * Definitions for valid values of TEXT_LINESPACING
 */
ValidValuesStruct lineSpacingValues[] =
{ 
	{ "LS_SINGLE",		DF_SINGLESPACE },	/* default */
	{ "LS_DOUBLE",		DF_DOUBLESPACE },
	{ "LS_TRIPLE",		DF_TRIPLESPACE },
	{ "LS_8LPI",		DF_8LPI },
	{ "LS_6LPI",		DF_6LPI },
	{ "LS_4LPI",		DF_4LPI },
	{ "LS_3LPI",		DF_3LPI },
	{ "LS_2_4LPI",		DF_2_4LPI },
	{ LIST_TERMINATOR,	0 }
};

/*
 * Definitions for valid values of TEXT_FONT
 */
ValidValuesStruct fontValues[] =
{ 
	{ "FONT_6LPI",		DF_FONT_0 },	/* default */
	{ "FONT_8LPI",		DF_FONT_3 },
	{ LIST_TERMINATOR,	0 }
};

/*
 * Definitions for valid values of TEXT_UNITS
 */
ValidValuesStruct unitsValues[] =
{ 
	{ "UNITS_ONE_TENTH_INCHES",	DF_UNITS_IN10 },
	{ "UNITS_MM",			DF_UNITS_MM },
	{ "UNITS_SCAN_LINES",		DF_PELS },
	{ LIST_TERMINATOR,	0 }
};

/*
 * Function prototypes
 */
static int validateAscii(char *name, int minimumValue, int maximumValue,
				int newValueToSetItTo, ushort *gvRealValue);
static void setAsciiDefaults();
/*------------------------------------------------------------------------------
TEL_SetFaxTextDefaults()
	This API reads a text description file (textDescFile), and sets the
	internal dialogic parameters which controls the description of 
	ASCII data to be faxed out.  This basically reads a file, parses
	the data, and sets the values.  TEL_SendFax() then uses this when
	faxing out text data.

	If textDescFile is NULL, all values as reset to the default valuess.
------------------------------------------------------------------------------*/
int TEL_SetFaxTextDefaults(char *textDescFile)
{
int		rc;
char		logMsg[512];
char		buf[256];
FILE		*fpTextDesc;
char		name[128];
char		value[128];
int		index;
int		valueInt;
int		minimumValue;
int		maximumValue;
char		sendToMonitorMsg[128];

int		pageLengthHasToBeSet=0;
char		pageLengthName[64];
int		newPageLengthValue=0;

int		topMarginHasToBeSet=0;
char		topMarginName[64];
int		newTopMarginValue=0;

int		bottomMarginHasToBeSet=0;
char		bottomMarginName[64];
int		newBottomMarginValue=0;

int		leftMarginHasToBeSet=0;
char		leftMarginName[64];
int		newLeftMarginValue=0;

int		rightMarginHasToBeSet=0;
char		rightMarginName[64];
int		newRightMarginValue=0;
	
/*
sprintf(sendToMonitorMsg,"TEL_SetFaxTextDefaults(%s)", textDescFile);
rc = send_to_monitor(MON_API, TEL_SENDFAX, sendToMonitorMsg);
*/

/* Check to see if InitTelecom API has been called. */
if(GV_InitCalled != 1)
{
	fax_log(ModuleName,REPORT_NORMAL,FAX_TELECOM_NOT_INIT, 
			FAX_TELECOM_NOT_INIT_MSG);
	HNDL_RETURN (TEL_FAILURE);
}

if (check_disconnect(ModuleName) != 0)
{
	HNDL_RETURN (TEL_DISCONNECTED);
}

if (textDescFile[0]=='\0')
{	/*
	 * Null is passed - set all values back to the defaults
	 */
	setAsciiDefaults();
	HNDL_RETURN (TEL_SUCCESS);
}

if ((fpTextDesc = fopen(textDescFile, "r")) == NULL)
{
	sprintf(logMsg, "Failed to open file <%s> for reading. errno=%d. [%s]",
			textDescFile, errno, strerror(errno));
	fax_log(ModuleName,REPORT_NORMAL,FAX_CANT_OPEN_FILE, logMsg);
	HNDL_RETURN (TEL_FAILURE);
}

memset((char *)buf, 0, sizeof(buf));
while (fgets(buf, sizeof(buf)-1, fpTextDesc) != NULL)
{
	if ( (buf[0] == '#') || buf[0] == ' ')
	{
		continue;
	}
	buf[strlen(buf)-1]='\0';	

	sscanf(buf, "%[^=]=%[^=]", name, value);

	index=0;
	while((strcmp(validNames[index], LIST_TERMINATOR)) != 0)
	{
		if (strcmp(name, validNames[index]) == 0)
		{
			break;
		}
		index++;
	}

	switch(index)
	{
	case 0: 		/* TEXT_WIDTH */
		index=0;
		while((strcmp(widthValues[index].validValue, LIST_TERMINATOR))
								!= 0)
		{
			if (strcmp(value, widthValues[index].validValue)
								 == 0)
			{
				gvWidth=(ushort)widthValues[index].dValue;
				break;
			}
			index++;
		}
		if((strcmp(widthValues[index].validValue,
						LIST_TERMINATOR)) == 0)
		{
			gvWidth=gvAsciiDefaultSettings.width;
			sprintf(logMsg,
				"Warning: Invalid setting of <%s> for <%s> "
				"attempted. Using default value of <%s>.",
				value, name, widthValues[0].validValue);
			fax_log(ModuleName,REPORT_NORMAL,
				FAX_INVALID_PARM_WARN, logMsg);
		}
		break;
	case 1:			/* TEXT_RESOLUTION */
		index=0;
		while((strcmp(resolutionValues[index].validValue,
							LIST_TERMINATOR)) != 0)
		{
			if (strcmp(value, resolutionValues[index].validValue)
								 == 0)
			{
				gvResolution=
					(ushort)resolutionValues[index].dValue;
				break;
			}
			index++;
		}
		if((strcmp(resolutionValues[index].validValue,
						LIST_TERMINATOR)) == 0)
		{
			gvResolution=gvAsciiDefaultSettings.resolution;
			sprintf(logMsg,
				"Warning: Invalid setting of <%s> for <%s> "
				"attempted. Using default value of <%s>.",
				value, name, resolutionValues[0].validValue);
			fax_log(ModuleName,REPORT_NORMAL, 
					FAX_INVALID_PARM_WARN, logMsg);
		}
		break;
	case 2:			/* TEXT_PAGELENGTH */
		/* 
		 * The pagelength is dependent upon the value of the
		 * units field; therefore, we have to wait to process the
		 * units first.  This will be processed after exitting the
		 * loop.
		 */
		pageLengthHasToBeSet=1;
		sscanf(value, "%d", &newPageLengthValue);
		sprintf(pageLengthName, "%s", name);
		break;
	case 3:			/* TEXT_TOPMARGIN */
		/* 
		 * The top margin is dependent upon the value of the
		 * units field; therefore, we have to wait to process the
		 * units first.  This will be processed after exitting the
		 * loop.
		 */
		topMarginHasToBeSet=1;
		sscanf(value, "%d", &newTopMarginValue);
		sprintf(topMarginName, "%s", name);
		break;
	case 4:			/* TEXT_BOTMARGIN */
		/* 
		 * The bottom margin is dependent upon the value of the
		 * units field; therefore, we have to wait to process the
		 * units first.  This will be processed after exitting the
		 * loop.
		 */
		bottomMarginHasToBeSet=1;
		sscanf(value, "%d", &newBottomMarginValue);
		sprintf(bottomMarginName, "%s", name);
		break;
	case 5:			/* TEXT_LEFTMARGIN */
		/* 
		 * The left margin is dependent upon the value of the
		 * units field; therefore, we have to wait to process the
		 * units first.  This will be processed after exitting the
		 * loop.
		 */
		leftMarginHasToBeSet=1;
		sscanf(value, "%d", &newLeftMarginValue);
		sprintf(leftMarginName, "%s", name);
		break;
	case 6:			/* TEXT_RIGHTMARGIN */
		/* 
		 * The right margin is dependent upon the value of the
		 * units field; therefore, we have to wait to process the
		 * units first.  This will be processed after exitting the
		 * loop.
		 */
		rightMarginHasToBeSet=1;
		sscanf(value, "%d", &newRightMarginValue);
		sprintf(rightMarginName, "%s", name);
		break;
	case 7:			/* TEXT_LINESPACING */
		index=0;
		while((strcmp(lineSpacingValues[index].validValue,
							LIST_TERMINATOR)) != 0)
		{
			if (strcmp(value, lineSpacingValues[index].validValue)
								 == 0)
			{
				gvAsciiData.linespace=
					(ushort)lineSpacingValues[index].dValue;
				break;
			}
			index++;
		}
		if((strcmp(lineSpacingValues[index].validValue,
						LIST_TERMINATOR)) == 0)
		{
			gvAsciiData.linespace=gvAsciiDefaultSettings.linespace;
			sprintf(logMsg,
				"Warning: Invalid setting of <%s> for <%s> "
				"attempted.  Using default value of <%s>.",
				value, name, lineSpacingValues[0].validValue);
			fax_log(ModuleName,REPORT_NORMAL, 
						FAX_INVALID_PARM_WARN, logMsg);
		}
		break;
	case 8:			/* TEXT_FONT */
		index=0;
		while((strcmp(fontValues[index].validValue,
							LIST_TERMINATOR)) != 0)
		{
			if (strcmp(value, fontValues[index].validValue)
								 == 0)
			{
				gvAsciiData.font=
					(ushort)fontValues[index].dValue;
				break;
			}
			index++;
		}
		if((strcmp(fontValues[index].validValue,
						LIST_TERMINATOR)) == 0)
		{
			gvAsciiData.font=gvAsciiDefaultSettings.font;
			sprintf(logMsg,
				"Warning: Invalid setting of <%s> for <%s> "
				"attempted.  Using default value of <%s>.",
				value, name, fontValues[0].validValue);
			fax_log(ModuleName,REPORT_NORMAL, 
					FAX_INVALID_PARM_WARN, logMsg);
		}
		break;
	case 9:			/* TEXT_UNITS */
		index=0;
		while((strcmp(unitsValues[index].validValue,
							LIST_TERMINATOR)) != 0)
		{
			if (strcmp(value, unitsValues[index].validValue)
								 == 0)
			{
				gvAsciiData.units=unitsValues[index].dValue;
				break;
			}
			index++;
		}
		if((strcmp(unitsValues[index].validValue,
						LIST_TERMINATOR)) == 0)
		{
			setAsciiDefaults();
			sprintf(logMsg,
				"Warning: Invalid setting of <%s> for <%s> "
				"attempted.  Ascii characteristics will be "
				"set to their defaults (11 inches/.2 inch "
				"top margins/.3 inch side margins).",
				value, name);
			fax_log(ModuleName,REPORT_NORMAL,
				FAX_INVALID_PARM_WARN, logMsg);
		}
		break;
	default:
		sprintf(logMsg, "Warning: Invalid name identifier "
				"<%s> found in file <%s>.  Entire line=<%s>. "
				"Entry is ignored.",
				name, buf, textDescFile);
		fax_log(ModuleName,REPORT_NORMAL,FAX_INVALID_PARM_WARN,logMsg);
                break;
	}
}

fclose(fpTextDesc);

/* 
 * The pagelength is dependent upon the value of the units field; therefore,
 * we had to wait to process the units first.
 */
if ( pageLengthHasToBeSet )
{
	switch (gvAsciiData.units)
	{
		case DF_UNITS_IN10:
			minimumValue=55;
			break;
		case DF_UNITS_MM:
			minimumValue=140;
			break;
		case DF_PELS:
			minimumValue=513;
			break;
	}
	maximumValue=-1;

	if ((rc=validateAscii(pageLengthName, minimumValue, maximumValue,
			newPageLengthValue, &(gvAsciiData.pagelength))) == -1)
	{
		/*
		 * Invalid data was passed.  A warning message was logged,
		 * and ASCII defaults are set.  All is ok.
		 */
		HNDL_RETURN (TEL_SUCCESS);	
	}

} /* pagelength */

/* 
 * The topmargin is dependent upon the value of the units field; therefore,
 * we had to wait to process the units first.
 */
if ( topMarginHasToBeSet )
{
	switch (gvAsciiData.units)
	{
		case DF_UNITS_IN10:
			maximumValue=52;
			break;
		case DF_UNITS_MM:
			maximumValue=134;
			break;
		case DF_PELS:
			maximumValue=512;
			break;
	}
	minimumValue=0;

	if ((rc=validateAscii(topMarginName, minimumValue, maximumValue,
				newTopMarginValue,
				&(gvAsciiData.topmargin))) == -1)
	{
		/*
		 * Invalid data was passed.  A warning message was logged,
		 * and ASCII defaults are set.  All is ok.
		 */
		HNDL_RETURN (TEL_SUCCESS);	
	}
} /* topmargin */

/* 
 * The bottommargin is dependent upon the value of the units field; therefore,
 * we had to wait to process the units first.
 */
if ( bottomMarginHasToBeSet )
{
	switch (gvAsciiData.units)
	{
		case DF_UNITS_IN10:
			maximumValue=52;
			break;
		case DF_UNITS_MM:
			maximumValue=134;
			break;
		case DF_PELS:
			maximumValue=512;
			break;
	}
	minimumValue=0;

	if ((rc=validateAscii(bottomMarginName, minimumValue, maximumValue,
			newBottomMarginValue,
			&(gvAsciiData.botmargin))) == -1)
	{
		/*
		 * Invalid data was passed.  A warning message was logged,
		 * and ASCII defaults are set.  All is ok.
		 */
		HNDL_RETURN (TEL_SUCCESS);	
	}
} /* topmargin */

/* 
 * The leftmargin is dependent upon the value of the units field; therefore,
 * we had to wait to process the units first.
 */
if ( leftMarginHasToBeSet )
{
	switch (gvAsciiData.units)
	{
		case DF_UNITS_IN10:
			maximumValue=25;
			break;
		case DF_UNITS_MM:
			maximumValue=64;
			break;
		case DF_PELS:
			maximumValue=512;
			break;
	}
	minimumValue=0;

	if ((rc=validateAscii(leftMarginName, minimumValue, maximumValue,
				newLeftMarginValue,
				&(gvAsciiData.leftmargin))) == -1)
	{
		/*
		 * Invalid data was passed.  A warning message was logged,
		 * and ASCII defaults are set.  All is ok.
		 */
		HNDL_RETURN (TEL_SUCCESS);	
	}
} /* leftmargin */

/* 
 * The rightmargin is dependent upon the value of the units field; therefore,
 * we had to wait to process the units first.
 */
if ( rightMarginHasToBeSet )
{
	switch (gvAsciiData.units)
	{
		case DF_UNITS_IN10:
			maximumValue=25;
			break;
		case DF_UNITS_MM:
			maximumValue=64;
			break;
		case DF_PELS:
			maximumValue=512;
			break;
	}
	minimumValue=0;

	if ((rc=validateAscii(rightMarginName, minimumValue, maximumValue,
				newRightMarginValue,
				&(gvAsciiData.rightmargin))) == -1)
	{
		/*
		 * Invalid data was passed.  A warning message was logged,
		 * and ASCII defaults are set.  All is ok.
		 */
		HNDL_RETURN (TEL_SUCCESS);	
	}
} /* rightmargin */

HNDL_RETURN(TEL_SUCCESS);

} /* END: TEL_SetFaxTextDefaults() */

/*------------------------------------------------------------------------------
validateAscii()
	This is a generic function to validate the pagelength, top margin,
	bottom margin, left margin, and right margin.  If the value is invalid,
	setAsciiDefaults() is called, which sets all ASCII characteristics 
	to their defaults.  This is because the default values are based on the
	units field.  If the units field is set back to 1/10 inches, all
	values MUST be set by 1/10 inches.
------------------------------------------------------------------------------*/
static int validateAscii(char *name, int minimumValue, int maximumValue,
				int newValueToSetItTo, ushort *gvRealValue)
{
	char		logMsg[512];
		
	char msg_invalid[]=
		"Warning: Invalid setting of %d for <%s>. Must be <= %d. Using defaults: 11 in, .2 in. vertical margins, .3 in. side margins.";

	if (newValueToSetItTo < minimumValue)
	{
		setAsciiDefaults();
		sprintf(logMsg,msg_invalid,newValueToSetItTo,name,minimumValue);
		fax_log(ModuleName,REPORT_NORMAL,FAX_INVALID_PARM_WARN,logMsg);
		return(-1);
	}

	if (maximumValue == -1)
	{
		*gvRealValue=newValueToSetItTo;
		return(0);		/* There is no maximum value */
	}

	if (newValueToSetItTo > maximumValue)
	{
		setAsciiDefaults();
		sprintf(logMsg,msg_invalid,newValueToSetItTo,name,minimumValue);
		fax_log(ModuleName,REPORT_NORMAL,FAX_INVALID_PARM_WARN,logMsg);
		return(-1);
	}

	*gvRealValue=newValueToSetItTo;
	return(0);

} /* END: validDateAscii() */

/*------------------------------------------------------------------------------
setAsciiDefaults()
	This will set all of the ascii characteristics to their default values.
------------------------------------------------------------------------------*/
static void setAsciiDefaults()
{
	gvWidth=gvAsciiDefaultSettings.width;
	gvResolution=gvAsciiDefaultSettings.resolution;
	gvAsciiData.pagelength=gvAsciiDefaultSettings.pagelength;
	gvAsciiData.pagepad=gvAsciiDefaultSettings.pagepad;
	gvAsciiData.topmargin=gvAsciiDefaultSettings.topmargin;
	gvAsciiData.botmargin=gvAsciiDefaultSettings.botmargin;
	gvAsciiData.leftmargin=gvAsciiDefaultSettings.leftmargin;
	gvAsciiData.rightmargin=gvAsciiDefaultSettings.rightmargin;
	gvAsciiData.linespace=gvAsciiDefaultSettings.linespace;
	gvAsciiData.font=gvAsciiDefaultSettings.font;
	gvAsciiData.tabstops=gvAsciiDefaultSettings.tabstops;
	gvAsciiData.units=gvAsciiDefaultSettings.units;
	gvAsciiData.flags=gvAsciiDefaultSettings.flags;
} /* END: setAsciiDefaults() */
#ifdef DEBUG
/*------------------------------------------------------------------------------
dumpASCII()
	Print each field of the ascii data structure.
------------------------------------------------------------------------------*/
static int dumpASCII()
{
	static char	mod[]="dumpASCII";
	char		logMsg[256];
	int		i;


	sprintf(logMsg, "gvAsciiData.pagelength:%d", 
					gvAsciiData.pagelength);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);
	sprintf(logMsg, "gvAsciiData.pagepad:%d", 
					gvAsciiData.pagepad);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);
	sprintf(logMsg, "gvAsciiData.topmargin:%d", 
					gvAsciiData.topmargin);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);
	sprintf(logMsg, "gvAsciiData.botmargin:%d", 
					gvAsciiData.botmargin);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);
	sprintf(logMsg, "gvAsciiData.leftmargin:%d", 
					gvAsciiData.leftmargin);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);
	sprintf(logMsg, "gvAsciiData.rightmargin:%d", 
					gvAsciiData.rightmargin);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);
	sprintf(logMsg, "gvAsciiData.linespace:%d", 
					gvAsciiData.linespace);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);
	sprintf(logMsg, "gvAsciiData.font:%d", 
					gvAsciiData.font);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);
	sprintf(logMsg, "gvAsciiData.tabstops:%d", 
					gvAsciiData.tabstops);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);
	sprintf(logMsg, "gvAsciiData.units:%d", 
					gvAsciiData.units);
	fax_log(ModuleName,REPORT_NORMAL,FAX_BASE,logMsg);

} /* END: dumpASCII() */
#endif
