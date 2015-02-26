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
 * Asynchronous interval timer.
 */
#include "IntervalTimer.h"

#define LOG_TAG "NfcNci"
#include <cutils/log.h>

IntervalTimer::IntervalTimer()
{
  mTimerId = 0;
  mCb = NULL;
}

bool IntervalTimer::Set(int aMs, TIMER_FUNC aCb)
{
  if (mTimerId == 0) {
    if (!aCb) {
      return false;
    }

    if (!Create(aCb)) {
      return false;
    }
  }
  if (aCb != mCb) {
    Kill();
    if (!Create(aCb)) {
      return false;
    }
  }

  int stat = 0;
  struct itimerspec ts;
  ts.it_value.tv_sec = aMs / 1000;
  ts.it_value.tv_nsec = (aMs % 1000) * 1000000;

  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;

  stat = timer_settime(mTimerId, 0, &ts, 0);
  if (stat == -1) {
    ALOGE("%s: fail set timer", __FUNCTION__);
  }
  return stat == 0;
}

IntervalTimer::~IntervalTimer()
{
  Kill();
}

void IntervalTimer::Kill()
{
  if (mTimerId == 0)
    return;

  timer_delete(mTimerId);
  mTimerId = 0;
  mCb = NULL;
}

bool IntervalTimer::Create(TIMER_FUNC aCb)
{
  struct sigevent se;
  int stat = 0;

  // Set the sigevent structure to cause the signal to be
  // delivered by creating a new thread.
  se.sigev_notify = SIGEV_THREAD;
  se.sigev_value.sival_ptr = &mTimerId;
  se.sigev_notify_function = aCb;
  se.sigev_notify_attributes = NULL;
  mCb = aCb;
  stat = timer_create(CLOCK_MONOTONIC, &se, &mTimerId);
  if (stat == -1) {
    ALOGE("%s: fail create timer", __FUNCTION__);
  }
  return stat == 0;
}
