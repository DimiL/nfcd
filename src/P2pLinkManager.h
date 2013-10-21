/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_P2pLinkManager_h
#define mozilla_nfcd_P2pLinkManager_h

#include "ISnepCallback.h"
#include "IHandoverCallback.h"

class NdefMessage;
class SnepServer;
class HandoverServer;

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
};

class P2pLinkManager{
public:
  P2pLinkManager();
  ~P2pLinkManager();

  void enableDisable(bool bEnable);
  void push(NdefMessage* ndef);

private:
  SnepCallback* mSnepCallback;
  HandoverCallback* mHandoverCallback;
  SnepServer* mSnepServer;
  HandoverServer* mHandoverServer;
};

#endif
