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
    } else if ((type[0] == RTD_HANDOVER_CARRIER[0]) && (type[1] == RTD_HANDOVER_CARRIER[1])) {
      handoverType = HANDOVER_CARRIER;
    }
  }

  if (handoverType != NOT_HANDOVER) {
    if (mHandoverClient) {
      ALOGD("%s: send NDEF by HANDOVER client", FUNC);
      if (HANDOVER_REQUEST == handoverType) {
        NdefMessage* selectMsg = mHandoverClient->processHandoverRequest(ndef);
        notifyNdefReceived(selectMsg);
        delete selectMsg;
      } else {
        mHandoverClient->put(ndef);
      }
    } else {
      ALOGE("%s: handover client not connected", FUNC);
    }
  } else {
    if (mSnepClient) {
      ALOGD("%s: send NDEF by SNEP client", FUNC);
      mSnepClient->put(ndef);
    } else {
      ALOGE("%s: snep client not connected", FUNC);
    }
  }
}

void P2pLinkManager::onLlcpActivated()
{
  mLinkState = LINK_STATE_UP;

  // Connect SNEP/HANDOVER client once llcp is activated.
  connectClients();
}

void P2pLinkManager::onLlcpDeactivated()
{
  mLinkState = LINK_STATE_DOWN;

  disconnectClients();
}

void P2pLinkManager::connectClients()
{
  if (!mSnepClient) {
    mSnepClient = new SnepClient();
    if (!mSnepClient->connect()) {
      mSnepClient->close();
      delete mSnepClient;
      mSnepClient = NULL;
    }
  }

  if (!mHandoverClient) {
    mHandoverClient = new HandoverClient();
    if (!mHandoverClient->connect()) {
      mHandoverClient->close();
      delete mHandoverClient;
      mHandoverClient = NULL;
    }
  }
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
