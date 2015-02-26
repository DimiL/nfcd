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

#ifndef mozilla_nfcd_LlcpServiceSocket_h
#define mozilla_nfcd_LlcpServiceSocket_h

#include "ILlcpServerSocket.h"

class ILlcpSocket;

/**
 * LlcpServiceSocket represents a LLCP Service to be used in a
 * Connection-oriented communication.
 */
class LlcpServiceSocket
  : public ILlcpServerSocket
{
public:
  LlcpServiceSocket(uint32_t aHandle,
                    int aLocalLinearBufferLength,
                    int aLocalMiu,
                    int aLocalRw);
  virtual ~LlcpServiceSocket();

  /**
   * Accept a connection request from a peer.
   *
   * @return ILlcpSocket interface.
   */
  ILlcpSocket* Accept();

  /**
   * Close a server socket.
   *
   * @return True if ok.
   */
  bool Close();

private:
  uint32_t mHandle;
  int mLocalLinearBufferLength;
  int mSap;
  int mLocalMiu;
  int mLocalRw;
};

#endif  // mozilla_nfcd_LlcpServiceSocket_h
