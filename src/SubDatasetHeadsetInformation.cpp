#include "SubDatasetHeadsetInformation.h"

namespace sereno
{
    SubDatasetHeadsetInformation::SubDatasetHeadsetInformation(SubDataset* publicSD) : m_public(publicSD), m_private(*publicSD)
    {}

    SubDatasetHeadsetInformation::~SubDatasetHeadsetInformation()
    {}
}
