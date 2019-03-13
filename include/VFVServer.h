#ifndef  VFVSERVER_INC
#define  VFVSERVER_INC

#include <map>
#include <string>
#include <stack>
#include "Server.h"
#include "VFVClientSocket.h"
#include "Datasets/BinaryDataset.h"
#include "MetaData.h"

#define UPDATE_THREAD_FRAMERATE 1
#define MAX_NB_HEADSETS         10

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
        VFV_SEND_ADD_VTK_DATASET         = 0, //Send an ADD VTK event
        VFV_SEND_ACKNOWLEDGE_ADD_DATASET = 1, //Acknowledge an event "add vtk"
        VFV_SEND_ROTATE_DATASET          = 2, //Send the rotation status of a dataset
        VFV_SEND_MOVE_DATASET            = 3, //Send the position status of a dataset
        VFV_SEND_HEADSET_INIT            = 4, //Send the initial values of a headset
        VFV_SEND_HEADSETS_STATUS         = 5, //Send all the headsets status except the client receiving the message
        VFV_SEND_HEADSET_BINDING_INFO    = 6, //Send the binding headset information to the client tablet
            
    };

    /* \brief The Class Server for the Vector Field Visualization application */
    class VFVServer : public Server<VFVClientSocket>
    {
        public:
            VFVServer(uint32_t nbThread, uint32_t port);
            VFVServer(VFVServer&& mvt);
            ~VFVServer();

            bool launch();
            void closeServer();

            /** \brief  The distinguishable color used in this sci vis application */
            static const uint32_t SCIVIS_DISTINGUISHABLE_COLORS[10];
        protected:

            void closeClient(SOCKET client);

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
             * \param rotate the rotation data*/
            void rotateSubDataset(VFVClientSocket* client, const VFVRotationInformation& rotate);

            /* \brief  Add a VTKDataset to the visualized datasets
             * \param client the client adding the dataset
             * \param dataset the dataset information to add */
            void addVTKDataset(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset);

            /* \brief  Update the headset "client" into the server's internal data
             * \param client the client pushing the new values to update
             * \param headset the values to push */
            void updateHeadset(VFVClientSocket* client, const VFVUpdateHeadset& headset);

            /* \brief  Send an empty message
             * \param client the client to send the message
             * \param type the type of the message*/
            void sendEmptyMessage(VFVClientSocket* client, uint16_t type);

            /* \brief  Send the Add VTK Dataset Event to a given client
             * \param client the client to send the message
             * \param dataset the dataset information 
             * \param datasetID the datasetID*/
            void sendAddVTKDatasetEvent(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset, uint32_t datasetID);

            /* \brief  Send a rotation event to client
             * \param client the client to send the information
             * \param rotate the rotate information*/
            void sendRotateDatasetEvent(VFVClientSocket* client, const VFVRotationInformation& rotate);

            /* \brief  Send a position event to client
             * \param client the client to send the information
             * \param position the position information */
            void sendMoveDatasetEvent(VFVClientSocket* client, const VFVMoveInformation& position);

            /* \brief  Send the whole dataset status to a client
             * \param client the client to send the data
             * \param dataset the dataset information
             * \param datasetID the dataset ID */
            void sendDatasetStatus(VFVClientSocket* client, Dataset* dataset, uint32_t datasetID);

            /* \brief Send the binding information to the tablet connected with a headset 
             * \param client the client to send the data (tablet)
             * \param headset the headset bound to the client */
            void sendHeadsetBindingInfo(VFVClientSocket* client, VFVClientSocket* headset);

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
            std::mutex   m_updateMutex;                          /*!< The update thread mutex*/
            std::thread* m_updateThread  = NULL;                 /*!< The update thread*/

            uint64_t m_currentDataset    = 0;                    /*!< The current Dataset id to push */
            uint64_t m_currentSubDataset = 0;                    /*!< The current SubDatase id, useful to determine the next subdataset 3D position*/
            uint32_t m_nbConnectedHeadsets = 0;
    };
}

#endif
