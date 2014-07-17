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
