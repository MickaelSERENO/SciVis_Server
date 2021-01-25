#ifndef  METADATA_INC
#define  METADATA_INC

#include <string>
#include "Datasets/VTKDataset.h"
#include "Datasets/VectorFieldDataset.h"
#include "VFVClientSocket.h"
#include "TransferFunction/TFType.h"
#include "TransferFunction/TransferFunction.h"
#include "Datasets/CloudPointDataset.h"
#include "Datasets/Annotation/AnnotationLogContainer.h"

namespace sereno
{
    class SubDatasetTFMetaData;

    /** \brief  The Meta data of the Merge transfer function */
    struct MergeTFMetaData
    {
        std::shared_ptr<SubDatasetTFMetaData> tf1; /*!< The meta data of the first transfer function being merged*/
        std::shared_ptr<SubDatasetTFMetaData> tf2; /*!< The meta data of the second transfer function being merged*/
    }; 

    class SubDatasetTFMetaData
    {
        public:
            SubDatasetTFMetaData(TFType tfType = TF_NONE, std::shared_ptr<TF> tf = nullptr)
            {
                setType(tfType);
                setTF(tf);
            }

            SubDatasetTFMetaData(const SubDatasetTFMetaData& cpy)
            {
                *this = cpy;
            }

            SubDatasetTFMetaData& operator=(const SubDatasetTFMetaData& cpy)
            {
                if(this != &cpy)
                {
                    setType(cpy.m_tfType);
                    m_tf = cpy.m_tf;
                    switch(cpy.m_tfType)
                    {
                        case TF_MERGE:
                            m_mergeMD = cpy.m_mergeMD;
                            break;
                        default:
                            break;
                    }
                }

                return *this;
            }

            ~SubDatasetTFMetaData()
            {
                setType(TF_NONE);
            }

            void setType(TFType type)
            {
                if(m_tfType != type)
                {
                    switch(m_tfType)
                    {
                        case TF_MERGE:
                            m_mergeMD.~MergeTFMetaData();
                            break;
                        default:
                            break;
                    }

                    m_tfType = type;

                    switch(type)
                    {
                        case TF_MERGE:
                            new(&m_mergeMD) MergeTFMetaData();
                            break;
                        default:
                            break;
                    }
                }
            }

            /** \brief  Get the type of the held transfer function
             * \return  The type of the transfer function being stored */
            TFType getType() const {return m_tfType;}

            /** \brief  Get a pointer to the stored transfer function
             * \return  The transfer function */
            std::shared_ptr<TF> getTF() const {return m_tf;}
            
            /** \brief  Set the current transfer function
             * \param tf the current transfer function */
            void setTF(std::shared_ptr<TF> tf) {m_tf = tf;}

            /** \brief  Get the MetaData of the MergeTF object. Call this method only if getType == TF_MERGE
             * \return  The MergeMetaData being stored */
            const MergeTFMetaData& getMergeTFMetaData() const {return m_mergeMD;}

            /** \brief  Get the MetaData of the MergeTF object. Call this method only if getType == TF_MERGE
             * \return  The MergeMetaData being stored */
            MergeTFMetaData& getMergeTFMetaData() {return m_mergeMD;}
        private:
            TFType              m_tfType = TF_NONE;  /*!< The Transfer Function type in use*/
            std::shared_ptr<TF> m_tf     = NULL;     /*!< The Transfer Function being used*/
            union
            {
                MergeTFMetaData m_mergeMD;
            }; /*!< The transfer function meta data*/
    };

    /** \brief  Subdataset meta data */
    struct SubDatasetMetaData
    {
        VFVClientSocket* hmdClient = NULL;            /*!< The HMD client locking this subdataset for modification.
                                                           NULL == no hmd client owning this subdataset*/

        VFVClientSocket* owner = NULL;         /*!< The client owning this SubDataset. No owner == public SubDataset*/
        time_t           lastModification = 0; /*!< The last modification time is us this subdataset received. 
                                                    This is used to automatically reset the owner*/
        std::shared_ptr<SubDatasetTFMetaData> tf; /*!< The transfer function information*/
        uint64_t         sdID      = 0;        /*!< SubDataset ID*/
        uint64_t         datasetID = 0;        /*!< Dataset ID*/
        bool             mapVisibility = true; /*!< The tatus about the map visibility*/
    };

    /*!< Structure representing MetaData associated with the opened Datasets*/
    struct DatasetMetaData
    {
        std::string name;      /*!< The MetaData's name*/
        uint32_t    datasetID; /*!< The MetaData's ID*/

        std::vector<SubDatasetMetaData> sdMetaData; /*!< SubDataset meta data*/

        /** \brief  Get the SubDatasetMetaData based on the sdID
         * \param sdID the SubDataset ID
         * \return  The SubDataset at the corresponding ID, NULL otherwise */
        SubDatasetMetaData* getSDMetaDataByID(uint32_t sdID)
        {
            for(SubDatasetMetaData& sd : sdMetaData)
                if(sd.sdID == sdID)
                    return &sd;
            return NULL;
        }
    };

    /** \brief  The VTK MetaData structure, containing metadata of VTK Datasets */
    struct VTKMetaData : public DatasetMetaData
    {
        VTKDataset* dataset; /*!< The dataset opened*/
        std::vector<uint32_t> ptFieldValueIndices;   /*!< the pt field values to take account of*/
        std::vector<uint32_t> cellFieldValueIndices; /*!< the cell field values to take account of*/
    };

    /** \brief  The VectorField MetaData structure, containing metadata of VectorField Datasets */
    struct VectorFieldMetaData : public DatasetMetaData
    {
        VectorFieldDataset* dataset; /*!< The dataset opened*/
    };

    /** \brief  The CloudPointer MetaData structure, containing metadata of CloudPoints Datasets */
    struct CloudPointMetaData : public DatasetMetaData
    {
        CloudPointDataset* dataset; /*!< The dataset opened*/
    };

    /** \brief  The LogMetaData structure, containing metadata of AnnotationLogContainer */
    struct LogMetaData
    {
        std::string name;  /*!< The name (e.g., the path) of the Log data*/
        uint32_t    logID; /*!< The associated ID*/
        AnnotationLogContainer* logData; /*!< The actual data*/
    };
}

#endif
