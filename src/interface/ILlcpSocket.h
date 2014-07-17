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
   * @param nSap Establish a connection to the peer.
   * @return      True if ok.
   */
  virtual bool connectToSap(int sap) = 0;

  /**
   * Establish a connection to the peer.
   *
   * @param sn Service name.
   * @return    True if ok.
   */
  virtual bool connectToService(const char* serviceName) = 0;

  /**
   * Close socket.
   *
   * @return Close socket.
   */
  virtual void close() = 0;

  /**
   * Send data to peer.
   *
   * @param sendBuff Buffer of data.
   * @return         True if sent ok.
   */
  virtual bool send(std::vector<uint8_t>& sendBuff) = 0;

  /**
   * Receive data from peer.
   *
   * @param recvBuff  Buffer to put received data.
   * @return          Number of bytes received.
   */
  virtual int receive(std::vector<uint8_t>& recvBuff) = 0;

  /**
   * Get peer's maximum information unit.
   *
   * @return Peer's maximum information unit.
   */
  virtual int getRemoteMiu() const = 0;

  /**
   * Get peer's receive window size.
   *
   * @return Peer's receive window size.
   */
  virtual int getRemoteRw() const = 0;

  /**
   * Get local service access point.
   *
   * @return Local service access point.
   */
  virtual int getLocalSap() const = 0;

  /**
   * Get local maximum information unit.
   *
   * @return Local receive window size.
   */
  virtual int getLocalMiu() const = 0;

  /**
   * Get local receive window size.
   *
   * @return Local receive window size.
   */
  virtual int getLocalRw() const = 0;
};

#endif
