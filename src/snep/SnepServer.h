/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_SnepServer_h
#define mozilla_nfcd_SnepServer_h

#include "SnepMessenger.h"
#include "ISnepCallback.h"

class SnepCallback
  : public ISnepCallback
{
public:
   SnepCallback();
   virtual ~SnepCallback();

   virtual SnepMessage* doPut(NdefMessage* msg);
   virtual SnepMessage* doGet(int acceptableLength, NdefMessage* msg);
};

class SnepServer{
public:
  SnepServer(ISnepCallback* callback);
  SnepServer(const char* serviceName, int serviceSap, ISnepCallback* callback);
  SnepServer(ISnepCallback* callback, int miu, int rwSize);
  SnepServer(const char* serviceName, int serviceSap, int fragmentLength, ISnepCallback* callback);
  ~SnepServer();

  static const int DEFAULT_MIU = 248;
  static const int DEFAULT_RW_SIZE = 1;
  static const int DEFAULT_PORT = 4;
  static const char* DEFAULT_SERVICE_NAME;

  void start();
  void stop();
  
  static bool handleRequest(SnepMessenger* messenger, ISnepCallback* callback);

  ILlcpServerSocket* mServerSocket;
  ISnepCallback*     mCallback;
  bool               mServerRunning;
  const char*        mServiceName;
  int                mServiceSap;
  int                mFragmentLength;
  int                mMiu;
  int                mRwSize;
};

class ConnectionThread {
public:
  ConnectionThread(SnepServer* server, ILlcpSocket* socket, int fragmentLength, ISnepCallback* callback);
  ~ConnectionThread();

  void run();
  bool isServerRunning();

  ILlcpSocket* mSock;
  SnepMessenger* mMessenger;
  ISnepCallback* mCallback;
  SnepServer* mServer;
};

#endif
