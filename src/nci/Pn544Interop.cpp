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
 * Implement operations that provide compatibility with NXP
 * PN544 controller.  Specifically facilitate peer-to-peer
 * operations with PN544 controller.
 */
#include "Pn544Interop.h"

#include "NfcNciUtil.h"
#include "IntervalTimer.h"
#include "Mutex.h"
#include "NfcTag.h"

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

extern void StartStopPolling(bool aIsStartPolling);
static void Pn544InteropStartPolling(union sigval); // Callback function for interval timer.

static const int gIntervalTime = 1000; // Millisecond between the check to restore polling.
static IntervalTimer gTimer;
static Mutex gMutex;
static bool gIsBusy = false;   // Is timer busy?
static bool gAbortNow = false; // Stop timer during next callback.

void Pn544InteropStopPolling()
{
  ALOGD("%s: enter", __FUNCTION__);
  gMutex.Lock();
  gTimer.Kill();
  StartStopPolling(false);
  gIsBusy = true;
  gAbortNow = false;
  gTimer.Set(gIntervalTime, Pn544InteropStartPolling); // After some time, start polling again.
  gMutex.Unlock();
  ALOGD("%s: exit", __FUNCTION__);
}

void Pn544InteropStartPolling(union sigval)
{
  ALOGD("%s: enter", __FUNCTION__);

  gMutex.Lock();
  NfcTag::ActivationState state = NfcTag::GetInstance().GetActivationState();

  if (gAbortNow) {
    ALOGD("%s: abort now", __FUNCTION__);

    gIsBusy = false;
    goto TheEnd;
  }

  if (state == NfcTag::Idle) {
    ALOGD("%s: start polling", __FUNCTION__);

    StartStopPolling(true);
    gIsBusy = false;
  } else {
    ALOGD("%s: try again later", __FUNCTION__);

    gTimer.Set(gIntervalTime, Pn544InteropStartPolling); // After some time, start polling again.
  }

TheEnd:
  gMutex.Unlock();

  ALOGD("%s: exit", __FUNCTION__);
}

bool Pn544InteropIsBusy()
{
  bool isBusy = false;
  gMutex.Lock();
  isBusy = gIsBusy;
  gMutex.Unlock();

  ALOGD("%s: %u", __FUNCTION__, isBusy);
  return isBusy;
}

void Pn544InteropAbortNow()
{
  ALOGD("%s", __FUNCTION__);

  gMutex.Lock();
  gAbortNow = true;
  gMutex.Unlock();
}
