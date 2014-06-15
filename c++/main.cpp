#define FUSE_USE_VERSION 26
#define _USE_GNU

#include <utility>
#include <string>
#include <exception>
#include <sstream>
#include <iostream>

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

static void ShowHelp(const std::string& firstArg)
{
    std::cout << "Usage: " << firstArg << " [-r] [-d] [-f] <required> <shares> <source> <target>" << std::endl
              << "    Creates a virtual erasure-coded mirror of the directory tree in <source> at <target>." << std::endl
              << "    A total of <shares> shares is created, and an arbitrary subset of <required> shares" << std::endl
              << "    is needed to recover it." << std::endl
              << std::endl
              << "    -r    Reverse the operation - erasure-coded data is available in <source> and the" << std::endl
              << "          decoded data will appear at <target>." << std::endl
              << "    -f    Stay in foreground." << std::endl
              << "    -d    Add debug output, implies -f." << std::endl;
}

int main(int argc, char *argv[])
{
    bool decode = false;
    unsigned int requiredShares = 0xffff;
    unsigned int numShares = 0xffff;
    std::string source;
    std::string target;

    int positionalOption = 0;
    std::vector<char*> fuseArgv;
    fuseArgv.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            ShowHelp(std::string(argv[0]));
            return 0;
        } else if (arg == "-r") {
            decode = true;
        } else if (arg == "-d" || arg == "-f") {
            fuseArgv.push_back(argv[i]);
        } else if (arg == "-o") {
            fuseArgv.push_back(argv[i]);
            ++i;
            fuseArgv.push_back(argv[i]);
        } else {
            if (positionalOption < 2) {
                std::istringstream s(arg);
                s >> (positionalOption == 0 ? requiredShares : numShares);
                if (s.fail()) {
                    ShowHelp(argv[0]);
                    return 1;
                }
            } else if (positionalOption == 2) {
                source = arg;
            } else if (positionalOption == 3) {
                target = arg;
                fuseArgv.push_back(argv[i]);
            } else {
                ShowHelp(argv[0]);
                return 1;
            }
            positionalOption++;
        }
    }
    if (positionalOption != 4
            || requiredShares > 0xff || numShares > 0xff
            || requiredShares > numShares
            || source.empty() || target.empty()) {
        ShowHelp(argv[0]);
        return 1;
    }

    if (source[source.size() - 1] != '/')
        source += "/";

    if (decode) {
        ZFecFS::globalZFecFSInstance = new ZFecFS::ZFecFSDecoder(requiredShares, numShares, source);
    } else {
        ZFecFS::globalZFecFSInstance = new ZFecFS::ZFecFSEncoder(requiredShares, numShares, source);
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

    // TODO provide some fuse arguments
    return fuse_main(fuseArgv.size(), fuseArgv.data(), &zfecfs_operations, NULL);
}

}
