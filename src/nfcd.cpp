/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nfcd.h"
#include "NfcManager.h"

#include "NfcService.h"
#include "NfcIpcSocket.h"
#include "DeviceHost.h"
#include "MessageHandler.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

int main() {

  // 1. Create NFC Manager and do initialize
  NfcManager* pNfcManager = new NfcManager();
  pNfcManager->initialize();

  MessageHandler::initialize();

  // 2. Enable Discovery
  pNfcManager->enableDiscovery();

  // 3. Create service thread to receive message from nfc library
  NfcService* pNfcService = NfcService::Instance();
  pNfcService->initialize(pNfcManager);

  // 4. Create IPC socket & main thread will enter while loop to read data from socket
  NfcIpcSocket* pNfcIpcSocket = NfcIpcSocket::Instance();
  pNfcIpcSocket->initialize();
  pNfcIpcSocket->loop();

  delete pNfcManager;
  //exit(0);
}
