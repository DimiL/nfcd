/* This Source Code Form is subject to the terms of the Mozilla 
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_SnepMessenger_h
#define mozilla_nfcd_SnepMessenger_h

#include "SnepMessage.h"
#include "DeviceHost.h"

class SnepMessenger{
public:
  SnepMessenger(bool isClient, ILlcpSocket* socket, int fragmentLength);
  ~SnepMessenger();

  ILlcpSocket* mSocket;
  int mFragmentLength;
  bool mIsClient;

  void sendMessage(SnepMessage& msg);
  SnepMessage* getMessage();
  void close();

  static SnepMessage* getPutRequest(NdefMessage& ndef);

private: 
  static const int HEADER_LENGTH = 6;
};

#endif

