#include "FluidDataset.h"

FluidDataset::FluidDataset(const std::string& name) : m_name(name), 
                                                      m_colorMode(RAINBOW), m_colorMin(0.0f), m_colorMax(1.0),
                                                      m_rotation{0.0, 0.0, 0.0, 1.0}
{}

void FluidDataset::setColor(ColorMode mode, float min, float max)
{
    m_colorMode = mode;
    m_colorMin  = min;
    m_colorMax  = max;
}

void FluidDataset::setRotation(const float* rotation)
{
    for(uint32_t i = 0; i < 4; i++)
        m_rotation[i] = rotation[i];
}
