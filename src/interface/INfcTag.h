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

#ifndef mozilla_nfcd_INfcTag_h
#define mozilla_nfcd_INfcTag_h

#include "TagTechnology.h"
#include <vector>

#define INTERFACE_TAG_MANAGER "NfcTagManager"

class NdefMessage;
class NdefInfo;

class INfcTag {
public:
  virtual ~INfcTag() {};

  /**
   * Connect to the tag in RF field.
   *
   * @param  aTechnology Specify the tag technology to be connected.
   * @return             True if ok.
   */
  virtual bool Connect(TagTechnology aTechnology) = 0;

  /**
   * Deactivate the RF field.
   *
   * @return True if ok.
   */
  virtual bool Disconnect() = 0;

  /**
   * Re-connect to the tag in RF field.
   *
   * @return True if ok.
   */
  virtual bool Reconnect() = 0;

  /**
   * Read the NDEF message on the tag.
   *
   * @return NDEF message.
   */
  virtual NdefMessage* ReadNdef() = 0;

  /**
   * Read tag information and fill the NdefInfo structure.
   *
   * @return NDEF Info structure.
   */
  virtual NdefInfo* ReadNdefInfo() = 0;

  /**
   * Write a NDEF message to the tag.
   *
   * @param  aNdef Contains a NDEF message.
   * @return       True if ok.
   */
  virtual bool WriteNdef(NdefMessage& aNdef) = 0;

  /**
   * Check if the tag is in the RF field.
   *
   * @return True if tag is in RF field.
   */
  virtual bool PresenceCheck() = 0;

  /**
   * Make the tag read-only.
   *
   * @return True if ok.
   */
  virtual bool MakeReadOnly() = 0;

  /**
   * Format a tag so it can store NDEF message.
   *
   * @return True if ok.
   */
  virtual bool FormatNdef() = 0;

  /**
   * Send raw data to the tag.
   *
   * @param  aCommand     Contains command to send.
   * @param  aOutResponse Contains tag's response.
   * @return True if ok.
   */
  virtual bool Transceive(const std::vector<uint8_t>& aCommand,
                          std::vector<uint8_t>& aOutResponse) = 0;

  /**
   * Get detected tag supported technologies.
   *
   * @return Technologies supported by the tag.
   */
  virtual std::vector<TagTechnology>& GetTechList() = 0;

  virtual std::vector<int>& GetTechHandles() = 0;
  virtual std::vector<int>& GetTechLibNfcTypes() = 0;
  virtual std::vector<std::vector<uint8_t> >& GetTechPollBytes() = 0;
  virtual std::vector<std::vector<uint8_t> >& GetTechActBytes() = 0;
  virtual std::vector<uint8_t>& GetUid() = 0;
  virtual int& GetConnectedHandle() = 0;
};

#endif
