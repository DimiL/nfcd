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

  uint32_t serverHandle = mHandle;
  uint32_t connHandle = PeerToPeer::getInstance().getNewHandle();
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

  uint32_t serverHandle = mHandle;
  bool stat = false;

  stat = PeerToPeer::getInstance().deregisterServer(serverHandle);

  ALOGD("%s: exit", __FUNCTION__);
  return true;
}
