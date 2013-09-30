/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
   * return      True if ok.
   */
  virtual bool connectToSap(int sap) = 0;

  /**
   * Establish a connection to the peer.
   *
   * @param sn Service name.
   * return    True if ok.
   */
  virtual bool connectToService(const char* serviceName) = 0;

  /**
   * Close socket.
   *
   * return Close socket.
   */
  virtual void close() = 0;

  /**
   * Send data to peer.
   *
   * @param data Buffer of data.
   * return      True if sent ok.
   */
  virtual bool send(std::vector<uint8_t>& data) = 0;

  /**
   * Receive data from peer.
   *
   * @param recvBuff Buffer to put received data.
   * return          Number of bytes received.
   */
  virtual int receive(std::vector<uint8_t>& recvBuff) = 0;

  /**
   * Get peer's maximum information unit.
   *
   * return Peer's maximum information unit.
   */

  virtual int getRemoteMiu() = 0;

  /**
   * Peer's maximum information unit.
   *
   * return Peer's receive window size.
   */
  virtual int getRemoteRw() = 0;

  virtual int getLocalSap() = 0;
  virtual int getLocalMiu() = 0;
  virtual int getLocalRw() = 0;
};

#endif
