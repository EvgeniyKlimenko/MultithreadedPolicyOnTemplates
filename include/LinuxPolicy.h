#if !defined( __LINUX_POLICY_H__ )
#define __LINUX_POLICY_H__

#ifdef __linux__

#include "Policy.h"

using LinuxException = SystemException;

class LinuxProcessTerminationHandler final
{
    std::function<void (void)> m_callback;

    static void SignalCallback(int sig, siginfo_t* info, void* param)
    {
        LinuxProcessTerminationHandler* handler = static_cast<LinuxProcessTerminationHandler*>(param);
        std::cerr << "Termination callback for signal " << sig << ", process ID: " << info->si_pid << ", user ID: " << info->si_uid << "." << std::endl;
        handler->m_callback();
    }

public:
    LinuxProcessTerminationHandler(std::function<void (void)>&& callback)
    : m_callback(callback)
    {
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));

        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = &LinuxProcessTerminationHandler::SignalCallback;
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        
        std::array<int, 8> signumArray; 

        signumArray[0] = SIGALRM;
        signumArray[1] = SIGHUP;
        signumArray[2] = SIGINT;
        signumArray[3] = SIGQUIT;
        signumArray[4] = SIGTSTP;
        signumArray[5] = SIGTERM;
		signumArray[6] = SIGUSR1;
		signumArray[7] = SIGUSR2;

        std::for_each(std::begin(signumArray), std::end(signumArray), [&sa](int signum)
        {
            if(sigaction(signum, &sa, nullptr) < 0)
                throw LinuxException(errno);
        });
    }
};

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
        std::cout << "System based threading completed." << std::endl;
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
    Data,
    PriorityQueue,
    LinuxLock,
    ScopedLocker,
    LinuxSynchronizer,
    LinuxThreadPool<>,
    LinuxThreadNumber<DEBUG_MODE>
>;

#endif // __linux__

#endif // __LINUX_POLICY_H__