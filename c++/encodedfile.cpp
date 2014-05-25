#include "encodedfile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <vector>

// TODO make everyting large-file-proof

namespace ZFecFS {


EncodedFile::~EncodedFile()
{
    close(fileHandle);
}

u_int64_t EncodedFile::Open(const DecodedPath& decodedPath, const FecWrapper& fecWrapper)
{
    int fileHandle = open(decodedPath.path.c_str(), O_RDONLY);
    if (fileHandle == -1)
        return 0;
    return reinterpret_cast<u_int64_t>(new EncodedFile(fileHandle,
                                                       decodedPath.index,
                                                       fecWrapper));
}

int EncodedFile::Read(char* const outBuffer, size_t size, off_t offset)
{
    if (size == 0) return 0;

    char* outBufferPos = outBuffer;
    FillMetadata(outBufferPos, size, offset);

    while (outBufferPos < outBuffer + size) {
        char* const outBufferPosBeforeFill = outBufferPos;
        size_t sizeWanted = outBuffer + size - outBufferPos;
        if (!FillData(outBufferPos,
                      std::min<size_t>(sizeWanted,
                                       transformBatchSize * fecWrapper.GetSharesRequired()),
                      offset))
            break;
        offset += outBufferPos - outBufferPosBeforeFill;
    }
    return outBufferPos - outBuffer;
}

void EncodedFile::FillMetadata(char*& outBuffer, size_t size, off_t& offset)
{
    if (offset < off_t(Metadata::size())) {
        Metadata meta(fecWrapper.GetSharesRequired(), shareIndex, OriginalSize());
        while (offset < off_t(Metadata::size())) {
            if (size == 0) {
                offset = 0;
                return;
            }
            *outBuffer++ = *(meta.begin() + offset++);
            size--;
        }
    }
    offset -= Metadata::size();
}

bool EncodedFile::FillData(char*& outBuffer, size_t size, off_t offset)
{
    unsigned int sharesRequired = fecWrapper.GetSharesRequired();
    if (lseek(fileHandle, offset * sharesRequired, SEEK_SET)
            != offset * sharesRequired)
        return -errno;

    // TODO how to check for out of memory?
    readBuffer.resize(size * sharesRequired);

    size_t sizeRead = read(fileHandle, readBuffer.data(), size * sharesRequired);
    if (sizeRead == -1u || sizeRead == 0)
        return false;
    sizeRead = std::min(sizeRead, size * sharesRequired);

    sizeRead = AdjustDataSize(sizeRead, offset);

    if (shareIndex < sharesRequired) {
        outBuffer = CopyNthElement(outBuffer, readBuffer.begin() + shareIndex,
                                   readBuffer.begin() + shareIndex + sizeRead, sharesRequired);
    } else {
        workBuffer.resize(sizeRead);

        // TODO can we have compile-time specializations for small required values?
        Distribute(workBuffer.begin(), readBuffer.begin(), readBuffer.end(),
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

size_t EncodedFile::AdjustDataSize(size_t sizeRead, off_t offset)
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
    return sizeRead;
}

off_t EncodedFile::OriginalSize() const
{
    if (!originalSizeSet) {
        originalSize = lseek(fileHandle, 0, SEEK_END);
        if (originalSize == static_cast<off_t>(-1))
            throw SimpleException("Error seeking to end of file.");
        originalSizeSet = true;
    }
    return originalSize;
}

template <class TOutIter, class TInIter>
TOutIter EncodedFile::CopyNthElement(TOutIter out, TInIter in, const TInIter end,
                                 unsigned int stride) const
{
    while (in < end) {
        *out++ = *in;
        in += stride;
    }
    return out;
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
