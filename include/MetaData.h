#ifndef  METADATA_INC
#define  METADATA_INC

#include <string>
#include "Datasets/VTKDataset.h"

namespace sereno
{
    /*!< Structure representing MetaData associated with the opened Datasets*/
    struct MetaData
    {
        std::string name;      /*!< The MetaData's name*/
        uint32_t    datasetID; /* < The MetaData's ID*/
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
