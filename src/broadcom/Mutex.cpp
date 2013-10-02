/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Encapsulate a mutex for thread synchronization.
 */
#include "Mutex.h"
#include <errno.h>

#define LOG_TAG "BroadcomNfc"
#include <cutils/log.h>

Mutex::Mutex()
{
  memset(&mMutex, 0, sizeof(mMutex));
  const int res = pthread_mutex_init(&mMutex, NULL);
  if (res != 0) {
    ALOGE("%s: fail init; error=0x%X", __FUNCTION__, res);
  }
}

Mutex::~Mutex()
{
  const int res = pthread_mutex_destroy(&mMutex);
  if (res != 0) {
    ALOGE("%s: fail destroy; error=0x%X", __FUNCTION__, res);
  }
}

void Mutex::lock()
{
  const int res = pthread_mutex_lock(&mMutex);
  if (res != 0) {
    ALOGE("%s: fail lock; error=0x%X", __FUNCTION__, res);
  }
}

void Mutex::unlock()
{
  const int res = pthread_mutex_unlock(&mMutex);
  if (res != 0) {
    ALOGE("%s: fail unlock; error=0x%X", __FUNCTION__, res);
  }
}

bool Mutex::tryLock()
{
  const int res = pthread_mutex_trylock(&mMutex);
  if ((res != 0) && (res != EBUSY)) {
    ALOGE("%s: error=0x%X", __FUNCTION__, res);
  }
  return res == 0;
}

pthread_mutex_t* Mutex::nativeHandle()
{
  return &mMutex;
}
