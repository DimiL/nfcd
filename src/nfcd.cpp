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

int main() {

  // Create NFC Manager and do initialize.
  NfcManager* pNfcManager = new NfcManager();

  // Create service thread to receive message from nfc library.
  NfcService* service = NfcService::Instance();
  MessageHandler* msgHandler = new MessageHandler(service);
  service->initialize(pNfcManager, msgHandler);

  service->enableNfc();
  service->enableDiscovery();
  service->selectSE();

  // Create IPC socket & main thread will enter while loop to read data from socket.
  NfcIpcSocket* socket = NfcIpcSocket::Instance();
  socket->initialize(msgHandler);
  socket->setSocketListener(service);
  msgHandler->setOutgoingSocket(socket);
  socket->loop();

  //TODO delete NfcIpcSocket, NfcService
  delete msgHandler;
  delete pNfcManager;
  //exit(0);
}
