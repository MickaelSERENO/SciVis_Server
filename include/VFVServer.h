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
#include "Datasets/BinaryDataset.h"
#include "Datasets/Annotation/Annotation.h"
#include "MetaData.h"
#include "AnchorHeadsetData.h"
#include "config.h"

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
        VFV_SEND_ADD_VTK_DATASET         = 0,  /*!< Send an ADD VTK event*/
        VFV_SEND_ACKNOWLEDGE_ADD_DATASET = 1,  /*!< Acknowledge an event "add vtk"*/
        VFV_SEND_ROTATE_DATASET          = 2,  /*!< Send the rotation status of a dataset*/
        VFV_SEND_MOVE_DATASET            = 3,  /*!< Send the position status of a dataset*/
        VFV_SEND_HEADSET_BINDING_INFO    = 4,  /*!< Send the binding headset information*/
        VFV_SEND_HEADSETS_STATUS         = 5,  /*!< Send all the headsets status except the client receiving the message*/
        VFV_SEND_HEADSET_ANCHOR_SEGMENT  = 6,  /*!< Send anchor segment*/
        VFV_SEND_HEADSET_ANCHOR_EOF      = 7,  /*!< Send anchor end of stream*/
        VFV_SEND_SUBDATASET_LOCK_OWNER   = 8,  /*!< Send the new subdataset lock owner*/
        VFV_SEND_SCALE_DATASET           = 9,  /*!< Send the scaling status of a dataset*/
        VFV_SEND_TF_DATASET              = 10, /*!< Send the Transfer Function status of a dataset*/
        VFV_SEND_START_ANNOTATION        = 11, /*!< Send the start annotation message (asking to start an annotation) */
        VFV_SEND_ANCHOR_ANNOTATION       = 12, /*!< Send the achor annotation message (anchor an annotation in a dataset)*/
        VFV_SEND_CLEAR_ANNOTATION        = 13, /*!< Send the clear annotations message (asking to clear all annotations in a specific subdataset) */
        VFV_SEND_ADD_SUBDATASET          = 14, /*!< Send the "add" subdataset command.*/
        VFV_SEND_DEL_SUBDATASET          = 15, /*!< Send the "delete" subdataset command.*/
        VFV_SEND_SUBDATASET_OWNER        = 16, /*!< Send the new SubDataset owner command.*/
        VFV_SEND_CURRENT_ACTION          = 17, /*!< Send the current action command.*/
        VFV_SEND_LOCATION                = 18, /*!< Send the location to the tablet.*/
        VFV_SEND_TABLET_LOCATION         = 19, /*!< Send the tablet's virtual location.*/
        VFV_SEND_LASSO                   = 20, /*!< Send the lasso data.*/
        VFV_SEND_TABLET_SCALE            = 21, /*!< Send the tablet's scale.*/
    };

    /** \brief  Clone a Transfer function based on its ype
     * \param type the transfer function type
     * \param tf the transfer function to clone
     * \return the new Transfer Function allocated using new. The caller is responsible to destroy that object*/
    TF* cloneTransferFunction(TFType type, std::shared_ptr<const TF> tf);

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

            void closeClient(SOCKET client);

            /* \brief  Get the dataset via its ID
             * \param datasetID the dataset ID
             * \param sdID the subdataset ID to test the validity of the Dataset. can be 0.
             * \return  NULL if not found, a pointer to the dataset if found*/
            Dataset* getDataset(uint32_t datasetID, uint32_t sdID);

            /* \brief  Tells wether a given client can modify a subdataset or not
             * \param client the client attempting to modify the subdataset. If NULL, the client is considered here as the server (which can modify everything)
             * \param sdMT the subdataset meta data
             * \return  true if yes, false otherwise */
            bool canModifySubDataset(VFVClientSocket* client, SubDatasetMetaData* sdMT);

            /* \brief  Get the Dataset meta data via its ID
             * \param datasetID the dataset ID
             * \param sdID the subdataset ID
             * \param sdMTPtr[out] if not NULL, will contain the SubDatasetMetaData corresponding
             * \return The MetaData being updated. NULL if not found. In this case, sdMTPtr will not be modified*/
            MetaData* getMetaData(uint32_t datasetID, uint32_t sdID, SubDatasetMetaData** sdMTPtr);

            /* \brief  Update the subdataset meta data last modification component via its ID
             * \param client the client modifying the metadata
             * \param datasetID the dataset ID
             * \param sdID the subdataset ID
             * \param sdMTPtr[out] if not NULL, will contain the SubDatasetMetaData corresponding
             * \return The MetaData being updated. NULL if not found*/
            MetaData* updateMetaDataModification(VFVClientSocket* client, uint32_t datasetID, uint32_t sdID, SubDatasetMetaData** sdMTPtr=NULL);

            /* \brief  Get the Headset ClientSocket object from a VFVClientSocket that can also be a tablet
             * \param client the client link to a headset. If client->isHeadset, returns client, otherwise returns client->getTabletData().headset
             * \return   the corresponding headset client object */
            VFVClientSocket* getHeadsetFromClient(VFVClientSocket* client);

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

            /* \brief Handle the change of transfer function
             * \param client the client asking for a new transfer function
             * \param tfSD the transfer function data. Not constant because the headset ID will change */
            void tfSubDataset(VFVClientSocket* client, VFVTransferFunctionSubDataset& tfSD);

            /* \brief Handle the scaling
             * \param client the client asking for a rotation
             * \param scale the scale values of the data. Not constant because the headset ID will change*/
            void scaleSubDataset(VFVClientSocket* client, VFVScaleInformation& scale);

            /* \brief  Add a VTKDataset to the visualized datasets
             * \param client the client adding the dataset
             * \param dataset the dataset information to add */
            void addVTKDataset(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset);

            /* \brief  Add a SubDataset to a given one
             * \param client the client adding the dataset
             * \param dataset the dataset information to add
             *
             * \return the SubDataset added. NULL if error*/
            SubDataset* onAddSubDataset(VFVClientSocket* client, const VFVAddSubDataset& dataset);

            /* \brief  Remove a known subdataset
             * \param dataset the dataset ID information */
            void removeSubDataset(const VFVRemoveSubDataset& dataset);

            /* \brief  Make public a known SubDataset
             * \param client the client making this subdataset public
             * \param dataset the dataset ID information */
            void onMakeSubDatasetPublic(VFVClientSocket* client, const VFVMakeSubDatasetPublic& dataset);

            /* \brief  Remove a known SubDataset
             * \param client the client asking to remove the subdataset
             * \param dataset the dataset information to remove */
            void onRemoveSubDataset(VFVClientSocket* client, const VFVRemoveSubDataset& dataset);

            /* \brief  Duplicate a known SubDataset
             * \param client the client asking to duplicate the subdataset
             * \param dataset the dataset information to duplicate */
            void onDuplicateSubDataset(VFVClientSocket* client, const VFVDuplicateSubDataset& dataset);

            /* \brief  send a tablet's virtual location to the hololens
             * \param client the tablet sending the location
             * \param location the tablet's virtual location */
            void onLocation(VFVClientSocket* client, const VFVLocation& location);

            /* \brief  send a tablet's lasso to the hololens
             * \param client the tablet sending the lasso
             * \param lasso the lasso to send */
            void onLasso(VFVClientSocket* client, const VFVLasso& lasso);
            
            /* \brief  set a tablet's virtual scale to the hololens
             * \param client the tablet sending the lasso
             * \param scale the tablet scale */
            void onTabletScale(VFVClientSocket* client, const VFVTabletScale& tabletScale);

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

            /* \brief  Send an empty message
             * \param client the client to send the message
             * \param type the type of the message*/
            void sendEmptyMessage(VFVClientSocket* client, uint16_t type);

            /* \brief  Send the Add VTK Dataset Event to a given client
             * \param client the client to send the message
             * \param dataset the dataset information 
             * \param datasetID the datasetID*/
            void sendAddVTKDatasetEvent(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset, uint32_t datasetID);

            /* \brief Send the AddSubDataset Event to a given client for a given SubDataset
             * \param client the client to send the message
             * \param SubDataset the subdataset information */
            void sendAddSubDataset(VFVClientSocket* client, const SubDataset* sd);

            /* \brief  Send the Remove SubDataset Event to a given client
             * \param client the client to send the message
             * \param dataset the dataset information*/
            void sendRemoveSubDatasetEvent(VFVClientSocket* client, const VFVRemoveSubDataset& dataset);

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

            /* \brief  Send transfer function of a subdataset event to client
             * \param client the client to send the information
             * \param tfSD the transfer function information */
            void sendTransferFunctionDataset(VFVClientSocket* client, const VFVTransferFunctionSubDataset& tfSD);

            /* \brief Send the current action message to a given client (will set what currently the headset is supposed to do)
             * \param client the client to send the information
             * \param currentActionID the action ID to send */
            void sendCurrentAction(VFVClientSocket* client, uint32_t currentActionID);

            /* \brief  Send the the subdataset status to a client
             * \param client the client to send the data
             * \param sd the subdataset information
             * \param datasetID the dataset ID that this SubDataset is attached to*/
            void sendSubDatasetStatus(VFVClientSocket* client, SubDataset* sd, uint32_t datasetID);

            /* \brief  Send the whole dataset status to a client
             * \param client the client to send the data
             * \param dataset the dataset information
             * \param datasetID the dataset ID */
            void sendDatasetStatus(VFVClientSocket* client, Dataset* dataset, uint32_t datasetID);

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
            void sendSubDatasetOwner(SubDatasetMetaData* data);

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

            /* \brief  Send the current status of the server on login
             * \param client the client to send the data */
            void onLoginSendCurrentStatus(VFVClientSocket* client);

            void onMessage(uint32_t bufID, VFVClientSocket* client, uint8_t* data, uint32_t size);

            /** \brief Main thread running for updating other devices*/
            void updateThread();

            /*----------------------------------------------------------------------------*/
            /*---------------------------------ATTRIBUTES---------------------------------*/
            /*----------------------------------------------------------------------------*/

            std::stack<uint32_t> m_availableHeadsetColors;       /*!< The available headset colors*/

            std::map<uint32_t, BinaryMetaData> m_binaryDatasets; /*!< The binary datasets opened*/
            std::map<uint32_t, VTKMetaData>    m_vtkDatasets;    /*!< The vtk datasets opened*/
            std::map<uint32_t, Dataset*>       m_datasets;       /*!< The datasets opened*/

            std::mutex   m_datasetMutex;                         /*!< The mutex handling the datasets*/
            std::thread* m_updateThread  = NULL;                 /*!< The update thread*/

            uint64_t m_currentDataset    = 0;                    /*!< The current Dataset id to push */
            uint64_t m_currentSubDataset = 0;                    /*!< The current SubDatase id, useful to determine the next subdataset 3D position*/
            uint32_t m_nbConnectedHeadsets = 0;

            VFVClientSocket*  m_headsetAnchorClient = NULL;      /*!< The client sending the anchor. If the client is NULL, m_anchorData has to be redone*/
            AnchorHeadsetData m_anchorData;                      /*!< The anchor data registered*/

#ifdef VFV_LOG_DATA
            std::mutex    m_logMutex; /*!< The log file mutex */
            std::ofstream m_log;      /*!< The output log file recording every messages received and sent*/
#endif
            //Mutex load order:
            //datasetMutex, mapMutex, logMutex
    };
}

#endif
