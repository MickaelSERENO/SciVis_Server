#ifndef  VFVSERVER_INC
#define  VFVSERVER_INC

#include <map>
#include <string>
#include "FluidDataset.h"
#include "Server.h"
#include "VFVClientSocket.h"

/* \brief The Class Server for the Vector Field Visualization application */
class VFVServer : public Server<VFVClientSocket>
{
    public:
        VFVServer(uint32_t nbThread, uint32_t port);
        ~VFVServer();
    protected:
        void onMessage(uint32_t bufID, VFVClientSocket* client, uint8_t* data, uint32_t size);
        std::map<std::string, FluidDataset> m_datasets;     /*!< The fluid datasets states*/
        std::mutex                          m_datasetMutex; /*!< The mutex handling the datasets*/
};

#endif
