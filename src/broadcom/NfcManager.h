/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcManager_h
#define mozilla_nfcd_NfcManager_h

#include "DeviceHost.h"
#include "P2pDevice.h"
#include "NativeNfcTag.h"

class NfcManager : public DeviceHost
{
public:

  NfcManager();
  virtual ~NfcManager();

  bool initialize();
  void enableDiscovery();
  void* getNativeStruct(const char* name);

private:
  P2pDevice* mP2pDevice;
  NativeNfcTag* mNativeNfcTag;

  void initializeNativeStructure();
  bool doInitialize();
};

#endif 
