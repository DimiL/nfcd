/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcManager_h
#define mozilla_nfcd_NfcManager_h

#include "DeviceHost.h"
#include "P2pDevice.h"
#include "LlcpServiceSocket.h"
#include "INfcManager.h"

class NfcTagManager;
class ILlcpServerSocket;
class ILlcpSocket;

class NfcManager : public DeviceHost, public INfcManager
{
public:
  static const int DEFAULT_LLCP_MIU = 1980;
  static const int DEFAULT_LLCP_RWSIZE = 2;

  NfcManager();
  virtual ~NfcManager();

  void* queryInterface(const char* name);

  bool doInitialize();
  bool doDeinitialize();

  void enableDiscovery();
  void disableDiscovery();

  bool doCheckLlcp();
  bool doActivateLlcp();

  ILlcpSocket* createLlcpSocket(int sap, int miu, int rw, int linearBufferLength);
  ILlcpServerSocket* createLlcpServerSocket(int nSap, const char* sn, int miu, int rw, int linearBufferLength);

  void setP2pInitiatorModes(int modes);
  void setP2pTargetModes(int modes);

  int getDefaultLlcpMiu();
  int getDefaultLlcpRwSize();

private:
  P2pDevice* mP2pDevice;
  NfcTagManager* mNfcTagManager;
};

#endif 
