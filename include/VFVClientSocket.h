#ifndef  VFVCLIENTSOCKET_INC
#define  VFVCLIENTSOCKET_INC

#include <queue>
#include <vector>
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <string>
#include <tuple>
#include <glm/glm.hpp>
#include "utils.h"
#include "ClientSocket.h"
#include "ColorMode.h"
#include "VFVDataInformation.h"
#include "VFVBufferValue.h"
#include "Types/ServerType.h"
#include "writeData.h"
#include "Quaternion.h"

#define CLIENT_PORT 8000

namespace sereno
{
    class VFVClientSocket;

    /* \brief The different type a client can be */
    enum VFVIdentityType
    {
        NO_IDENT = -1,
        HEADSET = 0,
        TABLET   = 1
    };

    /* \brief The type of message this application can receive */
    enum VFVMessageType
    {
        NOTHING            = -1,
        IDENT_HEADSET      = 0,
        IDENT_TABLET       = 1,
        ADD_BINARY_DATASET = 2,
        ADD_VTK_DATASET    = 3,
        ROTATE_DATASET     = 4,
        UPDATE_HEADSET     = 5,
        ANNOTATION_DATA    = 6,
        ANCHORING_DATA_SEGMENT = 7,
        ANCHORING_DATA_STATUS  = 8,
        HEADSET_CURRENT_ACTION = 9,
        HEADSET_CURRENT_SUB_DATASET = 10,
        END_MESSAGE_TYPE
    };

    /** \brief Enumeration of the client current action */
    enum VFVHeadsetCurrentActionType
    {
        HEADSET_CURRENT_ACTION_NOTHING   = 0,
        HEADSET_CURRENT_ACTION_MOVING    = 1,
        HEADSET_CURRENT_ACTION_SCALING   = 2,
        HEADSET_CURRENT_ACTION_ROTATING  = 3,
        HEADSET_CURRENT_ACTION_SKETCHING = 4
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
            struct VFVNoDataInformation         noData;        /*!< Structure for message with no internal data*/
            struct VFVIdentTabletInformation    identTablet;   /*!< Ident information of a tablet*/
            struct VFVBinaryDatasetInformation  binaryDataset; /*!< Binary Dataset information*/
            struct VFVVTKDatasetInformation     vtkDataset;    /*!< Binary Dataset information*/
            struct VFVColorInformation          color;         /*!< The color information sent from a tablet*/
            struct VFVRotationInformation       rotate;        /*!< The rotate information sent from a tablet*/
            struct VFVUpdateHeadset             headset;       /*!< The headset update data information*/
            struct VFVAnnotation                annotation;    /*!< The annotation data information*/
            struct VFVDefaultByteArray          defaultByteArray;    /*!< Default byte array information*/
            struct VFVAnchoringDataStatus       anchoringDataStatus; /*!< Anchoring data status*/
            struct VFVHeadsetCurrentAction      headsetCurrentAction;     /*!< The headset current action*/
            struct VFVHeadsetCurrentSubDataset  headsetCurrentSubDataset; /*!< The headset current SubDataset*/
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
                        case IDENT_HEADSET:
                            noData = cpy.noData;
                            break;
                        case IDENT_TABLET:
                            identTablet = cpy.identTablet;
                            break;
                        case ADD_BINARY_DATASET:
                            binaryDataset = cpy.binaryDataset;
                            break;
                        case ADD_VTK_DATASET:
                            vtkDataset = cpy.vtkDataset;
                            break;
                        case ROTATE_DATASET:
                            rotate = cpy.rotate;
                            break;
                        case UPDATE_HEADSET:
                            headset = cpy.headset;
                            break;
                        case ANNOTATION_DATA:
                            annotation = cpy.annotation;
                            break;
                        case ANCHORING_DATA_SEGMENT:
                            defaultByteArray = cpy.defaultByteArray;
                            break;
                        case ANCHORING_DATA_STATUS:
                            anchoringDataStatus = cpy.anchoringDataStatus;                            
                            break;
                        case HEADSET_CURRENT_ACTION:
                            headsetCurrentAction = cpy.headsetCurrentAction;
                            break;
                        case HEADSET_CURRENT_SUB_DATASET:
                            headsetCurrentSubDataset = cpy.headsetCurrentSubDataset;
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
                case IDENT_HEADSET:
                    new (&noData) VFVNoDataInformation;
                    break;
                case IDENT_TABLET:
                    new (&identTablet) VFVIdentTabletInformation;
                    break;
                case ADD_BINARY_DATASET:
                    new (&binaryDataset) VFVBinaryDatasetInformation;
                    break;
                case ADD_VTK_DATASET:
                    new (&vtkDataset) VFVVTKDatasetInformation;
                    break;
                case ROTATE_DATASET:
                    new (&rotate) VFVRotationInformation;
                    break;
                case UPDATE_HEADSET:
                    new (&headset) VFVUpdateHeadset;
                    break;
                case ANNOTATION_DATA:
                    new (&annotation) VFVAnnotation;
                    break;
                case ANCHORING_DATA_SEGMENT:
                    new (&defaultByteArray) VFVDefaultByteArray;
                    break;
                case ANCHORING_DATA_STATUS:
                    new (&anchoringDataStatus) VFVAnchoringDataStatus;
                    break;
                case HEADSET_CURRENT_ACTION:
                    new (&headsetCurrentAction) VFVHeadsetCurrentAction;
                    break;
                case HEADSET_CURRENT_SUB_DATASET:
                    new (&headsetCurrentSubDataset) VFVHeadsetCurrentSubDataset;
                    break;
                case NOTHING:
                    break;
                default:
                    WARNING << "Type " << type << " not handled yet " << std::endl;
                    return false;
            }
            return true;
        }

        ~VFVMessage()
        {
            switch(type)
            {
                case IDENT_HEADSET:
                    noData.~VFVNoDataInformation();
                    break;
                case IDENT_TABLET:
                    identTablet.~VFVIdentTabletInformation();
                    break;
                case ADD_BINARY_DATASET:
                    binaryDataset.~VFVBinaryDatasetInformation();
                    break;
                case ADD_VTK_DATASET:
                    vtkDataset.~VFVVTKDatasetInformation();
                    break;
                case ROTATE_DATASET:
                    rotate.~VFVRotationInformation();
                    break;
                case UPDATE_HEADSET:
                    headset.~VFVUpdateHeadset();
                    break;
                case ANNOTATION_DATA:
                    annotation.~VFVAnnotation();
                    break;
                case ANCHORING_DATA_SEGMENT:
                    defaultByteArray.~VFVDefaultByteArray();
                    break;
                case ANCHORING_DATA_STATUS:
                    anchoringDataStatus.~VFVAnchoringDataStatus();
                    break;
                case HEADSET_CURRENT_ACTION:
                    headsetCurrentAction.~VFVHeadsetCurrentAction();
                    break;
                case HEADSET_CURRENT_SUB_DATASET:
                    headsetCurrentSubDataset.~VFVHeadsetCurrentSubDataset();
                    break;
                case NOTHING:
                    break;
                default:
                    WARNING << "Type " << type << " not handled yet in the destructor " << std::endl;
                    break;
            }
        }
    };

    /** \brief  Tablet data structur */
    struct VFVTabletData
    {
        SOCKADDR_IN      headsetAddr;    /*!< What is the address of the headset bound to this tablet?*/
        VFVClientSocket* headset = NULL; /*!< WHat is the headset bound to this tablet?*/
    };

    /** \brief  Headset data structure */
    struct VFVHeadsetData
    {
        uint32_t                    id;                                             /*!< ID of the headset*/
        VFVClientSocket*            tablet = NULL;                                  /*!< The tablet bound to this Headset*/
        uint32_t                    color  = 0x000000;                              /*!< The displayed color representing this headset*/
        glm::vec3                   position;                                       /*!< 3D position of the headset*/
        Quaternionf                 rotation;                                       /*!< 3D rotation of the headset*/
        bool                        anchoringSent = false;                          /*!< Has the anchoring data been sent?*/
        VFVHeadsetCurrentActionType currentAction = HEADSET_CURRENT_ACTION_NOTHING; /*!< What is the tablet current action?*/
    };

    /* \brief VFVClientSocket class. Represent a Client for VFV Application */
    class VFVClientSocket : public ClientSocket
    {
        public:
            /* \brief Constructor*/
            VFVClientSocket();

            bool feedMessage(uint8_t* message, uint32_t size);

            /* \brief Set the client as tablet
             * \param headset IP the headset IP */
            bool setAsTablet(const std::string& headsetIP);

            /* \brief  Set the client as headset
             * \return true on success, false on failure */
            bool setAsHeadset();

            /* \brief Get a message 
             * \param msg[out] the variable to modify which will contain the message
             * \return true if msg is modified (a message was in the buffer), false otherwise*/
            bool pullMessage(VFVMessage* msg);

            /* \brief Is the client a tablet ?
             * \return whether or not the client is a tablet */
            bool isTablet() const {return m_identityType == TABLET;}

            /* \brief Is the client a headset ?
             * \return whether or not the client is a headset */
            bool isHeadset() const {return m_identityType == HEADSET;}

            /** \brief  Get the TabletData. Works only if isTablet returns true!
             * \return the Tablet Data*/
            const VFVTabletData& getTabletData() const {return m_tablet;}

            /** \brief  Get the TabletData. Works only if isTablet returns true!
             * \return the Tablet Data*/
            VFVTabletData& getTabletData() {return m_tablet;}

            /** \brief  Get the HeadsetData. Works only if isHeadset returns true!
             * \return the Headset Data*/
            const VFVHeadsetData& getHeadsetData() const {return m_headset;}

            /** \brief  Get the HeadsetData. Works only if isHeadset returns true!
             * \return the Headset Data*/
            VFVHeadsetData& getHeadsetData() {return m_headset;}
        private:
            static uint32_t nextHeadsetID;

            std::queue<VFVMessage> m_messages; /*!< List of messages parsed*/
            VFVMessage             m_curMsg;   /*!< The current in read message*/
            int32_t                m_cursor;   /*!< Indice cursor (what information ID are we reading at ?)*/

            VFVIdentityType        m_identityType = NO_IDENT; /*!< The type of the client (Tablet or headset ?)*/
            union
            {
                VFVTabletData  m_tablet;   /*!< The client is considered a tablet*/
                VFVHeadsetData m_headset; /*!< The client is considered as a headset*/
            };

            VFVBufferValue<uint32_t>    uint32Buffer;     /*!< The current uint32 buffer*/
            VFVBufferValue<uint16_t>    uint16Buffer;     /*!< The current uint16_t buffer*/
            VFVBufferValue<float>       floatBuffer;      /*!< The current float buffer*/
            VFVBufferValue<std::string> stringBuffer;     /*!< The current std::string buffer*/
            uint8_t*                    arrBuffer = NULL; /*!< The uint8_t array buffer*/
            uint32_t                    arrBufferIdx;     /*!< The current array buffer Idx*/
    };
}

#endif
