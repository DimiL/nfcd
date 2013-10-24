/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  void push(NdefMessage* ndef);
  void onLlcpActivated();
  void onLlcpDeactivated();
  bool isLlcpActive();

private:
  static const int LINK_STATE_DOWN = 1;
  static const int LINK_STATE_UP = 2;

  void connectClients();
  void disconnectClients();

  int mLinkState;

  NfcService* mNfcService;

  SnepCallback* mSnepCallback;
  SnepServer* mSnepServer;
  SnepClient* mSnepClient;

  HandoverCallback* mHandoverCallback;
  HandoverServer* mHandoverServer;
  HandoverClient* mHandoverClient;
};

#endif
