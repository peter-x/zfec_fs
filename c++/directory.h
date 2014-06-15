#ifndef ZFECFS_SHAREDDIR_H
#define ZFECFS_SHAREDDIR_H

#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>

#include <string>
#include <boost/thread/mutex.hpp>
#include <boost/utility.hpp>

#include "utils.h"

namespace ZFecFS {

class Directory : boost::noncopyable
{
public:
    Directory(const std::string& path)
    : dir(opendir(path.c_str()))
    {
        if (dir == NULL) throw SimpleException("Error opening directory.");
    }

    ~Directory()
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

    boost::mutex& GetMutex()
    {
        return mutex;
    }
private:

    boost::mutex mutex;
    DIR* dir;
};

} // namespace ZFecFS

#endif // ZFECFS_SHAREDDIR_H
