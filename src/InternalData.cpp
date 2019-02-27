#include "InternalData.h"

namespace sereno
{
    std::unique_ptr<InternalData> InternalData::singleton;

    InternalData::InternalData()
    {}

    void InternalData::initSingleton()
    {
        InternalData::singleton.reset(new InternalData());
    }

    InternalData* InternalData::getSingleton()
    {
        return InternalData::singleton.get();
    }
}
