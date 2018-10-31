#ifndef  VFVCLIENTSOCKET_INC
#define  VFVCLIENTSOCKET_INC

#include "ClientSocket.h"

class VFVClientSocket : public ClientSocket
{
    public:
        VFVClientSocket();
        void feedMessage(uint8_t* message, uint32_t size);
    private:
        
};

#endif
