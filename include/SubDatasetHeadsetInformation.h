#ifndef  SUBDATASETHEADSETINFORMATION_INC
#define  SUBDATASETHEADSETINFORMATION_INC

#include "visibility.h"
#include "Datasets/SubDataset.h"

namespace sereno
{
    /** \brief  SubDataset information per headset user */
    class SubDatasetHeadsetInformation
    {
        public:
            /** \brief  Default constructor */
            SubDatasetHeadsetInformation(SubDataset* sd);

            /** \brief  Default destructor */
            ~SubDatasetHeadsetInformation();

            /* \brief  Set the visibility of this SubDataset
             * \param v the new visibility */
            void setVisibility(int v) {m_visibility = v;}

            /* \brief  Get the visibility of this SubDataset
             * \return  The SuBDataset visibility */
            int getVisibility() {return m_visibility;}
        private:
            SubDataset* m_sd         = NULL;              /*!< The bound SubDataset*/
            int         m_visibility = VISIBILITY_PUBLIC; /*!< The visibility regarding the subdataset*/
    };
}

#endif
