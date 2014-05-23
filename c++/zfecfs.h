#ifndef ZFECFS_GLOBALS_H
#define ZFECFS_GLOBALS_H

#include <sys/types.h>

#include <string>

extern "C" {
#include <fuse.h>

#include "fec.h"

struct stat; // forward
}


namespace ZFecFS {


class ZFecFS {
protected:
    const unsigned int sharesRequired;
    const unsigned int numShares;
    const std::string source; // must be /-terminated
    fec_t* const fecData;

public:
    static ZFecFS& GetInstance();
    const std::string& GetSource() const { return source; }
    unsigned int GetNumShares() const { return numShares; }

    virtual int Getattr(const char* path, struct stat* stbuf) = 0;
    virtual int Opendir(const char* path, struct fuse_file_info* fileInfo) = 0;
    virtual int Readdir(const char* path, void* buffer, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fileInfo) = 0;
    virtual int Releasedir(const char* path, struct fuse_file_info* fileInfo) = 0;
    virtual int Open(const char* path, struct fuse_file_info* fileInfo) = 0;
    virtual int Read(const char* path, char* outBuffer, size_t size, off_t offset,
                     struct fuse_file_info* fileInfo) = 0;
    virtual int Release(const char* path, struct fuse_file_info* fileInfo) = 0;

protected:
    ZFecFS(unsigned int sharesRequired, unsigned int numShares, const std::string& source)
        : sharesRequired(sharesRequired)
        , numShares(numShares)
        , source(source)
        , fecData(fec_new(sharesRequired, numShares))
    {
    }

    virtual ~ZFecFS()
    {
        fec_free(fecData);
    }
};


} // namespaceZFecFS

#endif // ZFECFS_GLOBALS_H
