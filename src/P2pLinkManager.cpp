/*
 * Copyright (C) 2013-2014  Mozilla Foundation
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

#include "P2pLinkManager.h"

#include "NdefMessage.h"
#include "SnepMessage.h"
#include "SnepServer.h"
#include "SnepClient.h"
#include "HandoverServer.h"
#include "HandoverClient.h"
#include "NfcService.h"
#include "NfcDebug.h"

static const uint8_t RTD_HANDOVER_REQUEST[2] = {0x48, 0x72};  // "Hr"
static const uint8_t RTD_HANDOVER_SELECT[2] = {0x48, 0x73};   // "Hs"
static const uint8_t RTD_HANDOVER_CARRIER[2] = {0x48, 0x63};  // "Hc"
static const uint8_t RTD_HANDOVER_SIZE = 2;

enum HandoverType {
  NOT_HANDOVER = -1,
  HANDOVER_REQUEST,
  HANDOVER_SELECT,
  HANDOVER_CARRIER,
};

static P2pLinkManager* sP2pLinkManager = NULL;

SnepCallback::SnepCallback()
{
}

SnepCallback::~SnepCallback()
{
}

SnepMessage* SnepCallback::doPut(NdefMessage* ndef)
{
  if (!ndef) {
    ALOGE("%s: invalid parameter", FUNC);
    return NULL;
  }

  sP2pLinkManager->notifyNdefReceived(ndef);

  return SnepMessage::getMessage(SnepMessage::RESPONSE_SUCCESS);
}

// The NFC Forum Default SNEP server is not allowed to respond to
// SNEP GET requests - see SNEP 1.0 TS section 6.1. However,
// since Android 4.1 used the NFC Forum default server to
// implement connection handover, we will support this
// until we can deprecate it.
SnepMessage* SnepCallback::doGet(int acceptableLength, NdefMessage* ndef)
{
  if (!ndef) {
    ALOGE("%s: invalid parameter", FUNC);
    return NULL;
  }

  /**
   * Response Codes : NOT IMPLEMENTED
   * The server does not support the functionality required to fulfill
   * the request.
   */
  return SnepMessage::getMessage(SnepMessage::RESPONSE_NOT_IMPLEMENTED);
}

HandoverCallback::HandoverCallback()
{
}

HandoverCallback::~HandoverCallback()
{
}

void HandoverCallback::onMessageReceived(NdefMessage* ndef)
{
  sP2pLinkManager->notifyNdefReceived(ndef);
}

P2pLinkManager::P2pLinkManager(NfcService* service)
 : mLinkState(LINK_STATE_DOWN)
 , mSessionId(-1)
 , mSnepClient(NULL)
 , mHandoverClient(NULL)
{
  mSnepCallback = new SnepCallback();
  mSnepServer = new SnepServer(static_cast<ISnepCallback*>(mSnepCallback));

  mHandoverCallback = new HandoverCallback();
  mHandoverServer = new HandoverServer(static_cast<IHandoverCallback*>(mHandoverCallback));

  mNfcService = service;
  sP2pLinkManager = this;
}

P2pLinkManager::~P2pLinkManager()
{
  disconnectClients();

  delete mSnepCallback;
  delete mSnepServer;
  delete mHandoverCallback;
  delete mHandoverServer;
}

void P2pLinkManager::notifyNdefReceived(NdefMessage* ndef)
{
  mNfcService->onP2pReceivedNdef(ndef);
}

void P2pLinkManager::enableDisable(bool bEnable)
{
  if (bEnable) {
    mSnepServer->start();
    mHandoverServer->start();
  } else {
    mSnepServer->stop();
    mHandoverServer->stop();

    disconnectClients();
  }
}

// TODO : P2P push will block in llcp socket send/receive.
//        Maybe we should create a thread to do it ?
void P2pLinkManager::push(NdefMessage& ndef)
{
  if (ndef.mRecords.size() == 0) {
    ALOGE("%s: no NDEF record", FUNC);
    return;
  }

  // In current design nfcd only provide one "push" API to send a NDEF message through P2P link.
  // But nfcd will need to know if an NDEF message should be sent by SNEP client or HANDOVER client.
  // So parse NDEF message here to get correct client to send NDEF message.
  HandoverType handoverType = NOT_HANDOVER;
  NdefRecord* record = &(ndef.mRecords[0]);
  if (NdefRecord::TNF_WELL_KNOWN == record->mTnf && RTD_HANDOVER_SIZE == record->mType.size()) {
    std::vector<uint8_t>& type = record->mType;

    if ((type[0] == RTD_HANDOVER_REQUEST[0]) && (type[1] == RTD_HANDOVER_REQUEST[1])) {
      handoverType = HANDOVER_REQUEST;
    } else if ((type[0] == RTD_HANDOVER_SELECT[0])  && (type[1] == RTD_HANDOVER_SELECT[1])) {
      handoverType = HANDOVER_SELECT;
    }
  }

  // Handover Reuqest:
  // Hr is sent by handover client and will receive response Hs.
  if (HANDOVER_REQUEST == handoverType) {
    HandoverClient* pClient = getHandoverClient();
    if (pClient) {
      ALOGD("%s: send Handover Request by handover client", FUNC);
      NdefMessage* selectMsg = pClient->processHandoverRequest(ndef);
      if (selectMsg) {
        notifyNdefReceived(selectMsg);
        delete selectMsg;
      }
    } else {
      ALOGE("%s: handover client not connected", FUNC);
    }
  // Handover Select:
  // Hs is always sent in order to response for receiving Hr.
  // So we should send by the same socket receiving Hr (Hr is always received by HANDOVER server).
  } else if (HANDOVER_SELECT == handoverType) {
    if (mHandoverServer) {
      ALOGD("%s: send Handover Select by handover server", FUNC);
      mHandoverServer->put(ndef);
    } else {
      ALOGE("%s: handover server not created", FUNC);
    }
  // For all other non-handover message, send through SNEP protocol.
  } else {
    SnepClient* pClient = getSnepClient();
    if (pClient) {
      ALOGD("%s: send NDEF by SNEP client", FUNC);
      pClient->put(ndef);
    } else {
      ALOGE("%s: snep client not connected", FUNC);
    }
  }
}

void P2pLinkManager::onLlcpActivated()
{
  mLinkState = LINK_STATE_UP;
}

void P2pLinkManager::onLlcpDeactivated()
{
  mLinkState = LINK_STATE_DOWN;

  disconnectClients();
}

SnepClient* P2pLinkManager::getSnepClient()
{
  if (!mSnepClient) {
    mSnepClient = new SnepClient();
    if (!mSnepClient->connect()) {
      mSnepClient->close();
      delete mSnepClient;
      mSnepClient = NULL;
    }
  }
  return mSnepClient;
}

HandoverClient* P2pLinkManager::getHandoverClient()
{
  if (!mHandoverClient) {
    mHandoverClient = new HandoverClient();
    if (!mHandoverClient->connect()) {
      mHandoverClient->close();
      delete mHandoverClient;
      mHandoverClient = NULL;
    }
  }
  return mHandoverClient;
}

void P2pLinkManager::disconnectClients()
{
  if (mSnepClient) {
    mSnepClient->close();
    delete mSnepClient;
    mSnepClient = NULL;
  }
  if (mHandoverClient) {
    mHandoverClient->close();
    delete mHandoverClient;
    mHandoverClient = NULL;
  }
}

bool P2pLinkManager::isLlcpActive()
{
  return mLinkState != LINK_STATE_DOWN;
}
