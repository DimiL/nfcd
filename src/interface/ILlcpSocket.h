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

#ifndef mozilla_nfcd_ILlcpSocket_h
#define mozilla_nfcd_ILlcpSocket_h

#include <vector>

class ILlcpSocket {
public:
  virtual ~ILlcpSocket() {};

  /**
   * Establish a connection to the peer.
   *
   * @param  aSap Establish a connection to the peer.
   * @return      True if ok.
   */
  virtual bool ConnectToSap(int aSap) = 0;

  /**
   * Establish a connection to the peer.
   *
   * @param  aSN Service name.
   * @return     True if ok.
   */
  virtual bool ConnectToService(const char* aSN) = 0;

  /**
   * Close socket.
   *
   * @return Close socket.
   */
  virtual void Close() = 0;

  /**
   * Send data to peer.
   *
   * @param  aSendBuf Buffer of data.
   * @return          True if sent ok.
   */
  virtual bool Send(std::vector<uint8_t>& aSendBuf) = 0;

  /**
   * Receive data from peer.
   *
   * @param  aRecvBuf Buffer to put received data.
   * @return          Number of bytes received.
   */
  virtual int Receive(std::vector<uint8_t>& aRecvBuf) = 0;

  /**
   * Get peer's maximum information unit.
   *
   * @return Peer's maximum information unit.
   */
  virtual int GetRemoteMiu() const = 0;

  /**
   * Get peer's receive window size.
   *
   * @return Peer's receive window size.
   */
  virtual int GetRemoteRw() const = 0;

  /**
   * Get local service access point.
   *
   * @return Local service access point.
   */
  virtual int GetLocalSap() const = 0;

  /**
   * Get local maximum information unit.
   *
   * @return Local receive window size.
   */
  virtual int GetLocalMiu() const = 0;

  /**
   * Get local receive window size.
   *
   * @return Local receive window size.
   */
  virtual int GetLocalRw() const = 0;
};

#endif
