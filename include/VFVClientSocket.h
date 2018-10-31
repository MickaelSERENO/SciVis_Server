#ifndef  VFVCLIENTSOCKET_INC
#define  VFVCLIENTSOCKET_INC

#include <queue>
#include <vector>
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <string>
#include <tuple>
#include "utils.h"
#include "ClientSocket.h"
#include "ColorMode.h"
#include "VFVDataInformation.h"
#include "VFVBufferValue.h"
#include "Types/ServerType.h"

/* \brief The different type a client can be */
enum VFVIdentityType
{
    HOLOLENS = 0,
    TABLET   = 1
};

/* \brief The type of message this application can receive */
enum VFVMessageType
{
    NOTHING        = -1,
    IDENT_HOLOLENS = 0,
    IDENT_TABLET   = 1,
    ADD_DATASET,
    REMOVE_DATASET,
    ROTATE_DATASET,
    CHANGE_COLOR,  
    END_MESSAGE_TYPE
};

template<typename T>
struct IsString : std::false_type
{};
template<>
struct IsString<std::string> : std::true_type
{};

template<typename T>
struct IsFloat : std::false_type
{};
template<>
struct IsFloat<float> : std::true_type
{};
template<>
struct IsFloat<double> : std::true_type
{};

template<typename T>
struct IsUint : std::false_type
{};
template<>
struct IsUint<uint32_t> : std::true_type
{};
template<>
struct IsUint<uint16_t> : std::true_type
{};

/* \brief The message information */
struct VFVMessage
{
    VFVMessageType type; /*!< The type of the message*/

    union
    {
        struct VFVIdentTabletInformation identTablet; /*!< Ident information of a tablet*/
        struct VFVDatasetInformation     dataset;     /*!< Dataset information (ADD_DATASET, REMOVE_DATASET)*/
        struct VFVColorInformation       color;       /*!< The color information sent from a tablet*/
        struct VFVRotationInformation    rotate;      /*!< The rotate information sent from a tablet*/
    };

    VFVMessage() : type(NOTHING)
    {}

    VFVMessage(const VFVMessage& cpy)
    {
        *this = cpy;
    }

    VFVMessage& operator=(const VFVMessage& cpy)
    {
        if(this != &cpy)
        {
            setType(cpy.type);
            if(type != NOTHING)
            {
                switch(type)
                {
                    case IDENT_TABLET:
                        identTablet = cpy.identTablet;
                        break;
                    case ADD_DATASET:
                    case REMOVE_DATASET:
                        dataset = cpy.dataset;
                        break;
                    case CHANGE_COLOR:
                        color = cpy.color;
                        break;
                    case ROTATE_DATASET:
                        rotate = cpy.rotate;
                        break;
                    default:
                        WARNING << "Type " << cpy.type << " not handled yet in the copy constructor " << std::endl;
                        break;
                }
            }
        }
        return *this;
    }

    bool setType(VFVMessageType t)
    {
        if(t < NOTHING || t > END_MESSAGE_TYPE)
        {
            WARNING << "Wrong type " << t << " sent\n";
            return false;
        }
        type = t;
        switch(t)
        {
            case IDENT_TABLET:
                new (&identTablet) VFVIdentTabletInformation;
                break;
            case ADD_DATASET:
            case REMOVE_DATASET:
                new (&dataset) VFVDatasetInformation;
                break;
            case CHANGE_COLOR:
                new (&color) VFVColorInformation;
                break;
            case ROTATE_DATASET:
                new (&rotate) VFVRotationInformation;
                break;
            case NOTHING:
                break;
            default:
                WARNING << "Type " << type << " not handled yet " << std::endl;
                return false;
        }
        return true;
    }

    ~VFVMessage(){}
};

struct VFVTabletData
{
    SOCKADDR_IN hololensAddr;
};

struct VFVHololensData
{
    int a;
};

/* \brief VFVClientSocket class. Represent a Client for VFV Application */
class VFVClientSocket : public ClientSocket
{
    public:
        /* \brief Constructor*/
        VFVClientSocket();

        bool feedMessage(uint8_t* message, uint32_t size);

        /* \brief Set the client as tablet
         * \param hololens IP the hololens IP */
        bool setAsTablet(const std::string& hololensIP);

        /* \brief Get a message 
         * \param msg[out] the variable to modify which will contain the message
         * \return true if msg is modified (a message was in the buffer), false otherwise*/
        bool pullMessage(VFVMessage* msg);

        /* \brief Is the client a tablet ?
         * \return whether or not the client is a tablet */
        bool isTablet() const {return m_identityType == TABLET;}
    private:
        std::queue<VFVMessage> m_messages; /*!< List of messages*/
        VFVMessage             m_curMsg; /*!< The current in read message*/
        int32_t                m_cursor; /*!< Indice cursor (what information ID are we reading at ?)*/

        VFVIdentityType        m_identityType; /*!< The type of the client (Tablet or hololens ?)*/
        union
        {
            VFVTabletData   m_tablet;   /*!< The client is considered a tablet*/
            VFVHololensData m_hololens; /*!< The client is considered as a hololens*/
        };

        VFVBufferValue<uint32_t>    uint32Buffer; /*!< The current uint32 buffer*/
        VFVBufferValue<uint16_t>    uint16Buffer; /*!< The current uint16_t buffer*/
        VFVBufferValue<float>       floatBuffer;  /*!< The current float buffer*/
        VFVBufferValue<std::string> stringBuffer; /*!< The current std::string buffer*/
};

#endif
