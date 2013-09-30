/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <time.h>

/**
 *  Asynchronous interval timer.
 */
class IntervalTimer
{
public:
  typedef void (*TIMER_FUNC) (union sigval);

  IntervalTimer();
  ~IntervalTimer();

  bool set(int ms, TIMER_FUNC cb);
  void kill();
  bool create(TIMER_FUNC);

private:
  timer_t mTimerId;
  TIMER_FUNC mCb;
};
