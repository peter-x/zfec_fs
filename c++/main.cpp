#define FUSE_USE_VERSION 26
#define _USE_GNU

#include <utility>
#include <string>
#include <exception>

extern "C" {

#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>


}

#include <assert.h>

#include "zfecfs.h"
#include "zfecfsencoder.h"

namespace ZFecFS {

ZFecFS* globalZFecFSInstance;

ZFecFS& ZFecFS::GetInstance()
{
    assert(globalZFecFSInstance != 0);
    return *globalZFecFSInstance;
}


} // namespace ZFecFS


extern "C" {

static int zfecfs_getattr(const char* path, struct stat* stbuf)
{
    return ZFecFS::ZFecFS::GetInstance().Getattr(path, stbuf);
}

static int zfecfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    return ZFecFS::ZFecFS::GetInstance().Readdir(path, buf, filler, offset, fi);
}

static int zfecfs_open(const char *path, struct fuse_file_info *fi)
{
    return ZFecFS::ZFecFS::GetInstance().Open(path, fi);
}

size_t copy_nth_byte(char* out, const char* in, int stride, int len)
{
    char* orig_out = out;
    const char* end = in + len;
    while (in < end) {
        *out = *in;
        in += stride;
        out++;
    }
    return out - orig_out;
}

static int zfecfs_read(const char *path, char *outBuffer, size_t size, off_t offset,
              struct fuse_file_info *fileInfo)
{
    return ZFecFS::ZFecFS::GetInstance().Read(path, outBuffer, size, offset,
                                              fileInfo);
}


static struct fuse_operations zfecfs_operations;

// TODO make this multithreaded (for now use -s)

int main(int argc, char *argv[])
{
    bool decode = false;
    unsigned int required = 3;
    unsigned int numShares = 20;
    const char* source = "/";

    if (decode) {
        // TODO
    } else {
        ZFecFS::globalZFecFSInstance = new ZFecFS::ZFecFSEncoder(required, numShares, source);
    }

    zfecfs_operations.getattr = zfecfs_getattr;
    zfecfs_operations.readdir = zfecfs_readdir;
    zfecfs_operations.open = zfecfs_open;
    zfecfs_operations.read = zfecfs_read;

    return fuse_main(argc, argv, &zfecfs_operations, NULL);
}

}