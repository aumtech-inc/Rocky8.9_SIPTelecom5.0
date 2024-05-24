#include "mpacket.h"


// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
  #if !defined (MEMCHECK)
    #define new DEBUG_NEW
  #endif // !MEMCHECK
  #undef THIS_FILE
  static char THIS_FILE[] = __FILE__;
#endif


#define MIN_BLOCK 32



CMPacket::CMPacket(int sz) : CMBuffer(sz)
{
    // Note part of the construction is done in CMBuffer.
    error = 0;
    buffer = NULL;
    buffer_count=0;
    max_payload_size=0;
}

CMPacket::~CMPacket()
{
    Free();
}

MOCTET * CMPacket::Buffer() const
{
	if(buffer == NULL)
	{
		//printf("RGDEBUG:: buffer is not null\n");fflush(stdout);
	}
	else
	{
		//printf("RGDEBUG:: buffer null\n");fflush(stdout);
	}
    return buffer != NULL ? buffer : base;
}

int CMPacket::BufferCount() const
{
    return buffer != NULL ? buffer_count : DataCount();
}

void CMPacket::BufferCount(int n)
{
    if (buffer != NULL)
    {
        buffer_count = n;
    }
    else
    {
        DataCount(n);
    }
}

BOOL CMPacket::DataMakeSpace(int n)
{
    // For derived class overloading
    return CMBuffer::DataMakeSpace(n);
}

void CMPacket::Free()
{
    if (buffer)
    {
        delete [] buffer;
        buffer = NULL;
    }

    CMBuffer::Free();
    error = 0;
}

void CMPacket::SetHeaders(void * /*hdr*/, void * /*plhdr*/)
{
}

void CMPacket::GetHeaders(void * /*hdr*/, void * /*plhdr*/)
{
}

void CMPacket::ConfigPayload(int /*ts*/)
{
}

void CMPacket::Packetise(int /*ts*/)
{
}

int CMPacket::Segmentalise(int /*inbytes*/) 
{
    return 0;
}
