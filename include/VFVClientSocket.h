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
#include <map>
#include "Datasets/SubDataset.h"
#include "SubDatasetHeadsetInformation.h"
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
        NOTHING                     = -1,
        IDENT_HEADSET               = 0,
        IDENT_TABLET                = 1,
        ADD_BINARY_DATASET          = 2,
        ADD_VTK_DATASET             = 3,
        ROTATE_DATASET              = 4,
        UPDATE_HEADSET              = 5,
        ANNOTATION_DATA             = 6,
        ANCHORING_DATA_SEGMENT      = 7,
        ANCHORING_DATA_STATUS       = 8,
        HEADSET_CURRENT_ACTION      = 9,
        HEADSET_CURRENT_SUB_DATASET = 10,
        TRANSLATE_DATASET           = 11,
        SCALE_DATASET               = 12,
        VISIBILITY_DATASET          = 13,
        START_ANNOTATION            = 14,
        ANCHOR_ANNOTATION           = 15,
        CLEAR_ANNOTATIONS           = 16,
        NEXT_TRIAL                  = 17,
        END_MESSAGE_TYPE
    };

    /** \brief Enumeration of the client current action */
    enum VFVHeadsetCurrentActionType
    {
        HEADSET_CURRENT_ACTION_NOTHING     = 0,
        HEADSET_CURRENT_ACTION_MOVING      = 1,
        HEADSET_CURRENT_ACTION_SCALING     = 2,
        HEADSET_CURRENT_ACTION_ROTATING    = 3,
        HEADSET_CURRENT_ACTION_SKETCHING   = 4,
        HEADSET_CURRENT_ACTION_CREATEANNOT = 5
    };

    /** \brief  The different pointing interaction technique supported by this application */
    enum VFVPointingIT
    {
        POINTING_NONE    = -1,
        POINTING_GOGO    = 0,
        POINTING_WIM     = 1,
        POINTING_WIM_RAY = 2,
        POINTING_MANUAL  = 3,
    };

    enum VFVHandedness
    {
        HANDEDNESS_LEFT  = 0,
        HANDEDNESS_RIGHT = 1,
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
        VFVMessageType type = NOTHING; /*!< The type of the message*/
        struct VFVDataInformation* curMsg = NULL; //Pointer to the current message

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
            struct VFVMoveInformation           translate;  /*!< Translate information*/
            struct VFVScaleInformation          scale;      /*!< Scale information*/
            struct VFVVisibilityDataset         visibility; /*!< Visibility information*/
            struct VFVStartAnnotation           startAnnotation;  /*!< Start an annotation information*/
            struct VFVAnchorAnnotation          anchorAnnotation; /*!< Anchor an annotation at a specific location*/
            struct VFVClearAnnotations          clearAnnotations; /*!< Clear all the annotations of a specific dataset*/
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
                        case NEXT_TRIAL:
                            noData = cpy.noData;
                            curMsg = &noData;
                            break;
                        case IDENT_TABLET:
                            identTablet = cpy.identTablet;
                            curMsg = &identTablet;
                            break;
                        case ADD_BINARY_DATASET:
                            binaryDataset = cpy.binaryDataset;
                            curMsg = &binaryDataset;
                            break;
                        case ADD_VTK_DATASET:
                            vtkDataset = cpy.vtkDataset;
                            curMsg = &vtkDataset;
                            break;
                        case ROTATE_DATASET:
                            rotate = cpy.rotate;
                            curMsg = &rotate;
                            break;
                        case UPDATE_HEADSET:
                            headset = cpy.headset;
                            curMsg = &headset;
                            break;
                        case ANNOTATION_DATA:
                            annotation = cpy.annotation;
                            curMsg = &annotation;
                            break;
                        case ANCHORING_DATA_SEGMENT:
                            defaultByteArray = cpy.defaultByteArray;
                            curMsg = &defaultByteArray;
                            break;
                        case ANCHORING_DATA_STATUS:
                            anchoringDataStatus = cpy.anchoringDataStatus;                            
                            curMsg = &anchoringDataStatus;
                            break;
                        case HEADSET_CURRENT_ACTION:
                            headsetCurrentAction = cpy.headsetCurrentAction;
                            curMsg = &headsetCurrentAction;
                            break;
                        case HEADSET_CURRENT_SUB_DATASET:
                            headsetCurrentSubDataset = cpy.headsetCurrentSubDataset;
                            curMsg = &headsetCurrentSubDataset;
                            break;
                        case TRANSLATE_DATASET:
                            translate = cpy.translate;
                            curMsg = &translate;
                            break;
                        case SCALE_DATASET:
                            scale = cpy.scale;
                            curMsg = &scale;
                            break;
                        case VISIBILITY_DATASET:
                            visibility = cpy.visibility;
                            curMsg = &visibility;
                            break;
                        case START_ANNOTATION:
                            startAnnotation = cpy.startAnnotation;
                            curMsg = &startAnnotation;
                            break;
                        case ANCHOR_ANNOTATION:
                            anchorAnnotation = cpy.anchorAnnotation;
                            curMsg = &anchorAnnotation;
                            break;
                        case CLEAR_ANNOTATIONS:
                            clearAnnotations = cpy.clearAnnotations;
                            curMsg = &clearAnnotations;
                            break;
                        default:
                            WARNING << "Type " << cpy.type << " not handled yet in the copy constructor " << std::endl;
                            break;
                    }
                }
            }
            return *this;
        }

        /* \brief  Set the type of the message. Because the message is an union, this permits to set the correct union variable
         * \param t the new message type
         * \return   wheter the changement succeed or not */
        bool setType(VFVMessageType t)
        {
            if(t < NOTHING || t > END_MESSAGE_TYPE)
            {
                WARNING << "Wrong type " << t << " sent\n";
                return false;
            }
            clear();
            type = t;
            switch(t)
            {
                case IDENT_HEADSET:
                case NEXT_TRIAL:
                    new (&noData) VFVNoDataInformation;
                    curMsg = &noData;
                    noData.type = t;
                    break;
                case IDENT_TABLET:
                    new (&identTablet) VFVIdentTabletInformation;
                    curMsg = &identTablet;
                    break;
                case ADD_BINARY_DATASET:
                    new (&binaryDataset) VFVBinaryDatasetInformation;
                    curMsg = &binaryDataset;
                    break;
                case ADD_VTK_DATASET:
                    new (&vtkDataset) VFVVTKDatasetInformation;
                    curMsg = &vtkDataset;
                    break;
                case ROTATE_DATASET:
                    new (&rotate) VFVRotationInformation;
                    curMsg = &rotate;
                    break;
                case UPDATE_HEADSET:
                    new (&headset) VFVUpdateHeadset;
                    curMsg = &headset;
                    break;
                case ANNOTATION_DATA:
                    new (&annotation) VFVAnnotation;
                    curMsg = &annotation;
                    break;
                case ANCHORING_DATA_SEGMENT:
                    new (&defaultByteArray) VFVDefaultByteArray;
                    curMsg = &defaultByteArray;
                    noData.type = ANCHORING_DATA_STATUS;
                    break;
                case ANCHORING_DATA_STATUS:
                    new (&anchoringDataStatus) VFVAnchoringDataStatus;
                    curMsg = &anchoringDataStatus;
                    break;
                case HEADSET_CURRENT_ACTION:
                    new (&headsetCurrentAction) VFVHeadsetCurrentAction;
                    curMsg = &headsetCurrentAction;
                    break;
                case HEADSET_CURRENT_SUB_DATASET:
                    new (&headsetCurrentSubDataset) VFVHeadsetCurrentSubDataset;
                    curMsg = &headsetCurrentSubDataset;
                    break;
                case TRANSLATE_DATASET:
                    new (&translate) VFVMoveInformation;
                    curMsg = &translate;
                    break;
                case SCALE_DATASET:
                    new (&scale) VFVScaleInformation;
                    curMsg = &scale;
                    break;
                case VISIBILITY_DATASET:
                    new (&visibility) VFVVisibilityDataset;
                    curMsg = &visibility;
                    break;
                case START_ANNOTATION:
                    new (&startAnnotation) VFVStartAnnotation;
                    curMsg = &startAnnotation;
                    break;
                case ANCHOR_ANNOTATION:
                    new(&anchorAnnotation) VFVAnchorAnnotation;
                    curMsg = &anchorAnnotation;
                    break;
                case CLEAR_ANNOTATIONS:
                    new(&clearAnnotations) VFVClearAnnotations;
                    curMsg = &clearAnnotations;
                    break;
                case NOTHING:
                    break;
                default:
                    WARNING << "Type " << type << " not handled yet " << std::endl;
                    return false;
            }
            return true;
        }

        /** \brief  Clear the union variable based on the type */
        void clear()
        {
            curMsg = NULL;
            switch(type)
            {
                case IDENT_HEADSET:
                case NEXT_TRIAL:
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
                case TRANSLATE_DATASET:
                    translate.~VFVMoveInformation();
                    break;
                case SCALE_DATASET:
                    scale.~VFVScaleInformation();
                    break;
                case VISIBILITY_DATASET:
                    visibility.~VFVVisibilityDataset();
                    break;
                case START_ANNOTATION:
                    startAnnotation.~VFVStartAnnotation();
                    break;
                case ANCHOR_ANNOTATION:
                    anchorAnnotation.~VFVAnchorAnnotation();
                    break;
                case CLEAR_ANNOTATIONS:
                    clearAnnotations.~VFVClearAnnotations();
                    break;
                case NOTHING:
                    break;
                default:
                    WARNING << "Type " << type << " not handled yet in the destructor " << std::endl;
                    break;
            }
        }

        ~VFVMessage()
        {
            clear();
        }
    };

    /** \brief  Tablet data structur */
    struct VFVTabletData
    {
        SOCKADDR_IN      headsetAddr;    /*!< What is the address of the headset bound to this tablet?*/
        VFVClientSocket* headset = NULL; /*!< WHat is the headset bound to this tablet?*/
        int              number = 0;     /*!< The device number. This is defined by the device itself*/
        VFVHandedness    handedness = HANDEDNESS_RIGHT; /*!< The user's handedness (left or right)*/
    };

    /** \brief  The Pointing data of a headset (what pointing action and relative data is the user doing?) */
    struct VFVHeadsetPointingData
    {
        VFVPointingIT pointingIT       = POINTING_NONE; /*!< The pointing interaction technique in use*/
        int32_t       datasetID        = -1;            /*!< The dataset the user is manipulating*/
        int32_t       subDatasetID     = -1;            /*!< The subdataset the user is manipulating*/
        bool          pointingInPublic = true;          /*!< Is the user manipulating the dataset in the public space?*/
        glm::vec3     localSDPosition;                  /*!< The pointing position in the local subdataset space*/
        glm::vec3     headsetStartPosition;             /*!< The headset starting position when the pointing interaction technique started*/
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
        std::map<SubDataset*, SubDatasetHeadsetInformation> sdInfo;                 /*!< Information per dataset regarding each headset*/

        VFVHeadsetPointingData      pointingData; /*!< The pointing data of the headset*/
    };

    /* \brief VFVClientSocket class. Represent a Client for VFV Application */
    class VFVClientSocket : public ClientSocket
    {
        public:
            /* \brief Constructor*/
            VFVClientSocket();

            ~VFVClientSocket();

            bool feedMessage(uint8_t* message, uint32_t size);

            /* \brief Set the client as tablet
             * \param headset IP the headset IP 
             * \param handedness the tablet's handedness*/
            bool setAsTablet(const std::string& headsetIP, VFVHandedness handedness);

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
