#define DEBUG_MODE      1
#define USE_CRT_POLICY  0
#define USE_SINGLETON   0

#if defined( _WIN64 )
#pragma warning(disable:4244)
#endif // _WIN64

#if USE_CRT_POLICY==1

#include "CrtPolicy.h"
using CurrentThreadPoolPolicy = CrtThreadPoolPolicy;

#else

#if defined( __linux__ )

#include "LinuxPolicy.h"
using CurrentThreadPoolPolicy = LinuxThreadPoolPolicy;

#elif defined( _WIN64 )

#include "WindowsPolicy.h"
using CurrentThreadPoolPolicy = WindowsThreadPoolPolicy;

#endif

#endif // USE_CRT_POLICY


template < typename DerivedType, bool Singleton >
class AppLogic
{
public:
    static AppLogic & Create() { return DerivedType::Create(); }
    int Run() { return Self().Run(); }

protected:
    DerivedType & Self() { return (static_cast < DerivedType & >(*this)); }
};

template < typename DerivedType >
class AppLogic < DerivedType, false >
{
public:
    static std::unique_ptr < AppLogic > Create() { return DerivedType::Create(); }
    int Run () { return Self().Run(); }

protected:
    DerivedType & Self() { return (static_cast < DerivedType & >(*this)); }
};

template < typename AppType >
int RunApp ()
{
#if USE_SINGLETON==1
    AppType & app = AppType::Create();
    return app.Run();
#else
    std::unique_ptr < AppType > app = AppType::Create();
    return app->Run();
#endif // USE_SINGLETON
}

template < bool Debug = false >
struct Maxval
{
    static int Get() {return 1000;}
};

template <>
struct Maxval<true>
{
    static int Get() {return 10;}
};

class ThreadedApp:public AppLogic < ThreadedApp, false >
{
    CurrentThreadPoolPolicy m_policy;
public:

#if USE_SINGLETON==1
    static ThreadedApp & Create()
    {
        static ThreadedApp thr_app;
        return thr_app;
    }
#else
    static std::unique_ptr < ThreadedApp > Create ()
    {
        return std::unique_ptr < ThreadedApp >(new ThreadedApp());
    }
#endif	// USE_SINGLETON

    ~ThreadedApp ()
    {
        std::cout << "Threaded app finished." << std::endl;
    }

    int Run()
    {
        srand(time(nullptr));
        
        for (int i = 0; i < Maxval<DEBUG_MODE>::Get(); ++i)
        {
            // Generate secret number between 1 and maxval.
            int p = rand () % Maxval<DEBUG_MODE>::Get() + 1;
            int a = (p << 2) + 1;
            int b = a - i;

            m_policy.Perform(std::shared_ptr < Data >(new Data(a, b, p)));
        }

        std::cout << "Task in progress. Press any key to stop..." << std::endl;
        std::cin.get();

        m_policy.ShowExceptions();

        return 0;
    }

private:
    ThreadedApp()
    : m_policy(
        [](const std::shared_ptr< Data > &d)
        {
            std::cout << "DefaultCallback - got entry - " << d->Printout () << std::endl;
        })
    {
        std::cout << "Creating threaded app." << std::endl;
    }
};

int main()
{
	return RunApp < ThreadedApp > ();
}