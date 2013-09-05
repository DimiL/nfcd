/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_IP2pDevice_h
#define mozilla_nfcd_IP2pDevice_h

class IP2pDevice {
public:
  virtual ~IP2pDevice() {};

  virtual bool doConnect() = 0;
  virtual bool doDisconnect() = 0;
  virtual void doTransceive() = 0;
  virtual void doReceive() = 0;
  virtual bool doSend() = 0;

  virtual int& getMode() =0;
  virtual int& getHandle() = 0;
};

#endif
