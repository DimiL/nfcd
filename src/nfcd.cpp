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
  service->Initialize(pNfcManager, msgHandler);

  // Create IPC socket & main thread will enter while loop to read data from socket.
  NfcIpcSocket* socket = NfcIpcSocket::Instance();
  socket->Initialize(msgHandler);
  socket->SetSocketListener(service);
  msgHandler->SetOutgoingSocket(socket);
  socket->Loop();

  //TODO delete NfcIpcSocket, NfcService
  delete msgHandler;
  delete pNfcManager;
  //exit(0);
}
