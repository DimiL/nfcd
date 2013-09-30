/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once
#include <pthread.h>

/**
 *  Encapsulate a mutex for thread synchronization.
 */
class Mutex
{
public:
  Mutex();
  ~Mutex();

  /**
   * Block the thread and try lock the mutex.
   *
   * @return None.
   */
  void lock();

  /**
   * Unlock a mutex to unblock a thread.
   *
   * @return None.
   */
  void unlock();

  /**
   * Try to lock the mutex.
   *
   * @return True if the mutex is locked.
   */
  bool tryLock();

  /**
   * Get the handle of the mutex.
   *
   * @return Handle of the mutex.
   */
  pthread_mutex_t* nativeHandle();

  class Autolock 
  {
  public:
    inline Autolock(Mutex& mutex) : mLock(mutex)  { mLock.lock(); }
    inline Autolock(Mutex* mutex) : mLock(*mutex) { mLock.lock(); }
    inline ~Autolock() { mLock.unlock(); }
  private:
    Mutex& mLock;
  };

private:
  pthread_mutex_t mMutex;
};

typedef Mutex::Autolock AutoMutex;
