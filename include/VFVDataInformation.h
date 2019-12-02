#ifndef  VFVDATAINFORMATION_INC
#define  VFVDATAINFORMATION_INC

#include "utils.h"
#include "visibility.h"
#include "TransferFunction/TFType.h"
#include <string>
#include <cstdint>
#include <memory>
#include <sstream>

#define VFV_DATA_ERROR \
{\
    ERROR << "The parameter value as not the correct type for cursor = " << cursor << std::endl;\
    return false;\
}

#define VFV_BEGINING_TO_JSON(_oss, _sender, _headsetIP, _timeOffset, _type) \
{\
    (_oss) << "{\n" \
           << "    \"type\" : \"" << (_type) << "\",\n"           \
           << "    \"sender\" : \"" << (_sender) << "\",\n"       \
           << "    \"headsetIP\" : \"" << (_headsetIP) << "\",\n" \
           << "    \"timeOffset\" : " << (_timeOffset) << "\n";  \
}

#define VFV_END_TO_JSON(_oss)\
{\
    (_oss) << "}";\
}

#define VFV_SENDER_TABLET  "Tablet"
#define VFV_SENDER_HEADSET "Headset"
#define VFV_SENDER_SERVER  "Server"
#define VFV_SENDER_UNKNOWN "Unknown"


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

        /* \brief  Convert the message in a JSON format 
         *
         * \param sender who sent this message ? Server, Tablet or Headset
         * \param headsetIP what is the associated headset IP address?*/
        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;
            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "DataInformation");
            VFV_END_TO_JSON(oss);
            return oss.str();
        }
    };

    /** \brief  No Data information to receive yet */
    struct VFVNoDataInformation : public VFVDataInformation
    {
        uint32_t type;

        virtual char getTypeAt(uint32_t cursor) const {return 'I';}
        virtual int32_t getMaxCursor() const {return -1;}

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;
            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "NoDataInformation");
            oss << ",    \"type\" : " << type << "\n";
            VFV_END_TO_JSON(oss);
            return oss.str();
        }
    };

    struct VFVClearAnnotations : public VFVDataInformation
    {
        int32_t datasetID    = 0;
        int32_t subDatasetID = -1;

        virtual bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 0)
                datasetID = value;
            else if(cursor == 1)
                subDatasetID = value;
            else
                VFV_DATA_ERROR
            return true;
        }

        virtual char getTypeAt(uint32_t cursor) const 
        {
            if(cursor <= 1)
                return 'I';
            return 0;
        }

        virtual int32_t getMaxCursor() const {return 1;}

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "ClearAnnotations");
            oss << ",    \"datasetID\" : " << datasetID << ",\n"
                << "    \"subDatasetID\" : " << subDatasetID << "\n";
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
    };

    struct VFVAnchorAnnotation : public VFVDataInformation
    {
        int32_t datasetID    = 0;
        int32_t subDatasetID = -1;
        int32_t annotationID = -1; /*!< Only the server sets this information*/
        int32_t headsetID    = -1; /*!< Only the server sets this information*/
        float   localPos[3];

        virtual bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 0)
                datasetID = value;
            else if(cursor == 1)
                subDatasetID = value;
            else
                VFV_DATA_ERROR
            return true;
        }

        virtual bool pushValue(uint32_t cursor, float value)
        {
            if(cursor <= 4 && cursor >= 2)
                localPos[cursor-2] = value;
            else
                VFV_DATA_ERROR
            return true;
        }

        virtual char getTypeAt(uint32_t cursor) const 
        {
            if(cursor <= 1)
                return 'I';
            else if(cursor <= 4)
                return 'f';
            return 0;
        }
        virtual int32_t getMaxCursor() const {return 4;}

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "AnchorAnnotation");
            oss << ",    \"datasetID\" : " << datasetID << ",\n"
                << "    \"subDatasetID\" : " << subDatasetID << ",\n" 
                << "    \"annotationID\" : " << annotationID << ",\n"
                << "    \"localPos\" : [" << localPos[0] << "," << localPos[1] << "," << localPos[2] << "]\n";
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
    };

    /** \brief  Start to create an annotation usin the headset */
    struct VFVStartAnnotation : public VFVDataInformation
    {
        int32_t datasetID    = 0;
        int32_t subDatasetID = -1;
        int32_t pointingID   = 0;

        virtual bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 0)
                datasetID = value;
            else if(cursor == 1)
                subDatasetID = value;
            else if(cursor == 2)
                pointingID = value;
            else
                VFV_DATA_ERROR
            return true;
        }

        virtual char getTypeAt(uint32_t cursor) const 
        {
            if(cursor <= 2)
                return 'I';
            return 0;
        }
        virtual int32_t getMaxCursor() const {return 2;}

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "StartAnnotation");
            oss << ",    \"datasetID\" : " << datasetID << ",\n"
                << "    \"subDatasetID\" : " << subDatasetID << ",\n" 
                << "    \"pointingID\" : " << pointingID << "\n";
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
    };

    /** \brief  Set the current subdataset of the headset */
    struct VFVHeadsetCurrentSubDataset : public VFVDataInformation
    {
        int32_t datasetID    = 0;  /*!< The dataset ID*/
        int32_t subDatasetID = -1; /*!< The subdataset ID*/

        virtual bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 0)
                datasetID = value;
            else if(cursor == 1)
                subDatasetID = value;
            else
                VFV_DATA_ERROR
            return true;
        }

        virtual char getTypeAt(uint32_t cursor) const {return 'I';}
        virtual int32_t getMaxCursor() const {return 1;}

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "HeadsetCurrentSubDataset");
            oss << ",    \"datasetID\" : " << datasetID << ",\n"
                << "    \"subDatasetID\" : " << subDatasetID << "\n"; 
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
    };

    /** \brief  Set the headset current action */
    struct VFVHeadsetCurrentAction : public VFVDataInformation
    {
        uint32_t action = 0; /*!< The type of the action (see VFVHeadsetCurrentActionType)*/

        virtual bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 0)
                action = value;
            else
                VFV_DATA_ERROR
            return true;
        }

        virtual char getTypeAt(uint32_t cursor) const {return 'I';}
        virtual int32_t getMaxCursor() const {return 0;}

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "HeadsetCurrentAction");
            oss << ",    \"action\" : " << action << "\n";
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
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

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "AnchoringDataStatus");
            oss << ",    \"succeed\" : " << succeed << "\n";
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
    };

    /** \brief  default ByteArray message */
    struct VFVDefaultByteArray : public VFVDataInformation
    {
        uint32_t type = -1;

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

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "DefaultByteArray");
            oss << ",    \"type\" : " << type << ",\n"
                << "    \"dataSize\" : " << dataSize << "\n";
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
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
        float   position[3]; /*!< 3D headset position*/
        float   rotation[4]; /*!< 3D quaternion headset rotation*/

        int32_t pointingIT           = -1;       /*!< The pointing interaction technique in use (enum)*/
        int32_t pointingDatasetID    = -1;       /*!< The dataset currently manipulated (ID) with the pointing*/
        int32_t pointingSubDatasetID = -1;       /*!< The SubDataset currently manipulated (ID) with the pointing*/
        bool    pointingInPublic     = true;     /*!< Is the pointing action in the user's public space?*/
        float   pointingLocalSDPosition[3];      /*!< The position targeted by the pointing IT in the local Subdataset's space*/
        float   pointingHeadsetStartPosition[3]; /*!< The position of the headset once the pointing interaction started*/
        float   pointingHeadsetStartOrientation[4]; /*!< The orientation of the headset once the pointing interaction started*/

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor < 7)
                return 'f';
            else if(cursor < 10)
                return 'I';
            else if(cursor == 10)
                return 'b';
            else if(cursor < 14)
                return 'f';
            else if(cursor < 17)
                return 'f';
            else if(cursor < 21)
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
            else if(cursor < 14 && cursor > 10)
            {
                pointingLocalSDPosition[cursor-11] = value;
                return true;
            }
            else if(cursor < 17 && cursor > 13)
            {
                pointingHeadsetStartPosition[cursor-14] = value;
                return true;
            }
            else if(cursor < 21 && cursor > 16)
            {
                pointingHeadsetStartOrientation[cursor-17] = value;
                return true;
            }
            VFV_DATA_ERROR
        }

        bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 7)
            {
                pointingIT = value;
                return true;
            }
            else if(cursor == 8)
            {
                pointingDatasetID = value;
                return true;
            }

            else if(cursor == 9)
            {
                pointingSubDatasetID = value;
                return true;
            }
            VFV_DATA_ERROR
        }

        bool pushValue(uint32_t cursor, uint8_t value)
        {
            if(cursor == 10)
            {
                pointingInPublic = value;
                return true;
            }
            VFV_DATA_ERROR
        }

        int32_t getMaxCursor() const {return 20;}

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

#ifdef LOG_UPDATE_HEAD
            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "UpdateHeadset");
            oss << ",    \"position\" : [" << position[0] << "," << position[1] << "," << position[2] << "],\n"
                << "    \"rotation\" : [" << rotation[0] << "," << rotation[1] << "," << rotation[2] << "," << rotation[3] << "],\n" 
                << "    \"pointingIT\" : " << pointingIT << ",\n"
                << "    \"pointingDatasetID\" : " << pointingDatasetID << ",\n"
                << "    \"pointingSubDatasetID\" : " << pointingSubDatasetID << ",\n"
                << "    \"pointingInPublic\" : " << pointingInPublic << ",\n"
                << "    \"pointingLocalSDPosition\" : [" << pointingLocalSDPosition[0] << "," << pointingLocalSDPosition[1] << "," << pointingLocalSDPosition[2] << "],\n"
                << "    \"pointingHeadsetStartPosition\" : [" << pointingHeadsetStartPosition[0] << "," << pointingHeadsetStartPosition[1] << "," << pointingHeadsetStartPosition[2] << "],\n"
                << "    \"pointingHeadsetStartOrientation\" : [" << pointingHeadsetStartOrientation[0] << "," << pointingHeadsetStartOrientation[1] << "," << pointingHeadsetStartOrientation[2] << "," << pointingHeadsetStartOrientation[3] << "]\n";
            VFV_END_TO_JSON(oss);
#endif

            return oss.str();
        }
    };

    /** \brief  Structure containing information for Transfer Function to apply to a dataset*/
    struct VFVTransferFunctionSubDataset : public VFVDataInformation
    {
        struct GTFData
        {
            uint32_t propID; /*!< The property ID*/
            float    center; /*!< The center of the gaussian function*/
            float    scale;  /*!< The scale of the gaussian function*/
        };

        uint32_t datasetID;      /*!< The dataset ID*/
        uint32_t subDatasetID;   /*!< The SubDataset ID*/
        int32_t  headsetID = -1; /*!< The headset ID performing the rotation. -1 if not initialized.*/
        uint8_t  tfID = -1;      /*!< The transfer function ID*/
        struct
        {
            uint32_t nbProps = 0;          /*!< The number of properties*/
            uint8_t  colorMode;            /*!< The color mode to apply*/ 
            std::vector<GTFData> propData; /*!< Each property data*/
        }gtfData;

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor < 2) //dataset/subdatasetID
                return 'I';
            else if(cursor == 3) //tfID
                return 'b';
            else
            {
                switch((TFType)tfID)
                {
                    case TF_GTF:
                    case TF_TRIANGULAR_GTF:
                    {
                        if(cursor == 4) //nbProps
                            return 'I';
                        if((cursor-5) / 3 > gtfData.nbProps) //Check the size
                            return 0;

                        uint32_t offset = (cursor-5)%3;
                        if(offset == 0) //propID
                            return 'I';
                        return 'f'; //center, scale
                    }
                    default:
                        return 0;
                }
            }
            return 0;
        }

        bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 0)
            {
                datasetID = value;
                return true;
            }
            else if(cursor == 1)
            {
                subDatasetID = value;
                return true;
            }
            else if(cursor >= 3)
            {
                switch((TFType)tfID)
                {
                    case TF_GTF:
                    case TF_TRIANGULAR_GTF:
                    {
                        if(cursor == 4)
                        {
                            gtfData.nbProps = value;
                            return true;
                        }
                        else
                        {
                            uint32_t id     = (cursor-6)/3;
                            uint32_t offset = (cursor-6)%3;
                            if(id > gtfData.nbProps)
                                VFV_DATA_ERROR

                            if(offset == 0)
                            {
                                gtfData.propData[id].propID = value;
                                return true;
                            }
                        }
                    }
                    default:
                        VFV_DATA_ERROR
                }
            }
            VFV_DATA_ERROR
        }

        bool pushValue(uint32_t cursor, uint8_t value)
        {
            if(cursor == 2)
            {
                tfID = value;
                return true;
            }

            else if(cursor >= 3)
            {
                switch((TFType)tfID)
                {
                    case TF_GTF:
                    case TF_TRIANGULAR_GTF:
                        if(cursor == 5)
                        {
                            gtfData.colorMode = value;
                            return true;
                        }
                        break;
                    default:
                        VFV_DATA_ERROR
                };
            }
            VFV_DATA_ERROR;
        }

        bool pushValue(uint32_t cursor, float value)
        {
            if(cursor <= 2)
                VFV_DATA_ERROR
            switch((TFType)tfID)
            {
                case TF_GTF:
                case TF_TRIANGULAR_GTF:
                {
                    if(cursor == 4)
                        VFV_DATA_ERROR

                    uint32_t id     = (cursor-6)/3;
                    uint32_t offset = (cursor-6)%3;
                    if(id > gtfData.nbProps)
                        VFV_DATA_ERROR

                    if(offset == 1)
                    {
                        gtfData.propData[id].center = value;
                        return true;
                    }
                    else if(offset == 2)
                    {
                        gtfData.propData[id].scale = value;
                        return true;
                    }

                }

                default:
                    VFV_DATA_ERROR
            }
        }

        int32_t getMaxCursor() const 
        {
            switch((TFType)tfID)
            {
                case TF_GTF:
                case TF_TRIANGULAR_GTF:
                    return 4+gtfData.nbProps*3;
                default:
                    return 2;
            }
        }
    };

    /** \brief  Structure containing information for dataset scaling */
    struct VFVScaleInformation : public VFVDataInformation
    {
        uint32_t datasetID;      /*!< The dataset ID*/
        uint32_t subDatasetID;   /*!< The SubDataset ID*/
        int32_t  headsetID = -1; /*!< The headset ID performing the rotation. -1 if not initialized.*/
        float    scale[3];       /*!< The scale information*/

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
                scale[cursor-2] = value;
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

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "ScaleDataset");
            oss << ",    \"datasetID\" : " << datasetID << ",\n" 
                << "    \"subDatasetID\" : " << subDatasetID << ",\n"
                << "    \"scale\" : [" << scale[0] << "," << scale[1] << "," << scale[2] << "]\n";
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
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

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "MoveDataset");
            oss << ",    \"datasetID\" : " << datasetID << ",\n" 
                << "    \"subDatasetID\" : " << subDatasetID << ",\n"
                << "    \"position\" : [" << position[0] << "," << position[1] << "," << position[2] << "]\n";
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
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

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "RotateDataset");
            oss << ",    \"datasetID\" : " << datasetID << ",\n" 
                << "    \"subDatasetID\" : " << subDatasetID << ",\n"
                << "    \"quaternion\" : [" << quaternion[0] << "," << quaternion[1] << "," << quaternion[2] << "," << quaternion[3] << "]\n";
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
    };

    /* \brief Represents the information the tablet send when authentifying */
    struct VFVIdentTabletInformation : public VFVDataInformation
    {
        std::string headsetIP; /*!< The headset IP adresse it is bound to*/
        uint32_t handedness;   /*!< The user's handedness*/
        uint32_t tabletID;     /*!< The tablet ID, permits to define for instance roles per tablet */
        bool paired = false; /* !< To set: is the tablet paired after this message was handled? */

        char getTypeAt(uint32_t cursor) const
        {
            if(cursor == 0)
                return 's';
            else if(cursor == 1 || cursor == 2)
                return 'I';
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

        bool pushValue(uint32_t cursor, uint32_t value)
        {
            if(cursor == 1)
            {
                handedness = value;
                return true;
            }
            if(cursor == 2)
            {
                tabletID = value;
                return true;
            }
            VFV_DATA_ERROR
        }

        int32_t getMaxCursor() const {return 2;}

        virtual std::string toJson(const std::string& sender, const std::string& pairedHeadsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, pairedHeadsetIP, timeOffset, "TabletIdent");
            oss << ",    \"targetedHeadsetIP\" : \"" << headsetIP << "\",\n"
                << "    \"paired\" : " << paired << ",\n"
                << "    \"tabletID\" : " << tabletID << "\n"; 
            VFV_END_TO_JSON(oss);

            return oss.str();
        }
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

        virtual std::string toJson(const std::string& sender, const std::string& headsetIP, time_t timeOffset) const
        {
            std::ostringstream oss;

            VFV_BEGINING_TO_JSON(oss, sender, headsetIP, timeOffset, "VTKDataset");
            oss << ",    \"name\" : \"" << name << "\",\n";

            if(ptFields.size() == 0)
                oss << "    \"ptFields\" : [],\n";
            else if(ptFields.size() == 1)
                oss << "    \"ptFields\" : [" << ptFields[0] << "],\n";
            else if(ptFields.size() > 1)
            {
                oss << "    \"ptFields\" : [";
                for(uint32_t i = 0; i < ptFields.size()-1; i++)
                    oss << ptFields[i] << ",";
                oss << ptFields[ptFields.size()-1] << "],\n";
            }

            if(cellFields.size() == 0)
                oss << "    \"cellFields\" : []\n";
            else if(cellFields.size() == 1)
                oss << "    \"cellFields\" : [" << cellFields[0] << "]\n";
            else if(cellFields.size() > 1)
            {
                oss << "    \"cellFields\" : [";
                for(uint32_t i = 0; i < cellFields.size()-1; i++)
                    oss << cellFields[i] << ",";
                oss << cellFields[cellFields.size()-1] << "]\n";
            }

            VFV_END_TO_JSON(oss);

            return oss.str();
        }
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
