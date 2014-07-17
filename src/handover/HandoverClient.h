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

#ifndef mozilla_nfcd_HandoverClient_h
#define mozilla_nfcd_HandoverClient_h

class ILlcpSocket;
class NdefMessage;

class HandoverClient{
public:
  HandoverClient();
  ~HandoverClient();

  bool connect();
  NdefMessage* receive();
  bool put(NdefMessage& msg);
  void close();

  NdefMessage* processHandoverRequest(NdefMessage& msg);

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
