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

    /** \brief  All the colors that each headset are represented with */
    const uint32_t VFVServer::SCIVIS_DISTINGUISHABLE_COLORS[10] = {0xffe119, 0x4363d8, 0xf58231, 0xfabebe, 0xe6beff, 
                                                                   0x800000, 0x000075, 0xa9a9a9, 0xffffff, 0x000000};

    /* \brief  A free that does nothing
     * \param data the data to "not" free */
    void emptyFree(void* data){}

    /* \brief  Computed factorial of n
     * \param n the parameter
     * \return   n!  */
    uint32_t factorial(uint32_t n)
    {
        int res = 1;
        for(; n > 1; n--)
            res *= n;
        return res;
    }

    /**
     * \brief  Get the time offset (time of the day)
     *
     * \return   the time of the day
     */
    static time_t getTimeOffset()
    {
        struct timeval t;
        gettimeofday(&t, NULL);

        return 1e6 * t.tv_sec + t.tv_usec;
    }

    /** \brief  Get the string IP address of the headset. This function is used to save the targeted headset in json log functions
     * \param client the client to evaluate. Can either represent a tablet or a headset.
     *
     * \return   <ip>:Headset, <ip>:Tablet, None:Unknown or None:Tablet */
    static std::string getHeadsetIPAddr(VFVClientSocket* client)
    {
        bool isHeadset = false;
        bool isTablet = false;

        if(client)
        {
            isHeadset = client->isHeadset();
            isTablet = client->isTablet();
        }

        //Get headset IP address
        char headsetIP[1024];
        SOCKADDR_IN* headsetAddr = NULL;

        if(isHeadset)
            headsetAddr = &client->sockAddr;
        else if(isTablet && client->getTabletData().headset)
            headsetAddr = &client->getTabletData().headset->sockAddr;

        if(headsetAddr)
            inet_ntop(AF_INET, &headsetAddr->sin_addr, headsetIP, sizeof(headsetIP));
        else
            strcpy(headsetIP, "NONE");

        return std::string(headsetIP) + ':' + (isHeadset ? "Headset" : (isTablet ? "Tablet" : "Unknown"));
    }

    /** \brief  Get the MetaData of a SubDataset per headset. A MetaData permits to know meta information per headset (e.g., the private state of a subdataset)
     *
     * \param client the client owning the meta data
     * \param sd the SubDataset to search the meta data
     *
     * \return   the SubDatasetHeadsetInformation pointer. Do not delete it.  */
    static SubDatasetHeadsetInformation* getSubDatasetMetaData(VFVClientSocket* client, SubDataset* sd)
    {
        if(sd == NULL)
        {
            WARNING << "SubDataset 'sd' is NULL\n";
            return NULL;
        }

        SubDatasetHeadsetInformation* metaData = NULL;
        VFVHeadsetData* headset = NULL;

        if(client->isTablet())
        {
            if(client->getTabletData().headset)
                headset = &client->getTabletData().headset->getHeadsetData();
        }
        else if(client->isHeadset())
        {
            headset = &client->getHeadsetData();
        }

        if(headset)
        {
            auto it = headset->sdInfo.find(sd);
            if(it != headset->sdInfo.end())
                metaData = &it->second;
        }

        return metaData;
    }

    /**
     * \brief  Generate all the possible order based on the maximum number of techniques
     *
     * \param curID the current ID to generate the order for
     * \param printOrder should we print this order into the console?
     *
     * \return   the generated order
     */
    /*
    static std::vector<uint32_t> generateFactorialOrder(uint32_t curID, bool printOrder=false)
    {
        curID %= MAX_INTERACTION_TECHNIQUE_NUMBER;

        std::vector<uint32_t> orderAvailable(MAX_INTERACTION_TECHNIQUE_NUMBER);
        std::vector<uint32_t> trialOrder(MAX_INTERACTION_TECHNIQUE_NUMBER);

        //Fill all the distinguished value into an array
        for(uint32_t j = 0; j < MAX_INTERACTION_TECHNIQUE_NUMBER; j++)
            orderAvailable[j] = j;

        for(int32_t j = 0; j < MAX_INTERACTION_TECHNIQUE_NUMBER-1; j++) 
        {
            uint32_t div    = factorial(MAX_INTERACTION_TECHNIQUE_NUMBER-1-j);
            uint32_t target = curID / div;
            curID %= div;

            trialOrder[j] = orderAvailable[target];
            auto it = orderAvailable.begin();
            std::advance(it, target);
            orderAvailable.erase(it);
        }

        trialOrder[MAX_INTERACTION_TECHNIQUE_NUMBER-1] = orderAvailable[0];

        //Print the order
        if(printOrder)
        {
            INFO << "Technique Order : [";
            for(uint32_t j = 0; j < 3; j++)
                std::cout << trialOrder[j] << ",";
            std::cout << trialOrder[MAX_INTERACTION_TECHNIQUE_NUMBER-1] << "]\n";
        }

        return trialOrder;
    }
    */

    VFVServer::VFVServer(uint32_t nbThread, uint32_t port) : Server(nbThread, port)
    {
#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log.open("log.json", std::ios::out | std::ios::trunc);

            m_log << "{\n"
                  << "    \"data\" : [\n";

            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(NULL), getTimeOffset(), "OpenTheServer");
            m_log << "},\n";
        }
#endif

#ifdef CHI2020
        for(int i = 0; i < 3; i++)
            m_trialAnnotationPos[i] = 0;
        //Get the current pair ID (go from 0 to MAX_INTERACTION_TECHNIQUE_NUMBER! - 1)
        INFO << "Please, enter the pair ID for the CHI 2020 user study\n";
        std::cin >> m_pairID;
        INFO << "The pair ID is: " << m_pairID << std::endl;

        //Read the csv's position file
        std::ifstream csvPositionFile;
        csvPositionFile.open("position.csv", std::ios::in);

        csvPositionFile.seekg(0, std::ios_base::end);
        uint64_t csvFileSize = csvPositionFile.tellg();
        char* csvFileContent = (char*)malloc(sizeof(char)*csvFileSize+1);
        csvPositionFile.seekg(0, std::ios_base::beg);

        csvPositionFile.read(csvFileContent, csvFileSize);
        csvFileContent[csvFileSize-1] = '\0';

        //Get how many tokens the csv file contains
        std::stringstream convertor(csvFileContent);
        std::string token;

        uint64_t numTokens = 0;
        while(std::getline(convertor, token, ','))
            numTokens++;

        //Read all the tokens
        uint64_t trialPositionIdx=0;
        m_trialPositions = (float*)malloc(sizeof(float)*numTokens);
        convertor.clear();
        convertor.str(csvFileContent);
        while(std::getline(convertor, token, ','))
            m_trialPositions[trialPositionIdx++] = std::atof(token.c_str());

        //Free and close the file's content
        free(csvFileContent);
        csvPositionFile.close();

        //Set the pool of targets' positions
        for(uint32_t i = 0; i < TRIAL_TABLET_DATA_MAX_POOL_SIZE; i++)
        {
            uint64_t pos = rand()%(numTokens/3);

            for(uint8_t j = 0; j < 2; j++)
                m_trialTabletData[j].poolTargetPositionIdxStudy1[i] = m_trialTabletData[j].poolTargetPositionIdxStudy2[i] = pos;
        }

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(NULL), getTimeOffset(), "SetPairID");
            m_log << ", \"pairID\": " << m_pairID << "\n"
                  << "},\n";
        }
#endif

        m_trialTabletData[0].tabletID = 0;
        m_trialTabletData[1].tabletID = 1;

        //Set the technique IDs
        for(uint32_t i = 0; i < 2; i++)
        {
            for(uint32_t j = 0; j < MAX_INTERACTION_TECHNIQUE_NUMBER; j++)
                m_trialTabletData[i].techniqueOrder[j] = (j+m_pairID)%MAX_INTERACTION_TECHNIQUE_NUMBER;
        }

        //Add the dataset the users will play with
        VFVVTKDatasetInformation vtkInfo;
        vtkInfo.name = "Agulhas_10_resampled.vtk";
        vtkInfo.nbPtFields = 1;
        vtkInfo.ptFields.push_back(1);

#ifdef VFV_LOG_DATA
        m_log << vtkInfo.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(NULL), getTimeOffset());
        m_log << ",\n";
#endif

        addVTKDataset(NULL, vtkInfo);
#endif
    }

    VFVServer::VFVServer(VFVServer&& mvt) : Server(std::move(mvt))
    {
        m_updateThread     = mvt.m_updateThread;
        mvt.m_updateThread = NULL;
    }

    VFVServer::~VFVServer()
    {
        INFO << "Closing" << std::endl;
        for(auto& d : m_datasets)
            delete d.second;
#ifdef CHI2020
        if(m_trialPositions)
            free(m_trialPositions);
#endif
#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(NULL), getTimeOffset(), "CloseTheServer");
            m_log << "        }\n"
                  << "    ]\n"
                  << "}";
        }
#endif
    }

    bool VFVServer::launch()
    {
        //Init the available color 
        for(int32_t i = sizeof(SCIVIS_DISTINGUISHABLE_COLORS)/sizeof(SCIVIS_DISTINGUISHABLE_COLORS[0])-1; i >= 0; i--)
            m_availableHeadsetColors.push(SCIVIS_DISTINGUISHABLE_COLORS[i]);

        bool ret = Server::launch();
        m_updateThread = new std::thread(&VFVServer::updateThread, this);
        m_nextTrialThread = new std::thread(&VFVServer::nextTrialThread, this);

        return ret;
    }

    void VFVServer::cancel()
    {
        Server::cancel();
        if(m_updateThread && m_updateThread->joinable())
            pthread_cancel(m_updateThread->native_handle());
        if(m_nextTrialThread && m_nextTrialThread->joinable())
            pthread_cancel(m_nextTrialThread->native_handle());
    }

    void VFVServer::wait()
    {
        Server::wait();
        if(m_updateThread && m_updateThread->joinable())
            m_updateThread->join();
        if(m_nextTrialThread && m_nextTrialThread->joinable())
            m_nextTrialThread->join();
    }

    void VFVServer::closeServer()
    {
        Server::closeServer();
        if(m_updateThread != NULL)
        {
            delete m_updateThread;
            m_updateThread = 0;
        }

        if(m_nextTrialThread != NULL)
        {
            delete m_nextTrialThread;
            m_nextTrialThread = 0;
        }
    }

    void VFVServer::closeClient(SOCKET client)
    {
        m_mapMutex.lock();
            auto itClient = m_clientTable.find(client);
            if(itClient == m_clientTable.end())
            {
                m_mapMutex.unlock();
                return;
            } 
            auto c = itClient->second;

#ifdef VFV_LOG_DATA
            {
                std::lock_guard<std::mutex> logLock(m_logMutex);
                VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(c), getTimeOffset(), "DisconnectClient");
                m_log << ",  \"clientType\" : \"" << (c->isTablet() ? VFV_SENDER_TABLET : (c->isHeadset() ? VFV_SENDER_HEADSET : VFV_SENDER_UNKNOWN)) << "\"\n"
                      << "},\n";
            }
#endif


            INFO << "Disconnecting a client...\n";
            //Handle headset disconnections
            if(c->isHeadset())
            {
                INFO << "Disconnecting a headset client\n";
                m_availableHeadsetColors.push(c->getHeadsetData().color);
                m_nbConnectedHeadsets--;

                VFVClientSocket* tablet = c->getHeadsetData().tablet;
                if(tablet)
                {
                    tablet->getTabletData().headset = NULL;
                    c->getHeadsetData().tablet = NULL;
                    sendHeadsetBindingInfo(tablet, NULL);
                }

                if(m_headsetAnchorClient == c)
                {
                    m_headsetAnchorClient = NULL;
                    m_anchorData.finalize(false);
                }
            }

            //Handle table disconnections
            else if(c->isTablet())
            {
                INFO << "Disconnecting a tablet client\n";

                //Disconnect with the headset as well
                VFVClientSocket* headset = c->getTabletData().headset;
                if(headset)
                {
                    headset->getHeadsetData().tablet = NULL;
                    c->getTabletData().headset = NULL;
                    sendHeadsetBindingInfo(headset, headset);
                }
            }
        m_mapMutex.unlock();

        Server::closeClient(client);

        m_mapMutex.lock();
            askNewAnchor();
        m_mapMutex.unlock();
        INFO << "End of disconnection\n";
    }

    Dataset* VFVServer::getDataset(uint32_t datasetID, uint32_t sdID)
    {
        auto it = m_datasets.find(datasetID);
        if(it == m_datasets.end())
        {
            WARNING << "The dataset id 'datasetID' is not found\n";
            return NULL;
        }

        if(it->second->getNbSubDatasets() <= sdID)
        {
            WARNING << "The subdataset ID 'sdID' in dataset ID 'datasetID' is not found\n";
            return NULL;
        }

        return it->second;
    }

    MetaData* VFVServer::updateMetaDataModification(VFVClientSocket* client, uint32_t datasetID, uint32_t sdID)
    {
        MetaData* mt = NULL;
        auto it = m_vtkDatasets.find(datasetID);
        if(it != m_vtkDatasets.end())
        {
            if(it->second.sdMetaData.size() <= sdID)
                return NULL;
            mt = &it->second;
        }
        else
            return NULL;

        //Update time
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        mt->sdMetaData[sdID].lastModification = t.tv_nsec*1e-3 + t.tv_sec*1e6;

        //UPdate client
        if(client->isTablet() && client->getTabletData().headset)
        {
            if(mt->sdMetaData[sdID].hmdClient != client->getTabletData().headset)
            {
                mt->sdMetaData[sdID].hmdClient = client->getTabletData().headset;

                //Send owner to all the clients
                sendSubDatasetOwner(&mt->sdMetaData[sdID]);
            }
        }
        return mt;
    }

    void VFVServer::askNewAnchor()
    {
        //Re ask for a new anchor
        if(m_headsetAnchorClient == NULL)
        {
            m_anchorData.finalize(false);
            //Reset anchoring
            for(auto it : m_clientTable)
                if(it.second->isHeadset())
                    it.second->getHeadsetData().anchoringSent = false;

            for(auto it : m_clientTable)
                if(it.second->isHeadset())
                {
                    sendHeadsetBindingInfo(it.second, it.second);
                    break;
                }
        }
    }

    /*----------------------------------------------------------------------------*/
    /*----------------------------TREAT INCOMING DATA-----------------------------*/
    /*----------------------------------------------------------------------------*/

    void VFVServer::loginTablet(VFVClientSocket* client, const VFVIdentTabletInformation& identTablet)
    {
        bool alreadyConnected = client->isTablet();

        INFO << "Tablet connected.\n";
        std::lock_guard<std::mutex> lockDataset(m_datasetMutex);
        std::lock_guard<std::mutex> lock(m_mapMutex);

        if(!client->setAsTablet(identTablet.headsetIP))
        {
            VFVSERVER_NOT_A_TABLET
            return;
        }

        client->getTabletData().number = identTablet.tabletID;

        //Send already known datasets
        if(!alreadyConnected)
            onLoginSendCurrentStatus(client);

        //Tell the old bound headset the tablet disconnection
        if(client->getTabletData().headset)
        {
            sendHeadsetBindingInfo(client->getTabletData().headset, NULL);
            client->getTabletData().headset->getHeadsetData().tablet = NULL;
            client->getTabletData().headset = NULL;
        }

        //Useful if the packet was sent without headset information
        else if(identTablet.headsetIP.size() > 0)
        { 
            //Go through all the tablet to look for an already connected headset
            for(auto& clt : m_clientTable)
            {
                if(clt.second != client && clt.second->sockAddr.sin_addr.s_addr == client->getTabletData().headsetAddr.sin_addr.s_addr)
                {
                    INFO << "Headset found!\n";
                    clt.second->getHeadsetData().tablet = client;
                    client->getTabletData().headset     = clt.second;
                    sendHeadsetBindingInfo(clt.second, clt.second); //Send the headset the binding information
                    sendHeadsetBindingInfo(client,     clt.second); //Send the tablet the binding information
                    sendAllDatasetVisibility(client);
                    break;
                }
            }
        }

        INFO << std::endl;
    }

    void VFVServer::loginHeadset(VFVClientSocket* client)
    {
        std::lock_guard<std::mutex> lockDataset(m_datasetMutex);
        std::lock_guard<std::mutex> lock(m_mapMutex);

        bool alreadyConnected = client->isHeadset();

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

                    INFO << "Send the tablet the binding information\n";
                    sendHeadsetBindingInfo(clt.second, client); //Send the tablet the binding information

                    break;
                }
            }
        }

        //Set visualizable color
        if(!alreadyConnected)
        {
            client->getHeadsetData().color = m_availableHeadsetColors.top();
            m_availableHeadsetColors.pop();
            onLoginSendCurrentStatus(client);
        }

        //Add meta data of current opened datasets
        {
            for(auto& it : m_datasets)
            {
                for(uint32_t i = 0; i < it.second->getNbSubDatasets(); i++)
                {
                    SubDatasetHeadsetInformation info(it.second->getSubDataset(i));
                    std::pair<SubDataset*, SubDatasetHeadsetInformation> p(it.second->getSubDataset(i), info);
                    client->getHeadsetData().sdInfo.insert(p);
                }
            }
        }

        INFO << "End Connection" << std::endl;
    }

    void VFVServer::addVTKDataset(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset)
    {
        if(client != NULL && !client->isTablet())
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
            sd->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
        }

        VTKMetaData metaData;
        metaData.dataset = vtk;
        metaData.name    = dataset.name;
        metaData.ptFieldValueIndices   = dataset.ptFields;
        metaData.cellFieldValueIndices = dataset.cellFields;

        for(size_t i = 0; i < dataset.ptFields.size() + dataset.cellFields.size(); i++)
        {
            SubDatasetMetaData md;
            md.sdID = i;
            metaData.sdMetaData.push_back(md);
        }

        //Add it to the list
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        {
            metaData.datasetID = m_currentDataset;
            for(auto& it : metaData.sdMetaData)
                it.datasetID = m_currentDataset;
            m_vtkDatasets.insert(std::pair<uint32_t, VTKMetaData>(m_currentDataset, metaData));
            m_datasets.insert(std::pair<uint32_t, VTKDataset*>(m_currentDataset, vtk));
            m_currentDataset++;
        }

        //Send it to the other clients
        {
            std::lock_guard<std::mutex> lock2(m_mapMutex);
            for(auto clt : m_clientTable)
            {
                if(clt.second->isHeadset())
                {
                    for(uint32_t i = 0; i < vtk->getNbSubDatasets(); i++)
                    {
                        SubDataset* sd = vtk->getSubDataset(i);
                        SubDatasetHeadsetInformation info(sd);
                        std::pair<SubDataset*, SubDatasetHeadsetInformation> p(sd, info);
                        clt.second->getHeadsetData().sdInfo.insert(p);
                    }
                }

                sendAddVTKDatasetEvent(clt.second, dataset, metaData.datasetID);
                sendDatasetStatus(clt.second, vtk, metaData.datasetID);
            }
        }
    }

    void VFVServer::rotateSubDataset(VFVClientSocket* client, VFVRotationInformation& rotate)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        Dataset* dataset = getDataset(rotate.datasetID, rotate.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(rotate.datasetID, rotate.subDatasetID)
            return;
        }

        SubDataset* sd = dataset->getSubDataset(rotate.subDatasetID);

        if(!rotate.inPublic)
        {
            SubDatasetHeadsetInformation* sdMetaData = getSubDatasetMetaData(client, sd);
            if(!sdMetaData)
                return;

            sd = &sdMetaData->getPrivateSubDataset();
        }

        else if(client)
        {
            //Find the subdataset meta data and update it
            MetaData* mt = updateMetaDataModification(client, rotate.datasetID, rotate.subDatasetID);
            if(!mt)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(rotate.datasetID, rotate.subDatasetID)
                return;
            }
        }

        sd->setGlobalRotate(Quaternionf(rotate.quaternion[1], rotate.quaternion[2],
                                        rotate.quaternion[3], rotate.quaternion[0]));

        //Set the headsetID
        if(client)
        {
            if(client->isTablet() && client->getTabletData().headset)
                rotate.headsetID = client->getTabletData().headset->getHeadsetData().id;
            else if(client->isHeadset())
                rotate.headsetID = client->getHeadsetData().id;
        }

        //Send to all if public
        if(rotate.inPublic)
        {
            for(auto& clt : m_clientTable)
                if(clt.second != client)
                    sendRotateDatasetEvent(clt.second, rotate);
        }

        //Send just to the connected device counterpart if private
        else if(client)
        {
            if(client->isTablet() && client->getTabletData().headset)
                sendRotateDatasetEvent(client->getTabletData().headset, rotate);
            else if(client->isHeadset() && client->getHeadsetData().tablet)
                sendRotateDatasetEvent(client->getHeadsetData().tablet, rotate);
        }
    }

    void VFVServer::translateSubDataset(VFVClientSocket* client, VFVMoveInformation& translate)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        Dataset* dataset = getDataset(translate.datasetID, translate.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(translate.datasetID, translate.subDatasetID)
            return;
        }

        //Search for the meta data
        SubDataset* sd = dataset->getSubDataset(translate.subDatasetID);

        if(!translate.inPublic)
        {
            SubDatasetHeadsetInformation* sdMetaData = getSubDatasetMetaData(client, sd);
            if(!sdMetaData)
                return;

            sd = &sdMetaData->getPrivateSubDataset();
        }

        else if(client)
        {
            //Find the subdataset meta data and update it
            MetaData* mt = updateMetaDataModification(client, translate.datasetID, translate.subDatasetID);
            if(!mt)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(translate.datasetID, translate.subDatasetID)
                return;
            }
        }
        sd->setPosition(glm::vec3(translate.position[0], translate.position[1], translate.position[2]));

        //Set the headsetID
        if(client)
        {
            if(client->isTablet() && client->getTabletData().headset)
                translate.headsetID = client->getTabletData().headset->getHeadsetData().id;
            else if(client->isHeadset())
                translate.headsetID = client->getHeadsetData().id;
        }

        //Send to all if public
        if(translate.inPublic)
        {
            for(auto& clt : m_clientTable)
                if(clt.second != client)
                    sendMoveDatasetEvent(clt.second, translate);
        }

        //Send just to the connected device counterpart if private
        else if(client)
        {
            if(client->isTablet() && client->getTabletData().headset)
                sendMoveDatasetEvent(client->getTabletData().headset, translate);
            else if(client->isHeadset() && client->getHeadsetData().tablet)
                sendMoveDatasetEvent(client->getHeadsetData().tablet, translate);
        }
    }

    void VFVServer::scaleSubDataset(VFVClientSocket* client, VFVScaleInformation& scale)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        Dataset* dataset = getDataset(scale.datasetID, scale.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(scale.datasetID, scale.subDatasetID)
            return;
        }

        //Search for the meta data
        SubDataset* sd = dataset->getSubDataset(scale.subDatasetID);

        if(!scale.inPublic)
        {
            SubDatasetHeadsetInformation* sdMetaData = getSubDatasetMetaData(client, sd);
            if(!sdMetaData)
                return;

            sd = &sdMetaData->getPrivateSubDataset();
        }

        else if(client)
        {
            //Find the subdataset meta data and update it
            MetaData* mt = updateMetaDataModification(client, scale.datasetID, scale.subDatasetID);
            if(!mt)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(scale.datasetID, scale.subDatasetID)
                return;
            }
        }
        sd->setScale(glm::vec3(scale.scale[0], scale.scale[1], scale.scale[2]));

        //Set the headsetID
        if(client)
        {
            if(client->isTablet() && client->getTabletData().headset)
                scale.headsetID = client->getTabletData().headset->getHeadsetData().id;
            else if(client->isHeadset())
                scale.headsetID = client->getHeadsetData().id;
        }

        //Send to all if public
        if(scale.inPublic)
        {
            for(auto& clt : m_clientTable)
                if(clt.second != client)
                    sendScaleDatasetEvent(clt.second, scale);
        }

        //Send just to the connected device counterpart if private
        else if(client)
        {
            if(client->isTablet() && client->getTabletData().headset)
                sendScaleDatasetEvent(client->getTabletData().headset, scale);
            else if(client->isHeadset() && client->getHeadsetData().tablet)
                sendScaleDatasetEvent(client->getHeadsetData().tablet, scale);
        }
    }

    void VFVServer::setVisibility(VFVClientSocket* client, const VFVVisibilityDataset& visibility)
    {
        //Look for the counterPart to notify and the headset data
        std::lock_guard<std::mutex> lockDataset(m_datasetMutex);
        std::lock_guard<std::mutex> lock(m_mapMutex);
        VFVClientSocket* counterPart = NULL;
        VFVClientSocket* headset     = NULL;
        if(client->isTablet())
        {
            headset     = client->getTabletData().headset;
            counterPart = headset;
        }
        else if(client->isHeadset())
        {
            headset = client;
            counterPart = client->getHeadsetData().tablet;
        }
        if(!headset)
            return;

        Dataset* dataset = getDataset(visibility.datasetID, visibility.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(visibility.datasetID, visibility.subDatasetID);
            return;
        }

        //Search for the meta data
        SubDataset* sd = dataset->getSubDataset(visibility.subDatasetID);
        SubDatasetHeadsetInformation* sdMetaData = getSubDatasetMetaData(client, sd);

        if(!sdMetaData)
            return;

        sdMetaData->setVisibility(visibility.visibility);
        if(counterPart)
            sendVisibilityEvent(counterPart, visibility);
    }

    void VFVServer::updateHeadset(VFVClientSocket* client, const VFVUpdateHeadset& headset)
    {
        if(!client->isHeadset())
        {
            VFVSERVER_NOT_A_HEADSET  
            return;
        }

        auto& internalData = client->getHeadsetData();

        //Position and rotation
        for(uint32_t i = 0; i < 3; i++)
            internalData.position[i] = headset.position[i];
        for(uint32_t i = 0; i < 4; i++)
            internalData.rotation[i] = headset.rotation[i];

        //Pointing Data
        internalData.pointingData.pointingIT       = (VFVPointingIT)headset.pointingIT;
        internalData.pointingData.datasetID        = headset.pointingDatasetID;
        internalData.pointingData.subDatasetID     = headset.pointingSubDatasetID;
        internalData.pointingData.pointingInPublic = headset.pointingInPublic;
        for(uint32_t i = 0; i < 3; i++)
        {
            internalData.pointingData.localSDPosition[i]      = headset.pointingLocalSDPosition[i];
            internalData.pointingData.headsetStartPosition[i] = headset.pointingHeadsetStartPosition[i];
        }
    }

    void VFVServer::onStartAnnotation(VFVClientSocket* client, const VFVStartAnnotation& startAnnot)
    {
        std::lock_guard<std::mutex> lockMap(m_mapMutex);
        if(!client->isTablet())
        {
            VFVSERVER_NOT_A_TABLET
            return;
        }

        if(client->getTabletData().headset != NULL)
            sendStartAnnotation(client->getTabletData().headset, startAnnot);
    }

    void VFVServer::onAnchorAnnotation(VFVClientSocket* client, VFVAnchorAnnotation& anchorAnnot)
    {
        std::lock_guard<std::mutex> datasetLock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        if(!client && !anchorAnnot.inPublic)
        {
            WARNING << "Attending to write a private annotation without a client...\n";
            return;
        }
        uint32_t annotID = 0;
        uint32_t headsetID = -1;
        if(client != NULL) //Not the server
        {
            //Search for the headset ID
            VFVHeadsetData* headsetData = NULL;
            if(client->isTablet() && client->getTabletData().headset)
                headsetData = &client->getTabletData().headset->getHeadsetData();
            else if(client->isHeadset())
                headsetData = &client->getHeadsetData();

            if(!headsetData)
                return;
            headsetID = headsetData->id;
        }

        //Add the annotation first
        {
            Dataset* dataset = getDataset(anchorAnnot.datasetID, anchorAnnot.subDatasetID);
            if(dataset == NULL)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(anchorAnnot.datasetID, anchorAnnot.subDatasetID)
                return;
            }

            //Search for the meta data
            SubDataset* sd = dataset->getSubDataset(anchorAnnot.subDatasetID);

            if(!anchorAnnot.inPublic)
            {
                SubDatasetHeadsetInformation* sdMetaData  = NULL;
                sdMetaData = getSubDatasetMetaData(client, sd);
                if(!sdMetaData)
                    return;

                sd = &sdMetaData->getPrivateSubDataset();
            }
            annotID = sd->getAnnotations().size();
            sd->emplaceAnnotation(640, 640, anchorAnnot.localPos);
        }

        anchorAnnot.headsetID    = headsetID;
        anchorAnnot.annotationID = annotID;

        if(anchorAnnot.inPublic)
        {
            for(auto& clt : m_clientTable)
                sendAnchorAnnotation(clt.second, anchorAnnot);
        }
        else
        {
            if(client->isTablet() && client->getTabletData().headset)
                sendAnchorAnnotation(client->getTabletData().headset, anchorAnnot);
            else if(client->isHeadset() && client->getHeadsetData().tablet)
                sendAnchorAnnotation(client->getHeadsetData().tablet, anchorAnnot);
        }
    }

    void VFVServer::onClearAnnotations(VFVClientSocket* client, const VFVClearAnnotations& clearAnnots)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex); //Ensure that no one is touching the datasets
        std::lock_guard<std::mutex> lock2(m_mapMutex);    //Ensute that no one is modifying the list of clients (and relevant information)

        Dataset* dataset = getDataset(clearAnnots.datasetID, clearAnnots.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(clearAnnots.datasetID, clearAnnots.subDatasetID)
            return;
        }

        //Search for the meta data
        SubDataset* sd = dataset->getSubDataset(clearAnnots.subDatasetID);
        if(sd == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(clearAnnots.datasetID, clearAnnots.subDatasetID)
            return;
        }

        //Delete the annotations in the particular dataset
        if(clearAnnots.inPublic) //Message in the public space (no matter if server or client)
        {
            while(sd->getAnnotations().size())
                sd->removeAnnotation(sd->getAnnotations().begin());
        }

        else if(client != NULL) //If not the server (then the client)
        {
            SubDatasetHeadsetInformation* sdMetaData = getSubDatasetMetaData(client, sd);

            if(!sdMetaData)
                return;
            sd = &sdMetaData->getPrivateSubDataset();
            while(sd->getAnnotations().size())
                sd->removeAnnotation(sd->getAnnotations().begin());
        }

        else
        {
            WARNING << "Could not clear annotations in a non public space with client == NULL\n";
            return;
        }


        if(clearAnnots.inPublic)
        {
            for(auto& clt : m_clientTable)
                sendClearAnnotations(clt.second, clearAnnots);
        }

        else if(client)
        {
            if(client->isTablet() && client->getTabletData().headset)
                sendClearAnnotations(client->getTabletData().headset, clearAnnots);
            else if(client->isHeadset() && client->getHeadsetData().tablet)
                sendClearAnnotations(client->getHeadsetData().tablet, clearAnnots);
        }
    }

#ifdef CHI2020
    void VFVServer::onNextTrial(VFVClientSocket* client)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lock2(m_mapMutex);

        //Check alreay in next state
        if(m_waitSendNextTrial)
        {
            INFO << "Already waiting for the next trial to come. Discard.\n";
            return;
        }

        //Check the client. Is it a tablet?
        if(!client->isTablet())
        {
            VFVSERVER_NOT_A_TABLET
        }

        uint32_t id = client->getTabletData().number;

        if(id >= 2)
        {
            WARNING << "The maximum role ID should be 1 (included)\n";
            return;
        }

        if(m_trialTabletData[id].finishTraining)
        {
            if(id != m_currentTabletTrial)
            {
                WARNING << "Expected the tablet ID " << id << " to send the next trial...\n";
                return;
            }
        }
        else
        {
            INFO << "Tablet ID " << id << " has finished the training or break\n";
            m_trialTabletData[id].finishTraining = true;
            sendEmptyMessage(client, VFV_SEND_ACK_END_TRAINING);
        }

        //Send the next trial only if all the tablet finished the training
        if(m_trialTabletData[0].finishTraining && m_trialTabletData[1].finishTraining && id == m_currentTabletTrial)
        {
            INFO << "Received the next trial command by tablet ID " << id << std::endl;
            m_msWaitNextTrialTime = getTimeOffset() + TRIAL_WAITING_TIME;
            m_waitSendNextTrial = true;
        }
    }
#endif

    /*----------------------------------------------------------------------------*/
    /*-------------------------------SEND MESSAGES--------------------------------*/
    /*----------------------------------------------------------------------------*/

    void VFVServer::sendEmptyMessage(VFVClientSocket* client, uint16_t type)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t));
        writeUint16(data, type);

        INFO << "Sending EMPTY MESSAGE Event data. Type : " << type << std::endl;
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, sizeof(uint16_t));
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        VFVNoDataInformation noData;
        noData.type = type;
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << noData.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset())<< ",\n";
        }
#endif
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
        offset+= sizeof(uint32_t);
        for(int i : dataset.cellFields)
        {
            writeUint32(data+offset, i); //cellFieldValue[i]
            offset += sizeof(uint32_t);
        }

        INFO << "Sending ADD VTK DATASET Event data. File : " << dataset.name << "\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << dataset.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
        }
#endif
    }

    void VFVServer::sendRotateDatasetEvent(VFVClientSocket* client, const VFVRotationInformation& rotate)
    {
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + 4*sizeof(float) + sizeof(uint8_t);
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

        data[offset++] = rotate.inPublic;

        for(int i = 0; i < 4; i++, offset += sizeof(float)) //Quaternion rotation
            writeFloat(data+offset, rotate.quaternion[i]);

        INFO << "Sending ROTATE DATASET Event data. Data : " << rotate.datasetID << " sdID : " << rotate.subDatasetID
             << " Q = " << rotate.quaternion[0] << " " << rotate.quaternion[1] << " " << rotate.quaternion[2] << " " << rotate.quaternion[3] << "\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << rotate.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
        }
#endif
    }

    void VFVServer::sendScaleDatasetEvent(VFVClientSocket* client, const VFVScaleInformation& scale)
    {
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + 3*sizeof(float) + sizeof(uint8_t);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset=0;

        writeUint16(data, VFV_SEND_SCALE_DATASET); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, scale.datasetID); //The datasetID
        offset += sizeof(uint32_t);

        writeUint32(data+offset, scale.subDatasetID); //SubDataset ID
        offset += sizeof(uint32_t); 

        writeUint32(data+offset, scale.headsetID); //The headset ID
        offset += sizeof(uint32_t); 

        data[offset++] = scale.inPublic;

        for(int i = 0; i < 3; i++, offset += sizeof(float)) //3D Scaling
            writeFloat(data+offset, scale.scale[i]);

        INFO << "Sending SCALE DATASET Event data DatasetID " << scale.datasetID << " SubDataset ID " << scale.subDatasetID << " ["
             << scale.scale[0] << ", " << scale.scale[1] << ", " << scale.scale[2] << "]\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << scale.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
        }
#endif
    }

    void VFVServer::sendMoveDatasetEvent(VFVClientSocket* client, const VFVMoveInformation& position)
    {
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + 3*sizeof(float) + sizeof(uint8_t);
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

        data[offset++] = position.inPublic;

        for(int i = 0; i < 3; i++, offset += sizeof(float)) //Vector3 position
            writeFloat(data+offset, position.position[i]);

        INFO << "Sending MOVE DATASET Event data Dataset ID " << position.datasetID << " sdID : " << position.subDatasetID << " position : [" << position.position[0] << ", " << position.position[1] << ", " << position.position[2] << "]\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << position.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
        }
#endif
    }

    void VFVServer::onLoginSendCurrentStatus(VFVClientSocket* client)
    {
        //Send binding information
        if(client->isHeadset())
            sendHeadsetBindingInfo(client, client);
        else if(client->isTablet())
            sendHeadsetBindingInfo(client, client->getTabletData().headset);

        //Send common data
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
        {
            sendDatasetStatus(client, it.second, it.first);
            for(uint32_t i = 0; i < it.second->getNbSubDatasets(); i++)
            {
                SubDataset* sd = it.second->getSubDataset(i);
                for(uint32_t j = 0; j < sd->getAnnotations().size(); j++)
                {
                    VFVAnchorAnnotation anchorAnnot;
                    anchorAnnot.datasetID    = it.first;
                    anchorAnnot.subDatasetID = i;
                    anchorAnnot.inPublic     = 1;
                    anchorAnnot.annotationID = j;
                    auto annotIT = sd->getAnnotations().begin();
                    std::advance(annotIT, j);
                    for(uint32_t k = 0; k < 3; k++)
                        anchorAnnot.localPos[k] = (*annotIT)->getPosition()[k];
                    
                    sendAnchorAnnotation(client, anchorAnnot);
                }
            }
        }

        //Send anchoring data
        if(client->isHeadset())
            sendAnchoring(client);

        //Send next trial data
#ifdef CHI2020
        sendNextTrialDataCHI2020(client);
#endif
    }

    void VFVServer::sendAllDatasetVisibility(VFVClientSocket* client)
    {
        //Send visibility if linked
        if(client->isTablet() && client->getTabletData().headset)
        {
            VFVClientSocket* headset = client->getTabletData().headset;
            for(auto it : headset->getHeadsetData().sdInfo)
            {
                for(auto it2 : m_datasets)
                {
                    for(uint32_t i = 0; i < it2.second->getNbSubDatasets(); i++)
                    {
                        if(it2.second->getSubDataset(i) == it.first)
                        {
                            VFVVisibilityDataset vis;
                            vis.datasetID = it2.first;
                            vis.subDatasetID = i;
                            vis.visibility = it.second.getVisibility();
                            sendVisibilityEvent(client, vis);
                            goto endFor;
                        }
                    }
                }
endFor:
                continue;
            }
        }
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

            //Send scale
            VFVScaleInformation scale;
            scale.datasetID    = datasetID;
            scale.subDatasetID = i;
            for(uint32_t j = 0; j < 3; j++)
                scale.scale[j] = sd->getScale()[j];
            sendScaleDatasetEvent(client, scale);
        }
    }

    void VFVServer::sendAnnotationData(VFVClientSocket* client, Annotation* annot)
    {

    }

    void VFVServer::sendHeadsetBindingInfo(VFVClientSocket* client, VFVClientSocket* headset)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + 2*sizeof(uint8_t) + 3*sizeof(uint32_t));
        uint32_t offset = 0;

        //Type
        writeUint16(data+offset, VFV_SEND_HEADSET_BINDING_INFO);
        offset+=sizeof(uint16_t);

        uint32_t id    = -1;
        uint32_t color = 0x000000;
        bool tabletConnected = false;
        uint8_t firstConnected = 0;
        uint32_t tabletID = -1;

        if(headset)
        {
            id = headset->getHeadsetData().id;
            color = headset->getHeadsetData().color;
            tabletConnected = headset->getHeadsetData().tablet != NULL;
            if(tabletConnected)
                tabletID = headset->getHeadsetData().tablet->getTabletData().number;
        }

        //Headset ID
        writeUint32(data+offset, id);
        offset += sizeof(uint32_t);

        //Headset Color
        writeUint32(data+offset, color);
        offset+=sizeof(uint32_t);

        //Tablet connected
        data[offset++] = tabletConnected;

        //Tablet ID
        writeUint32(data+offset, tabletID);
        offset += sizeof(uint32_t);

        if(m_headsetAnchorClient == NULL)
        {
            for(auto& clt : m_clientTable)
            {
                if(clt.second == client)
                {
                    firstConnected = 1;
                    break;
                }
                else if(clt.second != client && clt.second->isHeadset())
                {
                    firstConnected = 0;
                    break;
                }
            }

            if(firstConnected && client == headset) //Send only if we are discussing with the headset and not the tablet
                m_headsetAnchorClient = headset;
        }
        data[offset++] = firstConnected;
        
        INFO << "Sending HEADSET BINDING INFO Event data\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);


#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "HeadsetBindingInfo");
            m_log << ",    \"headsetID\" : " << id << ",\n"
                  << "    \"color\" : " << color << ",\n"
                  << "    \"tabletConnected\" : " << tabletConnected << ",\n"
                  << "    \"tabletID\" : " << tabletID << ",\n"
                  << "    \"firstConnected\" : " << (bool)firstConnected << "\n"
                  << "},\n";
        }
#endif
    }

    void VFVServer::sendAnchoring(VFVClientSocket* client)
    {
        if(!m_anchorData.isCompleted() || !client->isHeadset())
            return;
        if(client->getHeadsetData().anchoringSent)
            return;

        //Send segment by segment
        for(auto& itSegment : m_anchorData.getSegmentData())
        {
            uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t)+sizeof(uint32_t));
            uint32_t offset = 0;

            //Header
            writeUint16(data, VFV_SEND_HEADSET_ANCHOR_SEGMENT);
            offset += sizeof(uint16_t);

            writeUint32(data+offset, itSegment.dataSize);
            offset += sizeof(uint32_t);

            //Header message
            std::shared_ptr<uint8_t> sharedData(data, free);
            SocketMessage<int> sm(client->socket, sharedData, offset);
            writeMessage(sm);

            //Segment message
            std::shared_ptr<uint8_t> sharedDataSegment(itSegment.data.get(), emptyFree);
            SocketMessage<int> smSegment(client->socket, sharedDataSegment, itSegment.dataSize);
            writeMessage(smSegment);

#ifdef VFV_LOG_DATA
            VFVDefaultByteArray byteArr;
            byteArr.type = VFV_SEND_HEADSET_ANCHOR_SEGMENT;
            byteArr.dataSize = itSegment.dataSize;
            {
                std::lock_guard<std::mutex> logLock(m_logMutex);
                m_log << byteArr.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
            }
#endif
        }

        sendEmptyMessage(client, VFV_SEND_HEADSET_ANCHOR_EOF);
        client->getHeadsetData().anchoringSent = true;
    }

    void VFVServer::sendAnchoring()
    {
        for(auto& it : m_clientTable)
            sendAnchoring(it.second);
        INFO << "Anchor sent to everyone" << std::endl;
    }

    void VFVServer::sendSubDatasetOwner(SubDatasetMetaData* metaData)
    {
        //Generate the data
        uint8_t* data   = (uint8_t*)malloc(sizeof(uint16_t) + 3*sizeof(uint32_t));
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_SUBDATASET_OWNER);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, metaData->datasetID);
        offset+= sizeof(uint32_t);

        writeUint32(data+offset, metaData->sdID);
        offset+= sizeof(uint32_t);

        uint32_t id = -1;
        if(metaData->hmdClient != NULL)
            id = metaData->hmdClient->getHeadsetData().id;
        writeUint32(data+offset, id); 
        offset+= sizeof(uint32_t);

        std::shared_ptr<uint8_t> sharedData(data, free);

        INFO << "Setting owner dataset ID " <<  metaData->datasetID << " sub dataset ID " << metaData->sdID << " headset ID" << id << std::endl;

        //Send the data
        for(auto it : m_clientTable)
        {
            SocketMessage<int> sm(it.second->socket, sharedData, offset);
            writeMessage(sm);

#ifdef VFV_LOG_DATA
            {
                std::lock_guard<std::mutex> logLock(m_logMutex);
                VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(it.second), getTimeOffset(), "SubDatasetOwner");
                m_log << ",    \"datasetID\"  : " << metaData->datasetID << ",\n"
                      << "    \"subDatasetID\" : " << metaData->sdID << ",\n"
                      << "    \"headsetID\" : " << id << "\n"
                      << "},\n";
            }
#endif
        }
    }

    void VFVServer::sendVisibilityEvent(VFVClientSocket* client, const VFVVisibilityDataset& visibility)
    {
        uint8_t* data   = (uint8_t*)malloc(sizeof(uint16_t) + 3*sizeof(uint32_t));
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_SET_VISIBILITY_DATASET);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, visibility.datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, visibility.subDatasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, visibility.visibility);
        offset += sizeof(uint32_t);

        std::shared_ptr<uint8_t> sharedData(data, free);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << visibility.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
        }
#endif
        
        INFO << "Sending new visibility : " << visibility.visibility << std::endl;
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
    }

    void VFVServer::sendStartAnnotation(VFVClientSocket* client, const VFVStartAnnotation& startAnnot)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + 3*sizeof(uint32_t) + 1);
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_START_ANNOTATION);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, startAnnot.datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, startAnnot.subDatasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, startAnnot.pointingID);
        offset += sizeof(uint32_t);

        data[offset++] = startAnnot.inPublic;

        std::shared_ptr<uint8_t> sharedData(data, free);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> lockJson(m_logMutex);
            m_log << startAnnot.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
        }
#endif

        INFO << "Sending start annotation \n";
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
    }

    void VFVServer::sendAnchorAnnotation(VFVClientSocket* client, const VFVAnchorAnnotation& anchorAnnot)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + 4*sizeof(uint32_t) + 3*sizeof(float) + 1);
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_ANCHOR_ANNOTATION);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, anchorAnnot.datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, anchorAnnot.subDatasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, anchorAnnot.annotationID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, anchorAnnot.headsetID);
        offset += sizeof(uint32_t);

        data[offset++] = anchorAnnot.inPublic;

        for(uint8_t i = 0; i < 3; i++)
        {
            writeFloat(data+offset, anchorAnnot.localPos[i]);
            offset += sizeof(float);
        }

        std::shared_ptr<uint8_t> sharedData(data, free);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> lockJson(m_logMutex);
            m_log << anchorAnnot.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
        }
#endif

        INFO << "Sending anchor annotation " << anchorAnnot.localPos[0] << "x" << anchorAnnot.localPos[1] << "x" << anchorAnnot.localPos[2] << "\n";
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
    }

    void VFVServer::sendClearAnnotations(VFVClientSocket* client, const VFVClearAnnotations& clearAnnot)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + 2*sizeof(uint32_t) + sizeof(uint8_t));
        uint32_t offset = 0;

        //ID
        writeUint16(data, VFV_SEND_CLEAR_ANNOTATION);
        offset += sizeof(uint16_t);

        //datasetID
        writeUint32(data+offset, clearAnnot.datasetID);
        offset += sizeof(uint32_t);

        //subdatasetID
        writeUint32(data+offset, clearAnnot.subDatasetID);
        offset += sizeof(uint32_t);

        data[offset++] = clearAnnot.inPublic;

        std::shared_ptr<uint8_t> sharedData(data, free);

        INFO << "Sending clear annotation \n";
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> lockJson(m_logMutex);
            m_log << clearAnnot.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
        }
#endif
    }

#ifdef CHI2020
    void VFVServer::sendNextTrialDataCHI2020(VFVClientSocket* client)
    {
        uint8_t* data   = (uint8_t*)malloc(sizeof(uint16_t) + 4*sizeof(uint32_t) + 3*sizeof(float));
        uint32_t offset = 0;
        uint32_t currentTechnique = m_trialTabletData[m_currentTabletTrial].techniqueOrder[m_currentTechniqueIdx];

        //ID
        writeUint16(data, VFV_SEND_NEXT_TRIAL_DATA_CHI2020);
        offset += sizeof(uint16_t);

        //Which tablet should anchor the annotation
        writeUint32(data+offset, m_currentTabletTrial);
        offset += sizeof(uint32_t);

        //The trial ID
        writeUint32(data+offset, m_currentTrialID);
        offset += sizeof(uint32_t);

        //The current study (1 or 2)
        writeUint32(data+offset, m_currentStudyID);
        offset += sizeof(uint32_t);

        //The current interaction technique to use
        writeUint32(data+offset, currentTechnique);
        offset += sizeof(uint32_t);

        //The annotation's position
        for(uint32_t i = 0; i < 3; i++, offset += sizeof(float))
            writeFloat(data+offset, m_trialAnnotationPos[i]);

        INFO << "Sending a next trial message data\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> lockJson(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "SendNextTrial");
            m_log << ",    \"currentTabletID\" : " << (int)m_currentTabletTrial << ",\n"
                  << "    \"currentTrialID\" : " << m_currentTrialID << ",\n"
                  << "    \"currentStudyID\" : " << m_currentStudyID << ",\n"
                  << "    \"annotationPos\" : [" << m_trialAnnotationPos[0] << "," << m_trialAnnotationPos[1] << "," << m_trialAnnotationPos[2] << "],\n"
                  << "    \"currentTechnique\" : " << currentTechnique << "\n"
                  << "},\n";
        }
#endif
    }
#endif

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

            if(msg.curMsg)
            {
#ifdef VFV_LOG_DATA
                bool isTablet  = client->isTablet();
                bool isHeadset = client->isHeadset();

                {
                    std::lock_guard<std::mutex> logLock(m_logMutex);
                    std::string str = msg.curMsg->toJson(isTablet ? VFV_SENDER_TABLET : (isHeadset ? VFV_SENDER_HEADSET : VFV_SENDER_UNKNOWN), getHeadsetIPAddr(client), getTimeOffset());

                    if(str.size())
                        m_log << str << ",\n";
                }
#endif
            }

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
                    std::lock_guard<std::mutex> lock(m_datasetMutex); //No dataset must be touched while we transmit anchor data
                    std::lock_guard<std::mutex> lockMap(m_mapMutex);

                    m_anchorData.finalize(msg.anchoringDataStatus.succeed);

                    if(msg.anchoringDataStatus.succeed == false)
                    {
                        INFO << "Asking for a new anchor\n";
                        m_headsetAnchorClient = NULL;
                        askNewAnchor();
                    }
                    else
                    {
                        client->getHeadsetData().anchoringSent = true;
                        sendAnchoring();
                    }
                    INFO << "End of anchoring data handling" << std::endl;
                    break;
                }
                case TRANSLATE_DATASET:
                {
                    translateSubDataset(client, msg.translate);
                    break;
                }
                case SCALE_DATASET:
                {
                    scaleSubDataset(client, msg.scale);
                    break;
                }

                case HEADSET_CURRENT_ACTION:
                {
                    //Look for the headset to modify
                    VFVClientSocket* headset = NULL;
                    if(client->isTablet())
                        headset = client->getTabletData().headset;
                    else if(client->isHeadset())
                        headset = client;
                    if(!headset)
                        break;

                    //Set the current action
                    std::lock_guard<std::mutex> lock(m_mapMutex);
                    headset->getHeadsetData().currentAction = (VFVHeadsetCurrentActionType)msg.headsetCurrentAction.action;
                    INFO << "Current action : " << msg.headsetCurrentAction.action << std::endl;
                    break;
                }
                case VISIBILITY_DATASET:
                {
                    setVisibility(client, msg.visibility);
                    break;
                }

                case START_ANNOTATION:
                {
                    onStartAnnotation(client, msg.startAnnotation);
                    break;
                }

                case ANCHOR_ANNOTATION:
                {
                    onAnchorAnnotation(client, msg.anchorAnnotation);
                    break;
                }

                case CLEAR_ANNOTATIONS:
                {
                    onClearAnnotations(client, msg.clearAnnotations);
                    break;
                }
#ifdef CHI2020
                case NEXT_TRIAL:
                {
                    onNextTrial(client);
                    break;
                }
#endif
                default:
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
            time_t endTime;

            clock_gettime(CLOCK_REALTIME, &beg);

            lockWriteThread();
            if(getBytesInWriting() < (1<<16))
            {
                unlockWriteThread();
                if(m_anchorData.isCompleted())
                {
                    std::lock_guard<std::mutex> lock2(m_datasetMutex);
                    std::lock_guard<std::mutex> lock(m_mapMutex);
                    //Send HEADSETS_STATUS
                    for(auto it : m_clientTable)
                    {
                        if(!it.second->isTablet() && !it.second->isHeadset())
                            continue;

                        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + sizeof(uint32_t) + 
                                                         MAX_NB_HEADSETS*(7*sizeof(float) + 3*sizeof(uint32_t) + 3*sizeof(uint32_t) + 1 + 6*sizeof(float)));
                        uint32_t offset    = 0;
                        uint32_t nbHeadset = 0;

                        //Type
                        writeUint16(data+offset, VFV_SEND_HEADSETS_STATUS);
                        offset += sizeof(uint16_t) + sizeof(uint32_t); //Write NB_HEADSET later
#ifdef LOG_UPDATE_HEAD
#ifdef VFV_LOG_DATA
                        m_logMutex.lock();
                        VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(it.second), getTimeOffset(), "HeadsetStatus");
                        m_log << ",    \"status\" : [";
                        bool logAdded = false;
#endif
#endif
                        for(auto& it2 : m_clientTable)
                        {
                            if(it2.second->isHeadset())
                            {
                                VFVHeadsetData& headsetData = it2.second->getHeadsetData();

                                //ID
                                writeUint32(data+offset, headsetData.id);
                                offset += sizeof(uint32_t);

                                //Color
                                writeUint32(data+offset, headsetData.color);
                                offset += sizeof(uint32_t);

                                //Current action
                                writeUint32(data+offset, headsetData.currentAction);
                                offset += sizeof(uint32_t);

                                //Position
                                for(uint32_t i = 0; i < 3; i++, offset+=sizeof(float))
                                    writeFloat(data+offset, headsetData.position[i]);

                                //Rotation
                                for(uint32_t i = 0; i < 4; i++, offset+=sizeof(float))
                                    writeFloat(data+offset, headsetData.rotation[i]);

                                //Pointing IT
                                writeUint32(data+offset, headsetData.pointingData.pointingIT);
                                offset += sizeof(uint32_t);

                                //Pointing Dataset ID
                                writeUint32(data+offset, headsetData.pointingData.datasetID);
                                offset += sizeof(uint32_t);

                                //Pointing SubDataset ID
                                writeUint32(data+offset, headsetData.pointingData.subDatasetID);
                                offset += sizeof(uint32_t);

                                //Pointing done in public space?
                                data[offset] = headsetData.pointingData.pointingInPublic ? 1 : 0;
                                offset++;

                                //Pointing local SD position
                                for(uint32_t i = 0; i < 3; i++, offset+=sizeof(float))
                                    writeFloat(data+offset, headsetData.pointingData.localSDPosition[i]);

                                //Pointing headset starting position
                                for(uint32_t i = 0; i < 3; i++, offset+=sizeof(float))
                                    writeFloat(data+offset, headsetData.pointingData.headsetStartPosition[i]);

#ifdef LOG_UPDATE_HEAD
#ifdef VFV_LOG_DATA
                                if(logAdded)
                                    m_log << ", ";

                                m_log << "{\n"
                                      << "    \"id\" : " << headsetData.id << ",\n"
                                      << "    \"color\" : " << headsetData.color << ",\n"
                                      << "    \"currentAction\" : " << headsetData.currentAction << ",\n"
                                      << "    \"position\" : [" << headsetData.position[0] << ", " << headsetData.position[1] << ", " << headsetData.position[2] << "],\n"
                                      << "    \"rotation\" : [" << headsetData.rotation[0] << ", " << headsetData.rotation[1] << ", " << headsetData.rotation[2] << ", " << headsetData.rotation[3] << "],\n"
                                      << "    \"pointingIT\" : " << headsetData.pointingData.pointingIT << ",\n"
                                      << "    \"pointingDatasetID\" : " << headsetData.pointingData.datasetID << ",\n"
                                      << "    \"pointingSubDatasetID\" : " << headsetData.pointingData.subDatasetID << ",\n"
                                      << "    \"pointingInPublic\" : " << headsetData.pointingData.pointingInPublic << ",\n"
                                      << "    \"pointingLocalSDPosition\" : [" << headsetData.pointingData.localSDPosition[0] << "," << headsetData.pointingData.localSDPosition[1] << "," << headsetData.pointingData.localSDPosition[2] << "],\n"
                                      << "    \"pointingHeadsetStartPosition\" : [" << headsetData.pointingData.headsetStartPosition[0] << "," << headsetData.pointingData.headsetStartPosition[1] << "," << headsetData.pointingData.headsetStartPosition[2] << "]\n"
                                      << "}\n";
                                logAdded = true;
#endif
#endif

                                nbHeadset++;
                            }
                        }

#ifdef LOG_UPDATE_HEAD
#ifdef VFV_LOG_DATA
                        m_log << "]},\n";
                        m_logMutex.unlock();
#endif
#endif

                        //Write the number of headset to take account of
                        writeUint32(data+sizeof(uint16_t), nbHeadset);

                        //Send the message to all
                        std::shared_ptr<uint8_t> sharedData(data, free);
                        SocketMessage<int> sm(it.first, sharedData, offset);
                        writeMessage(sm);
                    }
                }
                clock_gettime(CLOCK_REALTIME, &end);

                endTime = end.tv_nsec*1.e-3 + end.tv_sec*1.e6;

                //Check owner ending time
                {
                    for(auto& it : m_vtkDatasets)
                    {
                        for(auto& it2 : it.second.sdMetaData)
                        {
                            if(it2.hmdClient != NULL && endTime - it2.lastModification >= MAX_OWNER_TIME)
                            {
                                it2.hmdClient = NULL;
                                it2.lastModification = 0;
                                sendSubDatasetOwner(&it2);
                            }
                        }
                    }
                }
            }
            else
            {
                unlockWriteThread();
                clock_gettime(CLOCK_REALTIME, &end);
                endTime = end.tv_nsec*1.e-3 + end.tv_sec*1.e6;
            }

            usleep(std::max(0.0, 1.e6/UPDATE_THREAD_FRAMERATE - endTime + (beg.tv_nsec*1.e-3 + beg.tv_sec*1.e6)));
        }
    }

#ifdef CHI2020
    void VFVServer::nextTrialThread()
    {
        while(!m_closeThread)
        {
            m_datasetMutex.lock();

            //Should we send the next trial?
            if(m_waitSendNextTrial == true)
            {
                //Sleep for that much time
                time_t targetSleep = m_msWaitNextTrialTime;
                m_datasetMutex.unlock();

                if(getTimeOffset() < targetSleep)
                    usleep(targetSleep-getTimeOffset());

                VFVClearAnnotations clrAnnot;
                clrAnnot.datasetID    = 0;
                clrAnnot.subDatasetID = 0;
                clrAnnot.inPublic     = 1;

                onClearAnnotations(NULL, clrAnnot);

                m_datasetMutex.lock();
                {
                    m_currentTrialID++;

                    //Search for the next "tablet" and "headset" to be able to visualize the anchor
                    m_currentTabletTrial = (m_currentTabletTrial+1)%2;

                    if((m_currentStudyID == 1 && m_currentTrialID >= TRIAL_NUMBER_STUDY_1) ||
                       (m_currentStudyID == 2 && m_currentTrialID >= TRIAL_NUMBER_STUDY_2))
                    {
                        m_currentTrialID     = -1;
                        m_currentTabletTrial = 1; //This will be a 0 after the end of the break

                        if(m_currentTechniqueIdx == 3)
                        {
                            INFO << "\n---------------\n";
                            INFO << "SWITCHING STUDY\n";
                            INFO << "---------------\n";
                            m_currentStudyID++;
                            m_currentTechniqueIdx = 0;
                        }
                        else
                            m_currentTechniqueIdx++;

                        //This is for telling people that they can take a break
                        m_trialTabletData[0].finishTraining = false;
                        m_trialTabletData[1].finishTraining = false;
                    }

                    //Quit the training session
                    else if(m_currentStudyID == 0)
                    {
                        m_currentStudyID = 1;
                        m_currentTrialID = 0;
                        m_currentTabletTrial = 0;
                        m_currentTechniqueIdx = 0;
                    }

                    //Search what will be the next annotation's position
                    if(m_currentTrialID == -1)
                    {
                        for(int i = 0; i < 3; i++)
                            m_trialAnnotationPos[i] = 0;
                    }
                    else
                    {
                        if(m_currentStudyID == 1)
                            for(int i = 0; i < 3; i++)
                                m_trialAnnotationPos[i] = m_trialPositions[3*m_trialTabletData[m_currentTabletTrial].poolTargetPositionIdxStudy1[m_currentTrialID] + i];

                        else if(m_currentStudyID == 2)
                            for(int i = 0; i < 3; i++)
                                m_trialAnnotationPos[i] = m_trialPositions[3*m_trialTabletData[m_currentTabletTrial].poolTargetPositionIdxStudy2[m_currentTrialID] + i];
                    }


                    //Send the message to everyone
                    {
                        std::lock_guard<std::mutex> lock(m_mapMutex);
                        for(auto it : m_clientTable)
                            if(it.second->isTablet() || it.second->isHeadset())
                                sendNextTrialDataCHI2020(it.second);                                
                    }

                    //No more trial to send for now
                    m_waitSendNextTrial = false;

                    //We have finish the study. This is done after the whole computation because we need to send data information to all the devices
                    if(m_currentStudyID == 3)
                    {
                        INFO << "\n--------------\n";
                        INFO << "Study Finished\n";
                        INFO << "--------------\n\n";
                        m_datasetMutex.unlock();
                        return;
                    }
                }
                m_datasetMutex.unlock();
            }
            else
            {
                m_datasetMutex.unlock();
                usleep(8);
            }
        }
    }
#endif
}
