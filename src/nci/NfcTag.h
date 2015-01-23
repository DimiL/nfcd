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
  static NfcTag& getInstance();

  /**
   * Reset member variables.
   *
   * @param pNfcManager NFC manager class instance.
   * @return            None.
   */
  void initialize(NfcManager* pNfcManager);

  /**
   * Unblock all operations.
   *
   * @return None
   */
  void abort();

  /**
   * Handle connection-related events.
   *
   * @param  event Event code.
   * @param  data  Pointer to event data.
   * @return       None
   */
  void connectionEventHandler(UINT8 event, tNFA_CONN_EVT_DATA* data);

  /**
   * Reset all timeouts for all technologies to default values.
   *
   * @return None
   */
  void resetAllTransceiveTimeouts();

  /**
   * Get the timeout value for one technology.
   *
   * @param  techId One of the values in TECHNOLOGY_TYPE_* defined in NfcNciUtil.h.
   * @return        Timeout value in millisecond.
   */
  int getTransceiveTimeout(int techId);

  /**
   * What is the current state: Idle, Sleep, or Activated.
   *
   * @return Idle, Sleep, or Activated.
   */
  ActivationState getActivationState();

  /**
   * Set the current state: Idle or Sleep.
   *
   * @param  deactivated state of deactivation.
   * @return             None.
   */
  void setDeactivationState(tNFA_DEACTIVATED& deactivated);

  /**
   * Set the current state to Active.
   *
   * @return None
   */
  void setActivationState();

  /**
   * Get the protocol of the current tag.
   *
   * @return Protocol number.
   */
  tNFC_PROTOCOL getProtocol();

  /**
   * Does the peer support P2P?
   *
   * @return True if the peer supports P2P.
   */
  bool isP2pDiscovered();

  /**
   * Select the preferred P2P technology if there is a choice.
   *
   * @return None.
   */
  void selectP2p();

  /**
   * When multiple tags are discovered, just select the first one to activate.
   *
   * @return None.
   */
  void selectFirstTag();

  /**
   * Get the maximum size (octet) that a T1T can store.
   *
   * @return Maximum size in octets.
   */
  int getT1tMaxMessageSize();

  /**
   * Whether the currently activated tag is Mifare Ultralight.
   *
   * @return True if tag is Mifare Ultralight.
   */
  bool isMifareUltralight();

  /**
   * Whether the response is a T2T NACK response.
   * See NFC Digital Protocol Technical Specification (2010-11-17).
   * Chapter 9 (Type 2 Tag Platform), section 9.6 (READ).
   *
   * @param  response    Buffer contains T2T response.
   * @param  responseLen Length of the response.
   * @return             True if the response is NACK
   */
  bool isT2tNackResponse(const UINT8* response, UINT32 responseLen);

  /**
   * Whether NDEF-detection algorithm has timed out.
   *
   * @return True if NDEF-detection algorithm timed out.
   */
  bool isNdefDetectionTimedOut();

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
   * @param  activationData Data from activation.
   * @return                True if the activation is from the same tag previously
   *                        activated, false otherwise.
   */
  bool IsSameKovio(tNFA_ACTIVATED& activationData);

  /**
   * Discover the technologies that NFC service needs by interpreting
   * the data strucutures from the stack.
   *
   * @param  activationData Data from activation.
   * @return                None.
   */
  void discoverTechnologies(tNFA_ACTIVATED& activationData);

  /**
   * Discover the technologies that NFC service needs by interpreting
   * the data strucutures from the stack.
   *
   * @param  discoveryData Data from discovery events(s).
   * @return               None.
   */
  void discoverTechnologies(tNFA_DISC_RESULT& discoveryData);

  /**
   * Create a brand new Java NativeNfcTag object;
   * fill the objects's member variables with data;
   * notify NFC service;
   *
   * @param  activationData Data from activation.
   * @return                None.
   */
  void createNfcTag(tNFA_ACTIVATED& activationData);

  /**
   * Fill NativeNfcTag's members: mProtocols, mTechList, mTechHandles, mTechLibNfcTypes.
   *
   * @param  pINfcTag NFC tag interface.
   * @return          None.
   */
  void fillNfcTagMembers1(INfcTag* pINfcTag);

  /**
   * Fill NativeNfcTag's members: mConnectedTechIndex or mConnectedTechnology.
   *
   * @param  pINfcTag NFC tag interface.
   * @return          None.
   */
  void fillNfcTagMembers2(INfcTag* pINfcTag);

  /**
   * Fill NativeNfcTag's members: mTechPollBytes.
   *
   * @param  pINfcTag       NFC tag interface.
   * @param  activationData Data from activation.
   * @return                None.
   */
  void fillNfcTagMembers3(INfcTag* pINfcTag, tNFA_ACTIVATED& activationData);

  /**
   * Fill NativeNfcTag's members: mTechActBytes.
   *
   * @param  pINfcTag       NFC tag interface.
   * @param  activationData Data from activation.
   * @return                None.
   */
  void fillNfcTagMembers4(INfcTag* pINfcTag, tNFA_ACTIVATED& activationData);

  /**
   * Fill NativeNfcTag's members: mUid.
   *
   * @param  pINfcTag       NFC tag interface.
   * @param  activationData Data from activation.
   * @return                None.
   */
  void fillNfcTagMembers5(INfcTag* pINfcTag, tNFA_ACTIVATED& activationData);

  /**
   * Clear all data related to the technology, protocol of the tag.
   *
   * @return None.
   */
  void resetTechnologies();

  /**
   * Calculate type-1 tag's max message size based on header ROM bytes.
   *
   * @param  activate Reference to activation data.
   * @return          None.
   */
  void calculateT1tMaxMessageSize(tNFA_ACTIVATED& activate);
};
