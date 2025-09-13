#include "../encryption.hpp"

#define XXTEA_MX (((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (key[(p & 3) ^ e] ^ z))
constexpr uint32_t XXTEADELTA = 0x9e3779b9;

void xxteaEncrypt(uint32_t data[], uint8_t words, const uint32_t key[4], uint8_t loops)
{
    words--;
    uint32_t sum = 0, y;
    uint32_t z = data[words];
    do
    {
        sum += XXTEADELTA;
        uint8_t e = (sum>>2)&3;
        uint8_t p;
        for (p=0; p<words; p++)
        {
            y = data[p+1];
            z = data[p] += XXTEA_MX;
        }
        y = data[0];
        z = data[words] += XXTEA_MX;
    }
    while (--loops);
}

void xxteaDecrypt(uint32_t data[], uint8_t words, const uint32_t key[4], uint8_t loops)
{
    words--;
    uint32_t sum = loops*XXTEADELTA;
    uint32_t y = data[0];
    uint32_t z;
    do
    {
        uint8_t e = (sum>>2)&3;
        uint8_t p;
        for (p=words; p; p--)
        {
            z = data[p-1];
            y = data[p] -= XXTEA_MX;
        }
        z = data[words];
        y = data[p] -= XXTEA_MX;
        sum -= XXTEADELTA;
    }
    while (--loops);
}

void TEA_Encrypt_Key0 (uint32_t* data, uint8_t loops)
{
    uint32_t v0=data[0], v1=data[1];                          // set up
    uint32_t sum=0;          // a key schedule constant
    for (uint8_t i=0; i < loops; i++)                             // basic cycle start
    {
        sum += XXTEADELTA;
        v0 += (v1<<4) ^ (v1 + sum) ^ (v1>>5);
        v1 += (v0<<4) ^ (v0 + sum) ^ (v0>>5);
    }                 // end cycle
    data[0]=v0;
    data[1]=v1;
}

void TEA_Decrypt_Key0 (uint32_t* data, uint8_t loops)
{
    uint32_t v0=data[0], v1=data[1];                           // set up
    uint32_t sum=XXTEADELTA*loops; // a key schedule constant
    for (uint8_t i=0; i < loops; i++)                              // basic cycle start
    {
        v1 -= (v0<<4) ^ (v0 + sum) ^ (v0>>5);
        v0 -= (v1<<4) ^ (v1 + sum) ^ (v1>>5);
        sum -= XXTEADELTA;
    }                                          // end cycle
    data[0]=v0;
    data[1]=v1;
}



void XXTEA_Encrypt_Key0(uint32_t *Data, uint8_t Words, uint8_t Loops)
{
    uint32_t Sum = 0;
    uint32_t Z = Data[Words-1];
    uint32_t Y;
    for( ; Loops; Loops--)
    {
        Sum += XXTEADELTA;
        for (uint8_t P=0; P<(Words-1); P++)
        {
            Y = Data[P+1];
            Z = Data[P] += XXTEA_MX_KEY0(Y, Z, Sum);
        }
        Y = Data[0];
        Z = Data[Words-1] += XXTEA_MX_KEY0(Y, Z, Sum);
    }
}

void XXTEA_Decrypt_Key0(uint32_t *Data, uint8_t Words, uint8_t Loops)
{
    uint32_t Sum = Loops*XXTEADELTA;
    uint32_t Y = Data[0];
    uint32_t Z;
    for( ; Loops; Loops--)
    {
        for (uint8_t P=Words-1; P; P--)
        {
            Z = Data[P-1];
            Y = Data[P] -= XXTEA_MX_KEY0(Y, Z, Sum);
        }
        Z = Data[Words-1];
        Y = Data[0] -= XXTEA_MX_KEY0(Y, Z, Sum);
        Sum -= XXTEADELTA;
    }
}