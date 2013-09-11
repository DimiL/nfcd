/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_INfcManager_h
#define mozilla_nfcd_INfcManager_h

#include "ILlcpServerSocket.h"

class INfcManager {
public:
  virtual ~INfcManager() {};

  virtual void* queryInterface(const char* name) = 0;

  virtual bool doInitialize() = 0;
  virtual bool doDeinitialize() = 0;

  virtual void enableDiscovery() = 0;
  virtual void disableDiscovery() = 0;

  virtual bool doCheckLlcp() = 0;
  virtual bool doActivateLlcp() = 0;

  virtual ILlcpSocket* createLlcpSocket(int sap, int miu, int rw, int linearBufferLength) = 0;
  virtual ILlcpServerSocket* createLlcpServerSocket(int nSap, const char* sn, int miu, int rw, int linearBufferLength) = 0;

  virtual void setP2pInitiatorModes(int modes) = 0;
  virtual void setP2pTargetModes(int modes) = 0;

  virtual int getDefaultLlcpMiu() = 0;
  virtual int getDefaultLlcpRwSize() = 0;
};

#endif
