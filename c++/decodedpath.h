#ifndef DECODEDPATH_H
#define DECODEDPATH_H

#include <string>

#include "utils.h"

namespace ZFecFS {

/// Can be used to convert a path of the form '/<shareindex>/real/path'
/// into the absolute path '/path/to/source/real/path' and the index
class DecodedPath {
public:
    typedef unsigned int ShareIndex;

    const ShareIndex index;
    const bool indexGiven;
    const std::string path;

    static DecodedPath DecodePath(const std::string& path,
                                  const std::string& sourcePath)
    {
        std::string::const_iterator it = path.begin();
        while (it != path.end() && *it == '/') ++it;
        if (it == path.end())
            return DecodedPath(sourcePath);

        ShareIndex index = DecodeShareIndex(it, path.end());

        std::string absolutePath;
        absolutePath.reserve(sourcePath.size() + (path.end() - it));
        absolutePath.append(sourcePath);
        absolutePath.append(it, path.end());

        return DecodedPath(absolutePath, index);
    }

    template <class Iterator>
    static void EncodeShareIndex(DecodedPath::ShareIndex index, Iterator buffer)
    {
        if (index < 0 || index >= 256)
            throw SimpleException("Invalid share index");
        *buffer++ = Hex::EncodeDigit((index & 0xf0) >> 4);
        *buffer++ = Hex::EncodeDigit(index & 0xf);
    }

private:
    DecodedPath(const std::string& path, ShareIndex index)
        : index(index)
        , indexGiven(true)
        , path(path)
    {}

    explicit DecodedPath(const std::string& path)
        : index(0)
        , indexGiven(false)
        , path(path)
    {}

    static DecodedPath::ShareIndex DecodeShareIndex(std::string::const_iterator& pos,
                                                      std::string::const_iterator end) {
        if (end - pos < 2 || (pos + 2 != end && *(pos + 2) != '/'))
            throw SimpleException("Unable to parse share index.");
        ShareIndex index = Hex::DecodeDigit(*pos++) << 4;
        index += Hex::DecodeDigit(*pos++);
        return index;
    }
};

} // namespace ZFECFS

#endif // DECODEDPATH_H
