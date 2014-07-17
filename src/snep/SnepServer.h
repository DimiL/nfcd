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

#ifndef mozilla_nfcd_SnepServer_h
#define mozilla_nfcd_SnepServer_h

#include "SnepMessenger.h"

class ILlcpServerSocket;
class ISnepCallback;

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

class SnepConnectionThread {
public:
  SnepConnectionThread(SnepServer* server, ILlcpSocket* socket, int fragmentLength, ISnepCallback* callback);
  ~SnepConnectionThread();

  void run();
  bool isServerRunning() const;

  ILlcpSocket* mSock;
  SnepMessenger* mMessenger;
  ISnepCallback* mCallback;
  SnepServer* mServer;
};

#endif
