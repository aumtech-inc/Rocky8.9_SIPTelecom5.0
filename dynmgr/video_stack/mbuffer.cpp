#include "mbuffer.h"
#include "mutils.h"


// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
  #if !defined (MEMCHECK)
    #define new DEBUG_NEW
  #endif // !MEMCHECK
  #undef THIS_FILE
  static char THIS_FILE[] = __FILE__;
#endif


#define MIN_BLOCK 32



CMBuffer::CMBuffer(int sz)
{
    if (sz > 0)
    {
        base = new MOCTET[sz];
    }
    else
    {
        base = NULL;
    }
    max_data_size=sz;
    cur_ptr = base;

#ifdef _DEBUG
    isARtpPacket = FALSE;
#endif //_DEBUG
}

CMBuffer::~CMBuffer()
{
    Free();
}

CMBuffer::CMBuffer(const CMBuffer & p_other)
{
    max_data_size = 0;
#ifdef _DEBUG
    isARtpPacket = FALSE;
#endif //_DEBUG
    base = NULL;
    cur_ptr = NULL;
    operator=(p_other);
}

CMBuffer& CMBuffer::operator =(const CMBuffer & p_other)
{
    if ((&p_other != this) && (p_other.base != NULL))
    {
        max_data_size = p_other.max_data_size;

        if (base != NULL)
        {
            delete [] base;
        }

        base = new MOCTET[max_data_size];
        memcpy(base, p_other.base, max_data_size);
        cur_ptr = base + p_other.DataCount();

    #ifdef _DEBUG
        isARtpPacket = p_other.isARtpPacket;
    #endif //_DEBUG

    }
    return *this;
}

void CMBuffer::Free()
{
    if (base) {
        delete [] base;
        base = NULL;
    }
    cur_ptr = base;
    max_data_size = 0;
}


BOOL CMBuffer::DataMakeSpace(int n)
{
    int inc;
    int sp = MaxDataSize() - DataCount();

#if defined(_DEBUG)
    assert((int)(sp >= 0));
#endif

    if (n <= sp) return TRUE;
    inc = n - sp;

    MOCTET *b;
    int new_size = max_data_size + MAX(MIN_BLOCK, inc);
    int count = DataCount();
    
    b = new MOCTET [new_size];
    if (! b)
    {
        return FALSE;
    }

    if (base)
    {
        memcpy(b, base, count * sizeof(MOCTET));

        cur_ptr = &b[count];
        delete [] base;
        base = b;
    }
    else
    {
        cur_ptr = base = b;
    }
    max_data_size = new_size;

    return TRUE;
}

BOOL CMBuffer::DataAppend(const MOCTET *b, int n)
{
    if (! b)
        return FALSE;

    if (! DataMakeSpace(n))
        return FALSE;

    memmove(cur_ptr, b, n *sizeof(MOCTET));
    cur_ptr += n;

    return TRUE;
}
