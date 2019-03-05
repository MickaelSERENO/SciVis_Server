#include "VFVServer.h"

namespace sereno
{
#define DATASET_DIRECTORY "Datasets/"

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

        //Send already known datasets
        onLoginSendCurrentStatus(client);
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

        onLoginSendCurrentStatus(client);

        INFO << std::endl;
    }

    void VFVServer::addVTKDataset(VFVClientSocket* client, VFVVTKDatasetInformation& dataset)
    {
        if(!client->isTablet())
        {
            VFVSERVER_NOT_A_TABLET
            return;
        }

        //Open the dataset asked
        VTKParser*  parser = new VTKParser(DATASET_DIRECTORY+dataset.name);
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
        std::shared_ptr<VTKParser> sharedParser(parser);
        VTKDataset* vtk = new VTKDataset(sharedParser, ptFieldValues, cellFieldValues);

        //Update the position
        for(uint32_t i = 0; i < vtk->getNbSubDatasets(); i++, m_currentSubDataset++)
        {
            SubDataset* sd = vtk->getSubDataset(i);
            sd->setPosition(glm::vec3(m_currentSubDataset, 0.0f, 0.0f));
        }

        VTKMetaData metaData;
        metaData.dataset = vtk;
        metaData.name    = dataset.name;
        metaData.ptFieldValueIndices   = dataset.ptFields;
        metaData.cellFieldValueIndices = dataset.cellFields;

        //Add it to the list
        {
            std::lock_guard<std::mutex> lock(m_datasetMutex);
            metaData.datasetID = m_currentDataset;
            m_vtkDatasets.insert(std::pair<uint32_t, VTKMetaData>(m_currentDataset, metaData));
            m_datasets.insert(std::pair<uint32_t, VTKDataset*>(m_currentDataset, vtk));
            m_currentDataset++;
        }

        //Acknowledge the current client
        uint8_t* ackData = (uint8_t*)malloc(sizeof(uint16_t)+sizeof(uint32_t));
        writeUint16(ackData, VFV_SEND_ACKNOWLEDGE_ADD_DATASET);
        writeUint16(ackData+sizeof(uint16_t), m_currentDataset-1);
        std::shared_ptr<uint8_t> ackSharedData(ackData);
        SocketMessage<int> ackSm(client->socket, ackSharedData, sizeof(uint16_t)+sizeof(uint32_t));
        writeMessage(ackSm);

        //Send it to the other clients
        for(auto clt : m_clientTable)
        {
            if(clt.second != client)
            {
                sendAddVTKDatasetEvent(clt.second, dataset, metaData.datasetID);
                sendDatasetStatus(clt.second, vtk, metaData.datasetID);
            }
        }
    }


    void VFVServer::rotateSubDataset(VFVClientSocket* client, VFVRotationInformation& rotate)
    {
        Dataset* dataset = NULL;
        {
            auto it = m_datasets.find(rotate.datasetID);
            if(it == m_datasets.end())
            {
                VFVSERVER_DATASET_NOT_FOUND(rotate.datasetID)
                return;
            }

            if(it->second->getNbSubDatasets() <= rotate.subDatasetID)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(rotate.datasetID, rotate.subDatasetID)
                return;
            }

            dataset = it->second;
        }

        dataset->getSubDataset(rotate.subDatasetID)->setGlobalRotate(Quaternionf(rotate.quaternion[1], rotate.quaternion[2],
                                                                                 rotate.quaternion[3], rotate.quaternion[0]));

        std::lock_guard<std::mutex> lock(m_mapMutex);
        for(auto& clt : m_clientTable)
            if(clt.second != client)
                sendRotateDatasetEvent(clt.second, rotate);
    }

    void VFVServer::sendAddVTKDatasetEvent(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset, uint32_t datasetID)
    {
        uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t) + 
                            sizeof(uint8_t)*dataset.name.size() +
                            sizeof(uint32_t)*(dataset.ptFields.size()+dataset.cellFields.size()+2);

        uint8_t* data = (uint8_t*)malloc(dataSize);

        uint32_t offset=0;

        writeUint16(data, VFV_SEND_ADD_VTK_DATASET); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, datasetID); //The datasetID
        offset += sizeof(uint32_t);

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

        INFO << "Sending ADD VTK DATASET Event data\n";
        std::shared_ptr<uint8_t> sharedData(data);
        SocketMessage<int> sm(client->socket, sharedData, dataSize);
        writeMessage(sm);
    }

    void VFVServer::sendRotateDatasetEvent(VFVClientSocket* client, const VFVRotationInformation& rotate)
    {
        uint32_t dataSize = sizeof(uint16_t) + 2*sizeof(uint32_t) + 4*sizeof(float);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset=0;

        writeUint16(data, VFV_SEND_ROTATE_DATASET); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, rotate.datasetID); //The datasetID
        offset += sizeof(uint32_t);

        writeUint32(data+offset, rotate.subDatasetID); //SubDataset ID
        offset += sizeof(uint32_t); 

        for(int i = 0; i < 4; i++, offset += sizeof(float)) //Quaternion rotation
            writeFloat(data+offset, rotate.quaternion[i]);

        INFO << "Sending ROTATE DATASET Event data\n";
        std::shared_ptr<uint8_t> sharedData(data);
        SocketMessage<int> sm(client->socket, sharedData, dataSize);
        writeMessage(sm);
    }

    void VFVServer::sendMoveDatasetEvent(VFVClientSocket* client, const VFVMoveInformation& position)
    {
        uint32_t dataSize = sizeof(uint16_t) + 2*sizeof(uint32_t) + 3*sizeof(float);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset=0;

        writeUint16(data, VFV_SEND_MOVE_DATASET); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, position.datasetID); //The datasetID
        offset += sizeof(uint32_t);

        writeUint32(data+offset, position.subDatasetID); //SubDataset ID
        offset += sizeof(uint32_t); 

        for(int i = 0; i < 3; i++, offset += sizeof(float)) //Vector3 position
            writeFloat(data+offset, position.position[i]);

        INFO << "Sending MOVE DATASET Event data\n";
        std::shared_ptr<uint8_t> sharedData(data);
        SocketMessage<int> sm(client->socket, sharedData, dataSize);
        writeMessage(sm);
    }

    void VFVServer::onLoginSendCurrentStatus(VFVClientSocket* client)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        for(auto& it : m_vtkDatasets)
        {
            VFVVTKDatasetInformation dataset;

            dataset.name         = it.second.name;
            dataset.ptFields     = it.second.ptFieldValueIndices;
            dataset.cellFields   = it.second.cellFieldValueIndices;
            dataset.nbPtFields   = it.second.ptFieldValueIndices.size();
            dataset.nbCellFields = it.second.cellFieldValueIndices.size();

            //Send the event
            sendAddVTKDatasetEvent(client, dataset, it.first);
        }

        for(auto& it : m_datasets)
            sendDatasetStatus(client, it.second, it.first);
    }

    void VFVServer::sendDatasetStatus(VFVClientSocket* client, Dataset* dataset, uint32_t datasetID)
    {
        for(uint32_t i = 0; i < dataset->getNbSubDatasets(); i++)
        {
            SubDataset* sd = dataset->getSubDataset(i);
            //Send rotate
            VFVRotationInformation rotate;
            rotate.datasetID    = datasetID;
            rotate.subDatasetID = i;
            rotate.quaternion[0] = sd->getGlobalRotate().w;
            rotate.quaternion[1] = sd->getGlobalRotate().x;
            rotate.quaternion[2] = sd->getGlobalRotate().y;
            rotate.quaternion[3] = sd->getGlobalRotate().z;
            sendRotateDatasetEvent(client, rotate);

            //Send move
            VFVMoveInformation position;
            position.datasetID    = datasetID;
            position.subDatasetID = i;
            for(uint32_t j = 0; j < 3; j++)
                position.position[j] = sd->getPosition()[j];
            sendMoveDatasetEvent(client, position);
        }
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
