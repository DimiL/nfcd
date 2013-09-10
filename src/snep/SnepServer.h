/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_SnepServer_h
#define mozilla_nfcd_SnepServer_h

#include "SnepMessenger.h"
#include "ISnepCallback.h"


class ConnectionThread {
public:
  ConnectionThread(ILlcpSocket* socket, int fragmentLength);
  ~ConnectionThread();

  void run();

  ILlcpSocket* mSock;
  SnepMessenger* mMessenger;
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
  
  static bool handleRequest(SnepMessenger* messenger, ISnepCallback* callback);
};

#endif

