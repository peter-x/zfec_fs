#ifndef ZFECFS_DECODEDFILE_H
#define ZFECFS_DECODEDFILE_H

#include <sys/types.h>

#include <vector>
#include <string>

#include "fecwrapper.h"
#include "metadata.h"
#include "file.h"

namespace ZFecFS {

class DecodedFile
{
public:
    static DecodedFile* Open(const std::vector<std::string>& encodedFiles,
                             const FecWrapper& fecWrapper);
    size_t Size() const;

    static size_t Size(const std::string& encodedFilePath);

    int Read(char* outBuffer, size_t size, off_t offset);
private:
    DecodedFile(const std::vector<File>& encodedFiles, Metadata metadata,
                size_t encodedFileSize, const FecWrapper& fecWrapper)
        : encodedFiles(encodedFiles)
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

    static size_t Size(const Metadata& metadata, size_t encodedSize);

    const std::vector<File> encodedFiles;
    const Metadata metadata;
    const size_t encodedFileSize;
    const FecWrapper& fecWrapper;
};

} // namespace ZFecFS

#endif // ZFECFS_DECODEDFILE_H
