#ifndef  ANCHORHEADSETDATA_INC
#define  ANCHORHEADSETDATA_INC

#include <cstdint>
#include <vector>
#include "VFVDataInformation.h"

namespace sereno
{
    class AnchorHeadsetData
    {
        public:
            /* \brief  Is the anchor headset data completed ?
             * \return  true if completed, false otherwise */
            bool isCompleted() const {return m_isCompleted;}

            /* \brief Finalize the receiving segment data message
             * \param success is the data correctly sent? */
            void finalize(bool success) 
            {
                if(success)
                    m_isCompleted = true;
                else
                {
                    m_isCompleted = false;
                    m_segmentData.clear();
                }
            }

            /* \brief  Get all the segment values.
             * \return  array of the segment values */
            const std::vector<VFVDefaultByteArray>& getSegmentData() const {return m_segmentData;}

            /* \brief  Push a new data segment containing anchor information
             * \param arr the new segment to push */
            void pushDataSegment(VFVDefaultByteArray& arr) {m_segmentData.push_back(arr);}
        private:
            /** \brief  The segment data array */
            std::vector<VFVDefaultByteArray> m_segmentData;
            /** \brief  Is the segment data array completed? */
            bool                             m_isCompleted = false;
    };
}

#endif
