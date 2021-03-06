/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
