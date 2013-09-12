/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_ILlcpServerSocket_h
#define mozilla_nfcd_ILlcpServerSocket_h

#include "ILlcpSocket.h"

class ILlcpServerSocket {
public:
  virtual ~ILlcpServerSocket() {};

  virtual ILlcpSocket* accept() = 0;
  virtual bool close() = 0;
};

#endif
