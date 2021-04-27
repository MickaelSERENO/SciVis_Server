#include "MetaData.h"

namespace sereno
{
    MergeTFMetaData& MergeTFMetaData::operator=(const MergeTFMetaData& cpy)
    {
        if(this != &cpy)
        {
            if(cpy.tf1 != nullptr)
                tf1 = std::shared_ptr<SubDatasetTFMetaData>(new SubDatasetTFMetaData(*cpy.tf1.get()));
            if(cpy.tf2 != nullptr)
                tf2 = std::shared_ptr<SubDatasetTFMetaData>(new SubDatasetTFMetaData(*cpy.tf2.get()));
        }

        return *this;
    }
}
