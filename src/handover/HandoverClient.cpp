#include "HandoverClient.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "HandoverServer.h"
#include "ILlcpSocket.h"
#include "NdefMessage.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

HandoverClient::HandoverClient()
{
  mState = HandoverClient::DISCONNECTED;
  mServiceName = HandoverServer::DEFAULT_SERVICE_NAME;
  mMiu = HandoverClient::DEFAULT_MIU;
}

HandoverClient::~HandoverClient()
{
  close();
}

void HandoverClient::connect()
{
  ALOGD("%s: enter", __FUNCTION__);

  if (mState != HandoverClient::DISCONNECTED) {
    ALOGE("%s: socket already in use", __FUNCTION__);
    return;
  }
  mState = HandoverClient::CONNECTING;

  INfcManager* pINfcManager = NfcService::getNfcManager();

  mSocket = pINfcManager->createLlcpSocket(0, mMiu, 1, 1024);
  if (!mSocket) {
    ALOGE("%s: could not connect to socket", __FUNCTION__);
    mState = HandoverClient::DISCONNECTED;
    return;
  }

  if (!mSocket->connectToService(mServiceName)) {
    ALOGE("%s: could not connect to service (%s)", __FUNCTION__, mServiceName);
    delete mSocket;
    mState = HandoverClient::DISCONNECTED;
    return;
  }

  mState = HandoverClient::CONNECTED;

  ALOGD("%s: exit", __FUNCTION__);
}

void HandoverClient::put(NdefMessage& msg)
{
  ALOGD("%s: enter", __FUNCTION__);

  if (mState != HandoverClient::CONNECTED) {
    ALOGE("%s: not connected", __FUNCTION__);
    return;
  }

  std::vector<uint8_t> buf;
  msg.toByteArray(buf);
  mSocket->send(buf);

  ALOGD("%s: exit", __FUNCTION__);
}

void HandoverClient::close()
{
  ALOGD("%s: enter", __FUNCTION__);

  if (mSocket) {
    mSocket->close();
  }

  mState = HandoverClient::DISCONNECTED;

  ALOGD("%s: exit", __FUNCTION__);
}
