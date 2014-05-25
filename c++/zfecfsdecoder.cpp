#include "zfecfsdecoder.h"

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include <string>
#include <algorithm>

#include "decodedfile.h"
#include "utils.h"

namespace ZFecFS {

int ZFecFSDecoder::Getattr(const char *path, struct stat *stbuf)
{
    try {
        std::string realPath = GetFirstPathMatchInAnyShare(path, stbuf);
        if ((stbuf->st_mode & S_IFMT) == S_IFREG) {
            DecodedFile* file = DecodedFile::Open(std::vector<std::string>(1, realPath), GetFecWrapper());
            stbuf->st_size = file->Size();
        }
    } catch (const std::exception& exc) {
        return -ENOENT;
    }

    return 0;
}

int ZFecFSDecoder::Opendir(const char *path, fuse_file_info *fileInfo)
{
    // TODO
    return -ENOENT;
}

std::string ZFecFSDecoder::GetFirstPathMatchInAnyShare(const char* pathToFind,
                                                       struct stat* statBuf)
{
    struct stat statBufHere;
    if (statBuf == NULL) statBuf = &statBufHere;
    // TODO cache directory info of base directory?

    DIR* sourceDir = opendir(GetSource().c_str());
    if (sourceDir == NULL)
        throw SimpleException("Could not open base directory.");

    std::string potentialPath = GetSource();
    struct dirent* entry;
    while (true) {
        entry = readdir(sourceDir);
        if (entry == NULL) break;

        potentialPath.resize(GetSource().size());
        potentialPath.append(entry->d_name)
                     .append("/")
                     .append(pathToFind);

        struct stat statBuf;
        if (lstat(potentialPath.c_str(), &statBuf) == 0) {
            closedir(sourceDir);
            return potentialPath;
        }
    }

    closedir(sourceDir);

    throw SimpleException("File not found in any share.");
}

} // namespace ZFecFS
