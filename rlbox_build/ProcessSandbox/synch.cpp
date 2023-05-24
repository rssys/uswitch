#include "synch.h"
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>
#include "myHelpers.h"  // ERROR()

static pthread_mutexattr_t* getMutexAttr() {
  pthread_mutexattr_t* mutexattr = (pthread_mutexattr_t*)malloc(sizeof(pthread_mutexattr_t));
  pthread_mutexattr_init(mutexattr);
  pthread_mutexattr_setpshared(mutexattr, PTHREAD_PROCESS_SHARED);  // allow mutex to be used across processes
  pthread_mutexattr_settype(mutexattr, PTHREAD_MUTEX_NORMAL);  // don't need reentrant
  pthread_mutexattr_setrobust(mutexattr, PTHREAD_MUTEX_STALLED);  // deadlock if the owner dies, that's fine
  pthread_mutexattr_setprotocol(mutexattr, PTHREAD_PRIO_NONE);
  // Experiments on 12/9/17 imply that using PTHREAD_PRIO_PROTECT here fails (deadlocks),
  // while PTHREAD_PRIO_NONE and PTHREAD_PRIO_INHERIT seem to give similar performance
  // on microbenchmarks. More testing needed, e.g. on larger applications.
  return mutexattr;
}

static pthread_condattr_t* getCondAttr() {
  pthread_condattr_t* condattr = (pthread_condattr_t*)malloc(sizeof(pthread_condattr_t));
  pthread_condattr_init(condattr);
  pthread_condattr_setpshared(condattr, PTHREAD_PROCESS_SHARED);
  return condattr;
}

pthread_mutexattr_t* Invoker::mutexattr = NULL;
pthread_condattr_t* Invoker::condattr = NULL;

Invoker::Identity Invoker::oppositeIdentity(Invoker::Identity i) {
  if(i == MAIN_PROCESS) return OTHERSIDE;
  else if(i == OTHERSIDE) return MAIN_PROCESS;
  else ERROR("Unknown Invoker::Identity\n")
}

Invoker::Invoker() :
    curSystem(MUTEX_COND_SYSTEM),
    spinlock(UNLOCKED_FOR_TAKING_BY_MAIN_PROCESS),
    switchingSystems(false),
    shouldBeRunning(MAIN_PROCESS)
{
  if(!mutexattr) mutexattr = getMutexAttr();
  if(!condattr) condattr = getCondAttr();
  pthread_mutex_init(&mutex, mutexattr);
  pthread_cond_init(&cond, condattr);
}

void Invoker::initMutexCond() {
  // will hold the mutex throughout, except explicitly during wait()
  pthread_mutex_lock(&mutex);
}

void Invoker::deInitMutexCond(Invoker::Identity me) {
  shouldBeRunning = oppositeIdentity(me);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
}

void Invoker::initAtomicSpinlock(Invoker::Identity me) {
  // In this function we simply acquire the lock, assuming we have never
  // held it before. For the main process' init, nothing has happened
  // since the lock was initialized, so it should be ready for us.
  // For the otherside's init, it must follow the main process' init
  // because otherside waits for the main process to release its lock.
  // Even if otherside runs first, it won't acquire the lock until main
  // acquires and releases it, because it starts in the wrong unlock state.
  SpinlockState expectedState;
  do {
    expectedState =
      me == MAIN_PROCESS ? UNLOCKED_FOR_TAKING_BY_MAIN_PROCESS
                         : UNLOCKED_FOR_TAKING_BY_OTHERSIDE;
  } while (!spinlock.compare_exchange_weak(expectedState, LOCKED));
    // we do that as a do-while loop because a failed compare_exchange_weak modifies the expectedState variable.
}

void Invoker::deInitAtomicSpinlock(Invoker::Identity me) {
  // if we're OTHERSIDE, this will let the main process take the spinlock, but we
  // won't resume waiting on the spinlock.

  // if we're MAIN_PROCESS, this will simply reset the spinlock state
  // (ready for another call to initAtomicSpinlock())

  SpinlockState expectedState, releaseState;
  do {
    expectedState = LOCKED;
    releaseState = UNLOCKED_FOR_TAKING_BY_MAIN_PROCESS;
  } while (!spinlock.compare_exchange_weak(expectedState, releaseState));
    // we do that as a do-while loop because a failed compare_exchange_weak modifies the expectedState variable.

  // now don't reacquire, just return
}

void Invoker::initProcess(Invoker::Identity me) {
  if(curSystem != MUTEX_COND_SYSTEM) {
    ERROR("Expected to initProcess in MUTEX_COND_SYSTEM\n")
  }
  initMutexCond();
}

void Invoker::deInitProcess(Invoker::Identity me) {
  switch(curSystem) {
    case MUTEX_COND_SYSTEM:
    deInitMutexCond(me);
    break;
    case ATOMIC_SPINLOCK_SYSTEM:
    deInitAtomicSpinlock(me);
    break;
    default:
    ERROR("Unrecognized Invoker::System\n")
  }
}

void Invoker::invoke(Invoker::Identity me) {
  switch(curSystem) {
    case MUTEX_COND_SYSTEM:
      // signal the other side to wake up
      shouldBeRunning = oppositeIdentity(me);
      _mm_mfence();
      while(!(shouldBeRunning == me)) {
        pthread_cond_signal(&cond);
        pthread_cond_wait(&cond, &mutex);  // this also releases the mutex so the other side can run
      }
      handleWakeup(me);  // we have now woken up from our waiting
    break;
    case ATOMIC_SPINLOCK_SYSTEM:
      // first release the lock to allow the other side to run
      // we expect this to always succeed first time and not spin,
      // so we just loop on the operation without fancy optimizations
      SpinlockState expectedState, releaseState;
      do {
        expectedState = LOCKED;
        releaseState =
          me == MAIN_PROCESS ? UNLOCKED_FOR_TAKING_BY_OTHERSIDE
                             : UNLOCKED_FOR_TAKING_BY_MAIN_PROCESS;
      } while (!spinlock.compare_exchange_weak(expectedState, releaseState));
        // we do that as a do-while loop because a failed compare_exchange_weak modifies the expectedState variable.

      // now we spin, waiting for the other side to give back the lock.
      // http://danglingpointers.com/post/spinlock-implementation/
      while(true) {
        SpinlockState desiredState =
          me == MAIN_PROCESS ? UNLOCKED_FOR_TAKING_BY_MAIN_PROCESS
                             : UNLOCKED_FOR_TAKING_BY_OTHERSIDE;
        if (spinlock.load() == desiredState) {  // check if lock is open, without invalidating cacheline
          // then do the actual CAS (which we expect to succeed)
          if (spinlock.compare_exchange_weak(desiredState, LOCKED)) break;
        }
        // tell speculative execution to not go crazy
        _mm_pause();
      }

      handleWakeup(me);  // we have now woken up from our waiting
    break;
    default:
    ERROR("Unrecognized Invoker::System\n")
  }
}

void Invoker::handleWakeup(Invoker::Identity me) {
  if(switchingSystems) {
    // We assert that only the otherside can wake up and find switchingSystems==true.
    if(me != OTHERSIDE) ERROR("Assertion violation: me != OTHERSIDE\n")
    // in this case we were woken up only to switch systems, not because
    // the main process "actually" wanted to wake us up.
    switch(curSystem) {
      case MUTEX_COND_SYSTEM:
        // Switch us over to the atomic-spinlock system:
        switchingSystems = false;
        curSystem = ATOMIC_SPINLOCK_SYSTEM;
        _mm_mfence();
        // Signal the main process to run (still on the mutex-cond system)
        deInitMutexCond(me);
        // begin waiting. Next call will come on the atomic-spinlock system.
        initAtomicSpinlock(me);
      break;
      case ATOMIC_SPINLOCK_SYSTEM:
        // Switch us over to the mutex-cond system:
        switchingSystems = false;
        curSystem = MUTEX_COND_SYSTEM;
        _mm_mfence();
        // Signal the main process to run (still on the atomic-spinlock system)
        deInitAtomicSpinlock(me);
        // begin waiting. Next call will come on the mutex-cond system.
        initMutexCond();  // perhaps it would be better to wait on the condition variable instead
                          // of just directly waiting on the mutex.
                          // But that would be dangerous, because it is *possible* that the main
                          // process has *already* (that is, since deInitAtomicSpinlock() finished)
                          // signalled the condition variable for the next context switch (depending
                          // on the scheduling/interleaving), in which case we would sleep forever.
                          // So we just wait on the mutex this first time.
      break;
      default:
      ERROR("Unrecognized Invoker::System\n")
    }
    handleWakeup(me);
  }
}

void Invoker::switchToAtomicSpinlock() {
  if(curSystem == ATOMIC_SPINLOCK_SYSTEM) return;  // nothing to do
  switchingSystems = true;
  invoke(MAIN_PROCESS);  // as noted in synch.h, we assert that only the main process calls this method
  initAtomicSpinlock(MAIN_PROCESS);
  // we don't release the mutex; we'll just have the main process hold the mutex for the entire duration of time we're in atomic-spinlock mode
}

void Invoker::switchToMutexCond() {
  if(curSystem == MUTEX_COND_SYSTEM) return;  // nothing to do
  switchingSystems = true;
  invoke(MAIN_PROCESS);  // as noted in synch.h, we assert that only the main process calls this method
  deInitAtomicSpinlock(MAIN_PROCESS);
}
