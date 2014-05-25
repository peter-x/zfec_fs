#ifndef ENCODEDFILE_H
#define ENCODEDFILE_H

#include <sys/types.h>

#include "fecwrapper.h"
#include "decodedpath.h"

namespace ZFecFS {

class EncodedFile
{
public:
    ~EncodedFile();

    static u_int64_t Open(int requiredShares, const DecodedPath& shareIndex);
    static EncodedFile* FromHandle(u_int64_t handle)
    {
        return reinterpret_cast<EncodedFile*>(handle);
    }

    int Read(char* outBuffer, size_t size, off_t offset, const FecWrapper& fec);

private:
    EncodedFile(int fileHandle,
                unsigned int requiredShares,
                DecodedPath::ShareIndex shareIndex)
        : fileHandle(fileHandle)
        , requiredShares(requiredShares)
        , shareIndex(shareIndex)
    {}

    template <class TOutIter, class TInIter>
    size_t CopyNthElement(TOutIter out, TInIter in, TInIter end, unsigned int stride) const;
    template <class TOutIter, class TInIter>
    void Distribute(TOutIter out, TInIter in, const TInIter end, unsigned int chunks) const;

    const int fileHandle;
    const unsigned int requiredShares;
    const DecodedPath::ShareIndex shareIndex;
};

} // namespace ZFecFS

#endif // ENCODEDFILE_H
