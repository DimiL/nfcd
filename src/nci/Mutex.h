/*
 * Copyright (C) 2014  Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <pthread.h>

/**
 * Encapsulate a mutex for thread synchronization.
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
