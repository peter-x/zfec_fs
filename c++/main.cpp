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
#include "zfecfsdecoder.h"

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

static int zfecfs_opendir(const char *path, struct fuse_file_info *fileInfo)
{
    return ZFecFS::ZFecFS::GetInstance().Opendir(path, fileInfo);
}

static int zfecfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fileInfo)
{
    return ZFecFS::ZFecFS::GetInstance().Readdir(path, buf, filler, offset, fileInfo);
}

static int zfecfs_releasedir(const char *path, struct fuse_file_info *fileInfo)
{
    return ZFecFS::ZFecFS::GetInstance().Releasedir(path, fileInfo);
}

static int zfecfs_open(const char *path, struct fuse_file_info *fileInfo)
{
    return ZFecFS::ZFecFS::GetInstance().Open(path, fileInfo);
}

static int zfecfs_read(const char* path, char* outBuffer, size_t size, off_t offset,
              struct fuse_file_info* fileInfo)
{
    return ZFecFS::ZFecFS::GetInstance().Read(path, outBuffer, size, offset,
                                              fileInfo);
}

static int zfecfs_release(const char* path, struct fuse_file_info* fileInfo)
{
    return ZFecFS::ZFecFS::GetInstance().Release(path, fileInfo);
}

static struct fuse_operations zfecfs_operations;

// TODO make this multithreaded (for now use -s)

int main(int argc, char *argv[])
{
    bool decode = true;
    unsigned int required = 3;
    unsigned int numShares = 20;
    const char* source = "/tmp/x/";///tmp/enc/";///tmp/x/"; // TODO has to be /-terminated!

    if (decode) {
        ZFecFS::globalZFecFSInstance = new ZFecFS::ZFecFSDecoder(required, numShares, source);
    } else {
        ZFecFS::globalZFecFSInstance = new ZFecFS::ZFecFSEncoder(required, numShares, source);
    }

    zfecfs_operations.getattr = zfecfs_getattr;

    zfecfs_operations.opendir = zfecfs_opendir;
    zfecfs_operations.readdir = zfecfs_readdir;
    zfecfs_operations.releasedir = zfecfs_releasedir;

    zfecfs_operations.open = zfecfs_open;
    zfecfs_operations.read = zfecfs_read;
    zfecfs_operations.release = zfecfs_release;

    zfecfs_operations.flag_nopath = 1;
    zfecfs_operations.flag_nullpath_ok = 1;

    return fuse_main(argc, argv, &zfecfs_operations, NULL);
}

}
