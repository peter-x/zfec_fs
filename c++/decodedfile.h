#ifndef ZFECFS_DECODEDFILE_H
#define ZFECFS_DECODEDFILE_H

#include <sys/types.h>

#include <vector>
#include <string>

#include "fecwrapper.h"

namespace ZFecFS {

class DecodedFile
{
public:
    static DecodedFile* Open(const std::vector<std::string>& encodedFiles, const FecWrapper& fecWrapper);
    size_t Size() const;

    static size_t Size(std::string encodedFilePath,
                       const FecWrapper &fecWrapper);
    ~DecodedFile();
private:
    DecodedFile(const std::vector<int>& encodedFileHandles, const FecWrapper& fecWrapper)
        : encodedFileHandles(encodedFileHandles)
        , fecWrapper(fecWrapper)
    {}

    static void CloseHandles(const std::vector<int>& fileHandles);

    std::vector<int> encodedFileHandles;
    const FecWrapper& fecWrapper;
};

} // namespace ZFecFS

#endif // ZFECFS_DECODEDFILE_H
