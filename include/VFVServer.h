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

    enum VFVSendData
    {
        VFV_SEND_ADD_VTK_DATASET = 0,
    };

    /* \brief The Class Server for the Vector Field Visualization application */
    class VFVServer : public Server<VFVClientSocket>
    {
        public:
            VFVServer(uint32_t nbThread, uint32_t port);
            ~VFVServer();
        protected:
            static uint64_t currentDataset; /*!< The current Dataset id to push */

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

            void onMessage(uint32_t bufID, VFVClientSocket* client, uint8_t* data, uint32_t size);

            std::map<uint32_t, BinaryMetaData> m_binaryDatasets; /*!< The binary datasets opened*/
            std::map<uint32_t, VTKMetaData>    m_vtkDatasets;    /*!< The vtk datasets opened*/
            std::map<uint32_t, Dataset*>       m_datasets;       /*!< The datasets opened*/

            std::mutex                         m_datasetMutex;   /*!< The mutex handling the datasets*/
    };
}

#endif
