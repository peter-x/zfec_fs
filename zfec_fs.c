#define FUSE_USE_VERSION 26
#define _USE_GNU

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

#include "zfec/fec.h"

static struct {
    int num_shares;
    int required;
    const char *root; // must be /-terminated
    fec_t* fec_data;
} zfecfs_settings;


static int decode_hex_byte(const char* data)
{
    int v = 0;
    int i = 0;
    for (i = 0; i < 2; i ++) {
        if ('0' <= data[i] && data[i] <= '9') {
            v = (v << 4) | (data[i] - '0');
        } else if ('a' <= data[i] && data[i] <= 'f') {
            v = (v << 4) | (data[i] - 'a' + 10);
        } else {
            return -1;
        }
    }
    return v;
}

static void encode_hex_byte(int byte, char* buf)
{
    int i = 0;
    for (i = 0; i < 2; i ++) {
        int v = byte & 0xf;
        if (0 <= v && v <= 9) {
            buf[1 - i] = '0' + v;
        } else {
            buf[1 - i] = 'a' + (v - 10);
        }
        byte >>= 4;
    }
    buf[2] = 0;
}

/// Decodes given path into storage index plus real path.
/// Returns the index or -1 if no index is given but the path is syntactically
/// valid and -2 if the index is invalid.
/// real_path points to a buffer of size at least PATH_MAX.
static int decode_path(const char* path, char* real_path)
{
    strncpy(real_path, zfecfs_settings.root, PATH_MAX);
    real_path[PATH_MAX - 1] = 0;

    while (*path == '/') path++;
    if (*path == 0) return -1;

    int index = decode_hex_byte(path);
    if (index < 0) return -2;

    path += 2;
    if (*path == '/') path++;

    if (strlen(zfecfs_settings.root) + strlen(path) >= PATH_MAX)
        return -2;

    strcpy(real_path, zfecfs_settings.root);
    strcpy(real_path + strlen(zfecfs_settings.root), path);
    return index;
}

static int zfecfs_getattr(const char *path, struct stat *stbuf)
{
    char real_path[PATH_MAX];

    int index = decode_path(path, real_path);
    if (index < -1) return -ENOENT;

    memset(stbuf, 0, sizeof(struct stat));
    if (index == -1) {
        // TODO use root of filesystem?
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 255; // TODO
    } else {
        // TODO correct the size
        stat(real_path, stbuf);
    }

    return 0;
}

static int zfecfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    char real_path[PATH_MAX];
    int index = decode_path(path, real_path);
    if (index < -1) return -ENOENT;

    (void) offset;
    (void) fi;

    if (index == -1) {
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);
        int i = 0;
        for (i = 0; i < zfecfs_settings.num_shares; i ++) {
            char name[3];
            encode_hex_byte(i, name);
            filler(buf, name, NULL, 0);
        }
    } else {
        DIR* d = opendir(real_path);
        if (d == NULL) return 0;

        struct dirent* entry = readdir(d);
        while (entry != NULL) {
            filler(buf, entry->d_name, NULL, 0);
            entry = readdir(d);
        }
        closedir(d);
        return 0;
    }

    return 0;
}

static int zfecfs_open(const char *path, struct fuse_file_info *fi)
{
    char real_path[PATH_MAX];
    int index = decode_path(path, real_path);
    if (index < -1) return -ENOENT;

    // TODO try to open the real file

    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

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
    char real_path[PATH_MAX];
    int index = decode_path(path, real_path);
    if (index < 0) return -ENOENT;

    (void) fi;

    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;

    int required = zfecfs_settings.required;
    if (lseek(fd, offset * required, SEEK_SET) != offset * required) {
        close(fd);
        return -errno;
    }

    //if (size > 4096) size = 4096;
    char *read_buffer = (char*) malloc(size * required);
    if (read_buffer == NULL) {
        close(fd);
        return -errno;
    }

    size_t size_read = read(fd, read_buffer, size * required);
    if (size_read == -1) {
        free(read_buffer);
        close(fd);
        return -errno;
    }

    int ret = -1;
    if (index < required) {
        ret = copy_nth_byte(out_buffer, read_buffer + index, required, size_read - index);
    } else {
        char* work_buffer = (char*) malloc(size_read * required);
        char** fec_input = (char**) malloc(required * sizeof(char*)); 
        if (work_buffer == NULL || fec_input == NULL) {
            free(fec_input);
            free(work_buffer);
            free(read_buffer);
            close(fd);
            return -errno;
        }

        // TODO do this correction only if we did not reach EOF
        if (size_read % required != 0)
            size_read -= required - (size_read % required);
        unsigned int packet_size = size_read / required;
        int i = 0;
        for (i = 0; i < required; i ++) {
            fec_input[i] = work_buffer + i * packet_size;
            int num = copy_nth_byte(fec_input[i],
                                    read_buffer + i,
                                    required,
                                    size_read);
            while (num < packet_size) fec_input[i][num++] = 0;
        }
        fec_encode(zfecfs_settings.fec_data,
                   (const gf * const * const) fec_input,
                   (gf * const* const)&out_buffer,
                   (unsigned int*) &index,
                   1,
                   packet_size);
        ret = packet_size;

        free(fec_input);
        free(work_buffer);
    }

    free(read_buffer);
    close(fd);
    return ret;
}


static struct fuse_operations zfecfs_oper = {
    .getattr    = zfecfs_getattr,
    .readdir    = zfecfs_readdir,
    .open       = zfecfs_open,
    .read       = zfecfs_read,
};

// TODO make this multithreaded (for now use -s)

int main(int argc, char *argv[])
{
    zfecfs_settings.required = 3;
    zfecfs_settings.root = "/";
    zfecfs_settings.num_shares = 20;
    zfecfs_settings.fec_data = fec_new(zfecfs_settings.required,
                                       zfecfs_settings.num_shares);
    return fuse_main(argc, argv, &zfecfs_oper, NULL);
}

