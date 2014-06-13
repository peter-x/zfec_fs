#ifndef ZFECFS_FILE_H
#define ZFECFS_FILE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <string>

#include "utils.h"

namespace ZFecFS {

class File
{
public:
    File() { data = 0; }
    explicit File(const std::string& path)
    {
        data = new Data();
        int handle = open(path.c_str(), O_RDONLY);
        if (handle == -1) {
            delete data;
            throw SimpleException("Error opening file.");
        }
        data->handle = handle;
    }
    ~File()
    {
        if (data == 0) return;

        data->refCount--;
        if (data->refCount == 0) {
            close(data->handle);
            delete data;
        }
    }
    File(const File& other)
    {
        data = other.data;
        data->refCount++;
    }
    File& operator=(const File& other)
    {
        data = other.data;
        data->refCount++;
        return *this;
    }

    ssize_t Read(char* buffer, size_t size, off_t offset) const
    {
        ssize_t sizeRead = pread(data->handle, buffer, size, offset);
        if (sizeRead == -1)
            throw SimpleException("Error reading file.");
        return sizeRead;
    }

    off_t Size() const
    {
        off_t size = lseek(data->handle, 0, SEEK_END);
        if (size == off_t(-1))
            throw SimpleException("File size could not be determined.");
        return size;
    }
private:
    struct Data {
        unsigned int refCount;
        int handle;
        Data() : refCount(1), handle(-1) {}
    };

    mutable Data* data;
};

} // namespace ZFecFS

#endif // FILE_H
