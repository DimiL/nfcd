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

#ifndef mozilla_nfcd_P2pLinkManager_h
#define mozilla_nfcd_P2pLinkManager_h

#include "ISnepCallback.h"
#include "IHandoverCallback.h"

class NfcService;
class NdefMessage;
class SnepServer;
class SnepClient;
class HandoverServer;
class HandoverClient;

class SnepCallback
  : public ISnepCallback
{
public:
   SnepCallback();
   virtual ~SnepCallback();

   virtual SnepMessage* doPut(NdefMessage* msg);
   virtual SnepMessage* doGet(int acceptableLength, NdefMessage* msg);
};

class HandoverCallback
  : public IHandoverCallback
{
public:
   HandoverCallback();
   virtual ~HandoverCallback();

   virtual void onMessageReceived(NdefMessage* msg);
};

class P2pLinkManager{
public:
  P2pLinkManager(NfcService* service);
  ~P2pLinkManager();

  void notifyNdefReceived(NdefMessage* ndef);
  void enableDisable(bool bEnable);
  void push(NdefMessage& ndef);
  void onLlcpActivated();
  void onLlcpDeactivated();
  bool isLlcpActive();

  void setSessionId(int sessionId) { mSessionId = sessionId; }
  int getSessionId() { return mSessionId; }

private:
  static const int LINK_STATE_DOWN = 1;
  static const int LINK_STATE_UP = 2;

  SnepClient* getSnepClient();
  HandoverClient* getHandoverClient();
  void disconnectClients();

  int mLinkState;
  int mSessionId;

  NfcService* mNfcService;

  SnepCallback* mSnepCallback;
  SnepServer* mSnepServer;
  SnepClient* mSnepClient;

  HandoverCallback* mHandoverCallback;
  HandoverServer* mHandoverServer;
  HandoverClient* mHandoverClient;
};

#endif
