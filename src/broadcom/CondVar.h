/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once
#include <pthread.h>
#include "Mutex.h"

/**
 *  Encapsulate a condition variable for thread synchronization.
 */
class CondVar
{
public:
  CondVar();
  ~CondVar();

  /**
   * Block the caller and wait for a condition.
   *
   * @param  mutex Lock.
   * @return       None.
   */
  void wait(Mutex& mutex);

  /**
   * Block the caller and wait for a condition.
   *
   * @param  mutex    Lock.
   * @param  millisec Timeout in milliseconds.
   * @return          True if wait is successful; false if timeout occurs.
   */
  bool wait(Mutex& mutex, long millisec);

  /**
   * Unblock the waiting thread. 
   *
   * @return None.
   */
  void notifyOne();

private:
  pthread_cond_t mCondition;
};
