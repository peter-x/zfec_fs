#include "zfecfsencoder.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include "utils.h"
#include "decodedpath.h"
#include "encodedfile.h"

namespace ZFecFS {

class OpenOriginalFile {
    int fileHandle;
    DecodedPath::ShareIndex shareIndex;
};

ZFecFSEncoder::ZFecFSEncoder(unsigned int sharesRequired,
                             unsigned int numShares,
                             const std::string& source)
    : ZFecFS(sharesRequired, numShares, source)
{
}

int ZFecFSEncoder::Getattr(const char* path, struct stat* stbuf)
{
    try {
        DecodedPath decodedPath = DecodedPath::DecodePath(path, GetSource());
        if (decodedPath.indexGiven) {
            if (lstat(decodedPath.path.c_str(), stbuf) == -1)
                return -errno;
            if ((stbuf->st_mode & S_IFMT) == S_IFREG)
                stbuf->st_size = EncodedSize(stbuf->st_size);
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
    try {
        DecodedPath decodedPath = DecodedPath::DecodePath(path, GetSource());
        fileInfo->keep_cache = 1;
        fileInfo->fh = 0;
        if (decodedPath.indexGiven) {
            fileInfo->fh = reinterpret_cast<uint64_t>(opendir(decodedPath.path.c_str()));
            if (fileInfo->fh == 0)
                return -errno;
        }
    } catch (const std::exception& exc) {
        return -ENOENT;
    }
    return 0;
}

int ZFecFSEncoder::Readdir(const char* path, void* buffer, fuse_fill_dir_t filler,
                           off_t offset, fuse_file_info* fileInfo)
{

    try {
        DecodedPath decodedPath = DecodedPath::DecodePath(path, GetSource());
        struct stat st;
        memset(&st, 0, sizeof(st));

        if (!decodedPath.indexGiven) {
            filler(buffer, ".", NULL, 0);
            filler(buffer, "..", NULL, 0);
            for (DecodedPath::ShareIndex shareIndex = 0; shareIndex < numShares; ++shareIndex) {
                char name[3];
                DecodedPath::EncodeShareIndex(shareIndex, &(name[0]));
                // TODO provide stat
                filler(buffer, name, NULL, 0);
            }
        } else {
            DIR* d = reinterpret_cast<DIR*>(fileInfo->fh);
            if (d == NULL) return -errno;

            if (offset != 0)
                seekdir(d, offset);
            for (struct dirent* entry = readdir(d); entry != NULL; entry = readdir(d))
            {
                st.st_ino = entry->d_ino;
                st.st_mode = entry->d_type << 12;
                st.st_size = EncodedSize(st.st_size);

                if (filler(buffer, entry->d_name, &st, telldir(d)) == 1)
                    break;
            }
        }
    } catch (const std::exception& exc) {
        return -ENOENT;
    }
    return 0;
}

int ZFecFSEncoder::Releasedir(const char *path, fuse_file_info *fileInfo)
{
    (void) path;
    if (fileInfo->fh != 0) {
        DIR* d = reinterpret_cast<DIR*>(fileInfo->fh);
        closedir(d);
    }
    return 0;
}

int ZFecFSEncoder::Open(const char* path, fuse_file_info* fileInfo)
{
    // TODO which flags in fileInfo do we want to set?

    try {
        DecodedPath decodedPath = DecodedPath::DecodePath(path, GetSource());

        if ((fileInfo->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;

        try {
            fileInfo->fh = EncodedFile::Open(decodedPath);
        } catch (const std::exception& exc) {
            return -errno;
        }
    } catch (const std::exception& exc) {
        return -ENOENT;
    }
    return 0;
}

int ZFecFSEncoder::Read(const char *path, char *outBuffer,
                        size_t size, off_t offset,
                        fuse_file_info *fileInfo)
{
    //    char real_path[PATH_MAX];
    //    int index = decode_path(path, real_path);
    //    if (index < 0) return -ENOENT;

    //    (void) fi;

    //    int fd = open(real_path, O_RDONLY);
    //    if (fd == -1) return -errno;

    //    int required = zfecfsSettings.required;
    //    if (lseek(fd, offset * required, SEEK_SET) != offset * required) {
    //        close(fd);
    //        return -errno;
    //    }

    //    //if (size > 4096) size = 4096;
    //    char *read_buffer = (char*) malloc(size * required);
    //    if (read_buffer == NULL) {
    //        close(fd);
    //        return -errno;
    //    }

    //    size_t size_read = read(fd, read_buffer, size * required);
    //    if (size_read == -1) {
    //        free(read_buffer);
    //        close(fd);
    //        return -errno;
    //    }

    //    int ret = -1;
    //    if (index < required) {
    //        ret = copy_nth_byte(out_buffer, read_buffer + index, required, size_read - index);
    //    } else {
    //        char* work_buffer = (char*) malloc(size_read * required);
    //        char** fec_input = (char**) malloc(required * sizeof(char*));
    //        if (work_buffer == NULL || fec_input == NULL) {
    //            free(fec_input);
    //            free(work_buffer);
    //            free(read_buffer);
    //            close(fd);
    //            return -errno;
    //        }

    //        // TODO do this correction only if we did not reach EOF
    //        if (size_read % required != 0)
    //            size_read -= required - (size_read % required);
    //        unsigned int packet_size = size_read / required;
    //        int i = 0;
    //        for (i = 0; i < required; i ++) {
    //            fec_input[i] = work_buffer + i * packet_size;
    //            int num = copy_nth_byte(fec_input[i],
    //                                    read_buffer + i,
    //                                    required,
    //                                    size_read);
    //            while (num < packet_size) fec_input[i][num++] = 0;
    //        }
    //        fec_encode(zfecfsSettings.fecData,
    //                   (const gf * const * const) fec_input,
    //                   (gf * const* const)&out_buffer,
    //                   (unsigned int*) &index,
    //                   1,
    //                   packet_size);
    //        ret = packet_size;

    //        free(fec_input);
    //        free(work_buffer);
    //    }

    //    free(read_buffer);
    //    close(fd);
    //    return ret;
    //    return -ENOENT;
}

int ZFecFSEncoder::Release(const char *path, fuse_file_info *fileInfo)
{
    delete EncodedFile::FromHandle(fileInfo->fh);
}


} // namespace ZFecFS
