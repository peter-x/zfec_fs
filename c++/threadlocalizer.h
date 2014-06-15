#ifndef ZFECFS_THREADLOCALIZER_H
#define ZFECFS_THREADLOCALIZER_H

#include <map>

#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

namespace ZFecFS {

template <class ThreadLocalData>
class ThreadLocalizer
{
public:
    ThreadLocalData& Get()
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        return threadLocalData[pthread_self()];
    }

private:
    boost::mutex mutex;
    std::map<pthread_t, ThreadLocalData> threadLocalData;
};

} // namespace ZFecFS

#endif // ZFECFS_THREADLOCALIZER_H
