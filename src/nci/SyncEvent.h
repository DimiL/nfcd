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
#include "CondVar.h"
#include "Mutex.h"

/**
 * Synchronize two or more threads using a condition variable and a mutex.
 */
class SyncEvent
{
public:
  ~SyncEvent() {}

  /**
   * Start a synchronization operation.
   *
   * @return None.
   */
  void Start()
  {
    mMutex.Lock();
  }

  /**
   * Block the thread and wait for the event to occur.
   *
   * @return None.
   */
  void Wait()
  {
    mCondVar.Wait(mMutex);
  }

  /**
   * Block the thread and wait for the event to occur.
   *
   * @param  millisec Timeout in milliseconds.
   * @return          True if wait is successful; false if timeout occurs.
   */
  bool Wait(long aMillisec)
  {
    return mCondVar.Wait(mMutex, aMillisec);
  }

  /**
   * Notify a blocked thread that the event has occured. Unblocks it.
   *
   * @return None.
   */
  void NotifyOne()
  {
    mCondVar.NotifyOne();
  }

  /**
   * End a synchronization operation.
   *
   * @return None.
   */
  void End()
  {
    mMutex.Unlock();
  }

private:
  CondVar mCondVar;
  Mutex mMutex;
};

/**
 * Automatically start and end a synchronization event.
 */
class SyncEventGuard
{
public:

  /**
   * Start a synchronization operation.
   *
   * @return None.
   */
  SyncEventGuard(SyncEvent& event)
    : mEvent(event)
  {
    event.Start(); //automatically start operation
  };

  /**
   * End a synchronization operation.
   *
   * @return None.
   */
  ~SyncEventGuard()
  {
    mEvent.End(); //automatically end operation
  };

private:
  SyncEvent& mEvent;
};
