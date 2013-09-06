/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_SnepClient_h
#define mozilla_nfcd_SnepClient_h

#include "NdefMessage.h"
#include "SnepMessage.h"

class SnepClient{
public:
  SnepClient();
  ~SnepClient();

  void put(NdefMessage& msg);
  SnepMessage* get(NdefMessage& msg);
  void connect();
  void close();

private: 

};

#endif

