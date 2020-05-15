#ifndef  LOCATIONCLIENTSOCKET_INC
#define  LOCATIONCLIENTSOCKET_INC

#include "ClientSocket.h"
#include "VFVBufferValue.h"

#define LOCATION_PORT 8100

namespace sereno
{
    struct LocationMessage
    {
        // position
        float posX, posY, posZ;
        // rotation
        float rotX, rotY, rotZ, rotW;
    };

    class LocationClientSocket : public ClientSocket
    {
        public:
            virtual bool feedMessage(uint8_t* data, uint32_t size);
            bool pullMessage(LocationMessage* msg);
        private:
            std::queue<LocationMessage> m_messages; /*!< List of messages parsed*/
            LocationMessage             m_curMsg;   /*!< The current in read message*/

            VFVBufferValue<float>       floatBuffer;      /*!< The current float buffer*/
    };
}

#endif