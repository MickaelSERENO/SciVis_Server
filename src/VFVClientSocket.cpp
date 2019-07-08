#include "VFVClientSocket.h"

namespace sereno
{
    uint32_t VFVClientSocket::nextHeadsetID = 0;

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

#define FILL_BYTE_ARRAY\
    {\
        while(arrBufferIdx < uint32Buffer.getValue() && size)\
        {\
            arrBuffer[arrBufferIdx++] = message[0];\
            ADVANCE_BUFFER\
        }\
    }\

#define PUSH_BYTE_ARRAY \
    {\
        if(!arrBuffer) \
        {\
            PUSH_UINT32 \
            if(uint32Buffer.isFull()) \
            {\
                arrBufferIdx = 0;\
                arrBuffer = (uint8_t*)malloc(sizeof(uint8_t)*uint32Buffer.getValue());\
                FILL_BYTE_ARRAY\
            }\
        }\
        else\
            FILL_BYTE_ARRAY\
    }\

#define ERROR_VALUE \
    {\
        ERROR << "Could not push a value... type: " << m_curMsg.type << std::endl;\
        return false;\
    }

    VFVClientSocket::~VFVClientSocket()
    {
        if(isTablet())
            m_tablet.~VFVTabletData();
        else if(isHeadset())
            m_headset.~VFVHeadsetData();
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
                        m_cursor = 0;
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
                VFVDataInformation* info = m_curMsg.curMsg;

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
                        case 'b':
                        {
                            size--;
                            message++;
                            if(!info->pushValue(m_cursor, message[-1]))
                                ERROR_VALUE
                            m_cursor++;
                            break;
                        }
                        case 'a':
                        {
                            PUSH_BYTE_ARRAY
                            if(arrBufferIdx == uint32Buffer.getValue() && arrBuffer)
                            {
                                uint32Buffer.clear();
                                if(!info->pushValue(m_cursor, std::shared_ptr<uint8_t>(arrBuffer), arrBufferIdx))
                                {
                                    arrBuffer = NULL;
                                    ERROR_VALUE
                                }
                                arrBuffer = NULL;
                                m_cursor++;
                            }
                            break;
                        }
                        default:
                            WARNING << "Buffer typed '" << info->getTypeAt(m_cursor) << "' not handled yet at cursor = " << m_cursor << std::endl;
                            return false;
                    }

                    //Full message received
                    if(m_cursor >= info->getMaxCursor()+1)
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

    bool VFVClientSocket::setAsTablet(const std::string& headsetIP)
    {
        new (&m_tablet) VFVTabletData;

        m_identityType = TABLET;
        if(headsetIP.size())
        {
            if(!inet_pton(AF_INET, headsetIP.c_str(), &(m_tablet.headsetAddr.sin_addr)))
            {
                ERROR << "The IP " << headsetIP << " is not valid" << std::endl;
                return false;
            }
            m_tablet.headsetAddr.sin_port = htons(CLIENT_PORT);
            INFO << "Bound to Headset IP " << headsetIP << " port " << CLIENT_PORT << std::endl;
        }
        return true;
    }


    bool VFVClientSocket::setAsHeadset()
    {
        new (&m_headset) VFVHeadsetData;
        m_identityType = HEADSET;
        m_headset.id   = nextHeadsetID++;
        return true;
    }

    bool VFVClientSocket::pullMessage(VFVMessage* msg)
    {
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
