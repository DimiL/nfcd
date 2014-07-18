/*
 * Copyright (C) 2014  Mozilla Foundation
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

#ifndef mozilla_nfcd_LlcpSocket_h
#define mozilla_nfcd_LlcpSocket_h

#include <vector>
#include "ILlcpSocket.h"

/**
 * LlcpClientSocket represents a LLCP Connection-Oriented client to be used in a
 * connection-oriented communication.
 */
class LlcpSocket
  : public ILlcpSocket
{
public:
  LlcpSocket(unsigned int handle, int sap, int miu, int rw);
  LlcpSocket(unsigned int handle, int miu, int rw);
  virtual ~LlcpSocket();

  /**
   * Establish a connection to the peer.
   *
   * @param nSap Establish a connection to the peer.
   * @return      True if ok.
   */
  bool connectToSap(int nSap);

  /**
   * Establish a connection to the peer.
   *
   * @param sn Service name.
   * return    True if ok.
   */
  bool connectToService(const char* serviceName);

  /**
   * Close socket.
   *
   * @return True if ok.
   */
  void close();

  /**
   * Send data to peer.
   *
   * @param sendBuff Buffer of data.
   * @return         True if sent ok.
   */
  bool send(std::vector<uint8_t>& sendBuff);

  /**
   * Receive data from peer.
   *
   * @param recvBuff Buffer to put received data.
   * @return         Buffer to put received data.
   */
  int receive(std::vector<uint8_t>& recvBuff);

  /**
   * Get peer's maximum information unit.
   *
   * @return Peer's maximum information unit.
   */
  int getRemoteMiu() const;

  /**
   * Peer's maximum information unit.
   *
   * @return Peer's receive window size.
   */
  int getRemoteRw() const;

  int getLocalSap() const { return mSap; }
  int getLocalMiu() const { return mLocalMiu; }
  int getLocalRw() const { return mLocalRw; }

private:
  uint32_t mHandle;
  int mSap;
  int mLocalMiu;
  int mLocalRw;

  bool doConnect(int nSap);
  bool doConnectBy(const char* sn);
  bool doClose();

  bool doSend(std::vector<uint8_t>& data);
  int doReceive(std::vector<uint8_t>& recvBuff);

  int doGetRemoteSocketMIU() const;
  int doGetRemoteSocketRW() const;
};

#endif // mozilla_nfcd_LlcpSocket_h
