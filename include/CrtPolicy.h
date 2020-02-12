#if !defined( __CRT_POLICY_H__ )
#define  __CRT_POLICY_H__

#include "Policy.h"

template < bool Debug = false >
struct CrtThreadNumber
{
    static int Get()
    {
        return (std::thread::hardware_concurrency() << 1) + 1;
    }
};

template<> struct CrtThreadNumber<true>
{
    static int Get()
    {
        return 1;
    }
};

class CrtLock : public GenericLock < CrtLock >
{
    std::mutex m_mtx;
public:
    void Lock() { m_mtx.lock(); }
    void Unlock() { m_mtx.unlock(); }
};

class CrtSynchronizer : public GenericSync < CrtSynchronizer >
{
    volatile long m_count;
    volatile bool m_stopping;
    std::mutex m_l;
    std::condition_variable m_s;

public:
    template < typename ... Args >
    CrtSynchronizer(Args && ...) : m_count(0), m_stopping(false) {}

    void Signal()
    {
        std::unique_lock < std::mutex > lk (m_l);
        ++m_count;
        m_s.notify_one();
    }

    bool Wait()
    {
        std::unique_lock < std::mutex > lk(m_l);
        m_s.wait (lk,[this]()
            {
                return (m_count > 0) || m_stopping;
            });

        if (m_stopping) return true;
        --m_count;

        return false;
    }

    void Stop()
    {
        std::unique_lock < std::mutex > lk(m_l);
        m_stopping = true;
        m_s.notify_all();
    }
};

template <typename ThreadFunType = std::function<void(void)>>
class CrtThreadPool : public GenericThreadPool< CrtThreadPool< ThreadFunType > >
{
    int m_thread_num;
    std::list < std::thread > m_thl;
    ThreadFunType m_thfn;
public:
    CrtThreadPool(int thread_num, ThreadFunType&& thfn)
    : m_thread_num(thread_num)
    , m_thfn(thfn)
    {
        std::cout << m_thread_num << " working threads used." << std::endl;
    }

    ~CrtThreadPool()
    {
        std::for_each(std::begin(m_thl), std::end(m_thl), [](std::thread& th) { th.join (); });
        std::cout << "Threading completed." << std::endl;
    }

    void Start()
    {
        for (int i = 0; i < m_thread_num; ++i)
            m_thl.emplace_back(m_thfn);
    }

    static unsigned int GetCurrentThreadId()
    {
        std::stringstream sstm;
        sstm << std::this_thread::get_id();
        return std::stoul(sstm.str());
    }
};

using CrtThreadPoolPolicy = AsyncWorkPolicy
<
    Data, PriorityQueue, CrtLock, CrtSynchronizer,
    CrtThreadPool<>, CrtThreadNumber<DEBUG_MODE>
>;

#endif // __CRT_POLICY_H__