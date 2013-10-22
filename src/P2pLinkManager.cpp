#include "P2pLinkManager.h"

#include "NdefMessage.h"
#include "SnepMessage.h"
#include "SnepServer.h"
#include "SnepClient.h"
#include "HandoverServer.h"
#include "HandoverClient.h"
#include "NfcService.h"

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include "utils/Log.h"

static const char* HANDOVER_REQUEST = "urn:nfc:wkt:Hr";
static const char* HANDOVER_SELECT = "urn:nfc:wkt:Hs";
static const char* HANDOVER_CARRIER = "urn:nfc:wkt:Hc";

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
    ALOGE("%s: invalid parameter", __FUNCTION__);
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
    ALOGE("%s: invalid parameter", __FUNCTION__);
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
 : mSnepClient(NULL)
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
  mNfcService->onP2pReceiveNdef(ndef);
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

void P2pLinkManager::push(NdefMessage* ndef)
{
  if (!ndef)
    return;

  // Handover protocol is processed in upper layer. Currently gonk-ptotocol support one
  // API called push to send NDEF message through a P2P link, but nfcd need to know if
  // an NDEF message should be sent by SNEP or HANDOVER client.
  // So we parse the NDEF message here to know if should sent through HANDOVER.
  bool handover = false;
  if (ndef->mRecords.size() > 0) {
    NdefRecord* record = &(ndef->mRecords[0]);
    if (NdefRecord::TNF_WELL_KNOWN == record->mTnf) {
      const int size = record->mType.size();
      char* type = new char[size];
      for(int i = 0; i < size; i++) {
        type[i] = record->mType[i];
      }
      
      if (strncmp(HANDOVER_REQUEST, type, size) == 0 ||
          strncmp(HANDOVER_SELECT, type, size) == 0 ||
          strncmp(HANDOVER_CARRIER, type, size) == 0) {
        handover = true;
      }
  
      delete type;
    }
  }

  if (handover) {
    ALOGD("%s: pushed by handover protocol", __FUNCTION__);
    if (mHandoverClient)
      mHandoverClient->put(*ndef);
    else
      ALOGE("%s: handover client not connected", __FUNCTION__);
  } else {
    if (mSnepClient)
      mSnepClient->put(*ndef);
    else
      ALOGE("%s: snep client not connected", __FUNCTION__);
  }
}

void P2pLinkManager::onLlcpActivated()
{
  // Connect SNEP/HANDOVER client once llcp is activated.
  connectClients();
}

void P2pLinkManager::onLlcpDeactivated()
{
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