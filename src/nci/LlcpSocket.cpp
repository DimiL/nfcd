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

#include "LlcpSocket.h"

#include "PeerToPeer.h"

#define LOG_TAG "NfcNci"
#include <cutils/log.h>

LlcpSocket::LlcpSocket(unsigned int aHandle, int aSap, int aMiu, int aRw)
  : mHandle(aHandle)
  , mSap(aSap)
  , mLocalMiu(aMiu)
  , mLocalRw(aRw)
{
}

LlcpSocket::LlcpSocket(unsigned int aHandle, int aMiu, int aRw)
  : mHandle(aHandle)
  , mLocalMiu(aMiu)
  , mLocalRw(aRw)
{
}

LlcpSocket::~LlcpSocket()
{
}

/**
 * Interfaces.
 */
bool LlcpSocket::ConnectToSap(int aSap)
{
  return LlcpSocket::DoConnect(aSap);
}

bool LlcpSocket::ConnectToService(const char* aSn)
{
  return LlcpSocket::DoConnectBy(aSn);
}

void LlcpSocket::Close()
{
  LlcpSocket::DoClose();
}

bool LlcpSocket::Send(std::vector<uint8_t>& aData)
{
  return LlcpSocket::DoSend(aData);
}

int LlcpSocket::Receive(std::vector<uint8_t>& aRecvBuf)
{
  return LlcpSocket::DoReceive(aRecvBuf);;
}

int LlcpSocket::GetRemoteMiu() const
{
  return LlcpSocket::DoGetRemoteSocketMIU();
}

int LlcpSocket::GetRemoteRw() const
{
  return LlcpSocket::DoGetRemoteSocketRW();
}

/**
 * Private function.
 */
bool LlcpSocket::DoConnect(int aSap)
{
  ALOGD("%s: enter; sap=%d", __FUNCTION__, aSap);

  bool stat = PeerToPeer::GetInstance().ConnectConnOriented(mHandle, aSap);
  if (!stat) {
    ALOGE("%s: fail connect oriented", __FUNCTION__);
  }

  ALOGD("%s: exit", __FUNCTION__);
  return stat;
}

bool LlcpSocket::DoConnectBy(const char* aSn)
{
  ALOGD("%s: enter; sn = %s", __FUNCTION__, aSn);

  if (!aSn) {
    return false;
  }
  bool stat = PeerToPeer::GetInstance().ConnectConnOriented(mHandle, aSn);
  if (!stat) {
    ALOGE("%s: fail connect connection oriented", __FUNCTION__);
  }

  ALOGD("%s: exit", __FUNCTION__);
  return stat;
}

bool LlcpSocket::DoClose()
{
  ALOGD("%s: enter", __FUNCTION__);

  bool stat = PeerToPeer::GetInstance().DisconnectConnOriented(mHandle);
  if (!stat) {
    ALOGE("%s: fail disconnect connection oriented", __FUNCTION__);
  }

  ALOGD("%s: exit", __FUNCTION__);
  return true;  // TODO: stat?
}

bool LlcpSocket::DoSend(std::vector<uint8_t>& aData)
{
  UINT8* raw_ptr = new UINT8[aData.size()];

  for(uint32_t i = 0; i < aData.size(); i++) {
    raw_ptr[i] = (UINT8)aData[i];
  }

  bool stat = PeerToPeer::GetInstance().Send(mHandle, raw_ptr, aData.size());
  if (!stat) {
    ALOGE("%s: fail send", __FUNCTION__);
  }

  delete[] raw_ptr;

  return stat;
}

int LlcpSocket::DoReceive(std::vector<uint8_t>& aRecvBuf)
{
  const uint16_t MAX_BUF_SIZE = 4096;
  uint16_t actualLen = 0;

  UINT8* raw_ptr = new UINT8[MAX_BUF_SIZE];
  bool stat = PeerToPeer::GetInstance().Receive(mHandle, raw_ptr, MAX_BUF_SIZE, actualLen);

  int retval = 0;
  if (stat && (actualLen > 0)) {
    for (uint16_t i = 0; i < actualLen; i++) {
      aRecvBuf.push_back(raw_ptr[i]);
    }
    retval = actualLen;
  } else {
    retval = -1;
  }

  delete[] raw_ptr;

  return retval;
}

int LlcpSocket::DoGetRemoteSocketMIU() const
{
  ALOGD("%s: enter", __FUNCTION__);

  int miu = PeerToPeer::GetInstance().GetRemoteMaxInfoUnit(mHandle);

  ALOGD("%s: exit", __FUNCTION__);
  return miu;
}

int LlcpSocket::DoGetRemoteSocketRW() const
{
  ALOGD("%s: enter", __FUNCTION__);

  int rw = PeerToPeer::GetInstance().GetRemoteRecvWindow(mHandle);

  ALOGD("%s: exit", __FUNCTION__);
  return rw;
}
