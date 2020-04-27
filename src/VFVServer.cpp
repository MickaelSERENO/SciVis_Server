#include "VFVServer.h"
#include "TransferFunction/GTF.h"
#include "TransferFunction/TriangularGTF.h"
#include <random>
#include <algorithm>

#ifndef TEST
//#define TEST
#endif

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
    }

#define VFVSERVER_SUB_DATASET_NOT_FOUND(datasetID, subDatasetID)\
    {\
        WARNING << "Sub Dataset ID " << (subDatasetID) << " of dataset ID " << (datasetID) << " not found... disconnecting the client!"; \
    }

#define VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, datasetID, subDatasetID)\
    {\
        VFVClientSocket* _hmd = getHeadsetFromClient(client);\
        if(_hmd)\
            WARNING << "Client ID" << (client)->getHeadsetData().id << " cannot modify Sub Dataset ID " << (subDatasetID) << " of dataset ID " << (datasetID) << " not found... disconnecting the client!"; \
        else\
            WARNING << "Unknown client cannot modify Sub Dataset ID " << (subDatasetID) << " of dataset ID " << (datasetID) << " not found... disconnecting the client!"; \
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

    TF* cloneTransferFunction(TFType type, std::shared_ptr<const TF> tf)
    {
        TF* _tf = NULL;
        switch(type)
        {
            case TF_TRIANGULAR_GTF:
                _tf = new TriangularGTF(*(const TriangularGTF*)tf.get());
                break;
            case TF_GTF:
                _tf = new GTF(*(const GTF*)tf.get());
                break;
            default:
                WARNING << "Did not find TFType " << type << std::endl;
                break;
        }

        return _tf;
    } 


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
            m_log << std::flush;
        }
#endif

#ifdef TEST
        //Add the dataset the users will play with
        VFVVTKDatasetInformation vtkInfo;
        vtkInfo.name = "Agulhas_10_resampled.vtk";
        vtkInfo.nbPtFields = 1;
        vtkInfo.ptFields.push_back(1);

#ifdef VFV_LOG_DATA
        m_log << vtkInfo.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(NULL), getTimeOffset());
        m_log << ",\n";
        m_log << std::flush;
#endif
        addVTKDataset(NULL, vtkInfo);

        m_vtkDatasets[0].dataset->loadValues([](Dataset* dataset, uint32_t status, void* data)
        {
            INFO << "Loaded. Status: " << status << std::endl;
        }, NULL);
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
#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(NULL), getTimeOffset(), "CloseTheServer");
            m_log << "        }\n"
                  << "    ]\n"
                  << "}";
            m_log << std::flush;
            m_log.close();
        }
#endif

        //Delete meta data information
        for(auto& d : m_vtkDatasets)
            for(auto& sd : d.second.sdMetaData)
                sd.tf = NULL;

        for(auto& d : m_binaryDatasets)
            for(auto& sd : d.second.sdMetaData)
                sd.tf = NULL;

        //delete datasets
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

    void VFVServer::cancel()
    {
        Server::cancel();
        if(m_updateThread && m_updateThread->joinable())
            pthread_cancel(m_updateThread->native_handle());
    }

    void VFVServer::wait()
    {
        Server::wait();
        if(m_updateThread && m_updateThread->joinable())
            m_updateThread->join();
    }

    void VFVServer::closeServer()
    {
        Server::closeServer();
        if(m_updateThread != NULL)
        {
            delete m_updateThread;
            m_updateThread = 0;
        }
    }

    void VFVServer::closeClient(SOCKET client)
    {
        auto itClient = m_clientTable.find(client);
        if(itClient == m_clientTable.end())
        {
            return;
        } 
        auto c = itClient->second;

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(c), getTimeOffset(), "DisconnectClient");
            m_log << ",  \"clientType\" : \"" << (c->isTablet() ? VFV_SENDER_TABLET : (c->isHeadset() ? VFV_SENDER_HEADSET : VFV_SENDER_UNKNOWN)) << "\"\n"
                  << "},\n";
            m_log << std::flush;
        }
#endif


        INFO << "Disconnecting a client...\n";
        //Handle headset disconnections
        if(c->isHeadset())
        {
            INFO << "Disconnecting a headset client\n";

            auto f = [c,this](MetaData& mtData) 
            {
                for(SubDatasetMetaData& sdMT : mtData.sdMetaData)
                    if(sdMT.owner == c)
                    {
                        VFVRemoveSubDataset removeEvent;
                        removeEvent.datasetID    = sdMT.datasetID;
                        removeEvent.subDatasetID = sdMT.sdID;
                        removeSubDataset(removeEvent);
                    }
            };

            for(auto& it : m_vtkDatasets)
                f(it.second);
            for(auto& it : m_binaryDatasets)
                f(it.second);

            //Put available that color
            m_availableHeadsetColors.push(c->getHeadsetData().color);
            m_nbConnectedHeadsets--;

            VFVClientSocket* tablet = c->getHeadsetData().tablet;
            if(tablet)
            {
                tablet->getTabletData().headset = NULL;
                c->getHeadsetData().tablet = NULL;
                sendHeadsetBindingInfo(tablet);
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
                sendHeadsetBindingInfo(headset);
            }
        }

        Server::closeClient(client);

        askNewAnchor();
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

        if(it->second->getSubDataset(sdID) == NULL)
        {
            WARNING << "The subdataset ID 'sdID' in dataset ID 'datasetID' is not found\n";
            return NULL;
        }

        return it->second;
    }

    MetaData* VFVServer::getMetaData(uint32_t datasetID, uint32_t sdID, SubDatasetMetaData** sdMTPtr)
    {
        MetaData* mt = NULL;
        SubDatasetMetaData* sdMT = NULL;
        auto it = m_vtkDatasets.find(datasetID);
        if(it != m_vtkDatasets.end())
        {
            sdMT = it->second.getSDMetaDataByID(sdID);
            mt = &it->second;
        }
        else
            return NULL;

        if(sdMTPtr)
            *sdMTPtr = sdMT;

        return mt;
    }

    MetaData* VFVServer::updateMetaDataModification(VFVClientSocket* client, uint32_t datasetID, uint32_t sdID, SubDatasetMetaData** sdMT)
    {
        MetaData* mt = NULL;
        SubDatasetMetaData* _sdMT;
        if((mt = getMetaData(datasetID, sdID, &_sdMT)) == NULL || sdMT == NULL)
            return NULL;

        //Return sdMT
        if(sdMT)
            *sdMT = _sdMT;

        //Update time
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        _sdMT->lastModification = t.tv_nsec*1e-3 + t.tv_sec*1e6;

        //UPdate client
        if(client->isTablet() && client->getTabletData().headset)
        {
            if(_sdMT->hmdClient != client->getTabletData().headset)
            {
                _sdMT->hmdClient = client->getTabletData().headset;

                //Send owner to all the clients
                sendSubDatasetLockOwner(_sdMT);
            }
        }
        return mt;
    }

    VFVClientSocket* VFVServer::getHeadsetFromClient(VFVClientSocket* client)
    {
        if(client->isTablet())
            return client->getTabletData().headset;
        else if(client->isHeadset())
            return client;
        return NULL;
    }

    bool VFVServer::canModifySubDataset(VFVClientSocket* client, SubDatasetMetaData* sdMT)
    {
        if(client == NULL) //The server
            return true;

        VFVClientSocket* headset = getHeadsetFromClient(client);

        //Public subdataset
        if(sdMT->owner == NULL)
        {
            //Check if some is touching it
            if(sdMT->hmdClient == NULL)
                return true;
            if(sdMT->hmdClient == headset)
                return true;
        }

        //Private subdataset
        else
        {
            if(sdMT->owner == headset)
                return true;
        }

        return false;
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
                    sendHeadsetBindingInfo(it.second);
                    break;
                }
        }
    }

    /*----------------------------------------------------------------------------*/
    /*----------------------------TREAT INCOMING DATA-----------------------------*/
    /*----------------------------------------------------------------------------*/

    void VFVServer::loginTablet(VFVClientSocket* client, const VFVIdentTabletInformation& identTablet)
    {
        std::lock_guard<std::mutex> lockDataset(m_datasetMutex);
        std::lock_guard<std::mutex> lock(m_mapMutex);

        bool alreadyConnected = client->isTablet();

        INFO << "Tablet connected.\n";
        if(!client->setAsTablet(identTablet.headsetIP, (VFVHandedness)identTablet.handedness))
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
            client->getTabletData().headset->getHeadsetData().tablet = NULL;
            client->getTabletData().headset = NULL;
            sendHeadsetBindingInfo(client->getTabletData().headset);
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
                    sendHeadsetBindingInfo(clt.second); //Send the headset the binding information
                    sendHeadsetBindingInfo(client);     //Send the tablet the binding information
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

        //Set visualizable color
        if(!alreadyConnected)
        {
            client->getHeadsetData().color = m_availableHeadsetColors.top();
            m_availableHeadsetColors.pop();
            onLoginSendCurrentStatus(client);
        }

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
                    sendHeadsetBindingInfo(clt.second); //Send the tablet the binding information

                    break;
                }
            }
        }
        INFO << "End Connection" << std::endl;
    }

    void VFVServer::addVTKDataset(VFVClientSocket* client, const VFVVTKDatasetInformation& dataset)
    {
        if(client != NULL && !client->isTablet())
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
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
            SubDataset* sd = vtk->getSubDatasets()[i];
            sd->setPosition(glm::vec3(m_currentSubDataset*2.0f, 0.0f, 0.0f));
            sd->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
        }

        VTKMetaData metaData;
        metaData.dataset = vtk;
        metaData.name    = dataset.name;
        metaData.ptFieldValueIndices   = dataset.ptFields;
        metaData.cellFieldValueIndices = dataset.cellFields;

        for(uint32_t i = 0; i < vtk->getNbSubDatasets(); i++, m_currentSubDataset++)
        {
            SubDatasetMetaData md;
            md.sdID   = vtk->getSubDatasets()[i]->getID();
            md.tf     = std::make_shared<TriangularGTF>(dataset.ptFields.size(), RAINBOW);
            md.tfType = TF_TRIANGULAR_GTF;
            vtk->getSubDatasets()[i]->setTransferFunction(md.tf);
            metaData.sdMetaData.push_back(md);
        }

        //Add it to the list
        {
            std::lock_guard<std::mutex> lock(m_datasetMutex);
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
                sendAddVTKDatasetEvent(clt.second, dataset, metaData.datasetID);
                sendDatasetStatus(clt.second, vtk, metaData.datasetID);
            }
        }

        //Add a SubDataset if no one is registered yet
        if(vtk->getNbSubDatasets()        == 0 &&
           vtk->getPtFieldValues().size() != 0)
        {
            VFVAddSubDataset addSubDataset;
            addSubDataset.datasetID = metaData.datasetID;
            onAddSubDataset(NULL, addSubDataset);
        }
    }

    SubDataset* VFVServer::onAddSubDataset(VFVClientSocket* client, const VFVAddSubDataset& dataset)
    {
        INFO << "OnAddSubDataset" << std::endl;
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        auto it = m_datasets.find(dataset.datasetID);
        if(it == m_datasets.end())
        {
            WARNING << "The dataset id 'datasetID' is not found\n";
            return NULL;
        }

        //Add a dataset
        Dataset* d = it->second;
        MetaData* mt = getMetaData(dataset.datasetID, 0, NULL);
        if(mt == NULL)
        {
            ERROR << "Could not find the Meta Data of Dataset ID: " << dataset.datasetID << std::endl;
            return NULL;
        }
        SubDataset* sd = new SubDataset(d, std::to_string(d->getNbSubDatasets()+1), 0);
        d->addSubDataset(sd);
                
        SubDatasetMetaData md;
        md.sdID   = sd->getID();
        md.datasetID = dataset.datasetID;
        md.owner  = (dataset.isPublic ? NULL : getHeadsetFromClient(client));
        md.tf     = std::make_shared<TriangularGTF>(d->getPointFieldDescs().size()+1, RAINBOW);
        md.tfType = TF_TRIANGULAR_GTF;
        sd->setTransferFunction(md.tf);
        mt->sdMetaData.push_back(md);

        std::lock_guard<std::mutex> lock2(m_mapMutex);
        for(auto& clt : m_clientTable)
        {
            sendAddSubDataset(clt.second, sd);
            sendSubDatasetStatus(clt.second, sd, dataset.datasetID);
        }

        return sd;
    }

    void VFVServer::removeSubDataset(const VFVRemoveSubDataset& remove)
    {
        Dataset* dataset = getDataset(remove.datasetID, remove.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(remove.datasetID, remove.subDatasetID)
            return;
        }
        SubDataset* sd = dataset->getSubDataset(remove.subDatasetID);

        auto f = [sd, &remove](MetaData& mtData)
        {
            for(auto it = mtData.sdMetaData.begin(); it != mtData.sdMetaData.end();)
            {
                if(it->datasetID == remove.datasetID && it->sdID == remove.subDatasetID)
                    it = mtData.sdMetaData.erase(it);
                else
                    it++;
            }
        };

        for(auto it : m_binaryDatasets)
            f(it.second);
        for(auto it : m_vtkDatasets)
            f(it.second);

        //Remove the subdataset
        dataset->removeSubDataset(sd);

        //Tells all the clients
        for(auto& clt : m_clientTable)
            sendRemoveSubDatasetEvent(clt.second, remove);
    }

    void VFVServer::onMakeSubDatasetPublic(VFVClientSocket* client, const VFVMakeSubDatasetPublic& makePublic)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        if(client)
        {
            //Find the subdataset meta data and update it
            SubDatasetMetaData* sdMT = NULL;
            getMetaData(makePublic.datasetID, makePublic.subDatasetID, &sdMT);
            if(!sdMT)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(makePublic.datasetID, makePublic.subDatasetID)
                return;
            }

            VFVClientSocket* headset = getHeadsetFromClient(client);

            //Check about the privacy
            if(sdMT->owner == NULL || sdMT->owner != headset)
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, makePublic.datasetID, makePublic.subDatasetID)
                return;
            }

            sdMT->owner = NULL;

            //Send to all
            sendSubDatasetOwner(sdMT);
        }
    }

    void VFVServer::onDuplicateSubDataset(VFVClientSocket* client, const VFVDuplicateSubDataset& duplicate)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        //Find the subdataset meta data
        SubDatasetMetaData* sdMT = NULL;
        MetaData* mt = getMetaData(duplicate.datasetID, duplicate.subDatasetID, &sdMT);
        if(!mt)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(duplicate.datasetID, duplicate.subDatasetID)
            return;
        }

        if(client)
        {
            //Check about the privacy
            if(sdMT->owner != NULL && sdMT->owner != getHeadsetFromClient(client))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, duplicate.datasetID, duplicate.subDatasetID)
                return;
            }
        }

        //Find the SubDataset
        Dataset* dataset = getDataset(duplicate.datasetID, duplicate.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(duplicate.datasetID, duplicate.subDatasetID)
            return;
        }
        SubDataset* sdToDuplicate = dataset->getSubDataset(duplicate.subDatasetID);

        //Create a SubDataset
        SubDataset* sd = new SubDataset(dataset, sdToDuplicate->getName(), 0);
        dataset->addSubDataset(sd);

        //Copy data and Generate the corresponding MetaData
        sd->setGlobalRotate(sdToDuplicate->getGlobalRotate());
        sd->setScale(sdToDuplicate->getScale());
        //WE DO NOT TOUCH THE POSITION!
                
        SubDatasetMetaData md;
        md.sdID   = sd->getID();
        md.datasetID = duplicate.datasetID;
        md.tfType = sdMT->tfType;
        md.tf     = std::shared_ptr<TF>(cloneTransferFunction(sdMT->tfType, sdMT->tf));
        md.owner  = sdMT->owner;
        sd->setTransferFunction(md.tf);
        mt->sdMetaData.push_back(md);

        //Send it to all the clients
        for(auto& clt : m_clientTable)
        {
            sendAddSubDataset(clt.second, sd);
            sendSubDatasetStatus(clt.second, sd, duplicate.datasetID);
        }
    }

    void VFVServer::onRemoveSubDataset(VFVClientSocket* client, const VFVRemoveSubDataset& remove)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        //Find the subdataset meta data
        SubDatasetMetaData* sdMT = NULL;
        MetaData* mt = getMetaData(remove.datasetID, remove.subDatasetID, &sdMT);
        if(!mt)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(remove.datasetID, remove.subDatasetID)
            return;
        }

        if(client)
        {
            //Check about the privacy
            if(!canModifySubDataset(client, sdMT))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, remove.datasetID, remove.subDatasetID)
                return;
            }
        }

        removeSubDataset(remove);
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

        if(client)
        {
            //Find the subdataset meta data and update it
            SubDatasetMetaData* sdMT = NULL;
            MetaData* mt = updateMetaDataModification(client, rotate.datasetID, rotate.subDatasetID, &sdMT);
            if(!mt)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(rotate.datasetID, rotate.subDatasetID)
                return;
            }

            //Check about the privacy
            if(!canModifySubDataset(client, sdMT))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, rotate.datasetID, rotate.subDatasetID)
                return;
            }
        }

        sd->setGlobalRotate(Quaternionf(rotate.quaternion[1], rotate.quaternion[2],
                                        rotate.quaternion[3], rotate.quaternion[0]));

        //Set the headsetID
        if(client)
        {
            VFVClientSocket* headset = getHeadsetFromClient(client);
            if(headset)
                rotate.headsetID = headset->getHeadsetData().id;
        }

        //Send to all
        for(auto& clt : m_clientTable)
            if(clt.second != client)
                sendRotateDatasetEvent(clt.second, rotate);
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

        if(client)
        {
            //Find the subdataset meta data and update it
            SubDatasetMetaData* sdMT = NULL;
            MetaData* mt = updateMetaDataModification(client, translate.datasetID, translate.subDatasetID, &sdMT);
            if(!mt)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(translate.datasetID, translate.subDatasetID)
                return;
            }

            //Check about the privacy
            if(!canModifySubDataset(client, sdMT))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, translate.datasetID, translate.subDatasetID)
                return;
            }
        }
        sd->setPosition(glm::vec3(translate.position[0], translate.position[1], translate.position[2]));

        //Set the headsetID
        if(client)
        {
            VFVClientSocket* headset = getHeadsetFromClient(client);
            if(headset)
                translate.headsetID = headset->getHeadsetData().id;
        }

        for(auto& clt : m_clientTable)
            if(clt.second != client)
                sendMoveDatasetEvent(clt.second, translate);
    }

    void VFVServer::tfSubDataset(VFVClientSocket* client, VFVTransferFunctionSubDataset& tfSD)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        Dataset* dataset = getDataset(tfSD.datasetID, tfSD.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(tfSD.datasetID, tfSD.subDatasetID)
            return;
        }

        //Search for the meta data
        SubDataset* sd = dataset->getSubDataset(tfSD.subDatasetID);

        SubDatasetMetaData* sdMT = NULL;
        MetaData* mt = NULL;
        mt = getMetaData(tfSD.datasetID, tfSD.subDatasetID, &sdMT);
        if(!mt || !sdMT)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(tfSD.datasetID, tfSD.subDatasetID)
            return;
        }

        //Check about the privacy
        if(!canModifySubDataset(client, sdMT))
        {
            VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, tfSD.datasetID, tfSD.subDatasetID)
            return;
        }

        if(client)
            updateMetaDataModification(client, tfSD.datasetID, tfSD.subDatasetID);

        if(tfSD.gtfData.propData.size() != sd->getParent()->getPointFieldDescs().size())
            WARNING << "Attemps to use a transfer function with too few parameters... warning!\n";

        //Now, update the transfer function
        //
        //First check if the type has changes
        if(tfSD.tfID != sdMT->tfType)
        {
            if(sdMT->tf != NULL)
                sdMT->tf = NULL;

            sdMT->tfType = (TFType)tfSD.tfID;

            //Reallocate
            switch(sdMT->tfType)
            {
                case TF_GTF:
                    sdMT->tf = std::make_shared<GTF>(tfSD.gtfData.propData.size(), (ColorMode)tfSD.colorMode);
                    break;
                case TF_TRIANGULAR_GTF:
                    sdMT->tf = std::make_shared<TriangularGTF>(tfSD.gtfData.propData.size()+1, (ColorMode)tfSD.colorMode);
                    break;
                default:
                    ERROR << "The Transfer Function type: " << (int)tfSD.tfID << " is unknown. Set the NONE\n";
                    sdMT->tfType = TF_NONE;
                    sdMT->tf     = NULL;
                    break;
                sd->setTransferFunction(sdMT->tf);
            }
        }
        else
            sdMT->tf->setColorMode((ColorMode)tfSD.colorMode);

        //Update the corresponding one
        if(sdMT->tfType != TF_NONE)
        {
            switch(sdMT->tfType)
            {
                case TF_GTF:
                case TF_TRIANGULAR_GTF:
                {
                    //Get the ordered array of centers and scaling
                    float* centers = (float*)malloc(sizeof(float)*sdMT->tf->getDimension());
                    float* scales  = (float*)malloc(sizeof(float)*sdMT->tf->getDimension());

                    for(uint32_t i = 0; i < tfSD.gtfData.propData.size(); i++)
                    {
                        //Look for corresponding ID...
                        uint32_t tfID = sd->getParent()->getTFIndiceFromPointFieldID(tfSD.gtfData.propData[i].propID);
                        if(tfID != (uint32_t)-1 && tfID < sdMT->tf->getDimension())
                        {
                            centers[tfID] = tfSD.gtfData.propData[i].center;
                            scales[tfID]  = tfSD.gtfData.propData[i].scale;
                        }
                    }

                    //Set the center and scaling factors
                    if(sdMT->tfType == TF_GTF)
                    {
                        ((GTF*)sdMT->tf.get())->setCenter(centers);
                        ((GTF*)sdMT->tf.get())->setScale(scales);
                    }

                    else if(sdMT->tfType == TF_TRIANGULAR_GTF)
                    {
                        ((TriangularGTF*)sdMT->tf.get())->setCenter(centers);
                        ((TriangularGTF*)sdMT->tf.get())->setScale(scales);
                    }

                    //Free data
                    free(centers);
                    free(scales);
                    break;
                }
                default: //Should never come here
                    ERROR << "Missing the handle for one Transfer Function Type: " << sdMT->tfType << std::endl;
                    break;
            }
        }

        //Set the headsetID
        if(client)
        {
            VFVClientSocket* headset = getHeadsetFromClient(client);
            if(headset)
                tfSD.headsetID = headset->getHeadsetData().id;
        }

        //Send to all
        for(auto& clt : m_clientTable)
            if(clt.second != client)
                sendTransferFunctionDataset(clt.second, tfSD);
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

        if(client)
        {
            //Find the subdataset meta data and update it
            SubDatasetMetaData* sdMT = NULL;
            MetaData* mt = updateMetaDataModification(client, scale.datasetID, scale.subDatasetID, &sdMT);
            if(!mt)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(scale.datasetID, scale.subDatasetID)
                return;
            }

            //Check about the privacy
            if(!canModifySubDataset(client, sdMT))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, scale.datasetID, scale.subDatasetID)
                return;
            }
        }

        sd->setScale(glm::vec3(scale.scale[0], scale.scale[1], scale.scale[2]));

        //Set the headsetID
        if(client)
        {
            VFVClientSocket* headset = getHeadsetFromClient(client);
            if(headset)
                scale.headsetID = headset->getHeadsetData().id;
        }

        //Send to all
        for(auto& clt : m_clientTable)
            if(clt.second != client)
                sendScaleDatasetEvent(clt.second, scale);
    }

    void VFVServer::updateHeadset(VFVClientSocket* client, const VFVUpdateHeadset& headset)
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
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
        for(uint32_t i = 0; i < 4; i++)
            internalData.pointingData.headsetStartOrientation[i] = headset.pointingHeadsetStartOrientation[i];
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

        uint32_t annotID = 0;
        uint32_t headsetID = -1;
        if(client != NULL) //Not the server
        {
            VFVClientSocket* hmdClient = getHeadsetFromClient(client);
            if(hmdClient == NULL)
            {
                WARNING << "Not connected to a headset yet..." << std::endl;
                return;
            }
            headsetID = hmdClient->getHeadsetData().id;
        }

        Dataset* dataset = getDataset(anchorAnnot.datasetID, anchorAnnot.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(anchorAnnot.datasetID, anchorAnnot.subDatasetID)
            return;
        }
        SubDataset* sd = dataset->getSubDataset(anchorAnnot.subDatasetID);

        //Add the annotation first
        {
            //Search for the meta data
            SubDatasetMetaData* sdMT = NULL;
            getMetaData(anchorAnnot.datasetID, anchorAnnot.subDatasetID, &sdMT);
            if(sdMT == NULL)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(anchorAnnot.datasetID, anchorAnnot.subDatasetID)
                return;
            }

            //Check about the privacy
            if(!canModifySubDataset(client, sdMT))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, anchorAnnot.datasetID, anchorAnnot.subDatasetID)
                return;
            }

            annotID = sd->getAnnotations().size();
            sd->emplaceAnnotation(640, 640, anchorAnnot.localPos);
        }

        anchorAnnot.headsetID    = headsetID;
        anchorAnnot.annotationID = annotID;

        for(auto& clt : m_clientTable)
            sendAnchorAnnotation(clt.second, anchorAnnot);
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
        SubDataset* sd = dataset->getSubDataset(clearAnnots.subDatasetID);

        //Search for the meta data
        SubDatasetMetaData* sdMT = NULL;
        getMetaData(clearAnnots.datasetID, clearAnnots.subDatasetID, &sdMT);
        if(sdMT == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(clearAnnots.datasetID, clearAnnots.subDatasetID)
            return;
        }


        //Check about the privacy
        if(!canModifySubDataset(client, sdMT))
        {
            VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, clearAnnots.datasetID, clearAnnots.subDatasetID)
            return;
        }

        //Delete the annotations in the particular dataset
        while(sd->getAnnotations().size())
            sd->removeAnnotation(sd->getAnnotations().begin());

        for(auto& clt : m_clientTable)
            sendClearAnnotations(clt.second, clearAnnots);
    }

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
            m_log << std::flush;
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
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendAddSubDataset(VFVClientSocket* client, const SubDataset* sd)
    {
        for(auto it : m_datasets)
        {
            if(it.second == sd->getParent())
            {
                uint32_t id = it.first;
                uint32_t ownerID = -1;
                
                SubDatasetMetaData* sdMetaData = NULL;
                getMetaData(id, sd->getID(), &sdMetaData);
                if(sdMetaData && sdMetaData->owner != NULL)
                    ownerID = sdMetaData->owner->getHeadsetData().id;

                uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t)*2 + sizeof(uint32_t) + sizeof(uint32_t) +
                                    sd->getName().size()*sizeof(uint8_t);

                uint8_t* data = (uint8_t*)malloc(dataSize);
                uint32_t offset = 0;

                writeUint16(data, VFV_SEND_ADD_SUBDATASET);
                offset+=sizeof(uint16_t);

                //Dataset ID
                writeUint32(data+offset, id);
                offset+=sizeof(uint32_t);

                //SubDataset ID
                writeUint32(data+offset, sd->getID());
                offset+=sizeof(uint32_t);

                //SubDataset Name
                writeUint32(data+offset, sd->getName().size());
                offset+=sizeof(uint32_t);
                memcpy(data+offset, sd->getName().c_str(), sd->getName().size());
                offset+=sd->getName().size();

                //Owner ID
                writeUint32(data+offset, ownerID);
                offset+=sizeof(uint32_t);

                INFO << "Sending ADDSUBDATASET Event data. Name : " << sd->getName() << " Owner : " << ownerID << "\n";
                std::shared_ptr<uint8_t> sharedData(data, free);
                SocketMessage<int> sm(client->socket, sharedData, offset);
                writeMessage(sm);

#ifdef VFV_LOG_DATA
                {
                    std::lock_guard<std::mutex> logLock(m_logMutex);
                    VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "AddSubDataset");
                    m_log << ",    \"datasetID\" : " << id << ",\n"
                          << "    \"subDatasetID\" : " << sd->getID() << ",\n"
                          << "    \"name\" : " << sd->getName() << ",\n"
                          << "    \"owner\" : " << ownerID << "\n" 
                          << "},\n";
                    m_log << std::flush;
                }
#endif
                break;
            }
        }
    }

    void VFVServer::sendRemoveSubDatasetEvent(VFVClientSocket* client, const VFVRemoveSubDataset& dataset)
    {
        uint32_t dataSize = sizeof(uint16_t) + 2*sizeof(uint32_t);
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_DEL_SUBDATASET); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, dataset.datasetID); //The datasetID
        offset += sizeof(uint32_t);

        writeUint32(data+offset, dataset.subDatasetID); //SubDataset ID
        offset += sizeof(uint32_t); 

        INFO << "Sending Remove DATASET Event data. Data : " << dataset.datasetID << " sdID : " << dataset.subDatasetID << "\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << dataset.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
            m_log << std::flush;
        }
#endif
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

        INFO << "Sending ROTATE DATASET Event data. Data : " << rotate.datasetID << " sdID : " << rotate.subDatasetID
             << " Q = " << rotate.quaternion[0] << " " << rotate.quaternion[1] << " " << rotate.quaternion[2] << " " << rotate.quaternion[3] << "\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << rotate.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendScaleDatasetEvent(VFVClientSocket* client, const VFVScaleInformation& scale)
    {
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + 3*sizeof(float);
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
            m_log << std::flush;
        }
#endif
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

        INFO << "Sending MOVE DATASET Event data Dataset ID " << position.datasetID << " sdID : " << position.subDatasetID << " position : [" << position.position[0] << ", " << position.position[1] << ", " << position.position[2] << "]\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << position.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendCurrentAction(VFVClientSocket* client, uint32_t currentActionID)
    {
        uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_CURRENT_ACTION); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, currentActionID);
        offset += sizeof(uint32_t);

        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(NULL), getTimeOffset(), "CurrentAction");
            m_log << ",    \"actionID\" : " << currentActionID << "\n";
            VFV_END_TO_JSON(m_log);
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendTransferFunctionDataset(VFVClientSocket* client, const VFVTransferFunctionSubDataset& tfSD)
    {
        //Determine the size of the packet
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + sizeof(uint8_t);
        switch(tfSD.tfID)
        {
            case TF_GTF:
            case TF_TRIANGULAR_GTF:
                dataSize += sizeof(uint32_t) + sizeof(uint8_t) + (sizeof(uint32_t) + 2*sizeof(float))*tfSD.gtfData.propData.size();
                break;
            default:
                break;
        }

        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_TF_DATASET); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, tfSD.datasetID); //The datasetID
        offset += sizeof(uint32_t);

        writeUint32(data+offset, tfSD.subDatasetID); //SubDataset ID
        offset += sizeof(uint32_t); 

        writeUint32(data+offset, tfSD.headsetID); //The headset ID
        offset += sizeof(uint32_t); 

        data[offset] = tfSD.tfID; //The transfer function type
        offset++;

        data[offset] = tfSD.colorMode; //The color mode
        offset++;

        //Write tf specific data (depends on type)
        switch(tfSD.tfID)
        {
            case TF_GTF:
            case TF_TRIANGULAR_GTF:
            {
                writeUint32(data+offset, tfSD.gtfData.propData.size()); //The number of properties
                offset += sizeof(uint32_t);

                for(auto& propData : tfSD.gtfData.propData)
                {
                    writeUint32(data+offset, propData.propID); //The property ID
                    offset += sizeof(uint32_t);

                    writeFloat(data+offset, propData.center); //The center
                    offset += sizeof(float);

                    writeFloat(data+offset, propData.scale); //The scale
                    offset += sizeof(float);
                }
                break;
            }
            default:
                break;
        }

        INFO << "Sending TF_DATASET Event data Dataset ID " << tfSD.datasetID << " sdID : " << tfSD.subDatasetID << " tfID : " << (int)tfSD.tfID << std::endl;
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << tfSD.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::onLoginSendCurrentStatus(VFVClientSocket* client)
    {
        //Send binding information
        sendHeadsetBindingInfo(client);

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
            for(uint32_t i = 0; i < it.second->getNbSubDatasets(); i++)
            {
                SubDataset* sd = it.second->getSubDatasets()[i];

                sendAddSubDataset(client, sd);
                for(uint32_t j = 0; j < sd->getAnnotations().size(); j++)
                {
                    VFVAnchorAnnotation anchorAnnot;
                    anchorAnnot.datasetID    = it.first;
                    anchorAnnot.subDatasetID = sd->getID();
                    anchorAnnot.annotationID = j;
                    auto annotIT = sd->getAnnotations().begin();
                    std::advance(annotIT, j);
                    for(uint32_t k = 0; k < 3; k++)
                        anchorAnnot.localPos[k] = (*annotIT)->getPosition()[k];
                    
                    sendAnchorAnnotation(client, anchorAnnot);
                }
            }

            sendDatasetStatus(client, it.second, it.first);
        }

        //Send anchoring data
        if(client->isHeadset())
            sendAnchoring(client);
    }

    void VFVServer::sendSubDatasetStatus(VFVClientSocket* client, SubDataset* sd, uint32_t datasetID)
    {
        //Send rotate
        VFVRotationInformation rotate;
        rotate.datasetID    = datasetID;
        rotate.subDatasetID = sd->getID();
        rotate.quaternion[0] = sd->getGlobalRotate().w;
        rotate.quaternion[1] = sd->getGlobalRotate().x;
        rotate.quaternion[2] = sd->getGlobalRotate().y;
        rotate.quaternion[3] = sd->getGlobalRotate().z;
        sendRotateDatasetEvent(client, rotate);

        //Send move
        VFVMoveInformation position;
        position.datasetID    = datasetID;
        position.subDatasetID = sd->getID();
        for(uint32_t j = 0; j < 3; j++)
            position.position[j] = sd->getPosition()[j];
        sendMoveDatasetEvent(client, position);

        //Send scale
        VFVScaleInformation scale;
        scale.datasetID    = datasetID;
        scale.subDatasetID = sd->getID();
        for(uint32_t j = 0; j < 3; j++)
            scale.scale[j] = sd->getScale()[j];
        sendScaleDatasetEvent(client, scale);

        //Send transfer Function
        SubDatasetMetaData* sdMT = NULL;
        if(getMetaData(datasetID, sd->getID(), &sdMT) == NULL || sdMT == NULL)
        {
            ERROR << "Could not fetch SubDatasetMetaData of " << datasetID << ':' << sd->getID() << std::endl;
            return;
        }
        VFVTransferFunctionSubDataset tf;
        tf.datasetID    = datasetID;
        tf.subDatasetID = sd->getID();
        tf.tfID         = sdMT->tfType;
        if(sdMT->tf != NULL)
        {
            tf.colorMode    = sdMT->tf->getColorMode();
            switch(tf.tfID)
            {
                case TF_TRIANGULAR_GTF:
                {
                    TriangularGTF* gtf = reinterpret_cast<TriangularGTF*>(sdMT->tf.get());
                    for(uint32_t j = 0; j < gtf->getDimension()-1; j++)
                    {
                        VFVTransferFunctionSubDataset::GTFPropData propData;
                        propData.propID = sd->getParent()->getPointFieldDescs()[j].id;
                        propData.center = gtf->getCenter()[j];
                        propData.scale  = gtf->getScale()[j];

                        tf.gtfData.propData.push_back(propData);
                    }
                    break;
                }

                case TF_GTF:
                {
                    GTF* gtf = reinterpret_cast<GTF*>(sdMT->tf.get());
                    for(uint32_t j = 0; j < gtf->getDimension(); j++)
                    {
                        VFVTransferFunctionSubDataset::GTFPropData propData;
                        propData.propID = sd->getParent()->getPointFieldDescs()[j].id;
                        propData.center = gtf->getCenter()[j];
                        propData.scale  = gtf->getScale()[j];

                        tf.gtfData.propData.push_back(propData);
                    }
                    break;
                }
            }
        }
        sendTransferFunctionDataset(client, tf);
    }

    void VFVServer::sendDatasetStatus(VFVClientSocket* client, Dataset* dataset, uint32_t datasetID)
    {
        for(uint32_t i = 0; i < dataset->getNbSubDatasets(); i++)
            sendSubDatasetStatus(client, dataset->getSubDatasets()[i], datasetID);
    }

    void VFVServer::sendAnnotationData(VFVClientSocket* client, Annotation* annot)
    {

    }

    void VFVServer::sendHeadsetBindingInfo(VFVClientSocket* client)
    {
        VFVClientSocket* headset = NULL;
        VFVClientSocket* tablet  = NULL;

        if(client->isHeadset())
        {
            headset = client;
            tablet  = client->getHeadsetData().tablet;
        }
        else if(client->isTablet())
        {
            headset = client->getTabletData().headset;
            tablet  = client;
        }

        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + 2*sizeof(uint8_t) + 4*sizeof(uint32_t));
        uint32_t offset = 0;

        //Type
        writeUint16(data+offset, VFV_SEND_HEADSET_BINDING_INFO);
        offset+=sizeof(uint16_t);

        uint32_t id    = -1;
        uint32_t color = 0x000000;
        bool     tabletConnected = false;
        uint8_t  firstConnected = 0;
        uint32_t handedness = HANDEDNESS_RIGHT;
        uint32_t tabletID   = -1;

        if(headset)
        {
            id    = headset->getHeadsetData().id;
            color = headset->getHeadsetData().color;
        }

        if(tablet)
        {
            tabletConnected = true;
            tabletID        = tablet->getTabletData().number;
            handedness      = tablet->getTabletData().handedness;
        }

        //Headset ID
        writeUint32(data+offset, id);
        offset += sizeof(uint32_t);

        //Headset Color
        writeUint32(data+offset, color);
        offset+=sizeof(uint32_t);

        //Tablet connected
        data[offset++] = tabletConnected;

        //Handedness
        writeUint32(data+offset, handedness);
        offset+=sizeof(uint32_t);

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
                  << "    \"handedness\" : " << handedness << ",\n"
                  << "    \"tabletID\" : " << tabletID << ",\n"
                  << "    \"firstConnected\" : " << (bool)firstConnected << "\n"
                  << "},\n";
            m_log << std::flush;
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
                m_log << std::flush;
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

    void VFVServer::sendSubDatasetLockOwner(SubDatasetMetaData* metaData)
    {
        //Generate the data
        uint8_t* data   = (uint8_t*)malloc(sizeof(uint16_t) + 3*sizeof(uint32_t));
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_SUBDATASET_LOCK_OWNER);
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

        INFO << "Setting lock owner dataset ID " <<  metaData->datasetID << " sub dataset ID " << metaData->sdID << " headset ID" << id << std::endl;

        //Send the data
        for(auto it : m_clientTable)
        {
            SocketMessage<int> sm(it.second->socket, sharedData, offset);
            writeMessage(sm);

#ifdef VFV_LOG_DATA
            {
                std::lock_guard<std::mutex> logLock(m_logMutex);
                VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(it.second), getTimeOffset(), "SubDatasetLockOwner");
                m_log << ",    \"datasetID\"  : " << metaData->datasetID << ",\n"
                      << "    \"subDatasetID\" : " << metaData->sdID << ",\n"
                      << "    \"headsetID\" : " << id << "\n"
                      << "},\n";
                m_log << std::flush;
            }
#endif
        }
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
        if(metaData->owner != NULL)
            id = metaData->owner->getHeadsetData().id;
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
                m_log << std::flush;
            }
#endif
        }
    }

    void VFVServer::sendStartAnnotation(VFVClientSocket* client, const VFVStartAnnotation& startAnnot)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + 3*sizeof(uint32_t));
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_START_ANNOTATION);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, startAnnot.datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, startAnnot.subDatasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, startAnnot.pointingID);
        offset += sizeof(uint32_t);

        std::shared_ptr<uint8_t> sharedData(data, free);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> lockJson(m_logMutex);
            m_log << startAnnot.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
            m_log << std::flush;
        }
#endif

        INFO << "Sending start annotation \n";
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
    }

    void VFVServer::sendAnchorAnnotation(VFVClientSocket* client, const VFVAnchorAnnotation& anchorAnnot)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + 4*sizeof(uint32_t) + 3*sizeof(float));
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
            m_log << std::flush;
        }
#endif

        INFO << "Sending anchor annotation " << anchorAnnot.localPos[0] << "x" << anchorAnnot.localPos[1] << "x" << anchorAnnot.localPos[2] << "\n";
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
    }

    void VFVServer::sendClearAnnotations(VFVClientSocket* client, const VFVClearAnnotations& clearAnnot)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + 2*sizeof(uint32_t));
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

        std::shared_ptr<uint8_t> sharedData(data, free);

        INFO << "Sending clear annotation \n";
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> lockJson(m_logMutex);
            m_log << clearAnnot.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset()) << ",\n";
            m_log << std::flush;
        }
#endif
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

            if(msg.curMsg)
            {
#ifdef VFV_LOG_DATA
                {
                    bool isTablet  = client->isTablet();
                    bool isHeadset = client->isHeadset();

                    {
                        std::lock_guard<std::mutex> logLock(m_logMutex);
                        std::string str = msg.curMsg->toJson(isTablet ? VFV_SENDER_TABLET : (isHeadset ? VFV_SENDER_HEADSET : VFV_SENDER_UNKNOWN), getHeadsetIPAddr(client), getTimeOffset());

                        if(str.size())
                        {
                            m_log << str << ",\n";
                            m_log << std::flush;
                        }
                    }
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
                    std::lock_guard<std::mutex> lock(m_mapMutex);
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
                        std::lock_guard<std::mutex> lock(m_mapMutex);
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

                case TF_DATASET:
                {
                    tfSubDataset(client, msg.tfSD);
                    break;
                }

                case HEADSET_CURRENT_ACTION:
                {
                    //Look for the headset to modify
                    VFVClientSocket* headset = getHeadsetFromClient(client);
                    if(!headset)
                        break;

                    //Set the current action
                    if(headset != client)
                    {
                        std::lock_guard<std::mutex> lock(m_mapMutex);
                        headset->getHeadsetData().currentAction = (VFVHeadsetCurrentActionType)msg.headsetCurrentAction.action;
                        sendCurrentAction(headset, headset->getHeadsetData().currentAction);
                        INFO << "Current action : " << msg.headsetCurrentAction.action << std::endl;
                    }
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

                case ADD_SUBDATASET:
                {
                    onAddSubDataset(client, msg.addSubDataset);
                    break;
                }

                case REMOVE_SUBDATASET:
                {
                    onRemoveSubDataset(client, msg.removeSubDataset);
                    break;
                }

                case MAKE_SUBDATASET_PUBLIC:
                {
                    onMakeSubDatasetPublic(client, msg.makeSubDatasetPublic);
                    break;
                }

                case DUPLICATE_SUBDATASET:
                {
                    onDuplicateSubDataset(client, msg.duplicateSubDataset);
                    break;
                }

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

            if(m_anchorData.isCompleted())
            {
                std::lock_guard<std::mutex> lock2(m_datasetMutex);
                std::lock_guard<std::mutex> lock(m_mapMutex);

                //Send HEADSETS_STATUS
                for(auto it : m_clientTable)
                {
                    if((!it.second->isTablet() && !it.second->isHeadset()) || 
                        it.second->getBytesInWritting() > (1 << 16))
                        continue;

                    uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t) + sizeof(uint32_t) + 
                                                     MAX_NB_HEADSETS*(7*sizeof(float) + 3*sizeof(uint32_t) + 3*sizeof(uint32_t) + 1 + 10*sizeof(float)));
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
                    m_log << std::flush;
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

                            //Pointing headset starting orientation
                            for(uint32_t i = 0; i < 4; i++, offset+=sizeof(float))
                                writeFloat(data+offset, headsetData.pointingData.headsetStartOrientation[i]);

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
                                  << "    \"pointingHeadsetStartPosition\" : [" << headsetData.pointingData.headsetStartPosition[0] << "," << headsetData.pointingData.headsetStartPosition[1] << "," << headsetData.pointingData.headsetStartPosition[2] << "],\n"
                                  << "    \"pointingHeadsetStartOrientation\" : [" << headsetData.pointingData.headsetStartOrientation[0] << "," << headsetData.pointingData.headsetStartOrientation[1] << "," << headsetData.pointingData.headsetStartOrientation[2] << "," << headsetData.pointingData.headsetStartOrientation[3] << "]\n"
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
                    m_log << std::flush;
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
                std::lock_guard<std::mutex> lock2(m_datasetMutex);
                std::lock_guard<std::mutex> lock(m_mapMutex);
                for(auto& it : m_vtkDatasets)
                {
                    for(auto& it2 : it.second.sdMetaData)
                    {
                        if(it2.hmdClient != NULL && endTime - it2.lastModification >= MAX_OWNER_TIME)
                        {
                            it2.hmdClient = NULL;
                            it2.lastModification = 0;
                            sendSubDatasetLockOwner(&it2);
                        }
                    }
                }
            }

            //Sleep
            clock_gettime(CLOCK_REALTIME, &end);
            endTime = end.tv_nsec*1.e-3 + end.tv_sec*1.e6;
            usleep(std::max(0.0, 1.e6/UPDATE_THREAD_FRAMERATE - endTime + (beg.tv_nsec*1.e-3 + beg.tv_sec*1.e6)));
        }
    }
}
