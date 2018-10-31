#include "VFVServer.h"

VFVServer::VFVServer(uint32_t nbThread, uint32_t port) : Server(nbThread, port)
{}

VFVServer::~VFVServer()
{}

void VFVServer::onMessage(uint32_t bufID, VFVClientSocket* client, uint8_t* data, uint32_t size)
{
    VFVMessage msg;

    if(!client->feedMessage(data, size))
    {
        ERROR << "Error at feeding message to the client. Disconnecting" << std::endl;
        goto clientError;
    }

    //Handles the message received and reconstructed
    while(client->pullMessage(&msg))
    {
        INFO << "Pulling message\n";
        switch(msg.type)
        {
            case IDENT_TABLET:
            {
                INFO << "Tablet connected. Bound to Hololens IP " << msg.identTablet.hololensIP << std::endl;
                if(!client->setAsTablet(msg.identTablet.hololensIP))
                {
                    WARNING << "Disconnecting a client because he sent a wrong packet\n" << std::endl;
                    closeClient(client->socket);
                }
                else
                {
                    //TODO send the current state known
                }
                break;
            }
            case ROTATE_DATASET:
            {
                std::lock_guard<std::mutex> lock(m_datasetMutex);
                auto it = m_datasets.find(msg.rotate.dataset);
                if(it != m_datasets.end())
                {
                    FluidDataset& fd = it->second;
                    fd.setRotation(msg.rotate.quaternion);
                }
            }

                INFO << "Setting rotation of " << msg.rotate.dataset << " to " << msg.rotate.quaternion[0] << " " <<
                                                                                  msg.rotate.quaternion[1] << " " <<
                                                                                  msg.rotate.quaternion[2] << " " <<
                                                                                  msg.rotate.quaternion[3] << " " << std::endl;
                //TODO send data to the other clients
                break;
            case CHANGE_COLOR:
            {
                std::lock_guard<std::mutex> lock(m_datasetMutex);
                auto it = m_datasets.find(msg.color.dataset);
                if(it != m_datasets.end())
                {
                    FluidDataset& fd = it->second;
                    fd.setColor(msg.color.mode, msg.color.min, msg.color.max);
                }
            }
                //TODO send data to the other clients
                break;
            case ADD_DATASET:
            {
                if(!client->isTablet())
                {
                    WARNING << "A client which is not a tablet asked for opening a dataset..." << std::endl;
                    goto clientError;
                }
                
                std::lock_guard<std::mutex> lock(m_datasetMutex);
                if(m_datasets.find(msg.dataset.name) != m_datasets.end())
                {
                    INFO << "Opening dataset " << msg.dataset.name << std::endl;
                    m_datasets.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(msg.dataset.name), 
                                       std::forward_as_tuple(msg.dataset.name));
                }
                else
                    WARNING << "Dataset " << msg.dataset.name << " already opened\n";
            }
                //TODO send data to the other clients
                break;
            case REMOVE_DATASET:
            {
                std::lock_guard<std::mutex> lock(m_datasetMutex);
                m_datasets.erase(msg.dataset.name);
            }
                //TODO send data to the other clients
                break;
            default:
                WARNING << "Type " << msg.type << " not handled yet\n";
                break;
        }

        continue;
    }
    return;
clientError:
    closeClient(client->socket);
    return;
}
