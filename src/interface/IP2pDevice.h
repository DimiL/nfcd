/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_IP2pDevice_h
#define mozilla_nfcd_IP2pDevice_h

#define INTERFACE_P2P_DEVICE  "P2pDevice"

class IP2pDevice {
public:
  virtual ~IP2pDevice() {};

  virtual bool connect() = 0;
  virtual bool disconnect() = 0;
  virtual void transceive() = 0;
  virtual void receive() = 0;
  virtual bool send() = 0;

  virtual int& getMode() =0;
  virtual int& getHandle() = 0;
};

#endif
