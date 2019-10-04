#ifndef  METADATA_INC
#define  METADATA_INC

#include <string>
#include "Datasets/VTKDataset.h"
#include "VFVClientSocket.h"

namespace sereno
{
    struct SubDatasetMetaData
    {
        VFVClientSocket* hmdClient;            /*!< The HMD client owning this subdataset. 
                                                    NULL == no hmd client owning this subdataset*/
        time_t           lastModification = 0; /*!< The last modification time is us this subdataset received. 
                                                    This is used to automatically reset the owner*/
        uint64_t         sdID      = 0;        /*!< SubDataset ID*/
        uint64_t         datasetID = 0;        /*!< Dataset ID*/
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

    /** \brief  The Binary MetaData structure, containing metadata of Binary Datasets */
    struct BinaryMetaData : public MetaData
    {};
}

#endif
