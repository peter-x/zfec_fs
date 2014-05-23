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

#include "fec.h"

}

static struct {
    int numShares;
    int required;
    std::string source; // must be /-terminated
    fec_t* fecData;
} zfecfsSettings;

class SimpleException : public std::exception {
public:
    SimpleException(const char* message) : message(message) {
    }

    virtual const char* what() const throw() {
        return message;
    }
private:
    const char* message;
};

class ZFecFSMetadata {
public:
    static const int Size = 3;
};

class ZFecFSEncoder {
public:
    static int Getattr(const char* path, struct stat* stbuf) {
        try {
            DecodedPath decodedPath = DecodePath(path);
            if (decodedPath.IsIndexGiven()) {
                lstat(decodedPath.path.c_str(), stbuf);
                if ((stbuf->st_mode & S_IFMT) == S_IFREG)
                    stbuf->st_size = EncodedSize(stbuf->st_size);
            } else {
                memset(stbuf, 0, sizeof(struct stat));
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = zfecfsSettings.numShares + 2;
            }
        } catch (const std::exception& exc) {
            return -ENOENT;
        }

        return 0;
    }

    static int Readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
        (void) offset;
        (void) fi;

        try {
            DecodedPath decodedPath = DecodePath(path);
            struct stat st;
            memset(&st, 0, sizeof(st));

            if (!decodedPath.IsIndexGiven()) {
                filler(buffer, ".", NULL, 0);
                filler(buffer, "..", NULL, 0);
                for (DecodedPath::ShareIndex shareIndex = 0; shareIndex < zfecfsSettings.numShares; ++shareIndex) {
                    char name[3];
                    EncodeShareIndex(shareIndex, &(name[0]));
                    // TODO provide stat
                    filler(buffer, name, NULL, 0);
                }
            } else {
                DIR* d = opendir(decodedPath.path.c_str());
                if (d == NULL) return -errno;

                if (offset != 0)
                    seekdir(d, offset);
                for (struct dirent* entry = readdir(d); entry != NULL; entry = readdir(d))
                {
                    st.st_ino = entry->d_ino;
                    st.st_mode = entry->d_type << 12;
                    st.st_size = EncodedSize(st.st_size);

                    filler(buffer, entry->d_name, &st, telldir(d));
                }
                closedir(d);
                return 0;
            }
        } catch (const std::exception& exc) {
            return -ENOENT;
        }
        return 0;
    }

private:
    static off_t EncodedSize(off_t originalSize) {
        return (originalSize + zfecfsSettings.required - 1) / zfecfsSettings.required
                + ZFecFSMetadata::Size;
    }

    // ------------------------ Path stuff ----------------------
    struct DecodedPath {
        typedef int ShareIndex;
        static const ShareIndex ShareIndexNotGiven = -1;

        DecodedPath(const std::string& path)
            : index(ShareIndexNotGiven), path(zfecfsSettings.source + path) {}
        DecodedPath(ShareIndex index, const std::string& path)
            : index(index), path(zfecfsSettings.source + path) {}

        bool IsIndexGiven() const {
            return index != ShareIndexNotGiven;
        }

        const ShareIndex index;
        const std::string path;
    };


    static DecodedPath DecodePath(const std::string& path) {
        std::string::const_iterator it = path.begin();
        while (it != path.end() && *it == '/') ++it;
        if (it == path.end())
            return DecodedPath("");

        DecodedPath::ShareIndex index = DecodeShareIndex(it, path.end());

        std::string absolutePath;
        absolutePath.reserve(zfecfsSettings.source.size() + (path.end() - it));
        absolutePath.append(zfecfsSettings.source);
        absolutePath.append(it, path.end());

        return DecodedPath(index, absolutePath);
    }

    static DecodedPath::ShareIndex DecodeShareIndex(std::string::const_iterator& pos,
                                                      std::string::const_iterator end) {
        if (end - pos < 2 || (pos + 2 != end && *(pos + 2) != '/'))
            throw SimpleException("Unable to parse share index.");
        DecodedPath::ShareIndex index = DecodeHexDigit(*pos++) << 4;
        index += DecodeHexDigit(*pos++);
        if (index < 0 || index >= zfecfsSettings.numShares)
            throw SimpleException("Invalid share index.");
        return index;
    }

    template <class Iterator>
    static void EncodeShareIndex(DecodedPath::ShareIndex index, Iterator buffer) {
        if (index < 0 || index >= zfecfsSettings.numShares)
            throw SimpleException("Invalid share index");
        *buffer++ = EncodeHexDigit((index & 0xf0) >> 4);
        *buffer++ = EncodeHexDigit(index & 0xf);
    }

    static char EncodeHexDigit(const int number) {
        if (0 <= number && number < 10) {
            return '0' + number;
        } else if (10 <= number && number <= 15) {
            return 'a' + (number - 10);
        } else {
            throw SimpleException("Invalid hex digit value.");
        }
    }

    static int DecodeHexDigit(const char digit) {
        if (digit >= '0' && digit <= '9') {
            return digit - '0';
        } else if (digit >= 'a' && digit <= 'f') {
            return digit - 'a' + 10;
        } else {
            throw SimpleException("Invalid hex digit.");
        }
    }
};

extern "C" {

static int zfecfs_getattr(const char* path, struct stat* stbuf)
{
    return ZFecFSEncoder::Getattr(path, stbuf);
}

static int zfecfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    return ZFecFSEncoder::Readdir(path, buf, filler, offset, fi);
}

static int zfecfs_open(const char *path, struct fuse_file_info *fi)
{
//    char real_path[PATH_MAX];
//    int index = decode_path(path, real_path);
//    if (index < -1) return -ENOENT;

//    // TODO try to open the real file

//    if ((fi->flags & 3) != O_RDONLY)
//        return -EACCES;

    return 0;
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

static int zfecfs_read(const char *path, char *out_buffer, size_t size, off_t offset,
              struct fuse_file_info *fi)
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
    return -ENOENT;
}


static struct fuse_operations zfecfs_operations;

// TODO make this multithreaded (for now use -s)

int main(int argc, char *argv[])
{
    zfecfs_operations.getattr = zfecfs_getattr;
    zfecfs_operations.readdir = zfecfs_readdir;
    zfecfs_operations.open = zfecfs_open;
    zfecfs_operations.read = zfecfs_read;

    zfecfsSettings.required = 3;
    zfecfsSettings.source = "/";
    zfecfsSettings.numShares = 20;
    zfecfsSettings.fecData = fec_new(zfecfsSettings.required,
                                       zfecfsSettings.numShares);
    return fuse_main(argc, argv, &zfecfs_operations, NULL);
}

}
