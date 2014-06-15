#include "zfecfsdecoder.h"

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include <string>
#include <algorithm>
#include <tr1/unordered_set>

#include "directory.h"
#include "decodedfile.h"
#include "utils.h"

namespace ZFecFS {

int ZFecFSDecoder::Getattr(const char *path, struct stat *stbuf)
{
    if (path[0] != '/') return -ENOENT;

    try {
        std::string realPath = GetFirstPathMatchInAnyShare(path, stbuf);
        if (S_ISREG(stbuf->st_mode))
            stbuf->st_size = DecodedFile::Size(realPath);
    } catch (const std::exception& exc) {
        return -ENOENT;
    }

    return 0;
}

int ZFecFSDecoder::Opendir(const char* path, fuse_file_info* fileInfo)
{
    if (path[0] != '/') return -ENOENT;

    try {
        // check whether the directory exists in at least one share
        GetFirstPathMatchInAnyShare(path);

        fileInfo->keep_cache = 1;
        fileInfo->fh = reinterpret_cast<uint64_t>(new std::string(path));
    } catch (const std::exception& exc) {
        return -ENOENT;
    }

    return 0;
}

int ZFecFSDecoder::Readdir(const char*, void* buffer, fuse_fill_dir_t filler,
                           off_t /*offset*/, fuse_file_info* fileInfo)
{
    const std::string& path = *reinterpret_cast<std::string*>(fileInfo->fh);

    Directory sourceDir(GetSource());

    std::tr1::unordered_set<std::string> entriesSeen;
    struct stat st;
    memset(&st, 0, sizeof(st));

    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);
    std::string potentialPath = GetSource();
    while (true) {
        struct dirent* shareEntry = sourceDir.Readdir();
        if (shareEntry == NULL) break;
        if (IsDotDirectory(shareEntry->d_name))
            continue;

        potentialPath.resize(GetSource().size());
        potentialPath.append(shareEntry->d_name)
                     .append(path);
        try {
            Directory sharedDir(potentialPath);
            while (true) {
                struct dirent* entry = sharedDir.Readdir();
                if (entry == NULL) break;
                if (IsDotDirectory(entry->d_name))
                    continue;
                const bool inserted = entriesSeen.insert(entry->d_name).second;
                if (inserted) {
                    std::string absolutePath = potentialPath;
                    absolutePath.append("/");
                    absolutePath.append(entry->d_name);
                    st.st_ino = entry->d_ino;
                    st.st_mode = entry->d_type << 12;
                    if (S_ISREG(st.st_mode)) {
                        // TODO this is very expensive - do we really need it?
                        st.st_size = DecodedFile::Size(absolutePath);
                    } else {
                        st.st_size = 1;
                    }
                    if (filler(buffer, entry->d_name, &st, 0) == 1)
                        return -EIO;
                }
            }
        } catch (std::exception& exc) {
            continue;
        }
    }

    return 0;
}

int ZFecFSDecoder::Releasedir(const char *, fuse_file_info *fileInfo)
{
    delete reinterpret_cast<std::string*>(fileInfo->fh);
    return 0;
}

int ZFecFSDecoder::Open(const char *path, fuse_file_info *fileInfo)
{
    try {
        struct stat statBuf;
        std::vector<std::string> paths = GetFirstNumPathMatchesInAnyShare(path,
                                                                          GetFecWrapper().GetSharesRequired(),
                                                                          &statBuf);
        DecodedFile* file = DecodedFile::Open(paths, GetFecWrapper());
        fileInfo->fh = reinterpret_cast<u_int64_t>(file);
    } catch (const std::exception& exc) {
        return -ENOENT;
    }

    return 0;
}

std::vector<std::string> ZFecFSDecoder::GetFirstNumPathMatchesInAnyShare(
                             const char* pathToFind, unsigned int numMatches,
                             struct stat* statBuf )
{
    struct stat statBufHere;
    if (statBuf == NULL)
        statBuf = &statBufHere;

    Directory sourceDir(GetSource());

    std::vector<std::string> paths;
    std::string potentialPath = GetSource();
    while (paths.size() < numMatches) {
        struct dirent* entry = sourceDir.Readdir();
        if (entry == NULL) break;

        potentialPath.resize(GetSource().size());
        potentialPath.append(entry->d_name)
                     .append("/")
                     .append(pathToFind);

        if (lstat(potentialPath.c_str(), statBuf) == 0)
            paths.push_back(potentialPath);
    }
    if (paths.size() >= numMatches)
        return paths;
    else
        throw SimpleException("Not enough shares found for file.");
}

std::string ZFecFSDecoder::GetFirstPathMatchInAnyShare(const char* pathToFind,
                                                       struct stat* statBuf)
{
    struct stat statBufHere;
    if (statBuf == NULL) statBuf = &statBufHere;
    // TODO cache directory info of base directory?

    Directory sourceDir(GetSource());

    std::string potentialPath = GetSource();
    while (true) {
        struct dirent* entry = sourceDir.Readdir();
        if (entry == NULL) break;
        if (IsDotDirectory(entry->d_name))
            continue;

        potentialPath.resize(GetSource().size());
        potentialPath.append(entry->d_name)
                     .append(pathToFind);

        if (lstat(potentialPath.c_str(), statBuf) == 0)
            return potentialPath;
    }

    throw SimpleException("File not found in any share.");
}

} // namespace ZFecFS
