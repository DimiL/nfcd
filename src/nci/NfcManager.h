/*
 * Copyright (C) 2014  Mozilla Foundation
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

#ifndef mozilla_nfcd_NfcManager_h
#define mozilla_nfcd_NfcManager_h

#include "DeviceHost.h"
#include "INfcManager.h"

class P2pDevice;
class NfcTagManager;
class ILlcpServerSocket;
class ILlcpSocket;

class NfcManager
  : public DeviceHost
  , public INfcManager
{
public:
  static const int DEFAULT_LLCP_MIU = 1980;
  static const int DEFAULT_LLCP_RWSIZE = 2;

  NfcManager();
  virtual ~NfcManager();

  /**
   * To get a specific interface from NfcManager
   *
   * @param  aName Interface name
   * @return       Return specific interface if exist, null if cannot find.
   */
  void* QueryInterface(const char* aName);

  /**
   * Turn on NFC.
   *
   * @return True if ok.
   */
  bool Initialize();

  /**
   * Turn off NFC.
   *
   * @return True if ok.
   */
  bool Deinitialize();

  /**
   * Start polling and listening for devices.
   *
   * @return True if ok.
   */
  bool EnableDiscovery();

  /**
   * Stop polling and listening for devices.
   *
   * @return True if ok.
   */
  bool DisableDiscovery();

  /**
   * Start polling for devices.
   *
   * @return True if ok.
   */
  bool EnablePolling();

  /**
   * Stop polling for devices.
   *
   * @return True if ok.
   */
  bool DisablePolling();

  /**
   * Start peer-to-peer listening for devices.
   *
   * @return True if ok.
   */
  bool EnableP2pListening();

  /**
   * Stop peer-to-peer listening for devices.
   *
   * @return True if ok.
   */
  bool DisableP2pListening();

  /**
   * Check Llcp connection.
   * Not used in NCI case.
   *
   * @return True if ok.
   */
  bool CheckLlcp();

  /**
   * Activate Llcp connection.
   * Not used in NCI case.
   *
   * @return True if ok.
   */
  bool ActivateLlcp();

  /**
   * Create a LLCP connection-oriented socket.
   *
   * @param  aSap    Service access point.
   * @param  aMiu    Maximum information unit.
   * @param  aRw     Receive window size.
   * @param  aBufLen Max buffer size.
   * @return         ILlcpSocket interface.
   */
  ILlcpSocket* CreateLlcpSocket(int aSap,
                                int aMiu,
                                int aRw,
                                int aBufLen);

  /**
   * Create a new LLCP server socket.
   *
   * @param  aSap    Service access point.
   * @param  aSn     Service name.
   * @param  aMiu    Maximum information unit.
   * @param  aRw     Receive window size.
   * @param  aBugLen Max buffer size.
   * @return         LlcpServerSocket interface.
   */
  ILlcpServerSocket* CreateLlcpServerSocket(int aSap,
                                            const char* aSn,
                                            int aMiu,
                                            int aRw,
                                            int aBufLen);

  /**
   * Set P2P initiator's activation modes.
   *
   * @param  aModes Active and/or passive modes.
   * @return        None.
   */
  void SetP2pInitiatorModes(int aModes);

  /**
   * Set P2P target's activation modes.
   *
   * @param  modes Active and/or passive modes.
   * @return       None.
   */
  void SetP2pTargetModes(int aModes);

  /**
   * Get default Llcp connection maxumum information unit
   *
   * @return Default MIU.
   */
  int GetDefaultLlcpMiu() const { return NfcManager::DEFAULT_LLCP_MIU; };

  /**
   * Get default Llcp connection receive window size
   *
   * @return Default receive window size
   */
  int GetDefaultLlcpRwSize() const { return NfcManager::DEFAULT_LLCP_RWSIZE; };

  /**
   * NFC controller starts routing data in listen mode.
   *
   * @return True if ok.
   */
  bool DoSelectSecureElement();

  /**
   * NFC controller stops routing data in listen mode.
   *
   * @return True if ok.
   */
  bool DoDeselectSecureElement();

private:
  P2pDevice* mP2pDevice;
  NfcTagManager* mNfcTagManager;
};

#endif
