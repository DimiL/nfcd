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
   * @param  aName Interface name.
   * @return       Return specific interface if exist, null if cannot find.
   */
  virtual void* QueryInterface(const char* aName) = 0;

  /**
   * Turn on NFC.
   *
   * @return True if ok.
   */
  virtual bool Initialize() = 0;

  /**
   * Turn off NFC.
   *
   * @return True if ok.
   */
  virtual bool Deinitialize() = 0;

  /**
   * Start polling and listening for devices.
   *
   * @return True if ok.
   */
  virtual bool EnableDiscovery() = 0;

  /**
   * Stop polling and listening for devices.
   *
   * @return True if ok.
   */
  virtual bool DisableDiscovery() = 0;

  /**
   * Start polling for devices.
   *
   * @return True if ok.
   */
  virtual bool EnablePolling() = 0;

  /**
   * Stop polling for devices.
   *
   * @return True if ok.
   */
  virtual bool DisablePolling() = 0;

  /**
   * Start peer-to-peer listening for devices.
   *
   * @return True if ok.
   */
  virtual bool EnableP2pListening() = 0;

  /**
   * Stop peer-to-peer listening for devices.
   *
   * @return True if ok.
   */
  virtual bool DisableP2pListening() = 0;

  /**
   * Check Llcp connection.
   *
   * @return True if ok.
   */
  virtual bool CheckLlcp() = 0;

  /**
   * Activate Llcp connection.
   *
   * @return True if ok.
   */
  virtual bool ActivateLlcp() = 0;

  /**
   * Create a LLCP connection-oriented socket.
   *
   * @param  aSap    Service access point.
   * @param  aMiu    Maximum information unit.
   * @param  aRw     Receive window size.
   * @param  aBufLen Length Max buffer size.
   * @return         ILlcpSocket interface.
   */
  virtual ILlcpSocket* CreateLlcpSocket(int aSap,
                                        int aMiu,
                                        int aRw,
                                        int aBufLen) = 0;

  /**
   * Create a new LLCP server socket.
   *
   * @param  aSap    Service access point.
   * @param  aSn     Service name.
   * @param  aMiu    Maximum information unit.
   * @param  aRw     Receive window size.
   * @param  aBufLen Length Max buffer size.
   * @return         ILlcpServerSocket interface.
   */
  virtual ILlcpServerSocket* CreateLlcpServerSocket(int aSap,
                                                    const char* aSn,
                                                    int aMiu,
                                                    int aRw,
                                                    int aBufLen) = 0;

  /**
   * Get default Llcp connection maxumum information unit.
   *
   * @return Default MIU.
   */
  virtual int GetDefaultLlcpMiu() const = 0;

  /**
   * Get default Llcp connection receive window size.
   *
   * @return Default receive window size.
   */
  virtual int GetDefaultLlcpRwSize() const = 0;

  /**
   * NFC controller starts routing data in listen mode.
   *
   * @return True if ok.
   */
  virtual bool EnableSecureElement() = 0;

  /**
   * NFC controller stops routing data in listen mode.
   *
   * @return True if ok.
   */
  virtual bool DisableSecureElement() = 0;
};

#endif
