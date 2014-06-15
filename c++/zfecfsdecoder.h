#ifndef ZFECFSDECODER_H
#define ZFECFSDECODER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <string>

#include "zfecfs.h"
#include "filedecoder.h"

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
                        off_t offset, struct fuse_file_info *fileInfo);
    virtual int Releasedir(const char*, struct fuse_file_info* fileInfo);
    virtual int Open(const char* path, struct fuse_file_info* fileInfo);
    virtual int Read(const char*, char *outBuffer,
                     size_t size, off_t offset,
                     fuse_file_info *fileInfo) {
        try {
            return FromHandle(fileInfo->fh)->Read(outBuffer, size, offset);
        } catch (const std::exception& exc) {
            return -EIO;
        }
    }

    virtual int Release(const char*, fuse_file_info *fileInfo)
    {
        delete FromHandle(fileInfo->fh);
        return 0;
    }
private:
    std::string GetFirstPathMatchInAnyShare(const char* pathToFind,
                                            struct stat* statBuf = NULL);
    std::vector<std::string> GetFirstNumPathMatchesInAnyShare(
                           const char* pathToFind, unsigned int numMatches,
                           struct stat* statBuf = NULL);
    FileDecoder* CreateFileDecoder(const std::vector<File>& encodedFiles) const;
    Metadata ReadMetadata(const File& file) const;

    uint64_t ToHandle(FileDecoder* decoder) const
    {
        return reinterpret_cast<u_int64_t>(decoder);
    }
    FileDecoder* FromHandle(uint64_t handle) const
    {
        return reinterpret_cast<FileDecoder*>(handle);
    }
};

} // namespace ZFecFS

#endif // ZFECFSDECODER_H
