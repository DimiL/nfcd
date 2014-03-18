/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  void start()
  {
    mMutex.lock();
  }

  /**
   * Block the thread and wait for the event to occur.
   *
   * @return None.
   */
  void wait()
  {
    mCondVar.wait(mMutex);
  }

  /**
   * Block the thread and wait for the event to occur.
   * 
   * @param  millisec Timeout in milliseconds.
   * @return          True if wait is successful; false if timeout occurs.
   */
  bool wait(long millisec)
  {
    bool retVal = mCondVar.wait(mMutex, millisec);
    return retVal;
  }

  /**
   * Notify a blocked thread that the event has occured. Unblocks it.
   *
   * @return None.
   */
  void notifyOne()
  {
    mCondVar.notifyOne();
  }

  /**
   * End a synchronization operation.
   *
   * @return None.
   */
  void end()
  {
    mMutex.unlock();
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
    event.start(); //automatically start operation
  };

  /**
   * End a synchronization operation.
   *
   * @return None.
   */
  ~SyncEventGuard()
  {
    mEvent.end(); //automatically end operation
  };

private:
  SyncEvent& mEvent;
};
