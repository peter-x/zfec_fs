#ifndef ZFECFS_DECODEDFILE_H
#define ZFECFS_DECODEDFILE_H

#include <sys/types.h>

#include <vector>
#include <string>
#include <map>

#include "mutex.h"
#include "fecwrapper.h"
#include "metadata.h"
#include "file.h"
#include "threadlocalizer.h"

namespace ZFecFS {

class DecodedFile
{
public:
    static DecodedFile* Open(const std::vector<std::string>& encodedFiles,
                             const FecWrapper& fecWrapper);
    off_t Size() const;

    static off_t Size(const std::string& encodedFilePath);

    int Read(char* outBuffer, size_t size, off_t offset);
private:
    DecodedFile(const std::vector<File>& encodedFiles,
                const std::vector<unsigned char>& fileIndices,
                Metadata metadata,
                size_t encodedFileSize,
                const FecWrapper& fecWrapper)
        : encodedFiles(encodedFiles)
        , fileIndices(fileIndices)
        , metadata(metadata)
        , encodedFileSize(encodedFileSize)
        , fecWrapper(fecWrapper)
    { }

    class Constructor {
    public:
        Constructor(const std::vector<File>& encodedFiles,
                    const FecWrapper& fecWrapper)
            : encodedFiles(encodedFiles)
            , fecWrapper(fecWrapper) {}
        DecodedFile* CreateDecodedFile();
    private:
        Metadata ReadMetadataOfOneFile(unsigned int index) const;
        std::vector<File> encodedFiles;
        const FecWrapper& fecWrapper;
    };

    class ThreadLocalData {
    public:
        // TODO replace by data structure that does not initialize the data
        std::vector<std::vector<char> > readBuffers;
        std::vector<char> workBuffer;
    };

    ThreadLocalizer<ThreadLocalData> threadLocalData;

    void NormalizeIndices(std::vector<const char*>& fecInputPtrs, std::vector<unsigned int>& indices);
    template <class TOutIter, class TInIter>
    TOutIter CopyToNthElement(TOutIter out, TOutIter outEnd, TInIter in, unsigned int stride) const;
    static off_t Size(const Metadata& metadata, off_t encodedSize);

    const std::vector<File> encodedFiles;
    const std::vector<unsigned char> fileIndices;
    const Metadata metadata;
    const size_t encodedFileSize;
    const FecWrapper& fecWrapper;
};

} // namespace ZFecFS

#endif // ZFECFS_DECODEDFILE_H
