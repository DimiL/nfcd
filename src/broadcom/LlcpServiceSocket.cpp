/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LlcpServiceSocket.h"

#include "LlcpSocket.h"
#include "ILlcpSocket.h"
#include "PeerToPeer.h"

#define LOG_TAG "BroadcomNfc"
#include <cutils/log.h>

LlcpServiceSocket::LlcpServiceSocket(uint32_t handle, int localLinearBufferLength, int localMiu, int localRw)
  : mHandle(handle)
  , mLocalLinearBufferLength(localLinearBufferLength)
  , mLocalMiu(localMiu)
  , mLocalRw(localRw)
{
}

LlcpServiceSocket::~LlcpServiceSocket()
{
}

ILlcpSocket* LlcpServiceSocket::accept()
{
  ALOGD("%s: enter", __FUNCTION__);

  const uint32_t serverHandle = mHandle;
  const uint32_t connHandle = PeerToPeer::getInstance().getNewHandle();
  bool stat = false;

  stat = PeerToPeer::getInstance().accept(serverHandle, connHandle, mLocalMiu, mLocalRw);
  if (!stat) {
    ALOGE("%s: fail accept", __FUNCTION__);
    return NULL;
  }

  LlcpSocket* clientSocket = new LlcpSocket(connHandle, mLocalMiu, mLocalRw);

  ALOGD("%s: exit", __FUNCTION__);
  return static_cast<ILlcpSocket*>(clientSocket);
}

bool LlcpServiceSocket::close()
{
  ALOGD("%s: enter", __FUNCTION__);

  const uint32_t serverHandle = mHandle;
  bool stat = false;

  stat = PeerToPeer::getInstance().deregisterServer(serverHandle);
  if (!stat) {
    ALOGE("%s: fail deregister server", __FUNCTION__);
    return NULL;
  }

  ALOGD("%s: exit", __FUNCTION__);
  return true;
}
