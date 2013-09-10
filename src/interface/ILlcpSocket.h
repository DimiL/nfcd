/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_ILlcpSocket_h
#define mozilla_nfcd_ILlcpSocket_h

#include <vector>

class ILlcpSocket {
public:
  virtual ~ILlcpSocket() {};

  virtual void connectToSap(int sap) = 0;
  virtual void connectToService(const char* serviceName) = 0;
  virtual void close() = 0;
  virtual void send(std::vector<uint8_t>& data) = 0;
  virtual int receive(std::vector<uint8_t>& recvBuff) = 0;
  virtual int getRemoteMiu() = 0;
  virtual int getRemoteRw() = 0;

  virtual int getLocalSap() = 0;
  virtual int getLocalMiu() = 0;
  virtual int getLocalRw() = 0;
};

#endif
