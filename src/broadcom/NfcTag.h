/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Tag-reading, tag-writing operations.
 */

#pragma once
#include "SyncEvent.h"
#include "NfcUtil.h"
#include "NfcManager.h"

extern "C"
{
  #include "nfa_rw_api.h"
}

class INfcTag;

class NfcTag
{
public:
  enum ActivationState {Idle, Sleep, Active};
  static const int MAX_NUM_TECHNOLOGY = 10; //max number of technologies supported by one or more tags
  int mTechList [MAX_NUM_TECHNOLOGY]; //array of NFC technologies according to NFC service
  int mTechHandles [MAX_NUM_TECHNOLOGY]; //array of tag handles according to NFC service
  int mTechLibNfcTypes [MAX_NUM_TECHNOLOGY]; //array of detailed tag types according to NFC service
  int mNumTechList; //current number of NFC technologies in the list

  NfcTag ();

  static NfcTag& getInstance ();

  void initialize (NfcManager* pNfcManager);
  void abort ();

  void connectionEventHandler (UINT8 event, tNFA_CONN_EVT_DATA* data);

  ActivationState getActivationState ();
  void setDeactivationState (tNFA_DEACTIVATED& deactivated);
  void setActivationState ();

  tNFC_PROTOCOL getProtocol();

  bool isP2pDiscovered();
  void selectP2p();
  void selectFirstTag();

  int getT1tMaxMessageSize();
  bool isMifareUltralight();
  bool isT2tNackResponse(const UINT8* response, UINT32 responseLen);
  bool isNdefDetectionTimedOut();

private:
  ActivationState mActivationState;
  tNFC_PROTOCOL mProtocol;
  int mtT1tMaxMessageSize; //T1T max NDEF message size
  tNFA_STATUS mReadCompletedStatus;
  int mLastKovioUidLen;   // len of uid of last Kovio tag activated
  bool mNdefDetectionTimedOut; // whether NDEF detection algorithm timed out
  tNFC_RF_TECH_PARAMS mTechParams [MAX_NUM_TECHNOLOGY]; //array of technology parameters
  SyncEvent mReadCompleteEvent;
  struct timespec mLastKovioTime; // time of last Kovio tag activation
  UINT8 mLastKovioUid[NFC_KOVIO_MAX_LEN]; // uid of last Kovio tag activated

  NfcManager*     mNfcManager;

  bool IsSameKovio(tNFA_ACTIVATED& activationData);

  void discoverTechnologies(tNFA_ACTIVATED& activationData);
  void discoverTechnologies(tNFA_DISC_RESULT& discoveryData);

  void createNativeNfcTag(tNFA_ACTIVATED& activationData);

  void fillNativeNfcTagMembers1(INfcTag* pINfcTag);
  void fillNativeNfcTagMembers2(INfcTag* pINfcTag);
  void fillNativeNfcTagMembers3(INfcTag* pINfcTag, tNFA_ACTIVATED& activationData);
  void fillNativeNfcTagMembers4(INfcTag* pINfcTag, tNFA_ACTIVATED& activationData);
  void fillNativeNfcTagMembers5(INfcTag* pINfcTag, tNFA_ACTIVATED& activationData);

  void resetTechnologies();

  void calculateT1tMaxMessageSize(tNFA_ACTIVATED& activate);

  TagTechnology toGenericTagTechnology(uint32_t tech);
};

