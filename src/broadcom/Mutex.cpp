/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Encapsulate a mutex for thread synchronization.
 */

#include "Mutex.h"
#include <errno.h>

#define LOG_TAG "nfcd"
#include <cutils/log.h>

Mutex::Mutex ()
{
  memset (&mMutex, 0, sizeof(mMutex));
  int res = pthread_mutex_init (&mMutex, NULL);
  if (res != 0) {
    ALOGE ("Mutex::Mutex: fail init; error=0x%X", res);
  }
}

Mutex::~Mutex ()
{
  int res = pthread_mutex_destroy (&mMutex);
  if (res != 0) {
    ALOGE ("Mutex::~Mutex: fail destroy; error=0x%X", res);
  }
}

void Mutex::lock ()
{
  int res = pthread_mutex_lock (&mMutex);
  if (res != 0) {
    ALOGE ("Mutex::lock: fail lock; error=0x%X", res);
  }
}

void Mutex::unlock ()
{
  int res = pthread_mutex_unlock (&mMutex);
  if (res != 0) {
    ALOGE ("Mutex::unlock: fail unlock; error=0x%X", res);
  }
}

bool Mutex::tryLock ()
{
  int res = pthread_mutex_trylock (&mMutex);
  if ((res != 0) && (res != EBUSY)) {
    ALOGE ("Mutex::tryLock: error=0x%X", res);
  }
  return res == 0;
}

pthread_mutex_t* Mutex::nativeHandle ()
{
  return &mMutex;
}
