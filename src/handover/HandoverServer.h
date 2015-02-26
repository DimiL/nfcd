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

#ifndef mozilla_nfcd_HandoverPushServer_h
#define mozilla_nfcd_HandoverPushServer_h

class IHandoverCallback;
class NdefMessage;
class HandoverConnectionThread;

class HandoverServer{
public:
  HandoverServer(IHandoverCallback* aCallback);
  ~HandoverServer();

  static const int DEFAULT_MIU = 128;
  static const char* DEFAULT_SERVICE_NAME;
  static const int HANDOVER_SAP = 0x14;

  void Start();
  void Stop();
  bool Put(NdefMessage& msg);
  void SetConnectionThread(HandoverConnectionThread* aThread);

  ILlcpServerSocket* mServerSocket;
  int                mServiceSap;
  IHandoverCallback* mCallback;
  bool               mServerRunning;

private:
  HandoverConnectionThread* mConnectionThread;
};

class HandoverConnectionThread {
public:
  HandoverConnectionThread(HandoverServer* aServer,
                           ILlcpSocket* aSocket,
                           IHandoverCallback* aCallback);
  ~HandoverConnectionThread();

  void Run();
  bool IsServerRunning() const;

  ILlcpSocket* GetSocket() { return mSock; }
  IHandoverCallback* GetCallback() { return mCallback; }
  HandoverServer* GetServer() { return mServer; }

private:
  ILlcpSocket* mSock;
  IHandoverCallback* mCallback;
  HandoverServer* mServer;
};

#endif

