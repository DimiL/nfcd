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
#include "NfcDebug.h"

Mutex::Mutex()
{
  memset(&mMutex, 0, sizeof(mMutex));
  const int res = pthread_mutex_init(&mMutex, NULL);
  if (res != 0) {
    NCI_ERROR("fail init; error=0x%X", res);
  }
}

Mutex::~Mutex()
{
  const int res = pthread_mutex_destroy(&mMutex);
  if (res != 0) {
    NCI_ERROR("fail destroy; error=0x%X", res);
  }
}

void Mutex::Lock()
{
  const int res = pthread_mutex_lock(&mMutex);
  if (res != 0) {
    NCI_ERROR("fail lock; error=0x%X", res);
  }
}

void Mutex::Unlock()
{
  const int res = pthread_mutex_unlock(&mMutex);
  if (res != 0) {
    NCI_ERROR("fail unlock; error=0x%X", res);
  }
}

bool Mutex::TryLock()
{
  const int res = pthread_mutex_trylock(&mMutex);
  if ((res != 0) && (res != EBUSY)) {
    NCI_ERROR("error=0x%X", res);
  }
  return res == 0;
}

pthread_mutex_t* Mutex::GetHandle()
{
  return &mMutex;
}
