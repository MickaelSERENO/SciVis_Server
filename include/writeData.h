#ifndef  WRITEDATA_INC
#define  WRITEDATA_INC

#include <cstdint>

namespace sereno
{
    inline void writeUint32(uint8_t* buf, uint32_t value)
    {
        buf[0] = (value >> 24);
        buf[1] = (value >> 16) & 0xFF;
        buf[2] = (value >> 8)  & 0xFF;
        buf[3] = value & 0xFF;
    }

    inline void writeUint16(uint8_t* buf, uint16_t value)
    {
        buf[0] = (value >> 8)  & 0xFF;
        buf[1] = value & 0xFF;
    }
}

#endif
