/*------------------------------------------------------------------------------
File: testGetPkg.c
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <errno.h>

static int sGetPkgInfo(char *zProduct, char *zPkgInfoFile,
					char *zVersion, char *zMsg);

int main(int argc, char *argv[])
{
	char		version[8]= "";
	char		errMsg[256];
	int			rc;

	rc = sGetPkgInfo("arcDMTEL_SR6.1", "/home/.arcDMTEL_SR6.1.pkginfo",
			version, errMsg);

	printf("%d = sGetPkgInfo(arcDMTEL_SR6.1, /home/.arcDMTEL_SR6.1.pkginfo, "
						"%s, %s\n", rc, version, errMsg);
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int sGetPkgInfo(char *zProduct, char *zPkgInfoFile,
					char *zVersion, char *zMsg)
{
	FILE		*fp;
	char		buf[1024];
	char		*p;
	int			i;
	char		sVersion[64];
	char		sInstalled[64];

	memset((char *)sVersion, '\0', sizeof(sVersion));
	zMsg[0] = '\0';

	if ((fp = fopen(zPkgInfoFile, "r")) == NULL)
	{
		sprintf(zMsg, "Error: Unable to open (%s).  [%d, %s]. "
			"Product (%s) is not installed.  Install %s and retry.",
			zPkgInfoFile, errno, strerror(errno), zProduct, zProduct);
		return(-1);
	}

	(void)fseek(fp, 0, SEEK_END);
	(void)fseek(fp, -20, SEEK_CUR);

	memset((char *)buf, '\0', sizeof(buf));
	if ((fgets(buf, sizeof(buf) - 1, fp)) == NULL)
	{
		sprintf(zMsg, "Error: Unable to determine status/version of (%s).  "
			"Verify product %s is installed and retry.", zProduct, zProduct);
		fclose(fp);
		return(-1);
	}
	if ((strstr(buf, "REMOVED")) != NULL)
	{
		sprintf(zMsg, "Error: (%s) shows that %s is not installed.  "
			"Install %s and retry.", zPkgInfoFile, zProduct, zProduct);
		fclose(fp);
		return(-1);
	}

	// It is verified that it is installed; must get the version now.

	(void)fseek(fp, 0, SEEK_SET);
	memset((char *) buf, '\0', sizeof(buf));
	while  ((fgets(buf, sizeof(buf) - 1, fp)) != NULL)
	{
		buf[strlen(buf) - 1] = '\0';

		if ((p = strstr(buf, "VERSION")) == NULL)
		{
			continue;
		}
		sprintf(buf, "%s", p);
		p = strstr(buf, ":");
		p++;
		i = 0;
		memset((char *)sVersion, '\0', sizeof(sVersion));
		while (*p != '\0')
		{
			if ( (isdigit(*p)) || (*p == '.') )
			{
				sVersion[i++] = *p;
			}
			p++;
		}
	}
	if ( sVersion[0] == '\0' )
	{
		sprintf(zMsg, "Error: Unable to determine version from file (%s).  "
			"Verify the installation of %s and retry.", zPkgInfoFile, zProduct);
		fclose(fp);
		return(-1);
	}
	sprintf(zVersion, "%s", sVersion);

	fclose(fp);
	return(0);
	
} // END: sGetPkgInfo()
