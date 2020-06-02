#include "LocationServer.h"

namespace sereno
{
    LocationServer::LocationServer(uint32_t nbThread, uint32_t port, VFVServer* vfvServer) : Server(nbThread, port)
    {
        m_vfvServer = vfvServer;
    };

    void LocationServer::onMessage(uint32_t bufID, LocationClientSocket* client, uint8_t* data, uint32_t size)
    {
        LocationMessage msg;
        if(client->feedMessage(data, size)){

            while(client->pullMessage(&msg))
            {
                m_pos.x = msg.posX;
                m_pos.y = msg.posY;
                m_pos.z = msg.posZ;
                m_rot.x = msg.rotX;
                m_rot.y = msg.rotY;
                m_rot.z = msg.rotZ;
                m_rot.w = msg.rotW;
            }

            //INFO << "Tablet position: " << m_pos.x << " " << m_pos.y << " " << m_pos.z << "; "
            //     << "Tablet rotation: " << m_rot.x << " " << m_rot.y << " " << m_rot.z << " " << m_rot.w << std::endl;
            
            m_vfvServer->updateLocationTablet(m_pos, m_rot);
        }
    }
}
