#ifndef ENCODEDFILE_H
#define ENCODEDFILE_H

#include <sys/types.h>

#include <vector>

#include "fecwrapper.h"
#include "decodedpath.h"
#include "metadata.h"

namespace ZFecFS {

class EncodedFile
{
public:
    ~EncodedFile();

    static u_int64_t Open(const DecodedPath& decodedPath, const FecWrapper& fecWrapper);
    static EncodedFile* FromHandle(u_int64_t handle)
    {
        return reinterpret_cast<EncodedFile*>(handle);
    }

    int Read(char* outBuffer, size_t size, off_t offset);

    static off_t Size(off_t originalSize, int sharesRequired)
    {
        return (originalSize + sharesRequired - 1) / sharesRequired
                + Metadata::size();
    }

private:
    EncodedFile(int fileHandle,
                DecodedPath::ShareIndex shareIndex,
                const FecWrapper& fecWrapper)
        : fileHandle(fileHandle)
        , shareIndex(shareIndex)
        , fecWrapper(fecWrapper)
        , originalSize(0)
        , originalSizeSet(false)
    {
    }

    size_t AdjustDataSize(size_t sizeRead, off_t offset);
    off_t OriginalSize() const;

    template <class TOutIter, class TInIter>
    TOutIter CopyNthElement(TOutIter out, TInIter in, TInIter end, unsigned int stride) const;
    template <class TOutIter, class TInIter>
    void Distribute(TOutIter out, TInIter in, const TInIter end, unsigned int chunks) const;

    void FillMetadata(char*& outBuffer, size_t size, off_t offset);
    bool FillData(char*& outBuffer, size_t size, off_t offset);

    const static size_t transformBatchSize = 8192;

    const int fileHandle;
    const DecodedPath::ShareIndex shareIndex;

    const FecWrapper& fecWrapper;

    mutable off_t originalSize;
    mutable bool originalSizeSet;

    // TODO replace by data structure that does not initialize the data
    std::vector<char> readBuffer;
    std::vector<char> workBuffer;
};

} // namespace ZFecFS

#endif // ENCODEDFILE_H
