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

#ifndef mozilla_nfcd_IP2pDevice_h
#define mozilla_nfcd_IP2pDevice_h

#define INTERFACE_P2P_DEVICE  "P2pDevice"

/**
 * In current implementation, this interface is not yet being used.
 */
class IP2pDevice {
public:
  virtual ~IP2pDevice() {};

  virtual bool Connect() = 0;
  virtual bool Disconnect() = 0;
  virtual void Transceive() = 0;
  virtual void Receive() = 0;
  virtual bool Send() = 0;

  virtual int& GetMode() =0;
  virtual int& GetHandle() = 0;
};

#endif
