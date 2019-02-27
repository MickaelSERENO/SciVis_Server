#ifndef  INTERNATDATA_INC
#define  INTERNATDATA_INC

#include <memory>

namespace sereno
{
    /* \brief The internal data of the main application
     * Since the Server does not permit to passe internal data, we use this */
    class InternalData
    {
        public:

            /* \brief Init the internal data */
            static void          initSingleton();

            /* \brief Get the Internal Data singleton. Pay attention about thread !!
             * \return a pointer to the InternalData */
            static InternalData* getSingleton(); 
        private:
            /* \brief the singleton ptr */
            static std::unique_ptr<InternalData> singleton;

            /* \brief Constructor */
            InternalData();
            
            /* Explicitly disallow copying. */ 
            InternalData(const InternalData&)            = delete;
            InternalData& operator=(const InternalData&) = delete;
    };
}

#endif
