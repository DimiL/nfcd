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

#ifndef mozilla_nfcd_INfcManager_h
#define mozilla_nfcd_INfcManager_h

#include "ILlcpServerSocket.h"
#include "ILlcpSocket.h"

class INfcManager {
public:
  virtual ~INfcManager() {};

  /**
   * To get a specific interface from NfcManager.
   *
   * @param  name Interface name.
   * @return      Return specific interface if exist, null if cannot find.
   */
  virtual void* queryInterface(const char* name) = 0;

  /**
   * Turn on NFC.
   *
   * @return True if ok.
   */
  virtual bool initialize() = 0;

  /**
   * Turn off NFC.
   *
   * @return True if ok.
   */
  virtual bool deinitialize() = 0;

  /**
   * Start polling and listening for devices.
   *
   * @return True if ok.
   */
  virtual bool enableDiscovery() = 0;

  /**
   * Stop polling and listening for devices.
   *
   * @return True if ok.
   */
  virtual bool disableDiscovery() = 0;

  /**
   * Start polling for devices.
   *
   * @return True if ok.
   */
  virtual bool enablePolling() = 0;

  /**
   * Stop polling for devices.
   *
   * @return True if ok.
   */
  virtual bool disablePolling() = 0;

  /**
   * Start peer-to-peer listening for devices.
   *
   * @return True if ok.
   */
  virtual bool enableP2pListening() = 0;

  /**
   * Stop peer-to-peer listening for devices.
   *
   * @return True if ok.
   */
  virtual bool disableP2pListening() = 0;

  /**
   * Check Llcp connection.
   *
   * @return True if ok.
   */
  virtual bool checkLlcp() = 0;

  /**
   * Activate Llcp connection.
   *
   * @return True if ok.
   */
  virtual bool activateLlcp() = 0;

  /**
   * Create a LLCP connection-oriented socket.
   *
   * @param  sap                Service access point.
   * @param  miu                Maximum information unit.
   * @param  rw                 Receive window size.
   * @param  linearBufferLength Max buffer size.
   * @return                    ILlcpSocket interface.
   */
  virtual ILlcpSocket* createLlcpSocket(int sap, int miu, int rw, int linearBufferLength) = 0;

  /**
   * Create a new LLCP server socket.
   *
   * @param  nSap               Service access point.
   * @param  sn                 Service name.
   * @param  miu                Maximum information unit.
   * @param  rw                 Receive window size.
   * @param  linearBufferLength Max buffer size.
   * @return                    ILlcpServerSocket interface.
   */
  virtual ILlcpServerSocket* createLlcpServerSocket(int sap, const char* sn, int miu, int rw, int linearBufferLength) = 0;

  /**
   * Create a new LLCP server socket.
   *
   * @param  nSap               Service access point.
   * @param  sn                 Service name.
   * @param  miu                Maximum information unit.
   * @param  rw                 Receive window size.
   * @param  linearBufferLength Max buffer size.
   * @return                    ILlcpServerSocket interface.
   */
  virtual void setP2pInitiatorModes(int modes) = 0;

  /**
   * Set P2P target's activation modes.
   *
   * @param  modes Active and/or passive modes.
   * @return       None.
   */
  virtual void setP2pTargetModes(int modes) = 0;

  /**
   * Get default Llcp connection maxumum information unit.
   *
   * @return Default MIU.
   */
  virtual int getDefaultLlcpMiu() const = 0;

  /**
   * Get default Llcp connection receive window size.
   *
   * @return Default receive window size.
   */
  virtual int getDefaultLlcpRwSize() const = 0;

  /**
   * NFC controller starts routing data in listen mode.
   *
   * @return True if ok.
   */
  virtual bool doSelectSecureElement() = 0;

  /**
   * NFC controller stops routing data in listen mode.
   *
   * @return True if ok.
   */
  virtual bool doDeselectSecureElement() = 0;
};

#endif
