#include "encodedfile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <vector>


namespace ZFecFS {


EncodedFile::~EncodedFile()
{
    close(fileHandle);
}

u_int64_t EncodedFile::Open(int requiredShares, const DecodedPath& decodedPath)
{
    int fileHandle = open(decodedPath.path.c_str(), O_RDONLY);
    if (fileHandle == -1)
        return 0;
    return reinterpret_cast<u_int64_t>(new EncodedFile(fileHandle,
                                                       requiredShares,
                                                       decodedPath.index));
}

int EncodedFile::Read(char *outBuffer, size_t size, off_t offset, const FecWrapper &fec)
{
    unsigned int required = fec.GetSharesRequired();

    // TODO metadata

    if (lseek(fileHandle, offset * required, SEEK_SET) != offset * required)
        return -errno;

    if (size > 4096) size = 4096; // TODO read larger batches, but decode them in loops

    std::vector<char> readBuffer(size * required); // TODO how to check for out of memory (also below)?

    size_t sizeRead = read(fileHandle, readBuffer.data(), size * required);
    if (sizeRead == -1u)
        return -errno;
    sizeRead = std::min(sizeRead, size * required);

    if (shareIndex < required) {
        return CopyNthElement(outBuffer, readBuffer.begin() + shareIndex,
                              readBuffer.begin() + sizeRead, required);
    } else {
        // TODO modify this if we reached EOF
        if (sizeRead % required != 0)
            sizeRead -= required - (sizeRead % required);

        // TODO pool?
        std::vector<char> workBuffer(sizeRead);

        // TODO can we have compile-time specializations for small required values?
        Distribute(workBuffer.begin(), readBuffer.begin(), readBuffer.end(), required);

        int shareSize = sizeRead / required;
        std::vector<char*> fecInputPtrs(required);
        for (unsigned int i = 0; i < required; ++i)
            fecInputPtrs[i] = workBuffer.data() + i * shareSize;

        fec.Encode(outBuffer, fecInputPtrs.data(), shareIndex, shareSize);
        return shareSize;
    }
}

template <class TOutIter, class TInIter>
size_t EncodedFile::CopyNthElement(TOutIter out, TInIter in, const TInIter end,
                                   unsigned int stride) const
{
    TOutIter outpos = out;
    while (in < end) {
        *outpos++ = *in;
        in += stride;
    }

    return outpos - out;
}

template <class TOutIter, class TInIter>
void EncodedFile::Distribute(TOutIter out, TInIter in, const TInIter end,
                             unsigned int chunks) const
{
    assert((end - in) % chunks == 0);

    int chunkSize = (end - in) / chunks;

    for (int i = 0; i < chunks; ++i)
        CopyNthElement(out + i * chunkSize, in + i, end, chunks);
}

} // namespace ZFecFS
