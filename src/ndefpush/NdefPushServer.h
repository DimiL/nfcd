/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NdefPushServer_h
#define mozilla_nfcd_NdefPushServer_h

#include "INdefPushCallback.h"

class NdefPushCallback : public INdefPushCallback
{
public:
   NdefPushCallback();
   virtual ~NdefPushCallback();
};

class NdefPushServer{
public:
  NdefPushServer(INdefPushCallback* callback);
  ~NdefPushServer();

  static const int DEFAULT_MIU = 248;
  static const char* DEFAULT_SERVICE_NAME;
  static const int NDEFPUSH_SAP = 0x10;

  void start();
  void stop();
  
  ILlcpServerSocket* mServerSocket;
  int                mServiceSap;
  INdefPushCallback* mCallback;
  bool               mServerRunning;
};

class PushConnectionThread {
public:
  PushConnectionThread(NdefPushServer* server, ILlcpSocket* socket, INdefPushCallback* callback);
  ~PushConnectionThread();

  void run();
  bool isServerRunning();

  ILlcpSocket* mSock;
  INdefPushCallback* mCallback;
  NdefPushServer* mServer;
};
#endif

