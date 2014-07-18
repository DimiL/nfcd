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

#ifndef mozilla_nfcd_NfcIpcSocket_h
#define mozilla_nfcd_NfcIpcSocket_h

#include <pthread.h>
#include <time.h>
#include <binder/Parcel.h>

class MessageHandler;
class IpcSocketListener;

class NfcIpcSocket{
private:
  static NfcIpcSocket* sInstance;

public:
  ~NfcIpcSocket();

  static NfcIpcSocket* Instance();
  void initialize(MessageHandler* msgHandler);
  void loop();

  void setSocketListener(IpcSocketListener* lister);

  void writeToOutgoingQueue(uint8_t *data, size_t dataLen);
  void writeToIncomingQueue(uint8_t *data, size_t dataLen);

private:
  NfcIpcSocket();

  timespec mSleep_spec;
  timespec mSleep_spec_rem;

  static MessageHandler* sMsgHandler;

  IpcSocketListener* mListener;

  void initSocket();
  int getListenSocket();
};

#endif // mozilla_nfcd_NfcIpcSocket_h
