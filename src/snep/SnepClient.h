/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
