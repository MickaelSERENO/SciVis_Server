#ifndef  VFVDATAINFORMATION_INC
#define  VFVDATAINFORMATION_INC

#include "utils.h"
#include <string>
#include <cstdint>

#define VFV_DATA_ERROR \
    ERROR << "The parameter value as not the correct type for cursor = " << cursor << std::endl;\
    return false;

namespace sereno
{
    /* \brief Structure containing the Data information for the VFV Application 
     * I : uint32_t
     * i : uint16_t
     * s : string
     * f : float*/
    struct VFVDataInformation
    {
        virtual ~VFVDataInformation(){}

        /* \brief Get the type of the information at "cursor"
         * \param cursor the position of the information currently being read
         * \return 's' for string, 'i' for uint16_t, 'I' for uint32_t and 'f' for float. 0 is returned if nothing should be at this position */
        virtual char getTypeAt(uint32_t cursor) const = 0;

        /* \brief Get the maximum cursor position of the data being read (included)
         * \return the maximum information ID */
        virtual int32_t getMaxCursor() const = 0;

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

    /** \brief  No Data information to receive yet */
    struct VFVNoDataInformation : public VFVDataInformation
    {
        virtual char getTypeAt(uint32_t cursor) const {return 'I';}
        virtual int32_t getMaxCursor() const {return -1;}
    };

    /* \brief Structure containing information about the rotation of the tablet */
    struct VFVRotationInformation : public VFVDataInformation
    {
        uint32_t datasetID;     /*!< The dataset ID*/
        uint32_t subDatasetID;  /*!< The SubDataset ID*/
        float    quaternion[4]; /*!< The quaternion information*/

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor < 2)
                return 'I';
            else if(cursor < 6)
                return 'f';
            return 0;
        }

        bool pushValue(uint32_t cursor, float value)
        {
            if(cursor < 6 && cursor >= 2)
            {
                quaternion[cursor-2] = value;
                return true;
            }
            VFV_DATA_ERROR
        }

        bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 0)
            {
                datasetID = value;
                return true;
            }

            if(cursor == 1)
            {
                subDatasetID = value;
                return true;
            }
            VFV_DATA_ERROR
        }

        int32_t getMaxCursor() const {return 5;}
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

        int32_t getMaxCursor() const {return 0;}
    };

    /* \brief Represents the information about binary dataset addition*/
    struct VFVBinaryDatasetInformation : public VFVDataInformation
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

        int32_t getMaxCursor() const {return 0;}
    };

    /* \brief Represents the information about VTK Datasets*/
    struct VFVVTKDatasetInformation : public VFVDataInformation
    {
        std::string name;                 /*!< The name of the dataset*/
        uint32_t    nbPtFields   = 0;     /*!< The number of point field*/
        uint32_t    nbCellFields = 0;     /*!< The number of cell field*/
        std::vector<uint32_t> ptFields;   /*!<Indices of the point fields to read*/
        std::vector<uint32_t> cellFields; /*!<Indices of the cell fields to read*/

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor == 0)
                return 's';
            return 'I';
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

        bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 1)
            {
                nbPtFields = value;
                return true;
            }
            if(cursor > 1 && cursor < nbPtFields+2)
            {
                ptFields.push_back(value);
                return true;
            }
            if(cursor == nbPtFields+2)
            {
                nbCellFields = value;
                return true;
            }
            cellFields.push_back(value);
            return true;
        }

        int32_t getMaxCursor() const {return 2+nbPtFields+nbCellFields;}
    };

    /* \brief Represents the information about the change of color of a dataset represented */
    struct VFVColorInformation : public VFVDataInformation
    {
        uint32_t  datasetID;  /*!< The Dataset ID*/
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
                return 'i';
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
            if(cursor == 0)
            {
                datasetID = value;
                return true;
            }
            else if(cursor == 3)
            {
                mode = (ColorMode)value;
                return true;
            }
            VFV_DATA_ERROR
        }

        int32_t getMaxCursor() const {return 3;}
    };
}

#undef VFV_DATA_ERROR

#endif
