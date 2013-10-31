/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_INfcTag_h
#define mozilla_nfcd_INfcTag_h

#include "TagTechnology.h"
#include <vector>

#define INTERFACE_TAG_MANAGER "NfcTagManager"

class NdefMessage;
class NdefDetail;

class INfcTag {
public:
  virtual ~INfcTag() {};

  /**
   * Deactivate the RF field.
   *
   * @return True if ok.
   */
  virtual bool disconnect() = 0;

  /**
   * Re-connect to the tag in RF field.
   *
   * @return True if ok.
   */
  virtual bool reconnect() = 0;

  /**
   * Connect to the tag in RF field.
   *
   * @param  technology Specify the tag technology to be connected.
   * @return            Status code.
   */
  virtual int connectWithStatus(int technology) = 0;

  /**
   * Read the NDEF message on the tag.
   *
   * @return NDEF message.
   */
  virtual NdefMessage* findAndReadNdef() = 0;

  /**
   * Read tag information and fill the NdefDetail structure.
   *
   * @return NDEF detail structure.
   */
  virtual NdefDetail* ReadNdefDetail() = 0;

  /**
   * Write a NDEF message to the tag.
   *
   * @param  ndef Contains a NDEF message.
   * @return      True if ok.
   */
  virtual bool writeNdef(NdefMessage& ndef) = 0;

  /**
   * Check if the tag is in the RF field.
   *
   * @return True if tag is in RF field.
   */
  virtual bool presenceCheck() = 0;

  /**
   * Make the tag read-only.
   *
   * @return True if ok.
   */
  virtual bool makeReadOnly() = 0;

  /**
   * Format a tag so it can store NDEF message.
   *
   * @return True if ok.
   */
  virtual bool formatNdef() = 0;

  virtual std::vector<TagTechnology>& getTechList() = 0;
  virtual std::vector<int>& getTechHandles() = 0;
  virtual std::vector<int>& getTechLibNfcTypes() = 0;
  virtual std::vector<std::vector<uint8_t> >& getTechPollBytes() = 0;
  virtual std::vector<std::vector<uint8_t> >& getTechActBytes() = 0;
  virtual std::vector<std::vector<uint8_t> >& getUid() = 0;
  virtual int& getConnectedHandle() = 0;
};

#endif
