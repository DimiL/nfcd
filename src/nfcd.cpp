/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nfcd.h"

#include "NfcManager.h"
#include "NfcService.h"
#include "NfcIpcSocket.h"
#include "DeviceHost.h"
#include "MessageHandler.h"
#include "SnepServer.h"
#include "NdefPushServer.h"
#include "HandoverServer.h"

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include <cutils/log.h>

int main() {

  // 1. Create NFC Manager and do initialize
  NfcManager* pNfcManager = new NfcManager();
  pNfcManager->doInitialize();

  MessageHandler* msgHandler = new MessageHandler();

  // 2. Create service thread to receive message from nfc library
  NfcService* pNfcService = NfcService::Instance();
  pNfcService->initialize(pNfcManager, msgHandler);

  // 3. Create snep server
  // TODO : Maybe we should put this when p2p connection is established ?
  // Mark first because the function is not yet complete
  SnepCallback snepCallback;
  SnepServer snepServer(static_cast<ISnepCallback*>(&snepCallback));
  snepServer.start();

  // 4. Enable discovery MUST after push,snep,handover servers are established
  pNfcManager->enableDiscovery();

  // 5. Create IPC socket & main thread will enter while loop to read data from socket
  NfcIpcSocket* pNfcIpcSocket = NfcIpcSocket::Instance();
  pNfcIpcSocket->initialize(msgHandler);
  msgHandler->setSocket(pNfcIpcSocket);
  pNfcIpcSocket->loop();

  //TODO delete NfcIpcSocket, NfcService
  delete msgHandler;
  delete pNfcManager;
  //exit(0);
}
