/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_DeviceHost_h
#define mozilla_nfcd_DeviceHost_h

#include <stdio.h>

class DeviceHost{
public:
  DeviceHost();
  virtual ~DeviceHost();

  void notifyTagDiscovered(void* pTag);
  void notifyTargetDeselected();
  void notifyTransactionListeners();
  void notifyLlcpLinkActivation(void* pDevice);
  void notifyLlcpLinkDeactivated(void* pDevice);
  void notifyLlcpLinkFirstPacketReceived();
  void notifySeFieldActivated();
  void notifySeFieldDeactivated();
};

class NfcDepEndpoint {
public:
  //Peer-to-Peer Target
  static const uint8_t MODE_P2P_TARGET = 0x00;

  //Peer-to-Peer Initiator
  static const uint8_t MODE_P2P_INITIATOR = 0x01;

  //Invalid target mode
  static const uint8_t MODE_INVALID = 0xff;

private:
  virtual ~NfcDepEndpoint();
};
#endif
