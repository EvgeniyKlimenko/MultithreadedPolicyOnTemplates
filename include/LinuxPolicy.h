#if !defined( __LINUX_POLICY_H__ )
#define __LINUX_POLICY_H__

#ifdef __linux__

#include "Policy.h"

using LinuxException = SystemException;

template < bool Debug = false >
struct LinuxThreadNumber
{
    static int Get()
    {
        return (get_nprocs() << 1) + 1;
    }
};

template <>
struct LinuxThreadNumber<true>
{
    static int Get()
    {
        return 1;
    }
};

class LinuxLock : public GenericLock < LinuxLock >
{
    pthread_mutex_t m_mtx;
public:
    LinuxLock()
    {
        pthread_mutex_init(&m_mtx, nullptr);
    }
    
    ~LinuxLock()
    {
        pthread_mutex_destroy(&m_mtx);
    }

    void Lock()
    {
        pthread_mutex_lock(&m_mtx);
    }

    void Unlock()
    {
        pthread_mutex_unlock(&m_mtx);
    }
};

class LinuxSynchronizer : public GenericSync < LinuxSynchronizer >
{
    volatile bool m_stopping;
    int m_thread_num;
    sem_t m_s;

public:
    template < typename ... Args >
    LinuxSynchronizer(Args&& ... a)
    : m_stopping(false)
    , m_thread_num(std::forward < int >(a ...))
    {
        std::cout << "Created sys V5 semaphore." << std::endl;
        int res = sem_init (&m_s, 0, 0);
        if (res) throw LinuxException(errno);
    }

    ~LinuxSynchronizer()
    {
        sem_destroy(&m_s);
    }

    void Signal()
    {
        int res = sem_post(&m_s);
        if (res) throw LinuxException(errno);
    }

    bool Wait()
    {
        int res = sem_wait(&m_s);
        if (res) throw LinuxException (errno);
        return m_stopping;
    }

    void Stop()
    {
        m_stopping = true;
        for (int i = 0; i < m_thread_num; ++i)
            Signal();
    }
};

template <typename ThreadFunType = std::function<void(void)> >
class LinuxThreadPool : public GenericThreadPool< LinuxThreadPool< ThreadFunType > >
{
    int m_thread_num;
    std::list < pthread_t > m_thl;
    ThreadFunType m_thfn;
public:
    LinuxThreadPool(int thread_num, ThreadFunType&& thfn)
    : m_thread_num(thread_num)
    , m_thfn(thfn)
    {
        std::cout << m_thread_num << " working threads used." << std::endl;
    }

    ~LinuxThreadPool()
    {
        std::for_each(std::begin(m_thl), std::end(m_thl), [](pthread_t id) { pthread_join(id, nullptr); });
        std::cout << "Systemn based threading completed." << std::endl;
    }

    void Start()
    {
        for (int i = 0; i < m_thread_num; ++i)
        {
            pthread_t id;
            int res = pthread_create(&id, nullptr, &LinuxThreadPool::LinuxTheadPoolCallback, this);
            if (res) throw LinuxException (errno);

            m_thl.push_back(id);
        }
    }

    static unsigned int GetCurrentThreadId()
    {
        return static_cast<unsigned int>(pthread_self());
    }

private:
    static void* LinuxTheadPoolCallback(void* param)
    {
        LinuxThreadPool* self = static_cast<LinuxThreadPool*>(param);
        self->m_thfn();
        return nullptr;
    }
};

using LinuxThreadPoolPolicy = AsyncWorkPolicy
<
    Data, PriorityQueue, LinuxLock, LinuxSynchronizer,
    LinuxThreadPool<>, LinuxThreadNumber<DEBUG_MODE>
>;

#endif // __linux__

#endif // __LINUX_POLICY_H__