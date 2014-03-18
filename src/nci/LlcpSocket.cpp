/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LlcpSocket.h"

#include "PeerToPeer.h"

#define LOG_TAG "NfcNci"
#include <cutils/log.h>

LlcpSocket::LlcpSocket(unsigned int handle, int sap, int miu, int rw)
  : mHandle(handle)
  , mSap(sap)
  , mLocalMiu(miu)
  , mLocalRw(rw)
{
}

LlcpSocket::LlcpSocket(unsigned int handle, int miu, int rw)
  : mHandle(handle)
  , mLocalMiu(miu)
  , mLocalRw(rw)
{
}

LlcpSocket::~LlcpSocket()
{
}

/**
 * Interface.
 */
bool LlcpSocket::connectToSap(int sap)
{
  return LlcpSocket::doConnect(sap);
}

bool LlcpSocket::connectToService(const char* serviceName)
{
  return LlcpSocket::doConnectBy(serviceName);
}

void LlcpSocket::close()
{
  LlcpSocket::doClose();
}

bool LlcpSocket::send(std::vector<uint8_t>& data)
{
  return LlcpSocket::doSend(data);
}

int LlcpSocket::receive(std::vector<uint8_t>& recvBuff)
{
  return LlcpSocket::doReceive(recvBuff);;
}

int LlcpSocket::getRemoteMiu() const
{
  return LlcpSocket::doGetRemoteSocketMIU();
}

int LlcpSocket::getRemoteRw() const
{
  return LlcpSocket::doGetRemoteSocketRW();
}

/**
 * Private function.
 */ 
bool LlcpSocket::doConnect(int nSap)
{
  ALOGD("%s: enter; sap=%d", __FUNCTION__, nSap);

  bool stat = PeerToPeer::getInstance().connectConnOriented(mHandle, nSap);
  if (!stat) {
    ALOGE("%s: fail connect oriented", __FUNCTION__);
  }

  ALOGD("%s: exit", __FUNCTION__);
  return stat;
}

bool LlcpSocket::doConnectBy(const char* sn)
{
  ALOGD("%s: enter; sn = %s", __FUNCTION__, sn);

  if (!sn) {
    return false;
  }
  bool stat = PeerToPeer::getInstance().connectConnOriented(mHandle, sn);
  if (!stat) {
    ALOGE("%s: fail connect connection oriented", __FUNCTION__);
  }

  ALOGD("%s: exit", __FUNCTION__);
  return stat;
}

bool LlcpSocket::doClose()
{
  ALOGD("%s: enter", __FUNCTION__);

  bool stat = PeerToPeer::getInstance().disconnectConnOriented(mHandle);
  if (!stat) {
    ALOGE("%s: fail disconnect connection oriented", __FUNCTION__);
  }

  ALOGD("%s: exit", __FUNCTION__);
  return true;  // TODO: stat?
}

bool LlcpSocket::doSend(std::vector<uint8_t>& data)
{
  UINT8* raw_ptr = new UINT8[data.size()];

  for(uint32_t i = 0; i < data.size(); i++)
    raw_ptr[i] = (UINT8)data[i];

  bool stat = PeerToPeer::getInstance().send(mHandle, raw_ptr, data.size());
  if (!stat) {
    ALOGE("%s: fail send", __FUNCTION__);
  }  

  delete[] raw_ptr;

  return stat;
}

int LlcpSocket::doReceive(std::vector<uint8_t>& recvBuff)
{
  const uint16_t MAX_BUF_SIZE = 4096;
  uint16_t actualLen = 0;

  UINT8* raw_ptr = new UINT8[MAX_BUF_SIZE];
  bool stat = PeerToPeer::getInstance().receive(mHandle, raw_ptr, MAX_BUF_SIZE, actualLen);

  int retval = 0;
  if (stat && (actualLen > 0)) {
    for (uint16_t i = 0; i < actualLen; i++)
      recvBuff.push_back(raw_ptr[i]);
    retval = actualLen;
  } else {
    retval = -1;
  }

  delete[] raw_ptr;

  return retval;
}

int LlcpSocket::doGetRemoteSocketMIU() const
{
  ALOGD("%s: enter", __FUNCTION__);

  int miu = PeerToPeer::getInstance().getRemoteMaxInfoUnit(mHandle);

  ALOGD("%s: exit", __FUNCTION__);
  return miu;
}

int LlcpSocket::doGetRemoteSocketRW() const
{
  ALOGD("%s: enter", __FUNCTION__);

  int rw = PeerToPeer::getInstance().getRemoteRecvWindow(mHandle);

  ALOGD("%s: exit", __FUNCTION__);
  return rw;
}
