#ifndef  METADATA_INC
#define  METADATA_INC

#include <string>
#include "Datasets/VTKDataset.h"
#include "VFVClientSocket.h"
#include "TransferFunction/TFType.h"
#include "TransferFunction/TransferFunction.h"
#include "Datasets/CloudPointDataset.h"

namespace sereno
{
    /** \brief  Subdataset meta data */
    struct SubDatasetMetaData
    {
        VFVClientSocket* hmdClient = NULL;            /*!< The HMD client locking this subdataset for modification.
                                                           NULL == no hmd client owning this subdataset*/

        VFVClientSocket* owner = NULL;         /*!< The client owning this SubDataset. No owner == public SubDataset*/
        time_t           lastModification = 0; /*!< The last modification time is us this subdataset received. 
                                                    This is used to automatically reset the owner*/
        uint64_t         sdID      = 0;        /*!< SubDataset ID*/
        uint64_t         datasetID = 0;        /*!< Dataset ID*/
        TFType           tfType    = TF_NONE;  /*!< The Transfer Function type in use*/
        std::shared_ptr<TF> tf     = NULL;     /*!< The Transfer Function being used*/

        bool             mapVisibility = true; /*!< The tatus about the map visibility*/
    };

    /*!< Structure representing MetaData associated with the opened Datasets*/
    struct MetaData
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
    struct VTKMetaData : public MetaData
    {
        VTKDataset* dataset; /*!< The dataset opened*/
        std::vector<uint32_t> ptFieldValueIndices;   /*!< the pt field values to take account of*/
        std::vector<uint32_t> cellFieldValueIndices; /*!< the cell field values to take account of*/
    };

    /** \brief  The VectorField MetaData structure, containing metadata of VectorField Datasets */
    struct VectorFieldMetaData : public MetaData
    {};

    struct CloudPointMetaData : public MetaData
    {
        CloudPointDataset* dataset; /*!< The dataset opened*/
    };
}

#endif
