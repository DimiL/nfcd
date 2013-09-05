/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcIpcSocket_h
#define mozilla_nfcd_NfcIpcSocket_h

#include <pthread.h>
#include <time.h>
#include <binder/Parcel.h>

class MessageHandler;

class NfcIpcSocket{
private:
  static NfcIpcSocket* sInstance;

public:
  ~NfcIpcSocket();

  static NfcIpcSocket* Instance();
  void initialize(MessageHandler* msgHandler);
  void loop();

  void writeToOutgoingQueue(uint8_t *data, size_t dataLen);
  void writeToIncomingQueue(uint8_t *data, size_t dataLen);

private:
  NfcIpcSocket();

  timespec mSleep_spec;
  timespec mSleep_spec_rem;

  static MessageHandler* sMsgHandler;

  void initSocket();
  int getListenSocket();
  void onConnect();
};

#endif // mozilla_nfcd_NfcIpcSocket_h
