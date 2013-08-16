/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NativeNfcTag_h__
#define mozilla_NativeNfcTag_h__

#include <vector>

typedef unsigned char byte;

class NativeNfcTag
{
public:
  NativeNfcTag();
  ~NativeNfcTag();

  static const int STATUS_CODE_TARGET_LOST = 146;

  std::vector<int> mTechList;
  std::vector<int> mTechHandles;
  std::vector<int> mTechLibNfcTypes;
  // Dimi : TODO, java Bundle to C++...
  // Bundle[] mTechExtras;
  std::vector<std::vector<byte> > mTechPollBytes;
  std::vector<std::vector<byte> > mTechActBytes;
  std::vector<std::vector<byte> > mUid;

  // mConnectedHandle stores the *real* libnfc handle
  // that we're connected to.
  int mConnectedHandle;

  // mConnectedTechIndex stores to which technology
  // the upper layer stack is connected. Note that
  // we may be connected to a libnfchandle without being
  // connected to a technology - technology changes
  // may occur runtime, whereas the underlying handle
  // could stay present. Usually all technologies are on the
  // same handle, with the exception of multi-protocol
  // tags.
  int mConnectedTechIndex; // Index in mTechHandles

  bool mIsPresent; // Whether the tag is known to be still present

private:

};

#endif // mozilla_NativeNfcTag_h__
