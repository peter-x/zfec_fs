#ifndef UTILS_H
#define UTILS_H

#include <exception>

namespace ZFecFS {


class SimpleException : public std::exception {
public:
    SimpleException(const char* message) : message(message)
    {}

    virtual const char* what() const throw()
    {
        return message;
    }
private:
    const char* message;
};

inline bool IsDotDirectory(char* path)
{
    return path[0] == '.' && (path[1] == 0 || (path[1] == '.' && path[2] == 0));
}

struct Hex {
    static char EncodeDigit(const int number)
    {
        if (0 <= number && number < 10) {
            return '0' + number;
        } else if (10 <= number && number <= 15) {
            return 'a' + (number - 10);
        } else {
            throw SimpleException("Invalid hex digit value.");
        }
    }

    static int DecodeDigit(const char digit)
    {
        if (digit >= '0' && digit <= '9') {
            return digit - '0';
        } else if (digit >= 'a' && digit <= 'f') {
            return digit - 'a' + 10;
        } else {
            throw SimpleException("Invalid hex digit.");
        }
    }
};

} // namespace ZFecFS

#endif // UTILS_H
