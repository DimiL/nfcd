/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "SyncEvent.h"
#include "NfcUtil.h"
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
  enum ActivationState {Idle, Sleep, Active};
  static const int MAX_NUM_TECHNOLOGY = 10;  // Max number of technologies supported by one or more tags.
  int mTechList [MAX_NUM_TECHNOLOGY]; 		 // Array of NFC technologies according to NFC service.
  int mTechHandles [MAX_NUM_TECHNOLOGY]; 	 // Array of tag handles according to NFC service.
  int mTechLibNfcTypes [MAX_NUM_TECHNOLOGY]; // Array of detailed tag types according to NFC service.
  int mNumTechList; 						 // Current number of NFC technologies in the list.

  NfcTag ();

  /**
   * Get a reference to the singleton NfcTag object.
   *
   * @return Reference to NfcTag object.
   */
  static NfcTag& getInstance ();

  /**
   * Reset member variables.
   *
   * @param pNfcManager NFC manager class instance.
   * @return            None.
   */
  void initialize (NfcManager* pNfcManager);

  /**
   * Unblock all operations.
   *
   * @return None
   */
  void abort ();

  /**
   * Handle connection-related events.
   *
   * @param  event Event code.
   * @param  data  Pointer to event data.
   * @return       Mpme
   */
  void connectionEventHandler (UINT8 event, tNFA_CONN_EVT_DATA* data);

  /**
   * What is the current state: Idle, Sleep, or Activated.
   *
   * @return Idle, Sleep, or Activated.
   */
  ActivationState getActivationState ();

  /**
   * Set the current state: Idle or Sleep.
   *
   * @param  deactivated state of deactivation.
   * @return             None.
   */
  void setDeactivationState (tNFA_DEACTIVATED& deactivated);

  /**
   * Set the current state to Active.
   *
   * @return None
   */
  void setActivationState ();

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
  ActivationState mActivationState;
  tNFC_PROTOCOL mProtocol;
  int mtT1tMaxMessageSize; 					// T1T max NDEF message size.
  tNFA_STATUS mReadCompletedStatus;
  int mLastKovioUidLen;   					// Len of uid of last Kovio tag activated.
  bool mNdefDetectionTimedOut; 				// Whether NDEF detection algorithm timed out.
  SyncEvent mReadCompleteEvent;
  struct timespec mLastKovioTime; 			// Time of last Kovio tag activation.
  UINT8 mLastKovioUid[NFC_KOVIO_MAX_LEN]; 	// uid of last Kovio tag activated.
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

  /**
   * Convert vendor specefic tag technology define value to mozilla define value
   *
   * @param  tech Vendor specefic tag technology.
   * @return      Mozilla defined tag technology.
   */
  TagTechnology toGenericTagTechnology(uint32_t tech);
};
