/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implement operations that provide compatibility with NXP
 * PN544 controller.  Specifically facilitate peer-to-peer
 * operations with PN544 controller.
 */
#include "Pn544Interop.h"

#include "NfcUtil.h"
#include "IntervalTimer.h"
#include "Mutex.h"
#include "NfcTag.h"

#undef LOG_TAG
#define LOG_TAG "BroadcomNfc"
#include <cutils/log.h>

extern void startStopPolling(bool isStartPolling);

static const int gIntervalTime = 1000; // Millisecond between the check to restore polling.
static IntervalTimer gTimer;
static Mutex gMutex;
static void pn544InteropStartPolling(union sigval); // Callback function for interval timer.
static bool gIsBusy = false;   // Is timer busy?
static bool gAbortNow = false; // Stop timer during next callback.

void pn544InteropStopPolling()
{
  ALOGD("%s: enter", __FUNCTION__);
  gMutex.lock();
  gTimer.kill();
  startStopPolling(false);
  gIsBusy = true;
  gAbortNow = false;
  gTimer.set(gIntervalTime, pn544InteropStartPolling); // After some time, start polling again.
  gMutex.unlock();
  ALOGD("%s: exit", __FUNCTION__);
}

void pn544InteropStartPolling(union sigval)
{
  ALOGD("%s: enter", __FUNCTION__);

  gMutex.lock();
  NfcTag::ActivationState state = NfcTag::getInstance().getActivationState();

  if (gAbortNow) {
    ALOGD("%s: abort now", __FUNCTION__);

    gIsBusy = false;
    goto TheEnd;
  }

  if (state == NfcTag::Idle) {
    ALOGD("%s: start polling", __FUNCTION__);

    startStopPolling(true);
    gIsBusy = false;
  } else {
    ALOGD("%s: try again later", __FUNCTION__);

    gTimer.set(gIntervalTime, pn544InteropStartPolling); // After some time, start polling again.
  }

TheEnd:
  gMutex.unlock();

  ALOGD("%s: exit", __FUNCTION__);
}

bool pn544InteropIsBusy()
{
  bool isBusy = false;
  gMutex.lock();
  isBusy = gIsBusy;
  gMutex.unlock();

  ALOGD("%s: %u", __FUNCTION__, isBusy);
  return isBusy;
}

void pn544InteropAbortNow()
{
  ALOGD("%s", __FUNCTION__);

  gMutex.lock();
  gAbortNow = true;
  gMutex.unlock();
}
