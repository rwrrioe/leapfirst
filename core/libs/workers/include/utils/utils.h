#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <immintrin.h>
#elif defined(__APPLE__)
    #include <pthread.h>
#else
    #define _GNU_SOURCE
    #include <sched.h>
    #include <pthread.h>
#endif

namespace corepin {
    static void pin_to_core(int core) {
#ifdef _WIN32
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        if (core >= (int)si.dwNumberOfProcessors) {
            std::printf("[PIN] core %d unavailable, have %d cores\n",
                core, (int)si.dwNumberOfProcessors);
            fflush(stdout);
            return;
        }
        HANDLE thread = GetCurrentThread();
        DWORD_PTR mask = (1ULL << core);
        DWORD_PTR result = SetThreadAffinityMask(thread, mask);
        if (!result) {
            std::printf("[PIN] SetThreadAffinityMask failed for core %d, error=%lu\n",
                core, GetLastError());
            fflush(stdout);
        }
#elif defined(__APPLE__)
    (void)core;
#else
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
#endif
    }
}

namespace utils {
    static void spin_pause() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
        _mm_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
        asm volatile("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }
}
