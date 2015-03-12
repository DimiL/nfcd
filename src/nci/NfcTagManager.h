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

#ifndef mozilla_nfcd_NfcTagManager_h
#define mozilla_nfcd_NfcTagManager_h

#include <pthread.h>
#include <vector>

#include "INfcTag.h"
#include "NfcNciUtil.h"

extern "C"
{
  #include "nfa_rw_api.h"
}

class NfcTagManager
  : public INfcTag
{
public:
  NfcTagManager();
  virtual ~NfcTagManager();

  // INfcTag interface.
  bool Connect(TagTechnology aTechnology);
  bool Disconnect();
  bool Reconnect();
  NdefMessage* ReadNdef();
  NdefInfo* ReadNdefInfo();
  bool WriteNdef(NdefMessage& aNdef);
  bool PresenceCheck();
  bool MakeReadOnly();
  bool FormatNdef();
  bool Transceive(const std::vector<uint8_t>& aCommand,
                  std::vector<uint8_t>& aOutResponse);

  std::vector<TagTechnology>& GetTechList() { return mTechList; };
  std::vector<int>& GetTechHandles() { return mTechHandles; };
  std::vector<int>& GetTechLibNfcTypes() { return mTechLibNfcTypes; };
  std::vector<std::vector<uint8_t> >& GetTechPollBytes() { return mTechPollBytes; };
  std::vector<std::vector<uint8_t> >& GetTechActBytes() { return mTechActBytes; };
  std::vector<uint8_t>& GetUid() { return mUid; };
  int& GetConnectedHandle() { return mConnectedHandle; };

  /**
   * Does the tag contain a NDEF message?
   *
   * @param  aNdefInfo NDEF info.
   * @return           Status code; 0 is success.
   */
  static int DoCheckNdef(int aNdefInfo[]);

  /**
   * Notify tag I/O operation is timeout.
   *
   * @return None
   */
  static void NotifyRfTimeout();

   /**
   * Receive the completion status of transceive operation.
   *
   * @param  aBuf    Contains tag's response.
   * @param  aBufLen Length of buffer.
   * @return None
   */
  static void DoTransceiveComplete(uint8_t* aBuf,
                                   uint32_t aBufLen);

  /**
   * Send raw data to the tag; receive tag's response.
   *
   * @param  aCommand     Contains command to send.
   * @param  aOutResponse Contains tag's response.
   * @return True if ok.
   */
  static bool DoTransceive(const std::vector<uint8_t>& aCommand,
                           std::vector<uint8_t>& aOutResponse);

  /**
   * Read the NDEF message on the tag.
   *
   * @param  aBuf NDEF message read from tag.
   * @return      None.
   */
  static void DoRead(std::vector<uint8_t>& aBuf);

  /**
   * Write a NDEF message to the tag.
   *
   * @param  aBuf Contains a NDEF message.
   * @return      True if ok.
   */
  static bool DoWrite(std::vector<uint8_t>& aBuf);

  /**
   * Unblock all thread synchronization objects.
   *
   * @return None.
   */
  static void DoAbortWaits();

  /**
   * Receive the completion status of read operation. Called by
   * NFA_READ_CPLT_EVT.
   *
   * @param  aStatus Status of operation.
   * @return         None.
   */
  static void DoReadCompleted(tNFA_STATUS aStatus);

  /**
   * Receive the completion status of write operation. Called by
   * NFA_WRITE_CPLT_EVT.
   *
   * @param  aIsWriteOk Status of operation.
   * @return            None.
   */
  static void DoWriteStatus(bool aIsWriteOk);

  /**
   * Receive the completion status of connect operation.
   *
   * @param  aIsConnectOk Status of the operation.
   * @return              None.
   */
  static void DoConnectStatus(bool aIsConnectOk);

  /**
   * Receive the completion status of deactivate operation.
   *
   * @param  aStatus Status of operation.
   * @return None.
   */
  static void DoDeactivateStatus(int aStatus);

  /**
   * Connect to the tag in RF field.
   *
   * @param  aTargetHandle Handle of the tag.
   * @return Must return NXP status code, which NFC service expects.
   */
  static int DoConnect(int aTargetHandle);

  /**
   * Reset variables related to presence-check.
   *
   * @return None.
   */
  static void DoResetPresenceCheck();

  /**
   * Receive the result of presence-check.
   *
   * @param  aStatus Result of presence-check.
   * @return         None.
   */
  static void DoPresenceCheckResult(tNFA_STATUS aStatus);

  /**
   * Receive the result of checking whether the tag contains a NDEF
   * message. Called by the NFA_NDEF_DETECT_EVT.
   *
   * @param  aStatus      Status of the operation.
   * @param  aMaxSize     Maximum size of NDEF message.
   * @param  aCurrentSize Current size of NDEF message.
   * @param  aFlags       Indicate various states.
   * @return              None.
   */
  static void DoCheckNdefResult(tNFA_STATUS aStatus,
                                uint32_t aMaxSize,
                                uint32_t aCurrentSize,
                                uint8_t aFlags);

  /**
   * Register a callback to receive NDEF message from the tag
   * from the NFA_NDEF_DATA_EVT.
   *
   * @return None.
   */
  static void DoRegisterNdefTypeHandler();

  /**
   * No longer need to receive NDEF message from the tag.
   *
   * @return None.
   */
  static void DoDeregisterNdefTypeHandler();

  /**
   * Check if the tag is in the RF field.
   *
   * @return None.
   */
  static bool DoPresenceCheck();

  /**
   * Deactivate the RF field.
   *
   * @return True if ok.
   */
  static bool DoDisconnect();

  /**
   * Receive the result of making a tag read-only. Called by the
   * NFA_SET_TAG_RO_EVT.
   *
   * @param  aStatus Status of the operation.
   * @return         None.
   */
  static void DoMakeReadonlyResult(tNFA_STATUS aStatus);

  /**
   * Make the tag read-only.
   *
   * @return True if ok.
   */
  static bool DoMakeReadonly();

  /**
   * Receive the completion status of format operation. Called by NFA_FORMAT_CPLT_EVT.
   *
   * @param  aIsOk Status of operation.
   * @return       None.
   */
  static void DoFormatStatus(bool aIsOk);

  /**
   * Format a tag so it can store NDEF message.
   *
   * @return True if ok.
   */
  static bool DoNdefFormat();

  static bool DoIsNdefFormatable();

  int ConnectWithStatus(TechnologyType aTechnology);
  int ReconnectWithStatus(int aTargetHandle);
  int ReconnectWithStatus();
  NdefMessage* DoReadNdef();
  NdefInfo* DoReadNdefInfo();
  bool IsNdefFormatable();

private:
  pthread_mutex_t mMutex;

  std::vector<TagTechnology> mTechList;
  std::vector<int> mTechHandles;
  std::vector<int> mTechLibNfcTypes;
  std::vector<std::vector<uint8_t> > mTechPollBytes;
  std::vector<std::vector<uint8_t> > mTechActBytes;
  std::vector<uint8_t> mUid;

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
  int mConnectedTechIndex; // Index in mTechHandles.

  bool mIsPresent; // Whether the tag is known to be still present.

  /**
   * Deactivates the tag and re-selects it with the specified
   * rf interface.
   *
   * @param  aRfInterface Type of RF interface.
   * @return              Status code, 0 on success, 1 on failure,
   *                      146 (defined in service) on tag lost.
   */
  static int ReSelect(tNFA_INTF_TYPE aRfInterface);

  /**
   * Switch controller's RF interface to frame, ISO-DEP, or NFC-DEP.
   *
   * @param  aRfInterface Type of RF interface.
   * @return             True if ok.
   */
  static bool SwitchRfInterface(tNFA_INTF_TYPE aRfInterface);

  /**
   * Check if specified technology is mifare
   *
   * @param  aTechType Technology to check.
   * @return           True if it is Mifare.
   */
  static bool IsMifareTech(int aTechType);

  NdefType GetNdefType(int aLibnfcType);

  int GetConnectedLibNfcType();
};

#endif // mozilla_nfcd_NfcTagManager_h
