/*
 * Copyright (C) 2013-2014  Mozilla Foundation
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

#ifndef mozilla_nfcd_DeviceHost_h
#define mozilla_nfcd_DeviceHost_h

#include <stdio.h>

class INfcTag;
class IP2pDevice;
class TransactionEvent;

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
  void notifyLlcpLinkActivated(IP2pDevice* pDevice);

  /**
   * Notifies P2P Device is out of range, to deactivate LLCP link.
   *
   * @param pDevice IP2pDevice interface.
   * @return        None.
   */
  void notifyLlcpLinkDeactivated(IP2pDevice* pDevice);

  /**
   * Notifies HCI TRANSACTION event received.
   *
   * @param pEvent Contain transaction aid and payload
   * @return       None.
   */
  void notifyTransactionEvent(TransactionEvent* pEvent);

  // Interfaces are not yet used.
  void notifyTargetDeselected();
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

class TransactionEvent {
public:
  TransactionEvent();
  ~TransactionEvent();

  uint32_t aidLen;
  uint8_t* aid;

  uint32_t payloadLen;
  uint8_t* payload;
};
#endif
