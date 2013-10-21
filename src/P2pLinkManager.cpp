#include "P2pLinkManager.h"

#include "NdefMessage.h"
#include "SnepMessage.h"
#include "SnepServer.h"
#include "SnepClient.h"
#include "HandoverServer.h"
#include "HandoverClient.h"

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include "utils/Log.h"

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
  // TODO : We should send the ndef message to gecko
  // onReceiveComplete(msg);
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

P2pLinkManager::P2pLinkManager()
{
  mSnepCallback = new SnepCallback();
  mSnepServer = new SnepServer(static_cast<ISnepCallback*>(mSnepCallback));

  mHandoverCallback = new HandoverCallback();
  mHandoverServer = new HandoverServer(static_cast<IHandoverCallback*>(mHandoverCallback));

}

P2pLinkManager::~P2pLinkManager()
{
  delete mSnepCallback;
  delete mSnepServer;
  delete mHandoverCallback;
  delete mHandoverServer;
}

void P2pLinkManager::enableDisable(bool bEnable)
{
  if (bEnable) {
    mSnepServer->start();
    mHandoverServer->start();   
  } else {
    mSnepServer->stop();
    mHandoverServer->stop();
  }
}

void P2pLinkManager::push(NdefMessage* ndef)
{
  SnepClient snep;
  snep.connect();
  snep.put(*ndef);
  snep.close();
}
