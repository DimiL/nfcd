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

class NfcTagManager : INfcTag
{
public:
  NfcTagManager();
  virtual ~NfcTagManager();

  NdefMessage* findAndReadNdef();
  NdefDetail* ReadNdefDetail();
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

  std::vector<TagTechnology>& getTechList() { return mTechList; };
  std::vector<int>& getTechHandles() { return mTechHandles; };
  std::vector<int>& getTechLibNfcTypes() { return mTechLibNfcTypes; };
  std::vector<std::vector<uint8_t> >& getTechPollBytes() { return mTechPollBytes; };
  std::vector<std::vector<uint8_t> >& getTechActBytes() { return mTechActBytes; };
  std::vector<std::vector<uint8_t> >& getUid() { return mUid; };
  int& getConnectedHandle() { return mConnectedHandle; };

  static void doRead(std::vector<uint8_t>& buf);
  static int doCheckNdef(int ndefInfo[]);
  static bool doWrite(std::vector<uint8_t>& buf);
  static void doAbortWaits();
  static void doReadCompleted(tNFA_STATUS status);
  static void doWriteStatus(bool isWriteOk);
  static void doConnectStatus(bool isConnectOk);
  static void doDeactivateStatus(int status);
  static int doConnect(int targetHandle);
  static void doResetPresenceCheck();
  static void doPresenceCheckResult(tNFA_STATUS status);
  static void doCheckNdefResult(tNFA_STATUS status, uint32_t maxSize, uint32_t currentSize, uint8_t flags);
  static void doRegisterNdefTypeHandler();
  static void doDeregisterNdefTypeHandler();
  static bool doPresenceCheck();
  static bool doDisconnect();
  static void doMakeReadonlyResult(tNFA_STATUS status);
  static bool doMakeReadonly();

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
  int mConnectedTechIndex; // Index in mTechHandles

  bool mIsPresent; // Whether the tag is known to be still present

  static int reSelect(tNFA_INTF_TYPE rfInterface);
  static bool switchRfInterface(tNFA_INTF_TYPE rfInterface);
};

#endif // mozilla_nfcd_NfcTagManager_h
