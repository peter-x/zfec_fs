#ifndef ENCODEDFILE_H
#define ENCODEDFILE_H

#include <sys/types.h>

#include "decodedpath.h"

namespace ZFecFS {

class EncodedFile
{
public:
    ~EncodedFile();

    static u_int64_t Open(const DecodedPath& shareIndex);
    static EncodedFile* FromHandle(u_int64_t handle)
    {
        return reinterpret_cast<EncodedFile*>(handle);
    }

private:
    EncodedFile(int fileHandle, DecodedPath::ShareIndex shareIndex)
        : fileHandle(fileHandle)
        , shareIndex(shareIndex)
    {}

    const int fileHandle;
    const DecodedPath::ShareIndex shareIndex;
};

} // namespace ZFecFS

#endif // ENCODEDFILE_H
