#ifndef ENCODEDFILE_H
#define ENCODEDFILE_H

#include <sys/types.h>
#include <pthread.h>

#include <vector>
#include <map>

#include "fecwrapper.h"
#include "decodedpath.h"
#include "metadata.h"
#include "file.h"
#include "threadlocalizer.h"

namespace ZFecFS {

class FileEncoder
{
public:
    FileEncoder(const boost::shared_ptr<AbstractFile>& file,
                DecodedPath::ShareIndex shareIndex,
                const FecWrapper& fecWrapper)
        : file(file)
        , shareIndex(shareIndex)
        , fecWrapper(fecWrapper)
        , originalSize(0)
        , originalSizeSet(false)
    {
    }

    int Read(char* outBuffer, size_t size, off_t offset);

    static off_t Size(off_t originalSize, int sharesRequired)
    {
        return (originalSize + sharesRequired - 1) / sharesRequired
                + Metadata::size;
    }

private:
    size_t AdjustDataSize(std::vector<char>& readBuffer, size_t sizeRead, off_t offset);
    off_t OriginalSize() const;

    template <class TOutIter, class TInIter>
    TOutIter CopyNthElement(TOutIter out, TInIter in, TInIter end, unsigned int stride) const;
    template <class TOutIter, class TInIter>
    void Distribute(TOutIter out, TInIter in, const TInIter end, unsigned int chunks) const;

    void FillMetadata(char*& outBuffer, size_t size, off_t offset);
    bool FillData(char*& outBuffer, size_t size, off_t offset);

    const static size_t transformBatchSize = 8192;

    const boost::shared_ptr<AbstractFile> file;
    const DecodedPath::ShareIndex shareIndex;

    class ThreadLocalData {
    public:
        // TODO replace by data structure that does not initialize the data
        std::vector<char> readBuffer;
        std::vector<char> workBuffer;
    };

    ThreadLocalizer<ThreadLocalData> threadLocalData;

    const FecWrapper& fecWrapper;

    mutable boost::mutex mutex;
    mutable off_t originalSize;
    mutable bool originalSizeSet;
};

} // namespace ZFecFS

#endif // ENCODEDFILE_H
