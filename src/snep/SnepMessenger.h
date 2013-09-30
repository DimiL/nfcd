/* This Source Code Form is subject to the terms of the Mozilla 
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_SnepMessenger_h
#define mozilla_nfcd_SnepMessenger_h

#include "SnepMessage.h"

class ILlcpSocket;

class SnepMessenger{
public:
  SnepMessenger(bool isClient, ILlcpSocket* socket, uint32_t fragmentLength);
  ~SnepMessenger();

  ILlcpSocket* mSocket;
  uint32_t mFragmentLength;
  bool mIsClient;

  void sendMessage(SnepMessage& msg);
  SnepMessage* getMessage();
  void close();

  static SnepMessage* getPutRequest(NdefMessage& ndef);

private: 
  static const int HEADER_LENGTH = 6;

  bool socketSend(uint8_t field);
};

#endif
