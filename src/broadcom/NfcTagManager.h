/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcTagManager_h
#define mozilla_nfcd_NfcTagManager_h

#include <pthread.h>
#include <vector>

#include "INfcTag.h"
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

  NdefMessage* findAndReadNdef();
  NdefDetail* readNdefDetail();
  int reconnectWithStatus(int technology);
  int reconnectWithStatus();
  int connectWithStatus(int technology);
  void readNdef(std::vector<uint8_t>& buf);
  bool writeNdef(NdefMessage& ndef);
  int checkNdefWithStatus(int ndefinfo[]);
  bool disconnect();
  bool reconnect();
  bool presenceCheck();
  bool makeReadOnly();
  bool isNdefFormatable();
  bool formatNdef();

  std::vector<TagTechnology>& getTechList() { return mTechList; };
  std::vector<int>& getTechHandles() { return mTechHandles; };
  std::vector<int>& getTechLibNfcTypes() { return mTechLibNfcTypes; };
  std::vector<std::vector<uint8_t> >& getTechPollBytes() { return mTechPollBytes; };
  std::vector<std::vector<uint8_t> >& getTechActBytes() { return mTechActBytes; };
  std::vector<std::vector<uint8_t> >& getUid() { return mUid; };
  int& getConnectedHandle() { return mConnectedHandle; };

  /**
   * Does the tag contain a NDEF message?
   *
   * @param  ndefInfo NDEF info.
   * @return          Status code; 0 is success.
   */
  static int doCheckNdef(int ndefInfo[]);

  /**
   * Read the NDEF message on the tag.
   *
   * @param  buf NDEF message read from tag.
   * @return     None.
   */
  static void doRead(std::vector<uint8_t>& buf);

  /**
   * Write a NDEF message to the tag.
   *
   * @param  buf Contains a NDEF message.
   * @return     True if ok.
   */
  static bool doWrite(std::vector<uint8_t>& buf);

  /**
   * Unblock all thread synchronization objects.
   *
   * @return None.
   */
  static void doAbortWaits();

  /**
   * Receive the completion status of read operation. Called by
   * NFA_READ_CPLT_EVT.
   *
   * @param  status Status of operation.
   * @return        None.
   */
  static void doReadCompleted(tNFA_STATUS status);

  /**
   * Receive the completion status of write operation. Called by
   * NFA_WRITE_CPLT_EVT.
   *
   * @param  isWriteOk Status of operation.
   * @return           None.
   */
  static void doWriteStatus(bool isWriteOk);

  /**
   * Receive the completion status of connect operation.
   *
   * @param  isConnectOk Status of the operation.
   * @return             None.
   */
  static void doConnectStatus(bool isConnectOk);

  /**
   * Receive the completion status of deactivate operation.
   *
   * @return None.
   */
  static void doDeactivateStatus(int status);

  /**
   * Connect to the tag in RF field.
   *
   * @param  targetHandle Handle of the tag.
   * @return Must return NXP status code, which NFC service expects.
   */
  static int doConnect(int targetHandle);

  /**
   * Reset variables related to presence-check.
   *
   * @return None.
   */
  static void doResetPresenceCheck();

  /**
   * Receive the result of presence-check.
   *
   * @param  status Result of presence-check.
   * @return        None.
   */
  static void doPresenceCheckResult(tNFA_STATUS status);

  /**
   * Receive the result of checking whether the tag contains a NDEF
   * message. Called by the NFA_NDEF_DETECT_EVT.
   *
   * @param  status      Status of the operation.
   * @param  maxSize     Maximum size of NDEF message.
   * @param  currentSize Current size of NDEF message.
   * @param  flags       Indicate various states.
   * @return             None.
   */
  static void doCheckNdefResult(tNFA_STATUS status, uint32_t maxSize, uint32_t currentSize, uint8_t flags);

  /**
   * Register a callback to receive NDEF message from the tag
   * from the NFA_NDEF_DATA_EVT.
   *
   * @return None.
   */
  static void doRegisterNdefTypeHandler();

  /**
   * No longer need to receive NDEF message from the tag.
   *
   * @return None.
   */
  static void doDeregisterNdefTypeHandler();

  /**
   * Check if the tag is in the RF field.
   *
   * @return None.
   */
  static bool doPresenceCheck();

  /**
   * Deactivate the RF field.
   *
   * @return True if ok.
   */
  static bool doDisconnect();

  /**
   * Receive the result of making a tag read-only. Called by the
   * NFA_SET_TAG_RO_EVT.
   *
   * @param  status Status of the operation.
   * @return        None.
   */
  static void doMakeReadonlyResult(tNFA_STATUS status);

  /**
   * Make the tag read-only.
   *
   * @return True if ok.
   */
  static bool doMakeReadonly();

  /**
   * Receive the completion status of format operation. Called by NFA_FORMAT_CPLT_EVT.
   *
   * @param  isOk Status of operation.
   * @return      None.
   */
  static void formatStatus(bool isOk);

  /**
   * Format a tag so it can store NDEF message.
   *
   * @return True if ok.
   */
  static bool doNdefFormat();

  static bool doIsNdefFormatable();

private:
  pthread_mutex_t mMutex;

  std::vector<TagTechnology> mTechList;
  std::vector<int> mTechHandles;
  std::vector<int> mTechLibNfcTypes;
  std::vector<std::vector<uint8_t> > mTechPollBytes;
  std::vector<std::vector<uint8_t> > mTechActBytes;
  std::vector<std::vector<uint8_t> > mUid;

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
   * @param  rfInterface Type of RF interface.
   * @return             Status code, 0 on success, 1 on failure,
   *                     146 (defined in service) on tag lost.
   */
  static int reSelect(tNFA_INTF_TYPE rfInterface);

  /**
   * Switch controller's RF interface to frame, ISO-DEP, or NFC-DEP.
   * 
   * @param  rfInterface Type of RF interface.
   * @return             True if ok.
   */
  static bool switchRfInterface(tNFA_INTF_TYPE rfInterface);

  int getNdefType(int libnfcType);

  void addTechnology(TagTechnology tech, int handle, int libnfctype);

  int getConnectedLibNfcType();
};

#endif // mozilla_nfcd_NfcTagManager_h
