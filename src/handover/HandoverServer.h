/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_HandoverPushServer_h
#define mozilla_nfcd_HandoverPushServer_h

class IHandoverCallback;
class NdefMessage;
class HandoverConnectionThread;

class HandoverServer{
public:
  HandoverServer(IHandoverCallback* callback);
  ~HandoverServer();

  static const int DEFAULT_MIU = 128;
  static const char* DEFAULT_SERVICE_NAME;
  static const int HANDOVER_SAP = 0x14;

  void start();
  void stop();
  bool put(NdefMessage& msg);
  void setConnectionThread(HandoverConnectionThread* pThread);

  ILlcpServerSocket* mServerSocket;
  int                mServiceSap;
  IHandoverCallback* mCallback;
  bool               mServerRunning;

private:
  HandoverConnectionThread* mConnectionThread;
};

class HandoverConnectionThread {
public:
  HandoverConnectionThread(HandoverServer* server, ILlcpSocket* socket, IHandoverCallback* callback);
  ~HandoverConnectionThread();

  void run();
  bool isServerRunning() const;

  ILlcpSocket* getSocket() {  return mSock;  }
  IHandoverCallback* getCallback() {  return mCallback;  }
  HandoverServer* getServer() {  return mServer;  }

private:
  ILlcpSocket* mSock;
  IHandoverCallback* mCallback;
  HandoverServer* mServer;
};

#endif

