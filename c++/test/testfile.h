#ifndef ZFECFS_TEST_TESTFILE_H
#define ZFECFS_TEST_TESTFILE_H

#include <algorithm>
#include <string>

#include "file.h"

namespace ZFecFS {

class TestFile : public AbstractFile
{
public:
    explicit TestFile(const std::string& contents)
        : contents(contents)
    {}

    virtual ssize_t Read(char* buffer, size_t size, off_t offset) const
    {
        if (offset >= off_t(contents.size())) return 0;
        if (offset + size >= contents.size())
            size = contents.size() - offset;
        std::copy(contents.begin() + offset,
                  contents.begin() + offset + size,
                  buffer);
        return size;
    }

    virtual ssize_t Size() const
    {
        return contents.size();
    }
private:
    const std::string contents;
};

}

#endif // ZFECFS_TEST_TESTFILE_H
