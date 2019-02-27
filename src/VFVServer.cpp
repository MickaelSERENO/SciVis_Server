#include "VFVServer.h"

namespace sereno
{
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
                        //Go through all the tablet to look for an already connected hololens
                        std::lock_guard<std::mutex> lock(m_mapMutex);
                        for(auto& clt : m_clientTable)
                        {
                            
                            if((clt.second->sockAddr.sin_addr.s_addr == client->getTabletData().hololensAddr.sin_addr.s_addr) && 
                               (clt.second->sockAddr.sin_port        == client->getTabletData().hololensAddr.sin_port))
                            {
                                INFO << "Hololens found!\n";
                                clt.second->getHololensData().tablet = client;
                                client->getTabletData().hololens     = clt.second;
                                break;
                            }
                        }
                    }
                    break;
                }

                case IDENT_HOLOLENS:
                {
                    client->setAsHololens();

                    //Go through all the known client to look for an already connected tablet
                    std::lock_guard<std::mutex> lock(m_mapMutex);
                    for(auto& clt : m_clientTable)
                    {
                        if(clt.second->isTablet())
                        { 
                            if((client->sockAddr.sin_addr.s_addr == clt.second->getTabletData().hololensAddr.sin_addr.s_addr) && 
                               (client->sockAddr.sin_port        == clt.second->getTabletData().hololensAddr.sin_port))
                            {
                                INFO << "Tablet found!\n";
                                client->getHololensData().tablet     = clt.second;
                                clt.second->getTabletData().hololens = client;
                                break;
                            }
                        }
                    }
                    break;
                }
                case ROTATE_DATASET:
                {
                    std::lock_guard<std::mutex> lock(m_datasetMutex);
                    //TODO send data to the other clients
                    break;
                }
                case CHANGE_COLOR:
                {
                    std::lock_guard<std::mutex> lock(m_datasetMutex);
                    //TODO send data to the other clients
                    break;
                }
                case ADD_DATASET:
                {
                    if(!client->isTablet())
                    {
                        WARNING << "A client which is not a tablet asked for opening a dataset..." << std::endl;
                        goto clientError;
                    }
                    
                    std::lock_guard<std::mutex> lock(m_datasetMutex);
                    //TODO send data to the other clients
                    break;
                }
                case REMOVE_DATASET:
                {
                    std::lock_guard<std::mutex> lock(m_datasetMutex);
                    break;
                }
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
}
