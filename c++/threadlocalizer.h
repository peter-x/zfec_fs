#ifndef ZFECFS_THREADLOCALIZER_H
#define ZFECFS_THREADLOCALIZER_H

#include <pthread.h>

#include <map>

#include "mutex.h"

namespace ZFecFS {

template <class ThreadLocalData>
class ThreadLocalizer
{
public:
    ThreadLocalData& Get()
    {
        Mutex::Lock lock(mutex);
        return threadLocalData[pthread_self()];
    }

private:
    mutable Mutex mutex;
    std::map<pthread_t, ThreadLocalData> threadLocalData;
};

} // namespace ZFecFS

#endif // ZFECFS_THREADLOCALIZER_H
