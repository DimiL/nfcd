/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_LlcpServiceSocket_h
#define mozilla_nfcd_LlcpServiceSocket_h

#include "ILlcpServerSocket.h"
#include "ILlcpSocket.h"

class LlcpServiceSocket : public ILlcpServerSocket
{
public:
  LlcpServiceSocket(uint32_t handle, int localLinearBufferLength, int localMiu, int localRw);
  virtual ~LlcpServiceSocket();

  ILlcpSocket* accept();
  bool close();

private:
  uint32_t mHandle;
  int mLocalLinearBufferLength;
  int mSap;
  int mLocalMiu;
  int mLocalRw;

};

#endif  // mozilla_nfcd_LlcpServiceSocket_h
