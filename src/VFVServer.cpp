#include "VFVServer.h"

namespace sereno
{
#define DATASET_DIRECTORY "Datasets/"

#define VFVSERVER_NOT_A_TABLET\
    {\
        WARNING << "Disconnecting a client because he sent a wrong packet (excepted a TABLET)\n" << std::endl; \
        closeClient(client->socket);\
    }

#define VFVSERVER_NOT_A_HEADSET\
    {\
        WARNING << "Disconnecting a client because he sent a wrong packet (expected a HEADSET)\n" << std::endl; \
        closeClient(client->socket);\
    }

#define VFVSERVER_NOT_CORRECT_HEADSET\
    {\
        WARNING << "Disconnecting a client because he sent a wrong packet (expected the correct HEADSET)\n" << std::endl; \
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

    const uint32_t VFVServer::SCIVIS_DISTINGUISHABLE_COLORS[10] = {0xffe119, 0x4363d8, 0xf58231, 0xfabebe, 0xe6beff, 
                                                                   0x800000, 0x000075, 0xa9a9a9, 0xffffff, 0x000000};

    VFVServer::VFVServer(uint32_t nbThread, uint32_t port) : Server(nbThread, port)
    {
    }

    VFVServer::VFVServer(VFVServer&& mvt) : Server(std::move(mvt))
    {
        m_updateThread     = mvt.m_updateThread;
        mvt.m_updateThread = NULL;
    }

    VFVServer::~VFVServer()
    {
        for(auto& d : m_datasets)
            delete d.second;
    }

    bool VFVServer::launch()
    {
        //Init the available color 
        for(int32_t i = sizeof(SCIVIS_DISTINGUISHABLE_COLORS)/sizeof(SCIVIS_DISTINGUISHABLE_COLORS[0])-1; i >= 0; i--)
            m_availableHeadsetColors.push(SCIVIS_DISTINGUISHABLE_COLORS[i]);

        bool ret = Server::launch();
        m_updateThread = new std::thread(&VFVServer::updateThread, this);

        return ret;
    }

    void VFVServer::closeServer()
    {
        Server::closeServer();
        if(m_updateThread)
        {
            if(m_updateThread->native_handle() != 0)
            {
                pthread_cancel(m_updateThread->native_handle());
                m_updateThread->join();
            }
            delete m_updateThread;
            m_updateThread = 0;
        }
    }

    void VFVServer::closeClient(SOCKET client)
    {
        m_mapMutex.lock();
            auto c = m_clientTable[client];

            //Handle headset disconnections
            if(c->isHeadset())
            {
                INFO << "Disconnecting a headset client\n";
                m_availableHeadsetColors.push(c->getHeadsetData().color);
                m_nbConnectedHeadsets--;

                //Disconnect with the tablet as well
                for(auto it : m_clientTable)
                    if(it.second->isTablet() && it.second->getTabletData().headset == c)
                    {
                        it.second->getTabletData().headset = NULL;
                        sendHeadsetBindingInfo(it.second, NULL);
                        break;
                    }

                if(m_headsetAnchorClient == c)
                    m_headsetAnchorClient = NULL;
            }

            //Handle table disconnections
            else if(c->isTablet())
            {
                INFO << "Disconnecting a tablet client\n";

                //Disconnect with the headset as well
                for(auto it : m_clientTable)
                    if(it.second->isHeadset() && it.second->getHeadsetData().tablet == c)
                    {
                        it.second->getHeadsetData().tablet = NULL;
                        sendHeadsetBindingInfo(it.second, it.second);
                        break;
                    }
            }
        m_mapMutex.unlock();

        Server::closeClient(client);

        askNewAnchor();
        INFO << "End of disconnection\n";
    }

    void VFVServer::askNewAnchor()
    {
        //Re ask for a new anchor
        if(m_headsetAnchorClient == NULL)
        {
            m_mapMutex.lock();
                //Reset anchoring
                for(auto it : m_clientTable)
                    if(it.second->isHeadset())
                        it.second->getHeadsetData().anchoringSent = false;

                m_anchorData.finalize(false);
                for(auto it : m_clientTable)
                    if(it.second->isHeadset())
                    {
                        sendHeadsetBindingInfo(it.second, it.second);
                        break;
                    }
            m_mapMutex.unlock();
        }
    }

    /*----------------------------------------------------------------------------*/
    /*----------------------------TREAT INCOMING DATA-----------------------------*/
    /*----------------------------------------------------------------------------*/

    void VFVServer::loginTablet(VFVClientSocket* client, const VFVIdentTabletInformation& identTablet)
    {
        INFO << "Tablet connected.\n";
        if(!client->setAsTablet(identTablet.headsetIP))
        {
            VFVSERVER_NOT_A_TABLET
            return;
        }

        //Tell the old bound headset the tablet disconnection
        if(client->getTabletData().headset)
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            sendHeadsetBindingInfo(client->getTabletData().headset, NULL);
            client->getTabletData().headset->getHeadsetData().tablet = NULL;
            client->getTabletData().headset = NULL;
        }

        //Useful if the packet was sent without headset information
        else if(identTablet.headsetIP.size() > 0)
        { 
            //Go through all the tablet to look for an already connected headset
            std::lock_guard<std::mutex> lock(m_mapMutex);
            for(auto& clt : m_clientTable)
            {
                if(clt.second != client && clt.second->sockAddr.sin_addr.s_addr == client->getTabletData().headsetAddr.sin_addr.s_addr)
                {
                    INFO << "Headset found!\n";
                    clt.second->getHeadsetData().tablet = client;
                    client->getTabletData().headset     = clt.second;
                    sendHeadsetBindingInfo(clt.second, clt.second); //Send the headset the binding information
                    return;
                }
            }
        }

        INFO << std::endl;

        //Send already known datasets
        onLoginSendCurrentStatus(client);
    }

    void VFVServer::loginHeadset(VFVClientSocket* client)
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);

        if(m_nbConnectedHeadsets >= MAX_NB_HEADSETS)
        {
            WARNING << "Too much headsets connected... Disconnecting this one\n";
            closeClient(client->socket);
            return;
        }
        m_nbConnectedHeadsets++;

        client->setAsHeadset();

        INFO << "Connected as Headset...\n";

        //Go through all the known client to look for an already connected tablet
        for(auto& clt : m_clientTable)
        {
            if(clt.second->isTablet())
            { 
                if(clt.second != client && client->sockAddr.sin_addr.s_addr == clt.second->getTabletData().headsetAddr.sin_addr.s_addr)
                {
                    INFO << "Tablet found!\n";
                    client->getHeadsetData().tablet     = clt.second;
                    clt.second->getTabletData().headset = client;

                    //Send the tablet the binding information
                    sendHeadsetBindingInfo(clt.second, client);
                    break;
                }
            }
        }

        //Set visualizable color
        client->getHeadsetData().color = m_availableHeadsetColors.top();
        m_availableHeadsetColors.pop();

        onLoginSendCurrentStatus(client);

        INFO << std::endl;
    }

    void VFVServer::sendEmptyMessage(VFVClientSocket* client, uint16_t type)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t));
        writeUint16(data, type);

        INFO << "Sending EMPTY MESSAGE Event data. Type : " << type << std::endl;
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, sizeof(int16_t));
        writeMessage(sm);
    }

    void VFVServer::addVTKDataset(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset)
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
            sd->setPosition(glm::vec3(m_currentSubDataset*2.0f, 0.0f, 0.0f));
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
        std::shared_ptr<uint8_t> ackSharedData(ackData, free);
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

        if(client->isTablet() && client->getTabletData().headset)
            rotate.headsetID = client->getTabletData().headset->getHeadsetData().id;

        std::lock_guard<std::mutex> lock(m_mapMutex);
        for(auto& clt : m_clientTable)
            if(clt.second != client)
                sendRotateDatasetEvent(clt.second, rotate);
    }

    void VFVServer::updateHeadset(VFVClientSocket* client, const VFVUpdateHeadset& headset)
    {
        if(!client->isHeadset())
        {
            VFVSERVER_NOT_A_HEADSET  
            return;
        }

        auto& internalData = client->getHeadsetData();
        for(uint32_t i = 0; i < 3; i++)
            internalData.position[i] = headset.position[i];
        for(uint32_t i = 0; i < 4; i++)
            internalData.rotation[i] = headset.rotation[i];
    }

    /*----------------------------------------------------------------------------*/
    /*-------------------------------SEND MESSAGES--------------------------------*/
    /*----------------------------------------------------------------------------*/

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
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, dataSize);
        writeMessage(sm);
    }

    void VFVServer::sendRotateDatasetEvent(VFVClientSocket* client, const VFVRotationInformation& rotate)
    {
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + 4*sizeof(float);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset=0;

        writeUint16(data, VFV_SEND_ROTATE_DATASET); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, rotate.datasetID); //The datasetID
        offset += sizeof(uint32_t);

        writeUint32(data+offset, rotate.subDatasetID); //SubDataset ID
        offset += sizeof(uint32_t); 

        writeUint32(data+offset, rotate.headsetID); //The headset ID
        offset += sizeof(uint32_t); 

        for(int i = 0; i < 4; i++, offset += sizeof(float)) //Quaternion rotation
            writeFloat(data+offset, rotate.quaternion[i]);

        INFO << "Sending ROTATE DATASET Event data\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, dataSize);
        writeMessage(sm);
    }

    void VFVServer::sendMoveDatasetEvent(VFVClientSocket* client, const VFVMoveInformation& position)
    {
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + 3*sizeof(float);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset=0;

        writeUint16(data, VFV_SEND_MOVE_DATASET); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, position.datasetID); //The datasetID
        offset += sizeof(uint32_t);

        writeUint32(data+offset, position.subDatasetID); //SubDataset ID
        offset += sizeof(uint32_t); 

        writeUint32(data+offset, position.headsetID); //The headset ID
        offset += sizeof(uint32_t); 

        for(int i = 0; i < 3; i++, offset += sizeof(float)) //Vector3 position
            writeFloat(data+offset, position.position[i]);

        INFO << "Sending MOVE DATASET Event data\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, dataSize);
        writeMessage(sm);
    }

    void VFVServer::onLoginSendCurrentStatus(VFVClientSocket* client)
    {
        //Send binding information
        if(client->isHeadset())
            sendHeadsetBindingInfo(client, client);
        else if(client->isTablet())
            sendHeadsetBindingInfo(client, client->getTabletData().headset);

        //Send common data
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

        //Send anchoring data
        if(client->isHeadset())
            sendAnchoring(client);
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

    void VFVServer::sendHeadsetBindingInfo(VFVClientSocket* client, VFVClientSocket* headset)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + 2*sizeof(uint8_t) + 2*sizeof(uint32_t));
        uint32_t offset = 0;

        //Type
        writeUint16(data+offset, VFV_SEND_HEADSET_BINDING_INFO);
        offset+=sizeof(uint16_t);

        if(headset)
        {
            //Headset ID
            writeUint32(data+offset, headset->getHeadsetData().id);
            offset+=sizeof(uint32_t);

            //Headset color
            writeUint32(data+offset, headset->getHeadsetData().color);
            offset+=sizeof(uint32_t);

            //Tablet connected
            data[offset++] = headset->getHeadsetData().tablet != NULL;
        }
        else
        {
            //Headset ID
            writeUint32(data+offset, -1);
            offset += sizeof(uint32_t);

            //Headset Color
            writeUint32(data+offset, 0x000000);
            offset+=sizeof(uint32_t);

            //Tablet connected
            data[offset++] = 0;
        }

        //First headset
        data[offset] = 0;
        if(m_headsetAnchorClient == NULL)
        {
            for(auto& clt : m_clientTable)
            {
                if(clt.second == client)
                {
                    data[offset] = 1;
                    break;
                }
                else if(clt.second != client && clt.second->isHeadset())
                {
                    data[offset]=0;
                    break;
                }
            }

            if(data[offset] && client == headset) //Send only if we are discussing with the headset and not the tablet
                m_headsetAnchorClient = headset;
        }
        offset++;
        
        INFO << "Sending HEADSET BINDING INFO Event data\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
    }

    void VFVServer::sendAnchoring(VFVClientSocket* client)
    {
        if(!m_anchorData.isCompleted() || !client->isHeadset() || client->getHeadsetData().anchoringSent)
            return;

        //Send segment by segment
        for(auto& itSegment : m_anchorData.getSegmentData())
        {
            uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t)+sizeof(uint32_t));
            uint32_t offset = 0;

            writeUint16(data, VFV_SEND_HEADSET_ANCHOR_SEGMENT);
            offset += sizeof(uint16_t);

            writeUint32(data+offset, itSegment.dataSize);
            offset += sizeof(uint32_t);

            //Send header
            std::shared_ptr<uint8_t> sharedData(data, free);
            SocketMessage<int> sm(client->socket, sharedData, offset);
            writeMessage(sm);

            //Send data stream
            SocketMessage<int> smSegment(client->socket, itSegment.data, itSegment.dataSize);
            writeMessage(smSegment);
        }

        //Send end of anchor
        uint8_t* data = (uint8_t*)malloc(sizeof(uint8_t)*2);
        writeUint16(data, VFV_SEND_HEADSET_ANCHOR_EOF);
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, sizeof(uint16_t));
        writeMessage(sm);

        INFO << "Finish sending anchor\n";

        client->getHeadsetData().anchoringSent = true;
    }

    void VFVServer::sendAnchoring()
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);

        for(auto& it : m_clientTable)
            sendAnchoring(it.second);
    }

    /*----------------------------------------------------------------------------*/
    /*---------------------OVERRIDED METHOD + ADDITIONAL ONES---------------------*/
    /*----------------------------------------------------------------------------*/

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
            switch(msg.type)
            {
                case IDENT_TABLET:
                {
                    loginTablet(client, msg.identTablet);
                    break;
                }

                case IDENT_HEADSET:
                {
                    loginHeadset(client);
                    break;
                }

                case ADD_VTK_DATASET:
                {
                    addVTKDataset(client, msg.vtkDataset);
                    break;
                }

                case ROTATE_DATASET:
                {
                    rotateSubDataset(client, msg.rotate);
                    break;
                }

                case UPDATE_HEADSET:
                {
                    updateHeadset(client, msg.headset);
                    break;
                }
                case ANNOTATION_DATA:
                {
                    INFO << "Received annotation data!" << std::endl;
                    break;
                }

                case ANCHORING_DATA_SEGMENT:
                {
                    if(m_headsetAnchorClient != client)
                    {
                        VFVSERVER_NOT_CORRECT_HEADSET
                        return;
                    }
                    INFO << "Receiving anchor data segment sized : " << msg.defaultByteArray.dataSize << std::endl;
                    m_anchorData.pushDataSegment(msg.defaultByteArray);
                    break;
                }
                case ANCHORING_DATA_STATUS:
                {
                    if(m_headsetAnchorClient != client)
                    {
                        VFVSERVER_NOT_CORRECT_HEADSET
                        return;
                    }
                    INFO << "Receiving end of anchoring data : " << msg.anchoringDataStatus.succeed << std::endl;
                    m_anchorData.finalize(msg.anchoringDataStatus.succeed);
                    if(msg.anchoringDataStatus.succeed == false)
                        askNewAnchor();
                    else
                    {
                        client->getHeadsetData().anchoringSent = true;
                        sendAnchoring();
                    }
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

    void VFVServer::updateThread()
    {
        while(!m_closeThread)
        {
            struct timespec beg;
            struct timespec end;

            clock_gettime(CLOCK_REALTIME, &beg);
            {
                //Send HEADSETS_STATUS
                std::lock_guard<std::mutex> lock(m_mapMutex);
                INFO << "Size m_clientTable : " << m_clientTable.size() << std::endl;
                for(auto& it : m_clientTable)
                {
                    if(it.second->isHeadset())
                    {
                        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + sizeof(uint32_t) + 
                                                         MAX_NB_HEADSETS*(7*sizeof(float) + 2*sizeof(uint32_t)));
                        uint32_t offset    = 0;
                        uint32_t nbHeadset = 0;

                        //Type
                        writeUint16(data+offset, VFV_SEND_HEADSETS_STATUS);
                        offset += sizeof(uint16_t) + sizeof(uint32_t); //Write NB_HEADSET later

                        for(auto& it2 : m_clientTable)
                        {
                            if(it2.second->isHeadset() && it2.second != it.second)
                            {
                                //ID
                                writeUint32(data+offset, it2.second->getHeadsetData().id);
                                offset += sizeof(uint32_t);

                                //Color
                                writeUint32(data+offset, it2.second->getHeadsetData().color);
                                offset += sizeof(uint32_t);

                                //Position
                                for(uint32_t i = 0; i < 3; i++, offset+=sizeof(float))
                                    writeFloat(data+offset, it2.second->getHeadsetData().position[i]);

                                //Rotation
                                for(uint32_t i = 0; i < 4; i++, offset+=sizeof(float))
                                    writeFloat(data+offset, it2.second->getHeadsetData().rotation[i]);

                                nbHeadset++;
                            }
                        }

                        //Write the number of headset to take account of
                        writeUint32(data+sizeof(uint16_t), nbHeadset);

                        std::shared_ptr<uint8_t> sharedData(data, free);
                        SocketMessage<int> sm(it.first, sharedData, offset);
                        writeMessage(sm);
                    }
                }
            }
            clock_gettime(CLOCK_REALTIME, &end);

            usleep(std::max(0.0, 1.e6/UPDATE_THREAD_FRAMERATE - (end.tv_nsec*1.e-3 + end.tv_sec*1.e6) +
                                                                (beg.tv_nsec*1.e-3 + beg.tv_sec*1.e6)));
        }
    }
}
