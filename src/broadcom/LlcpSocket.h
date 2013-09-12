/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_LlcpSocket_h
#define mozilla_nfcd_LlcpSocket_h

#include "ILlcpSocket.h"
#include <vector>

class LlcpSocket : public ILlcpSocket
{
public:
  LlcpSocket(unsigned int handle, int sap, int miu, int rw);
  LlcpSocket(unsigned int handle, int miu, int rw);
  virtual ~LlcpSocket();

  bool connectToSap(int sap);
  bool connectToService(const char* serviceName);
  void close();
  void send(std::vector<uint8_t>& data);
  int receive(std::vector<uint8_t>& recvBuff);
  int getRemoteMiu();
  int getRemoteRw();

  int getLocalSap() {  return mSap;  }
  int getLocalMiu(){  return mLocalMiu;  }
  int getLocalRw(){  return mLocalRw;  }

private:
  uint32_t mHandle;
  int mSap;
  int mLocalMiu;
  int mLocalRw;

  bool LlcpSocket_doConnect (int nSap);
  bool LlcpSocket_doConnectBy (const char* sn);
  bool LlcpSocket_doSend (std::vector<uint8_t>& data);
  int LlcpSocket_doReceive(std::vector<uint8_t>& recvBuff);

  bool LlcpSocket_doClose();
  int LlcpSocket_doGetRemoteSocketMIU ();
  int LlcpSocket_doGetRemoteSocketRW ();
};

#endif // mozilla_nfcd_LlcpSocket_h
