/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NativeNfcManager_h__
#define mozilla_NativeNfcManager_h__

#include "DeviceHost.h"

class NativeP2pDevice;

class NativeNfcManager : public DeviceHost
{
public:

  NativeNfcManager();
  virtual ~NativeNfcManager();

  bool initialize();
  void enableDiscovery();
  void* getNativeStruct(const char* name);

private:
  NativeP2pDevice* mNativeP2pDevice;

  void initializeNativeStructure();
  bool doInitialize();
};

#endif 
