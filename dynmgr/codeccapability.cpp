
////////////////////////////////////////////////////////////////////////////////////////
//
//CodecCapabilities implementation
//

int CodecCapabilities::setCapability(const int capability)
{
	char mod[] = {"CodecCapabilities::setCapability"};
	if(capabilityIndex > MAX_NUM_OF_CODEC_SUPPORTED)
	{
		dynVarLog(__LINE__, -1, mod, REPORT_DETAIL, DYN_BASE, INFO,
					"Failed to setCapability total number of codec supported is exceeded");
		return -1;
	}

	dynVarLog(__LINE__, -1, mod, REPORT_DETAIL, DYN_BASE, INFO,
				"setting codec capability to %d", capability);
								

	for(int i=0; i<capabilityIndex; i++)
	{
		if(capabilities[i] == capability)
		{
			dynVarLog(__LINE__, -1, mod, REPORT_DETAIL, DYN_BASE, INFO,
						"Codec Capability=%d,  is already set ", capability);
			return 20;
		}
	}

	capabilities[capabilityIndex] = capability;
	capabilityIndex++;	
	return 0;
}

void CodecCapabilities::printCapabilities()
{
	for(int i=0; i<capabilityIndex; i++)
	{
		cout<<"CodecCapability["<<i<<"]="<<capabilities[i]<<endl;
	}
}

int CodecCapabilities::removeCapability(const int capability)
{
	char mod[] = {"CodecCapabilities::removeCapability"};
	for(int i=0; i<capabilityIndex; i++)
	{
		if(capabilities[i] == capability)
		{
			capabilities[i] = 0;
			capabilityIndex --;
			return 0;
		}
	}
	dynVarLog(__LINE__, -1, mod, REPORT_DETAIL, DYN_BASE, INFO,
					"Failed to remove codec capability=%d", capability);
	return -1;
}

int CodecCapabilities::getCapability(const int capability)
{
	char mod[] = {"CodecCapabilities::getCapability"};
	for(int i=0; i<capabilityIndex; i++)
	{
		if(capabilities[i] == capability)
		{
			return 0;
		}
	}
	dynVarLog(__LINE__, -1, mod, REPORT_DETAIL, DYN_BASE, INFO,
					"codec capability=%d is not set", capability);
	return -1;
}
