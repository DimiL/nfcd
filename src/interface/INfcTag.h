/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_INfcTag_h
#define mozilla_nfcd_INfcTag_h

#include "NdefMessage.h"
#include "TagTechnology.h"
#include <vector>

class INfcTag {
public:
  virtual ~INfcTag() {};

  virtual bool disconnect() = 0;
  virtual bool reconnect() = 0;
  virtual int connectWithStatus(int technology) = 0;
  virtual NdefMessage* findAndReadNdef() = 0;
  virtual NdefDetail* ReadNdefDetail() = 0;
  virtual bool writeNdef(NdefMessage& ndef) = 0;
  virtual bool presenceCheck() = 0;
  virtual bool makeReadOnly() = 0;

  virtual std::vector<TagTechnology>& getTechList() = 0;
  virtual std::vector<int>& getTechHandles() = 0;
  virtual std::vector<int>& getTechLibNfcTypes() = 0;
  virtual std::vector<std::vector<uint8_t> >& getTechPollBytes() = 0;
  virtual std::vector<std::vector<uint8_t> >& getTechActBytes() = 0;
  virtual std::vector<std::vector<uint8_t> >& getUid() = 0;
  virtual int& getConnectedHandle() = 0;
};

#endif
