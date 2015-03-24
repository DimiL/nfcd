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

#include "NfcManager.h"

#include "OverrideLog.h"
#include "config.h"
#include "NfcAdaptation.h"
#include "NfcDebug.h"
#include "SyncEvent.h"
#include "SecureElement.h"
#include "PeerToPeer.h"
#include "PowerSwitch.h"
#include "NfcTag.h"
#include "Pn544Interop.h"
#include "LlcpSocket.h"
#include "LlcpServiceSocket.h"
#include "NfcTagManager.h"
#include "P2pDevice.h"

extern "C"
{
  #include "nfa_p2p_api.h"
  #include "rw_api.h"
  #include "nfa_ee_api.h"
  #include "nfc_brcm_defs.h"
  #include "ce_api.h"
#ifdef NFCC_PN547
  #include "phNxpExtns.h"
  #include "phNxpConfig.h"
#endif
}

extern bool gIsTagDeactivating;
extern bool gIsSelectingRfInterface;

/**
 * public variables and functions
 */
nfc_data                gNat;
int                     gGeneralTransceiveTimeout = 1000;
void                    DoStartupConfig();
void                    StartRfDiscovery(bool isStart);
bool                    StartStopPolling(bool isStartPolling);

/**
 * private variables and functions
 */
static SyncEvent            sNfaEnableEvent;                // Event for NFA_Enable().
static SyncEvent            sNfaDisableEvent;               // Event for NFA_Disable().
static SyncEvent            sNfaEnableDisablePollingEvent;  // Event for NFA_EnablePolling(), NFA_DisablePolling().
static SyncEvent            sNfaSetConfigEvent;             // Event for Set_Config....
static SyncEvent            sNfaGetConfigEvent;             // Event for Get_Config....

static bool                 sIsNfaEnabled = false;
static bool                 sDiscoveryEnabled = false;      // Is polling for tag?
static bool                 sIsDisabling = false;
static bool                 sRfEnabled = false;             // Whether RF discovery is enabled.
static bool                 sSeRfActive = false;            // Whether RF with SE is likely active.
static bool                 sP2pActive = false;             // Whether p2p was last active.
static bool                 sIsSecElemSelected = false;     // Has NFC service selected a sec elem.

#define CONFIG_UPDATE_TECH_MASK     (1 << 1)
#define DEFAULT_TECH_MASK           (NFA_TECHNOLOGY_MASK_A \
                                     | NFA_TECHNOLOGY_MASK_B \
                                     | NFA_TECHNOLOGY_MASK_F \
                                     | NFA_TECHNOLOGY_MASK_ISO15693 \
                                     | NFA_TECHNOLOGY_MASK_B_PRIME \
                                     | NFA_TECHNOLOGY_MASK_A_ACTIVE \
                                     | NFA_TECHNOLOGY_MASK_F_ACTIVE \
                                     | NFA_TECHNOLOGY_MASK_KOVIO)


static void NfaConnectionCallback(UINT8 event, tNFA_CONN_EVT_DATA *eventData);
static void NfaDeviceManagementCallback(UINT8 event, tNFA_DM_CBACK_DATA *eventData);
static bool IsPeerToPeer(tNFA_ACTIVATED& activated);
static bool IsListenMode(tNFA_ACTIVATED& activated);

static UINT16 sCurrentConfigLen;
static UINT8 sConfig[256];

NfcManager::NfcManager()
 : mP2pDevice(NULL)
 , mNfcTagManager(NULL)
{
  mP2pDevice = new P2pDevice();
  mNfcTagManager = new NfcTagManager();
}

NfcManager::~NfcManager()
{
  delete mP2pDevice;
  delete mNfcTagManager;
}

/**
 * Interfaces.
 */

void* NfcManager::QueryInterface(const char* aName)
{
  if (0 == strcmp(aName, INTERFACE_P2P_DEVICE)) {
    return reinterpret_cast<void*>(mP2pDevice);
  } else if (0 == strcmp(aName, INTERFACE_TAG_MANAGER)) {
    return reinterpret_cast<void*>(mNfcTagManager);
  }

  return NULL;
}

bool NfcManager::Initialize()
{
  tNFA_STATUS stat = NFA_STATUS_OK;
  unsigned long num = 5;

  // Initialize PowerSwitch.
  PowerSwitch::GetInstance().Initialize(PowerSwitch::FULL_POWER);

  // Start GKI, NCI task, NFC task.
  NfcAdaptation& theInstance = NfcAdaptation::GetInstance();
  theInstance.Initialize();

  {
    SyncEventGuard guard(sNfaEnableEvent);
    tHAL_NFC_ENTRY* halFuncEntries = theInstance.GetHalEntryFuncs();
    NFA_Init(halFuncEntries);

    stat = NFA_Enable(NfaDeviceManagementCallback, NfaConnectionCallback);
    if (stat == NFA_STATUS_OK) {
      num = initializeGlobalAppLogLevel();
      CE_SetTraceLevel(num);
      LLCP_SetTraceLevel(num);
      NFC_SetTraceLevel(num);
      RW_SetTraceLevel(num);
      NFA_SetTraceLevel(num);
      NFA_P2pSetTraceLevel(num);

      sNfaEnableEvent.Wait(); // Wait for NFA command to finish.
    } else {
      NCI_ERROR("NFA_Enable fail, error = 0x%X", stat);
    }
#ifdef NFCC_PN547
    EXTNS_Init(NfaDeviceManagementCallback, NfaConnectionCallback);
#endif
  }

  if (stat == NFA_STATUS_OK) {
    if (sIsNfaEnabled) {
      SecureElement::GetInstance().Initialize(this);
      NfcTagManager::DoRegisterNdefTypeHandler();
      NfcTag::GetInstance().Initialize(this);

      PeerToPeer::GetInstance().Initialize(this);
      PeerToPeer::GetInstance().HandleNfcOnOff(true);

      // Add extra configuration here (work-arounds, etc.).
      {
        if (GetNumValue(NAME_POLLING_TECH_MASK, &num, sizeof(num)))
          gNat.tech_mask = num;
        else
          gNat.tech_mask = DEFAULT_TECH_MASK;

        NCI_DEBUG("tag polling tech mask = 0x%X", gNat.tech_mask);
      }

      // If this value exists, set polling interval.
      if (GetNumValue(NAME_NFA_DM_DISC_DURATION_POLL, &num, sizeof(num)))
        NFA_SetRfDiscoveryDuration(num);

      // Do custom NFCA startup configuration.
      DoStartupConfig();
      goto TheEnd;
    }
  }

  if (sIsNfaEnabled) {
    stat = Disable(FALSE /* ungraceful */);
  }

  theInstance.Finalize();

TheEnd:
  if (sIsNfaEnabled) {
    PowerSwitch::GetInstance().SetLevel(PowerSwitch::LOW_POWER);
  }

  return sIsNfaEnabled;
}

bool NfcManager::Deinitialize()
{
  NCI_DEBUG("enter");

  sIsDisabling = true;
  Pn544InteropAbortNow();
  SecureElement::GetInstance().Finalize();

  if (sIsNfaEnabled) {
    SyncEventGuard guard(sNfaDisableEvent);

    tNFA_STATUS stat = Disable(TRUE /* graceful */);
    if (stat == NFA_STATUS_OK) {
      NCI_DEBUG("wait for completion");
      sNfaDisableEvent.Wait(); // Wait for NFA command to finish.
      PeerToPeer::GetInstance().HandleNfcOnOff(false);
    } else {
      NCI_ERROR("NFA_Disable fail; error = 0x%X", stat);
    }
  }

  NfcTagManager::DoAbortWaits();
  NfcTag::GetInstance().Abort();
  // TODO : Implement LLCP.
  sIsNfaEnabled = false;
  sDiscoveryEnabled = false;
  sIsDisabling = false;
  sIsSecElemSelected = false;

  {
    // Unblock NFA_EnablePolling() and NFA_DisablePolling().
    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    sNfaEnableDisablePollingEvent.NotifyOne();
  }

  NfcAdaptation& theInstance = NfcAdaptation::GetInstance();
  theInstance.Finalize();

  NCI_DEBUG("exit");
  return true;
}

bool NfcManager::EnableDiscovery()
{
  tNFA_TECHNOLOGY_MASK tech_mask = DEFAULT_TECH_MASK;

  tech_mask = (tNFA_TECHNOLOGY_MASK)gNat.tech_mask;

  if (sDiscoveryEnabled) {
    NCI_WARNING("already polling");
    return true;
  }

  tNFA_STATUS stat = NFA_STATUS_OK;

  PowerSwitch::GetInstance().SetLevel(PowerSwitch::FULL_POWER);

  if (sRfEnabled) {
    // Stop RF discovery to reconfigure.
    StartRfDiscovery(false);
  }

  {
    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    stat = NFA_EnablePolling(tech_mask);
    if (stat == NFA_STATUS_OK) {
      NCI_DEBUG("wait for enable event");
      sDiscoveryEnabled = true;
      sNfaEnableDisablePollingEvent.Wait(); // Wait for NFA_POLL_ENABLED_EVT.
      NCI_DEBUG("got enabled event");
    } else {
      NCI_ERROR("NFA_EnablePolling fail; error = 0x%X", stat);
    }
  }

  // Start P2P listening if tag polling was enabled or the mask was 0.
  if (sDiscoveryEnabled || (tech_mask == 0)) {
    NCI_DEBUG("enable p2pListening");
    PeerToPeer::GetInstance().EnableP2pListening(true);

    //if NFC service has deselected the sec elem, then apply default routes.
    if (!sIsSecElemSelected) {
      // TODO : Emulator to support SE routing .
      SecureElement::GetInstance().RouteToDefault();
      //stat = SecureElement::getInstance().routeToDefault() ?
      //  NFA_STATUS_OK : NFA_STATUS_FAILED;
    }
  }

  // Actually start discovery.
  StartRfDiscovery(true);

  PowerSwitch::GetInstance().SetModeOn(PowerSwitch::DISCOVERY);

  NCI_DEBUG("exit");
  return stat == NFA_STATUS_OK;
}

bool NfcManager::DisableDiscovery()
{
  tNFA_STATUS status = NFA_STATUS_OK;
  NCI_DEBUG("enter;");

  Pn544InteropAbortNow();
  if (!sDiscoveryEnabled) {
    NCI_DEBUG("already disabled");
    goto TheEnd;
  }

  // Stop RF Discovery.
  StartRfDiscovery(false);

  if (sDiscoveryEnabled) {
    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    status = NFA_DisablePolling();
    if (status == NFA_STATUS_OK) {
      sDiscoveryEnabled = false;
      sNfaEnableDisablePollingEvent.Wait(); // Wait for NFA_POLL_DISABLED_EVT.
    } else {
      NCI_ERROR("NFA_DisablePolling fail, error=0x%X", status);
    }
  }

  PeerToPeer::GetInstance().EnableP2pListening(false);

  // If nothing is active after this, then tell the controller to power down.
  if (!(PowerSwitch::GetInstance().SetModeOff(PowerSwitch::DISCOVERY))) {
    PowerSwitch::GetInstance().SetLevel(PowerSwitch::LOW_POWER);
  }

  // We may have had RF field notifications that did not cause
  // any activate/deactive events. For example, caused by wireless
  // charging orbs. Those may cause us to go to sleep while the last
  // field event was indicating a field. To prevent sticking in that
  // state, always reset the rf field status when we disable discovery.

  SecureElement::GetInstance().ResetRfFieldStatus();
TheEnd:
  NCI_DEBUG("exit");
  return status == NFA_STATUS_OK;
}

bool NfcManager::EnablePolling()
{
  return StartStopPolling(true);
}

bool NfcManager::DisablePolling()
{
  return StartStopPolling(false);
}

bool NfcManager::EnableP2pListening()
{
  return PeerToPeer::GetInstance().EnableP2pListening(true);
}

bool NfcManager::DisableP2pListening()
{
  return PeerToPeer::GetInstance().EnableP2pListening(false);
}

bool NfcManager::CheckLlcp()
{
  // Not used in NCI case.
  return true;
}

bool NfcManager::ActivateLlcp()
{
  // Not used in NCI case.
  return true;
}

ILlcpSocket* NfcManager::CreateLlcpSocket(int aSap,
                                          int aMiu,
                                          int aRw,
                                          int aBufLen)
{
  NCI_DEBUG("enter; sap=%d; miu=%d; rw=%d; buffer len=%d", aSap, aMiu, aRw, aBufLen);

  const uint32_t handle = PeerToPeer::GetInstance().GetNewHandle();
  if(!(PeerToPeer::GetInstance().CreateClient(handle, aMiu, aRw)))
    NCI_ERROR("fail create p2p client");

  LlcpSocket* pLlcpSocket = new LlcpSocket(handle, aSap, aMiu, aRw);

  NCI_DEBUG("exit");
  return static_cast<ILlcpSocket*>(pLlcpSocket);
}

ILlcpServerSocket* NfcManager::CreateLlcpServerSocket(int aSap,
                                                      const char* aSn,
                                                      int aMiu,
                                                      int aRw,
                                                      int aBufLen)
{
  NCI_DEBUG("enter; sap=%d; sn =%s; miu=%d; rw=%d; buffer len= %d", aSap, aSn, aMiu, aRw, aBufLen);
  const uint32_t handle = PeerToPeer::GetInstance().GetNewHandle();
  LlcpServiceSocket* pLlcpServiceSocket = new LlcpServiceSocket(handle, aBufLen, aMiu, aRw);

  if (!(PeerToPeer::GetInstance().RegisterServer(handle, aSn))) {
    NCI_ERROR("register server fail");
    return NULL;
  }

  NCI_DEBUG("exit");
  return static_cast<ILlcpServerSocket*>(pLlcpServiceSocket);
}

bool NfcManager::DoSelectSecureElement()
{
  bool result = true;

  if (sIsSecElemSelected) {
    NCI_DEBUG("already selected");
    return result;
  }

  PowerSwitch::GetInstance().SetLevel(PowerSwitch::FULL_POWER);

  if (sRfEnabled) {
    // Stop RF Discovery if we were polling.
    StartRfDiscovery(false);
  }

  result = SecureElement::GetInstance().Activate();
  if (result) {
    SecureElement::GetInstance().RouteToSecureElement();
  }

  sIsSecElemSelected = true;

  StartRfDiscovery(true);
  PowerSwitch::GetInstance().SetModeOn(PowerSwitch::SE_ROUTING);

  return result;
}

bool NfcManager::DoDeselectSecureElement()
{
  bool result = false;
  bool reDiscover = false;

  if (!sIsSecElemSelected) {
    NCI_ERROR("already deselected");
    goto TheEnd;
  }

  if (PowerSwitch::GetInstance().GetLevel() == PowerSwitch::LOW_POWER) {
    NCI_DEBUG("do not deselect while power is OFF");
    sIsSecElemSelected = false;
    goto TheEnd;
  }

  if (sRfEnabled) {
    // Stop RF Discovery if we were polling.
    StartRfDiscovery(false);
    reDiscover = true;
  }

  result = SecureElement::GetInstance().RouteToDefault();
  sIsSecElemSelected = false;

  //if controller is not routing to sec elems AND there is no pipe connected,
  //then turn off the sec elems
  if (!SecureElement::GetInstance().IsBusy()) {
    SecureElement::GetInstance().Deactivate();
  }

TheEnd:
  if (reDiscover) {
    StartRfDiscovery(true);
  }

  //if nothing is active after this, then tell the controller to power down
  if (!PowerSwitch::GetInstance().SetModeOff(PowerSwitch::SE_ROUTING)) {
    PowerSwitch::GetInstance().SetLevel(PowerSwitch::LOW_POWER);
  }

  return result;
}

tNFA_STATUS NfcManager::Disable(bool aGraceful)
{
#ifdef NFCC_PN547
  EXTNS_Close();
#endif
  return NFA_Disable(aGraceful);
}

/**
 * Private functions.
 */
static void HandleRfDiscoveryEvent(tNFC_RESULT_DEVT* discoveredDevice)
{
  if (discoveredDevice->more) {
    // There is more discovery notification coming.
    return;
  }

  bool isP2p = NfcTag::GetInstance().IsP2pDiscovered();
  if (isP2p) {
    // Select the peer that supports P2P.
    NfcTag::GetInstance().SelectP2p();
  } else {
    // Select the first of multiple tags that is discovered.
    NfcTag::GetInstance().SelectFirstTag();
  }
}

void NfaDeviceManagementCallback(UINT8 dmEvent, tNFA_DM_CBACK_DATA* eventData)
{
  // NCI_DEBUG("enter; event=0x%X", dmEvent);

  switch (dmEvent) {
    // Result of NFA_Enable.
    case NFA_DM_ENABLE_EVT: {
      SyncEventGuard guard(sNfaEnableEvent);
      NCI_DEBUG("NFA_DM_ENABLE_EVT; status=0x%X", eventData->status);
      sIsNfaEnabled = eventData->status == NFA_STATUS_OK;
      sIsDisabling = false;
      sNfaEnableEvent.NotifyOne();
      break;
    }
    // Result of NFA_Disable.
    case NFA_DM_DISABLE_EVT: {
      SyncEventGuard guard(sNfaDisableEvent);
      NCI_DEBUG("NFA_DM_DISABLE_EVT");
      sIsNfaEnabled = false;
      sIsDisabling = false;
      sNfaDisableEvent.NotifyOne();
      break;
    }
    // Result of NFA_SetConfig.
    case NFA_DM_SET_CONFIG_EVT:
      NCI_DEBUG("NFA_DM_SET_CONFIG_EVT");
      {
        SyncEventGuard guard(sNfaSetConfigEvent);
        sNfaSetConfigEvent.NotifyOne();
      }
      break;
    // Result of NFA_GetConfig.
    case NFA_DM_GET_CONFIG_EVT:
      NCI_DEBUG("NFA_DM_GET_CONFIG_EVT");
      {
        SyncEventGuard guard(sNfaGetConfigEvent);
        if (eventData->status == NFA_STATUS_OK &&
            eventData->get_config.tlv_size <= sizeof(sConfig)) {
          sCurrentConfigLen = eventData->get_config.tlv_size;
          memcpy(sConfig, eventData->get_config.param_tlvs, eventData->get_config.tlv_size);
        } else {
          NCI_ERROR("NFA_DM_GET_CONFIG failed; status=0x%X", eventData->status);
          sCurrentConfigLen = 0;
        }
        sNfaGetConfigEvent.NotifyOne();
      }
      break;

    case NFA_DM_RF_FIELD_EVT:
      // NCI_DEBUG("NFA_DM_RF_FIELD_EVT; status=0x%X; field status=%u",
      //           eventData->rf_field.status, eventData->rf_field.rf_field_status);

      if (sIsDisabling || !sIsNfaEnabled) {
        break;
      }

      if (!sP2pActive && eventData->rf_field.status == NFA_STATUS_OK) {
        SecureElement::GetInstance().NotifyRfFieldEvent(
          eventData->rf_field.rf_field_status == NFA_DM_RF_FIELD_ON);
      }
      break;

    case NFA_DM_NFCC_TRANSPORT_ERR_EVT:
    case NFA_DM_NFCC_TIMEOUT_EVT: {
      if (dmEvent == NFA_DM_NFCC_TIMEOUT_EVT) {
        NCI_DEBUG("NFA_DM_NFCC_TIMEOUT_EVT; abort all outstanding operations");
      } else {
        NCI_DEBUG("NFA_DM_NFCC_TRANSPORT_ERR_EVT; abort all outstanding operations");
      }
      NfcTagManager::DoAbortWaits();
      NfcTag::GetInstance().Abort();
      // TODO : Implement LLCP.
      {
        NCI_DEBUG("aborting sNfaEnableDisablePollingEvent");
        SyncEventGuard guard(sNfaEnableDisablePollingEvent);
        sNfaEnableDisablePollingEvent.NotifyOne();
      }
      {
        NCI_DEBUG("aborting sNfaEnableEvent");
        SyncEventGuard guard(sNfaEnableEvent);
        sNfaEnableEvent.NotifyOne();
      }
      {
        NCI_DEBUG("aborting sNfaDisableEvent");
        SyncEventGuard guard(sNfaDisableEvent);
        sNfaDisableEvent.NotifyOne();
      }
      sDiscoveryEnabled = false;
      PowerSwitch::GetInstance().Abort();

      if (!sIsDisabling && sIsNfaEnabled) {
        NfcManager::Disable(false);
        sIsDisabling = true;
      } else {
        sIsNfaEnabled = false;
        sIsDisabling = false;
      }
      PowerSwitch::GetInstance().Initialize(PowerSwitch::UNKNOWN_LEVEL);
      NCI_DEBUG("aborted all waiting events");
    }
    break;

    case NFA_DM_PWR_MODE_CHANGE_EVT:
      PowerSwitch::GetInstance().DeviceManagementCallback(dmEvent, eventData);
      break;
    default:
      NCI_DEBUG("unhandled event");
      break;
  }
}

static void NfaConnectionCallback(UINT8 connEvent, tNFA_CONN_EVT_DATA* eventData)
{
  tNFA_STATUS status = NFA_STATUS_FAILED;
  NCI_DEBUG("enter; event=0x%X", connEvent);

  switch (connEvent) {
    // Whether polling successfully started.
    case NFA_POLL_ENABLED_EVT: {
      NCI_DEBUG("NFA_POLL_ENABLED_EVT: status = 0x%X", eventData->status);

      SyncEventGuard guard(sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.NotifyOne();
      break;
    }
    // Listening/Polling stopped.
    case NFA_POLL_DISABLED_EVT: {
      NCI_DEBUG("NFA_POLL_DISABLED_EVT: status = 0x%X", eventData->status);

      SyncEventGuard guard(sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.NotifyOne();
      break;
    }
    // RF Discovery started.
    case NFA_RF_DISCOVERY_STARTED_EVT: {
      NCI_DEBUG("NFA_RF_DISCOVERY_STARTED_EVT: status = 0x%X", eventData->status);

      SyncEventGuard guard(sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.NotifyOne();
      break;
    }
    // RF Discovery stopped event.
    case NFA_RF_DISCOVERY_STOPPED_EVT: {
      NCI_DEBUG("NFA_RF_DISCOVERY_STOPPED_EVT: status = 0x%X", eventData->status);

      SyncEventGuard guard(sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.NotifyOne();
      break;
    }
    // NFC link/protocol discovery notificaiton.
    case NFA_DISC_RESULT_EVT:
      status = eventData->disc_result.status;
      NCI_DEBUG("NFA_DISC_RESULT_EVT: status = 0x%X", status);
      if (status != NFA_STATUS_OK) {
        NCI_ERROR("NFA_DISC_RESULT_EVT error: status = 0x%x", status);
      } else {
        NfcTag::GetInstance().ConnectionEventHandler(connEvent, eventData);
        HandleRfDiscoveryEvent(&eventData->disc_result.discovery_ntf);
      }
      break;
    // NFC link/protocol activated.
    case NFA_ACTIVATED_EVT:
      NCI_DEBUG("NFA_ACTIVATED_EVT: gIsSelectingRfInterface=%d, sIsDisabling=%d",
              gIsSelectingRfInterface, sIsDisabling);
      if (sIsDisabling || !sIsNfaEnabled) {
        break;
      }

#ifdef NFCC_PN547
      if (EXTNS_GetConnectFlag()) {
        NfcTag::GetInstance().SetActivationState();
        NfcTagManager::DoConnectStatus(true);
        break;
      }
#endif

      NfcTag::GetInstance().SetActivationState();
      if (gIsSelectingRfInterface) {
        NfcTagManager::DoConnectStatus(true);
        break;
      }

      NfcTagManager::DoResetPresenceCheck();
      if (IsPeerToPeer(eventData->activated)) {
        sP2pActive = true;
        NCI_DEBUG("NFA_ACTIVATED_EVT; is p2p");
        // Disable RF field events in case of p2p.
        UINT8  nfa_disable_rf_events[] = { 0x00 };
        NCI_DEBUG("Disabling RF field events");
        status = NFA_SetConfig(NCI_PARAM_ID_RF_FIELD_INFO, sizeof(nfa_disable_rf_events),
                   &nfa_disable_rf_events[0]);
        NCI_DEBUG("Disabled RF field events, status = 0x%x", status);

        // For the SE, consider the field to be on while p2p is active.
        SecureElement::GetInstance().NotifyRfFieldEvent(true);
      } else if (Pn544InteropIsBusy() == false) {
        NfcTag::GetInstance().ConnectionEventHandler(connEvent, eventData);

        // We know it is not activating for P2P.  If it activated in
        // listen mode then it is likely for an SE transaction.
        // Send the RF Event.
        if (IsListenMode(eventData->activated)) {
          sSeRfActive = true;
          SecureElement::GetInstance().NotifyListenModeState(true);
        }
      }
      break;
    // NFC link/protocol deactivated.
    case NFA_DEACTIVATED_EVT:
      NCI_DEBUG("NFA_DEACTIVATED_EVT Type: %u, gIsTagDeactivating: %d",
                eventData->deactivated.type,gIsTagDeactivating);
      NfcTag::GetInstance().SetDeactivationState(eventData->deactivated);
      if (eventData->deactivated.type != NFA_DEACTIVATE_TYPE_SLEEP) {
        NfcTagManager::DoResetPresenceCheck();
        NfcTag::GetInstance().ConnectionEventHandler(connEvent, eventData);
        NfcTagManager::DoAbortWaits();
        NfcTag::GetInstance().Abort();
      } else if (gIsTagDeactivating) {
        NfcTagManager::DoDeactivateStatus(0);
#ifdef NFCC_PN547
      } else if (EXTNS_GetDeactivateFlag()) {
        NfcTagManager::DoDeactivateStatus(0);
#endif
      }

      // If RF is activated for what we think is a Secure Element transaction
      // and it is deactivated to either IDLE or DISCOVERY mode, notify w/event.
      if ((eventData->deactivated.type == NFA_DEACTIVATE_TYPE_IDLE) ||
          (eventData->deactivated.type == NFA_DEACTIVATE_TYPE_DISCOVERY)) {
        if (sSeRfActive) {
          sSeRfActive = false;
          if (!sIsDisabling && sIsNfaEnabled) {
            SecureElement::GetInstance().NotifyListenModeState(false);
          }
        } else if (sP2pActive) {
          sP2pActive = false;
          // Make sure RF field events are re-enabled.
          NCI_DEBUG("NFA_DEACTIVATED_EVT; is p2p");
          // Disable RF field events in case of p2p.
          UINT8 nfa_enable_rf_events[] = { 0x01 };

          if (!sIsDisabling && sIsNfaEnabled) {
            status = NFA_SetConfig(NCI_PARAM_ID_RF_FIELD_INFO, sizeof(nfa_enable_rf_events),
                                   &nfa_enable_rf_events[0]);
            NCI_DEBUG("Enabled RF field events, status = 0x%x", status);

            // Consider the field to be off at this point
            SecureElement::GetInstance().NotifyRfFieldEvent(false);
          }
        }
      }
      break;
    // NDEF Detection complete.
    case NFA_NDEF_DETECT_EVT:
      // If status is failure, it means the tag does not contain any or valid NDEF data.
      // Pass the failure status to the NFC Service.
      status = eventData->ndef_detect.status;
      NCI_DEBUG("NFA_NDEF_DETECT_EVT: status = 0x%X, protocol = %u, "
                "max_size = %lu, cur_size = %lu, flags = 0x%X",
                status,
                eventData->ndef_detect.protocol, eventData->ndef_detect.max_size,
                eventData->ndef_detect.cur_size, eventData->ndef_detect.flags);
      NfcTag::GetInstance().ConnectionEventHandler(connEvent, eventData);
      NfcTagManager::DoCheckNdefResult(status,
        eventData->ndef_detect.max_size, eventData->ndef_detect.cur_size,
        eventData->ndef_detect.flags);
      break;
    // Data message received (for non-NDEF reads).
    case NFA_DATA_EVT:
      NCI_DEBUG("NFA_DATA_EVT:  len = %d", eventData->data.len);
      NfcTagManager::DoTransceiveComplete(eventData->data.p_data,
                                          eventData->data.len);
      break;
    case NFA_RW_INTF_ERROR_EVT:
        NCI_DEBUG("NFC_RW_INTF_ERROR_EVT");
      NfcTagManager::NotifyRfTimeout();
      break;
    // Select completed.
    case NFA_SELECT_CPLT_EVT:
      status = eventData->status;
      NCI_DEBUG("NFA_SELECT_CPLT_EVT: status = 0x%X", status);
      if (status != NFA_STATUS_OK) {
        NCI_ERROR("NFA_SELECT_CPLT_EVT error: status = 0x%X", status);
      }
      break;
    // NDEF-read or tag-specific-read completed.
    case NFA_READ_CPLT_EVT:
      NCI_DEBUG("NFA_READ_CPLT_EVT: status = 0x%X", eventData->status);
      NfcTagManager::DoReadCompleted(eventData->status);
      NfcTag::GetInstance().ConnectionEventHandler(connEvent, eventData);
      break;
    // Write completed.
    case NFA_WRITE_CPLT_EVT:
      NCI_DEBUG("NFA_WRITE_CPLT_EVT: status = 0x%X", eventData->status);
      NfcTagManager::DoWriteStatus(eventData->status == NFA_STATUS_OK);
      break;
    // Tag set as Read only.
    case NFA_SET_TAG_RO_EVT:
      NCI_DEBUG("NFA_SET_TAG_RO_EVT: status = 0x%X", eventData->status);
      NfcTagManager::DoMakeReadonlyResult(eventData->status);
      break;
    // LLCP link is activated.
    case NFA_LLCP_ACTIVATED_EVT:
      NCI_DEBUG("NFA_LLCP_ACTIVATED_EVT: is_initiator: %d  remote_wks: %d, remote_lsc: %d, remote_link_miu: %d, local_link_miu: %d",
                eventData->llcp_activated.is_initiator,
                eventData->llcp_activated.remote_wks,
                eventData->llcp_activated.remote_lsc,
                eventData->llcp_activated.remote_link_miu,
                eventData->llcp_activated.local_link_miu);

      PeerToPeer::GetInstance().LlcpActivatedHandler(eventData->llcp_activated);
      break;
    // LLCP link is deactivated.
    case NFA_LLCP_DEACTIVATED_EVT:
      NCI_DEBUG("NFA_LLCP_DEACTIVATED_EVT");
      PeerToPeer::GetInstance().LlcpDeactivatedHandler(eventData->llcp_deactivated);
      break;
    // Received first packet over llcp.
    case NFA_LLCP_FIRST_PACKET_RECEIVED_EVT:
      NCI_DEBUG("NFA_LLCP_FIRST_PACKET_RECEIVED_EVT");
      PeerToPeer::GetInstance().LlcpFirstPacketHandler();
      break;

    case NFA_PRESENCE_CHECK_EVT:
      NCI_DEBUG("NFA_PRESENCE_CHECK_EVT");
      NfcTagManager::DoPresenceCheckResult(eventData->status);
      break;

    case NFA_FORMAT_CPLT_EVT:
      NCI_DEBUG("NFA_FORMAT_CPLT_EVT: status=0x%X", eventData->status);
      NfcTagManager::DoFormatStatus(eventData->status == NFA_STATUS_OK);
      break;

    case NFA_CE_UICC_LISTEN_CONFIGURED_EVT :
      NCI_DEBUG("NFA_CE_UICC_LISTEN_CONFIGURED_EVT : status=0x%X", eventData->status);
      SecureElement::GetInstance().ConnectionEventHandler(connEvent, eventData);
      break;

    case NFA_SET_P2P_LISTEN_TECH_EVT:
      NCI_DEBUG("NFA_SET_P2P_LISTEN_TECH_EVT");
      PeerToPeer::GetInstance().ConnectionEventHandler(connEvent, eventData);
      break;

    default:
      NCI_ERROR("unknown event ????");
      break;
  }
}

void StartRfDiscovery(bool isStart)
{
  tNFA_STATUS status = NFA_STATUS_FAILED;

  NCI_DEBUG("is start=%d", isStart);
  SyncEventGuard guard(sNfaEnableDisablePollingEvent);
  status  = isStart ? NFA_StartRfDiscovery() : NFA_StopRfDiscovery();
  if (status == NFA_STATUS_OK) {
    sNfaEnableDisablePollingEvent.Wait(); // Wait for NFA_RF_DISCOVERY_xxxx_EVT.
    sRfEnabled = isStart;
  } else {
    NCI_ERROR("NFA_StartRfDiscovery/NFA_StopRfDiscovery fail; error=0x%X", status);
  }
}

void DoStartupConfig()
{
  tNFA_STATUS stat = NFA_STATUS_FAILED;

  // If polling for Active mode, set the ordering so that we choose Active over Passive mode first.
  if (gNat.tech_mask & (NFA_TECHNOLOGY_MASK_A_ACTIVE | NFA_TECHNOLOGY_MASK_F_ACTIVE)) {
    UINT8  act_mode_order_param[] = { 0x01 };
    SyncEventGuard guard(sNfaSetConfigEvent);

    stat = NFA_SetConfig(NCI_PARAM_ID_ACT_ORDER, sizeof(act_mode_order_param), &act_mode_order_param[0]);
    if (stat == NFA_STATUS_OK)
      sNfaSetConfigEvent.Wait();
    else
      NCI_ERROR("NFA_SetConfig fail; error = 0x%X", stat);
  }
}

bool IsNfcActive()
{
  return sIsNfaEnabled;
}

bool StartStopPolling(bool isStartPolling)
{
  NCI_DEBUG("enter; isStart=%u", isStartPolling);
  tNFA_STATUS stat = NFA_STATUS_FAILED;

  StartRfDiscovery(false);
  if (isStartPolling) {
    tNFA_TECHNOLOGY_MASK tech_mask = DEFAULT_TECH_MASK;
    unsigned long num = 0;
    if (GetNumValue(NAME_POLLING_TECH_MASK, &num, sizeof(num)))
      tech_mask = num;

    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    NCI_DEBUG("enable polling");
    stat = NFA_EnablePolling(tech_mask);
    if (stat == NFA_STATUS_OK) {
      NCI_DEBUG("wait for enable event");
      sNfaEnableDisablePollingEvent.Wait(); // Wait for NFA_POLL_ENABLED_EVT.
    } else {
      NCI_ERROR ("NFA_EnablePolling fail, error=0x%X", stat);
    }
  } else {
    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    NCI_DEBUG("disable polling");
    stat = NFA_DisablePolling();
    if (stat == NFA_STATUS_OK) {
      sNfaEnableDisablePollingEvent.Wait(); // Wait for NFA_POLL_DISABLED_EVT.
    } else {
      NCI_ERROR("NFA_DisablePolling fail, error=0x%X", stat);
    }
  }
  StartRfDiscovery(true);
  NCI_DEBUG("exit");
  return stat == NFA_STATUS_OK;
}

static bool IsPeerToPeer(tNFA_ACTIVATED& activated)
{
  return activated.activate_ntf.protocol == NFA_PROTOCOL_NFC_DEP;
}

static bool IsListenMode(tNFA_ACTIVATED& activated)
{
  return ((NFC_DISCOVERY_TYPE_LISTEN_A == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_B == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_F == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_ISO15693 == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_B_PRIME == activated.activate_ntf.rf_tech_param.mode));
}
