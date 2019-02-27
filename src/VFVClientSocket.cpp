#include "VFVClientSocket.h"

namespace sereno
{
    VFVClientSocket::VFVClientSocket() : ClientSocket(), m_cursor(-1), stringBuffer(-1)
    {
        m_curMsg.type = NOTHING;
    }

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

#define PUSH_UINT16 \
    {\
        while(!uint16Buffer.isFull() && size) \
        {\
            uint16Buffer.pushValue(message[0]);\
            ADVANCE_BUFFER\
        }\
    }

#define PUSH_UINT32 \
    {\
        while(!uint32Buffer.isFull() && size) \
        {\
            uint32Buffer.pushValue(message[0]);\
            ADVANCE_BUFFER\
        }\
    }

#define PUSH_STRING \
    {\
        if(stringBuffer.maxSize < 0) \
        {\
            PUSH_UINT32 \
            if(uint32Buffer.isFull()) \
            {\
                stringBuffer.setMaxSize(uint32Buffer.getValue()); \
                if(stringBuffer.maxSize < 0)\
                    WARNING << "Negative string size detected" << std::endl;\
                uint32Buffer.clear();\
            }\
        }\
        if(stringBuffer.maxSize >= 0) \
        {\
            while(!stringBuffer.isFull() && size)\
            {\
                stringBuffer.pushValue(message[0]);\
                ADVANCE_BUFFER\
            }\
        }\
    }

#define ERROR_VALUE \
    {\
        ERROR << "Could not push a value..." << std::endl;\
        return false;\
    }

    bool VFVClientSocket::feedMessage(uint8_t* message, uint32_t size)
    {

        ClientSocket::feedMessage(message, size);

        //Test if the message contains something
        if(size == 0)
            return true;

        while(size != 0)
        {
            //Get the type of the message
            if(m_cursor == -1)
            {
                PUSH_UINT16
                if(uint16Buffer.isFull())
                {
                    bool ret = m_curMsg.setType((VFVMessageType)(uint16Buffer.getValue()));
                    uint16Buffer.clear();
                    if(ret)
                    {
                        INFO << "Type " << m_curMsg.type << " found \n";
                        m_cursor = 0;
                    }
                    else
                    {
                        WARNING << "Wrong type parsed in the client" << std::endl;
                        return false;
                    }
                }
            }

            if(m_cursor != -1)
            {
                //Determine to whom we have to send the message
                VFVDataInformation* info = NULL;
                switch(m_curMsg.type)
                {
                    case IDENT_TABLET:
                        info = &m_curMsg.identTablet;
                        break;
                    case ADD_DATASET:
                    case REMOVE_DATASET:
                        info = &m_curMsg.dataset;
                        break;
                    case CHANGE_COLOR:
                        info = &m_curMsg.color;
                        break;
                    case ROTATE_DATASET:
                        info = &m_curMsg.rotate;
                        break;
                    default:
                        break;
                }

                //Push the buffers
                if(info != NULL)
                {
                    switch(info->getTypeAt(m_cursor))
                    {
                        case 's':
                        {
                            PUSH_STRING
                            if(stringBuffer.isFull() && stringBuffer.maxSize >= 0)
                            {
                                if(!info->pushValue(m_cursor, stringBuffer.getValue()))
                                    ERROR_VALUE
                                m_cursor++;
                                stringBuffer.clear();
                                stringBuffer.setMaxSize(-1);
                            }
                            break;
                        }

                        case 'I':
                        {
                            PUSH_UINT32
                            if(uint32Buffer.isFull())
                            {
                                if(!info->pushValue(m_cursor, uint32Buffer.getValue()))
                                    ERROR_VALUE
                                m_cursor++;
                                uint32Buffer.clear();
                            }
                            break;
                        }

                        case 'i':
                        {
                            PUSH_UINT16
                            if(uint16Buffer.isFull())
                            {
                                if(!info->pushValue(m_cursor, uint16Buffer.getValue()))
                                    ERROR_VALUE
                                m_cursor++;
                                uint16Buffer.clear();
                            }
                            break;
                        }

                        case 'f':
                        {
                            PUSH_FLOAT
                            if(floatBuffer.isFull())
                            {
                                if(!info->pushValue(m_cursor, floatBuffer.getValue()))
                                    ERROR_VALUE
                                m_cursor++;
                                floatBuffer.clear();
                            }
                            break;
                        }
                        default:
                            WARNING << "Buffer typed " << info->getTypeAt(m_cursor) << "not handled yet" << std::endl;
                            return false;
                    }

                    //Full message received
                    if((uint32_t)m_cursor == info->getMaxCursor()+1)
                    {
                        m_messages.push(m_curMsg);
                        m_cursor = -1;
                        m_curMsg.type = NOTHING;
                    }
                }
            }
        }
        return true;
    }

    bool VFVClientSocket::setAsTablet(const std::string& hololensIP)
    {
        if(sockAddr.sin_port != htons(TABLET_PORT))
        {
            ERROR << "We are not connected to a tablet port... Quitting\n";
            return false;
        }

        m_identityType = TABLET;

        if(!inet_pton(AF_INET, hololensIP.c_str(), &(m_tablet.hololensAddr.sin_addr)))
        {
            ERROR << "The IP " << hololensIP << " is not valid" << std::endl;
            return false;
        }
        m_tablet.hololensAddr.sin_port = htons(HOLOLENS_PORT);
        return true;
    }


    bool VFVClientSocket::setAsHololens()
    {
        m_identityType = HOLOLENS;
        return true;
    }

    bool VFVClientSocket::pullMessage(VFVMessage* msg)
    {
        INFO << m_messages.size() << std::endl;
        if(m_messages.empty())
            return false;

        *msg = m_messages.front();
        m_messages.pop();
        return true;
    }

#undef ERROR_VALUE
#undef PUSH_STRING
#undef PUSH_UINT32
#undef PUSH_UINT16
#undef PUSH_FLOAT
#undef ADVANCE_BUFFER
}
