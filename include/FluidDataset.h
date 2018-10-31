#ifndef  FLUIDDATASET_INC
#define  FLUIDDATASET_INC

#include <string>
#include "ColorMode.h"

/* \brief Class representing a fluid dataset */
class FluidDataset
{
    public:
        /* \brief Constructor.
         * \param name the dataset name */
        FluidDataset(const std::string& name);

        /* \brief Set the color of this dataset
         * \param mode the color mode
         * \param min the minimum clamping value (ratio : 0.0, 1.0) 
         * \param max the maximum clamping value (ratio : 0.0, 1.0) */
        void setColor(ColorMode mode, float min, float max);

        /* \brief Set the rotation of the dataset
         * \param rotation the rotation quaternion (i, j, k, w) */
        void setRotation(const float* rotation);

        /* \brief Get the color mode
         * \return the color mode */
        ColorMode getColorMode() {return m_colorMode;}

        /* \brief Get the minimum clamping value
         * \return the minimum clamping value */
        float getMinColor()      {return m_colorMin;}

        /* \brief Get the maximum clamping value
         * \return the maximum clamping value */
        float getMaxColor()      {return m_colorMax;}

        /* \brief Get the rotation quaternion of this dataset (four values)
         * \return the rotation quaternion (i, j, k, w) */
        const float* getRotation() {return m_rotation;}
    private:
        std::string m_name;        /*!< Name of the dataset*/
        ColorMode   m_colorMode;   /*!< Color mode of the dataset*/
        float       m_colorMin;    /*!< Min color clamp of this dataset (ration : 0.0, 1.0)*/
        float       m_colorMax;    /*!< Max color clamp of this dataset (ration : 0.0, 1.0)*/
        float       m_rotation[4]; /*!< The quaternion value i, j, k, w*/
};

#endif
