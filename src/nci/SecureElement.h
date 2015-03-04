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
#include <string>
#include "SyncEvent.h"
#include "RouteDataSet.h"
#include "ICardEmulation.h"

extern "C"
{
  #include "nfa_ee_api.h"
  #include "nfa_hci_api.h"
  #include "nfa_hci_defs.h"
  #include "nfa_ce_api.h"
}

class NfcManager;

class SecureElement :
  public ICardEmulation
{
public:
  tNFA_HANDLE mActiveEeHandle;

  /**
   * Get the SecureElement singleton object.
   *
   * @return SecureElement object.
   */
  static SecureElement& GetInstance();

  /**
   * Initialize all member variables.
   *
   * @param  aNfcManager NFC manager class instance.
   * @return True if ok.
   */
  bool Initialize(NfcManager* aNfcManager);

  /**
   * Release all resources.
   *
   * @return None.
   */
  void Finalize();

  /**
   * Get the list of handles of all execution environments.
   *
   * @return List of handles of all execution environments.
   */
  void GetListOfEeHandles(std::vector<uint32_t>& aListSe);

  /**
   * Turn on the secure element.
   *
   * @return True if ok.
   */
  bool Activate();

  /**
   * Turn off the secure element.
   *
   * @return True if ok.
   */
  bool Deactivate();

  /**
   * Resets the RF field status.
   *
   * @return None.
   */
  void ResetRfFieldStatus();

  /**
   * Notify the NFC service about whether the SE was activated
   * in listen mode
   *
   * @return None.
   */
  void NotifyListenModeState(bool isActivated);

  /**
   * Notify the NFC service about RF field events from the stack.
   *
   * @return None.
   */
  void NotifyRfFieldEvent(bool isActive);

  /**
   * Notify the NFC service about a transaction event from secure element.
   *
   * @param  aAid Buffer contains AID.
   * @param  aAidLen Length of AID.
   * @param  aPayload Buffer contains payload.
   * @param  aPayloadLen Length of payload.
   * @return None.
   */
  void NotifyTransactionEvent(const uint8_t* aAid,
                              uint32_t aAidLen,
                              const uint8_t* aPayload,
                              uint32_t aPayloadLen);

  /**
   * Receive card-emulation related events from stack.
   *
   * @param  aEvent Event code.
   * @param  aEventData Event data.
   * @return None.
   */
  void ConnectionEventHandler(uint8_t aEvent,
                              tNFA_CONN_EVT_DATA* aEventData);

  /**
   * Specify which secure element to turn on.
   *
   * @param  aActiveSeOverride ID of secure element.
   * @return None.
   */
  void SetActiveSeOverride(uint8_t aActiveSeOverride);

  /**
   * Adjust controller's listen-mode routing table so transactions
   * are routed to the secure elements as specified in route.xml.
   *
   * @return True if ok.
   */
  bool RouteToSecureElement();

  // TODO : route.xml ???
  /**
   * Adjust controller's listen-mode routing table so transactions
   * are routed to the default destination specified in route.xml.
   *
   * @return True if ok.
   */
  bool RouteToDefault();

  /**
   * Can be used to determine if the SE is activated in listen mode.
   *
   * @return True if the SE is activated in listen mode.
   */
  bool IsActivatedInListenMode();

  /**
   * Can be used to determine if the SE is activated in an RF field.
   *
   * @return True if the SE is activated in an RF field.
   */
  bool IsRfFieldOn();

  void NotifyModeSet(tNFA_HANDLE aEeHandle, bool aSuccess);

  tNFA_HANDLE GetDefaultEeHandle();

  bool SendApdu(const std::vector<uint8_t>& aApdu);
private:
  static const unsigned int MAX_RESPONSE_SIZE = 1024;
  enum RouteSelection {NoRoute, DefaultRoute, SecElemRoute};
  static const int MAX_NUM_EE = 5;  // max number of EE's

  //see specification ETSI TS 102 622 v9.0.0 (Host Controller Interface); section 9.3.3.3
  static const uint8_t EVT_SEND_DATA = 0x10;
  static const tNFA_HANDLE EE_HANDLE_0xF3 = 0x4F3;  //handle to secure element in slot 0
  static const tNFA_HANDLE EE_HANDLE_0xF4 = 0x4F4;  //handle to secure element in slot 1
  // TODO: 0x01 & 0x02 is for Flame, we should use a more general way to specify this.
  static const tNFA_HANDLE EE_HANDLE_0x01 = 0x401;  //handle to secure element in slot 0
  static const tNFA_HANDLE EE_HANDLE_0x02 = 0x402;  //handle to secure element in slot 1
  static SecureElement sSecElem;
  static const char* APP_NAME;

  NfcManager* mNfcManager;
  tNFA_HANDLE mNfaHciHandle;  //NFA handle to NFA's HCI component
  bool mIsInit;  // whether EE is initialized
  uint8_t mActualNumEe;  // actual number of EE's reported by the stack
  uint8_t mNumEePresent;  // actual number of usable EE's
  uint16_t mActiveSeOverride;  // active "enable" seid, 0 means activate all SEs
  bool mIsPiping;  //is a pipe connected to the controller?
  RouteSelection mCurrentRouteSelection;
  int mActualResponseSize;  //number of bytes in the response received from secure element
  bool mActivatedInListenMode; // whether we're activated in listen mode
  tNFA_EE_INFO mEeInfo[MAX_NUM_EE];  //actual size stored in mActualNumEe
  tNFA_EE_DISCOVER_REQ mUiccInfo;
  SyncEvent mHciRegisterEvent;
  SyncEvent mEeSetModeEvent;
  SyncEvent mUiccListenEvent;
  Mutex mMutex;  // protects fields below
  bool mRfFieldIsOn;  // last known RF field state
  struct timespec mLastRfFieldToggle;  // last time RF field went off

  SecureElement();
  virtual ~SecureElement();

  /**
   * Receive Host Controller Interface-related events from stack.
   *
   * @param aEvent Event code.
   * @param aEventData Event data.
   * return None.
   */
  static void NfaHciCallback(tNFA_HCI_EVT aEvent,
                             tNFA_HCI_EVT_DATA* aEventData);

  /**
   * Find information about an execution environment.
   *
   * @param  aEeHandle Handle to execution environment.
   * @return Information about an execution environment.
   */
  tNFA_EE_INFO* FindEeByHandle(tNFA_HANDLE aEeHandle);

  /**
   * Get latest information about execution environments from stack.
   *
   * @return True if at least 1 EE is available.
   */
  bool GetEeInfo();

  /**
   * Convert status code to status text.
   *
   * @param  status Status code
   * @return None
   */
  static const char* EeStatusToString(uint8_t aStatus);
};
