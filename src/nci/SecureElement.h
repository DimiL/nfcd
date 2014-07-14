/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <vector>
#include <string>
#include "SyncEvent.h"

extern "C"
{
  #include "nfa_ee_api.h"
  #include "nfa_hci_api.h"
  #include "nfa_hci_defs.h"
  #include "nfa_ce_api.h"
}

class NfcManager;

class SecureElement
{
public:
  tNFA_HANDLE mActiveEeHandle;

  /**
   * Get the SecureElement singleton object.
   *
   * @return SecureElement object.
   */
  static SecureElement& getInstance();

  /**
   * Initialize all member variables.
   *
   * @param pNfcManager NFC manager class instance.
   * @return True if ok.
   */
  bool initialize(NfcManager* pNfcManager);

  /**
   * Release all resources.
   *
   * @return None.
   */
  void finalize();

  /**
   * Get the list of handles of all execution environments.
   *
   * @return List of handles of all execution environments.
   */
  void getListOfEeHandles(std::vector<uint32_t>& listSe);

  /**
   * Turn on the secure element.
   *
   * @return True if ok.
   */
  bool activate();

  /**
   * Turn off the secure element.
   *
   * @return True if ok.
   */
  bool deactivate();

  /**
   * Resets the RF field status.
   *
   * @return None.
   */
  void resetRfFieldStatus();

  /**
   * Store a copy of the execution environment information from the stack.
   *
   * @param  info execution environment information.
   * @return None.
   */
  void storeUiccInfo(tNFA_EE_DISCOVER_REQ& info);

  /**
   * Notify the NFC service about whether the SE was activated
   * in listen mode
   *
   * @return None.
   */
  void notifyListenModeState(bool isActivated);

  /**
   * Notify the NFC service about RF field events from the stack.
   *
   * @return None.
   */
  void notifyRfFieldEvent(bool isActive);

  /**
   * Notify the NFC service about a transaction event from secure element.
   *
   * @param  aid Buffer contains AID.
   * @param  aidLen Length of AID.
   * @param  payload Buffer contains payload.
   * @param  payloadLen Length of payload.
   * @return None.
   */
  void notifyTransactionEvent(const uint8_t* aid, uint32_t aidLen,
                              const uint8_t* payload, uint32_t payloadLen);

  /**
   * Receive card-emulation related events from stack.
   *
   * @param  event Event code.
   * @param  eventData Event data.
   * @return None.
   */
  void connectionEventHandler(uint8_t event, tNFA_CONN_EVT_DATA* eventData);

  /**
   * Specify which secure element to turn on.
   *
   * @param  activeSeOverride ID of secure element.
   * @return None.
   */
  void setActiveSeOverride(uint8_t activeSeOverride);

  /**
   * Adjust controller's listen-mode routing table so transactions
   * are routed to the secure elements as specified in route.xml.
   *
   * @return True if ok.
   */
  bool routeToSecureElement();

  // TODO : route.xml ???
  /**
   * Adjust controller's listen-mode routing table so transactions
   * are routed to the default destination specified in route.xml.
   *
   * @return True if ok.
   */
  bool routeToDefault();

  /**
   * Whether NFC controller is routing listen-mode events or a pipe is connected.
   *
   * @return True if either case is true.
   */
  bool isBusy();

  /**
   * Can be used to determine if the SE is activated in listen mode.
   *
   * @return True if the SE is activated in listen mode.
   */
  bool isActivatedInListenMode();

  /**
   * Can be used to determine if the SE is activated in an RF field.
   *
   * @return True if the SE is activated in an RF field.
   */
  bool isRfFieldOn();

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
  bool mbNewEE;
  uint16_t mActiveSeOverride;  // active "enable" seid, 0 means activate all SEs
  bool mIsPiping;  //is a pipe connected to the controller?
  RouteSelection mCurrentRouteSelection;
  int mActualResponseSize;  //number of bytes in the response received from secure element
  bool mActivatedInListenMode; // whether we're activated in listen mode
  tNFA_EE_INFO mEeInfo[MAX_NUM_EE];  //actual size stored in mActualNumEe
  tNFA_EE_DISCOVER_REQ mUiccInfo;
  SyncEvent mEeRegisterEvent;
  SyncEvent mHciRegisterEvent;
  SyncEvent mEeSetModeEvent;
  SyncEvent mRoutingEvent;
  SyncEvent mUiccInfoEvent;
  SyncEvent mUiccListenEvent;
  SyncEvent mAidAddRemoveEvent;
  Mutex mMutex;  // protects fields below
  bool mRfFieldIsOn;  // last known RF field state
  struct timespec mLastRfFieldToggle;  // last time RF field went off

  SecureElement();
  ~SecureElement();

  /**
   * Receive execution environment-related events from stack.
   *
   * @param event Event code.
   * @param eventData Event data.
   * return None.
   */
  static void nfaEeCallback(tNFA_EE_EVT event, tNFA_EE_CBACK_DATA* eventData);

  /**
   * Receive Host Controller Interface-related events from stack.
   *
   * @param event Event code.
   * @param eventData Event data.
   * return None.
   */
  static void nfaHciCallback(tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* eventData);

  /**
   * Find information about an execution environment.
   *
   * @param  eeHandle Handle to execution environment.
   * @return Information about an execution environment.
   */
  tNFA_EE_INFO* findEeByHandle(tNFA_HANDLE eeHandle);

  /**
   * Get the handle to the execution environment.
   *
   * @return Handle to the execution environment.
   */
  tNFA_HANDLE getDefaultEeHandle();

  /**
   * Adjust routes in the controller's listen-mode routing table.
   *
   * @param  selection which set of routes to configure the controller.
   * @return None.
   */
  void adjustRoutes(RouteSelection selection);

  /**
   * Adjust default routing based on protocol in NFC listen mode.
   *
   * @param  isRouteToEe Whether routing to EE (true) or host (false).
   * @return None.
   */
  void adjustProtocolRoutes(RouteSelection routeSelection);

  /**
   * Adjust default routing based on technology in NFC listen mode.
   *
   * @param  isRouteToEe Whether routing to EE (true) or host (false).
   * @return None.
   */
  void adjustTechnologyRoutes(RouteSelection routeSelection);

  /**
   * Get latest information about execution environments from stack.
   *
   * @return True if at least 1 EE is available.
   */
  bool getEeInfo();

  /**
   * Convert status code to status text.
   *
   * @param  status Status code
   * @return None
   */
  static const char* eeStatusToString(uint8_t status);
};
