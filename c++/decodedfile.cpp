#include "decodedfile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "utils.h"
#include "unistd.h"

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
    std::vector<int> fileHandles;
    for (unsigned int i = 0; i < originalFiles.size(); ++i) {
        int handle = open(originalFiles[i].c_str(), O_RDONLY);
        if (handle == -1) {
            CloseHandles(fileHandles);
            throw SimpleException("Erorr opening original file.");
        }
    }

    return new DecodedFile(fileHandles, fecWrapper);
}

size_t DecodedFile::Size()
{
    return 0; // TODO
}

} // namespace ZFecFS
