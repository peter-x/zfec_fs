#include "encodedfile.h"

#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "unistd.h"


namespace ZFecFS {


EncodedFile::~EncodedFile()
{
    close(fileHandle);
}

u_int64_t ZFecFS::EncodedFile::Open(const DecodedPath& decodedPath)
{
    int fileHandle = open(decodedPath.path.c_str(), O_RDONLY);
    if (fileHandle == -1)
        return 0;
    return reinterpret_cast<u_int64_t>(new EncodedFile(fileHandle, decodedPath.index));
}

} // namespace ZFecFS
