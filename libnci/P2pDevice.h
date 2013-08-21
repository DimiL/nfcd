/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_P2pDevice_h__
#define mozilla_P2pDevice_h__

class P2pDevice
{
public:

  P2pDevice();
  virtual ~P2pDevice();

  int mHandle;
  int mMode;
  unsigned char* mGeneralBytes;

private:
};

#endif // mozilla_P2pDevice_h__
