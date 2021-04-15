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
#include "Datasets/SubDatasetGroup.h"

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

    /** \brief Basic structure for annotation component meta data 
     * @tparam T the type of the actual stored annotation component. */
    template<typename T>
    struct AnnotationComponentMetaData
    {
        uint32_t annotID; /*!< The parent annotation log ID as defined by the server */
        uint32_t compID;  /*!< This component ID INSIDE the parent annotation linked to this component */
        std::shared_ptr<T> component; /*!< The linked component object */
    };

    /** \brief  The LogMetaData structure, containing metadata of AnnotationLogContainer */
    struct LogMetaData
    {
        std::string name;  /*!< The name (e.g., the path) of the Log data*/
        uint32_t    logID; /*!< The associated ID*/
        uint32_t    curPositionID = 0;   /*!< The next AnnotationPosition ID to use when adding an AnnotationPosition object*/
        std::shared_ptr<AnnotationLogContainer> logData; /*!< The actual data*/
        std::list<AnnotationComponentMetaData<AnnotationPosition>> positions; /*!< The list of AnnotationPosition objects */

        /** \brief  Add an AnnotationPosition to this object 
         * \return the AnnotationPositionMetaData object added */
        AnnotationComponentMetaData<AnnotationPosition>& addPosition()
        {
            std::shared_ptr<AnnotationPosition> pos = logData->buildAnnotationPositionView();
            AnnotationComponentMetaData<AnnotationPosition> mt;
            mt.annotID   = logID;
            mt.compID    = curPositionID;
            mt.component = pos;
            curPositionID++;
            positions.push_back(mt);
            return positions.back();
        }

        /** \brief  Get the component meta data based on the type T
         * @tparam T the type to look for. Invalid types results in nullptr. See AnnotationComponentMetaData for valid t parameters
         * \param compID the component ID to search for
         * \return   a pointer to T corresponding to the actual component object (see AnnotationComponentMetaData)*/
        template <typename T>
        AnnotationComponentMetaData<T>* getComponentMetaData(uint32_t compID){return nullptr;}
    };

    template<>
    inline AnnotationComponentMetaData<AnnotationPosition>* LogMetaData::getComponentMetaData<AnnotationPosition>(uint32_t compID)
    {
        auto it = std::find_if(positions.begin(), positions.end(), [compID](AnnotationComponentMetaData<AnnotationPosition>& mt){return mt.compID == compID;});
        if(it != positions.end())
            return &(*it);
        return nullptr;
    }
    
    /** \brief  Drawable annotation component meta data
     * @tparam T the type of the drawable component to store. An AnnotationComponentMetaData of template parameter T::type must exist*/
    template<typename T>
    struct DrawableAnnotationComponentMetaData
    {
        AnnotationComponentMetaData<typename T::type>* compMetaData;
        std::shared_ptr<T>                             drawable;
        uint32_t                                       drawableID;
    };

    struct DrawableAnnotationPositionMetaData : public DrawableAnnotationComponentMetaData<DrawableAnnotationPosition>
    {};

    /** \brief  The different subdataset groups usable */
    enum SubDatasetGroupType
    {
        SD_GROUP_SV_STACKED        = 0,
        SD_GROUP_SV_LINKED         = 1,
        SD_GROUP_SV_STACKED_LINKED = 2,
        SD_GROUP_NONE,
    };

    /** \brief The Metadata of subdataset groups */
    struct SubDatasetGroupMetaData
    {
        SubDatasetGroupType              type    = SD_GROUP_NONE; /*!< The type of the group*/
        std::shared_ptr<SubDatasetGroup> sdGroup = nullptr;       /*!< The group base class. Use "type" to cast this object*/
        uint32_t                         sdgID   = -1;            /*!< The ID of the group*/
        VFVClientSocket*                 owner   = nullptr;       /*!< The client owning this group. nullptr == the server owns it*/

        inline bool isSubjectiveView() const
        {
            return type == SD_GROUP_SV_STACKED ||
                   type == SD_GROUP_SV_LINKED  ||    
                   type == SD_GROUP_SV_STACKED_LINKED;
        }

        inline bool isStackedSubjectiveView() const
        {
            return isSubjectiveView(); //For the moment, every subjective views are stacked subjective views.
        }
    };

    /** \brief  Subdataset meta data */
    struct SubDatasetMetaData
    {
        VFVClientSocket* hmdClient = NULL;                      /*!< The HMD client locking this subdataset for modification.
                                                                     NULL == no hmd client owning this subdataset*/
        VFVClientSocket* owner = NULL;                          /*!< The client owning this SubDataset. No owner == public SubDataset*/
        time_t           lastModification = 0;                  /*!< The last modification time is us this subdataset received. 
                                                                     This is used to automatically reset the owner*/
        bool             visibleToOther = false;                /*!< Even if this subdataset is private, is it visible to others?*/

        std::shared_ptr<SubDatasetTFMetaData> tf;               /*!< The transfer function information*/
        uint64_t sdID      = 0;                                 /*!< SubDataset ID*/
        uint64_t datasetID = 0;                                 /*!< Dataset ID*/
        
        bool             mapVisibility = true;                  /*!< The status about the map visibility*/

        uint32_t         nbStackedTimesteps            = 1;     /*!< The number of stacked timesteps to render*/
        float            stackedTimestepsDepthClipping = false; /*!< The clipping values for timesteps to render in the stacked settings*/
        bool             stackedTimesteps              = false; /*!< Should we stack multiple timesteps?*/

        std::list<std::shared_ptr<DrawableAnnotationPositionMetaData>> annotPos; /*!< The annotation position components linked to this subdataset */
        uint32_t         currentDrawableID = 0;                                  /*!< What should be the next ID of the attached drawable (see DrawableAnnotation*) */

        SubDatasetGroupMetaData sdg;                                 /*!< The SubDatasetGroup linked with this SubDataset*/


        /** \brief Push a new DrawableAnnotationPosition meta data object  
         * \param annot the object to consider and configure. A link is created between the SubDataset and this drawable.  */
        void pushDrawableAnnotationPosition(std::shared_ptr<DrawableAnnotationPositionMetaData> annot)
        {
            annot->drawableID = currentDrawableID++;
            annotPos.push_back(annot);
        }

        template<typename T>
        std::shared_ptr<T> getDrawableAnnotation(uint32_t drawableID) {return nullptr;}

        private:
            template<typename T>
            std::shared_ptr<T> getDrawableAnnotation(uint32_t drawableID, const std::list<std::shared_ptr<T>>& data)
            {
                for(auto& it : data)
                    if(it->drawableID == drawableID)
                        return it;
                return nullptr;
            }
    };

    template<>
    inline std::shared_ptr<DrawableAnnotationPositionMetaData> SubDatasetMetaData::getDrawableAnnotation(uint32_t drawableID)
    { return getDrawableAnnotation(drawableID, annotPos); }

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
}

#endif
