/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Synchronize two or more threads using a condition variable and a mutex.
 */
#pragma once
#include "CondVar.h"
#include "Mutex.h"

class SyncEvent
{
public:
  ~SyncEvent()
  {
  }

  void start()
  {
    mMutex.lock();
  }

  void wait()
  {
    mCondVar.wait(mMutex);
  }

  bool wait(long millisec)
  {
    bool retVal = mCondVar.wait(mMutex, millisec);
    return retVal;
  }

  void notifyOne()
  {
    mCondVar.notifyOne();
  }

  void end()
  {
    mMutex.unlock();
  }

private:
  CondVar mCondVar;
  Mutex mMutex;
};

class SyncEventGuard
{
public:
  SyncEventGuard(SyncEvent& event)
    : mEvent(event)
  {
    event.start(); //automatically start operation
  };

  ~SyncEventGuard()
  {
    mEvent.end(); //automatically end operation
  };

private:
  SyncEvent& mEvent;
};

