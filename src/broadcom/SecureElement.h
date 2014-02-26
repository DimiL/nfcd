/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <vector>
#include <string>
#include "SyncEvent.h"
#include "RouteDataSet.h"

extern "C"
{
  #include "nfa_ee_api.h"
  #include "nfa_hci_api.h"
  #include "nfa_hci_defs.h"
  #include "nfa_ce_api.h"
}

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
   * @return True if ok.
   */
  bool initialize();

  /**
   * Release all resources.
   *
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
   * @param  seID ID of secure element.
   * @return True if ok.
   */
  bool activate(uint32_t seID);

  /**
   *  Turn off the secure element.
   *
   * @param  seID ID of secure element.
   * @return
   */
  bool deactivate(uint32_t seID);

  /**
   * Connect to the execution environment.
   *
   * @return True if ok.
   */
  bool connectEE();

  /**
   * Disconnect from the execution environment.
   *
   * @param  seID ID of secure element.
   * @return True if ok.
   */
  bool disconnectEE(uint32_t seID);

  /**
   * Send data to the secure element; read it's response.
   *
   * @return True if ok.
   */
  bool transceive(UINT8* xmitBuffer, INT32 xmitBufferSize, UINT8* recvBuffer,
        INT32 recvBufferMaxSize, INT32& recvBufferActualSize, INT32 timeoutMillisec);

  /**
   * Notify the NFC service about whether the SE was activated
   * in listen mode
   *
   * @return None.
   */
  void notifyListenModeState(bool isActivated);;

  /**
   * Notify the NFC service about RF field events from the stack.
   *
   * @return None.
   */
  void notifyRfFieldEvent(bool isActive);

  /**
   * Resets the field status.
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
   * Get the ID of the secure element.
   *
   * @param  eeHandle Handle to the secure element.
   * @param  uid Array to receive the ID.
   * @return True if ok.
   */
  bool getUiccId(tNFA_HANDLE eeHandle, std::vector<uint8_t>& uid);

  /**
   * Get all the technologies supported by a secure element.
   *
   * @param  eeHandle Handle to the secure element.
   * @param  techList List to receive the technologies.
   * @return True if ok.
   */
  bool getTechnologyList(tNFA_HANDLE eeHandle, std::vector<uint32_t>& techList);

  /**
   * Notify the NFC service about a transaction event from secure element.
   *
   * @param  aid Buffer contains application ID.
   * @param  aidLen Length of application ID.
   * @return None.
   */
  void notifyTransactionListenersOfAid (const uint8_t* aid, uint8_t aidLen);

  /**
   * Notify the NFC service about a transaction event from secure element.
   * The type-length-value contains AID and parameter.
   *
   * @param  tlv type-length-value encoded in Basic Encoding Rule.
   * @param  tlvLen Length tlv.
   * @return None.
   */
  void notifyTransactionListenersOfTlv (const uint8_t* tlv, uint8_t tlvLen);

  /**
   * Receive card-emulation related events from stack.
   *
   * @param  event Event code.
   * @param  eventData Event data.
   * @return None.
   */
  void connectionEventHandler(uint8_t event, tNFA_CONN_EVT_DATA* eventData);

  /**
   * Read route data from XML and apply them again
   * to every secure element.
   *
   * @return None.
   */
  void applyRoutes();

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
   * Returns number of secure elements we know about.
   *
   * @return Number of secure elements we know about/
   */
  uint8_t getActualNumEe();

  /**
   * Gets version information and id for a secure element.  The
   * 
   * @param  seIndex zero based index of the secure element to get verion info for.
   * @param  verInfo version infommation.
   * @param  verInfoSz
   * @param  seid
   * @return ture on success, false on failure.
   */
  bool getSeVerInfo(int seIndex, char * verInfo, int verInfoSz, UINT8 * seid);

  /**
   * Can be used to determine if the SE is activated in listen mode.
   *
   * @return True if the SE is activated in listen mode.
   */
  bool isActivatedInListenMode();

  /**
   * Can be used to determine if the SE is in an RF field.
   *
   * @return True if the SE is activated in an RF field.
   */
  bool isRfFieldOn();

private:
  static const unsigned int MAX_RESPONSE_SIZE = 1024;
  enum RouteSelection {NoRoute, DefaultRoute, SecElemRoute};
  static const int MAX_NUM_EE = 5;    //max number of EE's
  static const uint8_t STATIC_PIPE_0x70 = 0x70; //Broadcom's proprietary static pipe
  static const uint8_t STATIC_PIPE_0x71 = 0x71; //Broadcom's proprietary static pipe
  static const uint8_t EVT_SEND_DATA = 0x10;    //see specification ETSI TS 102 622 v9.0.0 (Host Controller Interface); section 9.3.3.3
  static const tNFA_HANDLE EE_HANDLE_0xF3 = 0x4F3; //handle to secure element in slot 0
  static const tNFA_HANDLE EE_HANDLE_0xF4 = 0x4F4; //handle to secure element in slot 1
  static SecureElement sSecElem; 
  static const char* APP_NAME;

  uint8_t mDestinationGate;       //destination gate of the UICC
  tNFA_HANDLE mNfaHciHandle;          //NFA handle to NFA's HCI component
  bool mIsInit;                // whether EE is initialized
  uint8_t mActualNumEe;           // actual number of EE's reported by the stack
  uint8_t mNumEePresent;          // actual number of usable EE's
  bool mbNewEE;
  uint8_t mNewPipeId;
  uint8_t mNewSourceGate;
  uint16_t mActiveSeOverride;      // active "enable" seid, 0 means activate all SEs
  tNFA_STATUS mCommandStatus;     //completion status of the last command
  bool mIsPiping;              //is a pipe connected to the controller?
  RouteSelection mCurrentRouteSelection;
  int mActualResponseSize;         //number of bytes in the response received from secure element
  bool mUseOberthurWarmReset;  //whether to use warm-reset command
  bool mActivatedInListenMode; // whether we're activated in listen mode
  uint8_t mOberthurWarmResetCommand; //warm-reset command byte
  tNFA_EE_INFO mEeInfo [MAX_NUM_EE];  //actual size stored in mActualNumEe
  tNFA_EE_DISCOVER_REQ mUiccInfo;
  tNFA_HCI_GET_GATE_PIPE_LIST mHciCfg;
  SyncEvent mEeRegisterEvent;
  SyncEvent mHciRegisterEvent;
  SyncEvent mEeSetModeEvent;
  SyncEvent mPipeListEvent;
  SyncEvent mCreatePipeEvent;
  SyncEvent mPipeOpenedEvent;
  SyncEvent mAllocateGateEvent;
  SyncEvent mDeallocateGateEvent;
  SyncEvent mRoutingEvent;
  SyncEvent mUiccInfoEvent;
  SyncEvent mUiccListenEvent;
  SyncEvent mAidAddRemoveEvent;
  SyncEvent mTransceiveEvent;
  SyncEvent mVerInfoEvent;
  SyncEvent mRegistryEvent;
  uint8_t mVerInfo [3];
  uint8_t mResponseData [MAX_RESPONSE_SIZE];
  RouteDataSet mRouteDataSet; //routing data
  std::vector<std::string> mUsedAids; //AID's that are used in current routes
  uint8_t mAidForEmptySelect[NCI_MAX_AID_LEN+1];
  Mutex mMutex; // protects fields below
  bool mRfFieldIsOn; // last known RF field state
  struct timespec mLastRfFieldToggle; // last time RF field went off

  SecureElement();
  ~SecureElement();

  /**
   * Receive execution environment-related events from stack.
   *
   * @param event Event code.
   * @param eventData Event data.
   * return None.
   */
  static void nfaEeCallback (tNFA_EE_EVT event, tNFA_EE_CBACK_DATA* eventData);

  /**
   * Receive Host Controller Interface-related events from stack.
   *
   * @param event Event code.
   * @param eventData Event data.
   * return None.
   */
  static void nfaHciCallback (tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* eventData);

  /**
   * Find information about an execution environment.
   *
   * @param  eeHandle Handle to execution environment.
   * @return Information about an execution environment.
   */
  tNFA_EE_INFO* findEeByHandle(tNFA_HANDLE eeHandle);

  /**
   * Find information about an execution environment.
   *
   * @param  eeHandle Handle to execution environment.
   * @return Information about the execution environment.
   */
  tNFA_EE_DISCOVER_INFO *findUiccByHandle (tNFA_HANDLE eeHandle);

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
  void adjustProtocolRoutes(RouteDataSet::Database* db, RouteSelection routeSelection);

  /**
   * Adjust default routing based on technology in NFC listen mode.
   *
   * @param  isRouteToEe Whether routing to EE (true) or host (false).
   * @return None.
   */
  void adjustTechnologyRoutes(RouteDataSet::Database* db, RouteSelection routeSelection);

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
  static const char* eeStatusToString (uint8_t status);

  /**
   * Encode AID in type-length-value using Basic Encoding Rule.
   *
   * @param tlv Buffer to store TLV.
   * @param tlvMaxLen TLV buffer's maximum length.
   * @param tlvActualLen TLV buffer's actual length.
   * @param aid Buffer of Application ID.
   * @param aidLen Aid buffer's actual length.
   * @return True if ok.
   */
  bool encodeAid(uint8_t* tlv, uint16_t tlvMaxLen, uint16_t& tlvActualLen, const uint8_t* aid, uint8_t aidLen);
};
