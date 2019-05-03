#ifndef  VFVBUFFERVALUE_INC
#define  VFVBUFFERVALUE_INC

#include <vector>
#include <cstdint>

namespace sereno
{
    /* \brief the VFV Buffer value being read */
    template <typename T>
    struct VFVBufferValue
    {
        VFVBufferValue(int32_t sizeT = sizeof(T)) : maxSize(sizeT)
        {
            if(sizeT >= 0)
                buffer.reserve(sizeT);
        }

        int32_t              maxSize; /*!< The maximum size for this value*/
        std::vector<uint8_t> buffer;  /*!< The current buffer*/

        /* \brief Clear the buffer */
        void clear()
        {
            buffer.clear();
        }

        /* \brief push a uint8_t value into the buffer
         * \param b the value to push */
        void pushValue(uint8_t b)
        {
            buffer.push_back(b);
        }

        /* \brief Set the desired maximum buffer size
         * \param size the desired maximum buffer size. */
        void setMaxSize(int32_t size)
        {
            maxSize = size;
            if(size >= 0)
                buffer.reserve(maxSize);
        }

        /* \brief Is the buffer full ?
         * \return true if full, false otherwise */
        bool isFull() const
        {
            return (uint32_t)(maxSize) == buffer.size();
        }

        /* \brief Get the stored value
         * \return the value */
        T getValue() {return T();}
    };

    /* \brief Specialization of uint32_t for a buffer 
     * \return a uint32_t value for a big endian number*/
    template<>
    inline uint32_t VFVBufferValue<uint32_t>::getValue()
    {
        return (buffer[0] << 24) + 
               (buffer[1] << 16) + 
               (buffer[2] << 8)  + 
               buffer[3];
    }

    template<>
    inline uint16_t VFVBufferValue<uint16_t>::getValue()
    {
        return (buffer[0] << 8) + buffer[1];
    }

    /* \brief Specialization of float for a buffer 
     * \return a float value for a big endian number*/
    template<>
    inline float VFVBufferValue<float>::getValue()
    {
        uint32_t v = (buffer[0] << 24) + 
                     (buffer[1] << 16) + 
                     (buffer[2] << 8)  + 
                     buffer[3];

        return *(float*)(&v);
    }

    /* \brief Specialization of std::string for a buffer 
     * \return the std::string buffer value*/
    template<>
    inline std::string VFVBufferValue<std::string>::getValue()
    {
        if(buffer.size() != 0)
            return std::string((char*)buffer.data(), buffer.size());
        return "";
    }
}

#endif
