#pragma once
#include <thread>

/*
    Spin briefly before yielding.

    The consumer loops used to call std::this_thread::yield() on every empty
    poll, which is a sched_yield syscall. At trade rates the queue is usually
    non-empty again within a few hundred cycles, so a short relaxed spin gets
    the next trade without ever entering the kernel.
*/
inline void cpuRelax() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield" ::: "memory");
#endif
}

class Backoff {
public:
    void pause() {
        if (spins_ < kSpinLimit) { cpuRelax(); ++spins_; }
        else std::this_thread::yield();
    }
    void reset() { spins_ = 0; }
private:
    static constexpr int kSpinLimit = 128;
    int spins_ = 0;
};
