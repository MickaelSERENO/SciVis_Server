#ifndef  VFVDATAINFORMATION_INC
#define  VFVDATAINFORMATION_INC

#include "utils.h"
#include <string>
#include <cstdint>

#define VFV_DATA_ERROR \
    ERROR << "The parameter value as not the correct type for cursor = " << cursor << std::endl;\
    return false;

/* \brief Structure containing the Data information for the VFV Application */
struct VFVDataInformation
{
    virtual ~VFVDataInformation(){}

    /* \brief Get the type of the information at "cursor"
     * \param cursor the position of the information currently being read
     * \return 's' for string, 'i' for uint16_t, 'I' for uint32_t and 'f' for float. 0 is returned if nothing should be at this position */
    virtual char getTypeAt(uint32_t cursor) const = 0;

    /* \brief Get the maximum cursor position of the data being read
     * \return the maximum information ID */
    virtual uint32_t getMaxCursor() const = 0;

    /* \brief Push a value into the data
     * \param cursor the information cursor
     * \param value the value to push
     * \return true if success, false otherwise */
    virtual bool pushValue(uint32_t cursor, const std::string& value) {return false;}
    virtual bool pushValue(uint32_t cursor, float value)              {return false;}
    virtual bool pushValue(uint32_t cursor, double value)             {return pushValue(cursor, (float)value);}
    virtual bool pushValue(uint32_t cursor, int16_t value)            {return pushValue(cursor, (uint16_t)value);}
    virtual bool pushValue(uint32_t cursor, uint16_t value)           {return pushValue(cursor, (uint32_t)value);}
    virtual bool pushValue(uint32_t cursor, int32_t value)            {return pushValue(cursor, (uint32_t)value);}
    virtual bool pushValue(uint32_t cursor, uint32_t value)           {return false;}
};

/* \brief Structure containing information about the rotation of the tablet */
struct VFVRotationInformation : public VFVDataInformation
{
    std::string dataset; /*!< The dataset name*/
    float quaternion[4]; /*!< The quaternion information*/

    char getTypeAt(uint32_t cursor) const
    {
        if(cursor == 0)
            return 's';
        else if(cursor < 5)
            return 'f';
        return 0;
    }

    bool pushValue(uint32_t cursor, float value)
    {
        if(cursor < 5 && cursor > 0)
        {
            quaternion[cursor-1] = value;
            return true;
        }
        VFV_DATA_ERROR
    }

    bool pushValue(uint32_t cursor, const std::string& value)
    {
        if(cursor == 0)
        {
            dataset = value;
            return true;
        }
        VFV_DATA_ERROR
    }

    uint32_t getMaxCursor() const {return 4;}
};

/* \brief Represents the information the tablet send when authentifying */
struct VFVIdentTabletInformation : public VFVDataInformation
{
    std::string hololensIP; /*!< The hololens IP adresse it is bound to*/

    char getTypeAt(uint32_t cursor) const
    {
        if(cursor == 0)
            return 's';
        return 0;
    }

    bool pushValue(uint32_t cursor, const std::string& value)
    {
        if(cursor == 0)
        {
            hololensIP = value;
            return true;
        }
        VFV_DATA_ERROR
    }

    uint32_t getMaxCursor() const {return 0;}
};

/* \brief Represents the information about dataset manipulation (adding, removing) */
struct VFVDatasetInformation : public VFVDataInformation
{
    std::string name; /*!< The name of the dataset*/

    char getTypeAt(uint32_t cursor) const
    {
        if(cursor == 0)
            return 's';
        return 0;
    }

    bool pushValue(uint32_t cursor, const std::string& value)
    {
        if(cursor == 0)
        {
            name = value;
            return true;
        }
        VFV_DATA_ERROR
    }

    uint32_t getMaxCursor() const {return 0;}
};

/* \brief Represents the information about the change of color of a dataset represented */
struct VFVColorInformation : public VFVDataInformation
{
    std::string dataset;  /*!< The dataset name bound to this call*/
    float     min;        /*!< Minimum range (clamping, ratio : 0.0, 1.0)*/
    float     max;        /*!< Maximum range (clamping, ratio : 0.0, 1.0)*/
    ColorMode mode;       /*!< The color mode to use*/

    char getTypeAt(uint32_t cursor) const
    {
        if(cursor == 1 || cursor == 2)
            return 'f';
        else if(cursor == 3)
            return 'I';
        else if(cursor == 0)
            return 's';
        return 0;
    }

    bool pushValue(uint32_t cursor, float value)
    {
        if(cursor == 1)
        {
            min = value;
            return true;
        }
        else if(cursor == 2)
        {
            max = value;
            return true;
        }

        VFV_DATA_ERROR
    }

    bool pushValue(uint32_t cursor, uint32_t value)
    {
        if(cursor == 3)
        {
            mode = (ColorMode)value;
            return true;
        }
        VFV_DATA_ERROR
    }

    bool pushValue(uint32_t cursor, const std::string& value)
    {
        if(cursor == 0)
        {
            dataset = value;
            return true;
        }

        VFV_DATA_ERROR
    }

    uint32_t getMaxCursor() const {return 3;}
};

#undef VFV_DATA_ERROR

#endif
