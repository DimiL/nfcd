/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_DeviceHost_h
#define mozilla_nfcd_DeviceHost_h

#include <stdio.h>

class INfcTag;
class IP2pDevice;

class DeviceHost {
public:
  DeviceHost() {};
  virtual ~DeviceHost() {};

  /**
   * Notifies tag detected.
   *
   * @param pTag INfcTag interface.
   * @return     None.
   */
  void notifyTagDiscovered(INfcTag* pTag);

  /**
   * Notifies P2P Device detected, to activate LLCP link.
   *
   * @param pDevice IP2pDevice interface.
   * @return        None.
   */
  void notifyLlcpLinkActivation(IP2pDevice* pDevice);

  /**
   * Notifies P2P Device is out of range, to deactivate LLCP link.
   *
   * @param pDevice IP2pDevice interface.
   * @return        None.
   */
  void notifyLlcpLinkDeactivated(IP2pDevice* pDevice);

  // Interfaces are not yet used.
  void notifyTargetDeselected();
  void notifyTransactionListeners();
  void notifyLlcpLinkFirstPacketReceived();
  void notifySeFieldActivated();
  void notifySeFieldDeactivated();
};

class NfcDepEndpoint {
public:
  //Peer-to-Peer Target.
  static const uint8_t MODE_P2P_TARGET = 0x00;

  //Peer-to-Peer Initiator.
  static const uint8_t MODE_P2P_INITIATOR = 0x01;

  //Invalid target mode.
  static const uint8_t MODE_INVALID = 0xff;

private:
  virtual ~NfcDepEndpoint();
};
#endif
