#ifndef ZFECFSENCODER_H
#define ZFECFSENCODER_H

#include <sys/types.h>
#include <sys/stat.h>

#include "zfecfs.h"
#include "encodedfile.h"

namespace ZFecFS {


class ZFecFSEncoder : public ZFecFS
{
public:
    ZFecFSEncoder(unsigned int sharesRequired,
                  unsigned int numShares,
                  const std::string &source);

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
        return EncodedFile::FromHandle(fileInfo->fh)->Read(outBuffer, size, offset, GetFecWrapper());
    }
    virtual int Release(const char*, fuse_file_info *fileInfo)
    {
        delete EncodedFile::FromHandle(fileInfo->fh);
        return 0;
    }

private:
    off_t EncodedSize(off_t originalSize) const
    {
        return (originalSize + sharesRequired - 1) / sharesRequired
                + 3;// TODO ZFecFSMetadata::Size;
    }
};


} // namespace ZFecFS

#endif // ZFECFSENCODER_H
