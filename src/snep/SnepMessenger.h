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

#ifndef mozilla_nfcd_SnepMessenger_h
#define mozilla_nfcd_SnepMessenger_h

#include "SnepMessage.h"

class ILlcpSocket;

class SnepMessenger{
public:
  SnepMessenger(bool aIsClient,
                ILlcpSocket* aSocket,
                uint32_t aFragmentLength);
  ~SnepMessenger();

  ILlcpSocket* mSocket;
  uint32_t mFragmentLength;
  bool mIsClient;

  void SendMessage(SnepMessage& aMsg);
  SnepMessage* GetMessage();
  void Close();
  static SnepMessage* GetPutRequest(NdefMessage& aNdef);

private:
  static const int HEADER_LENGTH = 6;

  bool SocketSend(uint8_t aField);
};

#endif
