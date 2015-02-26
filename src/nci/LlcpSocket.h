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
  LlcpSocket(unsigned int aHandle, int aSap, int aMiu, int aRw);
  LlcpSocket(unsigned int aHandle, int aMiu, int aRw);
  virtual ~LlcpSocket();

  /**
   * Establish a connection to the peer.
   *
   * @param  aSap Establish a connection to the peer.
   * @return      True if ok.
   */
  bool ConnectToSap(int aSap);

  /**
   * Establish a connection to the peer.
   *
   * @param  aSn Service name.
   * @return     True if ok.
   */
  bool ConnectToService(const char* aSn);

  /**
   * Close socket.
   *
   * @return True if ok.
   */
  void Close();

  /**
   * Send data to peer.
   *
   * @param  aSendBuf Buffer of data.
   * @return          True if sent ok.
   */
  bool Send(std::vector<uint8_t>& aSendBuf);

  /**
   * Receive data from peer.
   *
   * @param  aRecvBuf Buffer to put received data.
   * @return          Buffer to put received data.
   */
  int Receive(std::vector<uint8_t>& aRecvBuf);

  /**
   * Get peer's maximum information unit.
   *
   * @return Peer's maximum information unit.
   */
  int GetRemoteMiu() const;

  /**
   * Peer's maximum information unit.
   *
   * @return Peer's receive window size.
   */
  int GetRemoteRw() const;

  int GetLocalSap() const { return mSap; }
  int GetLocalMiu() const { return mLocalMiu; }
  int GetLocalRw() const { return mLocalRw; }

private:
  uint32_t mHandle;
  int mSap;
  int mLocalMiu;
  int mLocalRw;

  bool DoConnect(int aSap);
  bool DoConnectBy(const char* aSn);
  bool DoClose();

  bool DoSend(std::vector<uint8_t>& aData);
  int DoReceive(std::vector<uint8_t>& aRecvBuf);

  int DoGetRemoteSocketMIU() const;
  int DoGetRemoteSocketRW() const;
};

#endif // mozilla_nfcd_LlcpSocket_h
