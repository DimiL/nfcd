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

#include "NfcDebug.h"
#include "NfcNciUtil.h"
#include "IntervalTimer.h"
#include "Mutex.h"
#include "NfcTag.h"

extern void StartStopPolling(bool aIsStartPolling);
static void Pn544InteropStartPolling(union sigval); // Callback function for interval timer.

static const int gIntervalTime = 1000; // Millisecond between the check to restore polling.
static IntervalTimer gTimer;
static Mutex gMutex;
static bool gIsBusy = false;   // Is timer busy?
static bool gAbortNow = false; // Stop timer during next callback.

void Pn544InteropStopPolling()
{
  NCI_DEBUG("enter");
  gMutex.Lock();
  gTimer.Kill();
  StartStopPolling(false);
  gIsBusy = true;
  gAbortNow = false;
  gTimer.Set(gIntervalTime, Pn544InteropStartPolling); // After some time, start polling again.
  gMutex.Unlock();
  NCI_DEBUG("exit");
}

void Pn544InteropStartPolling(union sigval)
{
  NCI_DEBUG("enter");

  gMutex.Lock();
  NfcTag::ActivationState state = NfcTag::GetInstance().GetActivationState();

  if (gAbortNow) {
    NCI_DEBUG("abort now");

    gIsBusy = false;
    goto TheEnd;
  }

  if (state == NfcTag::Idle) {
    NCI_DEBUG("start polling");

    StartStopPolling(true);
    gIsBusy = false;
  } else {
    NCI_DEBUG("try again later");

    gTimer.Set(gIntervalTime, Pn544InteropStartPolling); // After some time, start polling again.
  }

TheEnd:
  gMutex.Unlock();

  NCI_DEBUG("exit");
}

bool Pn544InteropIsBusy()
{
  bool isBusy = false;
  gMutex.Lock();
  isBusy = gIsBusy;
  gMutex.Unlock();

  NCI_DEBUG("%u", isBusy);
  return isBusy;
}

void Pn544InteropAbortNow()
{
  NCI_DEBUG("enter");

  gMutex.Lock();
  gAbortNow = true;
  gMutex.Unlock();
}
