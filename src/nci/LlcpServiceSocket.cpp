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

#include "LlcpServiceSocket.h"

#include "LlcpSocket.h"
#include "ILlcpSocket.h"
#include "PeerToPeer.h"

#define LOG_TAG "NfcNci"
#include <cutils/log.h>

LlcpServiceSocket::LlcpServiceSocket(uint32_t aHandle,
                                     int aLocalLinearBufferLength,
                                     int aLocalMiu,
                                     int aLocalRw)
  : mHandle(aHandle)
  , mLocalLinearBufferLength(aLocalLinearBufferLength)
  , mLocalMiu(aLocalMiu)
  , mLocalRw(aLocalRw)
{
}

LlcpServiceSocket::~LlcpServiceSocket()
{
}

ILlcpSocket* LlcpServiceSocket::Accept()
{
  ALOGD("%s: enter", __FUNCTION__);

  const uint32_t serverHandle = mHandle;
  const uint32_t connHandle = PeerToPeer::GetInstance().GetNewHandle();
  bool stat = false;

  stat = PeerToPeer::GetInstance().Accept(serverHandle, connHandle, mLocalMiu, mLocalRw);
  if (!stat) {
    ALOGE("%s: fail accept", __FUNCTION__);
    return NULL;
  }

  LlcpSocket* clientSocket = new LlcpSocket(connHandle, mLocalMiu, mLocalRw);

  ALOGD("%s: exit", __FUNCTION__);
  return static_cast<ILlcpSocket*>(clientSocket);
}

bool LlcpServiceSocket::Close()
{
  ALOGD("%s: enter", __FUNCTION__);

  const uint32_t serverHandle = mHandle;
  bool stat = false;

  stat = PeerToPeer::GetInstance().DeregisterServer(serverHandle);
  if (!stat) {
    ALOGE("%s: fail deregister server", __FUNCTION__);
    return NULL;
  }

  ALOGD("%s: exit", __FUNCTION__);
  return true;
}
