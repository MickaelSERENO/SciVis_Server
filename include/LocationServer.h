#ifndef  LOCATIONSERVER_INC
#define  LOCATIONSERVER_INC

#include "Server.h"
#include "VFVServer.h"
#include "LocationClientSocket.h"
#include <glm/glm.hpp>
#include "Quaternion.h"

namespace sereno
{
    class LocationServer : public Server<LocationClientSocket>
    {
        public:
            LocationServer(uint32_t nbThread, uint32_t port, VFVServer* vfvServer);
            void onMessage(uint32_t bufID, LocationClientSocket* client, uint8_t* data, uint32_t size);
        protected:
            VFVServer*  m_vfvServer;
            glm::vec3   m_pos;
            Quaternionf m_rot;
    };
}

#endif