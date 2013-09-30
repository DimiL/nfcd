/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Asynchronous interval timer.
 */

#include "IntervalTimer.h"

#define LOG_TAG "BroadcomNfc"
#include <cutils/log.h>

IntervalTimer::IntervalTimer()
{
  mTimerId = 0;
  mCb = NULL;
}

bool IntervalTimer::set(int ms, TIMER_FUNC cb)
{
  if (mTimerId == 0) {
    if (cb == NULL)
      return false;

    if (!create(cb))
      return false;
  }
  if (cb != mCb) {
    kill();
    if (!create(cb))
      return false;
  }

  int stat = 0;
  struct itimerspec ts;
  ts.it_value.tv_sec = ms / 1000;
  ts.it_value.tv_nsec = (ms % 1000) * 1000000;

  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;

  stat = timer_settime(mTimerId, 0, &ts, 0);
  if (stat == -1)
    ALOGE("%s: fail set timer", __FUNCTION__);
  return stat == 0;
}

IntervalTimer::~IntervalTimer()
{
  kill();
}

void IntervalTimer::kill()
{
  if (mTimerId == 0)
    return;

  timer_delete(mTimerId);
  mTimerId = 0;
  mCb = NULL;
}

bool IntervalTimer::create(TIMER_FUNC cb)
{
  struct sigevent se;
  int stat = 0;

  // Set the sigevent structure to cause the signal to be
  // delivered by creating a new thread.
  se.sigev_notify = SIGEV_THREAD;
  se.sigev_value.sival_ptr = &mTimerId;
  se.sigev_notify_function = cb;
  se.sigev_notify_attributes = NULL;
  mCb = cb;
  stat = timer_create(CLOCK_MONOTONIC, &se, &mTimerId);
  if (stat == -1)
    ALOGE("%s: fail create timer", __FUNCTION__);
  return stat == 0;
}
