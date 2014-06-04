#include "decodedfile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "utils.h"
#include "unistd.h"

#include "metadata.h"


namespace ZFecFS {

DecodedFile::~DecodedFile()
{
    CloseHandles(originalFileHandles);
    originalFileHandles.clear();
}

void DecodedFile::CloseHandles(const std::vector<int>& fileHandles)
{
    for (unsigned int i = 0; i < fileHandles.size(); ++i)
        close(fileHandles[i]);
}

DecodedFile* DecodedFile::Open(const std::vector<std::string>& originalFiles,
                               const FecWrapper &fecWrapper)
{
    if (originalFiles.size() < fecWrapper.GetSharesRequired()
                            || fecWrapper.GetSharesRequired() < 1)
        throw SimpleException("Not enough encoded files.");

    std::vector<int> fileHandles;
    for (unsigned int i = 0; i < originalFiles.size(); ++i) {
        int handle = open(originalFiles[i].c_str(), O_RDONLY);
        if (handle == -1) {
            CloseHandles(fileHandles);
            throw SimpleException("Erorr opening original file.");
        }
        fileHandles.push_back(handle);
    }

    return new DecodedFile(fileHandles, fecWrapper);
}

size_t DecodedFile::Size() const
{
    struct stat statBuf;
    if (fstat(originalFileHandles.front(), &statBuf) == -1)
        throw SimpleException("Cannot stat first file.");

    char buffer[Metadata::size];
    size_t sizeRead = pread(originalFileHandles.front(), buffer, 3, 0);
    if (sizeRead != 3)
        throw SimpleException("Size cannot be read from file.");
    Metadata metadata(buffer);

    const size_t extraSize = Metadata::size + (metadata.excessBytes == 0 ? 0 : 1);
    if (statBuf.st_size < extraSize)
        throw SimpleException("Invalid encoded file.");
    return (statBuf.st_size - extraSize) * fecWrapper.GetSharesRequired() + metadata.excessBytes;
}

} // namespace ZFecFS
