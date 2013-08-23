/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_P2pDevice_h
#define mozilla_nfcd_P2pDevice_h

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

#endif // mozilla_nfcd_P2pDevice_h
