#pragma once

#include <pthread.h>
#include <atomic>

class Invoker {
  public:
    Invoker();

    enum Identity {
      MAIN_PROCESS,
      OTHERSIDE,
    };
    static Identity oppositeIdentity(Identity i);

    // Each process should call this before doing anything
    // me: caller indicates whether they are the main process or the otherside process
    void initProcess(Identity me);

    // Each process should call this when process is being killed
    // me: caller indicates whether they are the main process or the otherside process
    void deInitProcess(Identity me);

    // Used by either side, to "call" the other
    // me: caller indicates whether they are the main process or the otherside process
    void invoke(Identity me);

    // Switch from the mutex-cond system to the atomic-spinlock system
    // MUST BE CALLED FROM MAIN PROCESS (not otherside process)
    void switchToAtomicSpinlock();

    // Switch from the atomic-spinlock system to the mutex-cond system
    // MUST BE CALLED FROM MAIN PROCESS (not otherside process)
    void switchToMutexCond();

  protected:
    enum System {
      MUTEX_COND_SYSTEM,
      ATOMIC_SPINLOCK_SYSTEM,
    };
    System curSystem;

    // MUTEX-COND SYSTEM
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    static pthread_mutexattr_t* mutexattr;
    static pthread_condattr_t* condattr;
    volatile Identity shouldBeRunning;  // this is the 'condition' backing the condition variable

    // ATOMIC-SPINLOCK SYSTEM
    enum SpinlockState {
      UNLOCKED_FOR_TAKING_BY_MAIN_PROCESS,
      UNLOCKED_FOR_TAKING_BY_OTHERSIDE,
      LOCKED,
    };
    std::atomic<SpinlockState> spinlock;

  private:
    void initMutexCond();
    void deInitMutexCond(Identity me);
    void initAtomicSpinlock(Identity me);
    void deInitAtomicSpinlock(Identity me);
    bool switchingSystems;
    void handleWakeup(Identity me);
};
