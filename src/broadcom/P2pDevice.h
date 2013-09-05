/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_P2pDevice_h
#define mozilla_nfcd_P2pDevice_h

#include "IP2pDevice.h"

class P2pDevice : public IP2pDevice
{
public:
  P2pDevice();
  virtual ~P2pDevice();

  bool doConnect();
  bool doDisconnect();
  void doTransceive();
  void doReceive();
  bool doSend();

  int& getHandle();
  int& getMode();

private:

  int mHandle;
  int mMode;
  unsigned char* mGeneralBytes;
};

#endif // mozilla_nfcd_P2pDevice_h
