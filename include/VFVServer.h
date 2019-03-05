#ifndef  VFVSERVER_INC
#define  VFVSERVER_INC

#include <map>
#include <string>
#include "Server.h"
#include "VFVClientSocket.h"
#include "Datasets/BinaryDataset.h"
#include "MetaData.h"

namespace sereno
{
    inline bool endsWith(std::string const& value, std::string const& ending)
    {
        if (ending.size() > value.size()) return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    /** \brief Enum describing what the server can push */
    enum VFVSendData
    {
        VFV_SEND_ADD_VTK_DATASET         = 0,
        VFV_SEND_ACKNOWLEDGE_ADD_DATASET = 1,
        VFV_SEND_ROTATE_DATASET          = 2,
        VFV_SEND_MOVE_DATASET            = 3
    };

    /* \brief The Class Server for the Vector Field Visualization application */
    class VFVServer : public Server<VFVClientSocket>
    {
        public:
            VFVServer(uint32_t nbThread, uint32_t port);
            ~VFVServer();
        protected:

            /* \brief  Login the tablet "client"
             * \param client the client logged as a tablet
             * \param identTablet the message sent */
            void loginTablet(VFVClientSocket* client, VFVIdentTabletInformation& identTablet);

            /* \brief  Login the hololens "client"
             * \param client the client logged as a hololenz
             * \param identTablet the message sent */
            void loginHololens(VFVClientSocket* client);

            /* \brief Handle the rotation
             * \param client the client asking for a rotation
             * \param rotate the rotation data*/
            void rotateSubDataset(VFVClientSocket* client, VFVRotationInformation& rotate);

            /* \brief  Add a VTKDataset to the visualized datasets
             * \param client the client adding the dataset
             * \param dataset the dataset information to add */
            void addVTKDataset(VFVClientSocket* client, VFVVTKDatasetInformation& dataset);

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
             * \param datasetID the dataset ID
             */
            void sendDatasetStatus(VFVClientSocket* client, Dataset* dataset, uint32_t datasetID);

            /* \brief  Send the current status of the server on login
             * \param client the client to send the data */
            void onLoginSendCurrentStatus(VFVClientSocket* client);

            void onMessage(uint32_t bufID, VFVClientSocket* client, uint8_t* data, uint32_t size);

            std::map<uint32_t, BinaryMetaData> m_binaryDatasets; /*!< The binary datasets opened*/
            std::map<uint32_t, VTKMetaData>    m_vtkDatasets;    /*!< The vtk datasets opened*/
            std::map<uint32_t, Dataset*>       m_datasets;       /*!< The datasets opened*/

            std::mutex                         m_datasetMutex;   /*!< The mutex handling the datasets*/

            uint64_t m_currentDataset    = 0;                    /*!< The current Dataset id to push */
            uint64_t m_currentSubDataset = 0;                    /*!< The current SubDatase id, useful to determine the next subdataset 3D position*/
    };
}

#endif
