#include "VFVServer.h"

namespace sereno
{
#define VFVSERVER_NOT_A_TABLET\
    {\
        WARNING << "Disconnecting a client because he sent a wrong packet\n" << std::endl; \
        closeClient(client->socket);\
    }

#define VFVSERVER_DATASET_NOT_FOUND(datasetID)\
    {\
        WARNING << "Dataset ID " << (datasetID) << " not found... disconnecting the client!"; \
        close(client->socket);\
    }

#define VFVSERVER_SUB_DATASET_NOT_FOUND(datasetID, subDatasetID)\
    {\
        WARNING << "Sub Dataset ID " << (subDatasetID) << " of dataset ID " << (datasetID) << " not found... disconnecting the client!"; \
        close(client->socket);\
    }

    uint64_t VFVServer::currentDataset = 0;

    VFVServer::VFVServer(uint32_t nbThread, uint32_t port) : Server(nbThread, port)
    {}

    VFVServer::~VFVServer()
    {
        for(auto& d : m_datasets)
            delete d.second;
    }

    void VFVServer::loginTablet(VFVClientSocket* client,  VFVIdentTabletInformation& identTablet)
    {
        INFO << "Tablet connected.\n";
        if(!client->setAsTablet(identTablet.hololensIP))
        {
            VFVSERVER_NOT_A_TABLET
            return;
        }

        //Useful if the packet was sent without hololens information
        else if(identTablet.hololensIP.size() > 0)
        { 
            INFO << "Tablet connected. Bound to Hololens IP " << identTablet.hololensIP << std::endl;
            //Go through all the tablet to look for an already connected hololens
            std::lock_guard<std::mutex> lock(m_mapMutex);
            for(auto& clt : m_clientTable)
            {
                
                if(clt.second != client && clt.second->sockAddr.sin_addr.s_addr == client->getTabletData().hololensAddr.sin_addr.s_addr)
                {
                    INFO << "Hololens found!\n";
                    clt.second->getHololensData().tablet = client;
                    client->getTabletData().hololens     = clt.second;
                    return;
                }
            }
        }

        INFO << std::endl;
        //TODO send already known datasets
    }

    void VFVServer::loginHololens(VFVClientSocket* client)
    {
        client->setAsHololens();

        INFO << "Connected as Hololens...\n";

        //Go through all the known client to look for an already connected tablet
        std::lock_guard<std::mutex> lock(m_mapMutex);
        for(auto& clt : m_clientTable)
        {
            if(clt.second->isTablet())
            { 
                if(clt.second != client && client->sockAddr.sin_addr.s_addr == clt.second->getTabletData().hololensAddr.sin_addr.s_addr)
                {
                    INFO << "Tablet found!\n";
                    client->getHololensData().tablet     = clt.second;
                    clt.second->getTabletData().hololens = client;
                    break;
                }
            }
        }

        //TODO send already known datasets

        INFO << std::endl;
    }

    void VFVServer::addVTKDataset(VFVClientSocket* client, VFVVTKDatasetInformation& dataset)
    {
        if(!client->isTablet())
        {
            VFVSERVER_NOT_A_TABLET
            return;
        }

        VTKParser*  parser = new VTKParser("Datasets/"+dataset.name);
        if(!parser->parse())
        {
            ERROR << "Could not parse the VTK Dataset " << dataset.name << std::endl;
            delete parser;
            return;
        }

        //Determine VTKFieldValues to use
        std::vector<const VTKFieldValue*> ptFieldValues;
        std::vector<const VTKFieldValue*> cellFieldValues;

        std::vector<const VTKFieldValue*> parserPtValues   = parser->getPointFieldValueDescriptors();
        std::vector<const VTKFieldValue*> parserCellValues = parser->getCellFieldValueDescriptors();

        for(auto i : dataset.ptFields)
        {
            if(i >= parserPtValues.size())
            {
                ERROR << "Indice " << i << " is not a valid indice for Point Field Value in the dataset " << dataset.name << std::endl;
                delete parser;
                return;
            }
            ptFieldValues.push_back(parserPtValues[i]);
        }

        for(auto i : dataset.cellFields)
        {
            if(i >= parserCellValues.size())
            {
                ERROR << "Indice " << i << " is not a valid indice for Cell Field Value in the dataset " << dataset.name << std::endl;
                delete parser;
                return;
            }
            cellFieldValues.push_back(parserCellValues[i]);
        }

        INFO << "Opening VTK Dataset " << dataset.name << std::endl;

        //Create the dataset
        {
            std::shared_ptr<VTKParser> sharedParser(parser);
            VTKDataset* vtk = new VTKDataset(sharedParser, ptFieldValues, cellFieldValues);

            VTKMetaData metaData;
            metaData.dataset = vtk;
            metaData.name    = dataset.name;
            metaData.ptFieldValueIndices   = dataset.ptFields;
            metaData.cellFieldValueIndices = dataset.cellFields;

            //Add it to the list
            std::lock_guard<std::mutex> lock(m_datasetMutex);
            m_vtkDatasets.insert(std::pair<uint32_t, VTKMetaData>(currentDataset, metaData));
            m_datasets.insert(std::pair<uint32_t, VTKDataset*>(currentDataset, vtk));

            currentDataset++;
        }

        //Send it to the other clients
        for(auto clt : m_clientTable)
        {
            if(clt.second != client)
            {
                uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t) + 
                                    sizeof(uint8_t)*dataset.name.size() +
                                    sizeof(uint32_t)*(dataset.ptFields.size()+dataset.cellFields.size()+2);

                uint8_t* data = (uint8_t*)malloc(dataSize);

                uint32_t offset=0;

                writeUint16(data, VFV_SEND_ADD_VTK_DATASET); //Type
                offset += sizeof(uint16_t);

                writeUint32(data+offset, dataset.name.size()); //Dataset name size
                offset += sizeof(uint32_t); 

                memcpy(data+offset, dataset.name.data(), dataset.name.size()); //Dataset name
                offset += dataset.name.size()*sizeof(uint8_t);

                writeUint32(data+offset, dataset.ptFields.size()); //ptFieldValueSize
                offset+= sizeof(uint32_t);

                for(int i : dataset.ptFields)
                {
                    writeUint32(data+offset, i); //ptFieldValue[i]
                    offset += sizeof(uint32_t);
                }

                writeUint32(data+offset, dataset.cellFields.size()); //cellFieldValueSize
                for(int i : dataset.cellFields)
                {
                    writeUint32(data+offset, i); //cellFieldValue[i]
                    offset += sizeof(uint32_t);
                }

                INFO << "Sending data\n";
                std::shared_ptr<uint8_t> sharedData(data);
                SocketMessage<int> sm(clt.second->socket, sharedData, dataSize);
                writeMessage(sm);
            }
        }
    }


    void VFVServer::rotateSubDataset(VFVClientSocket* client, VFVRotationInformation& rotate)
    {
        auto it = m_datasets.find(rotate.datasetID);
        if(it == m_datasets.end())
            VFVSERVER_DATASET_NOT_FOUND(rotate.datasetID)
        if(it->second->getNbSubDatasets() <= rotate.subDatasetID)
            VFVSERVER_SUB_DATASET_NOT_FOUND(rotate.datasetID, rotate.subDatasetID)

        it->second->getSubDataset(rotate.subDatasetID)->setGlobalRotate(Quaternionf(rotate.quaternion[1], rotate.quaternion[2],
                                                                                    rotate.quaternion[3], rotate.quaternion[4]));

        //TODO tell other
    }

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
            INFO << "Pulling message\n\n";
            switch(msg.type)
            {
                case IDENT_TABLET:
                {
                    loginTablet(client, msg.identTablet);
                    break;
                }

                case IDENT_HOLOLENS:
                {
                    loginHololens(client);
                    break;
                }

                case ADD_VTK_DATASET:
                {
                    INFO << "Adding VTKDataset\n";
                    addVTKDataset(client, msg.vtkDataset);
                    break;
                }

                case ROTATE_DATASET:
                {
                    rotateSubDataset(client, msg.rotate);
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
