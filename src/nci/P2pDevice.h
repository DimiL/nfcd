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

#ifndef mozilla_nfcd_P2pDevice_h
#define mozilla_nfcd_P2pDevice_h

#include "IP2pDevice.h"

class P2pDevice
  : public IP2pDevice
{
public:
  P2pDevice();
  virtual ~P2pDevice();

  bool Connect();
  bool Disconnect();
  void Transceive();
  void Receive();
  bool Send();

  int& GetHandle();
  int& GetMode();

private:
  int mHandle;
  int mMode;
  unsigned char* mGeneralBytes;
};

#endif // mozilla_nfcd_P2pDevice_h
