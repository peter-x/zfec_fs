#include "fileencoder.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <vector>

#include <boost/thread/lock_guard.hpp>

// TODO make everyting large-file-proof

namespace ZFecFS {


int FileEncoder::Read(char* const outBuffer, size_t size, off_t offset)
{
    if (size == 0) return 0;

    char* outBufferPos = outBuffer;
    FillMetadata(outBufferPos, size, offset);

    while (outBufferPos < outBuffer + size) {
        const off_t offsetInData = offset - Metadata::size + (outBufferPos - outBuffer);
        const size_t sizeWanted = outBuffer + size - outBufferPos;
        if (!FillData(outBufferPos,
                      std::min<size_t>(sizeWanted,
                                       transformBatchSize * fecWrapper.GetSharesRequired()),
                      offsetInData))
            break;
    }
    return outBufferPos - outBuffer;
}

void FileEncoder::FillMetadata(char*& outBuffer, size_t size, off_t offset)
{
    if (offset < off_t(Metadata::size)) {
        Metadata meta(fecWrapper.GetSharesRequired(), shareIndex, OriginalSize());
        for (; size > 0 && offset < off_t(Metadata::size); size --)
            *outBuffer++ = *(meta.begin() + offset++);
    }
}

bool FileEncoder::FillData(char*& outBuffer, size_t size, off_t offset)
{
    unsigned int sharesRequired = fecWrapper.GetSharesRequired();
    std::vector<char>& readBuffer(threadLocalData.Get().readBuffer);
    readBuffer.resize(size * sharesRequired);

    size_t sizeRead = file->Read(readBuffer.data(), size * sharesRequired, offset * sharesRequired);
    if (sizeRead == 0)
        return false;
    sizeRead = std::min(sizeRead, size * sharesRequired);

    sizeRead = AdjustDataSize(readBuffer, sizeRead, offset);
    assert(sizeRead % sharesRequired == 0);
    assert(sizeRead > 0);

    if (shareIndex < sharesRequired) {
        // TODO can we have compile-time specializations for small required values?
        outBuffer = CopyNthElement(outBuffer, readBuffer.begin() + shareIndex,
                                   readBuffer.begin() + shareIndex + sizeRead, sharesRequired);
    } else {
        std::vector<char>& workBuffer(threadLocalData.Get().workBuffer);
        workBuffer.resize(sizeRead);

        Distribute(workBuffer.begin(),
                   readBuffer.begin(), readBuffer.begin() + sizeRead,
                   sharesRequired);

        int shareSize = sizeRead / sharesRequired;
        std::vector<char*> fecInputPtrs(sharesRequired);
        for (unsigned int i = 0; i < sharesRequired; ++i)
            fecInputPtrs[i] = workBuffer.data() + i * shareSize;

        fecWrapper.Encode(outBuffer, fecInputPtrs.data(), shareIndex, shareSize);
        outBuffer += shareSize;
    }
    return true;
}

size_t FileEncoder::AdjustDataSize(std::vector<char>& readBuffer, size_t sizeRead, off_t offset)
{
    unsigned int sharesRequired = fecWrapper.GetSharesRequired();
    int excessBytes = sizeRead % sharesRequired;
    if (excessBytes != 0) {
        // short read, check if we are at the end of the file
        if (off_t(offset * sharesRequired + sizeRead) < OriginalSize()) {
            // not EOF, remove excess data
            sizeRead -= excessBytes;
        } else {
            // EOF, add surplus zeros
            while (sizeRead % sharesRequired != 0)
                readBuffer[sizeRead++] = 0;
        }
    }
    assert(sizeRead % sharesRequired == 0);
    return sizeRead;
}

off_t FileEncoder::OriginalSize() const
{
    boost::lock_guard<boost::mutex> lock(mutex);

    if (!originalSizeSet) {
        originalSize = file->Size();
        originalSizeSet = true;
    }
    return originalSize;
}

template <class TOutIter, class TInIter>
TOutIter FileEncoder::CopyNthElement(TOutIter out, TInIter in, const TInIter end,
                                 unsigned int stride) const
{
    while (in < end) {
        *out++ = *in;
        in += stride;
    }
    return out;
}

template <class TOutIter, class TInIter>
void FileEncoder::Distribute(TOutIter out, TInIter in, const TInIter end,
                             unsigned int chunks) const
{
    assert((end - in) % chunks == 0);

    int chunkSize = (end - in) / chunks;

    assert((end - in) == ((out + chunks * chunkSize) - out));

    for (int i = 0; i < chunks; ++i)
        CopyNthElement(out + i * chunkSize, in + i, end, chunks);
}

} // namespace ZFecFS
