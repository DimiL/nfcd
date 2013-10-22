/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_HandoverClient_h
#define mozilla_nfcd_HandoverClient_h

class ILlcpSocket;
class NdefMessage;

class HandoverClient{
public:
  HandoverClient();
  ~HandoverClient();

  bool connect();
  void put(NdefMessage& msg);
  void close();

private:
  static const int DEFAULT_MIU = 128;

  static const int DISCONNECTED = 0;
  static const int CONNECTING = 1;
  static const int CONNECTED = 2;

  ILlcpSocket* mSocket;

  const char* mServiceName;
  int mState;
  int mMiu;
};

#endif
