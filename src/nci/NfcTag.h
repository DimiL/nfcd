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

#pragma once

#include <vector>
#include "SyncEvent.h"
#include "NfcNciUtil.h"
#include "TagTechnology.h"

extern "C"
{
  #include "nfa_rw_api.h"
}

class NfcManager;
class INfcTag;

/**
 *  Tag-reading, tag-writing operations.
 */
class NfcTag
{
public:
  NfcTag();

  enum ActivationState {Idle, Sleep, Active};

  // Max number of technologies supported by one or more tags.
  static const int MAX_NUM_TECHNOLOGY = 10;

  TechnologyType mTechList [MAX_NUM_TECHNOLOGY];  // Array of NFC technologies.
  int mTechHandles [MAX_NUM_TECHNOLOGY];          // Array of tag handles.
  int mTechLibNfcTypes [MAX_NUM_TECHNOLOGY];      // Array of detailed tag types.
  int mNumTechList;                               // Number of NFC technologies in the list.

  /**
   * Get a reference to the singleton NfcTag object.
   *
   * @return Reference to NfcTag object.
   */
  static NfcTag& GetInstance();

  /**
   * Reset member variables.
   *
   * @param  aNfcManager NFC manager class instance.
   * @return             None.
   */
  void Initialize(NfcManager* aNfcManager);

  /**
   * Unblock all operations.
   *
   * @return None
   */
  void Abort();

  /**
   * Handle connection-related events.
   *
   * @param  aEvent Event code.
   * @param  aData  Pointer to event data.
   * @return       None
   */
  void ConnectionEventHandler(UINT8 aEvent,
                              tNFA_CONN_EVT_DATA* aData);

  /**
   * Reset all timeouts for all technologies to default values.
   *
   * @return None
   */
  void ResetAllTransceiveTimeouts();

  /**
   * Get the timeout value for one technology.
   *
   * @param  aTechId One of the values in TECHNOLOGY_TYPE_* defined in NfcNciUtil.h.
   * @return         Timeout value in millisecond.
   */
  int GetTransceiveTimeout(int aTechId);

  /**
   * What is the current state: Idle, Sleep, or Activated.
   *
   * @return Idle, Sleep, or Activated.
   */
  ActivationState GetActivationState();

  /**
   * Set the current state: Idle or Sleep.
   *
   * @param  aDeactivated state of deactivation.
   * @return              None.
   */
  void SetDeactivationState(tNFA_DEACTIVATED& aDeactivated);

  /**
   * Set the current state to Active.
   *
   * @return None
   */
  void SetActivationState();

  /**
   * Get the protocol of the current tag.
   *
   * @return Protocol number.
   */
  tNFC_PROTOCOL GetProtocol();

  /**
   * Does the peer support P2P?
   *
   * @return True if the peer supports P2P.
   */
  bool IsP2pDiscovered();

  /**
   * Select the preferred P2P technology if there is a choice.
   *
   * @return None.
   */
  void SelectP2p();

  /**
   * When multiple tags are discovered, just select the first one to activate.
   *
   * @return None.
   */
  void SelectFirstTag();

  /**
   * Get the maximum size (octet) that a T1T can store.
   *
   * @return Maximum size in octets.
   */
  int GetT1tMaxMessageSize();

  /**
   * Whether the currently activated tag is Mifare Ultralight.
   *
   * @return True if tag is Mifare Ultralight.
   */
  bool IsMifareUltralight();

  /**
   * Whether the response is a T2T NACK response.
   * See NFC Digital Protocol Technical Specification (2010-11-17).
   * Chapter 9 (Type 2 Tag Platform), section 9.6 (READ).
   *
   * @param  aResponse    Buffer contains T2T response.
   * @param  aResponseLen Length of the response.
   * @return              True if the response is NACK
   */
  bool IsT2tNackResponse(const UINT8* aResponse,
                         UINT32 aResponseLen);

  /**
   * Whether NDEF-detection algorithm has timed out.
   *
   * @return True if NDEF-detection algorithm timed out.
   */
  bool IsNdefDetectionTimedOut();

private:
  std::vector<int> mTimeoutTable;
  ActivationState mActivationState;
  tNFC_PROTOCOL mProtocol;
  int mtT1tMaxMessageSize;                  // T1T max NDEF message size.
  tNFA_STATUS mReadCompletedStatus;
  int mLastKovioUidLen;                     // Len of uid of last Kovio tag activated.
  bool mNdefDetectionTimedOut;              // Whether NDEF detection algorithm timed out.
  SyncEvent mReadCompleteEvent;
  struct timespec mLastKovioTime;           // Time of last Kovio tag activation.
  UINT8 mLastKovioUid[NFC_KOVIO_MAX_LEN];   // uid of last Kovio tag activated.
  tNFC_RF_TECH_PARAMS mTechParams [MAX_NUM_TECHNOLOGY]; // Array of technology parameters.

  NfcManager*     mNfcManager;

  /**
   * Checks if tag activate is the same (UID) Kovio tag previously
   * activated. This is needed due to a problem with some Kovio
   * tags re-activating multiple times.
   *
   * @param  aActivationData Data from activation.
   * @return                 True if the activation is from the same tag previously
   *                         activated, false otherwise.
   */
  bool IsSameKovio(tNFA_ACTIVATED& aActivationData);

  /**
   * Discover the technologies that NFC service needs by interpreting
   * the data strucutures from the stack.
   *
   * @param  aActivationData Data from activation.
   * @return                 None.
   */
  void DiscoverTechnologies(tNFA_ACTIVATED& aActivationData);

  /**
   * Discover the technologies that NFC service needs by interpreting
   * the data strucutures from the stack.
   *
   * @param  aDiscoveryData Data from discovery events(s).
   * @return                None.
   */
  void DiscoverTechnologies(tNFA_DISC_RESULT& aDiscoveryData);

  /**
   * Create a brand new Java NativeNfcTag object;
   * fill the objects's member variables with data;
   * notify NFC service;
   *
   * @param  aActivationData Data from activation.
   * @return                 None.
   */
  void CreateNfcTag(tNFA_ACTIVATED& aActivationData);

  /**
   * Fill NativeNfcTag's members: mProtocols, mTechList, mTechHandles, mTechLibNfcTypes.
   *
   * @param  aINfcTag NFC tag interface.
   * @return          None.
   */
  void FillNfcTagMembers1(INfcTag* aINfcTag);

  /**
   * Fill NativeNfcTag's members: mConnectedTechIndex or mConnectedTechnology.
   *
   * @param  aINfcTag NFC tag interface.
   * @return          None.
   */
  void FillNfcTagMembers2(INfcTag* aINfcTag);

  /**
   * Fill NativeNfcTag's members: mTechPollBytes.
   *
   * @param  aINfcTag        NFC tag interface.
   * @param  aActivationData Data from activation.
   * @return                 None.
   */
  void FillNfcTagMembers3(INfcTag* aINfcTag,
                          tNFA_ACTIVATED& aActivationData);

  /**
   * Fill NativeNfcTag's members: mTechActBytes.
   *
   * @param  aINfcTag        NFC tag interface.
   * @param  aActivationData Data from activation.
   * @return                 None.
   */
  void FillNfcTagMembers4(INfcTag* aINfcTag,
                          tNFA_ACTIVATED& aActivationData);

  /**
   * Fill NativeNfcTag's members: mUid.
   *
   * @param  aINfcTag        NFC tag interface.
   * @param  aActivationData Data from activation.
   * @return                 None.
   */
  void FillNfcTagMembers5(INfcTag* aINfcTag,
                          tNFA_ACTIVATED& aActivationData);

  /**
   * Clear all data related to the technology, protocol of the tag.
   *
   * @return None.
   */
  void ResetTechnologies();

  /**
   * Calculate type-1 tag's max message size based on header ROM bytes.
   *
   * @param  aActivate Reference to activation data.
   * @return           None.
   */
  void CalculateT1tMaxMessageSize(tNFA_ACTIVATED& aActivate);
};
