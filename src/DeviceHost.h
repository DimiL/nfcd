/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_DeviceHost_h
#define mozilla_nfcd_DeviceHost_h

class DeviceHost{

public:

  DeviceHost();
  virtual ~DeviceHost();

  void notifyNdefMessageListeners(void* pTag);

  void notifyTargetDeselected();

  void notifyTransactionListeners();

  void notifyLlcpLinkActivation(void* pDevice);

  void notifyLlcpLinkDeactivated(void* pDevice);

  void notifyLlcpLinkFirstPacketReceived();

  void notifySeFieldActivated();

  void notifySeFieldDeactivated();
};

#endif
