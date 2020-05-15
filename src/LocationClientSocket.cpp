#include "LocationClientSocket.h"

namespace sereno
{
    bool LocationClientSocket::feedMessage(uint8_t* message, uint32_t size)
    {

#define ADVANCE_BUFFER \
    {\
        message++;\
        size--;\
    }

#define PUSH_FLOAT \
    {\
        while(!floatBuffer.isFull() && size) \
        {\
            floatBuffer.pushValue(message[0]);\
            ADVANCE_BUFFER\
        }\
    }

        ClientSocket::feedMessage(message, size);
        
        // get the coordinates
        if(size > 0)
        {
            PUSH_FLOAT
            if(floatBuffer.isFull())
            {
                m_curMsg.posX = floatBuffer.getValue();
                floatBuffer.clear();
            }

            PUSH_FLOAT
            if(floatBuffer.isFull())
            {
                m_curMsg.posY = floatBuffer.getValue();
                floatBuffer.clear();
            }

            PUSH_FLOAT
            if(floatBuffer.isFull())
            {
                m_curMsg.posZ = floatBuffer.getValue();
                floatBuffer.clear();
            }

            PUSH_FLOAT
            if(floatBuffer.isFull())
            {
                m_curMsg.rotX = floatBuffer.getValue();
                floatBuffer.clear();
            }

            PUSH_FLOAT
            if(floatBuffer.isFull())
            {
                m_curMsg.rotY = floatBuffer.getValue();
                floatBuffer.clear();
            }

            PUSH_FLOAT
            if(floatBuffer.isFull())
            {
                m_curMsg.rotZ = floatBuffer.getValue();
                floatBuffer.clear();
            }

            PUSH_FLOAT
            if(floatBuffer.isFull())
            {
                m_curMsg.rotW = floatBuffer.getValue();
                floatBuffer.clear();
            }
            
            m_messages.push(m_curMsg);
            return true;
        }
        return false;
    }
    
    bool LocationClientSocket::pullMessage(LocationMessage* msg)
    {
        if(m_messages.empty())
            return false;

        *msg = m_messages.front();
        m_messages.pop();
        return true;
    }
}