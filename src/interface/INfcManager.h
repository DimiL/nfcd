/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_INfcManager_h
#define mozilla_nfcd_INfcManager_h

#include "ILlcpServerSocket.h"
#include "ILlcpSocket.h"

class INfcManager {
public:
  virtual ~INfcManager() {};

  /**
   * Tp get a specific interface from NfcManager
   *
   * @param  name Interface name
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
   * @return None.
   */
  virtual void enableDiscovery() = 0;

  /**
   * Stop polling and listening for devices.
   *
   * @return None.
   */
  virtual void disableDiscovery() = 0;

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
};

#endif
