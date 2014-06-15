#ifndef ZFECFS_FILE_H
#define ZFECFS_FILE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <string>

#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>

#include "utils.h"

namespace ZFecFS {

class File : boost::noncopyable
{
public:
    explicit File(const std::string& path)
    {
        handle = open(path.c_str(), O_RDONLY);
        if (handle == -1)
            throw SimpleException("Error opening file.");
    }
    ~File()
    {
        if (handle != -1)
            close(handle);
    }

    ssize_t Read(char* buffer, size_t size, off_t offset) const
    {
        ssize_t sizeRead = pread(handle, buffer, size, offset);
        if (sizeRead == -1)
            throw SimpleException("Error reading file.");
        return sizeRead;
    }

    off_t Size() const
    {
        off_t size = lseek(handle, 0, SEEK_END);
        if (size == off_t(-1))
            throw SimpleException("File size could not be determined.");
        return size;
    }
private:
    int handle;
};

} // namespace ZFecFS

#endif // FILE_H
