#ifndef METADATA_H
#define METADATA_H

#include <sys/types.h>

namespace ZFecFS {

class Metadata
{
public:
    const unsigned char required;
    const unsigned char index;
    const unsigned char excessBytes;

    static const size_t size;
public:
    explicit Metadata(const char* data)
        : required(data[0])
        , index(data[1])
        , excessBytes(data[2])
    {}

    Metadata(unsigned int required, unsigned int index, unsigned int originalLength)
        : required(required)
        , index(index)
        , excessBytes(originalLength % required)
    {}

    const char* begin() const
    {
        return reinterpret_cast<const char*>(this);
    }

    const char* end() const
    {
        return begin() + size;
    }
};

} // namespace ZFecFS

#endif // METADATA_H
