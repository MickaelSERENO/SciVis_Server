#ifndef  VFVSERVER_INC
#define  VFVSERVER_INC

#include <map>
#include <string>
#include <stack>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/time.h>
#include <float.h>
#include "Server.h"
#include "VFVClientSocket.h"
#include "Datasets/VectorFieldDataset.h"
#include "Datasets/Annotation/Annotation.h"
#include "MetaData.h"
#include "AnchorHeadsetData.h"
#include "config.h"

#define VFVSERVER_ANNOTATION_NOT_FOUND(_annotID)\
    {\
        WARNING << "Annotation ID " << (_annotID) << " not found... disconnecting the client!"; \
    }

namespace sereno
{
    /* \brief  Does a string ends with another string value?
     *
     * \param value the string value to evaluate
     * \param ending the value to check on "value"
     *
     * \return true if "ending" ends "value", false otherwise */
    inline bool endsWith(std::string const& value, std::string const& ending)
    {
        if (ending.size() > value.size()) return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    /** \brief Enum describing what the server can push */
    enum VFVSendData
    {
        VFV_SEND_ADD_VTK_DATASET                                = 0,  /*!< Send an ADD VTK event*/
        VFV_SEND_ACKNOWLEDGE_ADD_DATASET                        = 1,  /*!< Acknowledge an event "add vtk"*/
        VFV_SEND_ROTATE_DATASET                                 = 2,  /*!< Send the rotation status of a dataset*/
        VFV_SEND_MOVE_DATASET                                   = 3,  /*!< Send the position status of a dataset*/
        VFV_SEND_HEADSET_BINDING_INFO                           = 4,  /*!< Send the binding headset information*/
        VFV_SEND_HEADSETS_STATUS                                = 5,  /*!< Send all the headsets status except the client receiving the message*/
        VFV_SEND_HEADSET_ANCHOR_SEGMENT                         = 6,  /*!< Send anchor segment*/
        VFV_SEND_HEADSET_ANCHOR_EOF                             = 7,  /*!< Send anchor end of stream*/
        VFV_SEND_SUBDATASET_LOCK_OWNER                          = 8,  /*!< Send the new subdataset lock owner*/
        VFV_SEND_SCALE_DATASET                                  = 9,  /*!< Send the scaling status of a dataset*/
        VFV_SEND_TF_DATASET                                     = 10, /*!< Send the Transfer Function status of a dataset*/
        VFV_SEND_START_ANNOTATION                               = 11, /*!< Send the start annotation message (asking to start an annotation) */
        VFV_SEND_ANCHOR_ANNOTATION                              = 12, /*!< Send the achor annotation message (anchor an annotation in a dataset)*/
        VFV_SEND_CLEAR_ANNOTATION                               = 13, /*!< Send the clear annotations message (asking to clear all annotations in a specific subdataset) */
        VFV_SEND_ADD_SUBDATASET                                 = 14, /*!< Send the "add" subdataset command.*/
        VFV_SEND_DEL_SUBDATASET                                 = 15, /*!< Send the "delete" subdataset command.*/
        VFV_SEND_SUBDATASET_OWNER                               = 16, /*!< Send the new SubDataset owner command.*/
        VFV_SEND_CURRENT_ACTION                                 = 17, /*!< Send the current action command.*/
        VFV_SEND_LOCATION                                       = 18, /*!< Send the location to the tablet.*/
        VFV_SEND_TABLET_LOCATION                                = 19, /*!< Send the tablet's virtual location.*/
        VFV_SEND_TABLET_SCALE                                   = 20, /*!< Send the tablet's scale.*/
        VFV_SEND_LASSO                                          = 21, /*!< Send the lasso data.*/
        VFV_SEND_CONFIRM_SELECTION                              = 22, /*!< Confirm a selection.*/
        VFV_SEND_ADD_CLOUDPOINT_DATASET                         = 23, /*!< Add a cloud point type dataset*/
        VFV_SEND_ADD_NEW_SELECTION_INPUT                        = 24, /*!< Add a new selection input*/
        VFV_SEND_TOGGLE_MAP_VISIBILITY                          = 25, /*!< Toggle the map visibility of a given subdataset*/
        VFV_SEND_VOLUMETRIC_MASK                                = 26, /*!< Send SubDataset computed volumetric mask*/
        VFV_SEND_RESET_VOLUMETRIC_SELECTION                     = 27, /*!< Reset the volumetric selection of a particular subdataset*/
        VFV_SEND_ADD_LOG_DATASET                                = 28, /*!< Open a Log dataset (annotation. Format: CSV)*/
        VFV_SEND_ADD_ANNOTATION_POSITION                        = 29, /*!< Add an annotation position object to an Annotation Log Object*/
        VFV_SEND_SET_ANNOTATION_POSITION_INDEXES                = 30, /*!< Set the indexes of the annotation position object*/
        VFV_SEND_ADD_ANNOTATION_POSITION_TO_SD                  = 31, /*!< Link an annotation position object to a subdataset, creating a new drawable*/
        VFV_SEND_SET_SUBDATASET_CLIPPING                        = 32, /*!< Set the clipping values of a subdataset*/
        VFV_SEND_SET_DRAWABLE_ANNOTATION_POSITION_DEFAULT_COLOR = 33, /*!< Set the default color to use for drawable annotation position*/
        VFV_SEND_SET_DRAWABLE_ANNOTATION_POSITION_MAPPED_IDX    = 34, /*!< Set which columns to read to map the data to the visual channel for a drawable annotation position*/
        VFV_SEND_ADD_SUBJECTIVE_VIEW_GROUP                      = 35, /*!< Add a new subjective view group*/
        VFV_SEND_ADD_SD_TO_SV_STACKED_LINKED_GROUP              = 36, /*!< Add a sub dataset to an already registered subjective view stacked_linked group*/
        VFV_SEND_SET_SV_STACKED_GLOBAL_PARAMETERS               = 37, /*!< Set the parameters of a subjective view stacked group*/
        VFV_SEND_REMOVE_SUBDATASET_GROUP                        = 38, /*!< Remove a SubDataset Group*/
        VFV_SEND_RENAME_SD                                      = 39, /*!< Rename a SubDataset*/
        VFV_SEND_DISPLAY_SHORT_MESSAGE                          = 40, /*!< Display on the device a short message*/
        VFV_SEND_END,
    };

    /** \brief  The types of existing dataset this server handles */
    enum DatasetType
    {
        DATASET_TYPE_NOT_FOUND    = -1,
        DATASET_TYPE_VTK          = 0,
        DATASET_TYPE_VECTOR_FIELD = 1,
        DATASET_TYPE_CLOUD_POINT  = 2,
    };

    /** \brief  Clone a Transfer function based on its type
     * \param tf the transfer function to clone
     * \return the new Transfer Function allocated using new. The caller is responsible to destroy that object*/
    SubDatasetTFMetaData* cloneTransferFunction(std::shared_ptr<const SubDatasetTFMetaData> tf);

    /* \brief The Class Server for the Vector Field Visualization application */
    class VFVServer : public Server<VFVClientSocket>
    {
        public:
            VFVServer(uint32_t nbThread, uint32_t port);
            VFVServer(VFVServer&& mvt);
            ~VFVServer();

            bool launch();
            void cancel();
            void wait();
            void closeServer();

            /* \brief  Update the tablet Location in the debug mode (e.g., VUFORIA mode)
             * \param pos the tablet position
             * \param rot the tablet rotation */
            void updateLocationTabletDebug(const glm::vec3& pos, const Quaternionf& rot);

            /* \brief Update the tablet position but do not send it yet
             * \param pos the tablet position
             * \param rot the tablet rotation
             * \param tabletID the tablet ID to update */
            void pushTabletVRPNPosition(const glm::vec3& pos, const Quaternionf& rot, int tabletID);

            /** \brief  Update the headset position but do not send it yet
             * \param pos the headset position
             * \param rot the headset rotation
             * \param tabletID the tabletID bound to this headset */
            void pushHeadsetVRPNPosition(const glm::vec3& pos, const Quaternionf& rot, int tabletID);

            /** \brief  Commit and send to all the clients all the devices' positions */
            void commitAllVRPNPositions();

            /** \brief  The distinguishable color used in this sci vis application */
            static const uint32_t SCIVIS_DISTINGUISHABLE_COLORS[10];
        protected:

            /** \brief  Get the type of a given dataset if registered
             * \param dataset the dataset to evaluate
             * \return   the type of the dataset */
            DatasetType getDatasetType(const Dataset* dataset) const;

            template <typename T = AnnotationPosition>
            LogMetaData* getLogComponentMetaData(uint32_t annotID, uint32_t compID, AnnotationComponentMetaData<T>** compMT)
            {
                auto it = m_logData.find(annotID);
                if(it == m_logData.end())
                {
                    VFVSERVER_ANNOTATION_NOT_FOUND(annotID);
                    return nullptr;
                }

                if(compMT)
                    *compMT = it->second.getComponentMetaData<T>(compID);
                return &(it->second);
            }

            void closeClient(SOCKET client);

            /* \brief  Get the dataset via its ID
             * \param datasetID the dataset ID
             * \param sdID the subdataset ID to test the validity of the Dataset. can be 0.
             * \param sd a pointer to store the pointer of the corresponding subdataset. nullptr == parameter ignored.
             * \return  NULL if not found, a pointer to the dataset if found*/
            Dataset* getDataset(uint32_t datasetID, uint32_t sdID, SubDataset** sd = nullptr);

            /* \brief  Tells wether a given client can modify a subdataset or not
             * \param client the client attempting to modify the subdataset. If NULL, the client is considered here as the server (which can modify everything)
             * \param sdMT the subdataset meta data
             * \return  true if yes, false otherwise */
            bool canModifySubDataset(VFVClientSocket* client, SubDatasetMetaData* sdMT);

            /* \brief  Get the Dataset meta data via its ID
             * \param datasetID the dataset ID
             * \param sdID the subdataset ID
             * \param sdMTPtr[out] if not NULL, will contain the SubDatasetMetaData corresponding
             * \return The DatasetMetaData being updated. NULL if not found. In this case, sdMTPtr will not be modified*/
            DatasetMetaData* getMetaData(uint32_t datasetID, uint32_t sdID, SubDatasetMetaData** sdMTPtr = nullptr);

            /* \brief  Update the subdataset meta data last modification component via its ID
             * \param client the client modifying the metadata
             * \param datasetID the dataset ID
             * \param sdID the subdataset ID
             * \param sdMTPtr[out] if not NULL, will contain the SubDatasetMetaData corresponding
             * \return The DatasetMetaData being updated. NULL if not found*/
            DatasetMetaData* updateMetaDataModification(VFVClientSocket* client, uint32_t datasetID, uint32_t sdID, SubDatasetMetaData** sdMTPtr=NULL);

            /* \brief  Get the Headset ClientSocket object from a VFVClientSocket that can also be a tablet
             * \param client the client link to a headset. If client->isHeadset, returns client, otherwise returns client->getTabletData().headset
             * \return   the corresponding headset client object */
            VFVClientSocket* getHeadsetFromClient(VFVClientSocket* client);

            void updateSDGroup(SubDataset* sd, bool setTF = false);

            bool canClientModifySubDatasetGroup(VFVClientSocket* client, const SubDatasetGroupMetaData& sdg);

            /** \brief Get the dataset ID from an already registered dataset
             * \param dataset the dataset to evaluate
             * \return the dataset ID, or the maximum value of uint32_t (i.e., the unsigned value of -1=)*/
            uint32_t getDatasetID(Dataset* dataset);

            /* \brief  Ask for a new anchor headset */
            void askNewAnchor();

            /* \brief  Login the tablet "client"
             * \param client the client logged as a tablet
             * \param identTablet the message sent */
            void loginTablet(VFVClientSocket* client, const VFVIdentTabletInformation& identTablet);

            /* \brief  Login the headset "client"
             * \param client the client logged as a hololenz
             * \param identTablet the message sent */
            void loginHeadset(VFVClientSocket* client);

            /* \brief Handle the rotation
             * \param client the client asking for a rotation
             * \param rotate the rotation data. Not constant because the headset ID will change*/
            void rotateSubDataset(VFVClientSocket* client, VFVRotationInformation& rotate);

            /* \brief Handle the translation
             * \param client the client asking for a rotation
             * \param position the position of the data. Not constant because the headset ID will change*/
            void translateSubDataset(VFVClientSocket* client, VFVMoveInformation& position);

            /* \brief Handle the scaling
             * \param client the client asking for a rotation
             * \param scale the scale values of the data. Not constant because the headset ID will change*/
            void scaleSubDataset(VFVClientSocket* client, VFVScaleInformation& scale);

            /* \brief Handle the clipping
             * \param client the client asking for a set in the clipping*/
            void setSubDatasetClipping(VFVClientSocket* client, VFVSetSubDatasetClipping& clipping);

            /* \brief Handle the change of transfer function
             * \param client the client asking for a new transfer function
             * \param tfSD the transfer function data. Not constant because the headset ID will change */
            void tfSubDataset(VFVClientSocket* client, VFVTransferFunctionSubDataset& tfSD);

            /* \brief  Add a VTKDataset to the visualized datasets
             * \param client the client adding the dataset
             * \param dataset the dataset information to add */
            void addVTKDataset(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset);

            /* \brief  Add a CloudPointDataset to the visualized datasets
             * \param client the client adding the dataset
             * \param dataset the dataset information to add */
            void addCloudPointDataset(VFVClientSocket* client, const VFVCloudPointDatasetInformation& dataset);

            /* \brief  Add a SubDataset to a given one
             * \param client the client adding the dataset
             * \param dataset the dataset information to add
             *
             * \return the SubDataset added. NULL if error*/
            SubDataset* onAddSubDataset(VFVClientSocket* client, const VFVAddSubDataset& dataset);

            /* \brief  Remove a known subdataset
             * \param dataset the dataset ID information */
            void removeSubDataset(const VFVRemoveSubDataset& dataset);

            /* \brief  Add a Log data to the dataset objects
             * \param client the client adding the dataset
             * \param logData the information needed to add a log dataset*/
            void addLogData(VFVClientSocket* client, const VFVOpenLogData& logData);

            /** \brief  Add a new AnnotationPosition to an already opened annotation log data
             * \param client the client asking for the addition
             * \param pos the information needed to add an annotation position data*/
            void addAnnotationPosition(VFVClientSocket* client, const VFVAddAnnotationPosition& pos);

            /** \brief  Link an annotation position component to a SubDataset 
             * \param client the client asking for the linkage
             * \param pos the information needed to link the SubDataset and the AnnotationPosition */
            void addAnnotationPositionToSD(VFVClientSocket* client, const VFVAddAnnotationPositionToSD& pos);

            /** \brief  Set the reading indexes of an annotation position object
             * \param client the client asking for the change
             * \param idx the new indexes to use */
            void onSetAnnotationPositionIndexes(VFVClientSocket* client, const VFVSetAnnotationPositionIndexes& idx);

            /* \brief  Make public a known SubDataset
             * \param client the client making this subdataset public
             * \param dataset the dataset ID information */
            void onMakeSubDatasetPublic(VFVClientSocket* client, const VFVMakeSubDatasetPublic& dataset);

            /* \brief  Remove a known SubDataset
             * \param client the client asking to remove the subdataset
             * \param dataset the dataset information to remove */
            void onRemoveSubDataset(VFVClientSocket* client, const VFVRemoveSubDataset& dataset);

            /* \brief  Rename a known SubDataset
             * \param client the client asking to rename the subdataset
             * \param dataset the dataset information to rename */
            void onRenameSubDataset(VFVClientSocket* client, const VFVRenameSubDataset& dataset);

            /* \brief  Duplicate a known SubDataset
             * \param client the client asking to duplicate the subdataset
             * \param dataset the dataset information to duplicate */
            void onDuplicateSubDataset(VFVClientSocket* client, const VFVDuplicateSubDataset& dataset);

            /** \brief  Duplicate a known SubDataset. This function shall be called when mutexes are already locked.
             * \param client The client asking to duplicate the subdataset
             * \param dataset the information targetting the subdataset to duplicate
             *  \return a pointer to the added subdataset. NULL is returned if an error occured. The pointer must be used shortly, as any modification to the dataset arrays might change its address (realloc) */
            SubDatasetMetaData* duplicateSubDataset(VFVClientSocket* client, const VFVDuplicateSubDataset& dataset);

            /* \brief  Merge two known SubDatasets
             * \param client the client asking to merge the subdatasets
             * \param dataset the datasets information to merge */
            void onMergeSubDatasets(VFVClientSocket* client, const VFVMergeSubDatasets& merge);

            /* \brief  send a tablet's virtual location to the hololens
             * \param client the tablet sending the location
             * \param location the tablet's virtual location */
            void onLocation(VFVClientSocket* client, const VFVLocation& location);
            
            /* \brief  set a tablet's virtual scale to the hololens
             * \param client the tablet sending the scale
             * \param scale the tablet scale */
            void onTabletScale(VFVClientSocket* client, const VFVTabletScale& tabletScale);

            /* \brief  send a tablet's lasso to the hololens
             * \param client the tablet sending the lasso
             * \param lasso the lasso to send */
            void onLasso(VFVClientSocket* client, const VFVLasso& lasso);

            /** \brief  FUnction handling the add new selection input message
             * \param client the client sending the message
             * \param msg the message parsed */
            void onAddNewSelectionInput(VFVClientSocket* client, const VFVAddNewSelectionInput& addInput);
            
            /* \brief  confirm a tablet's selection
             * \param client the tablet confirming the selection
             * \param confirmSelection confirmation message */
            void onConfirmSelection(VFVClientSocket* client, const VFVConfirmSelection& confirmSelection);

            /* \brief  CHange the map visibility of a given subdataset
             * \param client the client asking to change the map visibility
             * \param mapVisibility IDs and visibility parameters*/
            void onToggleMapVisibility(VFVClientSocket* client, const VFVToggleMapVisibility& mapVisibility);

            /* \brief  Update the headset "client" into the server's internal data
             * \param client the client pushing the new values to update
             * \param headset the values to push */
            void updateHeadset(VFVClientSocket* client, const VFVUpdateHeadset& headset);

            /* \brief  Tells the headset bound to a tablet to start an annotation
             * \param client the tablet client
             * \param startAnnot the start annotation message */
            void onStartAnnotation(VFVClientSocket* client, const VFVStartAnnotation& startAnnot);

            /* \brief  Anchor an annotation in a specific subdataset
             * \param client the client sending the message
             * \param anchorAnnot the message parsed containing information to anchor a newly created annotation */
            void onAnchorAnnotation(VFVClientSocket* client, VFVAnchorAnnotation& anchorAnnot);

            /* \brief  Clear annotations in a specific subdataset
             * \param client the client sending the message
             * \param clearAnnots the message parsed containing information about the dataset to clear the annotations*/
            void onClearAnnotations(VFVClientSocket* client, const VFVClearAnnotations& clearAnnots);

            /** \brief  Handle the "on reset volumetric selection" asked by a particular client
             * \param client the client asking to reset the selection
             * \param reset the reset data*/
            void onResetVolumetricSelection(VFVClientSocket* client, const VFVResetVolumetricSelection& reset);

            /** \brief Handle the "set the default color of drawable annotation position".
             * \param client The client asking to set the default color
             * \param color the new default color data message to use*/
            void setDrawableAnnotationPositionColor(VFVClientSocket* client, const VFVSetDrawableAnnotationPositionDefaultColor& color);

            /** \brief Handle the "set the indexes of the data to use for drawable annotation position".
             * \param client The client asking to set the default color
             * \param idx the new indices data message to use*/
            void setDrawableAnnotationPositionIdx(VFVClientSocket* client, const VFVSetDrawableAnnotationPositionMappedIdx& idx);

            /** \brief  Handling the "add subjective view group" message.
             * \param client The client asking to add a new subjective view group
             * \param addSV The information concerning the subjective view*/
            void addSubjectiveViewGroup(VFVClientSocket* client, const VFVAddSubjectiveViewGroup& addSV);

            /** \brief  Handling the "remove subdataset group" message.
             * \param client The client asking to remove a registered group
             * \param removeSDGroup The information concerning the group to remove*/
            void onRemoveSubDatasetGroup(VFVClientSocket* client, const VFVRemoveSubDatasetGroup& removeSDGroup);

            /** \brief Remove a SubDataset Group. Here, we do not consider any clients and will not block any mutex
             * \param removeSDGroup The information concerning the group to remove*/
            void removeSubDatasetGroup(const VFVRemoveSubDatasetGroup& removeSDGroup);

            /** \brief  Set all the common parameters for subjective view stacked groups
             * \param client the client asking for the change
             * \param params the new parameters*/
            void setSubjectiveViewStackedParameters(VFVClientSocket* client, const VFVSetSVStackedGroupGlobalParameters& params);

            /** \brief  Add a client to the SV Group. This function serves multiple other ones that needs to lock, at different time, mutexes
             * \param client the client asking to add a new subjective view inside a given group
             * \param addClient the information required to target the SV Group*/
            void addClientToSVGroup(VFVClientSocket* client, const VFVAddClientToSVGroup& addClient);

            /** \brief  Add a client to the SV Group. This function locks all the required mutex then calls onAddClientToSVGroup
             * \param client the client asking to add a new subjective view inside a given group
             * \param addClient the information required to target the SV Group*/
            void onAddClientToSVGroup(VFVClientSocket* client, const VFVAddClientToSVGroup& addClient);

            /** \brief  Save the visual as parameterized by a SubDataset. The visual, depending on the SubDataset's type, is usually stored as 3D objects
             * \param client the client asking to save a specific SubDataset
             * \param saveSDVisual the information required to find the subdataset to save*/
            void onSaveSubDatasetVisual(VFVClientSocket* client, const VFVSaveSubDatasetVisual& saveSDVisual);

            void onSetVolumetricSelectionMethod(VFVClientSocket* client, const VFVVolumetricSelectionMethod& method);

            /** \brief Save into the current log file a VFVDataInformation message
             * \param client the client to which the message should have been sent to
             * \param data the data to save in a JSON format */
            void saveMessageSentToJSONLog(VFVClientSocket* client, const VFVDataInformation& data);

            /* \brief  Send an empty message
             * \param client the client to send the message
             * \param type the type of the message*/
            void sendEmptyMessage(VFVClientSocket* client, uint16_t type);

            /* \brief  Send the Add VTK Dataset Event to a given client
             * \param client the client to send the message
             * \param dataset the dataset information 
             * \param datasetID the datasetID*/
            void sendAddVTKDatasetEvent(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset, uint32_t datasetID);

            /* \brief  Send the Add Cloud Point Dataset Event to a given client
             * \param client the client to send the message
             * \param dataset the dataset information 
             * \param datasetID the datasetID*/
            void sendAddCloudPointDatasetEvent(VFVClientSocket* client, const VFVCloudPointDatasetInformation& dataset, uint32_t datasetID);

            /* \brief Send the AddSubDataset Event to a given client for a given SubDataset
             * \param client the client to send the message
             * \param SubDataset the subdataset information */
            void sendAddSubDataset(VFVClientSocket* client, const SubDataset* sd);

            /* \brief  Send the Remove SubDataset Event to a given client
             * \param client the client to send the message
             * \param dataset the dataset information*/
            void sendRemoveSubDatasetEvent(VFVClientSocket* client, const VFVRemoveSubDataset& dataset);

            /* \brief  Send the Rename SubDataset Event to a given client
             * \param client the client to send the message
             * \param dataset the dataset information*/
            void sendRenameSubDataset(VFVClientSocket* client, const VFVRenameSubDataset& dataset);

            /* \brief  Send the AddLogData event to a given client
             * \param client the client to send the message
             * \param logData the log information
             * \param logID the ID representing this data*/
            void sendAddLogData(VFVClientSocket* client, const VFVOpenLogData& logData, uint32_t logID);

            /** \brief  Send an "Add Annotation Position" event to a client
             * \param client the client to send the message to
             * \param posMT the AnnotationPosition meta data*/
            void sendAddAnnotationPositionData(VFVClientSocket* client, const AnnotationComponentMetaData<AnnotationPosition>& posMT);

            /** \brief  Send an "Set Annotation Position Indexes" (headers) event to a client
             * \param client the client to send the message to
             * \param posMT the AnnotationPosition meta data*/
            void sendSetAnnotationPositionIndexes(VFVClientSocket* client, const AnnotationComponentMetaData<AnnotationPosition>& posMT);

            /** \brief  Send an "Add Annotation Position To SubDataset" event to a client
             * \param client the client to send the message to
             * \param sdMT the subdataset meta data needed to connect it to "drawable"
             * \param drawable the drawable meta data*/ 
            void sendAddAnnotationPositionToSD(VFVClientSocket* client, const SubDatasetMetaData& sdMT, const DrawableAnnotationPositionMetaData& drawable);

            /* \brief  Send a rotation event to client
             * \param client the client to send the information
             * \param rotate the rotate information*/
            void sendRotateDatasetEvent(VFVClientSocket* client, const VFVRotationInformation& rotate);

            /* \brief  Send a scaling event to client
             * \param client the client to send the information
             * \param scale the scale information*/
            void sendScaleDatasetEvent(VFVClientSocket* client, const VFVScaleInformation& scale);

            /* \brief  Send a position event to client
             * \param client the client to send the information
             * \param position the position information */
            void sendMoveDatasetEvent(VFVClientSocket* client, const VFVMoveInformation& position);

            /* \brief  Send a clipping event to client
             * \param client the client to send the information
             * \param clipping the clipping information */
            void sendSubDatasetClippingEvent(VFVClientSocket* client, const VFVSetSubDatasetClipping& clipping);

            /* \brief  Send transfer function of a subdataset event to client
             * \param client the client to send the information
             * \param tfSD the transfer function information */
            void sendTransferFunctionDataset(VFVClientSocket* client, const VFVTransferFunctionSubDataset& tfSD);

            /** \brief  Send the volumetric mask to a client
             * \param client the client to send the information
             * \param data the volumetric data byte array. See generateVolumetricMaskEvent function
             * \param size the size of the data array*/
            void sendVolumetricMaskDataset(VFVClientSocket* client, std::shared_ptr<uint8_t> data, size_t size);

            /* \brief Send the current action message to a given client (will set what currently the headset is supposed to do)
             * \param client the client to send the information
             * \param currentActionID the action ID to send */
            void sendCurrentAction(VFVClientSocket* client, uint32_t currentActionID);

            /* \brief  Send the subdataset status to a client
             * \param client the client to send the data
             * \param sd the subdataset information
             * \param datasetID the dataset ID that this SubDataset is attached to*/
            void sendSubDatasetStatus(VFVClientSocket* client, SubDataset* sd, uint32_t datasetID);

            /* \brief  Send the subdataset positional status to a client
             * \param client the client to send the data
             * \param sd the subdataset information
             * \param datasetID the dataset ID that this SubDataset is attached to*/
            void sendSubDatasetPositionStatus(VFVClientSocket* client, SubDataset* sd, uint32_t datasetID);

            /* \brief  Send the whole dataset status to a client
             * \param client the client to send the data
             * \param dataset the dataset information
             * \param datasetID the dataset ID */
            void sendDatasetStatus(VFVClientSocket* client, Dataset* dataset, uint32_t datasetID);

            /* \brief  Send the drawable annotation position status to a client
             * \param client the client to send the data
             * \param sdMT the subdataset meta data information containing the drawable
             * \param drawableMT the drawable metadata*/
            void sendDrawableAnnotationPositionStatus(VFVClientSocket* client, const SubDatasetMetaData& sdMT, const DrawableAnnotationPositionMetaData& drawableMT);

            /* \brief  Send the annotation data
             * \param client the client to send the data
             * \param annot the annotation's data */
            void sendAnnotationData(VFVClientSocket* client, Annotation* annot);

            /* \brief Send the binding information to the tablet and headset about the binding information
             * \param client the client to send the data (tablet or headset)*/
            void sendHeadsetBindingInfo(VFVClientSocket* client);

            /** \brief  Send the anchoring data to he given client
             * \param client the client to send the anchoring*/
            void sendAnchoring(VFVClientSocket* client);

            /** \brief  Send the anchoring data to all the client connected */
            void sendAnchoring();

            /* \brief Send the subdataset lock owner to all the clients (owner included)
             * \param data SubDataset meta data containing the new lock owner */
            void sendSubDatasetLockOwner(SubDatasetMetaData* data);

            /* \brief Send the subdataset owner to all the clients (owner included)
             * \param data SubDataset meta data containing the new owner */
            void sendSubDatasetOwnerToAll(SubDatasetMetaData* metaData);

            /** \brief  Send the subdataset owner to a given client
             * \param client the client to send the message to
             * \param data the data to send */
            void sendSubDatasetOwner(VFVClientSocket* client, SubDatasetMetaData* data);

            /* \brief  Send a start annotation message to a headset
             * \param client the headset client to send the message
             * \param startAnnot the start annotation message to send */
            void sendStartAnnotation(VFVClientSocket* client, const VFVStartAnnotation& startAnnot);

            /* \brief  Send an anchor annotation message to a specific client
             * \param client the client to send the message
             * \param anchorAnnot the anchor annotation message data */
            void sendAnchorAnnotation(VFVClientSocket* client, const VFVAnchorAnnotation& anchorAnnot);

            /* \brief  Send the clear annotations command to a specific client
             * \param client the client to send the message
             * \param clearAnnot the clean annotations command */
            void sendClearAnnotations(VFVClientSocket* client, const VFVClearAnnotations& clearAnnot);

            /* \brief  Send location to all tablets
             * \param pos Position de la tablette
             * \param rot Rotation de la tablette
             * \param client the client to send the message (should be a tablet)*/
            void sendLocationTablet(const glm::vec3& pos, const Quaternionf& rot, VFVClientSocket* client);

            /** \brief  Send a addNewSelectionInput
             * \param client the client to send the message to
             * \param addInput the message data to send */
            void sendAddNewSelectionInput(VFVClientSocket* client, const VFVAddNewSelectionInput& addInput);

            /** \brief  Send a toggleMapVisibility message
             * \param client the client to send the message to
             * \param visibility the message data to send */
            void sendToggleMapVisibility(VFVClientSocket* client, const VFVToggleMapVisibility& visibility);

            /** \brief  Send the reset volumetric selection message
             * \param client the client to send the message to
             * \param datasetID the dataset ID to reset the selection
             * \param sdID the visualization subdataset ID to reset the visualization from
             * \param headsetID the headset ID asking to reset it */
            void sendResetVolumetricSelection(VFVClientSocket* client, int datasetID, int sdID, int headsetID);

            /** \brief  Send a setting of the default color of a drawable annotation position object
             * \param client the client to send the message to
             * \param color the data to send */
            void sendSetDrawableAnnotationPositionColor(VFVClientSocket* client, const VFVSetDrawableAnnotationPositionDefaultColor& color);

            /** \brief  Send a setting of the mapped indices from the annotation log a drawable annotation position object is linked to
             * \param client the client to send the message to
             * \param idx the data to send */
            void sendSetDrawableAnnotationPositionIdx(VFVClientSocket* client, const VFVSetDrawableAnnotationPositionMappedIdx& idx);

            /** \brief  Send a new subjective view group
             * \param client the client to send the message to
             * \param addSV the information of the subjective view group to add*/
            void sendAddSubjectiveViewGroup(VFVClientSocket* client, const VFVAddSubjectiveViewGroup& addSV);

            /** \brief  Send a new subjective view group
             * \param client the client to send the message to
             * \param datasetID the dataset ID being the same for both subdatasets to add
             * \param sdStackedID the subdataset ID to stack. It can be -1 if there is no need to add this subdataset
             * \param sdLinkedID the subdataset ID to link. It can be -1 if there is no need to add this subdataset*/
            void sendAddSubDatasetToSVStackedGroup(VFVClientSocket* client, SubDatasetGroupMetaData& sdgMD, uint32_t datasetID, uint32_t sdStackedID, uint32_t sdLinkedID);

            /** \brief  Set the global parameters of a Subjective views stacked group
             * \param client the client to send the message to
             * \param params the global parameters information */
            void sendSVStackedGroupGlobalParameters(VFVClientSocket* client, const VFVSetSVStackedGroupGlobalParameters& params);

            /** \brief Remove a subdataset group from the client data 
             * \param client the client to send the message to
             * \param removeSDGroup the subdataset group information */
            void sendRemoveSubDatasetsGroup(VFVClientSocket* client, const VFVRemoveSubDatasetGroup& removeSDGroup);

            /** \brief  Send a short message to display on the client device
             * \param client the client to send the message to
             * \param msg the message (string) to display on the device for a short amount of time */
            void sendMessageToDisplay(VFVClientSocket* client, const std::string& msg);

            /* \brief  Send the current status of the server on login
             * \param client the client to send the data */
            void onLoginSendCurrentStatus(VFVClientSocket* client);

            void onMessage(uint32_t bufID, VFVClientSocket* client, uint8_t* data, uint32_t size);

            /** \brief Main thread running for updating other devices*/
            void updateThread();

            /** \brief  Push a heavy computation function
             * \param f the function to call in a separate thread */
            void pushHeavy(const std::function<void(void)>& f);

            /** \brief  The thread running for heavy computation */
            void computeThread();

            /*----------------------------------------------------------------------------*/
            /*---------------------------------ATTRIBUTES---------------------------------*/
            /*----------------------------------------------------------------------------*/

            std::stack<uint32_t> m_availableHeadsetColors;       /*!< The available headset colors*/

            std::map<uint32_t, VectorFieldMetaData>     m_binaryDatasets;     /*!< The binary datasets opened*/
            std::map<uint32_t, VTKMetaData>             m_vtkDatasets;        /*!< The vtk datasets opened*/
            std::map<uint32_t, CloudPointMetaData>      m_cloudPointDatasets; /*!< The cloud point datasets opened*/
            std::map<uint32_t, Dataset*>                m_datasets;           /*!< The datasets opened*/
            std::map<uint32_t, LogMetaData>             m_logData;            /*!< The log data*/
            std::map<uint32_t, SubDatasetGroupMetaData> m_sdGroups;           /*!< The registered SubDatasetGroup opened*/

            std::mutex   m_datasetMutex;                         /*!< The mutex handling the datasets*/
            std::thread* m_updateThread  = NULL;                 /*!< The update thread*/

            uint64_t m_currentDataset      = 0;                  /*!< The current Dataset id to push */
            uint64_t m_currentSubDataset   = 0;                  /*!< The current SubDatase id, useful to determine the next subdataset 3D position*/
            uint64_t m_currentLogData      = 0;                  /*!< The current Log data ID to push */
            uint64_t m_currentSDGroup      = 0;                  /*!< The current subdataset group ID to push*/
            uint32_t m_nbConnectedHeadsets = 0;                  /*!< The current number of connected head-mounted displays*/

            VFVClientSocket*  m_headsetAnchorClient = NULL;      /*!< The client sending the anchor. If the client is NULL, m_anchorData has to be redone*/
            AnchorHeadsetData m_anchorData;                      /*!< The anchor data registered*/

            std::mutex                  m_computeMutex;           /*!< Mutex for m_computeThread*/
            std::mutex                  m_computeTasksMutex;      /*!< Mutex for m_computeTasks*/
            std::condition_variable     m_computeCond;            /*!< The condition variable associated to the compute thread*/
            std::thread*                m_computeThread;          /*!< Thread handling heavy computation*/
            std::queue<std::function<void(void)>> m_computeTasks; /*!< The tasks to run by the compute Thread*/

#ifdef VFV_LOG_DATA
            std::mutex    m_logMutex; /*!< The log file mutex */
            std::ofstream m_log;      /*!< The output log file recording every messages received and sent*/
#endif
            //Mutex load order:
            //datasetMutex, mapMutex, logMutex
    };
}

#endif
