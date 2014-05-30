#ifndef ZFECFS_SHAREDDIR_H
#define ZFECFS_SHAREDDIR_H

#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>

#include "mutex.h"
#include "decodedpath.h"

namespace ZFecFS {

class SharedDir
{
public:
    static uint64_t Open(const DecodedPath& path)
    {
        DIR* dirStruct = opendir(path.path.c_str());
        if (dirStruct == NULL) return 0;

        SharedDir* dir = new SharedDir(dirStruct);
        return reinterpret_cast<uint64_t>(dir);
    }

    static SharedDir* FromHandle(uint64_t handle)
    {
        return reinterpret_cast<SharedDir*>(handle);
    }

    ~SharedDir()
    {
        closedir(dir);
    }

    void Seek(off_t offset)
    {
        seekdir(dir, offset);
    }

    struct dirent* Readdir()
    {
        return readdir(dir);
    }

    long Telldir()
    {
        return telldir(dir);
    }

    Mutex& GetMutex()
    {
        return mutex;
    }
private:
    SharedDir(DIR* dir)
      : dir(dir) {}

    Mutex mutex;
    DIR* dir;
};

} // namespace ZFecFS

#endif // ZFECFS_SHAREDDIR_H
