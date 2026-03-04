#include "../bitutils.hpp"


int Count1s(const uint8_t *Byte, int Bytes)
{
    int Count=0;
    for( ; Bytes>0; Bytes--)
    {
        Count += __builtin_popcount(*Byte++);
    }
    return Count;
}

