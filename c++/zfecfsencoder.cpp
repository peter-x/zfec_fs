#include "zfecfsencoder.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include <pthread.h>

#include "utils.h"
#include "decodedpath.h"
#include "shareddir.h"
#include "encodedfile.h"

namespace ZFecFS {


int ZFecFSEncoder::Getattr(const char* path, struct stat* stbuf)
{
    try {
        DecodedPath decodedPath = DecodedPath::DecodePath(path, GetSource());
        if (decodedPath.indexGiven) {
            if (lstat(decodedPath.path.c_str(), stbuf) == -1)
                return -errno;
            if ((stbuf->st_mode & S_IFMT) == S_IFREG)
                stbuf->st_size = EncodedFile::Size(stbuf->st_size, sharesRequired);
        } else {
            memset(stbuf, 0, sizeof(struct stat));
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = numShares + 2;
        }
    } catch (const std::exception& exc) {
        return -ENOENT;
    }

    return 0;
}

int ZFecFSEncoder::Opendir(const char* path, fuse_file_info* fileInfo)
{
    fileInfo->keep_cache = 1;
    fileInfo->fh = 0;
    try {
        DecodedPath decodedPath = DecodedPath::DecodePath(path, GetSource());
        if (decodedPath.indexGiven) {
            fileInfo->fh = SharedDir::Open(decodedPath);
            if (fileInfo->fh == 0)
                return -errno;
        }
    } catch (const std::exception& exc) {
        return -ENOENT;
    }
    return 0;
}

int ZFecFSEncoder::Readdir(const char*, void* buffer, fuse_fill_dir_t filler,
                           off_t offset, fuse_file_info* fileInfo)
{
    try {
        struct stat st;
        memset(&st, 0, sizeof(st));

        if (fileInfo->fh == 0) {
            filler(buffer, ".", NULL, 0);
            filler(buffer, "..", NULL, 0);
            for (DecodedPath::ShareIndex shareIndex = 0; shareIndex < numShares; ++shareIndex) {
                char name[3];
                DecodedPath::EncodeShareIndex(shareIndex, &(name[0]));
                // TODO provide stat
                filler(buffer, name, NULL, 0);
            }
        } else {
            SharedDir& dir = *SharedDir::FromHandle(fileInfo->fh);
            Mutex::Lock lock(dir.GetMutex());

            if (offset != 0)
                dir.Seek(offset);

            for (struct dirent* entry = dir.Readdir(); entry != NULL; entry = dir.Readdir()) {
                st.st_ino = entry->d_ino;
                st.st_mode = entry->d_type << 12;
                st.st_size = EncodedFile::Size(st.st_size, sharesRequired);

                if (filler(buffer, entry->d_name, &st, dir.Telldir()) == 1)
                    break;
            }
        }
    } catch (const std::exception& exc) {
        return -ENOENT;
    }
    return 0;
}

int ZFecFSEncoder::Releasedir(const char*, fuse_file_info *fileInfo)
{
    if (fileInfo->fh != 0) {
        delete SharedDir::FromHandle(fileInfo->fh);
        fileInfo->fh = 0;
    }
    return 0;
}

int ZFecFSEncoder::Open(const char* path, fuse_file_info* fileInfo)
{
    fileInfo->keep_cache = 1;

    try {
        DecodedPath decodedPath = DecodedPath::DecodePath(path, GetSource());

        if ((fileInfo->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;

        try {
            fileInfo->fh = EncodedFile::Open(decodedPath, GetFecWrapper());
        } catch (const std::exception& exc) {
            return -errno;
        }
    } catch (const std::exception& exc) {
        return -ENOENT;
    }
    return 0;
}

} // namespace ZFecFS
