/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Encapsulate a condition variable for thread synchronization.
 */

#include "CondVar.h"
#include <errno.h>

#define LOG_TAG "BroadcomNfc"
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

void CondVar::wait(Mutex& mutex)
{
  int const res = pthread_cond_wait(&mCondition, mutex.nativeHandle());
  if (res) {
    ALOGE("%s: fail wait; error=0x%X", __FUNCTION__, res);
  }
}

bool CondVar::wait(Mutex& mutex, long millisec)
{
  bool retVal = false;
  struct timespec absoluteTime;

  if (clock_gettime(CLOCK_MONOTONIC, &absoluteTime) == -1) {
    ALOGE("%s: fail get time; errno=0x%X", __FUNCTION__, errno);
  } else {
    absoluteTime.tv_sec += millisec / 1000;
    long ns = absoluteTime.tv_nsec + ((millisec % 1000) * 1000000);
    if (ns > 1000000000) {
      absoluteTime.tv_sec++;
      absoluteTime.tv_nsec = ns - 1000000000;
    } else {
      absoluteTime.tv_nsec = ns;
    }
  }
  //pthread_cond_timedwait_monotonic_np() is an Android-specific function
  //declared in /development/ndk/platforms/android-9/include/pthread.h;
  //it uses monotonic clock.
  //the standard pthread_cond_timedwait() uses realtime clock.
  int waitResult = pthread_cond_timedwait_monotonic_np(&mCondition, mutex.nativeHandle(), &absoluteTime);
  if ((waitResult != 0) && (waitResult != ETIMEDOUT)) {
    ALOGE("%s: fail timed wait; error=0x%X", __FUNCTION__, waitResult);
  }
  retVal = (waitResult == 0); //waited successfully
  return retVal;
}

void CondVar::notifyOne()
{
  int const res = pthread_cond_signal(&mCondition);
  if (res) {
    ALOGE("%s: fail signal; error=0x%X", __FUNCTION__, res);
  }
}
