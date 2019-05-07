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
            /** \brief  Default constructor. A private subdataset state will be created from the public states. However the transfer function might be needed to be set afterward (using the provided getters and setters)
             * \param publicSD the public subdataset states. Cannot be NULL*/
            SubDatasetHeadsetInformation(SubDataset* publicSD);

            /** \brief  Default destructor */
            ~SubDatasetHeadsetInformation();

            /* \brief  Set the visibility of this SubDataset
             * \param v the new visibility */
            void setVisibility(int v) {m_visibility = v;}

            /* \brief  Get the visibility of this SubDataset
             * \return  The SuBDataset visibility */
            int getVisibility() const {return m_visibility;}

            /* \brief  Get the public subdataset states
             * \return  The public subdataset states */
            SubDataset* getPublicSubDataset() {return m_public;}

            /* \brief Get the private subdataset states 
             * \return   The private subdataset states */
            SubDataset& getPrivateSubDataset() {return m_private;}

            /* \brief  Get the public subdataset states
             * \return  The public subdataset states */
            const SubDataset* getPublicSubDataset() const {return m_public;}

            /* \brief Get the private subdataset states 
             * \return   The private subdataset states */
            const SubDataset& getPrivateSubDataset() const {return m_private;}
        private:
            SubDataset* m_public     = NULL;              /*!< The bound SubDataset public state*/
            SubDataset  m_private;                        /*!< The bound SubDataset private state*/
            int         m_visibility = VISIBILITY_PUBLIC; /*!< The visibility regarding the subdataset*/
    };
}

#endif
