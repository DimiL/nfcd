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

#ifndef mozilla_nfcd_SnepClient_h
#define mozilla_nfcd_SnepClient_h

class NdefMessage;
class SnepMessage;
class SnepMessenger;

class SnepClient{
public:
  SnepClient();
  SnepClient(const char* serviceName);
  SnepClient(int miu, int rwSize);
  SnepClient(const char* serviceName, int fragmentLength);
  SnepClient(const char* serviceName, int acceptableLength, int fragmentLength);
  ~SnepClient();

  void put(NdefMessage& msg);
  SnepMessage* get(NdefMessage& msg);
  bool connect();
  void close();

private:
  static const int DEFAULT_ACCEPTABLE_LENGTH = 100*1024;
  static const int DEFAULT_MIU = 128;
  static const int DEFAULT_RWSIZE = 1;

  static const int DISCONNECTED = 0;
  static const int CONNECTING = 1;
  static const int CONNECTED = 2;

  SnepMessenger* mMessenger;
  const char* mServiceName;
  int mPort;
  int mState;
  int mAcceptableLength;
  int mFragmentLength;
  int mMiu;
  int mRwSize;
};

#endif
