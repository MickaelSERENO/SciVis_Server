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
#include "utils.h"
#include "ClientSocket.h"
#include "ColorMode.h"
#include "VFVDataInformation.h"
#include "VFVBufferValue.h"
#include "Types/ServerType.h"
#include "writeData.h"
#include "Quaternion.h"
#include "VolumetricSelection.h"
#include "Triangulor.h"

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
        NOTHING                                = -1,
        IDENT_HEADSET                          = 0,
        IDENT_TABLET                           = 1,
        ADD_BINARY_DATASET                     = 2,
        ADD_VTK_DATASET                        = 3,
        ROTATE_DATASET                         = 4,
        UPDATE_HEADSET                         = 5,
        ANNOTATION_DATA                        = 6,
        ANCHORING_DATA_SEGMENT                 = 7,
        ANCHORING_DATA_STATUS                  = 8,
        HEADSET_CURRENT_ACTION                 = 9,
        HEADSET_CURRENT_SUB_DATASET            = 10,
        TRANSLATE_DATASET                      = 11,
        SCALE_DATASET                          = 12,
        TF_DATASET                             = 13,
        START_ANNOTATION                       = 14,
        ANCHOR_ANNOTATION                      = 15,
        CLEAR_ANNOTATIONS                      = 16,
        ADD_SUBDATASET                         = 17,
        REMOVE_SUBDATASET                      = 18,
        MAKE_SUBDATASET_PUBLIC                 = 19,
        DUPLICATE_SUBDATASET                   = 20,
        LOCATION                               = 21,
        TABLETSCALE                            = 22,
        LASSO                                  = 23,
        CONFIRM_SELECTION                      = 24,
        ADD_CLOUD_POINT_DATASET                = 25,
        ADD_NEW_SELECTION_INPUT                = 26,
        TOGGLE_MAP_VISIBILITY                  = 27,
        MERGE_SUBDATASETS                      = 28,
        RESET_VOLUMETRIC_SELECTION             = 29,
        ADD_LOG_DATA                           = 30,
        ADD_ANNOTATION_POSITION                = 31,
        SET_ANNOTATION_POSITION_INDEXES        = 32,
        ADD_ANNOTATION_POSITION_TO_SD          = 33,
        SET_SUBDATASET_CLIPPING                = 34,
        SET_DRAWABLE_ANNOTATION_POSITION_COLOR = 35,
        SET_DRAWABLE_ANNOTATION_POSITION_IDX   = 36,
        ADD_SV_GROUP                           = 37,
        SET_SV_STACKED_GROUP_GLOBAL_PARAMETERS = 38,
        REMOVE_SD_GROUP                        = 39,
        ADD_CLIENT_TO_SV_GROUP                 = 40,
        RENAME_SUBDATASET                      = 41,
        SAVE_SUBDATASET_VISUAL                 = 42,
        VOLUMETRIC_SELECTION_METHOD            = 43,
        END_MESSAGE_TYPE
    };

    /** \brief Enumeration of the client current action */
    enum VFVHeadsetCurrentActionType
    {
        HEADSET_CURRENT_ACTION_NOTHING             = 0,
        HEADSET_CURRENT_ACTION_MOVING              = 1,
        HEADSET_CURRENT_ACTION_SCALING             = 2,
        HEADSET_CURRENT_ACTION_ROTATING            = 3,
        HEADSET_CURRENT_ACTION_SKETCHING           = 4,
        HEADSET_CURRENT_ACTION_CREATEANNOT         = 5,
        HEADSET_CURRENT_ACTION_LASSO               = 6,
        HEADSET_CURRENT_ACTION_SELECTING           = 7,
        HEADSET_CURRENT_ACTION_REVIEWING_SELECTION = 8
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

    /** \brief  The different handedness handled by this application */
    enum VFVHandedness
    {
        HANDEDNESS_LEFT  = 0,
        HANDEDNESS_RIGHT = 1,
    };

    enum VolumetricSelectionMethod
    {
        VOLUMETRIC_SELECTION_TANGIBLE = 0,
        VOLUMETRIC_SELECTION_FROM_TOP = 1,
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
            struct VFVNoDataInformation                         noData;        /*!< Structure for message with no internal data*/
            struct VFVIdentTabletInformation                    identTablet;   /*!< Ident information of a tablet*/
            struct VFVBinaryDatasetInformation                  binaryDataset; /*!< Binary Dataset information*/
            struct VFVVTKDatasetInformation                     vtkDataset;    /*!< Binary Dataset information*/
            struct VFVRotationInformation                       rotate;        /*!< The rotate information sent from a tablet*/
            struct VFVUpdateHeadset                             headset;       /*!< The headset update data information*/
            struct VFVAnnotation                                annotation;    /*!< The annotation data information*/
            struct VFVDefaultByteArray                          defaultByteArray;    /*!< Default byte array information*/
            struct VFVAnchoringDataStatus                       anchoringDataStatus; /*!< Anchoring data status*/
            struct VFVHeadsetCurrentAction                      headsetCurrentAction;     /*!< The headset current action*/
            struct VFVHeadsetCurrentSubDataset                  headsetCurrentSubDataset; /*!< The headset current SubDataset*/
            struct VFVMoveInformation                           translate;  /*!< Translate information*/
            struct VFVScaleInformation                          scale;      /*!< Scale information*/
            struct VFVTransferFunctionSubDataset                tfSD;       /*!< Transfer Function information for a SubDataset*/          
            struct VFVStartAnnotation                           startAnnotation;  /*!< Start an annotation information*/
            struct VFVAnchorAnnotation                          anchorAnnotation; /*!< Anchor an annotation at a specific location*/
            struct VFVClearAnnotations                          clearAnnotations; /*!< Clear all the annotations of a specific dataset*/
            struct VFVAddSubDataset                             addSubDataset;    /*!< Add a new default SubDataset*/
            struct VFVRemoveSubDataset                          removeSubDataset; /*!< Remove a registered SubDataset*/
            struct VFVDuplicateSubDataset                       duplicateSubDataset; /*!< Duplicate a registered SubDataset*/
            struct VFVMakeSubDatasetPublic                      makeSubDatasetPublic; /*!< Make a SubDataset public*/
            struct VFVLocation                                  location;         /*!< Update the tablet's virtual location*/
            struct VFVTabletScale                               tabletScale;      /*!< Tablet scale*/
            struct VFVLasso                                     lasso;            /*!< Lasso data*/
            struct VFVConfirmSelection                          confirmSelection; /*!< Confirm selection*/
            struct VFVCloudPointDatasetInformation              cloudPointDataset;  /*!< Cloud point dataset message*/
            struct VFVAddNewSelectionInput                      addNewSelectionInput; /*!< Start a new selection input*/
            struct VFVToggleMapVisibility                       toggleMapVisibility;  /*!< Toggle the map visibility*/
            struct VFVMergeSubDatasets                          mergeSubDatasets;     /*!< Merge two subdatasets visuals into one*/
            struct VFVResetVolumetricSelection                  resetVolumetricSelection; /*!< Reset the volumetric selection */
            struct VFVOpenLogData                               addLogData;               /*!< Open a log data annotation*/
            struct VFVAddAnnotationPosition                     addAnnotPos;              /*!< Add and link an annotation position object*/
            struct VFVSetAnnotationPositionIndexes              setAnnotPosIndexes;       /*!< Set the annot position database (indexes==headers)*/
            struct VFVAddAnnotationPositionToSD                 addAnnotPosToSD;          /*!< Add an annotation position to an already registered SubDataset*/
            struct VFVSetSubDatasetClipping                     setSDClipping;            /*!< Set the subdataset clipping values*/
            struct VFVSetDrawableAnnotationPositionDefaultColor setDrawableAnnotPosColor; /*!< Set the default drawable annotation position*/
            struct VFVSetDrawableAnnotationPositionMappedIdx    setDrawableAnnotPosIdx;   /*!< Set the data entries to read from the log that this drawable belongs to*/
            struct VFVAddSubjectiveViewGroup                    addSVGroup;               /*!< Add a subjective view group*/
            struct VFVRemoveSubDatasetGroup                     removeSDGroup;            /*!< Remove a subdataset group*/
            struct VFVSetSVStackedGroupGlobalParameters         setSVStackedGroupParams;  /*!< Set all the global parameters of a subjective view stacked froup*/
            struct VFVAddClientToSVGroup                        addClientToSVGroup;       /*!< Add a client to the subjective view group*/
            struct VFVRenameSubDataset                          renameSD;                 /*!< Rename a subdataset*/
            struct VFVSaveSubDatasetVisual                      saveSDVisual;             /*!< Save on disk the image of the subdataset*/
            struct VFVVolumetricSelectionMethod                          volumetricSelectionMethod; /*!< Select along the z axis*/
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
                        case TF_DATASET:
                            tfSD = cpy.tfSD;
                            curMsg = &tfSD;
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
                        case ADD_SUBDATASET:
                            addSubDataset = cpy.addSubDataset;
                            curMsg = &addSubDataset;
                            break;
                        case REMOVE_SUBDATASET:
                            removeSubDataset = cpy.removeSubDataset;
                            curMsg = &removeSubDataset;
                            break;
                        case MAKE_SUBDATASET_PUBLIC:
                            makeSubDatasetPublic = cpy.makeSubDatasetPublic;
                            curMsg = &makeSubDatasetPublic;
                            break;
                        case DUPLICATE_SUBDATASET:
                            duplicateSubDataset = cpy.duplicateSubDataset;
                            curMsg = &duplicateSubDataset;
                            break;
                        case LOCATION:
                            location = cpy.location;
                            curMsg = &location;
                            break;
                        case TABLETSCALE:
                            tabletScale = cpy.tabletScale;
                            curMsg = &tabletScale;
                            break;
                        case LASSO:
                            lasso = cpy.lasso;
                            curMsg = &lasso;
                            break;
                        case CONFIRM_SELECTION:
                            confirmSelection = cpy.confirmSelection;
                            curMsg = &confirmSelection;
                            break;
                        case ADD_CLOUD_POINT_DATASET:
                            cloudPointDataset = cpy.cloudPointDataset;
                            curMsg = &cloudPointDataset;
                            break;
                        case ADD_NEW_SELECTION_INPUT:
                            addNewSelectionInput = cpy.addNewSelectionInput;
                            curMsg = &addNewSelectionInput;
                            break;
                        case TOGGLE_MAP_VISIBILITY:
                            toggleMapVisibility = cpy.toggleMapVisibility;
                            curMsg = &toggleMapVisibility;
                            break;
                        case MERGE_SUBDATASETS:
                            mergeSubDatasets = cpy.mergeSubDatasets;
                            curMsg = &mergeSubDatasets;
                            break;
                        case RESET_VOLUMETRIC_SELECTION:
                            resetVolumetricSelection = cpy.resetVolumetricSelection;
                            curMsg = &resetVolumetricSelection;
                            break;
                        case ADD_LOG_DATA:
                            addLogData = cpy.addLogData;
                            curMsg = &addLogData;
                            break;
                        case ADD_ANNOTATION_POSITION:
                            addAnnotPos = cpy.addAnnotPos;
                            curMsg = &addAnnotPos;
                            break;
                        case SET_ANNOTATION_POSITION_INDEXES:
                            setAnnotPosIndexes = cpy.setAnnotPosIndexes;
                            curMsg = &setAnnotPosIndexes;
                            break;
                        case ADD_ANNOTATION_POSITION_TO_SD:
                            addAnnotPosToSD = cpy.addAnnotPosToSD;
                            curMsg = &addAnnotPosToSD;
                            break;
                        case SET_SUBDATASET_CLIPPING:
                            setSDClipping = cpy.setSDClipping;
                            curMsg = &setSDClipping;
                            break;
                        case SET_DRAWABLE_ANNOTATION_POSITION_COLOR:
                            setDrawableAnnotPosColor = cpy.setDrawableAnnotPosColor;
                            curMsg = &setDrawableAnnotPosColor;
                            break;
                        case SET_DRAWABLE_ANNOTATION_POSITION_IDX:
                            setDrawableAnnotPosIdx = cpy.setDrawableAnnotPosIdx;
                            curMsg = &setDrawableAnnotPosIdx;
                            break;
                        case ADD_SV_GROUP:
                            addSVGroup = cpy.addSVGroup;
                            curMsg = &addSVGroup;
                        	break;
                        case SET_SV_STACKED_GROUP_GLOBAL_PARAMETERS:
                            setSVStackedGroupParams = cpy.setSVStackedGroupParams;
                            curMsg = &setSVStackedGroupParams;
                        	break;
                        case REMOVE_SD_GROUP:
                            removeSDGroup = cpy.removeSDGroup;
                            curMsg = &removeSDGroup;
                        	break;
                        case ADD_CLIENT_TO_SV_GROUP:
                            addClientToSVGroup = cpy.addClientToSVGroup;
                            curMsg = &addClientToSVGroup;
                        	break;
                        case RENAME_SUBDATASET:
                            renameSD = cpy.renameSD;
                            curMsg = &renameSD;
                            break;
                        case SAVE_SUBDATASET_VISUAL:
                            saveSDVisual = cpy.saveSDVisual;
                            curMsg = &saveSDVisual;
                            break;
                        case VOLUMETRIC_SELECTION_METHOD:
                            volumetricSelectionMethod = cpy.volumetricSelectionMethod;
                            curMsg = &volumetricSelectionMethod;
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
                case TF_DATASET:
                    new (&tfSD) VFVTransferFunctionSubDataset;
                    curMsg = &tfSD;
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
                case ADD_SUBDATASET:
                    new(&addSubDataset) VFVAddSubDataset;
                    curMsg = &addSubDataset;
                    break;
                case REMOVE_SUBDATASET:
                    new(&removeSubDataset) VFVRemoveSubDataset;
                    curMsg = &removeSubDataset;
                    break;
                case MAKE_SUBDATASET_PUBLIC:
                    new(&makeSubDatasetPublic) VFVMakeSubDatasetPublic;
                    curMsg = &makeSubDatasetPublic;
                    break;
                case DUPLICATE_SUBDATASET:
                    new(&duplicateSubDataset) VFVDuplicateSubDataset;
                    curMsg = &duplicateSubDataset;
                    break;
                case LOCATION:
                    new(&location) VFVLocation;
                    curMsg = &location;
                    break;
                case TABLETSCALE:
                    new(&tabletScale) VFVTabletScale;
                    curMsg = &tabletScale;
                    break;
                case LASSO:
                    new(&lasso) VFVLasso;
                    curMsg = &lasso;
                    break;
                case CONFIRM_SELECTION:
                    new(&confirmSelection) VFVConfirmSelection;
                    curMsg = &confirmSelection;
                    break;
                case ADD_CLOUD_POINT_DATASET:
                    new(&cloudPointDataset) VFVCloudPointDatasetInformation;
                    curMsg = &cloudPointDataset;
                    break;
                case ADD_NEW_SELECTION_INPUT:
                    new(&addNewSelectionInput) VFVAddNewSelectionInput;
                    curMsg = &addNewSelectionInput;
                    break;
                case TOGGLE_MAP_VISIBILITY:
                    new(&toggleMapVisibility) VFVToggleMapVisibility;
                    curMsg = &toggleMapVisibility;
                    break;
                case MERGE_SUBDATASETS:
                    new(&mergeSubDatasets) VFVMergeSubDatasets;
                    curMsg = &mergeSubDatasets;
                    break;
                case RESET_VOLUMETRIC_SELECTION:
                    new(&resetVolumetricSelection) VFVResetVolumetricSelection;
                    curMsg = &resetVolumetricSelection;
                    break;
                case ADD_LOG_DATA:
                    new(&addLogData) VFVOpenLogData;
                    curMsg = &addLogData;
                    break;
                case ADD_ANNOTATION_POSITION:
                    new(&addAnnotPos) VFVAddAnnotationPosition;
                    curMsg = &addAnnotPos;
                    break;
                case SET_ANNOTATION_POSITION_INDEXES:
                    new(&setAnnotPosIndexes) VFVSetAnnotationPositionIndexes;
                    curMsg = &setAnnotPosIndexes;
                    break;
                case ADD_ANNOTATION_POSITION_TO_SD:
                    new(&addAnnotPosToSD) VFVAddAnnotationPositionToSD;
                    curMsg = &addAnnotPosToSD;
                    break;
                case SET_SUBDATASET_CLIPPING:
                    new(&setSDClipping) VFVSetSubDatasetClipping;
                    curMsg = &setSDClipping;
                    break;
                case SET_DRAWABLE_ANNOTATION_POSITION_COLOR:
                    new(&setDrawableAnnotPosColor) VFVSetDrawableAnnotationPositionDefaultColor;
                    curMsg = &setDrawableAnnotPosColor;
                    break;
                case SET_DRAWABLE_ANNOTATION_POSITION_IDX:
                    new(&setDrawableAnnotPosIdx) VFVSetDrawableAnnotationPositionMappedIdx;
                    curMsg = &setDrawableAnnotPosIdx;
                    break;
                case ADD_SV_GROUP:
                    new(&addSVGroup) VFVAddSubjectiveViewGroup;
                    curMsg = &addSVGroup;
                    break;
                case SET_SV_STACKED_GROUP_GLOBAL_PARAMETERS:
                    new(&setSVStackedGroupParams) VFVSetSVStackedGroupGlobalParameters;
                    curMsg = &setSVStackedGroupParams;
                    break;
                case REMOVE_SD_GROUP:
                    new(&removeSDGroup) VFVRemoveSubDatasetGroup;
                    curMsg = &removeSDGroup;
                    break;
                case ADD_CLIENT_TO_SV_GROUP:
                    new(&addClientToSVGroup) VFVAddClientToSVGroup;
                    curMsg = &addClientToSVGroup;
                    break;
                case RENAME_SUBDATASET:
                    new(&renameSD) VFVRenameSubDataset;
                    curMsg = &renameSD;
                    break;
                case SAVE_SUBDATASET_VISUAL:
                    new(&saveSDVisual) VFVSaveSubDatasetVisual;
                    curMsg = &saveSDVisual;
                    break;
                case VOLUMETRIC_SELECTION_METHOD:
                    new(&volumetricSelectionMethod) VFVVolumetricSelectionMethod;
                    curMsg = &volumetricSelectionMethod;
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
                case TF_DATASET:
                    tfSD.~VFVTransferFunctionSubDataset();
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
                case ADD_SUBDATASET:
                    addSubDataset.~VFVAddSubDataset();
                    break;
                case REMOVE_SUBDATASET:
                    removeSubDataset.~VFVRemoveSubDataset();
                    break;
                case MAKE_SUBDATASET_PUBLIC:
                    makeSubDatasetPublic.~VFVMakeSubDatasetPublic();
                    break;
                case DUPLICATE_SUBDATASET:
                    duplicateSubDataset.~VFVDuplicateSubDataset();
                    break;
                case LOCATION:
                    location.~VFVLocation();
                    break;
                case TABLETSCALE:
                    tabletScale.~VFVTabletScale();
                    break;
                case LASSO:
                    lasso.~VFVLasso();
                    break;
                case CONFIRM_SELECTION:
                    confirmSelection.~VFVConfirmSelection();
                    break;
                case ADD_CLOUD_POINT_DATASET:
                    cloudPointDataset.~VFVCloudPointDatasetInformation();
                    break;
                case ADD_NEW_SELECTION_INPUT:
                    addNewSelectionInput.~VFVAddNewSelectionInput();
                    break;
                case TOGGLE_MAP_VISIBILITY:
                    toggleMapVisibility.~VFVToggleMapVisibility();
                    break;
                case MERGE_SUBDATASETS:
                    mergeSubDatasets.~VFVMergeSubDatasets();
                    break;
                case RESET_VOLUMETRIC_SELECTION:
                    resetVolumetricSelection.~VFVResetVolumetricSelection();
                    break;
                case ADD_LOG_DATA:
                    addLogData.~VFVOpenLogData();
                    break;
                case ADD_ANNOTATION_POSITION:
                    addAnnotPos.~VFVAddAnnotationPosition();
                    break;
                case SET_ANNOTATION_POSITION_INDEXES:
                    setAnnotPosIndexes.~VFVSetAnnotationPositionIndexes();
                    break;
                case ADD_ANNOTATION_POSITION_TO_SD:
                    addAnnotPosToSD.~VFVAddAnnotationPositionToSD();
                    break;
                case SET_SUBDATASET_CLIPPING:
                    setSDClipping.~VFVSetSubDatasetClipping();
                    break;
                case SET_DRAWABLE_ANNOTATION_POSITION_COLOR:
                    setDrawableAnnotPosColor.~VFVSetDrawableAnnotationPositionDefaultColor();
                    break;
                case SET_DRAWABLE_ANNOTATION_POSITION_IDX:
                    setDrawableAnnotPosIdx.~VFVSetDrawableAnnotationPositionMappedIdx();
                    break;
                case ADD_SV_GROUP:
                    addSVGroup.~VFVAddSubjectiveViewGroup();
                	break;
                case SET_SV_STACKED_GROUP_GLOBAL_PARAMETERS:
                    setSVStackedGroupParams.~VFVSetSVStackedGroupGlobalParameters();
                	break;
                case REMOVE_SD_GROUP:
                    removeSDGroup.~VFVRemoveSubDatasetGroup();
                	break;
                case ADD_CLIENT_TO_SV_GROUP:
                    addClientToSVGroup.~VFVAddClientToSVGroup();
                	break;
                case RENAME_SUBDATASET:
                    renameSD.~VFVRenameSubDataset();
                    break;
                case SAVE_SUBDATASET_VISUAL:
                    saveSDVisual.~VFVSaveSubDatasetVisual();
                    break;
                case VOLUMETRIC_SELECTION_METHOD:
                    volumetricSelectionMethod.~VFVVolumetricSelectionMethod();
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
        VolumetricSelectionMethod volSelMethod = VOLUMETRIC_SELECTION_TANGIBLE;
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
        Quaternionf   headsetStartOrientation;          /*!< The headset starting orientation when the pointing interaction technique started*/
    };

    /** \brief  The lasso position of the tablet */
    struct VFVLassoPosition
    {
        glm::vec3   position; /*!< The position*/
        Quaternionf rotation; /*!< The rotation*/
    };

    /** \brief  The tangible brush mesh data */
    struct VFVTangibleBrushMesh : public VolumetricMesh
    {
        std::vector<glm::vec2> lasso; /*!< The lasso associated to this mesh*/
        bool isClosed = false; /*!< Is this mesh closed?*/

        VFVTangibleBrushMesh(const std::vector<glm::vec2>& _lasso, BooleanSelectionOp _op = SELECTION_OP_NONE) : VolumetricMesh(_op), lasso(_lasso) {};

        /** \brief  Close this mesh */
        void close();
    };

    /** \brief  The volumetric data of the associated headset */
    struct VFVVolumetricData
    {
        std::vector<glm::vec2>            lasso;      /*!< The drawn lasso*/
        VFVLassoPosition                  lassoPos;   /*!< The current lasso position*/
        glm::vec3                         lassoScale; /*!< The current lasso scale factor*/
        std::vector<VFVTangibleBrushMesh> meshes;     /*!< The volumetric meshes created by extrusion*/

        /** \brief  Close the mesh <mesh> using the lasso information
         * \param mesh the mesh to close */
        void closeMesh(VFVTangibleBrushMesh& mesh);

        /** \brief  Close the current mesh we are working on */
        void closeCurrentMesh();

        /** \brief  Push a new mesh to consider for the volumetric selection
         * \param op the boolean operation to apply for this mesh */
        void pushMesh(BooleanSelectionOp op);

        /** \brief  Push a new effective location of the multi-touch tablet to extrude the current mesh
         * \param loc the new position of the associated multi-touch tablet */
        void pushLocation(const VFVLassoPosition& loc);
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

        VFVHeadsetPointingData      pointingData; /*!< The pointing data of the headset*/

        VFVVolumetricData           volumetricData; /*!< Current spatial selection data*/

        /** \brief  Set the current action of this headset
         * \param action the current action of the headset */
        void setCurrentAction(VFVHeadsetCurrentActionType action);

        /** \brief  Is this headset performing a volumetric selection?
         * \return  true if yes, false otherwise */
        bool isInVolumetricSelection()
        {
            return currentAction == HEADSET_CURRENT_ACTION_LASSO || currentAction == HEADSET_CURRENT_ACTION_SELECTING || currentAction == HEADSET_CURRENT_ACTION_REVIEWING_SELECTION;
        }
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

            /* \brief  Set the VRPN registered position
             * \param pos the new VRPN position */
            void setVRPNPosition(const glm::vec3& pos) {m_vrpnPos = pos;}

            /* \brief  Set the VRPN registered rotation
             * \param rot the new VRPN rotation */
            void setVRPNRotation(const Quaternionf& rot) {m_vrpnRot = rot;}

            /* \brief  Get the VRPN registered position
             * \return  The VRPN registered position */
            const glm::vec3&   getVRPNPosition() const {return m_vrpnPos;}

            /* \brief  Get the VRPN registered rotation
             * \return  The VRPN registered rotation */
            const Quaternionf& getVRPNRotation() const {return m_vrpnRot;}
        private:
            static uint32_t nextHeadsetID;

            std::queue<VFVMessage> m_messages; /*!< List of messages parsed*/
            VFVMessage             m_curMsg;   /*!< The current in read message*/
            int32_t                m_cursor;   /*!< Indice cursor (what information ID are we reading at ?)*/

            VFVIdentityType        m_identityType = NO_IDENT; /*!< The type of the client (Tablet or headset ?)*/

            glm::vec3   m_vrpnPos; /*!< The VRPN position*/
            Quaternionf m_vrpnRot; /*!< The VRPN rotation*/
            union
            {
                VFVTabletData  m_tablet;  /*!< The client is considered a tablet*/
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
