#ifndef  VFVDATAINFORMATION_INC
#define  VFVDATAINFORMATION_INC

#include "utils.h"
#include <string>
#include <cstdint>
#include <memory>

#define VFV_DATA_ERROR \
{\
    ERROR << "The parameter value as not the correct type for cursor = " << cursor << std::endl;\
    return false;\
}

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
        virtual bool pushValue(uint32_t cursor, const std::string& value)       {return false;}
        virtual bool pushValue(uint32_t cursor, float value)                    {return false;}
        virtual bool pushValue(uint32_t cursor, double value)                   {return pushValue(cursor, (float)value);}
        virtual bool pushValue(uint32_t cursor, int16_t value)                  {return pushValue(cursor, (uint16_t)value);}
        virtual bool pushValue(uint32_t cursor, uint16_t value)                 {return pushValue(cursor, (uint32_t)value);}
        virtual bool pushValue(uint32_t cursor, int32_t value)                  {return pushValue(cursor, (uint32_t)value);}
        virtual bool pushValue(uint32_t cursor, uint32_t value)                 {return false;}
        virtual bool pushValue(uint32_t cursor, uint8_t value)                  {return pushValue(cursor, (uint32_t)value);}

        virtual bool pushValue(uint32_t cursor, std::shared_ptr<uint8_t> value, uint32_t size)
        {return false;}
    };

    /** \brief  No Data information to receive yet */
    struct VFVNoDataInformation : public VFVDataInformation
    {
        virtual char getTypeAt(uint32_t cursor) const {return 'I';}
        virtual int32_t getMaxCursor() const {return -1;}
    };

    /** \brief  The annotation existing type */
    enum VFVAnnotationType
    {
        ANNOTATION_STROKE,
        ANNOTATION_TEXT,
        ANNOTATION_NONE
    };

    struct VFVAnchoringDataStatus : public VFVDataInformation
    {
        bool succeed = false;

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor == 0)
                return 'b';
            return 0;
        }

        bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 0)
            {
                succeed = (value != 0);
                return true;
            }
            VFV_DATA_ERROR
        }

        int32_t getMaxCursor() const {return 0;}
    };

    /** \brief  default ByteArray message */
    struct VFVDefaultByteArray : public VFVDataInformation
    {
        std::shared_ptr<uint8_t> data;         /*!< Raw data*/
        uint32_t                 dataSize = 0; /*!< Data size*/

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor == 0)
                return 'a';
            return 0;
        }

        bool pushValue(uint32_t cursor, std::shared_ptr<uint8_t> value, uint32_t size)
        {
            if(cursor == 0)
            {
                dataSize = size;
                data     = value;
                return true;
            }
            return false;
        }

        int32_t getMaxCursor() const {return 0;}
    };

    /** \brief  Annotation stroke message */
    struct VFVAnnotationStroke : public VFVDataInformation
    {
        uint32_t color;        /** The RGBA color*/
        float    width;        /** The stroke width*/
        uint32_t nbPoints = 0; /** The number of points the stroke contains*/

        std::vector<float> pointsX; /** X coordinates of the stroke points*/
        std::vector<float> pointsY; /** Y coordinates of the stroke points*/

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor == 0 || cursor == 2)
                return 'I';
            else if(cursor == 1 || cursor < nbPoints*2+3)
                return 'f';
            return 0;
        }

        bool pushValue(uint32_t cursor, uint32_t value)
        {
            switch(cursor)
            {
                case 0:
                    color = value;
                    break;
                case 2:
                    nbPoints = value;
                    break;
                default:
                    VFV_DATA_ERROR
            }
            return true;
        }

        bool pushValue(uint32_t cursor, float value)
        {
            if(cursor == 1)
            {
                width = value;
                return true;
            }

            if(cursor < 3+nbPoints*2 && cursor > 2)
            {
                if((cursor-3)%2)
                    pointsY.push_back(value);
                pointsX.push_back(value);
                return true;
            }

            VFV_DATA_ERROR
        }

        bool pushValue(uint32_t cursor, const std::string& value) {return false;}

        bool pushValue(uint32_t cursor, std::shared_ptr<uint8_t> value, uint32_t size) {return false;}

        int32_t getMaxCursor() const {return 2+2*nbPoints;}
    };

    /** \brief  Annotation Text message */
    struct VFVAnnotationText : public VFVDataInformation
    {
        uint32_t    color; /** The RGBA color*/
        float       posX;  /** The X position*/
        float       posY;  /** The Y position*/
        std::string text;  /** The text value*/

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor == 0)
                return 'I';
            else if(cursor == 1 || cursor == 2)
                return 'f';
            else if(cursor == 3)
                return 's';
            return 0;
        }

        bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 0)
            {
                color = value;
                return true;
            }
            VFV_DATA_ERROR;
        }

        bool pushValue(uint32_t cursor, float value)
        {
            if(cursor == 1)
            {
                posX = value;
                return true;
            }
            
            else if(cursor == 2)
            {
                posY = value;
                return true;
            }

            VFV_DATA_ERROR
        }

        bool pushValue(uint32_t cursor, const std::string& value)
        {
            if(cursor == 3)
            {
                text = value;
                return true;
            }
            VFV_DATA_ERROR
        }

        bool pushValue(uint32_t cursor, std::shared_ptr<uint8_t> value, uint32_t size) {return false;}

        int32_t getMaxCursor() const {return 3;}
    };

    /** \brief  Whole annotation message */
    struct VFVAnnotation : public VFVDataInformation
    {
        uint32_t datasetID;     /** The dataset ID bound to the SubDataset*/
        uint32_t subDatasetID;  /** The SubDataset ID bound to the annotation*/
        uint32_t annotationID;  /** The annotation ID bound to the annotation*/
        uint32_t textureWidth;  /** The texture width*/
        uint32_t textureHeight; /** The texture height*/

        uint32_t nbStroke = 0;  /** The number of strokes this annotation contains*/
        uint32_t nbText   = 0;  /** The number of text this annotation contains*/

        std::vector<VFVAnnotationStroke> strokes; /** Array of strokes information*/
        std::vector<VFVAnnotationText>   texts;   /** Array of texts information*/

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor < 7)
                return 'I';
            if(m_annotType == ANNOTATION_STROKE)
                return m_strokeParse.getTypeAt(cursor-m_onCreationCursor);
            else if(m_annotType == ANNOTATION_NONE && nbStroke > strokes.size())
                return m_strokeParse.getTypeAt(0);
            else if(m_annotType == ANNOTATION_TEXT)
                return m_textParse.getTypeAt(cursor-m_onCreationCursor);
            else if(m_annotType == ANNOTATION_NONE && nbText > texts.size())
                return m_strokeParse.getTypeAt(0);
            return 0;
        }

#define ANNOTATION_PUSH_VALUE                                               \
{                                                                           \
    checkCreateInformation(cursor);                                         \
    bool _ret = false;                                                      \
    if(m_annotType == ANNOTATION_STROKE)                                    \
        _ret = m_strokeParse.pushValue(cursor - m_onCreationCursor, value); \
    else if(m_annotType == ANNOTATION_TEXT)                                 \
        _ret = m_textParse.pushValue(cursor - m_onCreationCursor, value);   \
    checkEndInformation(cursor);                                            \
    if(_ret)                                                                \
        m_lastCursor = cursor;                                              \
    return _ret;                                                            \
}

        bool pushValue(uint32_t cursor, const std::string& value)
        {
            ANNOTATION_PUSH_VALUE
        }

        bool pushValue(uint32_t cursor, float value)
        {
            ANNOTATION_PUSH_VALUE
        }

        bool pushValue(uint32_t cursor, std::shared_ptr<uint8_t> value, uint32_t size)
        {
            checkCreateInformation(cursor);
            bool _ret = false;
            if(m_annotType == ANNOTATION_STROKE)
                _ret = m_strokeParse.pushValue(cursor - m_onCreationCursor, value, size);
            else if(m_annotType == ANNOTATION_TEXT)
                _ret = m_textParse.pushValue(cursor - m_onCreationCursor, value, size);
            checkEndInformation(cursor);
            if(_ret)
                m_lastCursor = cursor;
            return _ret;
        }

        bool pushValue(uint32_t cursor, uint32_t value)
        {
            switch(cursor)
            {
                case 0:
                    datasetID = value;
                    break;
                case 1:
                    subDatasetID = value;
                    break;
                case 2:
                    annotationID = value;
                    break;
                case 3:
                    textureWidth = value;
                    break;
                case 4:
                    textureHeight = value;
                    break;
                case 5:
                    nbStroke = value;
                    break;
                case 6:
                    nbText = value;
                    break;
                default:
                {
                    ANNOTATION_PUSH_VALUE
                }
            }
            m_lastCursor = cursor;
            return true;
        }

        int32_t getMaxCursor() const
        {
            if(m_lastCursor < 6 || strokes.size() < nbStroke || texts.size() < nbText)
                return m_lastCursor+1; //Continue
            return m_lastCursor; //We have finished
        }

        private:
            void checkCreateInformation(uint32_t cursor)
            {
                m_lastCursor = cursor;
                if(m_recreateAnnot)
                {
                    m_recreateAnnot = false;
                    m_onCreationCursor = cursor;
                    if(strokes.size() < nbStroke)
                    {
                        new(&m_strokeParse)(VFVAnnotationStroke);
                        m_annotType = ANNOTATION_STROKE;
                    }
                    else
                    {
                        new(&m_textParse)(VFVAnnotationText);
                        m_annotType = ANNOTATION_TEXT;
                    }
                }
            }

            void checkEndInformation(uint32_t cursor)
            {
                int32_t maxCursor = -1;
            
                if(m_annotType == ANNOTATION_STROKE)
                    maxCursor = m_strokeParse.getMaxCursor();
                else if(m_annotType == ANNOTATION_TEXT)
                    maxCursor = m_textParse.getMaxCursor();
                if((int32_t)(cursor - m_onCreationCursor) >= maxCursor)
                {
                    if(m_annotType == ANNOTATION_STROKE)
                        strokes.push_back(m_strokeParse);
                    else if(m_annotType == ANNOTATION_TEXT)
                        texts.push_back(m_textParse);
                    m_recreateAnnot = true;
                    m_annotType = ANNOTATION_NONE;
                }
            }

            uint32_t m_onCreationCursor = 0;    /** The cursor used when the last data information was created*/
            uint32_t m_lastCursor       = 0;
            bool     m_recreateAnnot    = true;
            VFVAnnotationType m_annotType = ANNOTATION_NONE;
            VFVAnnotationStroke m_strokeParse;
            VFVAnnotationText   m_textParse;
    };

    /** \brief  Structure containing continuous stream of headset status */
    struct VFVUpdateHeadset : public VFVDataInformation
    {
        float position[3]; /*!< 3D headset position*/
        float rotation[4]; /*!< 3D quaternion headset rotation*/

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor < 7)
                return 'f';
            return 0;
        }

        bool pushValue(uint32_t cursor, float value)
        {
            if(cursor < 3)
            {
                position[cursor] = value;
                return true;
            }
            else if(cursor < 7)
            {
                rotation[cursor-3] = value;
                return true;
            }
            VFV_DATA_ERROR
        }

        int32_t getMaxCursor() const {return 6;}
    };

    /** \brief  Structure containing information for dataset movement */
    struct VFVMoveInformation : public VFVDataInformation
    {
        uint32_t datasetID;     /*!< The dataset ID*/
        uint32_t subDatasetID;  /*!< The SubDataset ID*/
        float    position[3];   /*!< The position information*/
        int32_t  headsetID = -1; /*!< The headset ID performing the rotation. -1 if not initialized.*/

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor < 2)
                return 'I';
            else if(cursor < 5)
                return 'f';
            return 0;
        }

        bool pushValue(uint32_t cursor, float value)
        {
            if(cursor < 5 && cursor >= 2)
            {
                position[cursor-2] = value;
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

        int32_t getMaxCursor() const {return 4;}
    };

    /* \brief Structure containing information about the rotation of the tablet */
    struct VFVRotationInformation : public VFVDataInformation
    {
        uint32_t datasetID;      /*!< The dataset ID*/
        uint32_t subDatasetID;   /*!< The SubDataset ID*/
        float    quaternion[4];  /*!< The quaternion information*/
        int32_t  headsetID = -1; /*!< The headset ID performing the rotation. -1 if not initialized.*/

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
        std::string headsetIP; /*!< The headset IP adresse it is bound to*/

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
                headsetIP = value;
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
