#ifndef ZFECFSENCODER_H
#define ZFECFSENCODER_H

#include <sys/types.h>
#include <sys/stat.h>

#include "zfecfs.h"

namespace ZFecFS {


class ZFecFSEncoder : public ZFecFS
{
public:
    ZFecFSEncoder(unsigned int sharesRequired,
                  unsigned int numShares,
                  const std::string &source);

    virtual int Getattr(const char* path, struct stat* stbuf);
    virtual int Readdir(const char* path, void* buffer, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi);
    virtual int Open(const char* path, struct fuse_file_info* fileInfo);
    virtual int Read(const char* path, char* outBuffer, size_t size, off_t offset,
                     struct fuse_file_info* fileInfo);
private:
    off_t EncodedSize(off_t originalSize) const
    {
        return (originalSize + sharesRequired - 1) / sharesRequired
                + 3;// TODO ZFecFSMetadata::Size;
    }
};


} // namespace ZFecFS

#endif // ZFECFSENCODER_H
