#include "VFVServer.h"
#include "TransferFunction/GTF.h"
#include "TransferFunction/TriangularGTF.h"
#include "TransferFunction/MergeTF.h"
#include <random>
#include <algorithm>
#include <iostream>
#include <filesystem>

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

#define VFVSERVER_ANNOTATION_COMPONENT_NOT_FOUND(_annotLogID, _annotComponentID)\
    {\
        WARNING << "Annotation Component ID " << (_annotComponentID) << " in annotation ID " << (_annotLogID) << " not found... disconnecting the client!"; \
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

    /** \brief  Generate a transferfunction message based on the subdataset the transfer function is attached to, and the transfer function (or one part of it) attached to it
     * \param datasetID the datasetID of the subdataset
     * \param sd the subdataset to evaluate
     * \param tfMT the transfer function meta data to evaluate
     *
     * \return   a new TransferFunction message */
    static VFVTransferFunctionSubDataset generateTFMessage(uint32_t datasetID, const SubDataset* sd, std::shared_ptr<SubDatasetTFMetaData> tfMT)
    {
        VFVTransferFunctionSubDataset tf;
        tf.datasetID    = datasetID;
        tf.subDatasetID = sd->getID();
        tf.changeTFType(tfMT->getType());
        tf.timestep     = tfMT->getTF()->getCurrentTimestep();
        tf.minClipping  = tfMT->getTF()->getMinClipping();
        tf.maxClipping  = tfMT->getTF()->getMaxClipping();

        if(tfMT != NULL)
        {
            tf.colorMode    = tfMT->getTF()->getColorMode();
            switch(tf.tfID)
            {
                case TF_TRIANGULAR_GTF:
                {
                    TriangularGTF* gtf = reinterpret_cast<TriangularGTF*>(tfMT->getTF().get());
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
                    GTF* gtf = reinterpret_cast<GTF*>(tfMT->getTF().get());
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

                case TF_MERGE:
                {
                    MergeTF* merge = reinterpret_cast<MergeTF*>(tfMT->getTF().get());
                    tf.mergeTFData.t   = merge->getInterpolationParameter();
                    tf.mergeTFData.tf1 = std::make_shared<VFVTransferFunctionSubDataset>(generateTFMessage(datasetID, sd, tfMT->getMergeTFMetaData().tf1));
                    tf.mergeTFData.tf2 = std::make_shared<VFVTransferFunctionSubDataset>(generateTFMessage(datasetID, sd, tfMT->getMergeTFMetaData().tf2));
                    break;
                }
            }
        }

        return tf;
    }

    /** \brief  Fill a GTF/TriangularGTF object based on a TransferFunction Message
     *
     * @tparam T GTF or TriangularGTF
     * \param tf the TransferFunction to fill
     * \param sd the SubDataset linked to this TransferFunction
     * \param tfSD the transfer function message to parse*/
    template <typename T>
    static void fillGTFWithMessage(T* tf, SubDataset* sd, const VFVTransferFunctionSubDataset& tfSD)
    {
        //Get the ordered array of centers and scaling
        float* centers = (float*)malloc(sizeof(float)*tf->getDimension());
        float* scales  = (float*)malloc(sizeof(float)*tf->getDimension());

        for(uint32_t i = 0; i < tfSD.gtfData.propData.size(); i++)
        {
            //Look for corresponding ID...
            uint32_t tfID = sd->getParent()->getTFIndiceFromPointFieldID(tfSD.gtfData.propData[i].propID);
            if(tfID != (uint32_t)-1 && tfID < tf->getDimension())
            {
                centers[tfID] = tfSD.gtfData.propData[i].center;
                scales[tfID]  = tfSD.gtfData.propData[i].scale;
            }
        }

        //Set the center and scaling factors
        tf->setCenter(centers);
        tf->setScale(scales);

        //Free data
        free(centers);
        free(scales);
    }

    /** \brief  Parse a network message to a transfer function object
     * \param sd the subdataset linked to the message
     * \param tfSD the network message to parse
     * \return   a new TransferFunction object */
    static SubDatasetTFMetaData* messageToTF(SubDataset* sd, const VFVTransferFunctionSubDataset& tfSD)
    {
        SubDatasetTFMetaData* tfMD = new SubDatasetTFMetaData((TFType)tfSD.tfID);

        switch((TFType)tfSD.tfID)
        {
            case TF_GTF:
            {
                GTF* tf = new GTF(tfSD.gtfData.propData.size(), (ColorMode)tfSD.colorMode);
                tfMD->setTF(std::shared_ptr<GTF>(tf));
                fillGTFWithMessage(tf, sd, tfSD);
                break;
            }
            case TF_TRIANGULAR_GTF:
            {
                TriangularGTF* tf = new TriangularGTF(tfSD.gtfData.propData.size()+1, (ColorMode)tfSD.colorMode);
                tfMD->setTF(std::shared_ptr<TriangularGTF>(tf));
                fillGTFWithMessage(tf, sd, tfSD);
                break;
            }
            case TF_MERGE:
            {
                std::shared_ptr<SubDatasetTFMetaData> tf1 = std::shared_ptr<SubDatasetTFMetaData>(messageToTF(sd, *tfSD.mergeTFData.tf1.get()));
                std::shared_ptr<SubDatasetTFMetaData> tf2 = std::shared_ptr<SubDatasetTFMetaData>(messageToTF(sd, *tfSD.mergeTFData.tf2.get()));
                MergeTF* merge = new MergeTF(tf1->getTF(), tf2->getTF(), tfSD.mergeTFData.t);
                tfMD->getMergeTFMetaData().tf1 = tf1;
                tfMD->getMergeTFMetaData().tf2 = tf2;
                tfMD->setTF(std::shared_ptr<TF>(merge));
                break;
            }
            default:
                ERROR << "The Transfer Function type: " << (int)tfSD.tfID << " is unknown. Set to TF_NONE\n";
                break;
        }
        tfMD->getTF()->setCurrentTimestep(tfSD.timestep);
        tfMD->getTF()->setClipping(tfSD.minClipping, tfSD.maxClipping);
        //sd->setTransferFunction(sdMT->tf->getTF());

        return tfMD;
    }

    /* \brief  Clone a TransferData to another
     * \param tf the transfer data meta data to clone
     * \return   a new transferdata meta data with a new transfer data*/
    SubDatasetTFMetaData* cloneTransferFunction(std::shared_ptr<SubDatasetTFMetaData> tf)
    {
        TF* _tf = NULL;
        switch(tf->getType())
        {
            case TF_TRIANGULAR_GTF:
                _tf = new TriangularGTF(*(const TriangularGTF*)tf->getTF().get());
                break;
            case TF_GTF:
                _tf = new GTF(*(const GTF*)tf->getTF().get());
                break;
            case TF_MERGE:
            {
                MergeTF* merge = reinterpret_cast<MergeTF*>(tf->getTF().get());
                std::shared_ptr<SubDatasetTFMetaData> tf1 = std::shared_ptr<SubDatasetTFMetaData>(cloneTransferFunction(tf->getMergeTFMetaData().tf1));
                std::shared_ptr<SubDatasetTFMetaData> tf2 = std::shared_ptr<SubDatasetTFMetaData>(cloneTransferFunction(tf->getMergeTFMetaData().tf2));
                _tf = new MergeTF(tf1->getTF(), tf2->getTF(), merge->getInterpolationParameter());

                SubDatasetTFMetaData* tfMD = new SubDatasetTFMetaData(tf->getType(), std::shared_ptr<TF>(_tf));
                tfMD->getMergeTFMetaData().tf1 = tf1;
                tfMD->getMergeTFMetaData().tf2 = tf2;

                return tfMD;
            }
            default:
                WARNING << "Did not find TFType " << tf->getType() << std::endl;
                break;
        }

        SubDatasetTFMetaData* tfMD = new SubDatasetTFMetaData(tf->getType(), std::shared_ptr<TF>(_tf));

        return tfMD;
    } 

    static std::shared_ptr<uint8_t> generateVolumetricMaskEvent(const SubDataset* sd, uint32_t datasetID, size_t* dataSize=NULL)
    {
        size_t volDataSize = 2 + 2*4 + 4 + sd->getVolumetricMaskSize() + 1;
        uint8_t* volData   = (uint8_t*)malloc(volDataSize);
        size_t offset = 0;

        writeUint16(volData + offset, VFV_SEND_VOLUMETRIC_MASK);
        offset += sizeof(uint16_t);

        //DatasetID
        writeUint32(volData+offset, datasetID);
        offset += sizeof(uint32_t);

        //SubDatasetID
        writeUint32(volData+offset, sd->getID());
        offset += sizeof(uint32_t);

        //Mask byte array
        writeUint32(volData+offset, sd->getVolumetricMaskSize());
        offset += sizeof(uint32_t);
        for(size_t i = 0; i < sd->getVolumetricMaskSize(); i++, offset++)
            volData[offset] = sd->getVolumetricMask()[i];

        volData[offset] = sd->isVolumetricMaskEnabled(); 
        offset++;

        std::shared_ptr<uint8_t> sharedVolData(volData, free);

        if(dataSize)
            *dataSize = offset;
        return sharedVolData;
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
        vtkInfo.name = "history2019-12-10.vtk";
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


//        VFVCloudPointDatasetInformation cloudInfo;
//        cloudInfo.name="1.cp";
//        addCloudPointDataset(NULL, cloudInfo);

        //Simulate a volumetric selection
//        VFVVolumetricData volData;
//        for(uint32_t i = 0; i < 32; i++)
//            volData.lasso.push_back(glm::vec2(cos(3.14*i/32.0), sin(3.14*i/32.0)));
//        volData.pushMesh(SELECTION_OP_UNION);
//        volData.lassoScale = glm::vec3(1.0f, 1.0f, 1.0f);
//        volData.pushLocation({glm::vec3(0.0f, 0.0f, 0.0f), Quaternionf()});
//        volData.pushLocation({glm::vec3(0.0f, 1.0f, 1.0f), Quaternionf()});
//        volData.closeCurrentMesh();
//        applyVolumetricSelection_cloudPoint(volData.meshes.back(), m_datasets[0]->getSubDataset(0));
//        m_datasets[0]->getSubDataset(0)->enableVolumetricMask(true);
#endif
    }

    VFVServer::VFVServer(VFVServer&& mvt) : Server(std::move(mvt))
    {
        m_updateThread     = mvt.m_updateThread;
        m_computeThread    = mvt.m_computeThread;
        mvt.m_updateThread = mvt.m_computeThread = NULL;
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

        for(auto& d : m_cloudPointDatasets)
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
        m_updateThread  = new std::thread(&VFVServer::updateThread, this);
        m_computeThread = new std::thread(&VFVServer::computeThread, this);

        return ret;
    }

    void VFVServer::cancel()
    {
        Server::cancel();
        m_computeCond.notify_all();
        if(m_updateThread && m_updateThread->joinable())
            pthread_cancel(m_updateThread->native_handle());
//        if(m_computeThread && m_computeThread->joinable())
//            pthread_cancel(m_computeThread->native_handle());
    }

    void VFVServer::wait()
    {
        Server::wait();
        if(m_updateThread && m_updateThread->joinable())
            m_updateThread->join();
        if(m_computeThread && m_computeThread->joinable())
            m_computeThread->join();
    }

    void VFVServer::closeServer()
    {
        Server::closeServer();
        if(m_updateThread != NULL)
        {
            delete m_updateThread;
            m_updateThread = 0;
        }
        if(m_computeThread != NULL)
        {
            delete m_computeThread;
            m_computeThread = 0;
        }
    }

    void VFVServer::updateLocationTabletDebug(const glm::vec3& pos, const Quaternionf& rot)
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        for(auto it: m_clientTable)
            if(it.second->isTablet())
                sendLocationTablet(pos, rot, it.second);
    }

    void VFVServer::pushTabletVRPNPosition(const glm::vec3& pos, const Quaternionf& rot, int tabletID)
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        for(auto it : m_clientTable)
        {
            VFVClientSocket* clt = it.second;
            if(clt->isTablet() && clt->getTabletData().number == tabletID)
            {
                clt->setVRPNPosition(pos);
                clt->setVRPNRotation(rot);
                break;
            }
        }
    }

    void VFVServer::pushHeadsetVRPNPosition(const glm::vec3& pos, const Quaternionf& rot, int tabletID)
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        for(auto it : m_clientTable)
        {
            VFVClientSocket* clt = it.second;
            if(clt->isHeadset() && clt->getHeadsetData().tablet != NULL && 
               clt->getHeadsetData().tablet->getTabletData().number == tabletID)
            {
                clt->setVRPNPosition(pos);
                clt->setVRPNRotation(rot);
                break;
            }
        }
    }

    void VFVServer::commitAllVRPNPositions()
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);

        //Search for every tablets
        for(auto it : m_clientTable)
        {
            VFVClientSocket* clt = it.second;
            if(clt->isTablet() && clt->getTabletData().headset != NULL)
            {
                auto changeRot = [](const Quaternionf& r)
                {
                    //Change axis orientation due to the VRPN orientation
                    Quaternionf rot = r;
                    float tmp = rot.y;
                    rot.y = rot.z;
                    rot.z = -tmp;
                    rot.x *= -1;

                    //Get the inverse because we translate from left handed to right handed, so the rotation "angle" should also be countered
                    //See https://stackoverflow.com/questions/18818102/convert-quaternion-representing-rotation-from-one-coordinate-system-to-another
                    return rot.getInverse();
                };

                //Compute the tablet rotation compare to its bound headset's rotation
                Quaternionf rotHVicon = clt->getTabletData().headset->getVRPNRotation();
                Quaternionf rotH      = clt->getTabletData().headset->getHeadsetData().rotation; 
                Quaternionf tabletRot = rotH * changeRot(rotHVicon.getInverse()*clt->getVRPNRotation());

                //Compute the tablet position
                glm::vec3 posHtoT   = clt->getVRPNPosition() - clt->getTabletData().headset->getVRPNPosition();
                posHtoT = clt->getTabletData().headset->getVRPNRotation().getInverse() * posHtoT;
                //Change axis orientation
                float tmp = posHtoT.y;
                posHtoT.y = posHtoT.z;
                posHtoT.z = -tmp;
                posHtoT.x *= -1;

                posHtoT = rotH*posHtoT;

                glm::vec3 tabletPos = clt->getTabletData().headset->getHeadsetData().position + posHtoT;

                //Send the location
                sendLocationTablet(tabletPos, tabletRot, clt);
            }
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

            auto f = [c,this](DatasetMetaData& mtData) 
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

    Dataset* VFVServer::getDataset(uint32_t datasetID, uint32_t sdID, SubDataset** sd)
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

        if(sd)
            *sd = it->second->getSubDataset(sdID);

        return it->second;
    }

    DatasetMetaData* VFVServer::getMetaData(uint32_t datasetID, uint32_t sdID, SubDatasetMetaData** sdMTPtr)
    {
        DatasetMetaData* mt = NULL;
        SubDatasetMetaData* sdMT = NULL;
        auto it = m_vtkDatasets.find(datasetID);
        if(it != m_vtkDatasets.end())
        {
            sdMT = it->second.getSDMetaDataByID(sdID);
            mt = &it->second;
        }
        else
        {
            auto it2 = m_cloudPointDatasets.find(datasetID);
            if(it2 != m_cloudPointDatasets.end())
            {
                sdMT = it2->second.getSDMetaDataByID(sdID);
                mt = &it2->second;
            }
        }

        if(sdMTPtr)
            *sdMTPtr = sdMT;

        return mt;
    }

    DatasetType VFVServer::getDatasetType(const Dataset* d) const
    {
        for(auto& it : m_vtkDatasets)
            if(it.second.dataset == d)
                return DATASET_TYPE_VTK;

        for(auto& it : m_binaryDatasets)
            if(it.second.dataset == d)
                return DATASET_TYPE_VECTOR_FIELD;

        for(auto& it : m_cloudPointDatasets)
            if(it.second.dataset == d)
                return DATASET_TYPE_CLOUD_POINT;

        return DATASET_TYPE_NOT_FOUND;
    }

    DatasetMetaData* VFVServer::updateMetaDataModification(VFVClientSocket* client, uint32_t datasetID, uint32_t sdID, SubDatasetMetaData** sdMT)
    {
        DatasetMetaData* mt = NULL;
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

    void VFVServer::updateSDGroup(SubDataset* sd, bool setTF)
    {
        if(sd->getSubDatasetGroup())
        {
            sd->getSubDatasetGroup()->updateSubDatasets();

            //Update the Transfer function meta data
            if(setTF)
            {
                SubDatasetMetaData* sdMT = nullptr;
                uint32_t datasetID = getDatasetID(sd->getParent());

                getMetaData(datasetID, sd->getID(), &sdMT);
                if(sdMT != nullptr)
                {
                    auto it = m_sdGroups.find(sdMT->sdgID);
                    if(it != m_sdGroups.end())
                    {
                        if(it->second.isSubjectiveView())
                        {
                            SubDatasetSubjectiveStackedLinkedGroup* svGroup = (SubDatasetSubjectiveStackedLinkedGroup*)it->second.sdGroup.get();
                            for(auto subjView : svGroup->getLinkedSubDatasets())
                            {
                                if(subjView.first != nullptr && subjView.second != nullptr)
                                {
                                    SubDatasetMetaData* stackedMT = nullptr;
                                    SubDatasetMetaData* linkedMT  = nullptr;
                                    getMetaData(datasetID, subjView.first->getID(), &stackedMT);
                                    getMetaData(datasetID, subjView.first->getID(), &linkedMT);

                                    if(stackedMT && linkedMT && linkedMT->tf != nullptr)
                                    {
                                        stackedMT->tf = std::shared_ptr<SubDatasetTFMetaData>(new SubDatasetTFMetaData(*linkedMT->tf.get()));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    bool VFVServer::canClientModifySubDatasetGroup(VFVClientSocket* client, const SubDatasetGroupMetaData& sdg)
    {
        VFVClientSocket* hmdClient = nullptr;
        if(client != NULL) //Not the server
        {
            hmdClient = getHeadsetFromClient(client);
            if(hmdClient == NULL)
            {
                WARNING << "Not connected to a headset yet..." << std::endl;
                return false;
            }
        }

        if(client != NULL && sdg.owner != NULL && sdg.owner != hmdClient)
        {
            ERROR << "The client cannot modify this subdataset group ID " << sdg.sdgID << std::endl;
            return false;
        }

        return true;
    }

    uint32_t VFVServer::getDatasetID(Dataset* dataset)
    {
        for(auto& it : m_datasets)
            if(it.second == dataset)
            {
                return it.first;
            }

        return -1;
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
                    sendHeadsetBindingInfo(client);

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

        //Search for other VTK subfiles part of this serie (time serie data)
        std::vector<std::string> suffixes;
        for(const auto& entry : std::filesystem::directory_iterator(DATASET_DIRECTORY))
        {
            std::string timePath = entry.path().filename().string();
            if(timePath.rfind(dataset.name, 0) == 0 && timePath.size() > dataset.name.size()+1)
            {
                std::string suffix = timePath.substr(dataset.name.size()+1);
                for(char c : suffix)
                {
                    if(c < '0' || c > '9')
                        goto endFor;
                }
                suffixes.push_back(suffix);
endFor:;
            }
        }

        //Sort the suffixes
        if(suffixes.size())
        {
            std::sort(suffixes.begin(), suffixes.end());
            for(const auto& s : suffixes)
            {
                VTKParser* suffixParser = new VTKParser(DATASET_DIRECTORY+dataset.name+"."+s);
                if(!suffixParser->parse())
                {
                    ERROR << "Could not parse the VTK Dataset " << dataset.name + "." + s << std::endl;
                    delete suffixParser;
                    continue;
                }

                std::shared_ptr<VTKParser> sharedSuffixParser(suffixParser);
                vtk->addTimestep(sharedSuffixParser);
            }
        }

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

        for(uint32_t i = 0; i < vtk->getNbSubDatasets(); i++)
        {
            SubDatasetMetaData md;
            md.sdID   = vtk->getSubDatasets()[i]->getID();

            SubDatasetTFMetaData* tfMD = new SubDatasetTFMetaData(TF_TRIANGULAR_GTF, std::make_shared<TriangularGTF>(dataset.ptFields.size()+1, RAINBOW));
            md.tf     = std::shared_ptr<SubDatasetTFMetaData>(tfMD);
            vtk->getSubDatasets()[i]->setTransferFunction(md.tf->getTF());
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

    void VFVServer::addCloudPointDataset(VFVClientSocket* client, const VFVCloudPointDatasetInformation& dataset)
    {
        if(client != NULL && !client->isTablet())
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            VFVSERVER_NOT_A_TABLET
            return;
        }

        CloudPointDataset* cloudPoint = new CloudPointDataset(DATASET_DIRECTORY+dataset.name);

        //We do need to load t at this stage
        std::thread* t = cloudPoint->loadValues(NULL, NULL);
        if(t && t->joinable()) 
            t->join();

        //Update the position
        for(uint32_t i = 0; i < cloudPoint->getNbSubDatasets(); i++, m_currentSubDataset++)
        {
            SubDataset* sd = cloudPoint->getSubDatasets()[i];
            sd->setPosition(glm::vec3(m_currentSubDataset*2.0f, 0.0f, 0.0f));
            sd->setScale(glm::vec3(0.5f, 0.5f, 0.5f));
        }

        CloudPointMetaData metaData;
        metaData.dataset = cloudPoint;
        metaData.name    = dataset.name;

        for(uint32_t i = 0; i < cloudPoint->getNbSubDatasets(); i++)
        {
            SubDatasetMetaData md;
            md.sdID   = cloudPoint->getSubDatasets()[i]->getID();
            SubDatasetTFMetaData* tfMD = new SubDatasetTFMetaData(TF_GTF, std::make_shared<GTF>(1, RAINBOW));
            md.tf     = std::shared_ptr<SubDatasetTFMetaData>(tfMD);
            cloudPoint->getSubDatasets()[i]->setTransferFunction(md.tf->getTF());
            metaData.sdMetaData.push_back(md);
        }

        //Add it to the list
        {
            std::lock_guard<std::mutex> lock(m_datasetMutex);
            metaData.datasetID = m_currentDataset;
            for(auto& it : metaData.sdMetaData)
                it.datasetID = m_currentDataset;
            m_cloudPointDatasets.insert(std::pair<uint32_t, CloudPointMetaData>(m_currentDataset, metaData));
            m_datasets.insert(std::pair<uint32_t, CloudPointDataset*>(m_currentDataset, cloudPoint));
            m_currentDataset++;
        }

        //Send it to the other clients
        {
            std::lock_guard<std::mutex> lock2(m_mapMutex);
            for(auto clt : m_clientTable)
            {
                sendAddCloudPointDatasetEvent(clt.second, dataset, metaData.datasetID);
                sendDatasetStatus(clt.second, cloudPoint, metaData.datasetID);
            }
        }

        //Add a SubDataset if no one is registered yet
        if(cloudPoint->getNbSubDatasets() == 0)
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
        DatasetMetaData* mt = getMetaData(dataset.datasetID, 0, NULL);
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

        SubDatasetTFMetaData* tfMD = new SubDatasetTFMetaData(TF_TRIANGULAR_GTF, std::make_shared<TriangularGTF>(d->getPointFieldDescs().size()+1, RAINBOW));
        md.tf     = std::shared_ptr<SubDatasetTFMetaData>(tfMD);
        sd->setTransferFunction(md.tf->getTF());
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

        SubDatasetMetaData* sdMT = nullptr;
        SubDataset* sd = dataset->getSubDataset(remove.subDatasetID);
        DatasetMetaData* mtData = getMetaData(remove.datasetID, remove.subDatasetID, &sdMT);
        if(!mtData || !sdMT)
        {
            ERROR << "Could not get the meta data of the subdatasets" << std::endl;
            return;
        }

        for(auto it = mtData->sdMetaData.begin(); it != mtData->sdMetaData.end();)
        {
            if(it->datasetID == remove.datasetID && it->sdID == remove.subDatasetID)
            {
                it = mtData->sdMetaData.erase(it);
                break;
            }
            else
                it++;
        }

        //There might be extra steps on removing a subdataset based on its group
        if(sdMT->sdgID != -1)
        {
            auto sdgIT = m_sdGroups.find(sdMT->sdgID);
            if(sdgIT == m_sdGroups.end())
            {
                ERROR << "The Subdataset is registered as having a subdatasetgroup, but the subdataset group meta data is unavailable" << std::endl;
                return;
            }

            if(sdgIT->second.isSubjectiveView())
            {
                SubDatasetSubjectiveStackedLinkedGroup* svg = (SubDatasetSubjectiveStackedLinkedGroup*)(sdgIT->second.sdGroup.get()); 

                //If base --> remove the group
                if(sd == svg->getBase())
                {
                    VFVRemoveSubDatasetGroup removeSDG;
                    removeSDG.sdgID = sdgIT->first;
                    removeSubDatasetGroup(removeSDG);
                }

                //If subjective views --> remove its counter part
                else
                {
                    auto subjViews = svg->getLinkedSubDataset(sd);
                    if(subjViews.first != nullptr && subjViews.second != nullptr)
                    {
                        sdMT->sdgID = -1;
                        svg->removeSubDataset(sd); //This removes the counter part as well

                        SubDataset* counterPart = subjViews.first;
                        if(subjViews.first == sd)
                            counterPart = subjViews.second;

                        SubDatasetMetaData* counterPartMT;
                        getMetaData(remove.datasetID, counterPart->getID(), &counterPartMT);
                        if(counterPartMT)
                            counterPartMT->sdgID = -1;

                        VFVRemoveSubDataset removeCounter = remove;
                        removeCounter.subDatasetID = counterPart->getID();
                        removeSubDataset(removeCounter);
                    }
                }
            }
        }

        dataset->removeSubDataset(sd); //This shall also set the subdataset group as required

        //Tells all the clients
        for(auto& clt : m_clientTable)
            sendRemoveSubDatasetEvent(clt.second, remove);
    }
    
    void VFVServer::onSaveSubDatasetVisual(VFVClientSocket* client, const VFVSaveSubDatasetVisual& saveSDVisual)
    {
        //TODO
    }

    void VFVServer::onSetVolumetricSelectionMethod(VFVClientSocket* client, const VFVVolumetricSelectionMethod& method)
    {
        if(client != NULL && !client->isTablet())
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            VFVSERVER_NOT_A_TABLET
            return;
        }

        client->getTabletData().volSelMethod = (VolumetricSelectionMethod)method.method;
    }

    void VFVServer::addLogData(VFVClientSocket* client, const VFVOpenLogData& logData)
    {
        if(client != NULL && !client->isTablet())
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            VFVSERVER_NOT_A_TABLET
            return;
        }

        const std::string fullPath = "Logs/"+logData.fileName;

        std::shared_ptr<AnnotationLogContainer> annot = std::make_shared<AnnotationLogContainer>(logData.hasHeader);
        INFO << "On Open Log Data file " << fullPath << std::endl;

        //Parse data based on its extension
        std::string extension = std::filesystem::path(fullPath).extension();
        if(extension == ".csv")
        {
            if(!annot->readFromCSV(fullPath))
            {
                ERROR << "Cannot parse file " << fullPath << ". Discard" << std::endl;
                return;
            }
        }
        else
        {
            ERROR << "Unknown file extension " << extension << ". Discard" << std::endl;
            return;
        }

        //Set the time column ID
        annot->setTimeInd(logData.timeID);

        //Add this annotation
        LogMetaData metaData;
        metaData.logData = annot;
        metaData.name    = logData.fileName;
        {
            std::lock_guard<std::mutex> lock(m_datasetMutex);
            metaData.logID = m_currentLogData;
            m_logData.emplace(std::make_pair(metaData.logID, metaData));
            m_currentLogData++;
        }

        //Send it to all clients
        {
            std::lock_guard<std::mutex> lock2(m_mapMutex);
            for(auto clt : m_clientTable)
                sendAddLogData(clt.second, logData, metaData.logID);
        }
    }

    void VFVServer::addAnnotationPosition(VFVClientSocket* client, const VFVAddAnnotationPosition& pos)
    {
        if(client != NULL && !client->isTablet())
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            VFVSERVER_NOT_A_TABLET
            return;
        }

        //Search for the AnnotationLog object
        std::lock_guard<std::mutex> dataLock(m_datasetMutex);
        auto it = m_logData.find(pos.annotLogID);
        if(it == m_logData.end())
        {
            VFVSERVER_ANNOTATION_NOT_FOUND(pos.annotLogID);
            return;
        }

        AnnotationComponentMetaData<AnnotationPosition>& posMT = it->second.addPosition();

        //Send it to all clients
        {
            std::lock_guard<std::mutex> lock2(m_mapMutex);
            for(auto clt : m_clientTable)
            {
                sendAddAnnotationPositionData(clt.second, posMT);
                sendSetAnnotationPositionIndexes(clt.second, posMT);
            }
        }
    }

    void VFVServer::addAnnotationPositionToSD(VFVClientSocket* client, const VFVAddAnnotationPositionToSD& pos)
    {
        //Check if the client is valid
        if(client != NULL && !client->isTablet())
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            VFVSERVER_NOT_A_TABLET
            return;
        }

        //Search for the SD
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        SubDatasetMetaData* sdMT;
        getMetaData(pos.datasetID, pos.sdID, &sdMT);
        if(sdMT == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(pos.datasetID, pos.sdID)
            return;
        }

        //Search for the annotation position component
        AnnotationComponentMetaData<AnnotationPosition>* posIT = nullptr;
        LogMetaData* annot = getLogComponentMetaData(pos.annotLogID, pos.annotComponentID, &posIT);
        if(posIT == nullptr || annot == nullptr)
        {
            VFVSERVER_ANNOTATION_COMPONENT_NOT_FOUND(pos.annotLogID, pos.annotComponentID);
            return;
        }

        //Create a new visualization
        std::shared_ptr<DrawableAnnotationPositionMetaData> drawable = std::make_shared<DrawableAnnotationPositionMetaData>();
        drawable->compMetaData = posIT;
        drawable->drawable     = std::make_shared<DrawableAnnotationPosition>(annot->logData, posIT->component);
        sdMT->pushDrawableAnnotationPosition(drawable);
        {
            std::lock_guard<std::mutex> lock2(m_mapMutex);
            for(auto it : m_clientTable)
                sendAddAnnotationPositionToSD(it.second, *sdMT, *(drawable.get()));
        }
    }

    void VFVServer::onSetAnnotationPositionIndexes(VFVClientSocket* client, const VFVSetAnnotationPositionIndexes& idx)
    {
        //Check if the client is valid
        if(client != NULL && !client->isTablet())
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            VFVSERVER_NOT_A_TABLET
            return;
        }

        AnnotationComponentMetaData<AnnotationPosition>* posIT = nullptr;
        getLogComponentMetaData(idx.annotLogID, idx.annotComponentID, &posIT);
        if(posIT == nullptr)
        {
            VFVSERVER_ANNOTATION_COMPONENT_NOT_FOUND(idx.annotLogID, idx.annotComponentID);
            return;
        }

        posIT->component->setXYZIndices(idx.indexes[0], idx.indexes[1], idx.indexes[2]);

        //Send it to all clients
        {
            std::lock_guard<std::mutex> lock2(m_mapMutex);
            for(auto clt : m_clientTable)
                sendSetAnnotationPositionIndexes(clt.second, *posIT);
        }
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
            sendSubDatasetOwnerToAll(sdMT);
        }
    }

    void VFVServer::onDuplicateSubDataset(VFVClientSocket* client, const VFVDuplicateSubDataset& duplicate)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);
        duplicateSubDataset(client, duplicate);
    }

    SubDatasetMetaData* VFVServer::duplicateSubDataset(VFVClientSocket* client, const VFVDuplicateSubDataset& duplicate)
    {
        //Find the subdataset meta data
        SubDatasetMetaData* sdMT = NULL;
        DatasetMetaData* mt = getMetaData(duplicate.datasetID, duplicate.subDatasetID, &sdMT);
        if(!mt)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(duplicate.datasetID, duplicate.subDatasetID)
            return nullptr;
        }

        if(client)
        {
            //Check about the privacy
            if(sdMT->owner != NULL && sdMT->owner != getHeadsetFromClient(client))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, duplicate.datasetID, duplicate.subDatasetID)
                return nullptr;
            }
        }

        //Find the SubDataset
        Dataset* dataset = getDataset(duplicate.datasetID, duplicate.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(duplicate.datasetID, duplicate.subDatasetID)
            return nullptr;
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
        md.tf     = std::shared_ptr<SubDatasetTFMetaData>(cloneTransferFunction(sdMT->tf));
        md.owner  = sdMT->owner;
        md.mapVisibility = sdMT->mapVisibility;
        sd->setTransferFunction(md.tf->getTF());
        mt->sdMetaData.push_back(md);

        //Send it to all the clients
        for(auto& clt : m_clientTable)
        {
            sendAddSubDataset(clt.second, sd);
            sendSubDatasetStatus(clt.second, sd, duplicate.datasetID);
        }

        return &mt->sdMetaData.back();
    }

    void VFVServer::onMergeSubDatasets(VFVClientSocket* client, const VFVMergeSubDatasets& merge)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        INFO << "On Merge SubDatasets IDs " << merge.sd1ID << " : " << merge.sd2ID << std::endl;

        //Find the subdatasets meta data
        //SD1
        SubDatasetMetaData* sd1MT = NULL;
        DatasetMetaData* mt1 = getMetaData(merge.datasetID, merge.sd1ID, &sd1MT);
        if(!mt1)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(merge.datasetID, merge.sd1ID)
            return;
        }

        if(client)
        {
            //Check about the privacy
            if(sd1MT->owner != NULL && sd1MT->owner != getHeadsetFromClient(client))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, merge.datasetID, merge.sd1ID)
                return;
            }
        }

        //SD2
        SubDatasetMetaData* sd2MT = NULL;
        DatasetMetaData* mt2 = getMetaData(merge.datasetID, merge.sd2ID, &sd2MT);
        if(!mt2)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(merge.datasetID, merge.sd2ID)
            return;
        }

        if(client)
        {
            //Check about the privacy
            if(sd2MT->owner != NULL && sd2MT->owner != sd1MT->owner)
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, merge.datasetID, merge.sd2ID)
                return;
            }
        }

        //Find the SubDatasets
        Dataset* dataset = getDataset(merge.datasetID, merge.sd1ID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(merge.datasetID, merge.sd1ID)
            return;
        }
        SubDataset* sd1 = dataset->getSubDataset(merge.sd1ID);
        SubDataset* sd2 = dataset->getSubDataset(merge.sd2ID);

        if(sd2 == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(merge.datasetID, merge.sd2ID)
            return;
        }

        //Create a SubDataset
        SubDataset* sd = new SubDataset(dataset, sd1->getName() + "-" + sd2->getName(), 0);
        dataset->addSubDataset(sd);

        //Clone the transfer functions
        SubDatasetTFMetaData* clonedSD1TF = cloneTransferFunction(sd1MT->tf);
        SubDatasetTFMetaData* clonedSD2TF = cloneTransferFunction(sd2MT->tf);

        //Handle the metaData
        SubDatasetTFMetaData* tfMD = new SubDatasetTFMetaData(TF_MERGE, std::shared_ptr<TF>(new MergeTF(clonedSD1TF->getTF(), clonedSD2TF->getTF())));
        tfMD->getMergeTFMetaData().tf1 = std::shared_ptr<SubDatasetTFMetaData>(clonedSD1TF);
        tfMD->getMergeTFMetaData().tf2 = std::shared_ptr<SubDatasetTFMetaData>(clonedSD2TF);

        SubDatasetMetaData md;
        md.sdID   = sd->getID();
        md.datasetID = merge.datasetID;
        md.tf     = std::shared_ptr<SubDatasetTFMetaData>(tfMD); 
        md.owner  = sd1MT->owner;
        sd->setTransferFunction(md.tf->getTF());
        mt1->sdMetaData.push_back(md);

        //Send it to all the clients
        for(auto& clt : m_clientTable)
        {
            sendAddSubDataset(clt.second, sd);
            sendSubDatasetStatus(clt.second, sd, merge.datasetID);
        }
    }

    void VFVServer::onLocation(VFVClientSocket* client, const VFVLocation& location)
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        VFVClientSocket* headset = getHeadsetFromClient(client);

        if(headset)
        {
            //Check the mesh
            //if(headset->getHeadsetData().isInVolumetricSelection())
            {
                headset->getHeadsetData().volumetricData.pushLocation(
                    {
                        glm::vec3(location.position[0], location.position[1], location.position[2]),
                        Quaternionf(location.rotation[1], location.rotation[2], location.rotation[3], location.rotation[0])
                    });
            }

            //Generate the data
            uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(float) + 4*sizeof(float);
            uint8_t* data = (uint8_t*)malloc(dataSize);
            uint32_t offset = 0;

            //Message ID
            writeUint16(data, VFV_SEND_TABLET_LOCATION);
            offset += sizeof(uint16_t);

            //Position
            writeFloat(data+offset, location.position[0]);
            offset += sizeof(float);
            writeFloat(data+offset, location.position[1]);
            offset += sizeof(float);
            writeFloat(data+offset, location.position[2]);
            offset += sizeof(float);

            //Rotation
            writeFloat(data+offset, location.rotation[0]);
            offset += sizeof(float);
            writeFloat(data+offset, location.rotation[1]);
            offset += sizeof(float);
            writeFloat(data+offset, location.rotation[2]);
            offset += sizeof(float);
            writeFloat(data+offset, location.rotation[3]);
            offset += sizeof(float);

            std::shared_ptr<uint8_t> sharedData(data, free);

            //Send the data
            SocketMessage<int> sm(headset->socket, sharedData, offset);
            writeMessage(sm);

#ifdef VFV_LOG_DATA
            {
                std::lock_guard<std::mutex> lockJson(m_logMutex);
                m_log << location.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(headset), getTimeOffset());
                m_log << ",\n";
                m_log << std::flush;
            }
#endif
        }
    }

    void VFVServer::onTabletScale(VFVClientSocket* client, const VFVTabletScale& tabletScale)
    {
        INFO << "Tablet scale received: " << tabletScale.scale << std::endl;
                
        VFVClientSocket* headset = getHeadsetFromClient(client);
        if(headset)
        {
            headset->getHeadsetData().volumetricData.lassoScale = glm::vec3(tabletScale.scale * tabletScale.width/2.0f, tabletScale.scale, tabletScale.scale * tabletScale.height/2.0f);

            //Generate the data
            uint32_t dataSize = sizeof(uint16_t) + 5*sizeof(float);
            uint8_t* data = (uint8_t*)malloc(dataSize);
            uint32_t offset = 0;

            //Message ID
            writeUint16(data, VFV_SEND_TABLET_SCALE);
            offset += sizeof(uint16_t);

            //Scale information
            writeFloat(data+offset, tabletScale.scale);
            offset += sizeof(float);
            writeFloat(data+offset, tabletScale.width);
            offset += sizeof(float);
            writeFloat(data+offset, tabletScale.height);
            offset += sizeof(float);
            writeFloat(data+offset, tabletScale.posx);
            offset += sizeof(float);
            writeFloat(data+offset, tabletScale.posy);
            offset += sizeof(float);

            std::shared_ptr<uint8_t> sharedData(data, free);

            //Send the data
            SocketMessage<int> sm(headset->socket, sharedData, offset);
            writeMessage(sm);
        }
    }

    void VFVServer::onLasso(VFVClientSocket* client, const VFVLasso& lasso)
    {
        INFO << "Lasso received: size: " << lasso.size << std::endl;
        if(lasso.size % 3 != 0)
            WARNING << "The lasso is not valid. Assert fail: lasso.size % 3 == 0" << std::endl;
         
        std::lock_guard<std::mutex> lockMap(m_mapMutex);
        VFVClientSocket* headset = getHeadsetFromClient(client);
        if(headset)
        {
            //Store the new lasso
            headset->getHeadsetData().volumetricData.lasso.clear();
            for(int32_t i = 0; i < (int32_t)lasso.size-2; i+=3)
                headset->getHeadsetData().volumetricData.lasso.push_back(glm::vec2(lasso.data[i], lasso.data[i+1])); 

            //And send it to the headset
            uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t) + lasso.size * sizeof(float);
            uint8_t* data = (uint8_t*)malloc(dataSize);
            uint32_t offset = 0;

            //Message ID
            writeUint16(data, VFV_SEND_LASSO);
            offset += sizeof(uint16_t);

            //Size
            writeUint32(data+offset, lasso.size);
            offset += sizeof(uint32_t);

            //Lasso data
            for(uint32_t i = 0; i < lasso.size; i++)
            {
                writeFloat(data+offset, lasso.data[i]);
                offset += sizeof(float);
            }

            std::shared_ptr<uint8_t> sharedData(data, free);

            //Send the data
            SocketMessage<int> sm(headset->socket, sharedData, offset);
            writeMessage(sm);
        }
    }

    void VFVServer::onConfirmSelection(VFVClientSocket* client, const VFVConfirmSelection& confirmSelection)
    {
        INFO << "Selection confirmed" << std::endl;
                
        std::lock_guard<std::mutex> lockDataset(m_datasetMutex);
        std::lock_guard<std::mutex> lock(m_mapMutex);

        VFVClientSocket* headset = getHeadsetFromClient(client);
        if(headset)
        {
            /*----------------------------------------------------------------------------*/
            /*-----------------------Apply the volumetric selection-----------------------*/
            /*----------------------------------------------------------------------------*/
            headset->getHeadsetData().volumetricData.closeCurrentMesh();

            //Get the subdataset and the associated dataset
            Dataset* dataset = getDataset(confirmSelection.datasetID, confirmSelection.subDatasetID);
            if(dataset == NULL)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(confirmSelection.datasetID, confirmSelection.subDatasetID)
                return;
            }
            SubDataset* sd = dataset->getSubDataset(confirmSelection.subDatasetID);

            //Get the type of the dataset
            void(*applyFunc)(const VolumetricMesh&, SubDataset*) = nullptr;
            switch(getDatasetType(dataset))
            {
                case DATASET_TYPE_VTK:
                    applyFunc = &applyVolumetricSelection_vtk;
                    break;

                case DATASET_TYPE_CLOUD_POINT:
                    applyFunc = &applyVolumetricSelection_cloudPoint;
                    break;

                default:
                    break;
            }

            //Apply the correct function
            for(auto& mesh : headset->getHeadsetData().volumetricData.meshes)
                applyFunc(mesh, sd);

            sd->enableVolumetricMask(true);

            /*----------------------------------------------------------------------------*/
            /*---------------------Send the confirm selection message---------------------*/
            /*----------------------------------------------------------------------------*/
            uint32_t dataSize = sizeof(uint16_t) + 2*sizeof(uint32_t);
            uint8_t* data = (uint8_t*)malloc(dataSize);
            size_t offset = 0;

            //Message ID
            writeUint16(data, VFV_SEND_CONFIRM_SELECTION);
            offset += sizeof(uint16_t);
            
            //DatasetID
            writeUint32(data+offset, confirmSelection.datasetID);
            offset += sizeof(uint32_t);

            //SubDatasetID
            writeUint32(data+offset, confirmSelection.subDatasetID);
            offset += sizeof(uint32_t);

            std::shared_ptr<uint8_t> sharedData(data, free);

            //Send the data
            SocketMessage<int> sm(headset->socket, sharedData, offset);
            writeMessage(sm);

            /*----------------------------------------------------------------------------*/
            /*----------------------Send the volumetric mask as well----------------------*/
            /*----------------------------------------------------------------------------*/

            std::shared_ptr<uint8_t> sharedVolData = generateVolumetricMaskEvent(sd, confirmSelection.datasetID, &offset);

            for(auto it : m_clientTable)
                sendVolumetricMaskDataset(it.second, sharedVolData, offset);
        }
    }

    void VFVServer::onAddNewSelectionInput(VFVClientSocket* client, const VFVAddNewSelectionInput& addInput)
    {
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        VFVClientSocket* headset = getHeadsetFromClient(client);
        if(!headset)
            return;

        headset->getHeadsetData().volumetricData.pushMesh((BooleanSelectionOp)addInput.booleanOp);

        //Set the current action
        sendAddNewSelectionInput(headset, addInput);
    }

    void VFVServer::onToggleMapVisibility(VFVClientSocket* client, const VFVToggleMapVisibility& visibility)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        Dataset* dataset = getDataset(visibility.datasetID, visibility.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(visibility.datasetID, visibility.subDatasetID)
            return;
        }

        //Find the subdataset meta data and update it
        SubDatasetMetaData* sdMT = NULL;
        DatasetMetaData* mt = NULL;
        if(client)
            mt = updateMetaDataModification(client, visibility.datasetID, visibility.subDatasetID, &sdMT);
        else
            mt = getMetaData(visibility.datasetID, visibility.subDatasetID, &sdMT);

        if(!mt)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(visibility.datasetID, visibility.subDatasetID)
            return;
        }

        //Check about the privacy
        if(!canModifySubDataset(client, sdMT))
        {
            VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, visibility.datasetID, visibility.subDatasetID)
            return;
        }

        sdMT->mapVisibility = visibility.visibility;

        for(auto& clt: m_clientTable)
            sendToggleMapVisibility(clt.second, visibility);
    }

    void VFVServer::onRemoveSubDataset(VFVClientSocket* client, const VFVRemoveSubDataset& remove)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        //Find the subdataset meta data
        SubDatasetMetaData* sdMT = NULL;
        DatasetMetaData* mt = getMetaData(remove.datasetID, remove.subDatasetID, &sdMT);
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

    void VFVServer::onRenameSubDataset(VFVClientSocket* client, const VFVRenameSubDataset& rename)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        //Find the subdataset meta data
        SubDatasetMetaData* sdMT = NULL;
        DatasetMetaData* mt = getMetaData(rename.datasetID, rename.subDatasetID, &sdMT);
        if(!mt)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(rename.datasetID, rename.subDatasetID)
            return;
        }

        if(client)
        {
            //Check about the privacy
            if(!canModifySubDataset(client, sdMT))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, rename.datasetID, rename.subDatasetID)
                return;
            }
        }

        Dataset* dataset = getDataset(rename.datasetID, rename.subDatasetID);
        if(dataset == nullptr)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(rename.datasetID, rename.subDatasetID)
            return;
        }
        SubDataset* sd = dataset->getSubDataset(rename.subDatasetID);

        sd->setName(rename.name);

        for(auto clt : m_clientTable)
            sendRenameSubDataset(clt.second, rename);
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
            DatasetMetaData* mt = updateMetaDataModification(client, rotate.datasetID, rotate.subDatasetID, &sdMT);
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

        updateSDGroup(sd);

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
            DatasetMetaData* mt = updateMetaDataModification(client, translate.datasetID, translate.subDatasetID, &sdMT);
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

        updateSDGroup(sd);

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
        DatasetMetaData* mt = NULL;
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

        //Set the transfer function
        sdMT->tf = std::shared_ptr<SubDatasetTFMetaData>(messageToTF(sd, tfSD));
        sd->setTransferFunction(sdMT->tf->getTF());

        //Set the headsetID
        if(client)
        {
            VFVClientSocket* headset = getHeadsetFromClient(client);
            if(headset)
                tfSD.headsetID = headset->getHeadsetData().id;
        }

        updateSDGroup(sd, true);

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
            DatasetMetaData* mt = updateMetaDataModification(client, scale.datasetID, scale.subDatasetID, &sdMT);
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

        updateSDGroup(sd);

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

    void VFVServer::setSubDatasetClipping(VFVClientSocket* client, VFVSetSubDatasetClipping& clipping)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex);
        std::lock_guard<std::mutex> lockMap(m_mapMutex);

        Dataset* dataset = getDataset(clipping.datasetID, clipping.subDatasetID);
        if(dataset == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(clipping.datasetID, clipping.subDatasetID)
            return;
        }

        //Search for the meta data
        SubDataset* sd = dataset->getSubDataset(clipping.subDatasetID);

        if(client)
        {
            //Find the subdataset meta data and update it
            SubDatasetMetaData* sdMT = NULL;
            DatasetMetaData* mt = updateMetaDataModification(client, clipping.datasetID, clipping.subDatasetID, &sdMT);
            if(!mt)
            {
                VFVSERVER_SUB_DATASET_NOT_FOUND(clipping.datasetID, clipping.subDatasetID)
                return;
            }

            //Check about the privacy
            if(!canModifySubDataset(client, sdMT))
            {
                VFVSERVER_CANNOT_MODIFY_SUBDATASET(client, clipping.datasetID, clipping.subDatasetID)
                return;
            }
        }

        sd->setDepthClipping(clipping.minDepthClipping, clipping.maxDepthClipping);
        updateSDGroup(sd);

        //Send to all
        for(auto& clt : m_clientTable)
            if(clt.second != client)
                sendSubDatasetClippingEvent(clt.second, clipping);
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
//        std::lock_guard<std::mutex> lockMap(m_mapMutex);
        if(!client->isTablet())
        {
            VFVSERVER_NOT_A_TABLET
            return;
        }

//        if(client->getTabletData().headset != NULL)
//            sendStartAnnotation(client->getTabletData().headset, startAnnot);
        VFVAnchorAnnotation anchorAnnot;
        anchorAnnot.subDatasetID = startAnnot.subDatasetID;
        anchorAnnot.datasetID    = startAnnot.datasetID;
        anchorAnnot.localPos[0] = 0.0f;
        anchorAnnot.localPos[1] = 0.0f;
        anchorAnnot.localPos[2] = 0.0f;
        onAnchorAnnotation(nullptr, anchorAnnot);
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

            annotID = sd->getAnnotationCanvas().size();
            sd->emplaceAnnotationCanvas(640, 640, anchorAnnot.localPos);
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
        while(sd->getAnnotationCanvas().size())
            sd->removeAnnotationCanvas(sd->getAnnotationCanvas().begin());

        for(auto& clt : m_clientTable)
            sendClearAnnotations(clt.second, clearAnnots);
    }

    void VFVServer::onResetVolumetricSelection(VFVClientSocket* client, const VFVResetVolumetricSelection& reset)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex); //Ensure that no one is touching the datasets
        std::lock_guard<std::mutex> lock2(m_mapMutex);    //Ensute that no one is modifying the list of clients (and relevant information)

        //Check that the dataset exists
        Dataset* dataset = getDataset(reset.datasetID, reset.subDatasetID);
        if(dataset == nullptr)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(reset.datasetID, reset.subDatasetID)
            return;
        }
        SubDataset* sd = dataset->getSubDataset(reset.subDatasetID);
        sd->resetVolumetricMask(true, false);

        //Get the headset asking for this piece of information
        int headsetID = -1;
        if(client)
        {
            VFVClientSocket* hmdClient = getHeadsetFromClient(client);
            if(hmdClient == NULL)
            {
                WARNING << "Not connected to a headset yet..." << std::endl;
            }
            headsetID = hmdClient->getHeadsetData().id;
        }

        updateSDGroup(sd);

        for(auto& clt : m_clientTable)
            sendResetVolumetricSelection(clt.second, reset.datasetID, reset.subDatasetID, headsetID);
    }

    void VFVServer::setDrawableAnnotationPositionColor(VFVClientSocket* client, const VFVSetDrawableAnnotationPositionDefaultColor& color)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex); //Ensure that no one is touching the datasets

        //Search for the meta data
        SubDatasetMetaData* sdMT = NULL;
        getMetaData(color.datasetID, color.subDatasetID, &sdMT);
        if(sdMT == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(color.datasetID, color.subDatasetID)
            return;
        }

        //Search for the drawable meta data and set the data
        std::shared_ptr<DrawableAnnotationPositionMetaData> drawable = sdMT->getDrawableAnnotation<DrawableAnnotationPositionMetaData>(color.drawableID);
        float r =  ((color.color >> 16)&0xff)/255.0f;
        float g =  ((color.color >> 8 )&0xff)/255.0f;
        float b =  ((color.color      )&0xff)/255.0f;
        float a =  ((color.color >> 24)&0xff)/255.0f;
        drawable->drawable->setColor(glm::vec4(r, g, b, a));

        //Send the information to every clients
        std::lock_guard<std::mutex> lock2(m_mapMutex);    //Ensure that no one is modifying the list of clients (and relevant information)
        for(auto& clt : m_clientTable)
            sendSetDrawableAnnotationPositionColor(clt.second, color);
    }

    void VFVServer::setDrawableAnnotationPositionIdx(VFVClientSocket* client, const VFVSetDrawableAnnotationPositionMappedIdx& idx)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex); //Ensure that no one is touching the datasets

        //Search for the meta data
        SubDatasetMetaData* sdMT = NULL;
        getMetaData(idx.datasetID, idx.subDatasetID, &sdMT);
        if(sdMT == NULL)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(idx.datasetID, idx.subDatasetID)
            return;
        }

        //Search for the drawable meta data and set the data
        std::shared_ptr<DrawableAnnotationPositionMetaData> drawable = sdMT->getDrawableAnnotation<DrawableAnnotationPositionMetaData>(idx.drawableID);
        drawable->drawable->setMappedDataIndices(idx.idx);

        //Send the information to every clients
        std::lock_guard<std::mutex> lock2(m_mapMutex);    //Ensure that no one is modifying the list of clients (and relevant information)
        for(auto& clt : m_clientTable)
            sendSetDrawableAnnotationPositionIdx(clt.second, idx);
    }

    void VFVServer::addSubjectiveViewGroup(VFVClientSocket* client, const VFVAddSubjectiveViewGroup& addSV)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex); //Ensure that no one is touching the datasets
        std::lock_guard<std::mutex> lock2(m_mapMutex);    //Ensure that no one is modifying the list of clients (and relevant information)

        VFVClientSocket* hmdClient = nullptr;
        if(client != NULL) //Not the server
        {
            hmdClient = getHeadsetFromClient(client);
            if(hmdClient == NULL)
            {
                WARNING << "Not connected to a headset yet..." << std::endl;
                return;
            }
        }

        //Find and search for the dataset/subdataset
        SubDatasetMetaData* sdMT = nullptr;
        SubDataset*         sd   = nullptr;
        getMetaData(addSV.baseDatasetID, addSV.baseSDID, &sdMT);
        getDataset(addSV.baseDatasetID, addSV.baseSDID, &sd);
        if(sdMT == NULL || sd == nullptr)
        {
            VFVSERVER_SUB_DATASET_NOT_FOUND(addSV.baseDatasetID, addSV.baseSDID)
            return;
        }

        if(!canModifySubDataset(client, sdMT))
        {
            ERROR << "The subdataset serving as a base is not public and is not owned by this client.... Exiting\n";
            return;
        }

        INFO << "Creating a new subjective view group..." << std::endl;

        //Create the subjective view
        SubDatasetSubjectiveGroup* svGroup = nullptr;

        if(addSV.svType == SD_GROUP_SV_STACKED ||
           addSV.svType == SD_GROUP_SV_LINKED  ||
           addSV.svType == SD_GROUP_SV_STACKED_LINKED)
            svGroup = new SubDatasetSubjectiveStackedLinkedGroup(sd);

        if(svGroup == nullptr)
        {
            WARNING << "Was not able to create a subjective view typed " << addSV.svType << ". Discard\n";
            return;
        }

        INFO << "Add the subjective view group to the known groups..." << std::endl;

        //Add the subjective view to the known groups
        SubDatasetGroupMetaData sdgMT;
        sdgMT.type    = (SubDatasetGroupType)addSV.svType;
        sdgMT.sdGroup = std::shared_ptr<SubDatasetGroup>(svGroup);
        sdgMT.sdgID   = m_currentSDGroup;
        sdgMT.owner   = sdMT->owner;
        m_currentSDGroup++;
        m_sdGroups.insert(std::pair(sdgMT.sdgID, sdgMT));

        //Set based subdataset to this subdataset group
        sdMT->sdgID = sdgMT.sdgID;

        INFO << "Send the subjective view group to all clients..." << std::endl;

        //Send this group to everyone
        VFVAddSubjectiveViewGroup cpyAddSV = addSV;
        cpyAddSV.sdgID = sdgMT.sdgID;
        for(auto& clt : m_clientTable)
            sendAddSubjectiveViewGroup(clt.second, cpyAddSV);

        if(addSV.svType == SD_GROUP_SV_STACKED ||
           addSV.svType == SD_GROUP_SV_LINKED  ||
           addSV.svType == SD_GROUP_SV_STACKED_LINKED)
        {
            SubDatasetSubjectiveStackedLinkedGroup* svg = (SubDatasetSubjectiveStackedLinkedGroup*)svGroup;
            VFVSetSVStackedGroupGlobalParameters params;
            params.sdgID = sdgMT.sdgID;
            params.merged = svg->getMerge();
            params.gap = svg->getGap();
            params.stackMethod = svg->getStackingMethod();

            for(auto& clt : m_clientTable)
            {
                sendSVStackedGroupGlobalParameters(clt.second, params);
            }
        }

        INFO << "Create a personal subjective view for this client...." << std::endl;
        //Create a subjective view for this client (if applied)
        if(hmdClient)
        {
            VFVAddClientToSVGroup addClient;
            addClient.sdgID = sdgMT.sdgID;
            addClientToSVGroup(client, addClient);
        }

        INFO << "End of subjective views..." << std::endl;
    }

    void VFVServer::onRemoveSubDatasetGroup(VFVClientSocket* client, const VFVRemoveSubDatasetGroup& removeSDGroup)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex); //Ensure that no one is touching the datasets
        std::lock_guard<std::mutex> lock2(m_mapMutex);

        //Searching for the sd group
        auto svIT = m_sdGroups.find(removeSDGroup.sdgID);
        if(svIT == m_sdGroups.end())
        {
            ERROR << "Was not able to find the subdataset group " << removeSDGroup.sdgID << std::endl;
            return;
        }


        if(!canClientModifySubDatasetGroup(client, svIT->second))
        {
            ERROR << "The client cannot modify this subdataset group ID " << removeSDGroup.sdgID << std::endl;
            return;
        }

        removeSubDatasetGroup(removeSDGroup);
    }

    void VFVServer::removeSubDatasetGroup(const VFVRemoveSubDatasetGroup& removeSDGroup)
    {
        //Searching for the sd group
        auto svIT = m_sdGroups.find(removeSDGroup.sdgID);
        if(svIT == m_sdGroups.end())
        {
            ERROR << "Was not able to find the subdataset group " << removeSDGroup.sdgID << std::endl;
            return;
        }

        //It might be useful to test per sd group types.
        if(svIT->second.isSubjectiveView())
        {
            std::list<SubDataset*> subdatasets = svIT->second.sdGroup->getSubDatasets();

            //If subjective views --> remove every subjective views
            SubDataset* base = ((SubDatasetSubjectiveGroup*)svIT->second.sdGroup.get())->getBase();
            uint32_t datasetID = getDatasetID(base->getParent());

            for(auto sd : subdatasets)
            {
                if(sd != base)
                {
                    VFVRemoveSubDataset removeSD;
                    removeSD.datasetID    = datasetID;
                    removeSD.subDatasetID = sd->getID();
                    removeSubDataset(removeSD);
                }
            }
        }

        //Update SD meta data
        std::list<SubDataset*> subdatasets = svIT->second.sdGroup->getSubDatasets();
        for(auto sd : subdatasets)
        {
            uint32_t datasetID = getDatasetID(sd->getParent());
            SubDatasetMetaData* sdMD;
            getMetaData(datasetID, sd->getID(), &sdMD);
            if(sdMD)
                sdMD->sdgID = -1;
        }
        m_sdGroups.erase(svIT);

        //Send the message to the other users
        for(auto& clt : m_clientTable)
            sendRemoveSubDatasetsGroup(clt.second, removeSDGroup);

        INFO << "End of removing SubDatasetGroup" << std::endl;
    }

    void VFVServer::setSubjectiveViewStackedParameters(VFVClientSocket* client, const VFVSetSVStackedGroupGlobalParameters& params)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex); //Ensure that no one is touching the datasets

        //Searching for the subjective group
        auto svIT = m_sdGroups.find(params.sdgID);
        if(svIT == m_sdGroups.end())
        {
            ERROR << "Was not able to find the subdataset group " << params.sdgID << std::endl;
            return;
        }

        if(!svIT->second.isStackedSubjectiveView())
        {
            ERROR << "The subdataset group ID " << params.sdgID << " is not a subjective stacked view group.\n";
            return;
        }

        if(params.stackMethod >= STACK_END)
        {
            ERROR << "The stack method " << params.stackMethod << " is unknown...\n";
            return;
        }

        if(!canClientModifySubDatasetGroup(client, svIT->second))
        {
            ERROR << "The client cannot modify the subdataset group ID " << params.sdgID << std::endl;
            return;
        }

        SubDatasetSubjectiveStackedGroup* svg = (SubDatasetSubjectiveStackedGroup*)svIT->second.sdGroup.get();
        svg->setMerge(params.merged);
        svg->setStackingMethod((StackingEnum)params.stackMethod);
        svg->setGap(params.gap);

        svg->updateSubDatasets();

        //Retrieve the datasetID
        //uint32_t datasetID = getDatasetID(svg->getBase()->getParent());

        std::lock_guard<std::mutex> lock2(m_mapMutex);
        for(auto& clt : m_clientTable)
        {
            sendSVStackedGroupGlobalParameters(clt.second, params);

/* 
 *          for(SubDataset* sd : svg->getSubDatasets())
 *              sendSubDatasetPositionStatus(clt.second, sd, datasetID);
*/
        }
    }

    void VFVServer::addClientToSVGroup(VFVClientSocket* client, const VFVAddClientToSVGroup& addClient)
    {
        //Searching for the subjective group
        auto svIT = m_sdGroups.find(addClient.sdgID);
        if(svIT == m_sdGroups.end())
        {
            ERROR << "Was not able to find the subdataset group " << addClient.sdgID << std::endl;
            return;
        }

        if(!svIT->second.isSubjectiveView())
        {
            ERROR << "The subdataset group ID " << addClient.sdgID << " is not a subjective group.\n";
            return;
        }

        SubDatasetSubjectiveGroup* svg = (SubDatasetSubjectiveGroup*)svIT->second.sdGroup.get();

        if(!canClientModifySubDatasetGroup(client, svIT->second))
        {
            ERROR << "The client cannot modify the subdataset group ID " << addClient.sdgID << std::endl;
            return;
        }

        VFVClientSocket* hmdClient = getHeadsetFromClient(client);
        if(hmdClient == NULL)
        {
            ERROR << "Cannot add a subdataset subjective view to a non-connected client..." << std::endl;
            return;
        }

        //Duplicate the base
        uint32_t datasetID = getDatasetID(svg->getBase()->getParent());

        if(datasetID == (uint32_t)-1)
        {
            ERROR << "A Dataset was not found.... Internal error\n";
            return;
        }

        if(svIT->second.isStackedSubjectiveView())
        {
            SubDataset* subjectiveSDs[2] = {nullptr, nullptr};
            bool        toDuplicate[2]   = {false, false};
            if(svIT->second.type == SD_GROUP_SV_STACKED ||
               svIT->second.type == SD_GROUP_SV_STACKED_LINKED)
                toDuplicate[0] = true;

            if(svIT->second.type == SD_GROUP_SV_LINKED ||
               svIT->second.type == SD_GROUP_SV_STACKED_LINKED)
                toDuplicate[1] = true;
            for(uint32_t i = 0; i < 2; i++)
            {
                if(!toDuplicate[i])
                    continue;
                VFVDuplicateSubDataset duplicate;
                duplicate.datasetID    = datasetID;
                duplicate.subDatasetID = svg->getBase()->getID();
                SubDatasetMetaData* sdMT = duplicateSubDataset(client, duplicate);
                if(sdMT == nullptr)
                {
                    ERROR << "Was not able to duplicate the base subdataset of the subdataset group... quitting\n";

                    //Unbound previous added subdatasets
                    for(uint32_t j = 0; j < i; j++)
                    {
                        if(!toDuplicate[j])
                            continue;


                        getMetaData(datasetID, subjectiveSDs[j]->getID(), &sdMT);
                        if(sdMT)
                        {
                            sdMT->sdgID = -1;
                            sdMT->owner = nullptr;
                            VFVRemoveSubDataset removeSD;
                            removeSD.datasetID    = datasetID;
                            removeSD.subDatasetID = subjectiveSDs[j]->getID();
                            removeSubDataset(removeSD);
                        }
                    }
                    return;
                }

                sdMT->sdgID = svIT->second.sdgID;
                sdMT->owner = hmdClient;
                subjectiveSDs[i] = m_datasets[sdMT->datasetID]->getSubDataset(sdMT->sdID);
            }

            //Check linked subdataset, and place it at the correct position
            if(toDuplicate[1])
            {
                glm::vec3 sdPosition = hmdClient->getHeadsetData().position + 
                                       hmdClient->getHeadsetData().rotation * glm::vec3(0.0f, 0.0f, 0.7f) +
                                       glm::vec3(0.0f, -0.50f, 0.0f);

                glm::vec3 sdScale = glm::vec3(0.5f, 0.5f, 0.5f);
                subjectiveSDs[1]->setScale(sdScale);
                subjectiveSDs[1]->setPosition(sdPosition);
            }

            ((SubDatasetSubjectiveStackedLinkedGroup*)(svg))->addSubjectiveSubDataset(subjectiveSDs[0], subjectiveSDs[1]);
            svg->updateSubDatasets();

            //Send "add SubDataset to SVGroup", and send the new updated positions/graphical properties
            for(auto& clt : m_clientTable)
            {
                for(uint32_t i = 0; i < 2; i++)
                    if(subjectiveSDs[i])
                        sendSubDatasetStatus(clt.second, subjectiveSDs[i], datasetID);

                sendAddSubDatasetToSVStackedGroup(clt.second, svIT->second,
                                                  datasetID, (uint32_t)((subjectiveSDs[0])?subjectiveSDs[0]->getID():-1), 
                                                             (uint32_t)((subjectiveSDs[1])?subjectiveSDs[1]->getID():-1));
            }
        }
    }

    void VFVServer::onAddClientToSVGroup(VFVClientSocket* client, const VFVAddClientToSVGroup& addClient)
    {
        std::lock_guard<std::mutex> lock(m_datasetMutex); //Ensure that no one is touching the datasets
        std::lock_guard<std::mutex> lock2(m_mapMutex);    //Ensure that no one is modifying the list of clients (and relevant information)

        addClientToSVGroup(client, addClient);
    }

    /*----------------------------------------------------------------------------*/
    /*-------------------------------SEND MESSAGES--------------------------------*/
    /*----------------------------------------------------------------------------*/

    void VFVServer::saveMessageSentToJSONLog(VFVClientSocket* client, const VFVDataInformation& data)
    {
#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            m_log << data.toJson(VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset())<< ",\n";
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendEmptyMessage(VFVClientSocket* client, uint16_t type)
    {
        uint8_t* data = (uint8_t*)malloc(sizeof(uint16_t));
        writeUint16(data, type);

        INFO << "Sending EMPTY MESSAGE Event data. Type : " << type << std::endl;
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, sizeof(uint16_t));
        writeMessage(sm);

        VFVNoDataInformation noData;
        noData.type = type;
        saveMessageSentToJSONLog(client, noData);
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

        saveMessageSentToJSONLog(client, dataset);
    }

    void VFVServer::sendAddCloudPointDatasetEvent(VFVClientSocket* client, const VFVCloudPointDatasetInformation& dataset, uint32_t datasetID)
    {
        uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t) + 
                            sizeof(uint8_t)*dataset.name.size();

        uint8_t* data = (uint8_t*)malloc(dataSize);

        uint32_t offset=0;

        writeUint16(data, VFV_SEND_ADD_CLOUDPOINT_DATASET); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, datasetID); //The datasetID
        offset += sizeof(uint32_t);

        writeUint32(data+offset, dataset.name.size()); //Dataset name size
        offset += sizeof(uint32_t); 

        memcpy(data+offset, dataset.name.data(), dataset.name.size()); //Dataset name
        offset += dataset.name.size()*sizeof(uint8_t);

        INFO << "Sending ADD CLOUD POINT DATASET Event data. File : " << dataset.name << "\n";
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, dataset);
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

        saveMessageSentToJSONLog(client, dataset);
    }

    void VFVServer::sendAddLogData(VFVClientSocket* client, const VFVOpenLogData& logData, uint32_t logID)
    {
        uint32_t dataSize = sizeof(uint16_t) + 
                            sizeof(uint32_t) +
                            sizeof(uint32_t) + logData.fileName.size() +
                            sizeof(uint8_t)  +
                            sizeof(uint32_t);

        uint8_t* data   = (uint8_t*)malloc(dataSize);
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_ADD_LOG_DATASET);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, logID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, logData.fileName.size());
        offset += sizeof(uint32_t);

        for(uint32_t i = 0; i < logData.fileName.size(); offset++, i++)
            data[offset] = logData.fileName[i];

        data[offset] = logData.hasHeader;
        offset++;

        writeUint32(data+offset, logData.timeID);
        offset += sizeof(uint32_t);

        INFO << "Sending ADD LOG DATASET Event data. Data : " << logData.fileName << " ID " << logID << std::endl;
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "AddLogData");
            m_log << ",    \"logID\" : " << logID << ",\n"
                  << "    \"fileName\" : " << logData.fileName << ",\n"
                  << "    \"hasHeader\" : " << logData.hasHeader << ",\n" 
                  << "    \"timeID\" : " << logData.timeID << "\n"
                  << "},\n";
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendAddAnnotationPositionData(VFVClientSocket* client, const AnnotationComponentMetaData<AnnotationPosition>& posMT)
    {
        uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t);
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_ADD_ANNOTATION_POSITION);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, posMT.annotID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, posMT.compID);
        offset += sizeof(uint32_t);

        INFO << "Sending 'ADD ANNOTATION POSITION Event data. AnnotID: " << posMT.annotID << " PosID: " << posMT.compID << std::endl;

        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "AddAnnotationPosition");
            m_log << ",    \"annotID\" : " << posMT.annotID << ",\n"
                  << "    \"compID\" : " << posMT.compID << "\n"
                  << "},\n";
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendSetAnnotationPositionIndexes(VFVClientSocket* client, const AnnotationComponentMetaData<AnnotationPosition>& posMT)
    {
        uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t) + 3*sizeof(uint32_t);
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_SET_ANNOTATION_POSITION_INDEXES);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, posMT.annotID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, posMT.compID);
        offset += sizeof(uint32_t);

        int32_t indices[3];
        posMT.component->getPosIndices(indices);

        for(uint32_t i = 0; i < 3; i++, offset+=sizeof(uint32_t))
            writeUint32(data+offset, indices[i]);

        INFO << "Sending 'SET ANNOTATION POSITION INDICES Event data. AnnotID: " << posMT.annotID << " PosID: " << posMT.compID 
             << "X: " << indices[0] << " Y: " << indices[1] << " Z: " << indices[2] << std::endl;

        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "SetAnnotationPositionIndexes");
            m_log << ",    \"annotID\" : " << posMT.annotID << ",\n"
                  << "    \"compID\" : " << posMT.compID << ",\n"
                  << "    \"indexes\" : [" << indices[0] << ", " << indices[1] << ", " << indices[2] << "]\n"
                  << "},\n";
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendAddAnnotationPositionToSD(VFVClientSocket* client, const SubDatasetMetaData& sdMT, const DrawableAnnotationPositionMetaData& drawable)
    {
        uint32_t dataSize = sizeof(uint16_t) + 5*sizeof(uint32_t);
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_ADD_ANNOTATION_POSITION_TO_SD);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, sdMT.datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, sdMT.sdID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, drawable.compMetaData->annotID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, drawable.compMetaData->compID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, drawable.drawableID);
        offset += sizeof(uint32_t);

        INFO << "Sending 'ADD ANNOTATION POSITION INDICES Event data. DatasetID: " << sdMT.datasetID << " sdID: " << sdMT.sdID 
             << " AnnotID: " << drawable.compMetaData->annotID << " compID: " << drawable.compMetaData->compID << " drawableID: " << drawable.drawableID << std::endl;

        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "SetAnnotationPositionIndexes");
            m_log << ",    \"datasetID\" : " << sdMT.datasetID << ",\n"
                  << "    \"subDatasetID\" : " << sdMT.sdID << ",\n"
                  << "    \"annotID\" : " << drawable.compMetaData->annotID << ",\n"
                  << "    \"compID\" : " << drawable.compMetaData->compID << ",\n"
                  << "    \"drawableID\" : " << drawable.drawableID << "\n"
                  << "},\n";
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

        saveMessageSentToJSONLog(client, rotate);
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

        saveMessageSentToJSONLog(client, scale);
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

        saveMessageSentToJSONLog(client, position);
    }

    void VFVServer::sendSubDatasetClippingEvent(VFVClientSocket* client, const VFVSetSubDatasetClipping& clipping)
    {
        uint32_t dataSize = sizeof(uint16_t) + 2*sizeof(uint32_t) + 2*sizeof(float);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset=0;

        writeUint16(data, VFV_SEND_SET_SUBDATASET_CLIPPING); //Type
        offset += sizeof(uint16_t);

        writeUint32(data+offset, clipping.datasetID); //The datasetID
        offset += sizeof(uint32_t);

        writeUint32(data+offset, clipping.subDatasetID); //SubDataset ID
        offset += sizeof(uint32_t); 

        writeFloat(data+offset, clipping.minDepthClipping);
        offset += sizeof(float);

        writeFloat(data+offset, clipping.maxDepthClipping);
        offset += sizeof(float);

        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, clipping);
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

    /** \brief  Get the size needed to allocated for a specific transfer function
     * \param tfSD the transfer function to evaluate
     * \return   the size in byte to allocate for sending this transfer function */
    static size_t getTransferFunctionSize(const VFVTransferFunctionSubDataset& tfSD)
    {
        int size = 0;
        switch(tfSD.tfID)
        {
            case TF_GTF:
            case TF_TRIANGULAR_GTF:
            {
                size = sizeof(uint32_t) + (sizeof(uint32_t) + 2*sizeof(float))*tfSD.gtfData.propData.size();
                break;
            }
            case TF_MERGE:
            {
                size = sizeof(float) + getTransferFunctionSize(*tfSD.mergeTFData.tf1.get()) + getTransferFunctionSize(*tfSD.mergeTFData.tf2.get()) + 2*(2*sizeof(uint8_t) + 3*sizeof(float)); //2 == colorMode + type, 3*sizeof(float) == timesteps + min + max clipping. Multiply by two because we merge two TF
                break;
            }
            default:
                WARNING << "TF ID : " << tfSD.tfID << " is not found. Size == 0" << std::endl;
                break;
        }

        return size;
    }

    static uint32_t fillTransferFunctionMessage(uint8_t* data, uint32_t offset, const VFVTransferFunctionSubDataset& tfSD)
    {
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
            case TF_MERGE:
            {
                writeFloat(data+offset, tfSD.mergeTFData.t);
                offset += sizeof(float);

                //TF1
                data[offset] = tfSD.mergeTFData.tf1->tfID;
                offset++;
                data[offset] = tfSD.mergeTFData.tf1->colorMode;
                offset++;
                writeFloat(data+offset, tfSD.mergeTFData.tf1->timestep);
                offset += sizeof(float);
                writeFloat(data+offset, tfSD.mergeTFData.tf1->minClipping);
                offset += sizeof(float);
                writeFloat(data+offset, tfSD.mergeTFData.tf1->maxClipping);
                offset += sizeof(float);
                offset = fillTransferFunctionMessage(data, offset, *tfSD.mergeTFData.tf1.get());

                //TF2
                data[offset] = tfSD.mergeTFData.tf2->tfID;
                offset++;
                data[offset] = tfSD.mergeTFData.tf2->colorMode;
                offset++;
                writeFloat(data+offset, tfSD.mergeTFData.tf2->timestep);
                offset += sizeof(float);
                writeFloat(data+offset, tfSD.mergeTFData.tf2->minClipping);
                offset += sizeof(float);
                writeFloat(data+offset, tfSD.mergeTFData.tf2->maxClipping);
                offset += sizeof(float);
                offset = fillTransferFunctionMessage(data, offset, *tfSD.mergeTFData.tf2.get());

                break;
            }

            default:
                break;
        }

        return offset;
    }

    void VFVServer::sendTransferFunctionDataset(VFVClientSocket* client, const VFVTransferFunctionSubDataset& tfSD)
    {
        //Determine the size of the packet
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + 2*sizeof(uint8_t) + 3*sizeof(float) + getTransferFunctionSize(tfSD);

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

        writeFloat(data+offset, tfSD.timestep); //the timestep
        offset += sizeof(float);

        writeFloat(data+offset, tfSD.minClipping); //the minimum clipping
        offset += sizeof(float);

        writeFloat(data+offset, tfSD.maxClipping); //the maximum clipping
        offset += sizeof(float);

        INFO << "Timestep: " << tfSD.timestep << std::endl;

        //Fill tf-specific data
        offset = fillTransferFunctionMessage(data, offset, tfSD);

        INFO << "Sending TF_DATASET Event data Dataset ID " << tfSD.datasetID << " sdID : " << tfSD.subDatasetID << " tfID : " << (int)tfSD.tfID << std::endl;
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, tfSD);
    }

    void VFVServer::sendVolumetricMaskDataset(VFVClientSocket* client, std::shared_ptr<uint8_t> sharedVolData, size_t size)
    {
        INFO << "Sending volumetric mask data. Data size: " << size << std::endl;
        SocketMessage<int> sm(client->socket, sharedVolData, size);
        writeMessage(sm);
    }

    void VFVServer::sendDrawableAnnotationPositionStatus(VFVClientSocket* client, const SubDatasetMetaData& sdMT, const DrawableAnnotationPositionMetaData& drawableMT)
    {
        VFVSetDrawableAnnotationPositionDefaultColor color;
        color.datasetID    = sdMT.datasetID;
        color.subDatasetID = sdMT.sdID;
        color.drawableID   = drawableMT.drawableID;
        color.color        = (std::min((uint32_t)(drawableMT.drawable->getColor()[3]*255.0f), 255U) << 24) + 
                             (std::min((uint32_t)(drawableMT.drawable->getColor()[0]*255.0f), 255U) << 16) + 
                             (std::min((uint32_t)(drawableMT.drawable->getColor()[1]*255.0f), 255U) << 8) + 
                             (std::min((uint32_t)(drawableMT.drawable->getColor()[2]*255.0f), 255U));
        sendSetDrawableAnnotationPositionColor(client, color);

        VFVSetDrawableAnnotationPositionMappedIdx idx;
        idx.datasetID    = sdMT.datasetID;
        idx.subDatasetID = sdMT.sdID;
        idx.drawableID   = drawableMT.drawableID;
        idx.idx          = drawableMT.drawable->getMappedDataIndices();
        sendSetDrawableAnnotationPositionIdx(client, idx);
    }

    void VFVServer::onLoginSendCurrentStatus(VFVClientSocket* client)
    {
        //Send binding information
        sendHeadsetBindingInfo(client);

        //Send common data
        for(auto& it : m_logData)
        {
            VFVOpenLogData logData;
            logData.fileName  = it.second.name;
            logData.hasHeader = (it.second.logData->getHeaders().size() != 0);
            logData.timeID    = it.second.logData->getTimeInd();
            sendAddLogData(client, logData, it.second.logID);

            for(auto& posIT : it.second.positions)
            {
                sendAddAnnotationPositionData(client, posIT);
                sendSetAnnotationPositionIndexes(client, posIT);
            }
        }

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

        for(auto& it : m_cloudPointDatasets)
        {
            VFVCloudPointDatasetInformation dataset;
            dataset.name = it.second.name;
            sendAddCloudPointDatasetEvent(client, dataset, it.first);
        }

        for(auto& it : m_datasets)
        {
            for(uint32_t i = 0; i < it.second->getNbSubDatasets(); i++)
            {
                SubDataset* sd = it.second->getSubDatasets()[i];

                sendAddSubDataset(client, sd);
                for(uint32_t j = 0; j < sd->getAnnotationCanvas().size(); j++)
                {
                    VFVAnchorAnnotation anchorAnnot;
                    anchorAnnot.datasetID    = it.first;
                    anchorAnnot.subDatasetID = sd->getID();
                    anchorAnnot.annotationID = j;
                    auto annotIT = sd->getAnnotationCanvas().begin();
                    std::advance(annotIT, j);
                    for(uint32_t k = 0; k < 3; k++)
                        anchorAnnot.localPos[k] = (*annotIT)->getPosition()[k];
                    
                    sendAnchorAnnotation(client, anchorAnnot);
                }

                SubDatasetMetaData* sdMT = nullptr;
                getMetaData(it.first, sd->getID(), &sdMT);
                
                if(sdMT)
                {
                    for(auto& pos : sdMT->annotPos)
                    {
                        sendAddAnnotationPositionToSD(client, *sdMT, *(pos.get()));
                        sendDrawableAnnotationPositionStatus(client, *sdMT, *(pos.get()));
                    }
                }
                else
                {
                    ERROR << "Error... a subdataset, that should have been registered, is not...\n";
                    continue;
                }
            }

            sendDatasetStatus(client, it.second, it.first);
        }

        for(auto& it : m_sdGroups)
        {
            if(it.second.isSubjectiveView())
            {
                //Send the group
                SubDatasetSubjectiveStackedLinkedGroup* svGroup = (SubDatasetSubjectiveStackedLinkedGroup*)it.second.sdGroup.get();
                SubDataset* sdBase = svGroup->getBase();
                uint32_t datasetBaseID = getDatasetID(sdBase->getParent());

                VFVAddSubjectiveViewGroup addSVG;
                addSVG.svType        = it.second.type;
                addSVG.sdgID         = it.second.sdgID;
                addSVG.baseDatasetID = datasetBaseID;
                addSVG.baseSDID      = sdBase->getID();
                sendAddSubjectiveViewGroup(client, addSVG);

                //Send global parameters
                VFVSetSVStackedGroupGlobalParameters globalParams;
                globalParams.sdgID       = it.second.sdgID;
                globalParams.stackMethod = (uint32_t)svGroup->getStackingMethod();
                globalParams.merged      = svGroup->getMerge();
                globalParams.gap         = svGroup->getGap();
                sendSVStackedGroupGlobalParameters(client, globalParams);


                //Send each subjective views
                for(auto& p : svGroup->getLinkedSubDatasets())
                {
                    uint32_t stacked = (uint32_t)-1;
                    uint32_t linked  = (uint32_t)-1;

                    if(p.first)
                        stacked = p.first->getID();
                    if(p.second)
                        linked  = p.second->getID();

                    sendAddSubDatasetToSVStackedGroup(client, it.second, datasetBaseID, stacked, linked);
                }
            }
        }

        //Send anchoring data
        if(client->isHeadset())
            sendAnchoring(client);
    }

    void VFVServer::sendSubDatasetStatus(VFVClientSocket* client, SubDataset* sd, uint32_t datasetID)
    {
        sendSubDatasetPositionStatus(client, sd, datasetID);

        //Send transfer Function
        SubDatasetMetaData* sdMT = NULL;
        if(getMetaData(datasetID, sd->getID(), &sdMT) == NULL || sdMT == NULL)
        {
            ERROR << "Could not fetch SubDatasetMetaData of " << datasetID << ':' << sd->getID() << std::endl;
            return;
        }
        sendTransferFunctionDataset(client, generateTFMessage(datasetID, sd, sdMT->tf));

        //Send map visibility
        VFVToggleMapVisibility map;
        map.datasetID = datasetID;
        map.subDatasetID = sd->getID();
        map.visibility = sdMT->mapVisibility;
        sendToggleMapVisibility(client, map);

        //The volumetric mask
        size_t dataSize;
        auto data = generateVolumetricMaskEvent(sd, datasetID, &dataSize);
        sendVolumetricMaskDataset(client, data, dataSize);

        //The clipping plane
        VFVSetSubDatasetClipping clipping;
        clipping.datasetID = datasetID;
        clipping.subDatasetID = sd->getID();
        clipping.minDepthClipping = sd->getMinDepthClipping();
        clipping.maxDepthClipping = sd->getMaxDepthClipping();
        sendSubDatasetClippingEvent(client, clipping);

        sendSubDatasetOwner(client, sdMT);
    }

    void VFVServer::sendSubDatasetPositionStatus(VFVClientSocket* client, SubDataset* sd, uint32_t datasetID)
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
        if(!m_anchorData.isCompleted())
        {
            WARNING << "Cannot send the anchoring data: the anchoring data is not complete yet\n";
            return;
        }
        if(!client->isHeadset())
        {
            WARNING << "Cannot send the anchoring data. The client is not an headset\n";
            return;
        }
        if(client->getHeadsetData().anchoringSent)
        {
            WARNING << "Cannot send the anchoring data. The client already has the anchoring data sent\n";
            return;
        }

        INFO << "Sending Anchoring to an headset\n";

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

            VFVDefaultByteArray byteArr;
            byteArr.type = VFV_SEND_HEADSET_ANCHOR_SEGMENT;
            byteArr.dataSize = itSegment.dataSize;
            saveMessageSentToJSONLog(client, byteArr);
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

    void VFVServer::sendSubDatasetOwnerToAll(SubDatasetMetaData* metaData)
    {
        //Send the data
        for(auto it : m_clientTable)
            sendSubDatasetOwner(it.second, metaData);
    }

    void VFVServer::sendSubDatasetOwner(VFVClientSocket* client, SubDatasetMetaData* metaData)
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

        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "SubDatasetOwner");
            m_log << ",    \"datasetID\"  : " << metaData->datasetID << ",\n"
                  << "    \"subDatasetID\" : " << metaData->sdID << ",\n"
                  << "    \"headsetID\" : " << id << "\n"
                  << "},\n";
            m_log << std::flush;
        }
#endif
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

        INFO << "Sending start annotation \n";
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, startAnnot);
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

        INFO << "Sending anchor annotation " << anchorAnnot.localPos[0] << "x" << anchorAnnot.localPos[1] << "x" << anchorAnnot.localPos[2] << "\n";
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
        saveMessageSentToJSONLog(client, anchorAnnot);
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
        saveMessageSentToJSONLog(client, clearAnnot);
    }

    void VFVServer::sendLocationTablet(const glm::vec3& pos, const Quaternionf& rot, VFVClientSocket* client)
    {
        //Generate the data
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(float) + 4*sizeof(float);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset = 0;

        //Message ID
        writeUint16(data, VFV_SEND_LOCATION);
        offset += sizeof(uint16_t);

        //Position
        writeFloat(data+offset, pos.x);
        offset += sizeof(float);
        writeFloat(data+offset, pos.y);
        offset += sizeof(float);
        writeFloat(data+offset, pos.z);
        offset += sizeof(float);

        //Rotation
        writeFloat(data+offset, rot.w);
        offset += sizeof(float);
        writeFloat(data+offset, rot.x);
        offset += sizeof(float);
        writeFloat(data+offset, rot.y);
        offset += sizeof(float);
        writeFloat(data+offset, rot.z);
        offset += sizeof(float);

        //INFO << "Sending tablet location: "
        //     << "Tablet position: " << pos.x << " " << pos.y << " " << pos.z << "; "
        //     << "Tablet rotation: " << rot.w << " " << rot.x << " " << rot.y << " " << rot.z << std::endl;
        
        std::shared_ptr<uint8_t> sharedData(data, free);
        
        //Send the data
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> lockJson(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "LocationTablet");
            m_log << ",    \"position\" : [" << pos[0] << ", " << pos[1] << ", " << pos[2] << "],\n"
                  << "    \"rotation\" : [" << rot[0] << ", " << rot[1] << ", " << rot[2] << ", " << rot[3] << "]\n";
            VFV_END_TO_JSON(m_log);
            m_log << ",\n";
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendAddNewSelectionInput(VFVClientSocket* client, const VFVAddNewSelectionInput& addInput)
    {
        uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset = 0;

        //Message ID
        writeUint16(data, VFV_SEND_ADD_NEW_SELECTION_INPUT);
        offset += sizeof(uint16_t);

        //Boolean operation in use
        writeUint32(data+offset, addInput.booleanOp);
        offset += sizeof(uint32_t);

        std::shared_ptr<uint8_t> sharedData(data, free);
        
        //Send the data
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
        saveMessageSentToJSONLog(client, addInput);
    }

    void VFVServer::sendToggleMapVisibility(VFVClientSocket* client, const VFVToggleMapVisibility& visibility)
    {
        uint32_t dataSize = sizeof(uint16_t) + 2*sizeof(uint32_t) + sizeof(uint8_t);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset = 0;

        //Message ID
        writeUint16(data, VFV_SEND_TOGGLE_MAP_VISIBILITY);
        offset += sizeof(uint16_t);

        //Dataset ID
        writeUint32(data+offset, visibility.datasetID);
        offset += sizeof(uint32_t);

        //SubDataset ID
        writeUint32(data+offset, visibility.subDatasetID);
        offset += sizeof(uint32_t);

        //Visibility status
        data[offset] = visibility.visibility ? 1 : 0;
        offset++;

        std::shared_ptr<uint8_t> sharedData(data, free);
        
        //Send the data
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
        
        saveMessageSentToJSONLog(client, visibility);
    }


    void VFVServer::sendResetVolumetricSelection(VFVClientSocket* client, int datasetID, int sdID, int headsetID)
    {
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_RESET_VOLUMETRIC_SELECTION);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, sdID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, headsetID);
        offset += sizeof(uint32_t);

        //Send the data
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);
        
#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> lockJson(m_logMutex);
            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "SendResetVolumetricSelection");
            m_log << ",    \"datasetID\" : " << datasetID << ",\n"
                  << "    \"subDatasetID\" : " << sdID << ",\n"
                  << "    \"headsetID\" : " << headsetID << "\n";
            VFV_END_TO_JSON(m_log);
            m_log << ",\n" << std::flush;
        }
#endif
    }

    void VFVServer::sendSetDrawableAnnotationPositionColor(VFVClientSocket* client, const VFVSetDrawableAnnotationPositionDefaultColor& color)
    {
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + sizeof(uint32_t);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_SET_DRAWABLE_ANNOTATION_POSITION_DEFAULT_COLOR);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, color.datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, color.subDatasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, color.drawableID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, color.color);
        offset += sizeof(uint32_t);

        INFO << "Set color..." << std::endl;

        //Send the data
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, color);
    }

    void VFVServer::sendSetDrawableAnnotationPositionIdx(VFVClientSocket* client, const VFVSetDrawableAnnotationPositionMappedIdx& idx)
    {

        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + (1+idx.idx.size())*sizeof(uint32_t);
        uint8_t* data = (uint8_t*)malloc(dataSize);
        uint32_t offset = 0;

        writeUint16(data, VFV_SEND_SET_DRAWABLE_ANNOTATION_POSITION_MAPPED_IDX);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, idx.datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, idx.subDatasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, idx.drawableID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, idx.idx.size());
        offset += sizeof(uint32_t);

        for(uint32_t i = 0; i < idx.idx.size(); i++, offset += sizeof(uint32_t))
            writeUint32(data+offset, idx.idx[i]);

        //Send the data
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, idx);
    }

    void VFVServer::sendAddSubjectiveViewGroup(VFVClientSocket* client, const VFVAddSubjectiveViewGroup& addSV)
    {
        uint32_t dataSize = sizeof(uint16_t) + sizeof(uint8_t) + 3*sizeof(uint32_t);
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_ADD_SUBJECTIVE_VIEW_GROUP);
        offset += sizeof(uint16_t);

        data[offset] = addSV.svType;
        offset++;

        writeUint32(data+offset, addSV.baseDatasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, addSV.baseSDID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, addSV.sdgID);
        offset += sizeof(uint32_t);

        //Send the data
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, addSV);
    }

    void VFVServer::sendAddSubDatasetToSVStackedGroup(VFVClientSocket* client, SubDatasetGroupMetaData& sdgMD, uint32_t datasetID, uint32_t sdStackedID, uint32_t sdLinkedID)
    {
        uint32_t dataSize = sizeof(uint16_t) + 4*sizeof(uint32_t);
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_ADD_SD_TO_SV_STACKED_LINKED_GROUP);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, sdgMD.sdgID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, sdStackedID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, sdLinkedID);
        offset += sizeof(uint32_t);

        //Send the data
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        {
            std::lock_guard<std::mutex> lockJson(m_logMutex);

            VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "AddSubDatasetToSVStackedGroup");
            m_log << ",    \"sdgID\" : " << sdgMD.sdgID << ",\n"
                  << "    \"datasetID\" : " << datasetID << ",\n"
                  << "    \"sdStackedID\" : " << sdStackedID << ",\n"
                  << "    \"sdLinkedID\" : " << sdLinkedID << "\n";
            m_log << std::flush;
        }
#endif
    }

    void VFVServer::sendSVStackedGroupGlobalParameters(VFVClientSocket* client, const VFVSetSVStackedGroupGlobalParameters& params)
    {
        uint32_t dataSize = sizeof(uint16_t) + 2*sizeof(uint32_t) + sizeof(float) + 1;
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_SET_SV_STACKED_GLOBAL_PARAMETERS);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, params.sdgID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, params.stackMethod);
        offset += sizeof(uint32_t);

        writeFloat(data+offset, params.gap);
        offset += sizeof(float);

        data[offset] = params.merged;
        offset++;

        //Send the data
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, params);
    }

    void VFVServer::sendRemoveSubDatasetsGroup(VFVClientSocket* client, const VFVRemoveSubDatasetGroup& removeSDGroup)
    {
        uint32_t dataSize = sizeof(uint16_t) + 1*sizeof(uint32_t);
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_REMOVE_SUBDATASET_GROUP);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, removeSDGroup.sdgID);
        offset += sizeof(uint32_t);

        //Send the data
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, removeSDGroup);
    }

    void VFVServer::sendRenameSubDataset(VFVClientSocket* client, const VFVRenameSubDataset& rename)
    {
        uint32_t dataSize = sizeof(uint16_t) + 3*sizeof(uint32_t) + rename.name.size();
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_RENAME_SD);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, rename.datasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, rename.subDatasetID);
        offset += sizeof(uint32_t);

        writeUint32(data+offset, rename.name.size());
        offset += sizeof(uint32_t);

        memcpy(data+offset, rename.name.c_str(), rename.name.size());
        offset += rename.name.size();

        //Send the data
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

        saveMessageSentToJSONLog(client, rename);
    }

    void VFVServer::sendMessageToDisplay(VFVClientSocket* client, const std::string& msg)
    {
        uint32_t dataSize = sizeof(uint16_t) + sizeof(uint32_t) + msg.size();
        uint8_t* data     = (uint8_t*)malloc(dataSize);
        uint32_t offset   = 0;

        writeUint16(data, VFV_SEND_DISPLAY_SHORT_MESSAGE);
        offset += sizeof(uint16_t);

        writeUint32(data+offset, msg.size());
        offset += sizeof(uint32_t);

        memcpy(data+offset, msg.c_str(), msg.size());
        offset += msg.size();

        //Send the data
        std::shared_ptr<uint8_t> sharedData(data, free);
        SocketMessage<int> sm(client->socket, sharedData, offset);
        writeMessage(sm);

#ifdef VFV_LOG_DATA
        std::lock_guard<std::mutex> logLock(m_logMutex);
        VFV_BEGINING_TO_JSON(m_log, VFV_SENDER_SERVER, getHeadsetIPAddr(client), getTimeOffset(), "DisplayShortMessage");
        m_log << ",    \"message\" : \"" << msg << "\"\n"
              << "},\n";
        m_log << std::flush;
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
                bool isTablet  = client->isTablet();
                bool isHeadset = client->isHeadset();
                std::lock_guard<std::mutex> logLock(m_logMutex);
                std::string str = msg.curMsg->toJson(isTablet ? VFV_SENDER_TABLET : (isHeadset ? VFV_SENDER_HEADSET : VFV_SENDER_UNKNOWN), getHeadsetIPAddr(client), getTimeOffset());

                if(str.size())
                {
                    m_log << str << ",\n";
                    m_log << std::flush;
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
                    std::lock_guard<std::mutex> lock(m_mapMutex);
                    VFVClientSocket* headset = getHeadsetFromClient(client);
                    if(!headset)
                        break;

                    //Set the current action
                    if(headset != client)
                    {
                        headset->getHeadsetData().setCurrentAction((VFVHeadsetCurrentActionType)msg.headsetCurrentAction.action);
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

                case LOCATION:
                {
                    onLocation(client, msg.location);
                    break;
                }

                case TABLETSCALE:
                {
                    onTabletScale(client, msg.tabletScale);
                    break;
                }

                case LASSO:
                {
                    onLasso(client, msg.lasso);
                    break;
                }

                case CONFIRM_SELECTION:
                {
                    onConfirmSelection(client, msg.confirmSelection);
                    break;
                }

                case ADD_CLOUD_POINT_DATASET:
                {
                    addCloudPointDataset(client, msg.cloudPointDataset);
                    break;
                }

                case ADD_NEW_SELECTION_INPUT:
                {
                    onAddNewSelectionInput(client, msg.addNewSelectionInput);
                    break;
                }

                case TOGGLE_MAP_VISIBILITY:
                {
                    onToggleMapVisibility(client, msg.toggleMapVisibility);
                    break;
                }

                case MERGE_SUBDATASETS:
                {
                    onMergeSubDatasets(client, msg.mergeSubDatasets);
                    break;
                }

                case RESET_VOLUMETRIC_SELECTION:
                {
                    onResetVolumetricSelection(client, msg.resetVolumetricSelection);
                    break;
                }

                case ADD_LOG_DATA:
                {
                    addLogData(client, msg.addLogData);
                    break;
                }
                
                case ADD_ANNOTATION_POSITION:
                {
                    addAnnotationPosition(client, msg.addAnnotPos);
                    break;
                }

                case SET_ANNOTATION_POSITION_INDEXES:
                {
                    onSetAnnotationPositionIndexes(client, msg.setAnnotPosIndexes);
                    break;
                }

                case ADD_ANNOTATION_POSITION_TO_SD:
                {
                    addAnnotationPositionToSD(client, msg.addAnnotPosToSD);
                    break;
                }

                case SET_SUBDATASET_CLIPPING:
                {
                    setSubDatasetClipping(client, msg.setSDClipping);
                    break;
                }

                case SET_DRAWABLE_ANNOTATION_POSITION_COLOR:
                {
                    setDrawableAnnotationPositionColor(client, msg.setDrawableAnnotPosColor);
                    break;
                }

                case SET_DRAWABLE_ANNOTATION_POSITION_IDX:
                {
                    setDrawableAnnotationPositionIdx(client, msg.setDrawableAnnotPosIdx);
                    break;
                }

                case ADD_SV_GROUP:
                {
                    addSubjectiveViewGroup(client, msg.addSVGroup);
                    break;
                }

                case SET_SV_STACKED_GROUP_GLOBAL_PARAMETERS:
                {
                    setSubjectiveViewStackedParameters(client, msg.setSVStackedGroupParams);
                    break;
                }

                case REMOVE_SD_GROUP:
                {
                    onRemoveSubDatasetGroup(client, msg.removeSDGroup);
                    break;
                }

                case ADD_CLIENT_TO_SV_GROUP:
                {
                    addClientToSVGroup(client, msg.addClientToSVGroup);
                    break;
                }

                case RENAME_SUBDATASET:
                {
                    onRenameSubDataset(client, msg.renameSD);
                    break;
                }

                case SAVE_SUBDATASET_VISUAL:
                {
                    onSaveSubDatasetVisual(client, msg.saveSDVisual);
                    break;
                }

                case VOLUMETRIC_SELECTION_METHOD:
                {
                    onSetVolumetricSelectionMethod(client, msg.volumetricSelectionMethod);
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

                auto f = [this, endTime](DatasetMetaData& mt)
                {
                    for(auto& it2 : mt.sdMetaData)
                    {
                        if(it2.hmdClient != NULL && endTime - it2.lastModification >= MAX_OWNER_TIME)
                        {
                            it2.hmdClient = NULL;
                            it2.lastModification = 0;
                            sendSubDatasetLockOwner(&it2);
                        }
                    }
                };

                for(auto& it : m_vtkDatasets)
                    f(it.second);

                for(auto& it : m_binaryDatasets)
                    f(it.second);

                for(auto& it : m_cloudPointDatasets)
                    f(it.second);
            }

            //Sleep
            clock_gettime(CLOCK_REALTIME, &end);
            endTime = end.tv_nsec*1.e-3 + end.tv_sec*1.e6;
            usleep(std::max(0.0, 1.e6/UPDATE_THREAD_FRAMERATE - endTime + (beg.tv_nsec*1.e-3 + beg.tv_sec*1.e6)));
        }
    }

    void VFVServer::pushHeavy(const std::function<void(void)>& f)
    {
        std::unique_lock<std::mutex> lock(m_computeTasksMutex);
        std::condition_variable cv;

        while(cv.wait_for(lock, std::chrono::microseconds(1), [&](){return m_computeTasks.size() < 10;}));
        m_computeTasks.push(f);
    }

    void VFVServer::computeThread()
    {
        while(!m_closeThread)
        {
            std::unique_lock<std::mutex> lock(m_computeMutex);
            m_computeCond.wait(lock, [&]() {return m_closeThread || !m_computeTasks.empty();});

            m_computeTasksMutex.lock();
            if(m_computeTasks.empty())
            {
                m_computeTasksMutex.unlock();
                continue;
            }

            auto f = m_computeTasks.front();
            m_computeTasks.pop();
            m_computeTasksMutex.unlock();

            f();
        }
    }
}
