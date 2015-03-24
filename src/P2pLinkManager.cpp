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

SnepMessage* SnepCallback::DoPut(NdefMessage* aNdef)
{
  if (!aNdef) {
    NFCD_ERROR("invalid parameter");
    return NULL;
  }

  sP2pLinkManager->NotifyNdefReceived(aNdef);

  return SnepMessage::GetMessage(SnepMessage::RESPONSE_SUCCESS);
}

// The NFC Forum Default SNEP server is not allowed to respond to
// SNEP GET requests - see SNEP 1.0 TS section 6.1. However,
// since Android 4.1 used the NFC Forum default server to
// implement connection handover, we will support this
// until we can deprecate it.
SnepMessage* SnepCallback::DoGet(int aAcceptableLength,
                                 NdefMessage* aNdef)
{
  if (!aNdef) {
    NFCD_ERROR("invalid parameter");
    return NULL;
  }

  /**
   * Response Codes : NOT IMPLEMENTED
   * The server does not support the functionality required to fulfill
   * the request.
   */
  return SnepMessage::GetMessage(SnepMessage::RESPONSE_NOT_IMPLEMENTED);
}

HandoverCallback::HandoverCallback()
{
}

HandoverCallback::~HandoverCallback()
{
}

void HandoverCallback::OnMessageReceived(NdefMessage* aNdef)
{
  sP2pLinkManager->NotifyNdefReceived(aNdef);
}

P2pLinkManager::P2pLinkManager(NfcService* aService)
 : mLinkState(LINK_STATE_DOWN)
 , mSessionId(-1)
 , mSnepClient(NULL)
 , mHandoverClient(NULL)
{
  mSnepCallback = new SnepCallback();
  mSnepServer = new SnepServer(static_cast<ISnepCallback*>(mSnepCallback));

  mHandoverCallback = new HandoverCallback();
  mHandoverServer = new HandoverServer(static_cast<IHandoverCallback*>(mHandoverCallback));

  mNfcService = aService;
  sP2pLinkManager = this;
}

P2pLinkManager::~P2pLinkManager()
{
  DisconnectClients();

  delete mSnepCallback;
  delete mSnepServer;
  delete mHandoverCallback;
  delete mHandoverServer;
}

void P2pLinkManager::NotifyNdefReceived(NdefMessage* aNdef)
{
  mNfcService->OnP2pReceivedNdef(aNdef);
}

void P2pLinkManager::EnableDisable(bool aEnable)
{
  if (aEnable) {
    mSnepServer->Start();
    mHandoverServer->Start();
  } else {
    mSnepServer->Stop();
    mHandoverServer->Stop();

    DisconnectClients();
  }
}

// TODO : P2P push will block in llcp socket send/receive.
//        Maybe we should create a thread to do it ?
void P2pLinkManager::Push(NdefMessage& aNdef)
{
  if (aNdef.mRecords.size() == 0) {
    NFCD_ERROR("no NDEF record");
    return;
  }

  // In current design nfcd only provide one "push" API to send a NDEF message through P2P link.
  // But nfcd will need to know if an NDEF message should be sent by SNEP client or HANDOVER client.
  // So parse NDEF message here to get correct client to send NDEF message.
  HandoverType handoverType = NOT_HANDOVER;
  NdefRecord* record = &(aNdef.mRecords[0]);
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
    HandoverClient* pClient = GetHandoverClient();
    if (pClient) {
      NFCD_DEBUG("send Handover Request by handover client");
      NdefMessage* selectMsg = pClient->ProcessHandoverRequest(aNdef);
      if (selectMsg) {
        NotifyNdefReceived(selectMsg);
        delete selectMsg;
      }
    } else {
      NFCD_ERROR("handover client not connected");
    }
  // Handover Select:
  // Hs is always sent in order to response for receiving Hr.
  // So we should send by the same socket receiving Hr (Hr is always received by HANDOVER server).
  } else if (HANDOVER_SELECT == handoverType) {
    if (mHandoverServer) {
      NFCD_DEBUG("send Handover Select by handover server");
      mHandoverServer->Put(aNdef);
    } else {
      NFCD_ERROR("handover server not created");
    }
  // For all other non-handover message, send through SNEP protocol.
  } else {
    SnepClient* pClient = GetSnepClient();
    if (pClient) {
      NFCD_DEBUG("send NDEF by SNEP client");
      pClient->Put(aNdef);
    } else {
      NFCD_ERROR("snep client not connected");
    }
  }
}

void P2pLinkManager::OnLlcpActivated()
{
  mLinkState = LINK_STATE_UP;
}

void P2pLinkManager::OnLlcpDeactivated()
{
  mLinkState = LINK_STATE_DOWN;

  DisconnectClients();
}

SnepClient* P2pLinkManager::GetSnepClient()
{
  if (!mSnepClient) {
    mSnepClient = new SnepClient();
    if (!mSnepClient->Connect()) {
      mSnepClient->Close();
      delete mSnepClient;
      mSnepClient = NULL;
    }
  }
  return mSnepClient;
}

HandoverClient* P2pLinkManager::GetHandoverClient()
{
  if (!mHandoverClient) {
    mHandoverClient = new HandoverClient();
    if (!mHandoverClient->Connect()) {
      mHandoverClient->Close();
      delete mHandoverClient;
      mHandoverClient = NULL;
    }
  }
  return mHandoverClient;
}

void P2pLinkManager::DisconnectClients()
{
  if (mSnepClient) {
    mSnepClient->Close();
    delete mSnepClient;
    mSnepClient = NULL;
  }
  if (mHandoverClient) {
    mHandoverClient->Close();
    delete mHandoverClient;
    mHandoverClient = NULL;
  }
}

bool P2pLinkManager::IsLlcpActive()
{
  return mLinkState != LINK_STATE_DOWN;
}
