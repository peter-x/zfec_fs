#ifndef ZFECFSENCODER_H
#define ZFECFSENCODER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <exception>

#include "zfecfs.h"
#include "fileencoder.h"

namespace ZFecFS {


class ZFecFSEncoder : public ZFecFS
{
public:
    ZFecFSEncoder(unsigned int sharesRequired,
                  unsigned int numShares,
                  const std::string &source)
    : ZFecFS(sharesRequired, numShares, source)
    {}

    virtual int Getattr(const char* path, struct stat* stbuf);
    virtual int Opendir(const char* path, struct fuse_file_info* fileInfo);
    virtual int Readdir(const char* path, void* buffer, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fileInfo);
    virtual int Releasedir(const char* path, struct fuse_file_info* fileInfo);
    virtual int Open(const char* path, struct fuse_file_info* fileInfo);
    virtual int Read(const char*, char *outBuffer,
                     size_t size, off_t offset,
                     fuse_file_info *fileInfo)
    {
        try {
            return FromHandle(fileInfo->fh)->Read(outBuffer, size, offset);
        } catch (const std::exception& exc) {
            return -EIO;
        }
    }
    virtual int Release(const char*, fuse_file_info *fileInfo)
    {
        delete FromHandle(fileInfo->fh);
        fileInfo->fh = 0;
        return 0;
    }
private:
    uint64_t ToHandle(FileEncoder* encoder) const
    {
        return reinterpret_cast<uint64_t>(encoder);
    }
    FileEncoder* FromHandle(uint64_t handle) const
    {
        return reinterpret_cast<FileEncoder*>(handle);
    }
};


} // namespace ZFecFS

#endif // ZFECFSENCODER_H
