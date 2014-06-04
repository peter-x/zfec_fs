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
    static DecodedFile* Open(const std::vector<std::string>& originalFiles, const FecWrapper& fecWrapper);
    size_t Size() const;

    ~DecodedFile();
private:
    DecodedFile(const std::vector<int>& originalFileHandles, const FecWrapper& fecWrapper)
        : originalFileHandles(originalFileHandles)
        , fecWrapper(fecWrapper)
    {}

    static void CloseHandles(const std::vector<int>& fileHandles);

    std::vector<int> originalFileHandles;
    const FecWrapper& fecWrapper;
};

} // namespace ZFecFS

#endif // ZFECFS_DECODEDFILE_H
