#ifndef ZFECFS_MUTEX_H
#define ZFECFS_MUTEX_H

#include <pthread.h>

namespace ZFecFS {

class Mutex
{
public:
    Mutex() { pthread_mutex_init(&mutex, NULL); }
    ~Mutex() { pthread_mutex_destroy(&mutex); }

    class Lock
    {
    public:
        Lock(Mutex& mutex) : mutex(mutex)
        { pthread_mutex_lock(&mutex.mutex); }
        ~Lock() { pthread_mutex_unlock(&mutex.mutex); }
    private:
        Mutex& mutex;
    };
private:
    pthread_mutex_t mutex;
};

} // namespace ZFecFS

#endif // ZFECFS_MUTEX_H
