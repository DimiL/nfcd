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

/**
 *  Encapsulate a condition variable for thread synchronization.
 */
#include "CondVar.h"
#include <errno.h>

#define LOG_TAG "NfcNci"
#include <cutils/log.h>

CondVar::CondVar()
{
  memset(&mCondition, 0, sizeof(mCondition));
  int const res = pthread_cond_init(&mCondition, NULL);
  if (res) {
    ALOGE("%s: fail init; error=0x%X", __FUNCTION__, res);
  }
}

CondVar::~CondVar()
{
  int const res = pthread_cond_destroy(&mCondition);
  if (res) {
    ALOGE("%s: fail destroy; error=0x%X", __FUNCTION__, res);
  }
}

void CondVar::Wait(Mutex& aMutex)
{
  int const res = pthread_cond_wait(&mCondition, aMutex.GetHandle());
  if (res) {
    ALOGE("%s: fail wait; error=0x%X", __FUNCTION__, res);
  }
}

bool CondVar::Wait(Mutex& aMutex, long aMillisec)
{
  bool retVal = false;
  struct timespec absoluteTime;

  if (clock_gettime(CLOCK_MONOTONIC, &absoluteTime) == -1) {
    ALOGE("%s: fail get time; errno=0x%X", __FUNCTION__, errno);
  } else {
    absoluteTime.tv_sec += aMillisec / 1000;
    long ns = absoluteTime.tv_nsec + ((aMillisec % 1000) * 1000000);
    if (ns > 1000000000) {
      absoluteTime.tv_sec++;
      absoluteTime.tv_nsec = ns - 1000000000;
    } else {
      absoluteTime.tv_nsec = ns;
    }
  }
  // pthread_cond_timedwait_monotonic_np() is an Android-specific function.
  // Declared in /development/ndk/platforms/android-9/include/pthread.h.
  // It uses monotonic clock.
  // The standard pthread_cond_timedwait() uses realtime clock.
  const int waitResult = pthread_cond_timedwait_monotonic_np(&mCondition, aMutex.GetHandle(), &absoluteTime);
  if ((waitResult != 0) && (waitResult != ETIMEDOUT)) {
    ALOGE("%s: fail timed wait; error=0x%X", __FUNCTION__, waitResult);
  }
  retVal = (waitResult == 0); // Waited successfully.
  return retVal;
}

void CondVar::NotifyOne()
{
  const int res = pthread_cond_signal(&mCondition);
  if (res) {
    ALOGE("%s: fail signal; error=0x%X", __FUNCTION__, res);
  }
}
