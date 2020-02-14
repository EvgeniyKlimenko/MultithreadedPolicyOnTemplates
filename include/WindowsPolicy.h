#if !defined( __WINDOWS_POLICY_H__ )
#define __WINDOWS_POLICY_H__

#ifdef _WIN64

#include "Policy.h"

using _tstring = std::basic_string<_TCHAR>;
using _tstringstream = std::basic_stringstream<_TCHAR>;

#include "Policy.h"

class WindowsException final : public std::exception
{
    ULONG m_err;
    std::string m_descr;
public:
    WindowsException(ULONG err) : m_err(err)
    {
        Resolve();
    }
    
    WindowsException(const WindowsException& other)
    {
        m_err = other.m_err;
        if (other.m_descr.empty())
            Resolve();
        else
            m_descr = other.m_descr;
    }

    WindowsException& operator= (const WindowsException& other)
    {
        if (this != &other)
        {
            m_err = other.m_err;
            if (other.m_descr.empty())
                Resolve();
            else
                m_descr = other.m_descr;
        }

        return *this;
    }
    
    virtual ~WindowsException() = default;
    
    const char* what() const noexcept override
    {
        return m_descr.c_str();
    }

private:
    void Resolve()
    {
        _TCHAR* msg = nullptr;

        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
            nullptr, m_err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<_TCHAR*>(&msg), 0, nullptr);
        if (msg)
        {
            _tstringstream stm;
            stm << _T("Windows error ") << m_err << _T(" - ") << msg;
            _tstring str = stm.str();
            m_descr.assign(str.begin(), str.end());
            LocalFree(msg);
        }
    }
};

class WindowsProcessTerminationHandler final
{
    static WindowsProcessTerminationHandler* s_self;
    std::function<void (void)> m_callback;

    static BOOL __stdcall ConsoleCtrlCallback(ULONG code)
    {
        std::cerr << "Console control callback for code " << code << " raised." << std::endl;
        s_self->m_callback();
        return TRUE;
    }

public:
    WindowsProcessTerminationHandler(std::function<void (void)>&& callback)
    : m_callback(callback)
    {
        if (!SetConsoleCtrlHandler(&WindowsProcessTerminationHandler::ConsoleCtrlCallback, TRUE))
            throw WindowsException(GetLastError());

        WindowsProcessTerminationHandler::s_self = this;
    }
};

template < bool Debug = false >
struct WindowsThreadNumber
{
    static LONG Get()
    {
        SYSTEM_INFO sysInfo = {0};
        GetSystemInfo(&sysInfo);
        return (sysInfo.dwNumberOfProcessors << 1L) + 1L;
    }
};

template <>
struct WindowsThreadNumber<true>
{
    static LONG Get()
    {
        return 1L;
    }
};


class WindowsLock : public GenericLock < WindowsLock >
{
    CRITICAL_SECTION m_cs;
public:
    WindowsLock()
    {
        InitializeCriticalSection(&m_cs);
    }
    
    ~WindowsLock()
    {
        DeleteCriticalSection(&m_cs);
    }

    void Lock()
    {
        EnterCriticalSection(&m_cs);
    }

    void Unlock()
    {
        LeaveCriticalSection(&m_cs);
    }
};

class WindowsSynchronizer : public GenericSync < WindowsSynchronizer >
{
    volatile bool m_stopping;
    LONG m_threadNum;
    HANDLE m_hSem;

public:
    template < typename ... Args >
    WindowsSynchronizer(Args&& ... a)
    : m_stopping(false)
    , m_threadNum(std::forward < LONG >(a ...))
    {
        std::cout << "Created Windows API semaphore." << std::endl;
        m_hSem = CreateSemaphoreEx(nullptr, 0, INT_MAX, _T("WindowsPolicyDemoSemaphore"),
            0, SEMAPHORE_MODIFY_STATE | SYNCHRONIZE);
        if (!m_hSem) throw WindowsException(GetLastError());
    }

    ~WindowsSynchronizer()
    {
        CloseHandle(m_hSem);
    }

    void Signal()
    {
        BOOL res = ReleaseSemaphore(m_hSem, 1, nullptr);
        if (!res) throw WindowsException(GetLastError());
    }

    bool Wait()
    {
        ULONG res = WaitForSingleObject(m_hSem, INFINITE);
        if (res == WAIT_FAILED) throw WindowsException(GetLastError());
        assert(res == WAIT_OBJECT_0);
        return m_stopping;
    }

    void Stop()
    {
        m_stopping = true;
        for (LONG i = 0; i < m_threadNum; ++i)
            Signal();
    }
};

template <typename ThreadFunType = std::function<void(void)> >
class WindowsThreadPool : public GenericThreadPool< WindowsThreadPool< ThreadFunType > >
{
    LONG m_threadNum;
    std::vector < HANDLE > m_thv;
    ThreadFunType m_thfn;
public:
    WindowsThreadPool(LONG threadNum, ThreadFunType&& thfn)
    : m_threadNum(threadNum)
    , m_thfn(thfn)
    {
        std::cout << m_threadNum << " working threads used." << std::endl;
    }

    ~WindowsThreadPool()
    {
        ULONG count = static_cast<ULONG>(m_thv.size());
        PHANDLE handles = &m_thv[0];

        WaitForMultipleObjects(count, handles, TRUE, INFINITE);
        std::for_each(std::begin(m_thv), std::end(m_thv), [](HANDLE& h) {CloseHandle(h);});
        std::cout << "System based threading completed." << std::endl;
    }

    void Start()
    {
        for (int i = 0; i < m_threadNum; ++i)
        {
            uintptr_t th = _beginthreadex(nullptr, 0, &WindowsThreadPool::WindowsTheadPoolCallback, this, 0, nullptr);
            if (th == static_cast<uintptr_t>(-1)) throw SystemException(errno);

            m_thv.push_back(reinterpret_cast<HANDLE>(th));
        }
    }

    static unsigned int GetCurrentThreadId()
    {
        return ::GetCurrentThreadId();
    }

private:
    static unsigned int __stdcall WindowsTheadPoolCallback(void* param)
    {
        WindowsThreadPool* self = static_cast<WindowsThreadPool*>(param);
        self->m_thfn();
        return 0;
    }
};

using WindowsThreadPoolPolicy = AsyncWorkPolicy
<
    Data,
    PriorityQueue,
    WindowsLock,
    ScopedLocker,
    WindowsSynchronizer,
    WindowsThreadPool<>,
    WindowsThreadNumber<DEBUG_MODE>
>;


#endif // _WIN64

#endif // __WINDOWS_POLICY_H__