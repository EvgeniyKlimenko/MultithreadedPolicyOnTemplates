#if !defined( __POLICY_H__ )
#define  __POLICY_H__

#include "Common.h"

class SystemException final : public std::exception
{
    int m_err;
public:
    SystemException(int err) : m_err(err) {}
    
    SystemException(const SystemException& other)
    {
        m_err = other.m_err;
    }

    SystemException& operator= (const SystemException& other)
    {
        if (this != &other)
        {
            m_err = other.m_err;
        }

        return *this;
    }
    
    virtual ~SystemException () = default;
    
    const char *what() const noexcept override
    {
        return strerror(m_err);
    }
};

class Exceptioning
{
    std::mutex m_l;
    std::list < std::exception_ptr > m_exl;
public:
    void Add(std::exception_ptr exp)
    {
        std::unique_lock < std::mutex > locker(m_l);
        m_exl.push_back(exp);
    }
        
    void Show()
    {
        std::for_each(std::begin(m_exl), std::end(m_exl),
        [](std::exception_ptr exp)
        {
            try
            {
                std::rethrow_exception(exp);
            }
            catch (std::exception & e)
            {
                std::cerr << "Exception caught: " << e.what() << std::endl;
            }
        });
    }
        
    bool Present()
    {
        return !m_exl.empty ();
    }
};

template
<
    typename DataType,
    template < typename > typename QueueType,
    typename LockType,
    template < typename > typename LockerType,
    typename SyncType,
    typename ThreadPoolType,
    typename ThreadNumber
>
class AsyncWorkPolicy
{
    using CallbackType =
    std::function
    <
    void (typename QueueType< DataType >::DataPtrType)
    >;

    Exceptioning m_excp;
    QueueType< DataType > m_queue;
    CallbackType m_callback;
    LockType m_lock;
    SyncType m_sync;
    ThreadPoolType m_thread_pool;
public:
    AsyncWorkPolicy(CallbackType&& callback)
    : m_callback(callback)
    , m_sync(ThreadNumber::Get())
    , m_thread_pool(ThreadNumber::Get(), std::bind(&AsyncWorkPolicy::ThreadPoolCallback, this))
    {
        try
        {
            m_thread_pool.Start();
            std::cout << "Starting policy." << std::endl;
        }
        catch (std::exception &)
        {
            m_excp.Add(std::current_exception());
        }
    }

    ~AsyncWorkPolicy()
    {
        Stop();
    }

    void ShowExceptions()
    {
        if (m_excp.Present())
            m_excp.Show();
    }

    void Perform (const typename QueueType< DataType >::DataPtrType& dataPtr)
    {
        try
        {
            EnqueueAtomically(dataPtr);
            m_sync.Signal();
        }
        catch (std::exception &)
        {
            m_excp.Add(std::current_exception());
        }
    }

    void Stop()
    {
        std::cout << "Stopping policy." << std::endl;
        m_sync.Stop();
    }

protected:
    void ThreadPoolCallback()
    {
        try
        {
            std::cout << "Thread " << ThreadPoolType::GetCurrentThreadId() << " started." << std::endl;

            for (;;)
            {
                bool stop = m_sync.Wait();
                if (stop) break;

                typename QueueType< DataType >::DataPtrType dataPtr = DequeueAtomically();
                if (!dataPtr) continue;
                
                m_callback(dataPtr);
            }

            std::cout << "Thread " << ThreadPoolType::GetCurrentThreadId() << " finished." << std::endl;
        }
        catch (std::exception &)
        {
            m_excp.Add(std::current_exception ());
        }
    }

    void EnqueueAtomically(const typename QueueType< DataType >::DataPtrType& dataPtr)
    {
        LockerType < LockType > l(m_lock);
        m_queue.Enqueue(dataPtr);
    }

    typename QueueType< DataType >::DataPtrType DequeueAtomically()
    {
        LockerType < LockType > l(m_lock);
        return m_queue.Dequeue();
    }
};

class Data
{
    int m_a;
    int m_b;
    int m_p;

public:
    Data(int a, int b, int p = 10):m_a (a), m_b (b), m_p (p) {}

    int GetPriority() { return m_p; }

    std::string Printout()
    {
        std::stringstream sstm;
        sstm << "{ a: " << m_a << ", b: " << m_b << ", priority: " << m_p << " }." << std::endl;
        return sstm.str ();
    }
};

template < typename DataType > class PriorityQueue
{
public:
    using DataPtrType = std::shared_ptr < DataType >;
    
    struct Comparator
    {
        bool operator()(const DataPtrType& lhs, const DataPtrType& rhs) const
        {
            assert(lhs && rhs);
            return (lhs->GetPriority () < rhs->GetPriority ());
        }
    };

    void Enqueue(const DataPtrType & d)
    {
        std::cout << "Enqueue entry - " << d->Printout () << std::endl;
        m_q.push (d);
    }

    DataPtrType Dequeue ()
    {
        if (m_q.empty ()) return DataPtrType();
        DataPtrType d = m_q.top ();
        m_q.pop ();

        std::cout << "Dequeue entry - " << d->Printout () << std::endl;
        
        return d;
    }

private:
    std::priority_queue 
    <
        DataPtrType,
        std::deque < DataPtrType >,
        typename PriorityQueue::Comparator
    > m_q;
};

template < typename DerivedType > class GenericLock
{
public:
    GenericLock() = default;
    ~GenericLock() = default;

    void Lock ()
    {
        Self().Lock();
    }

    void Unlock ()
    {
        Self().Unlock();
    }

protected:
    DerivedType & Self()
    {
        return (static_cast < DerivedType & >(*this));
    }
};

template < typename LockType > class ScopedLocker
{
    LockType& m_lock;
public:    
    ScopedLocker(LockType& lock) : m_lock(lock) { m_lock.Lock(); }
    ~ScopedLocker() { m_lock.Unlock(); }
};

template < typename DerivedType > class GenericSync
{
public:
    void Signal() { Self().Signal(); }
    bool Wait() { return Self().Wait(); }
    void Stop() { Self().Stop(); }

protected:
    DerivedType & Self()
    {
        return (static_cast < DerivedType & >(*this));
    }
};

template <typename DerivedType> class GenericThreadPool
{
public:
    void Start()
    {
        Self().Start();
    }

    static unsigned int GetCurrentThreadId()
    {
        return DerivedType::GetCurrentThreadId();
    }
    
protected:
    DerivedType& Self()
    {
        return static_cast<DerivedType&>(*this);
    }
};

#endif // __POLICY_H__