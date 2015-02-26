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
 * Encapsulate a mutex for thread synchronization.
 */
#include "Mutex.h"
#include <errno.h>

#define LOG_TAG "NfcNci"
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

void Mutex::Lock()
{
  const int res = pthread_mutex_lock(&mMutex);
  if (res != 0) {
    ALOGE("%s: fail lock; error=0x%X", __FUNCTION__, res);
  }
}

void Mutex::Unlock()
{
  const int res = pthread_mutex_unlock(&mMutex);
  if (res != 0) {
    ALOGE("%s: fail unlock; error=0x%X", __FUNCTION__, res);
  }
}

bool Mutex::TryLock()
{
  const int res = pthread_mutex_trylock(&mMutex);
  if ((res != 0) && (res != EBUSY)) {
    ALOGE("%s: error=0x%X", __FUNCTION__, res);
  }
  return res == 0;
}

pthread_mutex_t* Mutex::GetHandle()
{
  return &mMutex;
}
