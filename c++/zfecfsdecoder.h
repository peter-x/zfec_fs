#ifndef ZFECFSDECODER_H
#define ZFECFSDECODER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <string>

#include "zfecfs.h"

namespace ZFecFS {

class ZFecFSDecoder : public ZFecFS
{
public:
    ZFecFSDecoder(unsigned int sharesRequired,
                  unsigned int numShares,
                  const std::string &source)
    : ZFecFS(sharesRequired, numShares, source)
    {}

    virtual int Getattr(const char* path, struct stat* stbuf);
    virtual int Opendir(const char* path, struct fuse_file_info* fileInfo);
    virtual int Readdir(const char*, void* buffer, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fileInfo)
    { return -ENOENT; /* TODO */ }
    virtual int Releasedir(const char*, struct fuse_file_info* fileInfo)
    { return -ENOENT; /* TODO */ }
    virtual int Open(const char* path, struct fuse_file_info* fileInfo)
    { return -ENOENT; /* TODO */ }
    virtual int Read(const char*, char *outBuffer,
                     size_t size, off_t offset,
                     fuse_file_info *fileInfo)
    { return -ENOENT; /* TODO */ }
    virtual int Release(const char*, fuse_file_info *fileInfo)
    { return -ENOENT; /* TODO */ }
private:
    /// @note statBuf is optional
    std::string GetFirstPathMatchInAnyShare(const char* pathToFind, struct stat* statBuf);
};

} // namespace ZFecFS

#endif // ZFECFSDECODER_H
